#pragma once

#include "platform/IInputSource.h"

#include <cstdint>

struct wl_display;
struct wl_keyboard;
struct wl_pointer;
struct wl_registry;
struct wl_seat;
struct wp_cursor_shape_device_v1;
struct wp_cursor_shape_manager_v1;
struct xkb_context;
struct xkb_keymap;
struct xkb_state;

// Wayland keyboard and pointer input.
//
// Wayland delivers input as events, but IInputSource is polled - so this
// class accumulates event state as it arrives and hands out a snapshot on
// demand. It needs no dispatch loop of its own: the keyboard lives on the
// same wl_display connection as the window, so WaylandWindow::PumpEvents()
// already dispatches these events. That mirrors how Win32 input arrives
// through the same message pump as everything else.
//
// This class opens its OWN wl_registry, rather than being handed a wl_seat
// the window already bound. That is forced by the IWindow/IInputSource
// split: a compositor sends wl_seat.capabilities immediately after the bind
// request, so the listener has to be attached at bind time or the event is
// dispatched to a listener-less proxy and dropped. GLFW and SDL attach it at
// bind time because their window and input are one subsystem; keeping ours
// separate means input must do its own registry pass. Both registries live
// on the same wl_display, so PumpEvents() still dispatches everything.
//
// Cursor visibility is carried by EMouseMode rather than a separate
// switch: Absolute shows the cursor, Relative hides it. That matches
// DirectXTK's Mouse::MODE_RELATIVE, which the Win32 backend already
// forwards to and which already hides the cursor there - so this is the
// contract the interface inherited, not a new one.
//
// Hiding is wl_pointer.set_cursor with a null surface. RESTORING is the
// awkward half: Wayland core has no "show the default cursor" request, so
// without wp_cursor_shape_v1 it means loading the user's cursor theme,
// wrapping an image in a wl_buffer, and running a timer for animated
// cursors. With it, the compositor is simply told a shape name.
//
// Two Wayland behaviours have no Win32 equivalent and are easy to get wrong:
//
//   1. wl_seat.capabilities is a LIVE event, not a startup query. It fires
//      again when a keyboard is plugged in or removed, so wl_keyboard is
//      created and destroyed in response rather than once at construction.
//      A keyboard may therefore appear several frames after startup.
//   2. wl_keyboard.leave arrives when focus is lost, and the matching key
//      release never does. Without clearing state there, a key held while
//      alt-tabbing stays down forever.
//
// NOT implemented, deliberately: key repeat. Wayland sends a rate/delay via
// repeat_info and expects the client to run the timer itself (GLFW, SDL and
// Godot each implement this by hand). Nothing in this app repeats - End and
// Insert are read on release - so it is left out rather than half-done.
namespace NSPlatformWayland
{
    class WaylandInput final : public NSInput::IInputSource
    {
    public:
        // If the compositor advertises no seat, the object still works and
        // simply reports empty state forever.
        explicit WaylandInput(wl_display* display);
        ~WaylandInput() override;

        WaylandInput(const WaylandInput&) = delete;
        WaylandInput& operator=(const WaylandInput&) = delete;

        void Update() override;

        NSInput::KeyboardState GetKeyboardState() const override { return m_keyboard; }
        NSInput::MouseState GetMouseState() const override { return m_mouse; }

        void SetMouseMode(NSInput::EMouseMode mode) override;

        // Compositor event handlers. Declared here so the C listener structs
        // in the .cpp can reach the instance state - same pattern as
        // WaylandWindow.
        void OnRegistryGlobal(wl_registry* registry, uint32_t name, const char* interface, uint32_t version);
        void OnSeatCapabilities(uint32_t capabilities);
        void OnKeymap(uint32_t format, int32_t fd, uint32_t size);
        void OnKeyboardLeave();
        void OnKey(uint32_t key, uint32_t state);
        void OnModifiers(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group);

        void OnPointerEnter(uint32_t serial, double x, double y);
        void OnPointerLeave();
        void OnPointerMotion(double x, double y);
        void OnPointerButton(uint32_t button, uint32_t state);
        void OnPointerAxis(uint32_t axis, double value);

    private:
        void DestroyKeyboard();
        void DestroyPointer();

        // Must be re-applied on every pointer enter: the compositor resets
        // the cursor per surface-entry, so setting it once does not stick.
        void ApplyCursorVisibility();

        wl_display* m_display{ nullptr };    // owned by WaylandWindow
        wl_registry* m_registry{ nullptr };
        wl_seat* m_seat{ nullptr };
        wl_keyboard* m_keyboardDevice{ nullptr };
        wl_pointer* m_pointerDevice{ nullptr };

        // Absent on compositors older than the cursor-shape protocol. We
        // then decline to hide at all rather than hide with no way back.
        wp_cursor_shape_manager_v1* m_cursorShapeManager{ nullptr };
        wp_cursor_shape_device_v1* m_cursorShapeDevice{ nullptr };

        // set_cursor must quote the serial of the enter event it responds
        // to; the compositor uses it to confirm the request follows real
        // user interaction.
        uint32_t m_pointerEnterSerial{ 0 };

        // Deltas are derived by differencing successive positions, so the
        // first motion after the cursor enters has nothing to difference
        // against and must not produce a jump.
        bool m_hasPointerPosition{ false };

        // Accumulators, kept separate from the published MouseState.
        // Events arrive many times per frame; the consumer reads once per
        // frame AFTER calling Update(). If Update() cleared the published
        // values directly, the caller would only ever see zero - so it
        // publishes these, then clears them for the next frame.
        int32_t m_pendingDeltaX{ 0 };
        int32_t m_pendingDeltaY{ 0 };
        float m_pendingWheel{ 0.0f };

        xkb_context* m_xkbContext{ nullptr };
        xkb_keymap* m_xkbKeymap{ nullptr };
        xkb_state* m_xkbState{ nullptr };

        NSInput::KeyboardState m_keyboard;
        NSInput::MouseState m_mouse;
    };
}
