#include "stdafx.h"
#include "app.h"

#include "dxgidebug.h"

#include "DXSampleHelper.h"
#include "platform_win32.h"

platform plat{};

app::app(UINT width, UINT height, std::wstring title, HINSTANCE hInstance, int nCmdShow) : IApp(width, height, title),
    m_viewport(0.f, 0.f, static_cast<float>(width), static_cast<float>(height)),
    m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
    m_rtvDescriptorSize{},
    m_srvDescriptorSize{},
    m_maxObjects{},
    m_constantDataGpuVirtualAddr{},
    m_constantDataCpuAddr(nullptr),
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
    plat = platform(width, height, title, hInstance, nCmdShow, this);

    m_assetsPath = std::filesystem::current_path().generic_wstring().append(L"/");

    WCHAR executablePath[512];
    GetAssetsPath(executablePath, _countof(executablePath));
    m_executablePath = executablePath;

    m_aspectRatio = static_cast<float>(width) / static_cast<float>(height);

    m_projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV4, m_aspectRatio, 01.f, 500.f);

    // m_lightDir = DirectX::XMVectorSet(0.f, -1.f, 0.f, 0.0f);
    m_lightDir = DirectX::XMVectorSet(-0.577f, 0.577f, -0.577f, 0.0f);
    m_lightColor = DirectX::XMVectorSet(0.9f, 0.9f, 0.9f, 1.0f);

    ThrowIfFailed(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
    ThrowIfFailed(CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_wicFactory)));

    m_keyboard = std::make_unique<DirectX::Keyboard>();
    m_mouse = std::make_unique<DirectX::Mouse>();
    m_mouse->SetWindow(plat.GetHWND());
}
app::~app() {
}
void app::OnDestroy()
{
    if (m_constantDataCpuAddr && m_perFrameConstants)
    {
        m_perFrameConstants->Unmap(0, nullptr);
        m_constantDataCpuAddr = nullptr;
    }

    m_mouse.release();
    m_mouse.reset();
    m_keyboard.release();
    m_keyboard.reset();

    m_commandList.Reset();
    for (UINT i = 0; i < FrameCount; i++)
    {
        m_commandAllocators[i].Reset();
    }

    m_pipeline.Reset();
    m_rootSignature.Reset();

    m_fallbackTextureUpload.Reset();
    m_fallbackTexture.Reset();

    m_perFrameConstants.Reset();

    m_srvHeap.Reset();
    m_dsvHeap.Reset();
    m_rtvHeap.Reset();

    m_depthStencil.Reset();

    for (UINT i = 0; i < FrameCount; i++)
    {
        m_renderTarget[i].Reset();
    }

    m_swapchain.Reset();


    m_wicFactory.Reset();
    m_device.Reset();

    m_model.UnloadGPU();

    if (m_fence && m_commandQueue && m_fenceEvent)
    {
        WaitForGPU();
    }

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
    }

    // Describe and create the command queue.
    {
        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        ThrowIfFailed(m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_commandQueue)));
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

        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

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
    }
}
void app::LoadAssets() {
    // Command List
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex].Get(), nullptr, IID_PPV_ARGS(&m_commandList)));

    // Create synchronization objects and wait until assets have been uploaded to
    // the GPU.
    {
        ThrowIfFailed(m_device->CreateFence(m_fenceGeneration, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceGeneration++;

        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr) {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        WaitForGPU();
    }

    m_model = Model(m_device.Get(), m_wicFactory.Get());
    m_model.m_rotation = { 0.f, 0.f, 0.f };
    {
        m_model.Load(GetAssetFullPath(L"res/lowpoly_ramen_bowl.glb"), m_commandList.Get());

        // ThrowIfFailed(m_commandList->Reset(m_commandAllocators[0].Get(),
        // nullptr));

        m_model.UploadGPU(m_commandList.Get(), m_commandQueue.Get());

        WaitForGPU();

        m_model.ResetUploadHeaps();
    }

    m_maxObjects = static_cast<UINT>(m_model.GetMeshes().size());

    // Root signature
    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE rootSignature{};
        rootSignature.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rootSignature, sizeof(rootSignature)))) {
            rootSignature.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        CD3DX12_DESCRIPTOR_RANGE1 srvRange[1]{};
        srvRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

        CD3DX12_ROOT_PARAMETER1 rp[2]{};
        rp[0].InitAsConstantBufferView(0, 0);
        rp[1].InitAsDescriptorTable(1, &srvRange[0], D3D12_SHADER_VISIBILITY_PIXEL);

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
    }

    // Create the constant buffer memory and map the resource
    {
        const D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        size_t cbSize = ((UINT64)m_maxObjects) * FrameCount * sizeof(PaddedConstantBuffer);

        const D3D12_RESOURCE_DESC heapDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);
        ThrowIfFailed(m_device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &heapDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_perFrameConstants.ReleaseAndGetAddressOf())));
        CD3DX12_RANGE readRange(0, 0);
        ThrowIfFailed(m_perFrameConstants->Map(0, &readRange, reinterpret_cast<void**>(&m_constantDataCpuAddr)));

        m_constantDataGpuVirtualAddr = m_perFrameConstants->GetGPUVirtualAddress();
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
        }
    }

    // Default texture
    {
        const UINT width = 64;
        const UINT height = 64;
        const UINT squareSize = width / 8;

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = width;
        desc.Height = height;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        CD3DX12_HEAP_PROPERTIES defaultHeapProp(D3D12_HEAP_TYPE_DEFAULT);
        ThrowIfFailed(m_device->CreateCommittedResource(&defaultHeapProp, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_fallbackTexture)));

        const UINT rowPitch = width * 4;
        const UINT dataSize = rowPitch * height;
        CD3DX12_HEAP_PROPERTIES uploadHeapProp(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(dataSize);
        ThrowIfFailed(m_device->CreateCommittedResource(&uploadHeapProp, D3D12_HEAP_FLAG_NONE, &uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_fallbackTextureUpload)));

        uint8_t* mappedData = nullptr;
        ThrowIfFailed(m_fallbackTextureUpload->Map(0, nullptr, reinterpret_cast<void**>(&mappedData)));

        for (UINT y = 0; y < height; y++) {
            uint32_t* row = reinterpret_cast<uint32_t*>(mappedData + y * rowPitch);
            for (UINT x = 0; x < width; x++) {
                bool isBlack = ((x / squareSize) + (y / squareSize)) % 2 == 0;
                row[x] = isBlack ? 0xFF000000 : 0xFFFFFFFF;
            }
        }
        m_fallbackTextureUpload->Unmap(0, nullptr);

        D3D12_TEXTURE_COPY_LOCATION srcLoc{};
        srcLoc.pResource = m_fallbackTextureUpload.Get();
        srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLoc.PlacedFootprint.Footprint.Width = width;
        srcLoc.PlacedFootprint.Footprint.Height = height;
        srcLoc.PlacedFootprint.Footprint.Depth = 1;
        srcLoc.PlacedFootprint.Footprint.Format = desc.Format;
        srcLoc.PlacedFootprint.Footprint.RowPitch = rowPitch;

        D3D12_TEXTURE_COPY_LOCATION dstLoc{};
        dstLoc.pResource = m_fallbackTexture.Get();
        dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex = 0;

        ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr));
        m_commandList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_fallbackTexture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        m_commandList->ResourceBarrier(1, &barrier);

        ThrowIfFailed(m_commandList->Close());
        ID3D12CommandList* ppCmdLists[] = { m_commandList.Get() };
        m_commandQueue->ExecuteCommandLists(_countof(ppCmdLists), ppCmdLists);
    }

    WaitForGPU();

    // Shader Resource Views
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{};
    srvHeapDesc.NumDescriptors = static_cast<UINT>(m_model.GetMeshes().size()); // 1 texture for every meshes
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));

    m_srvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // SRV Creation
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_srvHeap->GetCPUDescriptorHandleForHeapStart());
        UINT textureIndex = 0;
        for (const auto& mesh : m_model.GetMeshes()) {

            D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
            desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            desc.Texture2D.MipLevels = 1;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            ID3D12Resource* texResource = nullptr;
            if (mesh.defaultDiffuseTexture) {
                texResource = mesh.defaultDiffuseTexture.Get();
            }
            else {
                texResource = m_fallbackTexture.Get();
            }

            m_device->CreateShaderResourceView(texResource, &desc, srvHandle);
            srvHandle.Offset(1, m_srvDescriptorSize);
            textureIndex++;
        }
    }

    m_fallbackTextureUpload.Reset();
}
void app::PopulateCommandList() {
    ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());

    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_pipeline.Get()));

    ID3D12DescriptorHeap* ppHeaps[] = { m_srvHeap.Get() };
    m_commandList->SetDescriptorHeaps(1, ppHeaps);

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

    UINT constantBufferIndex = (m_frameIndex % FrameCount) * m_maxObjects;
    auto baseGpuAddr = m_constantDataGpuVirtualAddr + sizeof(PaddedConstantBuffer) * constantBufferIndex;
    CD3DX12_GPU_DESCRIPTOR_HANDLE srvGPUHandle(m_srvHeap->GetGPUDescriptorHandleForHeapStart());

    ConstantBuffer cbParams{};

    DirectX::XMStoreFloat4x4(&cbParams.viewMatrix, DirectX::XMMatrixTranspose(m_viewMatrix));
    DirectX::XMStoreFloat4x4(&cbParams.projectionMatrix, DirectX::XMMatrixTranspose(m_projectionMatrix));
    DirectX::XMStoreFloat4(&cbParams.lightDir, m_lightDir);
    DirectX::XMStoreFloat4(&cbParams.lightColor, m_lightColor);

    m_model.RotateAdd({ 0.f, 1.f * static_cast<FLOAT>(m_timer.GetElapsedSeconds()), 0.f });
    m_model.Draw({ m_commandList.Get(), srvGPUHandle, baseGpuAddr, m_constantDataCpuAddr, cbParams, constantBufferIndex, m_srvDescriptorSize });

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
    
    if (mouseState.leftButton and mouseState.positionMode == DirectX::Mouse::MODE_ABSOLUTE) {
        m_mouse->SetMode(DirectX::Mouse::MODE_RELATIVE);
    }
    
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
