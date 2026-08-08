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

        // ALWAYS EMPTY on Win32 — a stub, not an implementation. The
        // Wayland side gets UTF-8 from xkb inside its key handler; the
        // equivalent here is WM_CHAR, which arrives as UTF-16 in the
        // window procedure and needs converting, plus surrogate pairs
        // reassembling. Win32Window would have to route WM_CHAR here, and
        // none of it can be tested without a Windows machine (see the
        // standing risk in PLAN.md). Text fields will silently do nothing
        // on Windows until then.
        const std::string& GetTextInput() const override { return m_textInput; }

        // STUBS, like GetTextInput above. Win32's clipboard is genuinely
        // simpler than Wayland's — OpenClipboard/GetClipboardData with
        // CF_UNICODETEXT, no pipes, no serials, no asynchronous handshake,
        // because Windows really does store the bytes centrally. It still
        // needs UTF-16↔UTF-8 conversion and an HWND, and none of it can be
        // tested here (see the standing risk in PLAN.md), so it is left
        // honestly empty rather than written blind and assumed correct.
        const std::string& GetClipboardText() override { return m_clipboard; }
        void SetClipboardText(const std::string&) override {}

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

        // Never written — see GetTextInput/GetClipboardText above. They
        // exist so the reference returns have something valid to bind to.
        std::string m_textInput;
        std::string m_clipboard;
    };
}
