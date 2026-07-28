#pragma once

#include "PlatformHeaders_Win32.h"
#include "platform/IInputSource.h"

#include <memory>

namespace DirectX
{
    class Keyboard;
    class Mouse;
}

// Wraps DirectXTK12's Keyboard/Mouse classes (the same library the
// original app.cpp already used directly) behind the neutral
// IInputSource interface. This is the "fast, zero-risk" option from the
// plan — reusing a proven Windows input library rather than reimplementing
// raw input right away. A future hardening pass could replace this
// internal implementation with WM_INPUT/RegisterRawInputDevices without
// IInputSource's callers ever needing to change.
namespace NSPlatformWin32
{
    class Win32Input final : public NSInput::IInputSource
    {
    public:
        Win32Input();
        ~Win32Input() override;

        void Update() override;

        NSInput::KeyboardState GetKeyboardState() const override { return m_keyboardState; }
        NSInput::MouseState GetMouseState() const override { return m_mouseState; }

        void SetMouseMode(NSInput::EMouseMode mode) override;

        // Not part of IInputSource — DirectXTK12's Mouse needs a window
        // handle to clip/capture the cursor for relative mode. Call once,
        // right after the window is created.
        void SetWindow(HWND hwnd);

    private:
        std::unique_ptr<DirectX::Keyboard> m_keyboard;
        std::unique_ptr<DirectX::Mouse> m_mouse;

        NSInput::KeyboardState m_keyboardState;
        NSInput::MouseState m_mouseState;
    };
}
