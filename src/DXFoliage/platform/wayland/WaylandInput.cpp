#include "stdafx.h"
#include "WaylandInput.h"

#include "Logger.h"

#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include <cursor-shape-v1-client-protocol.h>
#include <pointer-constraints-unstable-v1-client-protocol.h>
#include <relative-pointer-unstable-v1-client-protocol.h>

#include <linux/input-event-codes.h>

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

        // Wayland reports raw evdev button codes, not an enum of its own.
        NSInput::EMouseButton ToMouseButton(uint32_t code, bool& recognised)
        {
            recognised = true;
            switch (code)
            {
                case BTN_LEFT:   return NSInput::EMouseButton::Left;
                case BTN_RIGHT:  return NSInput::EMouseButton::Right;
                case BTN_MIDDLE: return NSInput::EMouseButton::Middle;
                default:
                    // Side buttons, tilt, and anything else this project
                    // has no name for.
                    recognised = false;
                    return NSInput::EMouseButton::Left;
            }
        }

        // Wayland scroll is ~10 units per wheel detent; IInputSource
        // specifies notches. See MouseState::wheelDelta.
        constexpr double kScrollUnitsPerNotch = 10.0;
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

        static void PointerEnter(void* data, wl_pointer*, uint32_t serial, wl_surface* surface,
                                 wl_fixed_t x, wl_fixed_t y)
        {
            auto* self = static_cast<WaylandInput*>(data);
            self->SetPointerSurface(surface);
            self->OnPointerEnter(serial, wl_fixed_to_double(x), wl_fixed_to_double(y));
        }

        // dx/dy are accelerated (pointer-speed settings applied);
        // dx_unaccel is raw device movement. Accelerated is what a cursor
        // would have done, which is what feels right for camera look -
        // raw is for things that must ignore desktop tuning.
        static void RelativeMotion(void* data, zwp_relative_pointer_v1*,
                                   uint32_t, uint32_t,
                                   wl_fixed_t dx, wl_fixed_t dy,
                                   wl_fixed_t, wl_fixed_t)
        {
            static_cast<WaylandInput*>(data)->OnRelativeMotion(
                wl_fixed_to_double(dx), wl_fixed_to_double(dy));
        }
        static void PointerLeave(void* data, wl_pointer*, uint32_t, wl_surface*)
        {
            static_cast<WaylandInput*>(data)->OnPointerLeave();
        }
        static void PointerMotion(void* data, wl_pointer*, uint32_t, wl_fixed_t x, wl_fixed_t y)
        {
            static_cast<WaylandInput*>(data)->OnPointerMotion(
                wl_fixed_to_double(x), wl_fixed_to_double(y));
        }
        static void PointerButton(void* data, wl_pointer*, uint32_t, uint32_t,
                                  uint32_t button, uint32_t state)
        {
            static_cast<WaylandInput*>(data)->OnPointerButton(button, state);
        }
        static void PointerAxis(void* data, wl_pointer*, uint32_t, uint32_t axis, wl_fixed_t value)
        {
            static_cast<WaylandInput*>(data)->OnPointerAxis(axis, wl_fixed_to_double(value));
        }
        static void PointerFrame(void*, wl_pointer*) {}
        static void PointerAxisSource(void*, wl_pointer*, uint32_t) {}
        static void PointerAxisStop(void*, wl_pointer*, uint32_t, uint32_t) {}
        static void PointerAxisDiscrete(void*, wl_pointer*, uint32_t, int32_t) {}
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

        // Designated initialisers: wl_pointer_listener also declares
        // axis_value120 (protocol v8) and axis_relative_direction (v9),
        // which are left null deliberately. We bind version 5, so the
        // compositor may never send them - and naming the fields keeps
        // this correct if the struct grows again.
        //
        // The two unused ones are named explicitly rather than omitted:
        // "deliberately null" and "forgot" look identical to a reader and
        // to -Wmissing-designated-field-initializers. Saying it in code
        // means the day we bind v8 for high-resolution scroll, the
        // compiler points here instead of staying silent.
        const wl_pointer_listener kPointerListener{
            .enter = WaylandInputCallbacks::PointerEnter,
            .leave = WaylandInputCallbacks::PointerLeave,
            .motion = WaylandInputCallbacks::PointerMotion,
            .button = WaylandInputCallbacks::PointerButton,
            .axis = WaylandInputCallbacks::PointerAxis,
            .frame = WaylandInputCallbacks::PointerFrame,
            .axis_source = WaylandInputCallbacks::PointerAxisSource,
            .axis_stop = WaylandInputCallbacks::PointerAxisStop,
            .axis_discrete = WaylandInputCallbacks::PointerAxisDiscrete,
            .axis_value120 = nullptr,            // protocol v8, unbound
            .axis_relative_direction = nullptr,  // protocol v9, unbound
        };

        const zwp_relative_pointer_v1_listener kRelativePointerListener{
            WaylandInputCallbacks::RelativeMotion,
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
        if (std::strcmp(interface, wp_cursor_shape_manager_v1_interface.name) == 0)
        {
            m_cursorShapeManager = static_cast<wp_cursor_shape_manager_v1*>(
                wl_registry_bind(registry, name, &wp_cursor_shape_manager_v1_interface, 1));
            return;
        }

        if (std::strcmp(interface, zwp_relative_pointer_manager_v1_interface.name) == 0)
        {
            m_relativePointerManager = static_cast<zwp_relative_pointer_manager_v1*>(
                wl_registry_bind(registry, name, &zwp_relative_pointer_manager_v1_interface, 1));
            return;
        }

        if (std::strcmp(interface, zwp_pointer_constraints_v1_interface.name) == 0)
        {
            m_pointerConstraints = static_cast<zwp_pointer_constraints_v1*>(
                wl_registry_bind(registry, name, &zwp_pointer_constraints_v1_interface, 1));
            return;
        }

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
        DestroyPointer();
        if (m_lockedPointer) zwp_locked_pointer_v1_destroy(m_lockedPointer);
        if (m_relativePointer) zwp_relative_pointer_v1_destroy(m_relativePointer);
        if (m_pointerConstraints) zwp_pointer_constraints_v1_destroy(m_pointerConstraints);
        if (m_relativePointerManager) zwp_relative_pointer_manager_v1_destroy(m_relativePointerManager);
        if (m_cursorShapeManager) wp_cursor_shape_manager_v1_destroy(m_cursorShapeManager);
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

    void WaylandInput::DestroyPointer()
    {
        if (m_cursorShapeDevice) { wp_cursor_shape_device_v1_destroy(m_cursorShapeDevice); m_cursorShapeDevice = nullptr; }
        if (m_pointerDevice) { wl_pointer_destroy(m_pointerDevice); m_pointerDevice = nullptr; }

        // Same reasoning as the keyboard: a button held when the device
        // vanished can never be released.
        m_mouse.buttonsDown.reset();
        m_hasPointerPosition = false;
        m_pendingDeltaX = 0.0;
        m_pendingDeltaY = 0.0;
    }

    void WaylandInput::OnSeatCapabilities(uint32_t capabilities)
    {
        const bool hasKeyboard = (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0;
        const bool hasPointer = (capabilities & WL_SEAT_CAPABILITY_POINTER) != 0;

        if (hasKeyboard and not m_keyboardDevice)
        {
            m_keyboardDevice = wl_seat_get_keyboard(m_seat);
            wl_keyboard_add_listener(m_keyboardDevice, &kKeyboardListener, this);
        }
        else if (not hasKeyboard and m_keyboardDevice)
        {
            DestroyKeyboard();
        }

        if (hasPointer and not m_pointerDevice)
        {
            m_pointerDevice = wl_seat_get_pointer(m_seat);
            wl_pointer_add_listener(m_pointerDevice, &kPointerListener, this);

            if (m_cursorShapeManager)
            {
                m_cursorShapeDevice = wp_cursor_shape_manager_v1_get_pointer(
                    m_cursorShapeManager, m_pointerDevice);
            }

            g_FDebug("Wayland: pointer attached%s",
                m_cursorShapeManager ? "" : " (no cursor-shape support; cursor hiding disabled)");
        }
        else if (not hasPointer and m_pointerDevice)
        {
            DestroyPointer();
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

    void WaylandInput::OnPointerEnter(uint32_t serial, double x, double y)
    {
        m_pointerEnterSerial = serial;

        // The compositor resets the cursor whenever the pointer enters a
        // surface, so a hidden cursor reappears unless re-hidden here.
        ApplyCursorVisibility();

        // Seed the position without producing a delta - the cursor did not
        // travel from wherever it happened to be last time.
        m_mouse.x = static_cast<int32_t>(x);
        m_mouse.y = static_cast<int32_t>(y);
        m_hasPointerPosition = true;
    }

    void WaylandInput::SetMouseMode(NSInput::EMouseMode mode)
    {
        if (m_mouse.mode == mode) return;

        m_mouse.mode = mode;
        ApplyCursorVisibility();
        ApplyPointerLock();
    }

    void WaylandInput::SetPointerSurface(wl_surface* surface)
    {
        m_pointerSurface = surface;
    }

    void WaylandInput::ApplyPointerLock()
    {
        const bool wantLock = (m_mouse.mode == NSInput::EMouseMode::Relative);

        if (not wantLock)
        {
            if (m_lockedPointer) { zwp_locked_pointer_v1_destroy(m_lockedPointer); m_lockedPointer = nullptr; }
            if (m_relativePointer) { zwp_relative_pointer_v1_destroy(m_relativePointer); m_relativePointer = nullptr; }
            return;
        }

        if (not m_pointerDevice or not m_pointerSurface) return;
        if (not m_relativePointerManager or not m_pointerConstraints)
        {
            // Falls back to position differencing in OnPointerMotion,
            // which works until the cursor reaches a window edge.
            return;
        }

        if (not m_relativePointer)
        {
            m_relativePointer = zwp_relative_pointer_manager_v1_get_relative_pointer(
                m_relativePointerManager, m_pointerDevice);
            zwp_relative_pointer_v1_add_listener(m_relativePointer, &kRelativePointerListener, this);
        }

        if (not m_lockedPointer)
        {
            // PERSISTENT rather than ONESHOT: the lock must survive the
            // pointer losing and regaining focus, otherwise alt-tabbing
            // back into the window silently drops camera control.
            m_lockedPointer = zwp_pointer_constraints_v1_lock_pointer(
                m_pointerConstraints, m_pointerSurface, m_pointerDevice, nullptr,
                ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
        }
    }

    void WaylandInput::OnRelativeMotion(double dx, double dy)
    {
        // Only meaningful while locked; ignoring it otherwise stops these
        // deltas double-counting with the position-differenced ones.
        if (m_mouse.mode != NSInput::EMouseMode::Relative) return;

        m_pendingDeltaX += dx;
        m_pendingDeltaY += dy;



    }

    void WaylandInput::ApplyCursorVisibility()
    {
        if (not m_pointerDevice or m_pointerEnterSerial == 0) return;

        // Without cursor-shape there is no way to put the cursor back, so
        // refuse to take it away. A visible cursor in relative mode is a
        // cosmetic flaw; one that can never be restored is a broken app.
        if (not m_cursorShapeDevice) return;

        if (m_mouse.mode == NSInput::EMouseMode::Relative)
        {
            // Null surface = no cursor drawn over ours.
            wl_pointer_set_cursor(m_pointerDevice, m_pointerEnterSerial, nullptr, 0, 0);
        }
        else
        {
            // The compositor already knows the user's theme; we name a
            // shape rather than supplying pixels.
            wp_cursor_shape_device_v1_set_shape(m_cursorShapeDevice, m_pointerEnterSerial,
                WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT);
        }
    }

    void WaylandInput::OnPointerLeave()
    {
        m_mouse.buttonsDown.reset();
        m_hasPointerPosition = false;
    }

    void WaylandInput::OnPointerMotion(double x, double y)
    {
        const int32_t newX = static_cast<int32_t>(x);
        const int32_t newY = static_cast<int32_t>(y);

        // While locked, the relative-pointer protocol is the delta source
        // and the cursor does not move - differencing would contribute
        // nothing but would double-count if it ever did.
        if (m_hasPointerPosition and m_mouse.mode != NSInput::EMouseMode::Relative)
        {
            // Derived from successive absolute positions, so it stops at
            // the window edge. Genuine unbounded relative motion needs the
            // relative-pointer and pointer-constraints protocols, which is
            // why EMouseMode::Relative still reports nothing useful.
            m_pendingDeltaX += newX - m_mouse.x;
            m_pendingDeltaY += newY - m_mouse.y;
        }

        m_mouse.x = newX;
        m_mouse.y = newY;
        m_hasPointerPosition = true;

        // than once a frame, so only a sample is printed.
    }

    void WaylandInput::OnPointerButton(uint32_t button, uint32_t state)
    {
        bool recognised = false;
        const NSInput::EMouseButton mapped = ToMouseButton(button, recognised);
        if (not recognised) return;

        m_mouse.buttonsDown[static_cast<size_t>(mapped)] =
            (state == WL_POINTER_BUTTON_STATE_PRESSED);

    }

    void WaylandInput::OnPointerAxis(uint32_t axis, double value)
    {
        // Horizontal scroll has nowhere to go in MouseState, so it is
        // dropped rather than folded into the vertical value.
        if (axis != WL_POINTER_AXIS_VERTICAL_SCROLL) return;

        // Wayland's axis is positive when scrolling DOWN; callers expect
        // the wheel-forward convention where positive is up, matching
        // Win32's WHEEL_DELTA sign.
        m_pendingWheel -= static_cast<float>(value / kScrollUnitsPerNotch);

    }

    void WaylandInput::Update()
    {
        // Nothing to poll: the events above already ran during
        // WaylandWindow::PumpEvents(), which shares this display
        // connection.
        //
        // What happens here is publishing one frame's worth of accumulated
        // motion and scroll, then resetting the accumulators. Callers read
        // GetMouseState() AFTER Update(), so clearing the published fields
        // here instead would hand them zeros every frame. Position and
        // buttons are levels rather than accumulations and simply persist.
        m_mouse.deltaX = static_cast<int32_t>(m_pendingDeltaX);
        m_mouse.deltaY = static_cast<int32_t>(m_pendingDeltaY);
        m_mouse.wheelDelta = m_pendingWheel;

        // Carry the sub-pixel remainder rather than discarding it: six
        // frames of 0.4 should become two pixels of movement, not zero.
        m_pendingDeltaX -= m_mouse.deltaX;
        m_pendingDeltaY -= m_mouse.deltaY;
        m_pendingWheel = 0.0f;
    }
}