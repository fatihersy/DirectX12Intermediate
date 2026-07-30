#include "stdafx.h"
#include "WaylandInput.h"

#include "Logger.h"

#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include <cstring>

#include <sys/mman.h>
#include <unistd.h>

namespace NSPlatformWayland
{
    namespace
    {
        // xkbcommon speaks X11 keycodes; Wayland reports raw evdev
        // scancodes. The two differ by a constant 8, a fossil of X11
        // reserving codes 0-7. Forgetting this does not fail loudly - every
        // key simply maps to the wrong one, which reads like a broken
        // keymap rather than an off-by-eight.
        constexpr uint32_t kEvdevToXkbOffset = 8;

        NSInput::EKey ToEKey(xkb_keysym_t sym)
        {
            // Letters are folded to a single case: the keysym reflects the
            // active modifiers, so Shift+W arrives as XKB_KEY_W while a
            // bare W arrives as XKB_KEY_w, and callers want one EKey::W.
            //
            // Layout caveat: this maps by SYMBOL, not physical position, so
            // on a non-QWERTY layout the physical W key reports whatever
            // symbol that layout puts there. GLFW and SDL avoid this by
            // building a scancode table from the keymap. That matters for
            // WASD movement; it does not matter for End/Insert, so it is
            // left simple until there is something to move.
            if (sym >= XKB_KEY_a and sym <= XKB_KEY_z)
            {
                return static_cast<NSInput::EKey>(
                    static_cast<uint8_t>(NSInput::EKey::A) + (sym - XKB_KEY_a));
            }
            if (sym >= XKB_KEY_A and sym <= XKB_KEY_Z)
            {
                return static_cast<NSInput::EKey>(
                    static_cast<uint8_t>(NSInput::EKey::A) + (sym - XKB_KEY_A));
            }
            if (sym >= XKB_KEY_0 and sym <= XKB_KEY_9)
            {
                return static_cast<NSInput::EKey>(
                    static_cast<uint8_t>(NSInput::EKey::Num0) + (sym - XKB_KEY_0));
            }
            if (sym >= XKB_KEY_F1 and sym <= XKB_KEY_F12)
            {
                return static_cast<NSInput::EKey>(
                    static_cast<uint8_t>(NSInput::EKey::F1) + (sym - XKB_KEY_F1));
            }

            switch (sym)
            {
                case XKB_KEY_space:        return NSInput::EKey::Space;
                case XKB_KEY_Return:
                case XKB_KEY_KP_Enter:     return NSInput::EKey::Enter;
                case XKB_KEY_Escape:       return NSInput::EKey::Escape;
                case XKB_KEY_Tab:          return NSInput::EKey::Tab;
                case XKB_KEY_BackSpace:    return NSInput::EKey::Backspace;
                case XKB_KEY_Insert:       return NSInput::EKey::Insert;
                case XKB_KEY_Delete:       return NSInput::EKey::Delete;
                case XKB_KEY_Home:         return NSInput::EKey::Home;
                case XKB_KEY_End:          return NSInput::EKey::End;
                case XKB_KEY_Prior:        return NSInput::EKey::PageUp;
                case XKB_KEY_Next:         return NSInput::EKey::PageDown;
                case XKB_KEY_Up:           return NSInput::EKey::Up;
                case XKB_KEY_Down:         return NSInput::EKey::Down;
                case XKB_KEY_Left:         return NSInput::EKey::Left;
                case XKB_KEY_Right:        return NSInput::EKey::Right;
                case XKB_KEY_Shift_L:      return NSInput::EKey::LeftShift;
                case XKB_KEY_Shift_R:      return NSInput::EKey::RightShift;
                case XKB_KEY_Control_L:    return NSInput::EKey::LeftControl;
                case XKB_KEY_Control_R:    return NSInput::EKey::RightControl;
                case XKB_KEY_Alt_L:        return NSInput::EKey::LeftAlt;
                // AltGr on most layouts; treated as right alt, matching
                // what Win32 reports for the same physical key.
                case XKB_KEY_Alt_R:
                case XKB_KEY_ISO_Level3_Shift: return NSInput::EKey::RightAlt;
                default:                   return NSInput::EKey::Unknown;
            }
        }
    }

    // Wayland's C API takes listener structs of plain function pointers with
    // a void* user-data, so each event needs a static trampoline.
    struct WaylandInputCallbacks
    {
        static void SeatCapabilities(void* data, wl_seat*, uint32_t capabilities)
        {
            static_cast<WaylandInput*>(data)->OnSeatCapabilities(capabilities);
        }
        static void SeatName(void*, wl_seat*, const char*) {}

        static void RegistryGlobal(void* data, wl_registry* registry, uint32_t name,
                                   const char* interface, uint32_t version)
        {
            static_cast<WaylandInput*>(data)->OnRegistryGlobal(registry, name, interface, version);
        }
        static void RegistryGlobalRemove(void*, wl_registry*, uint32_t) {}

        static void Keymap(void* data, wl_keyboard*, uint32_t format, int32_t fd, uint32_t size)
        {
            static_cast<WaylandInput*>(data)->OnKeymap(format, fd, size);
        }
        static void Enter(void*, wl_keyboard*, uint32_t, wl_surface*, wl_array*) {}
        static void Leave(void* data, wl_keyboard*, uint32_t, wl_surface*)
        {
            static_cast<WaylandInput*>(data)->OnKeyboardLeave();
        }
        static void Key(void* data, wl_keyboard*, uint32_t, uint32_t, uint32_t key, uint32_t state)
        {
            static_cast<WaylandInput*>(data)->OnKey(key, state);
        }
        static void Modifiers(void* data, wl_keyboard*, uint32_t,
            uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group)
        {
            static_cast<WaylandInput*>(data)->OnModifiers(depressed, latched, locked, group);
        }
        static void RepeatInfo(void*, wl_keyboard*, int32_t, int32_t) {}
    };

    namespace
    {
        const wl_registry_listener kRegistryListener{
            WaylandInputCallbacks::RegistryGlobal,
            WaylandInputCallbacks::RegistryGlobalRemove,
        };

        const wl_seat_listener kSeatListener{
            WaylandInputCallbacks::SeatCapabilities,
            WaylandInputCallbacks::SeatName,
        };

        const wl_keyboard_listener kKeyboardListener{
            WaylandInputCallbacks::Keymap,
            WaylandInputCallbacks::Enter,
            WaylandInputCallbacks::Leave,
            WaylandInputCallbacks::Key,
            WaylandInputCallbacks::Modifiers,
            WaylandInputCallbacks::RepeatInfo,
        };
    }

    void WaylandInput::OnRegistryGlobal(wl_registry* registry, uint32_t name, const char* interface, uint32_t)
    {
        if (std::strcmp(interface, wl_seat_interface.name) != 0) return;

        // Only the first seat. Multi-seat systems exist and SDL/Godot
        // support them, but that is a lot of machinery for a demo that
        // cannot use a second keyboard for anything.
        if (m_seat) return;

        // Version 5 is enough for keyboard and for the pointer axis events
        // stage 2 will need.
        m_seat = static_cast<wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, 5));

        // Attached immediately, in the same call as the bind: the
        // capabilities event is already on its way back and would be lost
        // if the listener were installed any later.
        wl_seat_add_listener(m_seat, &kSeatListener, this);
    }

    WaylandInput::WaylandInput(wl_display* display)
        : m_display(display)
    {
        m_xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        if (not m_xkbContext)
        {
            g_FError("Wayland: xkb_context_new failed; input disabled");
            return;
        }

        m_registry = wl_display_get_registry(m_display);
        wl_registry_add_listener(m_registry, &kRegistryListener, this);

        // Two roundtrips for the same reason WaylandWindow::Create needs
        // them: the first delivers the globals, the second the events our
        // bind produced - here, wl_seat.capabilities.
        wl_display_roundtrip(m_display);
        wl_display_roundtrip(m_display);

        if (not m_seat) g_FWarn("Wayland: compositor advertised no wl_seat; input disabled");
    }

    WaylandInput::~WaylandInput()
    {
        DestroyKeyboard();
        if (m_seat) wl_seat_release(m_seat);
        if (m_registry) wl_registry_destroy(m_registry);
        if (m_xkbContext) xkb_context_unref(m_xkbContext);
    }

    void WaylandInput::DestroyKeyboard()
    {
        if (m_xkbState) { xkb_state_unref(m_xkbState); m_xkbState = nullptr; }
        if (m_xkbKeymap) { xkb_keymap_unref(m_xkbKeymap); m_xkbKeymap = nullptr; }
        if (m_keyboardDevice) { wl_keyboard_destroy(m_keyboardDevice); m_keyboardDevice = nullptr; }

        // Anything held when the device vanished can never be released.
        m_keyboard.down.reset();
    }

    void WaylandInput::OnSeatCapabilities(uint32_t capabilities)
    {
        const bool hasKeyboard = (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0;

        if (hasKeyboard and not m_keyboardDevice)
        {
            m_keyboardDevice = wl_seat_get_keyboard(m_seat);
            wl_keyboard_add_listener(m_keyboardDevice, &kKeyboardListener, this);
        }
        else if (not hasKeyboard and m_keyboardDevice)
        {
            DestroyKeyboard();
        }
    }

    void WaylandInput::OnKeymap(uint32_t format, int32_t fd, uint32_t size)
    {
        // The compositor passes the keymap as a file descriptor, not a
        // string - it can be tens of kilobytes, and this way every client
        // maps the same pages instead of each receiving a copy. We own the
        // fd and must close it on every path, including the failures.
        if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)
        {
            g_FWarn("Wayland: unsupported keymap format; keyboard disabled");
            close(fd);
            return;
        }

        char* mapped = static_cast<char*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
        if (mapped == MAP_FAILED)
        {
            g_FError("Wayland: could not mmap keymap");
            close(fd);
            return;
        }

        xkb_keymap* keymap = xkb_keymap_new_from_string(
            m_xkbContext, mapped, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);

        munmap(mapped, size);
        close(fd);

        if (not keymap)
        {
            g_FError("Wayland: xkb_keymap_new_from_string failed");
            return;
        }

        xkb_state* state = xkb_state_new(keymap);
        if (not state)
        {
            g_FError("Wayland: xkb_state_new failed");
            xkb_keymap_unref(keymap);
            return;
        }

        // This event fires again whenever the user switches layout, so
        // replace rather than assume this is the first one.
        if (m_xkbState) xkb_state_unref(m_xkbState);
        if (m_xkbKeymap) xkb_keymap_unref(m_xkbKeymap);
        m_xkbKeymap = keymap;
        m_xkbState = state;

        // Mirrors the Vulkan backend logging its chosen device: confirms the
        // seat -> keyboard -> keymap chain completed, which is otherwise
        // silent and indistinguishable from "no keyboard attached".
        g_FDebug("Wayland: keyboard keymap loaded (%d keycodes)",
            static_cast<int>(xkb_keymap_max_keycode(m_xkbKeymap) -
                             xkb_keymap_min_keycode(m_xkbKeymap) + 1));
    }

    void WaylandInput::OnKeyboardLeave()
    {
        // Focus left, and the release events for anything currently held
        // will never arrive. Clearing here is what stops keys sticking
        // down after alt-tab.
        m_keyboard.down.reset();
    }

    void WaylandInput::OnKey(uint32_t key, uint32_t state)
    {
        if (not m_xkbState) return;

        const xkb_keycode_t keycode = key + kEvdevToXkbOffset;
        const NSInput::EKey mapped = ToEKey(xkb_state_key_get_one_sym(m_xkbState, keycode));
        if (mapped == NSInput::EKey::Unknown) return;

        m_keyboard.down[static_cast<size_t>(mapped)] =
            (state == WL_KEYBOARD_KEY_STATE_PRESSED);
    }

    void WaylandInput::OnModifiers(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group)
    {
        if (not m_xkbState) return;

        // Without this, xkb_state_key_get_one_sym would never see Shift or
        // Caps Lock and would report unshifted symbols forever.
        xkb_state_update_mask(m_xkbState, depressed, latched, locked, 0, 0, group);
    }

    void WaylandInput::Update()
    {
        // Nothing to poll: the events above already ran during
        // WaylandWindow::PumpEvents(), which shares this display
        // connection. Per-frame values that accumulate rather than latch -
        // mouse deltas and wheel - get reset here once the pointer lands.
        m_mouse.deltaX = 0;
        m_mouse.deltaY = 0;
        m_mouse.wheelDelta = 0.0f;
    }
}
