#pragma once

#include "platform/IInputSource.h"

#include <cstdint>

// Translates our polled input state into ImGui's event queue. The input
// half of the ImGui integration, mirroring ImGuiRenderer's GPU half.
//
// Deliberately NOT in ImGuiRenderer: that class owns textures and geometry
// and knows nothing about a keyboard. This lives beside app, which is
// what owns IInputSource and decides what the UI is for.
//
// POLLED STATE, EVENT API. ImGui wants events, IInputSource gives
// snapshots — normally that mismatch means keeping shadow copies to spot
// changes. It does not here: ImGui::AddKeyEvent and AddMousePosEvent both
// filter duplicates internally (imgui.cpp "Filter duplicate"), so feeding
// the current state every frame produces exactly the transitions ImGui
// needs and nothing else.
namespace NSImGuiInput
{
    // Call once per frame AFTER IInputSource::Update() and BEFORE
    // ImGui::NewFrame(). displayWidth/Height are LOGICAL units — the same
    // ones mouse coordinates arrive in — not framebuffer pixels; see
    // IWindow.h on why conflating them is the classic HiDPI bug.
    void Feed(const NSInput::KeyboardState& keyboard,
              const NSInput::MouseState& mouse,
              const std::string& text,
              uint32_t displayWidth, uint32_t displayHeight,
              float deltaSeconds);

    // Points ImGui's clipboard hooks at the platform's. Call once, after
    // ImGui::CreateContext(). The source must outlive the context.
    //
    // ImGui's getter returns a bare const char* it does not own and reads
    // immediately, so IInputSource::GetClipboardText's reference-to-member
    // return is exactly the right shape — no lifetime juggling.
    void BindClipboard(NSInput::IInputSource& source);

    // True when ImGui has claimed the input and the application should
    // NOT also act on it — clicking a button must not also drive the
    // camera. Valid after NewFrame().
    bool WantsMouse();
    bool WantsKeyboard();
}
