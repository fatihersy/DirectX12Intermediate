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
    m_frameConstantsCpuAddr(nullptr),
    m_meshConstantsCpuAddr(nullptr),
    m_frameIndex{},
    m_fenceEvent(nullptr),
    m_fenceGeneration{},
    m_aspectRatio{},
    m_camEye({ 0.f, 10.25f, 20.5f, 0.f }),
    m_camFwd({ 0.f, 0.f, -1.f, 0.f }),
    m_camUp({ 0.f, 1.f, 0.f, 0.f }),
    m_camYaw{},
    m_camPitch{},
    m_camSpeed(10.f),
    m_lookSensitivity(.1f),
    m_viewMatrix{}
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

    m_projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV4, m_aspectRatio, .01f, 500.f);

    m_lightDir = DirectX::XMVectorSet(0.f, -1.f, 0.f, 0.0f);
    //m_lightDir = DirectX::XMVectorSet(-0.577f, 0.577f, -0.577f, 0.0f);
    m_lightColor = DirectX::XMVectorSet(0.9f, 0.9f, 0.9f, 1.0f);

    ThrowIfFailed(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
    ThrowIfFailed(CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_wicFactory)));

    m_keyboard = std::make_unique<DirectX::Keyboard>();
    m_mouse = std::make_unique<DirectX::Mouse>();
    m_mouse->SetWindow(plat.GetHWND());
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
    
    m_pipeline.Reset();
    m_rootSignature.Reset();

    if (m_fallbackTexture.uploadBuffer) m_fallbackTexture.uploadBuffer.Reset();
    if (m_fallbackTexture.defaultBuffer) m_fallbackTexture.defaultBuffer.Reset();
    
    m_frameConstantsGpuResource.Reset();
    m_meshConstantsGpuResource.Reset();


    m_model.UnloadGPU();

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

void app::OnInit() {
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
                g_FError("No free SRV descriptors available");
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
                g_FError("Invalid SRV descriptor handle to free!");
                return;
            }

            INT idx = static_cast<INT>(offset / userData->im_imGuiSrvDescriptorSize);
            if (idx >= static_cast<INT>(info->SrvDescriptorHeap->GetDesc().NumDescriptors))
            {
                g_FError("SRV descriptor index out of bounds!");
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

    m_model.RotateAdd({ 0.f, 5.f * static_cast<FLOAT>(m_timer.GetElapsedSeconds()), 0.f });

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

    // Create synchronization objects and wait until assets have been uploaded to
    // the GPU.
    {
        ThrowIfFailed(m_device->CreateFence(m_fenceGeneration, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fence->SetName(L"app::m_fence");
        m_fenceGeneration++;

        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr) {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        WaitForGPU();
    }

    // SRV Descriptor Heaps
    {
        // Model SRV Descriptor
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

        // ImGui SRV Descriptor
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

        WaitForGPU();
    }

    // Root signature
    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE rootSignature{};
        rootSignature.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rootSignature, sizeof(rootSignature)))) {
            rootSignature.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        CD3DX12_DESCRIPTOR_RANGE1 srvRange[1]{};
        srvRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, static_cast<INT>(FTextureType::FTextureType_MAX), 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

        CD3DX12_ROOT_PARAMETER1 rp[3]{};
        rp[0].InitAsConstantBufferView(0, 0);
        rp[1].InitAsConstantBufferView(1, 0);
        rp[2].InitAsDescriptorTable(1, &srvRange[0], D3D12_SHADER_VISIBILITY_PIXEL);

        D3D12_STATIC_SAMPLER_DESC sampler{};
        sampler.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
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

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(_countof(rp), rp, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3D10Blob> signature;
        ComPtr<ID3D10Blob> error;
        HRESULT hr = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, rootSignature.HighestVersion, &signature, &error);
        if (FAILED(hr)) {
            if (error) {
                const char* errorMsg = reinterpret_cast<const char*>(error->GetBufferPointer());
                g_FError(errorMsg);
            }
            throw std::runtime_error("Failed to serialize root signature");
        }
        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
        m_rootSignature->SetName(L"app::m_rootSignature");
    }

    // Create the constant buffer memory and map the resource
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
            const size_t cbSize = c_maxObjects * sizeof(PaddedMeshConstants);

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
    }

    // Create the pipeline state, which includes compiling and loading shaders.
    {
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
        ComPtr<ID3D10Blob> vertexShader, pixelShader;
        // Vertex Shader
        {
            ComPtr<ID3D10Blob> error;
            HRESULT hr = D3DCompileFromFile((m_assetsPath + L"shader.hlsl").c_str(), nullptr, nullptr, "mainVS", "vs_5_0", compileFlags, 0, &vertexShader, &error);
            if (FAILED(hr)) {
                if (error) {
                    const char* errorMsg = reinterpret_cast<const char*>(error->GetBufferPointer());
                    g_FError(errorMsg);
                }
                ThrowIfFailed(hr);
            }
        }
        // Pixel Shader
        {
            ComPtr<ID3D10Blob> error;
            HRESULT hr = D3DCompileFromFile(GetAssetFullPath(L"shader.hlsl").c_str(), nullptr, nullptr, "mainPS", "ps_5_0", compileFlags, 0, &pixelShader, &error);
            if (FAILED(hr)) {
                if (error) {
                    const char* errorMsg = reinterpret_cast<const char*>(error->GetBufferPointer());
                    g_FError(errorMsg);
                }
                ThrowIfFailed(hr);
            }
        }

        D3D12_INPUT_ELEMENT_DESC inputElements[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };

        // Create the pipeline state objects, which includes compiling and loading
        // shaders.
        {
            D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
            desc.InputLayout = { inputElements, _countof(inputElements) };
            desc.pRootSignature = m_rootSignature.Get();
            desc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
            desc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
            desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
            desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
            desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
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
            ThrowIfFailed(m_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_pipeline)));
            m_pipeline->SetName(L"app::m_pipeline");
        }
    }

    m_model = Model(m_device.Get(), m_wicFactory.Get());
    m_model.m_rotation = { 0.f, 0.f, 0.f };
    {
        ThrowIfFailed(m_commandList->Close());
        ThrowIfFailed(m_commandList->Reset(m_commandAllocators[0].Get(), nullptr));

        m_model.Load(GetAssetFullPath(L"res/lowpoly_ramen_bowl.glb"), m_commandList.Get());

        m_model.UploadGPU(m_commandList.Get(), m_commandQueue.Get());
    }

    WaitForGPU();

    m_model.ResetUploadHeaps();
    m_fallbackTexture.uploadBuffer.Reset();
}
void app::PopulateCommandList()
{
    ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());

    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_pipeline.Get()));

    ID3D12DescriptorHeap* ppModelHeap[] = { im_modelSrvHeap.Get() };
    m_commandList->SetDescriptorHeaps(1, ppModelHeap);

    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);
    m_commandList->SetPipelineState(m_pipeline.Get());

    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTarget[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_commandList->ResourceBarrier(1, &barrier);
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    const float clearColor[] = { .18f, .2f, .41f, 1.f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

    m_commandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    UINT bufferIndex = (m_frameIndex % FrameCount);
    auto frameConstantGpuAddrBase = m_frameConstantsGpuVirtualAddr + sizeof(PaddedFrameConstants) * bufferIndex;

    frameConstants frameCB{};
    DirectX::XMStoreFloat4x4(&frameCB.viewMatrix, DirectX::XMMatrixTranspose(m_viewMatrix));
    DirectX::XMStoreFloat4x4(&frameCB.projectionMatrix, DirectX::XMMatrixTranspose(m_projectionMatrix));
    DirectX::XMStoreFloat4(&frameCB.lightDir, m_lightDir);
    DirectX::XMStoreFloat4(&frameCB.lightColor, m_lightColor);

    memcpy(&m_frameConstantsCpuAddr[0].constant, &frameCB, sizeof(frameConstants));

    m_commandList->SetGraphicsRootConstantBufferView(0, frameConstantGpuAddrBase);

    CD3DX12_GPU_DESCRIPTOR_HANDLE srvGPUHandle(im_modelSrvHeap->GetGPUDescriptorHandleForHeapStart());
    m_model.Draw({ m_commandList.Get(), srvGPUHandle, im_modelSrvDescriptorSize, bufferIndex, m_meshConstantsGpuVirtualAddr, m_meshConstantsCpuAddr });

    ID3D12DescriptorHeap* ppImGuiHeap[] = { im_imGuiSrvHeap.Get() };
    m_commandList->SetDescriptorHeaps(1, ppImGuiHeap);

    ImGui::Begin("Model");
    {
        const std::vector<Mesh>& meshes = m_model.GetMeshes();
        for (size_t meshIndex = 0; meshIndex < meshes.size(); meshIndex++)
        {
            const Mesh& mesh = meshes[meshIndex];

            ImGui::LabelText(FString::format("Mesh %d", meshIndex).c_str(), "Vertices: %u -- Indices: %u", mesh.vertexCount, mesh.indexCount);
        }
    }
    ImGui::End();

    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());

    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTarget[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        m_commandList->ResourceBarrier(1, &barrier);
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
        g_FError("Invalid allocation amount");
        *out_cpu_desc_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
        *out_gpu_desc_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
        return;
    }

    if (im_freeModelSRVindices.size() < static_cast<size_t>(allocAmount)) {
        g_FError("No free SRV descriptors available");
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
        g_FError("No contiguous SRV index list found");
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
