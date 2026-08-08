#pragma once

#include "platform/IInputSource.h"

#include <cstdint>

struct wl_display;
struct wl_keyboard;
struct wl_pointer;
struct wl_registry;
struct wl_seat;
struct wl_surface;
struct wp_cursor_shape_device_v1;
struct wl_data_device;
struct wl_data_device_manager;
struct wl_data_offer;
struct wl_data_source;
struct wp_cursor_shape_manager_v1;
struct zwp_locked_pointer_v1;
struct zwp_pointer_constraints_v1;
struct zwp_relative_pointer_manager_v1;
struct zwp_relative_pointer_v1;
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
// EMouseMode::Relative additionally LOCKS the pointer. That is two
// protocols working together: pointer-constraints freezes the cursor in
// place so it can never reach a screen edge, and relative-pointer then
// delivers motion that is genuinely unbounded rather than the difference
// between two clamped positions. Without the lock, deltas silently stop
// at the window border - which is what the position-differencing fallback
// in OnPointerMotion still does when these protocols are unavailable.
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
// Also carries TEXT INPUT and the CLIPBOARD, neither of which is strictly
// "input". Both are here because both belong to the seat: text comes from
// the same xkb state as the key events (and cannot be derived from EKey —
// punctuation and accented characters have no EKey at all), and
// wl_data_device is obtained from wl_seat, with set_selection requiring a
// serial from a real input event. Splitting either out would mean the new
// interface reaching back into this one.
//
// NOT implemented, deliberately: key repeat for held LETTERS. Wayland
// sends a rate/delay via repeat_info and expects the client to run the
// timer itself (GLFW, SDL and Godot each implement this by hand).
//
// This used to say "nothing in this app repeats", which stopped being
// true when ImGui arrived. What is actually true now: ImGui derives
// repeat for NAVIGATION keys itself from how long a key has been down
// (io.KeyRepeatDelay/Rate), and we re-send the down state every frame, so
// held Backspace and arrows already repeat inside a text field. Only a
// held letter fails to spam, because a character is emitted once on the
// initial press. That is the remaining half of task #19.
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
        const std::string& GetTextInput() const override { return m_textInput; }

        const std::string& GetClipboardText() override;
        void SetClipboardText(const std::string& text) override;

        void SetMouseMode(NSInput::EMouseMode mode) override;

        // Compositor event handlers. Declared here so the C listener structs
        // in the .cpp can reach the instance state - same pattern as
        // WaylandWindow.
        void OnRegistryGlobal(wl_registry* registry, uint32_t name, const char* interface, uint32_t version);
        void OnSeatCapabilities(uint32_t capabilities);
        void OnKeymap(uint32_t format, int32_t fd, uint32_t size);
        void OnKeyboardLeave();
        void OnKey(uint32_t key, uint32_t state);
        void AppendTextFor(uint32_t keycode);

        void NoteInputSerial(uint32_t serial);
        void CreateDataDevice();
        void OnDataOfferCreated(wl_data_offer* offer);
        void OnDataOfferMime(wl_data_offer* offer, const char* mimeType);
        void OnSelection(wl_data_offer* offer);
        void OnDataSourceSend(int32_t fd);
        void OnDataSourceCancelled(wl_data_source* source);
        void OnModifiers(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group);

        void OnPointerEnter(uint32_t serial, double x, double y);
        void OnPointerLeave();
        void OnPointerMotion(double x, double y);
        void OnPointerButton(uint32_t button, uint32_t state);
        void OnPointerAxis(uint32_t axis, double value);
        void OnRelativeMotion(double dx, double dy);
        void SetPointerSurface(wl_surface* surface);

    private:
        void DestroyKeyboard();
        void DestroyPointer();

        // Must be re-applied on every pointer enter: the compositor resets
        // the cursor per surface-entry, so setting it once does not stick.
        void ApplyCursorVisibility();
        void ApplyPointerLock();

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

        // Handed to us by wl_pointer.enter. Pointer constraints lock to a
        // surface, and taking it from the enter event means input still
        // never has to know the window object exists.
        wl_surface* m_pointerSurface{ nullptr };

        zwp_relative_pointer_manager_v1* m_relativePointerManager{ nullptr };
        zwp_pointer_constraints_v1* m_pointerConstraints{ nullptr };
        zwp_relative_pointer_v1* m_relativePointer{ nullptr };
        zwp_locked_pointer_v1* m_lockedPointer{ nullptr };

        // Deltas are derived by differencing successive positions, so the
        // first motion after the cursor enters has nothing to difference
        // against and must not produce a jump.
        bool m_hasPointerPosition{ false };

        // Accumulators, kept separate from the published MouseState.
        // Events arrive many times per frame; the consumer reads once per
        // frame AFTER calling Update(). If Update() cleared the published
        // values directly, the caller would only ever see zero - so it
        // publishes these, then clears them for the next frame.
        //
        // DOUBLE, not int: relative-pointer deltas are wl_fixed_t and are
        // routinely fractional (0.4, 1.1) because pointer acceleration is
        // applied. Truncating each event would discard every slow movement
        // entirely and shave the fraction off every fast one. Update()
        // publishes the whole-pixel part and carries the remainder, so
        // sub-pixel motion accumulates into real movement instead of
        // vanishing. Win32 raw input is integral, which is why MouseState
        // itself can stay int32_t.
        double m_pendingDeltaX{ 0.0 };
        double m_pendingDeltaY{ 0.0 };
        float m_pendingWheel{ 0.0f };

        xkb_context* m_xkbContext{ nullptr };
        xkb_keymap* m_xkbKeymap{ nullptr };
        xkb_state* m_xkbState{ nullptr };

        // Two buffers, same reason as the mouse deltas: events arrive
        // during PumpEvents and accumulate into m_pendingText, then
        // Update() publishes one frame's worth into m_textInput and
        // starts a fresh accumulator. Publishing in place would let a
        // consumer read the same characters twice.
        std::string m_pendingText;
        std::string m_textInput;

        // Clipboard. Wayland stores nothing centrally: whoever copied last
        // stays the owner and is asked to produce the bytes on demand.
        wl_data_device_manager* m_dataDeviceManager{ nullptr };
        wl_data_device* m_dataDevice{ nullptr };
        // Our source, alive only while WE own the clipboard; the compositor
        // cancels it when another application copies.
        wl_data_source* m_dataSource{ nullptr };
        // Someone else's current offer, and whether it can give us text.
        wl_data_offer* m_selectionOffer{ nullptr };
        bool m_offerHasText{ false };

        std::string m_clipboardOwned;    // what WE published; served on demand
        std::string m_clipboardFetched;  // last read from another application

        // set_selection is rejected without a serial from real user input,
        // so the most recent key/button serial is kept for it.
        uint32_t m_lastInputSerial{ 0 };

        NSInput::KeyboardState m_keyboard;
        NSInput::MouseState m_mouse;
    };
}
