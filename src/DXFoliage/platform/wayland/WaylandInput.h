#pragma once

#include "platform/IInputSource.h"

// Wayland input. Placeholder for now: the real implementation binds
// wl_seat -> wl_keyboard/wl_pointer and translates the keymap the
// compositor sends (via xkbcommon) into NSInput::EKey. Reporting empty
// state keeps the app running so windowing and rendering can be brought
// up first.
namespace NSPlatformWayland
{
    class WaylandInput final : public NSInput::IInputSource
    {
    public:
        void Update() override {}

        NSInput::KeyboardState GetKeyboardState() const override { return m_keyboard; }
        NSInput::MouseState GetMouseState() const override { return m_mouse; }

        void SetMouseMode(NSInput::EMouseMode mode) override { m_mouse.mode = mode; }

    private:
        NSInput::KeyboardState m_keyboard;
        NSInput::MouseState m_mouse;
    };
}
