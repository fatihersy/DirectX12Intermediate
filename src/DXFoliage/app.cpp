#include "stdafx.h"
#include "app.h"

#include "dxgidebug.h"

#include "DXSampleHelper.h"
#include "Platform.h"

IApp* IApp::s_instance = nullptr;

Platform plat;

app::app(uint32_t width, uint32_t height, std::wstring_view title, HINSTANCE hInstance, int nCmdShow) : IApp(width, height)
{
    s_instance = this;

    im_windowedRECT = { 0L, 0L, static_cast<LONG>(width), static_cast<LONG>(height)};
    im_aspectRatio = static_cast<float>(width) / static_cast<float>(height);

    plat.OnInit(SWindow
    {
        .hInstance = hInstance,
        .hWnd = nullptr,
        .pApp = this,
        .nCmdShow = nCmdShow,
        .width = width,
        .height = height,
        .title = title
    });

    im_assetsPath = std::filesystem::current_path();

    WCHAR executablePath[512];
    GetExecutablePath(executablePath, _countof(executablePath));
    im_executablePath = executablePath;

    ThrowIfFailed(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
    ThrowIfFailed(CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&im_wicFactory)));

    m_keyboard = std::make_unique<DirectX::Keyboard>();
    m_mouse = std::make_unique<DirectX::Mouse>();
    m_mouse->SetWindow(plat.GetWindow());
}
void app::OnInit()
{
    LoadPipeline();
    LoadAssets();

    plat.ShowWindow();

    m_keyboardTracker.Reset();
}
app::~app()
{
    s_instance = nullptr;
}
void app::OnDestroy()
{
    m_renderer.OnDestroy();

    if (m_mouse.release()) {};
    m_mouse.reset();
    if (m_keyboard.release()) {};
    m_keyboard.reset();

    im_wicFactory.Reset();
    m_factory.Reset();
    im_device.Reset();

    ComPtr<IDXGIDebug1> dxgiDebug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
    {
        dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
    }
    dxgiDebug.Reset();
};
int app::Run()
{
    MSG msg{};

    while (msg.message != WM_QUIT)
    {
        plat.Dispatch(msg);
    }

    return static_cast<int>(msg.wParam);
}
void app::LoadPipeline()
{
    UINT dxgiFactoryFlags{};

    ComPtr<ID3D12Debug6> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();

        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }

    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_factory)));

    // Seeking compatible device
    {
        ComPtr<IDXGIAdapter1> adapter;
        ComPtr<IDXGIFactory7> factory;
        if (SUCCEEDED(m_factory->QueryInterface(IID_PPV_ARGS(&factory))))
        {
            for (
                UINT adapterIndex{};
                SUCCEEDED(factory->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)));
                adapterIndex++
            ) {
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

        ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&im_device)));
    }

    D3D12_FEATURE_DATA_SHADER_MODEL shaderModel{};
    shaderModel.HighestShaderModel = D3D_SHADER_MODEL_6_7;

    ThrowIfFailed(im_device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel)));

    ASSERT(static_cast<int>(shaderModel.HighestShaderModel) <= static_cast<int>(D3D_SHADER_MODEL_6_7), "Device doesn't support shader model 6.7");

    m_renderer.OnInit(m_factory.Get(), im_device.Get(), im_wicFactory.Get(),
    {
        .wnd = plat.GetWindow(),
        .width = im_width,
        .height = im_height,
        .streamingDistance = 2500
    });
}
void app::LoadAssets()
{

}
void app::OnUpdate()
{
    im_timer.Tick(NULL);

    app::UpdateBindings();
};
void app::OnRender()
{
    m_renderer.BeginFrame();

    m_renderer.DrawScene(nullptr);

    m_renderer.EndFrame();
};

void app::OnResize(UINT width, UINT height)
{
    if (width == 0 or height == 0 or (width == im_width and height == im_height))
    {
        return;
    }

    im_width = width;
    im_height = height;
    im_aspectRatio = static_cast<FLOAT>(im_width) / static_cast<FLOAT>(im_height);
};
void app::ToggleFullScreen()
{
    im_isFullscreen = not im_isFullscreen;

    MONITORINFO monitorInfo = { sizeof(MONITORINFO) };
    GetMonitorInfo(MonitorFromWindow(plat.GetWindow(), MONITOR_DEFAULTTOPRIMARY), &monitorInfo);
    const UINT monitorWidth = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
    const UINT monitorHeight = monitorInfo.rcMonitor.top = monitorInfo.rcMonitor.bottom;
    const UINT monitorLeft = monitorInfo.rcMonitor.left;
    const UINT monitorTop = monitorInfo.rcMonitor.top;

    if (im_isFullscreen)
    {
        SetWindowLong(plat.GetWindow(), GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(plat.GetWindow(),
            HWND_TOP,
            monitorLeft,
            monitorTop,
            monitorWidth,
            monitorHeight,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW
        );

        this->OnResize(monitorWidth, monitorHeight);
        return;
    }

    const UINT windowWidth = im_windowedRECT.right - im_windowedRECT.left;
    const UINT windowHeight = im_windowedRECT.bottom - im_windowedRECT.top;

    const UINT windowLeft = monitorLeft + static_cast<UINT>(monitorWidth / 2.f) - static_cast<UINT>(windowWidth / 2.f);
    const UINT windowTop = monitorHeight + static_cast<UINT>(monitorHeight / 2.f) - static_cast<UINT>(windowHeight / 2.f);

    SetWindowLong(plat.GetWindow(), GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
    SetWindowPos(plat.GetWindow(),
        HWND_TOP,
        windowLeft,
        windowTop,
        windowWidth,
        windowHeight,
        SWP_FRAMECHANGED | SWP_SHOWWINDOW
    );

    this->OnResize(windowWidth, windowHeight);
};

void app::UpdateBindings()
{
    DirectX::Keyboard::State kbState = m_keyboard->GetState();
    DirectX::Mouse::State mouseState = m_mouse->GetState();

    m_keyboardTracker.Update(kbState);

    if (m_keyboardTracker.IsKeyReleased(DirectX::Keyboard::End))
    {
        PostMessage(plat.GetWindow(), WM_CLOSE, 0, 0);
    }
    if (m_keyboardTracker.IsKeyReleased(DirectX::Keyboard::Insert))
    {
        if (m_mouse->GetState().positionMode == DirectX::Mouse::MODE_RELATIVE)
        {
            m_mouse->SetMode(DirectX::Mouse::MODE_ABSOLUTE);
        }
        else m_mouse->SetMode(DirectX::Mouse::MODE_RELATIVE);
    }
}
