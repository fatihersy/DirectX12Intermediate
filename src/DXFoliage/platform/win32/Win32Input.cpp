#include "stdafx.h"
#include "Win32Input.h"

#include "directxtk12/Keyboard.h"
#include "directxtk12/Mouse.h"

namespace NSPlatformWin32
{
    namespace
    {
        // Maps our neutral key enum to DirectXTK12's Keys enum. Returns
        // DirectX::Keyboard::None for anything we don't have a mapping
        // for (there is no such case today — every EKey value from A to
        // RightAlt is covered — but it keeps the switch total/safe
        // against future additions to EKey).
        DirectX::Keyboard::Keys MapKey(NSInput::EKey key)
        {
            using DXKey = DirectX::Keyboard::Keys;

            switch (key)
            {
                case NSInput::EKey::A: return DXKey::A;
                case NSInput::EKey::B: return DXKey::B;
                case NSInput::EKey::C: return DXKey::C;
                case NSInput::EKey::D: return DXKey::D;
                case NSInput::EKey::E: return DXKey::E;
                case NSInput::EKey::F: return DXKey::F;
                case NSInput::EKey::G: return DXKey::G;
                case NSInput::EKey::H: return DXKey::H;
                case NSInput::EKey::I: return DXKey::I;
                case NSInput::EKey::J: return DXKey::J;
                case NSInput::EKey::K: return DXKey::K;
                case NSInput::EKey::L: return DXKey::L;
                case NSInput::EKey::M: return DXKey::M;
                case NSInput::EKey::N: return DXKey::N;
                case NSInput::EKey::O: return DXKey::O;
                case NSInput::EKey::P: return DXKey::P;
                case NSInput::EKey::Q: return DXKey::Q;
                case NSInput::EKey::R: return DXKey::R;
                case NSInput::EKey::S: return DXKey::S;
                case NSInput::EKey::T: return DXKey::T;
                case NSInput::EKey::U: return DXKey::U;
                case NSInput::EKey::V: return DXKey::V;
                case NSInput::EKey::W: return DXKey::W;
                case NSInput::EKey::X: return DXKey::X;
                case NSInput::EKey::Y: return DXKey::Y;
                case NSInput::EKey::Z: return DXKey::Z;

                // Top-row digits — DirectXTK names these D0-D9.
                case NSInput::EKey::Num0: return DXKey::D0;
                case NSInput::EKey::Num1: return DXKey::D1;
                case NSInput::EKey::Num2: return DXKey::D2;
                case NSInput::EKey::Num3: return DXKey::D3;
                case NSInput::EKey::Num4: return DXKey::D4;
                case NSInput::EKey::Num5: return DXKey::D5;
                case NSInput::EKey::Num6: return DXKey::D6;
                case NSInput::EKey::Num7: return DXKey::D7;
                case NSInput::EKey::Num8: return DXKey::D8;
                case NSInput::EKey::Num9: return DXKey::D9;

                case NSInput::EKey::F1: return DXKey::F1;
                case NSInput::EKey::F2: return DXKey::F2;
                case NSInput::EKey::F3: return DXKey::F3;
                case NSInput::EKey::F4: return DXKey::F4;
                case NSInput::EKey::F5: return DXKey::F5;
                case NSInput::EKey::F6: return DXKey::F6;
                case NSInput::EKey::F7: return DXKey::F7;
                case NSInput::EKey::F8: return DXKey::F8;
                case NSInput::EKey::F9: return DXKey::F9;
                case NSInput::EKey::F10: return DXKey::F10;
                case NSInput::EKey::F11: return DXKey::F11;
                case NSInput::EKey::F12: return DXKey::F12;

                case NSInput::EKey::Space: return DXKey::Space;
                case NSInput::EKey::Enter: return DXKey::Enter;
                case NSInput::EKey::Escape: return DXKey::Escape;
                case NSInput::EKey::Tab: return DXKey::Tab;
                case NSInput::EKey::Backspace: return DXKey::Back;
                case NSInput::EKey::Insert: return DXKey::Insert;
                case NSInput::EKey::Delete: return DXKey::Delete;
                case NSInput::EKey::Home: return DXKey::Home;
                case NSInput::EKey::End: return DXKey::End;
                case NSInput::EKey::PageUp: return DXKey::PageUp;
                case NSInput::EKey::PageDown: return DXKey::PageDown;
                case NSInput::EKey::Up: return DXKey::Up;
                case NSInput::EKey::Down: return DXKey::Down;
                case NSInput::EKey::Left: return DXKey::Left;
                case NSInput::EKey::Right: return DXKey::Right;
                case NSInput::EKey::LeftShift: return DXKey::LeftShift;
                case NSInput::EKey::RightShift: return DXKey::RightShift;
                case NSInput::EKey::LeftControl: return DXKey::LeftControl;
                case NSInput::EKey::RightControl: return DXKey::RightControl;
                case NSInput::EKey::LeftAlt: return DXKey::LeftAlt;
                case NSInput::EKey::RightAlt: return DXKey::RightAlt;

                default: return DXKey::None;
            }
        }
    }

    Win32Input::Win32Input()
        : m_keyboard(std::make_unique<DirectX::Keyboard>())
        , m_mouse(std::make_unique<DirectX::Mouse>())
    {
    }

    Win32Input::~Win32Input() = default;

    void Win32Input::SetWindow(HWND hwnd)
    {
        m_mouse->SetWindow(hwnd);
    }

    void Win32Input::Update()
    {
        const DirectX::Keyboard::State kbState = m_keyboard->GetState();

        NSInput::KeyboardState newKeyState;
        for (uint8_t i = static_cast<uint8_t>(NSInput::EKey::A); i < static_cast<uint8_t>(NSInput::EKey::Count); ++i)
        {
            const auto key = static_cast<NSInput::EKey>(i);
            if (kbState.IsKeyDown(MapKey(key)))
            {
                newKeyState.down.set(i);
            }
        }
        m_keyboardState = newKeyState;

        const DirectX::Mouse::State mouseState = m_mouse->GetState();

        NSInput::MouseState newMouseState;
        newMouseState.mode = (mouseState.positionMode == DirectX::Mouse::MODE_RELATIVE)
            ? NSInput::EMouseMode::Relative
            : NSInput::EMouseMode::Absolute;

        // DirectXTK12 reports state.x/state.y as an absolute screen
        // position in MODE_ABSOLUTE, but as a frame-to-frame delta in
        // MODE_RELATIVE — so which field we fill in depends on the mode.
        if (newMouseState.mode == NSInput::EMouseMode::Relative)
        {
            newMouseState.deltaX = mouseState.x;
            newMouseState.deltaY = mouseState.y;
        }
        else
        {
            newMouseState.x = mouseState.x;
            newMouseState.y = mouseState.y;
        }

        newMouseState.wheelDelta = static_cast<float>(mouseState.scrollWheelValue);

        if (mouseState.leftButton)   newMouseState.buttonsDown.set(static_cast<size_t>(NSInput::EMouseButton::Left));
        if (mouseState.rightButton)  newMouseState.buttonsDown.set(static_cast<size_t>(NSInput::EMouseButton::Right));
        if (mouseState.middleButton) newMouseState.buttonsDown.set(static_cast<size_t>(NSInput::EMouseButton::Middle));

        m_mouseState = newMouseState;
    }

    void Win32Input::SetMouseMode(NSInput::EMouseMode mode)
    {
        m_mouse->SetMode(mode == NSInput::EMouseMode::Relative ? DirectX::Mouse::MODE_RELATIVE : DirectX::Mouse::MODE_ABSOLUTE);
    }
}
