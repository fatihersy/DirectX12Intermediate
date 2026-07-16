#pragma once

#include "IApp.h"

#include "directxtk12/Keyboard.h"
#include "directxtk12/Mouse.h"

#include "Renderer.h"

class app : public IApp
{
public:
    app(uint32_t width, uint32_t height, std::wstring_view title, HINSTANCE hInstance, int nCmdShow);
    ~app();

    int Run();

    void OnInit() override;
    void OnUpdate() override;
    void OnRender() override;
    void OnDestroy() override;
    void OnResize(UINT width, UINT height) override;
    void ToggleFullScreen() override;

private:
    ComPtr<IDXGIFactory7> m_factory;

    void LoadPipeline();
    void LoadAssets();
    void UpdateBindings();

    Renderer m_renderer;

    std::unique_ptr<DirectX::Keyboard> m_keyboard;
    std::unique_ptr<DirectX::Mouse> m_mouse;
    DirectX::Keyboard::KeyboardStateTracker m_keyboardTracker;
};
