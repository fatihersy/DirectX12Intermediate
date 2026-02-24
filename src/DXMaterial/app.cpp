#include "stdafx.h"
#include "app.h"

#include "dxgidebug.h"

#include "DXSampleHelper.h"
#include "platform_win32.h"

#include "imgui.h"
#include "imgui_impl_dx12.h"

IApp* IApp::s_instance = nullptr;

platform plat{};

app::app(UINT width, UINT height, std::wstring title, HINSTANCE hInstance, int nCmdShow) : IApp(width, height, title),
    m_viewport(0.f, 0.f, static_cast<float>(width), static_cast<float>(height)),
    m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
    m_rtvDescriptorSize{},
    m_frameConstantsGpuVirtualAddr{},
    m_meshConstantsGpuVirtualAddr{},
    m_skyDomeConstantsGpuVirtualAddr{},
    m_skyDomeConstantsCpuAddr(nullptr),
    m_frameConstantsCpuAddr(nullptr),
    m_meshConstantsCpuAddr(nullptr),
    m_frameIndex{},
    m_fenceEvent(nullptr),
    m_fenceGeneration{},
    m_aspectRatio{},
    m_camEye({ 0.f, 0.f, 100.f, 0.f }),
    m_camFwd({ 0.f, 0.f, -1.f, 0.f }),
    m_camUp({ 0.f, 1.f, 0.f, 0.f }),
    m_camYaw{},
    m_camPitch{},
    m_camSpeed(10.f),
    m_lookSensitivity(.1f),
    m_viewMatrix{},
    m_isSkyDomeDirty(true),
    m_skyDomeConstantsUpload{},
    m_skyDomeConstantsDefault{}
{
    s_instance = this;

    m_defaultWindowedRECT = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    m_isFullscreen = false;

    plat = platform(width, height, title, hInstance, nCmdShow, s_instance);

    m_assetsPath = std::filesystem::current_path().generic_wstring().append(L"/");

    WCHAR executablePath[512];
    GetAssetsPath(executablePath, _countof(executablePath));
    m_executablePath = executablePath;

    m_aspectRatio = static_cast<float>(width) / static_cast<float>(height);

    m_projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV4, m_aspectRatio, .01f, 20000.f);

    m_lightDir = DirectX::XMVectorSet(0.f, -1.f, 0.f, 0.0f);
    //m_lightDir = DirectX::XMVectorSet(-0.577f, 0.577f, -0.577f, 0.0f);
    m_lightColor = DirectX::XMVectorSet(0.9f, 0.9f, 0.9f, 1.0f);

    ThrowIfFailed(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
    ThrowIfFailed(CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_wicFactory)));

    m_keyboard = std::make_unique<DirectX::Keyboard>();
    m_mouse = std::make_unique<DirectX::Mouse>();
    m_mouse->SetWindow(plat.GetHWND());

    m_skyDomeConstantsUpload.BetaR = { 5.802e-6f, 13.558e-6f, 33.1e-6f };
    m_skyDomeConstantsUpload.BetaMScatter = 3.996e-6f;
    m_skyDomeConstantsUpload.BetaMExtinct = 3.996e-6f / 0.9f;
    m_skyDomeConstantsUpload.MieG = 0.8f;
    m_skyDomeConstantsUpload.HR = 8000.0f;
    m_skyDomeConstantsUpload.HM = 1200.0f;
    m_skyDomeConstantsUpload.Rg = 6360000.0f;
    m_skyDomeConstantsUpload.Rt = 6420000.0f;
    m_skyDomeConstantsUpload.SunIntensity = 20.0f;
    DirectX::XMStoreFloat3(&m_skyDomeConstantsUpload.SunDir, DirectX::XMVector3Normalize(DirectX::XMVectorNegate(m_lightDir)));
}
app::~app() {
    s_instance = nullptr;
}
void app::OnDestroy()
{
    if (m_frameConstantsGpuResource) m_frameConstantsGpuResource->Unmap(0, nullptr);
    if (m_meshConstantsGpuResource) m_meshConstantsGpuResource->Unmap(0, nullptr);
    if (m_frameConstantsCpuAddr) m_frameConstantsCpuAddr = nullptr;
    if (m_meshConstantsCpuAddr) m_meshConstantsCpuAddr = nullptr;
        
    ImGui_ImplDX12_Shutdown();
    ImGui::DestroyContext();

    m_mouse.release();
    m_mouse.reset();
    m_keyboard.release();
    m_keyboard.reset();

    m_commandList.Reset();

    for (UINT i = 0; i < FrameCount; i++) m_commandAllocators[i].Reset();
    
    if (m_skyDomeConstantsGpuResource) m_skyDomeConstantsGpuResource->Unmap(0, nullptr);
    m_skyDomeConstantsCpuAddr = nullptr;

    m_skyDome.UnloadGPU();
    m_skyDomePipelineRoot.Reset();
    m_skyDomePipelineGraphics.Reset();
    m_skyDomePipelineTransmittance.Reset();
    m_skyDomePipelineScattering.Reset();
    m_skyDomeDescHeap.Reset();
    m_skyDomeConstantsGpuResource.Reset();
    m_trasmittanceLUT.Reset();
    m_scatteringLUT.Reset();

    if (m_fallbackTexture.uploadBuffer) m_fallbackTexture.uploadBuffer.Reset();
    if (m_fallbackTexture.defaultBuffer) m_fallbackTexture.defaultBuffer.Reset();
    
    m_frameConstantsGpuResource.Reset();
    m_meshConstantsGpuResource.Reset();

    std::for_each(m_sphere.begin(), m_sphere.end(), [](Model& model) {
        model.UnloadGPU();
    });

    im_modelSrvHeap.Reset();
    im_imGuiSrvHeap.Reset();
    im_fallbackTexSrvHeap.Reset();
    m_dsvHeap.Reset();
    m_rtvHeap.Reset();

    m_depthStencil.Reset();

    for (UINT i = 0; i < FrameCount; i++) m_renderTarget[i].Reset();
    
    m_swapchain.Reset();

    m_wicFactory.Reset();
    m_device.Reset();

    if (m_fence && m_commandQueue && m_fenceEvent) WaitForGPU();
    
    m_fence.Reset();
    m_commandQueue.Reset();

    if (m_fenceEvent)
    {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }

    ComPtr<IDXGIDebug1> dxgiDebug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
    {
        dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
    }
    dxgiDebug.Reset();
}

void app::OnInit()
{
    ThrowIfFailed(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&m_dxcLibrary)));
    ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_dxcCompiler)));
    ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_dxcUtils)));
    ThrowIfFailed(DxcCreateInstance(CLSID_DxcValidator, IID_PPV_ARGS(&m_dxcValidator)));
    ThrowIfFailed(m_dxcUtils->CreateDefaultIncludeHandler(&m_dxcIncludeHandler));

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
        initInfo.CommandQueue = m_commandQueue.Get();
        initInfo.NumFramesInFlight = FrameCount;
        initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        initInfo.DSVFormat = DXGI_FORMAT_D32_FLOAT;

        initInfo.SrvDescriptorHeap = im_imGuiSrvHeap.Get();
        initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo * info, D3D12_CPU_DESCRIPTOR_HANDLE * out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE * out_gpu_desc_handle)
        {
            app* userData = static_cast<app*>(info->UserData);
            if (userData->im_freeImGuiSRVindices.empty())
            {
                g_FError("No free SRV descriptors available\n");
                *out_cpu_desc_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
                *out_gpu_desc_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
                return;
            }


            INT idx = userData->im_freeImGuiSRVindices.back();
            userData->im_freeImGuiSRVindices.pop_back();

            CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(info->SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
            *out_cpu_desc_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuHandle, idx, userData->im_imGuiSrvDescriptorSize);

            CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(info->SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
            *out_gpu_desc_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuHandle, idx, userData->im_imGuiSrvDescriptorSize);
        };
        initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo * info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_desc_handle)
        {
            app* userData = static_cast<app*>(info->UserData);

            CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(info->SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
            ptrdiff_t offset = cpu_desc_handle.ptr - cpuHandle.ptr;
            if (offset % userData->im_imGuiSrvDescriptorSize != 0 || offset < 0)
            {
                g_FError("Invalid SRV descriptor handle to free!\n");
                return;
            }

            INT idx = static_cast<INT>(offset / userData->im_imGuiSrvDescriptorSize);
            if (idx >= static_cast<INT>(info->SrvDescriptorHeap->GetDesc().NumDescriptors))
            {
                g_FError("SRV descriptor index out of bounds!\n");
                return;
            }

            userData->im_freeImGuiSRVindices.push_back(idx);
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
    m_timer.Tick(NULL);

    ImGui_ImplDX12_NewFrame();
    ImGui::NewFrame();

    DirectX::XMFLOAT3 fcamEye{};
    DirectX::XMStoreFloat3(&fcamEye, m_camEye);
    m_skyDome.SetPosition(fcamEye);

    DirectX::XMStoreFloat3(&m_skyDomeConstantsUpload.SunDir, DirectX::XMVector3Normalize(DirectX::XMVectorNegate(m_lightDir)));

    app::UpdateKeyBindings();
    app::UpdateMouseBindings();
    app::UpdateCamera();
}
void app::OnRender() {
    PopulateCommandList();

    ID3D12CommandList* ppCommandList[]{ m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandList), ppCommandList);

    ThrowIfFailed(m_swapchain->Present(1, 0));

    MoveToNextFrame();
}
void app::WaitForGPU() {
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceGeneration));

    ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceGeneration, m_fenceEvent));
    WaitForSingleObjectEx(m_fenceEvent, INFINITE, false);

    m_fenceGeneration++;
}
void app::MoveToNextFrame() {
    UINT64 fenceGen = m_fenceGeneration;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fenceGen));
    m_fenceGeneration++;

    if (m_fence->GetCompletedValue() < fenceGen) {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fenceGen, m_fenceEvent));
        WaitForSingleObjectEx(m_fenceEvent, INFINITE, false);
    }

    m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();
}

void app::LoadPipeline() {
    UINT dxgiFactoryFlags = 0;

    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();

        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }

    ComPtr<IDXGIFactory7> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    // Seeking a compatible device
    {
        ComPtr<IDXGIAdapter1> adapter;

        for (UINT adapterIndex = 0; adapterIndex < DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(adapterIndex, &adapter); adapterIndex++) {
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

    // Describe and create the command queue.
    {
        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        ThrowIfFailed(m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_commandQueue)));
        m_commandQueue->SetName(L"app::m_commandQueue");
    }

    // Describe and create the swap chain.
    {
        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.BufferCount = FrameCount;
        desc.Width = m_width;
        desc.Height = m_height;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.SampleDesc.Count = 1;

        ComPtr<IDXGISwapChain1> swapChain;
        ThrowIfFailed(factory->CreateSwapChainForHwnd(m_commandQueue.Get(), plat.GetHWND(), &desc, nullptr, nullptr, &swapChain));

        ThrowIfFailed(factory->MakeWindowAssociation(plat.GetHWND(), DXGI_MWA_NO_ALT_ENTER));
        ThrowIfFailed(swapChain.As(&m_swapchain));
        m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();
    }

    // Create descriptor heaps.
    {
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
        rtvHeapDesc.NumDescriptors = FrameCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));
        m_rtvHeap->SetName(L"app::m_rtvHeap");

        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));
        m_dsvHeap->SetName(L"app::m_dsvHeap");

        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    // Create frame resources
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

        for (UINT n = 0; n < FrameCount; n++) {
            ThrowIfFailed(m_swapchain->GetBuffer(n, IID_PPV_ARGS(&m_renderTarget[n])));
            m_device->CreateRenderTargetView(m_renderTarget[n].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, m_rtvDescriptorSize);

            ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[n])));
        }
    }

    // Create the depth stencil view.
    {
        D3D12_DEPTH_STENCIL_VIEW_DESC viewDesc{};
        viewDesc.Format = DXGI_FORMAT_D32_FLOAT;
        viewDesc.Flags = D3D12_DSV_FLAG_NONE;
        viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

        D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, m_width, m_height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
        D3D12_CLEAR_VALUE clearVal = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.f, 0);
        ThrowIfFailed(m_device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearVal, IID_PPV_ARGS(&m_depthStencil)));
        CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
        m_device->CreateDepthStencilView(m_depthStencil.Get(), &viewDesc, dsvHandle);
        m_depthStencil->SetName(L"app::m_depthStencil");
    }
}
void app::LoadAssets() {
    // Command List
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex].Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
    m_commandList->SetName(L"app::m_commandList");

    // Create synchronization objects
    {
        ThrowIfFailed(m_device->CreateFence(m_fenceGeneration, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fence->SetName(L"app::m_fence");
        m_fenceGeneration++;

        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr) {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }
    }

    // Descriptor Heaps
    {
        // Model Descriptor
        {
            D3D12_DESCRIPTOR_HEAP_DESC desc{};
            desc.NumDescriptors = static_cast<UINT>(c_maxObjects * static_cast<UINT>(FTextureType::FTextureType_MAX));
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            ThrowIfFailed(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&im_modelSrvHeap)));
            im_modelSrvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            im_modelSrvHeap->SetName(L"IApp::im_modelSrvHeap");

            for (UINT i{}; i < desc.NumDescriptors; i++) im_freeModelSRVindices.push_back(i);
        }

        // ImGui Descriptor
        {
            D3D12_DESCRIPTOR_HEAP_DESC desc{};
            desc.NumDescriptors = 100u;
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            ThrowIfFailed(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&im_imGuiSrvHeap)));
            im_imGuiSrvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            im_imGuiSrvHeap->SetName(L"IApp::im_imGuiSrvHeap");

            for (UINT i{}; i < desc.NumDescriptors; i++) im_freeImGuiSRVindices.push_back(i);
        }

        // Sky Dome Descriptor
        {
            D3D12_DESCRIPTOR_HEAP_DESC desc{};
            desc.NumDescriptors = 4u;
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            ThrowIfFailed(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_skyDomeDescHeap)));
            m_skyDomeDescHeapSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            m_skyDomeDescHeap->SetName(L"IApp::m_skyDomeDescHeap");
        }
    }

    // Default texture
    {
        ThrowIfFailed(m_commandList->Close());
        ThrowIfFailed(m_commandList->Reset(m_commandAllocators[0].Get(), nullptr));

        m_fallbackTexture.textureType = FTextureType::FTextureType_DIFFUSE;
        m_fallbackTexture.width = 64;
        m_fallbackTexture.height = 64;
        m_fallbackTexture.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        const UINT squareSize = m_fallbackTexture.width / 8;

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = m_fallbackTexture.width;
        desc.Height = m_fallbackTexture.height;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = m_fallbackTexture.format;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        CD3DX12_HEAP_PROPERTIES defaultHeapProp(D3D12_HEAP_TYPE_DEFAULT);
        ThrowIfFailed(m_device->CreateCommittedResource(
            &defaultHeapProp,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&m_fallbackTexture.defaultBuffer))
        );
        m_fallbackTexture.defaultBuffer->SetName(L"app::m_fallbackTexture.defaultBuffer");

        CD3DX12_RESOURCE_BARRIER barrierDefaultBufferToCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(
            m_fallbackTexture.defaultBuffer.Get(),
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COPY_DEST
        );
        m_commandList->ResourceBarrier(1, &barrierDefaultBufferToCopyDest);

        m_fallbackTexture.RowPitch = m_fallbackTexture.width * 4;
        const UINT dataSize = m_fallbackTexture.RowPitch * m_fallbackTexture.height;
        CD3DX12_HEAP_PROPERTIES uploadHeapProp(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(dataSize);
        ThrowIfFailed(m_device->CreateCommittedResource(
            &uploadHeapProp,
            D3D12_HEAP_FLAG_NONE,
            &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_fallbackTexture.uploadBuffer))
        );
        m_fallbackTexture.uploadBuffer->SetName(L"app::m_fallbackTexture.uploadBuffer");

        uint8_t* mappedData = nullptr;
        ThrowIfFailed(m_fallbackTexture.uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData)));

        for (UINT y = 0; y < m_fallbackTexture.height; y++) {
            uint32_t* row = reinterpret_cast<uint32_t*>(mappedData + y * m_fallbackTexture.RowPitch);
            for (UINT x = 0; x < m_fallbackTexture.width; x++) {
                bool isBlack = ((x / squareSize) + (y / squareSize)) % 2 == 0;
                row[x] = isBlack ? 0xFF000000 : 0xFFFF00FF;
            }
        }
        m_fallbackTexture.uploadBuffer->Unmap(0, nullptr);

        D3D12_TEXTURE_COPY_LOCATION srcLoc{};
        srcLoc.pResource = m_fallbackTexture.uploadBuffer.Get();
        srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLoc.PlacedFootprint.Footprint.Width = m_fallbackTexture.width;
        srcLoc.PlacedFootprint.Footprint.Height = m_fallbackTexture.height;
        srcLoc.PlacedFootprint.Footprint.Depth = 1;
        srcLoc.PlacedFootprint.Footprint.Format = m_fallbackTexture.format;
        srcLoc.PlacedFootprint.Footprint.RowPitch = m_fallbackTexture.RowPitch;

        D3D12_TEXTURE_COPY_LOCATION dstLoc{};
        dstLoc.pResource = m_fallbackTexture.defaultBuffer.Get();
        dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex = 0;

        //ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr));
        m_commandList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_fallbackTexture.defaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        m_commandList->ResourceBarrier(1, &barrier);

        D3D12_DESCRIPTOR_HEAP_DESC srvDescHeapDesc{};
        srvDescHeapDesc.NumDescriptors = 1u;
        srvDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&srvDescHeapDesc, IID_PPV_ARGS(&im_fallbackTexSrvHeap)));
        im_fallbackTexSrvHeap->SetName(L"IApp::im_fallbackTexSrvHeap");
        im_fallbackSrvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        m_fallbackTexture.cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(im_fallbackTexSrvHeap->GetCPUDescriptorHandleForHeapStart(), 0, im_fallbackSrvDescriptorSize);
        //m_fallbackTexture.gpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(im_fallbackTexSrvHeap->GetGPUDescriptorHandleForHeapStart(), 0, im_fallbackSrvDescriptorSize);

        im_fallbackTextureCpuHandle = m_fallbackTexture.cpuHandle;

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Format = desc.Format;
        m_device->CreateShaderResourceView(m_fallbackTexture.defaultBuffer.Get(), &srvDesc, m_fallbackTexture.cpuHandle);

        ThrowIfFailed(m_commandList->Close());
        ID3D12CommandList* const ppCmdList[] = { m_commandList.Get() };
        m_commandQueue->ExecuteCommandLists(1, ppCmdList);

        WaitForGPU();
    }

    // Root signatures
    {}
    {
        // Model Pipeline Root
        {
            D3D12_FEATURE_DATA_ROOT_SIGNATURE rsData{};
            rsData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

            if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rsData, sizeof(rsData)))) {
                rsData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
            }

            CD3DX12_DESCRIPTOR_RANGE1 srvRange[1]{};
            srvRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, static_cast<INT>(FTextureType::FTextureType_MAX), 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

            CD3DX12_ROOT_PARAMETER1 rp[3]{};
            rp[0].InitAsConstantBufferView(0, 0);
            rp[1].InitAsConstantBufferView(1, 0);
            rp[2].InitAsDescriptorTable(1, &srvRange[0], D3D12_SHADER_VISIBILITY_PIXEL);

            D3D12_STATIC_SAMPLER_DESC sampler{};
            sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            sampler.MipLODBias = 0;
            sampler.MaxAnisotropy = 0;
            sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
            sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
            sampler.MinLOD = 0.f;
            sampler.MaxLOD = D3D12_FLOAT32_MAX;
            sampler.ShaderRegister = 0;
            sampler.RegisterSpace = 0;
            sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rsDesc;
            rsDesc.Init_1_1(_countof(rp), rp, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

            ComPtr<ID3D10Blob> signature;
            ComPtr<ID3D10Blob> error;
            HRESULT hr = D3DX12SerializeVersionedRootSignature(&rsDesc, rsData.HighestVersion, &signature, &error);
            if (FAILED(hr)) {
                if (error) {
                    const char* errorMsg = reinterpret_cast<const char*>(error->GetBufferPointer());
                    g_FError(errorMsg);
                }
                throw std::runtime_error("Failed to serialize root signature");
            }
            ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_modelPipelineRoot)));
            m_modelPipelineRoot->SetName(L"app::m_modelPipelineRoot");
        }

        // Sky Dome Pipeline Root
        {
            D3D12_FEATURE_DATA_ROOT_SIGNATURE rsData{};
            rsData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

            if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rsData, sizeof(rsData)))) {
                rsData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
            }

            CD3DX12_DESCRIPTOR_RANGE1 uavRange{};
            uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

            CD3DX12_DESCRIPTOR_RANGE1 srvRange{};
            srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

            CD3DX12_ROOT_PARAMETER1 rp[5]{};
            rp[0].InitAsConstantBufferView(0, 0); // Frame
            rp[1].InitAsConstantBufferView(1, 0); // Mesh
            rp[2].InitAsConstantBufferView(2, 0); // Sky dome constants
            rp[3].InitAsDescriptorTable(1, &uavRange);
            rp[4].InitAsDescriptorTable(1, &srvRange);

            D3D12_STATIC_SAMPLER_DESC sampler{};
            sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rsDesc;
            rsDesc.Init_1_1(_countof(rp), rp, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

            ComPtr<ID3D10Blob> signature;
            ComPtr<ID3D10Blob> error;
            HRESULT hr = D3DX12SerializeVersionedRootSignature(&rsDesc, rsData.HighestVersion, &signature, &error);
            if (FAILED(hr)) {
                if (error) {
                    const char* errorMsg = reinterpret_cast<const char*>(error->GetBufferPointer());
                    g_FError(errorMsg);
                }
                throw std::runtime_error("Failed to serialize sky dome root signature");
            }
            ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_skyDomePipelineRoot)));
            m_skyDomePipelineRoot->SetName(L"app::m_skyDomePipelineRoot");
        }
    }

    // Create the constant buffer memory
    {
        // Per frame 
        {
            const D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            const size_t cbSize = FrameCount * sizeof(PaddedFrameConstants);

            const D3D12_RESOURCE_DESC heapDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);
            ThrowIfFailed(m_device->CreateCommittedResource(
                &uploadHeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &heapDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(m_frameConstantsGpuResource.ReleaseAndGetAddressOf()))
            );
            m_frameConstantsGpuResource->SetName(L"app::m_frameConstantsGpuResource");
            CD3DX12_RANGE readRange(0, 0);
            ThrowIfFailed(m_frameConstantsGpuResource->Map(0, &readRange, reinterpret_cast<void**>(&m_frameConstantsCpuAddr)));

            m_frameConstantsGpuVirtualAddr = m_frameConstantsGpuResource->GetGPUVirtualAddress();
        }

        // Per mesh
        {
            const D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            const size_t cbSize = static_cast<size_t>(FrameCount) * c_maxObjects * sizeof(PaddedMeshConstants);

            const D3D12_RESOURCE_DESC heapDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);
            ThrowIfFailed(m_device->CreateCommittedResource(
                &uploadHeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &heapDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(m_meshConstantsGpuResource.ReleaseAndGetAddressOf()))
            );
            m_meshConstantsGpuResource->SetName(L"app::m_meshConstantsGpuResource");
            CD3DX12_RANGE readRange(0, 0);
            ThrowIfFailed(m_meshConstantsGpuResource->Map(0, &readRange, reinterpret_cast<void**>(&m_meshConstantsCpuAddr)));

            m_meshConstantsGpuVirtualAddr = m_meshConstantsGpuResource->GetGPUVirtualAddress();
        }

        // Sky Dome
        {
            const D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            const size_t cbSize = static_cast<size_t>(FrameCount) * sizeof(PaddedSkyDomeConstants);

            const D3D12_RESOURCE_DESC heapDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);
            ThrowIfFailed(m_device->CreateCommittedResource(
                &heapProp,
                D3D12_HEAP_FLAG_NONE,
                &heapDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(m_skyDomeConstantsGpuResource.ReleaseAndGetAddressOf()))
            );
            m_skyDomeConstantsGpuResource->SetName(L"app::m_skyDomeConstantsGpuResource");
            CD3DX12_RANGE readRange(0, 0);
            ThrowIfFailed(m_skyDomeConstantsGpuResource->Map(0, &readRange, reinterpret_cast<void**>(&m_skyDomeConstantsCpuAddr)));

            m_skyDomeConstantsGpuVirtualAddr = m_skyDomeConstantsGpuResource->GetGPUVirtualAddress();
        }
    }

    // Create the pipeline states, which includes compiling and loading shaders.
    {
        ComPtr<IDxcOperationResult> opResult;
        HRESULT hr{};
        auto validateOpResult = [](IDxcLibrary* library, HRESULT hr, IDxcOperationResult* opResult)
        {
            if (FAILED(hr)) throw std::runtime_error("Cannot validate");
        
            ComPtr<IDxcBlobEncoding> errorBlob;
            if (SUCCEEDED(opResult->GetErrorBuffer(&errorBlob))) {
                ComPtr<IDxcBlobEncoding> errorBlobUtf8;
                if (errorBlob.Get() && errorBlob->GetBufferSize() > 0)
                {
                    ThrowIfFailed(library->GetBlobAsUtf8(errorBlob.Get(), &errorBlobUtf8));
                    const char* errstr = reinterpret_cast<const char*>(errorBlobUtf8->GetBufferPointer());
                    size_t errlen = errorBlobUtf8->GetBufferSize();
                    if (errorBlobUtf8) g_FError("%s\n", std::string(errstr, errlen));
                }
            }
        };

        auto compileShader = [this, &validateOpResult, &hr](IDxcBlobEncoding* sourceBlob, ComPtr<IDxcBlob>& shader, std::vector<LPCWSTR> args)
        {
            DxcBuffer sourceBuffer{};
            sourceBuffer.Encoding = DXC_CP_ACP;
            sourceBuffer.Ptr = sourceBlob->GetBufferPointer();
            sourceBuffer.Size = sourceBlob->GetBufferSize();
        
            ComPtr<IDxcResult> compileResult;
            ThrowIfFailed(m_dxcCompiler->Compile(&sourceBuffer, args.data(), static_cast<UINT>(args.size()), m_dxcIncludeHandler.Get(), IID_PPV_ARGS(&compileResult)));
        
            ComPtr<IDxcBlobUtf8> error;
            ThrowIfFailed(compileResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&error), nullptr));
        
            if (error && error->GetStringLength() > 0)
                g_FError("Shader Error: %s\n", std::string(error->GetStringPointer(), error->GetStringLength()));
        
            compileResult->GetStatus(&hr);
            ThrowIfFailed(hr);
        
            ThrowIfFailed(compileResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader), nullptr));
        };

        // Model Pipeline
        {
            ComPtr<IDxcBlob> vertexShader;
            ComPtr<IDxcBlob> pixelShader;

            // Shader Compile
            {
                ComPtr<IDxcBlobEncoding> vertexSource;
                ComPtr<IDxcBlobEncoding> pixelSource;

                ThrowIfFailed(m_dxcUtils->LoadFile((m_assetsPath + L"VS.hlsl").c_str(), nullptr, &vertexSource));
                ThrowIfFailed(m_dxcUtils->LoadFile((m_assetsPath + L"PS.hlsl").c_str(), nullptr, &pixelSource));

                compileShader(vertexSource.Get(), vertexShader, {
                    L"-E", L"mainVS",
                    L"-T", L"vs_6_0",
                    L"-Zi",
                    L"-Od"
                });

                compileShader(pixelSource.Get(), pixelShader, {
                    L"-E", L"mainPS",
                    L"-T", L"ps_6_0",
                    L"-Zi",
                    L"-Od"
                });
  
                hr = m_dxcValidator->Validate(vertexShader.Get(), DxcValidatorFlags_Default, &opResult);
                validateOpResult(m_dxcLibrary.Get(), hr, opResult.Get());
                hr = m_dxcValidator->Validate(pixelShader.Get(), DxcValidatorFlags_Default, &opResult);
                validateOpResult(m_dxcLibrary.Get(), hr, opResult.Get());
            }

            D3D12_INPUT_ELEMENT_DESC inputElements[] = {
                {"POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, position),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                {"NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                {"TANGENT",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, tangent), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                {"BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, bitangent), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                {"TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(Vertex, texCoord), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            };

            // Pipeline
            {
                D3D12_RENDER_TARGET_BLEND_DESC blendDesc{};
                blendDesc.BlendEnable = TRUE;
                blendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
                blendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
                blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
                blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
                blendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
                blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
                blendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

                D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
                desc.InputLayout = { inputElements, _countof(inputElements) };
                desc.pRootSignature = m_modelPipelineRoot.Get();
                desc.VS = CD3DX12_SHADER_BYTECODE(vertexShader->GetBufferPointer(), vertexShader->GetBufferSize());
                desc.PS = CD3DX12_SHADER_BYTECODE(pixelShader->GetBufferPointer(), pixelShader->GetBufferSize());
                desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
                desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
                desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
                desc.BlendState.RenderTarget[0] = blendDesc;
                desc.DepthStencilState.DepthEnable = TRUE;
                desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
                desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
                desc.DepthStencilState.StencilEnable = FALSE;
                desc.NumRenderTargets = 1;
                desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
                desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
                desc.SampleDesc.Count = 1;
                desc.SampleMask = UINT_MAX;
                desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
                ThrowIfFailed(m_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_modelPipeline)));
                m_modelPipeline->SetName(L"app::m_modelPipeline");
            }
        }

        // Sky Dome Pipeline
        {}
        {
            ComPtr<IDxcBlob> vertexShader;
            ComPtr<IDxcBlob> pixelShader;
            ComPtr<IDxcBlob> transmittanceShader;
            ComPtr<IDxcBlob> scatteringShader;

            ComPtr<IDxcBlobEncoding> shaderSource;
            ThrowIfFailed(m_dxcUtils->LoadFile((m_assetsPath + L"SkyDome.hlsl").c_str(), nullptr, &shaderSource));

            compileShader(shaderSource.Get(), vertexShader, {
                L"-E", L"VS_Sky",
                L"-T", L"vs_6_0",
                L"-Zi",
                L"-Od"
            });

            compileShader(shaderSource.Get(), pixelShader, {
                L"-E", L"PS_Sky",
                L"-T", L"ps_6_0",
                L"-Zi",
                L"-Od"
            });

            compileShader(shaderSource.Get(), transmittanceShader, {
                L"-E", L"CS_Transmittance",
                L"-T", L"cs_6_0",
                L"-Zi",
                L"-Od",
                L"-D", L"COMPUTE_SHADER=1"
            });

            compileShader(shaderSource.Get(), scatteringShader, {
                L"-E", L"CS_Scattering",
                L"-T", L"cs_6_0",
                L"-Zi",
                L"-Od",
                L"-D", L"COMPUTE_SHADER=1"
            });

            hr = m_dxcValidator->Validate(vertexShader.Get(), DxcValidatorFlags_Default, &opResult);
            validateOpResult(m_dxcLibrary.Get(), hr, opResult.Get());
            hr = m_dxcValidator->Validate(pixelShader.Get(), DxcValidatorFlags_Default, &opResult);
            validateOpResult(m_dxcLibrary.Get(), hr, opResult.Get());
            hr = m_dxcValidator->Validate(transmittanceShader.Get(), DxcValidatorFlags_Default, &opResult);
            validateOpResult(m_dxcLibrary.Get(), hr, opResult.Get());
            hr = m_dxcValidator->Validate(scatteringShader.Get(), DxcValidatorFlags_Default, &opResult);
            validateOpResult(m_dxcLibrary.Get(), hr, opResult.Get());

            // Compute Pipeline
            {
                D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
                desc.pRootSignature = m_skyDomePipelineRoot.Get();
                desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

                desc.CS = CD3DX12_SHADER_BYTECODE(transmittanceShader->GetBufferPointer(), transmittanceShader->GetBufferSize());
                ThrowIfFailed(m_device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&m_skyDomePipelineTransmittance)));
                m_skyDomePipelineTransmittance->SetName(L"app::m_skyDomePipelineTransmittance");

                desc.CS = CD3DX12_SHADER_BYTECODE(scatteringShader->GetBufferPointer(), scatteringShader->GetBufferSize());
                ThrowIfFailed(m_device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&m_skyDomePipelineScattering)));
                m_skyDomePipelineScattering->SetName(L"app::m_skyDomePipelineScattering");
            }

            // Graphics Pipeline
            {
                D3D12_INPUT_ELEMENT_DESC inputElements[] = {
                    {"POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, position),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                    {"NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, normal),    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                    {"TANGENT",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, tangent),   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                    {"BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, bitangent), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                    {"TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(Vertex, texCoord),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                };

                CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
                rasterizerDesc.CullMode = D3D12_CULL_MODE_FRONT;

                CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc(D3D12_DEFAULT);
                depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
                depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

                CD3DX12_BLEND_DESC blendDesc(D3D12_DEFAULT);

                D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
                desc.pRootSignature = m_skyDomePipelineRoot.Get();
                desc.VS = CD3DX12_SHADER_BYTECODE(vertexShader->GetBufferPointer(), vertexShader->GetBufferSize());
                desc.PS = CD3DX12_SHADER_BYTECODE(pixelShader->GetBufferPointer(), pixelShader->GetBufferSize());
                desc.RasterizerState = rasterizerDesc;
                desc.DepthStencilState = depthStencilDesc;
                desc.BlendState = blendDesc;
                desc.InputLayout = { inputElements, _countof(inputElements) };
                desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
                desc.NumRenderTargets = 1;
                desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
                desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
                desc.SampleDesc.Count = 1;
                desc.SampleMask = UINT_MAX;
                ThrowIfFailed(m_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_skyDomePipelineGraphics)));
                m_skyDomePipelineGraphics->SetName(L"app::m_skyDomePipelineGraphics");
            }
        }
    }

    // Sky Dome.
    {}
    {
        auto desc23132 = CD3DX12_RESOURCE_DESC::Tex3D(
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            32, 128, 32 * 8
        );

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = 256;
        desc.Height = 64;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
        m_device->CreateCommittedResource(
            &defaultHeap,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&m_trasmittanceLUT)
        );
        m_trasmittanceLUT->SetName(L"TransmittenceLUT");

        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
        desc.Width = 32;
        desc.Height = 128;
        desc.DepthOrArraySize = 32 * 8;
        m_device->CreateCommittedResource(
            &defaultHeap,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&m_scatteringLUT)
        );
        m_scatteringLUT->SetName(L"ScatteringLUT");

        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_skyDomeDescHeap->GetCPUDescriptorHandleForHeapStart();

        m_device->CreateUnorderedAccessView(m_trasmittanceLUT.Get(), nullptr, nullptr, CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuHandle, 0, m_skyDomeDescHeapSize));
        m_device->CreateUnorderedAccessView(m_scatteringLUT.Get(), nullptr, nullptr, CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuHandle, 1, m_skyDomeDescHeapSize));

        D3D12_SHADER_RESOURCE_VIEW_DESC transmittanceSrv{};
        transmittanceSrv.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        transmittanceSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        transmittanceSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        transmittanceSrv.Texture2D.MipLevels = 1;
        m_device->CreateShaderResourceView(m_trasmittanceLUT.Get(), &transmittanceSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuHandle, 2, m_skyDomeDescHeapSize));
        
        D3D12_SHADER_RESOURCE_VIEW_DESC scatteringSrv{};
        scatteringSrv.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        scatteringSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
        scatteringSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        scatteringSrv.Texture3D.MipLevels = 1;
        m_device->CreateShaderResourceView(m_scatteringLUT.Get(), &scatteringSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuHandle, 3, m_skyDomeDescHeapSize));

        m_skyDome = Model("Sky Dome", m_device.Get(), m_wicFactory.Get());

        PrimitiveTraits<SDome> traits(SDome{
            .radius = 10000.f,
            .sliceCount = 64,
            .stackCount = 32
        });
        m_skyDome.As<SDome>(m_commandList.Get(), traits) ? 0 : throw std::runtime_error("Sky Dome creation failed");
    }

    // Creating the material grid
    {
        const float stride = 11.f;
        const float gridStartPosX = 5.f * stride / -2.f;
        const float gridStartPosY = 5.f * stride / -2.f;

        int32_t idx{};
        std::for_each(m_sphere.begin(), m_sphere.end(), [&](Model& model)
        {
            const int32_t col = idx % 6;
            const int32_t row = idx / 6;
            const float metallic = col * .2f;
            const float roughness = row * .2f;
            
            const float posX = -col * stride - gridStartPosX;
            const float posY = row * stride + gridStartPosY;
            
            model = Model(FString::format("Sphere%d", idx).c_str(), m_device.Get(), m_wicFactory.Get());
            
            PrimitiveTraits<SSphere> desc(SSphere{
                .radius = 5.f,
                .sliceCount = 20,
                .stackCount = 20
            });
            model.As<SSphere>(m_commandList.Get(), desc) ? 0 : throw std::runtime_error("Sphere create failed");
            
            model.SetPosition({ posX, posY, 0.f });
            model.SetMetallic(metallic);
            model.SetRoughness(roughness);
            idx++;
        });
    }

    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[0].Get(), nullptr));
    m_skyDome.UploadGPU(m_commandList.Get(), m_commandQueue.Get());
    WaitForGPU();

    std::for_each(m_sphere.begin(), m_sphere.end(), [this](Model& model) {
        ThrowIfFailed(m_commandList->Reset(m_commandAllocators[0].Get(), nullptr));
        model.UploadGPU(m_commandList.Get(), m_commandQueue.Get());
        WaitForGPU();
    });

    m_skyDome.ResetUploadHeaps();
    std::for_each(m_sphere.begin(), m_sphere.end(), [](Model& model) {
        model.ResetUploadHeaps();
    });

    m_fallbackTexture.uploadBuffer.Reset();
}
void app::PopulateCommandList()
{
    ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());

    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_modelPipeline.Get()));

    {
        CD3DX12_RESOURCE_BARRIER barriers[] = {
            CD3DX12_RESOURCE_BARRIER::Transition(m_renderTarget[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)
        };

        m_commandList->ResourceBarrier(_countof(barriers), barriers);
    }

    UINT frameCBoffset = (m_frameIndex % FrameCount);
    UINT meshCBoffset = frameCBoffset * c_maxObjects;
    unsigned long long frameConstantGpuAddrBase = m_frameConstantsGpuVirtualAddr + sizeof(PaddedFrameConstants) * frameCBoffset;

    frameConstants frameCB{};
    DirectX::XMStoreFloat4x4(&frameCB.viewMatrix, m_viewMatrix);
    DirectX::XMStoreFloat4x4(&frameCB.projectionMatrix, m_projectionMatrix);
    DirectX::XMStoreFloat4(&frameCB.lightDir, m_lightDir);
    DirectX::XMStoreFloat4(&frameCB.lightColor, m_lightColor);
    XMStoreFloat3(&frameCB.camPos, m_camEye);

    memcpy(&m_frameConstantsCpuAddr[frameCBoffset].constant, &frameCB, sizeof(frameConstants));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    const float clearColor[] = { .18f, .2f, .41f, 1.f };

    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

    // Sky Dome
    {
        unsigned long long skyDomeConstantGpuAddrBase = m_skyDomeConstantsGpuVirtualAddr + sizeof(PaddedSkyDomeConstants) * frameCBoffset;

        ID3D12DescriptorHeap* ppSkyDomeHeap[] = { m_skyDomeDescHeap.Get() };
        m_commandList->SetDescriptorHeaps(1, ppSkyDomeHeap);
        m_commandList->SetGraphicsRootSignature(m_skyDomePipelineRoot.Get());
        m_commandList->RSSetViewports(1, &m_viewport);
        m_commandList->RSSetScissorRects(1, &m_scissorRect);
        m_commandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        m_commandList->SetGraphicsRootConstantBufferView(0, frameConstantGpuAddrBase);
        m_commandList->SetGraphicsRootConstantBufferView(2, skyDomeConstantGpuAddrBase);

        CD3DX12_GPU_DESCRIPTOR_HANDLE srvTable(m_skyDomeDescHeap->GetGPUDescriptorHandleForHeapStart(), 2, m_skyDomeDescHeapSize);
        m_commandList->SetGraphicsRootDescriptorTable(4, srvTable);

        UpdateSkyDome();

        m_commandList->SetPipelineState(m_skyDomePipelineGraphics.Get());

        m_skyDome.Draw([this, &frameCBoffset, &meshCBoffset](Mesh& mesh, UINT meshIndex, DirectX::XMMATRIX worldMatrix)
        {
            auto meshConstantGpuAddrBase = m_meshConstantsGpuVirtualAddr + sizeof(PaddedMeshConstants) * meshCBoffset;
            
            meshConstants constants{};
            DirectX::XMStoreFloat4x4(&constants.worldMatrix, worldMatrix);
            DirectX::XMVECTOR det;
            DirectX::XMMATRIX worldInverse = DirectX::XMMatrixInverse(&det, worldMatrix);
            DirectX::XMMATRIX normalMatrix = DirectX::XMMatrixTranspose(worldInverse);
            DirectX::XMStoreFloat3x4(&constants.normalMatrix, normalMatrix);
            
            constants.baseColor = mesh.material.m_baseColor;
            constants.metallic = mesh.material.m_metallic;
            constants.roughness = mesh.material.m_roughness;
            constants.opacity = mesh.material.m_opacity;
            constants.textureFlags = mesh.material.m_textureFlags;
            
            memcpy(&m_meshConstantsCpuAddr[meshCBoffset].constant, &constants, sizeof(meshConstants));
            
            m_commandList->SetGraphicsRootConstantBufferView(1, meshConstantGpuAddrBase);
            
            m_commandList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
            m_commandList->IASetIndexBuffer(&mesh.indexBufferView);
            m_commandList->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
        });
        meshCBoffset += static_cast<UINT>(m_skyDome.GetMeshes().size());

        {
            CD3DX12_RESOURCE_BARRIER barriers[] = {
                CD3DX12_RESOURCE_BARRIER::Transition(m_trasmittanceLUT.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON),
                CD3DX12_RESOURCE_BARRIER::Transition(m_scatteringLUT.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON)
            };

            m_commandList->ResourceBarrier(_countof(barriers), barriers);
        }
    }

    // Objects
    {
        ID3D12DescriptorHeap* ppModelHeap[] = { im_modelSrvHeap.Get() };
        m_commandList->SetDescriptorHeaps(1, ppModelHeap);

        m_commandList->SetGraphicsRootSignature(m_modelPipelineRoot.Get());
        m_commandList->RSSetViewports(1, &m_viewport);
        m_commandList->RSSetScissorRects(1, &m_scissorRect);
        m_commandList->SetPipelineState(m_modelPipeline.Get());

        m_commandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        m_commandList->SetGraphicsRootConstantBufferView(0, frameConstantGpuAddrBase);

        CD3DX12_GPU_DESCRIPTOR_HANDLE srvGPUHandle(im_modelSrvHeap->GetGPUDescriptorHandleForHeapStart());

        std::for_each(m_sphere.begin(), m_sphere.end(), [this, &srvGPUHandle, &frameCBoffset, &meshCBoffset](Model& model) {
            model.Draw({ m_commandList.Get(), srvGPUHandle, im_modelSrvDescriptorSize, frameCBoffset, meshCBoffset, m_meshConstantsGpuVirtualAddr, m_meshConstantsCpuAddr });
            meshCBoffset += static_cast<UINT>(model.GetMeshes().size());
        });
    }

    // UI
    {
        ID3D12DescriptorHeap* ppImGuiHeap[] = { im_imGuiSrvHeap.Get() };
        m_commandList->SetDescriptorHeaps(1, ppImGuiHeap);

        ImGui::Begin("Model");
        {
            std::for_each(m_sphere.begin(), m_sphere.end(), [](Model& model)
                {
                    // Use CollapsingHeader to make each sphere a collapsible child section
                    if (ImGui::CollapsingHeader(model.m_name.c_str()))
                    {
                        DirectX::XMFLOAT3 pos = model.GetPosition();
                        if (ImGui::DragFloat3("Position", &pos.x, 0.1f, std::numeric_limits<float>::lowest(), std::numeric_limits<float>::max()))
                        {
                            model.SetPosition(pos);
                        }

                        float metallic = model.GetMetallic();
                        if (ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f))
                        {
                            model.SetMetallic(metallic);
                        }
                        model.SetMetallic(metallic);

                        float roughness = model.GetRoughness();
                        if (ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f))
                        {
                            model.SetRoughness(roughness);
                        }
                        model.SetRoughness(roughness);
                    }
                });
            if (ImGui::CollapsingHeader(m_skyDome.m_name.c_str()))
            {
                DirectX::XMFLOAT3 pos = m_skyDome.GetPosition();
                if (ImGui::DragFloat3("Position", &pos.x, 0.1f, std::numeric_limits<float>::lowest(), std::numeric_limits<float>::max()))
                {
                    m_skyDome.SetPosition(pos);
                }
            }
        }
        ImGui::End();

        ImGui::Render();
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());
    }

    {
        CD3DX12_RESOURCE_BARRIER barriers[] = {
            CD3DX12_RESOURCE_BARRIER::Transition(m_renderTarget[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT)
        };

        m_commandList->ResourceBarrier(_countof(barriers), barriers);
    }

    ThrowIfFailed(m_commandList->Close());
}

void app::UpdateKeyBindings() {

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
            move = DirectX::XMVectorAdd(move, m_camFwd);
        }
        if (kbState.S) {
            move = DirectX::XMVectorSubtract(move, m_camFwd);
        }
        if (kbState.A) {
            auto left = DirectX::XMVector3Cross(m_camFwd, m_camUp);
            left = DirectX::XMVector3Normalize(left);
            move = DirectX::XMVectorAdd(move, left);
        }
        if (kbState.D) {
            auto right = DirectX::XMVector3Cross(m_camUp, m_camFwd);
            right = DirectX::XMVector3Normalize(right);
            move = DirectX::XMVectorAdd(move, right);
        }
        if (kbState.Q) {
            move = DirectX::XMVectorAdd(move, m_camUp);
        }
        if (kbState.E) {
            move = DirectX::XMVectorSubtract(move, m_camUp);
        }
    
        if (DirectX::XMVector3Greater(DirectX::XMVector3LengthSq(move), DirectX::g_XMEpsilon)) {
            move = DirectX::XMVector3Normalize(move);
            move = DirectX::XMVectorScale(move, m_camSpeed * static_cast<FLOAT>(m_timer.GetElapsedSeconds()));
            m_camEye = DirectX::XMVectorAdd(m_camEye, move);
        }
    }
}
void app::UpdateMouseBindings() {
    auto mouseState = m_mouse->GetState();
    
    if (mouseState.positionMode == DirectX::Mouse::MODE_RELATIVE) {
        FLOAT dx = static_cast<FLOAT>(mouseState.x) * m_lookSensitivity;
        FLOAT dy = static_cast<FLOAT>(mouseState.y) * m_lookSensitivity;
    
        m_camYaw += dx;
        m_camPitch -= dy;
    
        m_camPitch = std::clamp(m_camPitch, -89.f, 89.f);
    
        m_mouse->ResetScrollWheelValue();
    }
}
void app::UpdateCamera() {
    DirectX::XMMATRIX rotMatrix = DirectX::XMMatrixRotationRollPitchYaw(DirectX::XMConvertToRadians(m_camPitch), DirectX::XMConvertToRadians(m_camYaw), 0.f);

    m_camFwd = DirectX::XMVector3TransformCoord({ 0.f, 0.f, -1.f, 0.f }, rotMatrix);
    m_camFwd = DirectX::XMVector3Normalize(m_camFwd);

    DirectX::XMVECTOR lookAt = DirectX::XMVectorAdd(m_camEye, m_camFwd);

    m_camUp = DirectX::XMVector3TransformCoord({ 0.f, 1.f, 0.f, 0.f }, rotMatrix);
    m_camUp = DirectX::XMVector3Normalize(m_camUp);

    m_viewMatrix = DirectX::XMMatrixLookAtLH(m_camEye, lookAt, m_camUp);
}
void app::UpdateSkyDome()
{
    if      (not Float3Equals(m_skyDomeConstantsUpload.BetaR, m_skyDomeConstantsDefault.BetaR)  ) m_isSkyDomeDirty = true;
    else if (m_skyDomeConstantsUpload.BetaMScatter != m_skyDomeConstantsDefault.BetaMScatter) m_isSkyDomeDirty = true;
    else if (m_skyDomeConstantsUpload.BetaMExtinct != m_skyDomeConstantsDefault.BetaMExtinct) m_isSkyDomeDirty = true;
    else if (m_skyDomeConstantsUpload.MieG         != m_skyDomeConstantsDefault.MieG        ) m_isSkyDomeDirty = true;
    else if (m_skyDomeConstantsUpload.HR           != m_skyDomeConstantsDefault.HR          ) m_isSkyDomeDirty = true;
    else if (m_skyDomeConstantsUpload.HM           != m_skyDomeConstantsDefault.HM          ) m_isSkyDomeDirty = true;
    else if (m_skyDomeConstantsUpload.Rg           != m_skyDomeConstantsDefault.Rg          ) m_isSkyDomeDirty = true;
    else if (m_skyDomeConstantsUpload.Rt           != m_skyDomeConstantsDefault.Rt          ) m_isSkyDomeDirty = true;
    else if (m_skyDomeConstantsUpload.SunIntensity != m_skyDomeConstantsDefault.SunIntensity) m_isSkyDomeDirty = true;
    else if (not Float3Equals(m_skyDomeConstantsUpload.SunDir, m_skyDomeConstantsDefault.SunDir)) m_isSkyDomeDirty = true;

    if (m_isSkyDomeDirty)
    {
        m_skyDomeConstantsDefault = m_skyDomeConstantsUpload;

        UINT frameCBoffset = (m_frameIndex % FrameCount);
        memcpy(&m_skyDomeConstantsCpuAddr[frameCBoffset].constant, &m_skyDomeConstantsDefault, sizeof(skyDomeConstants));

        unsigned long long skyDomeConstantGpuAddrBase = m_skyDomeConstantsGpuVirtualAddr + sizeof(PaddedSkyDomeConstants) * frameCBoffset;

        ID3D12DescriptorHeap* ppHeaps[] = { m_skyDomeDescHeap.Get() };
        m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
        m_commandList->SetComputeRootSignature(m_skyDomePipelineRoot.Get());
        m_commandList->SetComputeRootConstantBufferView(2, skyDomeConstantGpuAddrBase);

        CD3DX12_GPU_DESCRIPTOR_HANDLE heapStart(m_skyDomeDescHeap->GetGPUDescriptorHandleForHeapStart());

        // Pass 1: Transmittance
        {
            CD3DX12_RESOURCE_BARRIER barriers[] = {
                CD3DX12_RESOURCE_BARRIER::Transition(m_trasmittanceLUT.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
            };
            m_commandList->ResourceBarrier(_countof(barriers), barriers);

            m_commandList->SetPipelineState(m_skyDomePipelineTransmittance.Get());
            m_commandList->SetComputeRootDescriptorTable(3, CD3DX12_GPU_DESCRIPTOR_HANDLE(heapStart, 0, m_skyDomeDescHeapSize));
            m_commandList->Dispatch(
                (256 + 7) / 8,
                (64 + 7) / 8,
                1
            );
        }

        // Pass 2: Scattering
        {
            CD3DX12_RESOURCE_BARRIER barriers[] = {
                CD3DX12_RESOURCE_BARRIER::UAV(m_trasmittanceLUT.Get()),
                CD3DX12_RESOURCE_BARRIER::Transition(m_trasmittanceLUT.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
                CD3DX12_RESOURCE_BARRIER::Transition(m_scatteringLUT.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
            };
            m_commandList->ResourceBarrier(_countof(barriers), barriers);

            m_commandList->SetPipelineState(m_skyDomePipelineScattering.Get());
            m_commandList->SetComputeRootDescriptorTable(3, CD3DX12_GPU_DESCRIPTOR_HANDLE(heapStart, 1, m_skyDomeDescHeapSize));
            m_commandList->SetComputeRootDescriptorTable(4, CD3DX12_GPU_DESCRIPTOR_HANDLE(heapStart, 2, m_skyDomeDescHeapSize));
            m_commandList->Dispatch(
                (32 + 3) / 4,
                (128 + 3) / 4,
                (256 + 3) / 4
            );
        }

        {
            CD3DX12_RESOURCE_BARRIER barriers[] = {
                CD3DX12_RESOURCE_BARRIER::UAV(m_scatteringLUT.Get()),
                CD3DX12_RESOURCE_BARRIER::Transition(m_trasmittanceLUT.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
                CD3DX12_RESOURCE_BARRIER::Transition(m_scatteringLUT.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
            };

            m_commandList->ResourceBarrier(_countof(barriers), barriers);
        }

        m_isSkyDomeDirty = false;
    }
    else
    {
        CD3DX12_RESOURCE_BARRIER barriers[] = {
            CD3DX12_RESOURCE_BARRIER::Transition(m_trasmittanceLUT.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
            CD3DX12_RESOURCE_BARRIER::Transition(m_scatteringLUT.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        };

        m_commandList->ResourceBarrier(_countof(barriers), barriers);
    }
}

void app::OnResize(UINT width, UINT height) {
    if (width == 0 or height == 0 or (width == m_width and height == m_height))
    {
        return;
    }

    m_width = width;
    m_height = height;
    m_aspectRatio = static_cast<FLOAT>(m_width) / static_cast<FLOAT>(m_height);

    WaitForGPU();

    for (UINT i = 0; i < FrameCount; i++)
    {
        m_renderTarget[i].Reset();
    }
    m_depthStencil.Reset();

    {
        DXGI_SWAP_CHAIN_DESC1 desc{};
        m_swapchain->GetDesc1(&desc);
        ThrowIfFailed(m_swapchain->ResizeBuffers(FrameCount, m_width, m_height, desc.Format, desc.Flags));
    }

    m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < FrameCount; i++)
    {
        ThrowIfFailed(m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_renderTarget[i])));
        m_device->CreateRenderTargetView(m_renderTarget[i].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, m_rtvDescriptorSize);
    }

    {
        D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, m_width, m_height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
        D3D12_CLEAR_VALUE clearVal = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.f, 0);
        ThrowIfFailed(m_device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearVal, IID_PPV_ARGS(&m_depthStencil)));
    }

    {
        D3D12_DEPTH_STENCIL_VIEW_DESC desc{};
        desc.Format = DXGI_FORMAT_D32_FLOAT;
        desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        desc.Flags = D3D12_DSV_FLAG_NONE;
        CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
        m_device->CreateDepthStencilView(m_depthStencil.Get(), &desc, dsvHandle);
    }

    m_viewport = CD3DX12_VIEWPORT(0.f, 0.f, static_cast<FLOAT>(m_width), static_cast<FLOAT>(m_height));
    m_scissorRect = CD3DX12_RECT(0L, 0L, static_cast<LONG>(m_width), static_cast<LONG>(m_height));

    const DirectX::XMMATRIX oldProjection = m_projectionMatrix;
    m_projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV4, m_aspectRatio, .01f, 500.f);
}

void app::ToggleFullScreen()
{
    m_isFullscreen = not m_isFullscreen;

    MONITORINFO monitorInfo = { sizeof(MONITORINFO) };
    GetMonitorInfo(MonitorFromWindow(plat.GetHWND(), MONITOR_DEFAULTTOPRIMARY), &monitorInfo);
    const UINT monitorWidth = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
    const UINT monitorHeight = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
    const UINT monitorLeft = monitorInfo.rcMonitor.left;
    const UINT monitorTop = monitorInfo.rcMonitor.top;

    if (m_isFullscreen)
    {
        SetWindowLong(plat.GetHWND(), GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(plat.GetHWND(), HWND_TOP, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top, monitorWidth, monitorHeight, SWP_FRAMECHANGED | SWP_SHOWWINDOW);

        app::OnResize(monitorWidth, monitorHeight);
        return;
    }

    const UINT windowWidth  = m_defaultWindowedRECT.right - m_defaultWindowedRECT.left;
    const UINT windowHeight = m_defaultWindowedRECT.bottom - m_defaultWindowedRECT.top;

    const UINT windowLeft = monitorLeft + static_cast<UINT>(monitorWidth / 2.f) - static_cast<UINT>(windowWidth / 2.f);
    const UINT windowTop  = monitorTop + static_cast<UINT>(monitorHeight / 2.f) - static_cast<UINT>(windowHeight / 2.f);

    SetWindowLong(plat.GetHWND(), GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
    SetWindowPos(plat.GetHWND(), HWND_TOP, windowLeft, windowTop, windowWidth, windowHeight, SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    app::OnResize(windowWidth, windowHeight);
}

void IApp::modelSrvAlloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle, int allocAmount)
{
    if (allocAmount <= 0) {
        g_FError("Invalid allocation amount\n");
        *out_cpu_desc_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
        *out_gpu_desc_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
        return;
    }

    if (im_freeModelSRVindices.size() < static_cast<size_t>(allocAmount)) {
        g_FError("No free SRV descriptors available\n");
        *out_cpu_desc_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
        *out_gpu_desc_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
        return;
    }

    std::sort(im_freeModelSRVindices.begin(), im_freeModelSRVindices.end());

    int baseIdx = -1;
    size_t eraseStartPos = -1;

    for (size_t i = 0u; i <= im_freeModelSRVindices.size() - static_cast<size_t>(allocAmount); ++i) {
        int start = im_freeModelSRVindices[i];
        bool isContiguous = true;
        for (int j = 1; j < allocAmount; ++j) {
            if (im_freeModelSRVindices[i + static_cast<size_t>(j)] != start + j) {
                isContiguous = false;
                break;
            }
        }
        if (isContiguous) {
            baseIdx = start;
            eraseStartPos = i;
            break;
        }
    }

    if (baseIdx == -1) {
        g_FError("No contiguous SRV index list found\n");
        *out_cpu_desc_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
        *out_gpu_desc_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
        return;
    }

    im_freeModelSRVindices.erase(
        im_freeModelSRVindices.begin() + eraseStartPos,
        im_freeModelSRVindices.begin() + eraseStartPos + static_cast<size_t>(allocAmount)
    );

    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(im_modelSrvHeap->GetCPUDescriptorHandleForHeapStart(), baseIdx, im_modelSrvDescriptorSize);
    *out_cpu_desc_handle = cpuHandle;

    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(im_modelSrvHeap->GetGPUDescriptorHandleForHeapStart(), baseIdx, im_modelSrvDescriptorSize);
    *out_gpu_desc_handle = gpuHandle;
};
void IApp::modelSrvFree(D3D12_CPU_DESCRIPTOR_HANDLE cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_desc_handle)
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(im_modelSrvHeap->GetCPUDescriptorHandleForHeapStart());
    ptrdiff_t offset = cpu_desc_handle.ptr - cpuHandle.ptr;
    if (offset % im_modelSrvDescriptorSize != 0 || offset < 0)
    {
        g_FError("Invalid SRV descriptor handle to free!");
        return;
    }
    
    INT idx = static_cast<INT>(offset / im_modelSrvDescriptorSize);
    if (idx >= static_cast<INT>(im_modelSrvHeap->GetDesc().NumDescriptors))
    {
        g_FError("SRV descriptor index out of bounds!");
        return;
    }

    im_freeModelSRVindices.push_back(idx);
};
