#pragma once

#include "IApp.h"

#include "directxtk12/Keyboard.h"
#include "directxtk12/Mouse.h"

#include "Renderer.h"
#include "Scene.h"

class app : public IApp
{
public:
    app(UINT width, UINT height, std::wstring title, HINSTANCE hInstance, int nCmdShow);
    ~app();

    void Run();

    void OnInit() override;
    void OnDestroy() override;
    void OnUpdate() override;
    void OnRender() override;
    void OnResize(UINT width, UINT height) override;
    void ToggleFullScreen() override;

private:
    ComPtr<IWICImagingFactory2> m_wicFactory;
    ComPtr<IDXGIFactory7> m_factory;
    ComPtr<ID3D12Device14> m_device;

    void LoadPipeline();
    void LoadAssets();
    void UpdateKeyBindings();
    void UpdateMouseBindings();

    Scene m_scene;
    Renderer m_renderer;
    std::unique_ptr<ShaderCompiler> m_shaderCompiler;
    std::unique_ptr<DirectX::Keyboard> m_keyboard;
    std::unique_ptr<DirectX::Mouse> m_mouse;
    DirectX::Keyboard::KeyboardStateTracker m_keyboardTracker;

    // ImGui
    ComPtr<ID3D12DescriptorHeap> m_imGuiSrvHeap;
    std::vector<INT> m_freeImGuiSRVindices;
    UINT m_imGuiSrvDescriptorSize{};

};

