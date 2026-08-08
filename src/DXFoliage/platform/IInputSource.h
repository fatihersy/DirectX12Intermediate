#pragma once

#include <bitset>
#include <cstdint>
#include <string>

// The input contract every backend (Win32, Wayland, ...) implements.
// Replaces DirectXTK12's Keyboard/Mouse classes (which only exist on
// Windows) with a neutral polled-state API in the same shape, so app.cpp
// doesn't need to change how it reads input, only where that input
// ultimately comes from.
namespace NSInput
{
    enum class EKey : uint8_t
    {
        Unknown = 0,

        A, B, C, D, E, F, G, H, I, J, K, L, M,
        N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

        // Top-row number keys (not the numpad — this project has no use
        // for a separate numpad mapping yet).
        Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

        Space, Enter, Escape, Tab, Backspace,
        Insert, Delete, Home, End, PageUp, PageDown,
        Up, Down, Left, Right,
        LeftShift, RightShift, LeftControl, RightControl, LeftAlt, RightAlt,

        Count
    };

    enum class EMouseButton : uint8_t
    {
        Left = 0,
        Right,
        Middle,

        Count
    };

    enum class EMouseMode : uint8_t
    {
        Absolute, // normal OS cursor, reports screen position
        Relative, // cursor hidden/captured, reports frame-to-frame deltas (for camera look)
    };

    using KeyMask = std::bitset<static_cast<size_t>(EKey::Count)>;
    using MouseButtonMask = std::bitset<static_cast<size_t>(EMouseButton::Count)>;

    struct KeyboardState
    {
        KeyMask down;

        bool IsDown(EKey key) const { return down[static_cast<size_t>(key)]; }
    };

    struct MouseState
    {
        int32_t x{};
        int32_t y{};
        int32_t deltaX{};
        int32_t deltaY{};
        // Scroll accumulated since the last Update(), in NOTCHES: 1.0 is
        // one detent of a conventional mouse wheel, and fractional values
        // are normal from touchpads and free-spinning wheels. Each backend
        // normalises into this unit - Wayland reports ~10.0 per notch,
        // Win32's WHEEL_DELTA is 120 - so callers never see the difference.
        float wheelDelta{};
        MouseButtonMask buttonsDown;
        EMouseMode mode{ EMouseMode::Absolute };

        bool IsDown(EMouseButton button) const { return buttonsDown[static_cast<size_t>(button)]; }
    };

    class IInputSource
    {
    public:
        virtual ~IInputSource() = default;

        // Polls the OS/compositor for the latest state. Call once per
        // frame, before reading GetKeyboardState()/GetMouseState().
        virtual void Update() = 0;

        virtual KeyboardState GetKeyboardState() const = 0;
        virtual MouseState GetMouseState() const = 0;

        // UTF-8 text typed since the last Update(), usually empty.
        //
        // Deliberately NOT part of KeyboardState. That struct is a
        // SNAPSHOT — "which keys are down right now" — and is copied every
        // frame by KeyStateTracker to diff against. Text is the opposite:
        // a transient queue that must be consumed exactly once, and it
        // cannot be reconstructed from key state because the same physical
        // key yields 'a', 'A' or nothing depending on modifiers, layout
        // and dead keys. The platform layer resolves all of that; callers
        // just read the result.
        virtual const std::string& GetTextInput() const = 0;

        // Clipboard. Here rather than in PlatformUtils because on Wayland
        // it genuinely belongs to the seat: wl_data_device comes from
        // wl_seat, and set_selection is rejected without a serial from a
        // recent input event. Splitting it into its own interface would
        // mean that interface reaching back into this one.
        //
        // Get is NOT const and NOT cheap: Wayland keeps no clipboard
        // storage, so reading means asking the owning application to write
        // into a pipe and waiting for it. The implementation bounds that
        // wait rather than blocking the frame indefinitely — a paste can
        // come back empty if the other application is unresponsive.
        virtual const std::string& GetClipboardText() = 0;
        virtual void SetClipboardText(const std::string& text) = 0;

        virtual void SetMouseMode(EMouseMode mode) = 0;
    };

    // Pure bookkeeping, no OS code involved: diffs two KeyboardState
    // snapshots so callers can ask "was this key just pressed/released
    // this frame?" instead of only "is it down right now?". Replaces
    // DirectX::Keyboard::KeyboardStateTracker.
    class KeyStateTracker
    {
    public:
        void Reset() { m_previous = KeyboardState{}; }

        void Update(const KeyboardState& current)
        {
            m_pressed = current.down & ~m_previous.down;
            m_released = m_previous.down & ~current.down;
            m_previous = current;
        }

        bool IsKeyPressed(EKey key) const { return m_pressed[static_cast<size_t>(key)]; }
        bool IsKeyReleased(EKey key) const { return m_released[static_cast<size_t>(key)]; }

    private:
        KeyboardState m_previous;
        KeyMask m_pressed;
        KeyMask m_released;
    };
}
