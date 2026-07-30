#pragma once

#include "platform/IInputSource.h"

#include <cstdint>

struct wl_display;
struct wl_keyboard;
struct wl_registry;
struct wl_seat;
struct xkb_context;
struct xkb_keymap;
struct xkb_state;

// Wayland keyboard input.
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

        void SetMouseMode(NSInput::EMouseMode mode) override { m_mouse.mode = mode; }

        // Compositor event handlers. Declared here so the C listener structs
        // in the .cpp can reach the instance state - same pattern as
        // WaylandWindow.
        void OnRegistryGlobal(wl_registry* registry, uint32_t name, const char* interface, uint32_t version);
        void OnSeatCapabilities(uint32_t capabilities);
        void OnKeymap(uint32_t format, int32_t fd, uint32_t size);
        void OnKeyboardLeave();
        void OnKey(uint32_t key, uint32_t state);
        void OnModifiers(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group);

    private:
        void DestroyKeyboard();

        wl_display* m_display{ nullptr };    // owned by WaylandWindow
        wl_registry* m_registry{ nullptr };
        wl_seat* m_seat{ nullptr };
        wl_keyboard* m_keyboardDevice{ nullptr };

        xkb_context* m_xkbContext{ nullptr };
        xkb_keymap* m_xkbKeymap{ nullptr };
        xkb_state* m_xkbState{ nullptr };

        NSInput::KeyboardState m_keyboard;
        NSInput::MouseState m_mouse;
    };
}
