#include "stdafx.h"
#include "app.h"

#include "dxgidebug.h"

#include "DXSampleHelper.h"
#include "platform_win32.h"

#include "imgui.h"
#include "imgui_impl_dx12.h"

#include "RenderPass.h"
#include "Model.h"

IApp* IApp::s_instance = nullptr;

platform plat{};

app::app(UINT width, UINT height, std::wstring title, HINSTANCE hInstance, int nCmdShow) : IApp(width, height, title)
{
    s_instance = this;

    im_defaultWindowedRECT = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    im_isFullscreen = false;
    im_aspectRatio = static_cast<float>(width) / static_cast<float>(height);

    plat = platform(width, height, title, hInstance, nCmdShow, s_instance);

    im_assetsPath = std::filesystem::current_path().generic_wstring().append(L"/");

    WCHAR executablePath[512];
    GetExecutablePath(executablePath, _countof(executablePath));
    im_executablePath = executablePath;

    ThrowIfFailed(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
    ThrowIfFailed(CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_wicFactory)));

    m_keyboard = std::make_unique<DirectX::Keyboard>();
    m_mouse = std::make_unique<DirectX::Mouse>();
    m_mouse->SetWindow(plat.GetHWND());

    m_shaderCompiler = std::make_unique<ShaderCompiler>();
}
app::~app() {
    s_instance = nullptr;
}
void app::OnDestroy()
{
    ImGui_ImplDX12_Shutdown();
    ImGui::DestroyContext();
    m_imGuiSrvHeap.Reset();

    m_renderer.Execute([this](NSRenderer::Ctx ctx, NSRenderer::GraphicsCommandList cmdList){
        m_scene.OnDestroy(ctx);
    });

    m_mouse.release();
    m_mouse.reset();
    m_keyboard.release();
    m_keyboard.reset();

    m_renderer.OnDestroy();

    m_wicFactory.Reset();
    m_factory.Reset();
    m_device.Reset();

    ComPtr<IDXGIDebug1> dxgiDebug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
    {
        dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
    }
    dxgiDebug.Reset();
}

void app::OnInit()
{
    app::LoadPipeline();
    app::LoadAssets();

    // ImGui Init
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui_ImplDX12_InitInfo initInfo{};
        initInfo.UserData = s_instance;
        initInfo.Device = m_device.Get();
        initInfo.CommandQueue = m_renderer.ImGui_getCmdQueue();
        initInfo.NumFramesInFlight = IApp::ic_frameCount;
        initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        initInfo.DSVFormat = DXGI_FORMAT_D32_FLOAT;

        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.NumDescriptors = 100u;
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_imGuiSrvHeap)));
        m_imGuiSrvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        m_imGuiSrvHeap->SetName(L"app::im_imGuiSrvHeap");

        for (UINT i{}; i < desc.NumDescriptors; i++) m_freeImGuiSRVindices.push_back(i);
        
        initInfo.SrvDescriptorHeap = m_imGuiSrvHeap.Get();
        initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo * info, D3D12_CPU_DESCRIPTOR_HANDLE * out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE * out_gpu_desc_handle)
        {
            app* userData = static_cast<app*>(info->UserData);
            if (userData->m_freeImGuiSRVindices.empty())
            {
                g_FError("No free SRV descriptors available\n");
                *out_cpu_desc_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
                *out_gpu_desc_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
                return;
            }


            INT idx = userData->m_freeImGuiSRVindices.back();
            userData->m_freeImGuiSRVindices.pop_back();

            CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(info->SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
            *out_cpu_desc_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuHandle, idx, userData->m_imGuiSrvDescriptorSize);

            CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(info->SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
            *out_gpu_desc_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuHandle, idx, userData->m_imGuiSrvDescriptorSize);
        };
        initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo * info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_desc_handle)
        {
            app* userData = static_cast<app*>(info->UserData);

            CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(info->SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
            ptrdiff_t offset = cpu_desc_handle.ptr - cpuHandle.ptr;
            if (offset % userData->m_imGuiSrvDescriptorSize != 0 || offset < 0)
            {
                g_FError("Invalid SRV descriptor handle to free!\n");
                return;
            }

            INT idx = static_cast<INT>(offset / userData->m_imGuiSrvDescriptorSize);
            if (idx >= static_cast<INT>(info->SrvDescriptorHeap->GetDesc().NumDescriptors))
            {
                g_FError("SRV descriptor index out of bounds!\n");
                return;
            }

            userData->m_freeImGuiSRVindices.push_back(idx);
        };
        ImGui_ImplDX12_Init(&initInfo);
    }

    plat.PlatShowWindow();

    m_keyboardTracker.Reset();
}
void app::Run() {
    MSG msg {};

    while (msg.message != WM_QUIT) {
        plat.PlatMessageDispatch(msg);
    }
}
void app::OnUpdate() {
    im_timer.Tick(NULL);

    ImGui_ImplDX12_NewFrame();
    ImGui::NewFrame();

    app::UpdateKeyBindings();
    app::UpdateMouseBindings();

    m_scene.OnUpdate();
}
void app::OnRender()
{
    m_renderer.BeginFrame();

    m_renderer.DrawScene(m_scene);

    m_renderer.EndFrame();
}

void app::LoadPipeline() {
    UINT dxgiFactoryFlags = 0;

    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();

        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }

    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_factory)));

    // Seeking a compatible device
    {
        ComPtr<IDXGIAdapter1> adapter;

        for (UINT adapterIndex = 0; adapterIndex < DXGI_ERROR_NOT_FOUND != m_factory->EnumAdapters1(adapterIndex, &adapter); adapterIndex++) {
            DXGI_ADAPTER_DESC1 desc{};
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                continue;
            }
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_2, _uuidof(ID3D12Device), nullptr))) {
                break;
            }
        }
        ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&m_device)));
        m_device->SetName(L"app::m_device");

    }

    m_renderer.Init(m_factory.Get(), m_device.Get(), plat.GetHWND(), im_width, im_height);
}
void app::LoadAssets()
{
    m_renderer.Execute([this](NSRenderer::Ctx ctx, NSRenderer::GraphicsCommandList cmdList)
    {
        m_scene = Scene(m_device.Get(), m_wicFactory.Get(), { 0.f, 0.f, 100.f, 0.f }, 10.f, .1f, 12.f);

        m_scene.AddObject<SDome>(ctx, "Sky Dome", {}, SDome{
            .radius = 10000.f,
            .sliceCount = 64,
            .stackCount = 32
        });

        {
            const float stride = 11.f;
            const float gridStartPosX = 5.f * stride / -2.f;
            const float gridStartPosY = 5.f * stride / -2.f;

            int32_t idx{};
            for (size_t i = 0; i < 36u; i++)
            {
                const int32_t col = idx % 6;
                const int32_t row = idx / 6;
                const float metallic = col * .2f;
                const float roughness = row * .2f;
                const float posX = -col * stride - gridStartPosX;
                const float posY = row * stride + gridStartPosY;

                PrimitiveTraits<SSphere> desc({
                    .radius = 5.f,
                    .sliceCount = 20,
                    .stackCount = 20
                });

                Model& model = m_scene.AddObject<SSphere>(
                    ctx,
                    FString::format("Sphere%d", idx).c_str(),
                    { posX, posY, 0.f },
                    desc,
                    metallic,
                    roughness
                );
                idx++;
            }
        }

        for (Model& model : m_scene.m_models) {
            model.UploadGPU(ctx, cmdList);
        }

        m_renderer.AddPass<RenderPass::SkyDomePass>(m_device.Get(), ctx).SetEnabled(true);
        m_renderer.AddPass<RenderPass::GeometryPass>(m_device.Get(), ctx).SetEnabled(true);
    });
}

void app::UpdateKeyBindings()
{
    auto kbState = m_keyboard->GetState();
    m_keyboardTracker.Update(kbState);
    
    if (kbState.Escape) {
        m_mouse->SetMode(DirectX::Mouse::MODE_ABSOLUTE);
    }
    
    if (kbState.End) {
        PostMessage(plat.GetHWND(), WM_CLOSE, 0, 0);
    }

    if (kbState.Insert)
    {
        if (m_mouse->GetState().positionMode == DirectX::Mouse::MODE_RELATIVE)
        {
            m_mouse->SetMode(DirectX::Mouse::MODE_ABSOLUTE);
        }
        else  m_mouse->SetMode(DirectX::Mouse::MODE_RELATIVE);
    }
    
    // Camera Movement
    {
        DirectX::XMVECTOR move = DirectX::XMVectorZero();

        if (kbState.W) {
            move = DirectX::XMVectorAdd(move, m_scene.m_camera.camFwd);
        }
        if (kbState.S) {
            move = DirectX::XMVectorSubtract(move, m_scene.m_camera.camFwd);
        }
        if (kbState.A) {
            auto left = DirectX::XMVector3Cross(m_scene.m_camera.camFwd, m_scene.m_camera.camUp);
            left = DirectX::XMVector3Normalize(left);
            move = DirectX::XMVectorAdd(move, left);
        }
        if (kbState.D) {
            auto right = DirectX::XMVector3Cross(m_scene.m_camera.camUp, m_scene.m_camera.camFwd);
            right = DirectX::XMVector3Normalize(right);
            move = DirectX::XMVectorAdd(move, right);
        }
        if (kbState.Q) {
            move = DirectX::XMVectorAdd(move, m_scene.m_camera.camUp);
        }
        if (kbState.E) {
            move = DirectX::XMVectorSubtract(move, m_scene.m_camera.camUp);
        }

        if (DirectX::XMVector3Greater(DirectX::XMVector3LengthSq(move), DirectX::g_XMEpsilon)) {
            move = DirectX::XMVector3Normalize(move);
            move = DirectX::XMVectorScale(move, m_scene.m_camera.camSpeed * static_cast<FLOAT>(im_timer.GetElapsedSeconds()));
            m_scene.m_camera.camEye = DirectX::XMVectorAdd(m_scene.m_camera.camEye, move);
        }
    }
}
void app::UpdateMouseBindings()
{
    auto mouseState = m_mouse->GetState();

    if (mouseState.positionMode == DirectX::Mouse::MODE_RELATIVE) {
        FLOAT dx = static_cast<FLOAT>(mouseState.x) * m_scene.m_camera.lookSensitivity;
        FLOAT dy = static_cast<FLOAT>(mouseState.y) * m_scene.m_camera.lookSensitivity;

        m_scene.m_camera.camYaw += dx;
        m_scene.m_camera.camPitch -= dy;

        m_scene.m_camera.camPitch = std::clamp(m_scene.m_camera.camPitch, -89.f, 89.f);
    
        m_mouse->ResetScrollWheelValue();
    }
}

void app::OnResize(UINT width, UINT height)
{
    if (width == 0 or height == 0 or (width == im_width and height == im_height))
    {
        return;
    }

    im_width = width;
    im_height = height;
    im_aspectRatio = static_cast<FLOAT>(im_width) / static_cast<FLOAT>(im_height);

    m_renderer.Resize(im_width, im_height);

    DirectX::XMMATRIX& projectionMatrix = m_scene.m_camera.projectionMatrix;
    const DirectX::XMMATRIX oldProjection = projectionMatrix;
    projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV4, im_aspectRatio, m_scene.NEAR_CLIP, m_scene.FAR_CLIP);
}

void app::ToggleFullScreen()
{
    im_isFullscreen = not im_isFullscreen;

    MONITORINFO monitorInfo = { sizeof(MONITORINFO) };
    GetMonitorInfo(MonitorFromWindow(plat.GetHWND(), MONITOR_DEFAULTTOPRIMARY), &monitorInfo);
    const UINT monitorWidth = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
    const UINT monitorHeight = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
    const UINT monitorLeft = monitorInfo.rcMonitor.left;
    const UINT monitorTop = monitorInfo.rcMonitor.top;

    if (im_isFullscreen)
    {
        SetWindowLong(plat.GetHWND(), GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(plat.GetHWND(), HWND_TOP, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top, monitorWidth, monitorHeight, SWP_FRAMECHANGED | SWP_SHOWWINDOW);

        app::OnResize(monitorWidth, monitorHeight);
        return;
    }

    const UINT windowWidth  = im_defaultWindowedRECT.right - im_defaultWindowedRECT.left;
    const UINT windowHeight = im_defaultWindowedRECT.bottom - im_defaultWindowedRECT.top;

    const UINT windowLeft = monitorLeft + static_cast<UINT>(monitorWidth / 2.f) - static_cast<UINT>(windowWidth / 2.f);
    const UINT windowTop  = monitorTop + static_cast<UINT>(monitorHeight / 2.f) - static_cast<UINT>(windowHeight / 2.f);

    SetWindowLong(plat.GetHWND(), GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
    SetWindowPos(plat.GetHWND(), HWND_TOP, windowLeft, windowTop, windowWidth, windowHeight, SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    app::OnResize(windowWidth, windowHeight);
}
