#include "stdafx.h"
#include "ImGuiInput.h"

#include "imgui.h"

namespace NSImGuiInput
{
    namespace
    {
        // EKey -> ImGuiKey. The three long runs (letters, digits, function
        // keys) are contiguous in BOTH enums, so they map by arithmetic
        // rather than 48 case labels — with static_asserts on the
        // endpoints, because that arithmetic is only correct while both
        // enums stay contiguous and a reorder would silently shift every
        // key by one.
        constexpr ImGuiKey ToImGuiKey(NSInput::EKey key)
        {
            using EKey = NSInput::EKey;

            static_assert(static_cast<int>(EKey::Z) - static_cast<int>(EKey::A) == 25);
            static_assert(static_cast<int>(EKey::Num9) - static_cast<int>(EKey::Num0) == 9);
            static_assert(static_cast<int>(EKey::F12) - static_cast<int>(EKey::F1) == 11);
            static_assert(ImGuiKey_Z - ImGuiKey_A == 25);
            static_assert(ImGuiKey_9 - ImGuiKey_0 == 9);
            static_assert(ImGuiKey_F12 - ImGuiKey_F1 == 11);

            const int k = static_cast<int>(key);
            if (key >= EKey::A and key <= EKey::Z)
                return static_cast<ImGuiKey>(ImGuiKey_A + (k - static_cast<int>(EKey::A)));
            if (key >= EKey::Num0 and key <= EKey::Num9)
                return static_cast<ImGuiKey>(ImGuiKey_0 + (k - static_cast<int>(EKey::Num0)));
            if (key >= EKey::F1 and key <= EKey::F12)
                return static_cast<ImGuiKey>(ImGuiKey_F1 + (k - static_cast<int>(EKey::F1)));

            switch (key)
            {
                case EKey::Space:        return ImGuiKey_Space;
                case EKey::Enter:        return ImGuiKey_Enter;
                case EKey::Escape:       return ImGuiKey_Escape;
                case EKey::Tab:          return ImGuiKey_Tab;
                case EKey::Backspace:    return ImGuiKey_Backspace;
                case EKey::Insert:       return ImGuiKey_Insert;
                case EKey::Delete:       return ImGuiKey_Delete;
                case EKey::Home:         return ImGuiKey_Home;
                case EKey::End:          return ImGuiKey_End;
                case EKey::PageUp:       return ImGuiKey_PageUp;
                case EKey::PageDown:     return ImGuiKey_PageDown;
                case EKey::Up:           return ImGuiKey_UpArrow;
                case EKey::Down:         return ImGuiKey_DownArrow;
                case EKey::Left:         return ImGuiKey_LeftArrow;
                case EKey::Right:        return ImGuiKey_RightArrow;
                case EKey::LeftShift:    return ImGuiKey_LeftShift;
                case EKey::RightShift:   return ImGuiKey_RightShift;
                case EKey::LeftControl:  return ImGuiKey_LeftCtrl;
                case EKey::RightControl: return ImGuiKey_RightCtrl;
                case EKey::LeftAlt:      return ImGuiKey_LeftAlt;
                case EKey::RightAlt:     return ImGuiKey_RightAlt;
                default:                 return ImGuiKey_None;
            }
        }

        constexpr int ToImGuiMouseButton(NSInput::EMouseButton button)
        {
            switch (button)
            {
                case NSInput::EMouseButton::Left:   return 0;
                case NSInput::EMouseButton::Right:  return 1;
                case NSInput::EMouseButton::Middle: return 2;
                default:                            return -1;
            }
        }
    }

    void Feed(const NSInput::KeyboardState& keyboard,
              const NSInput::MouseState& mouse,
              const std::string& text,
              uint32_t displayWidth, uint32_t displayHeight,
              float deltaSeconds)
    {
        ImGuiIO& io = ImGui::GetIO();

        io.DisplaySize = ImVec2(static_cast<float>(displayWidth), static_cast<float>(displayHeight));
        // ImGui asserts on a zero or negative delta, and a stalled frame
        // (breakpoint, compositor pause) can produce one.
        io.DeltaTime = deltaSeconds > 0.0f ? deltaSeconds : 1.0f / 60.0f;

        // In Relative mode the cursor is captured for camera look and its
        // absolute position is meaningless — telling ImGui the pointer
        // left the window stops it hovering whatever it last touched.
        if (mouse.mode == NSInput::EMouseMode::Relative)
        {
            io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        }
        else
        {
            io.AddMousePosEvent(static_cast<float>(mouse.x), static_cast<float>(mouse.y));
        }

        for (size_t i = 0; i < static_cast<size_t>(NSInput::EMouseButton::Count); ++i)
        {
            const auto button = static_cast<NSInput::EMouseButton>(i);
            const int imguiButton = ToImGuiMouseButton(button);
            if (imguiButton >= 0) io.AddMouseButtonEvent(imguiButton, mouse.IsDown(button));
        }

        // Already normalised to notches by the platform layer — Wayland
        // reports ~10 per detent and Win32 uses 120, and IInputSource
        // hides that difference, which is exactly what ImGui expects.
        if (mouse.wheelDelta != 0.0f) io.AddMouseWheelEvent(0.0f, mouse.wheelDelta);

        for (size_t i = 1; i < static_cast<size_t>(NSInput::EKey::Count); ++i)
        {
            const auto key = static_cast<NSInput::EKey>(i);
            const ImGuiKey imguiKey = ToImGuiKey(key);
            if (imguiKey != ImGuiKey_None) io.AddKeyEvent(imguiKey, keyboard.IsDown(key));
        }

        // Modifiers are sent separately as well as being real keys —
        // ImGui uses these for shortcut matching, and they are what make
        // Ctrl+click and shift-select work.
        io.AddKeyEvent(ImGuiMod_Ctrl,
            keyboard.IsDown(NSInput::EKey::LeftControl) or keyboard.IsDown(NSInput::EKey::RightControl));
        io.AddKeyEvent(ImGuiMod_Shift,
            keyboard.IsDown(NSInput::EKey::LeftShift) or keyboard.IsDown(NSInput::EKey::RightShift));
        io.AddKeyEvent(ImGuiMod_Alt,
            keyboard.IsDown(NSInput::EKey::LeftAlt) or keyboard.IsDown(NSInput::EKey::RightAlt));

        // Text, if any. Separate from the key events above because a
        // character is not a key: 'A' is Shift+A on one layout and a bare
        // keypress on another, and plenty of characters have no EKey at
        // all. The platform layer resolves layout, modifiers and dead
        // keys; this just forwards the result.
        //
        // Key REPEAT needs nothing extra here. ImGui derives it from how
        // long a key has been down (io.KeyRepeatDelay/Rate), and the loop
        // above re-sends the current down state every frame — so held
        // Backspace and arrows repeat inside InputText without a timer on
        // our side. Only held *letters* do not spam, since a character is
        // only emitted on the initial press.
        if (not text.empty()) io.AddInputCharactersUTF8(text.c_str());
    }

    void BindClipboard(NSInput::IInputSource& source)
    {
        // Dear ImGui 1.92 (we pin 1.92.5 in conanfile.py) moved these
        // from ImGuiIO to ImGuiPlatformIO. The old io.GetClipboardTextFn
        // pair still compiles but is the legacy path and is ignored when
        // the Platform_ ones are set.
        ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();
        platformIO.Platform_ClipboardUserData = &source;

        platformIO.Platform_GetClipboardTextFn = [](ImGuiContext*) -> const char*
        {
            auto* src = static_cast<NSInput::IInputSource*>(
                ImGui::GetPlatformIO().Platform_ClipboardUserData);
            // Returns a reference to a member string, so the pointer stays
            // valid until the next call — which is all ImGui promises to
            // need. On Wayland this is the expensive one: it round-trips
            // to whichever application owns the clipboard.
            return src->GetClipboardText().c_str();
        };

        platformIO.Platform_SetClipboardTextFn = [](ImGuiContext*, const char* text)
        {
            auto* src = static_cast<NSInput::IInputSource*>(
                ImGui::GetPlatformIO().Platform_ClipboardUserData);
            src->SetClipboardText(text ? text : "");
        };
    }

    bool WantsMouse()
    {
        return ImGui::GetCurrentContext() != nullptr and ImGui::GetIO().WantCaptureMouse;
    }

    bool WantsKeyboard()
    {
        return ImGui::GetCurrentContext() != nullptr and ImGui::GetIO().WantCaptureKeyboard;
    }
}
