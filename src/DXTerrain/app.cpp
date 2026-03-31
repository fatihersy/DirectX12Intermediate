#include "stdafx.h"
#include "app.h"

#include "dxgidebug.h"

#include "DXSampleHelper.h"
#include "Platform.h"

IApp* IApp::s_instance = nullptr;

Platform plat;

app::app(UINT width, UINT height, std::wstring_view title, HINSTANCE hInstance, int nCmdShow) : IApp(width, height, title)
{
    s_instance = this;

    plat = Platform(SWindow{
        .hInstance = hInstance,
        .hWnd = nullptr,
        .pApp = this,
        .nCmdShow = nCmdShow,
        .width = width,
        .height = height,
        .title = title
    });

    im_aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    im_assetsPath = std::filesystem::current_path().generic_wstring().append(L"\\");

    ThrowIfFailed(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
    ThrowIfFailed(CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_wicFactory)));

    WCHAR executablePath[512];
    GetExecutablePath(executablePath, _countof(executablePath));
    im_executablePath = executablePath;
}
app::~app()
{

}
void app::OnDestroy()
{
    m_renderer.OnDestroy();
};

void app::Run()
{
    MSG msg{};

    while (msg.message != WM_QUIT)
    {
        plat.Dispatch(msg);
    }
}

void app::OnInit()
{
    LoadPipeline();
    LoadAssets();

    plat.ShowWindow();
};
void app::OnUpdate()
{

};
void app::OnRender()
{
    m_renderer.BeginFrame();

    m_renderer.EndFrame();
};
void app::OnResize(UINT width, UINT height) {};
void app::ToggleFullScreen() {};

void app::LoadPipeline()
{
    UINT dxgiFactoryFlags{};

    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();

        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }

    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_factory)));

    // Seeking compatible device
    {
        ComPtr<IDXGIAdapter1> adapter;
        ComPtr<IDXGIFactory6> factory;
        if (SUCCEEDED(m_factory->QueryInterface(IID_PPV_ARGS(&factory))))
        {
            for (
                UINT adapterIndex = 0;
                SUCCEEDED(factory->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)));
                adapterIndex++
            )
            {
                DXGI_ADAPTER_DESC1 desc{};
                adapter->GetDesc1(&desc);

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

                if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_2, __uuidof(ID3D12Device), nullptr)))
                {
                    break;
                }
            }
        }

        if (adapter.Get() == nullptr)
        {
            for (UINT adapterIndex{}; SUCCEEDED(m_factory->EnumAdapters1(adapterIndex, &adapter)); adapterIndex++)
            {
                DXGI_ADAPTER_DESC1 desc{};
                adapter->GetDesc1(&desc);

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

                if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_2, __uuidof(ID3D12Device), nullptr)))
                {
                    break;
                }
            }
        }

        ThrowIfFailed(D3D12CreateDevice(adapter.Detach(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&m_device)));
    }

    m_renderer.Init(m_factory.Get(), m_device.Get(), plat.GetWindow(), im_width, im_height);
}
void app::LoadAssets()
{

}
