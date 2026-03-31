#pragma once

#include "IApp.h"

#include "Renderer.h"

class app : public IApp
{
public:
    app(UINT width, UINT height, std::wstring_view title, HINSTANCE hInstance, int nCmdShow);
    ~app();

    void Run();

    void OnInit() override;
    void OnUpdate() override;
    void OnRender() override;
    void OnDestroy() override;
    void OnResize(UINT width, UINT height) override;
    void ToggleFullScreen() override;

    void LoadPipeline();
    void LoadAssets();

private:
    ComPtr<IDXGIFactory7> m_factory;
    ComPtr<ID3D12Device14> m_device;
    ComPtr<IWICImagingFactory2> m_wicFactory;

    Renderer m_renderer;
};

