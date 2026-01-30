#include "stdafx.h"
#include "app.h"
#include "platform_win32.h"
#include "DXSampleHelper.h"

platform plat{};

app::app(UINT width, UINT height, std::wstring title, HINSTANCE hInstance, int nCmdShow) : IApp(width, height, title),
    m_viewport(0.f, 0.f, static_cast<float>(width), static_cast<float>(height)),
    m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
    m_rtvDescriptorSize{},
    m_constantDataGpuVirtualAddr{},
    m_constantDataCpuAddr(nullptr),
    m_frameIndex{},
    m_fenceEvent(nullptr),
    m_fenceGeneration{},
    m_aspectRatio{},
    m_angle{}
{
    plat = platform(width, height, title, hInstance, nCmdShow, this);

    WCHAR assetsPath[512];
    GetAssetsPath(assetsPath, _countof(assetsPath));
    m_assetsPath = assetsPath;

    m_aspectRatio = static_cast<float>(width) / static_cast<float>(height);

    m_worldMatrix = DirectX::XMMatrixIdentity();

    static const DirectX::XMVECTORF32 c_eye{ 0.f, 50.f, -50.f, 0.f};
    static const DirectX::XMVECTORF32 c_at { 0.f, 10.f,  0.f, 0.f };
    static const DirectX::XMVECTORF32 c_up { 0.f, 1.f,  0.f, 0.f };
    m_viewMatrix = DirectX::XMMatrixLookAtLH(c_eye, c_at, c_up);

    m_projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV4, m_aspectRatio, 01.f, 500.f);

    m_lightDir = DirectX::XMVectorSet(-0.577f, 0.577f, -0.577f, 0.0f);
    //m_lightDir = DirectX::XMVectorSet(0.f, -1.f, 0.f, 0.0f);
    m_lightColor = DirectX::XMVectorSet(0.9f, 0.9f, 0.9f, 1.0f);

}
app::~app() {}
void app::OnDestroy() {}

void app::OnInit()
{
    app::LoadPipeline();
    app::LoadAssets();
    plat.PlatShowWindow();
}
void app::Run()
{
    MSG msg = { 0 };

    while (msg.message != WM_QUIT)
    {
        plat.PlatMessageDispatch(msg);
    }
}
void app::OnUpdate()
{
    static const float rotationSpeed = .01f;

    m_angle += rotationSpeed;

    if (m_angle >= DirectX::XM_2PI)
    {
        m_angle -= DirectX::XM_2PI;
    }

    m_worldMatrix = DirectX::XMMatrixRotationY(m_angle);
}
void app::OnRender()
{
    PopulateCommandList();

    ID3D12CommandList* ppCommandList[]{ m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandList), ppCommandList);

    ThrowIfFailed(m_swapchain->Present(1, 0));

    MoveToNextFrame();
}
void app::WaitForGPU()
{
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceGeneration));

    ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceGeneration, m_fenceEvent));
    WaitForSingleObjectEx(m_fenceEvent, INFINITE, false);

    m_fenceGeneration++;
}
void app::MoveToNextFrame()
{
    UINT64 fenceGen = m_fenceGeneration;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fenceGen));
    m_fenceGeneration++;

    if (m_fence->GetCompletedValue() < fenceGen)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fenceGen, m_fenceEvent));
        WaitForSingleObjectEx(m_fenceEvent, INFINITE, false);
    }

    m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();
}

void app::LoadPipeline()
{
    UINT dxgiFactoryFlags = 0;

    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();

        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }

    ComPtr<IDXGIFactory7> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    // Seeking a compatible device
    {
        ComPtr<IDXGIAdapter1> adapter;

        for (UINT adapterIndex = 0; adapterIndex < DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(adapterIndex, &adapter); adapterIndex++)
        {
            DXGI_ADAPTER_DESC1 desc{};
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                continue;
            }
            if(SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_2, _uuidof(ID3D12Device), nullptr)))
            {
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
        ThrowIfFailed(factory->CreateSwapChainForHwnd(
            m_commandQueue.Get(),
            plat.GetHWND(),
            &desc,
            nullptr,
            nullptr,
            &swapChain
        ));

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

        for (UINT n = 0; n < FrameCount; n++)
        {
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
        ThrowIfFailed(m_device->CreateCommittedResource(
            &heapProp,
            D3D12_HEAP_FLAG_NONE,
            &resDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clearVal,
            IID_PPV_ARGS(&m_depthStencil)
        ));
        CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
        m_device->CreateDepthStencilView(m_depthStencil.Get(), &viewDesc, dsvHandle);
    }
}
void app::LoadAssets()
{
    //Root signature
    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE rootSignature{};
        rootSignature.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rootSignature, sizeof(rootSignature))))
        {
            rootSignature.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        CD3DX12_ROOT_PARAMETER1 rp[1]{};
        rp[0].InitAsConstantBufferView(0, 0);

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(_countof(rp), rp, 0, nullptr,
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
        );

        ComPtr<ID3D10Blob> signature;
        ComPtr<ID3D10Blob> error;
        HRESULT hr = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, rootSignature.HighestVersion, &signature, &error);
        if (FAILED(hr))
        {
            if (error)
            {
                const char* errorMsg = reinterpret_cast<const char*>(error->GetBufferPointer());
                OutputDebugStringA(errorMsg);
            }
            ThrowIfFailed(hr);
        }
        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
    }

    // Create the constant buffer memory and map the resource
    {
        const D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        size_t cbSize = ((UINT64)c_maxObjects) * FrameCount * sizeof(PaddedConstantBuffer);

        const D3D12_RESOURCE_DESC heapDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);
        ThrowIfFailed(m_device->CreateCommittedResource(
            &uploadHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &heapDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(m_perFrameConstants.ReleaseAndGetAddressOf())
        ));
        ThrowIfFailed(m_perFrameConstants->Map(0, nullptr, reinterpret_cast<void**>(&m_constantDataCpuAddr)));

        m_constantDataGpuVirtualAddr = m_perFrameConstants->GetGPUVirtualAddress();
    }

    // Create the pipeline state, which includes compiling and loading shaders.
    {
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
        ComPtr<ID3D10Blob> vertexShader, pixelShader;
        // Vertex Shader
        {
            ComPtr<ID3D10Blob> error;
            HRESULT hr = D3DCompileFromFile(GetAssetFullPath(L"shader.hlsl").c_str(), nullptr, nullptr, "mainVS", "vs_5_0", compileFlags, 0, &vertexShader, &error);
            if (FAILED(hr))
            {
                if (error)
                {
                    const char* errorMsg = reinterpret_cast<const char*>(error->GetBufferPointer());
                    OutputDebugStringA(errorMsg);
                }
                ThrowIfFailed(hr);
            }
        }
        // Pixel Shader
        {
            ComPtr<ID3D10Blob> error;
            HRESULT hr = D3DCompileFromFile(GetAssetFullPath(L"shader.hlsl").c_str(), nullptr, nullptr, "mainPS", "ps_5_0", compileFlags, 0, &pixelShader, &error);
            if (FAILED(hr))
            {
                if (error)
                {
                    const char* errorMsg = reinterpret_cast<const char*>(error->GetBufferPointer());
                    OutputDebugStringA(errorMsg);
                }
                ThrowIfFailed(hr);
            }
        }

        D3D12_INPUT_ELEMENT_DESC inputElements[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };

        // Create the pipeline state objects, which includes compiling and loading shaders.
        {
            D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
            desc.InputLayout = { inputElements, _countof(inputElements)};
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

    // Command List 
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex].Get(), nullptr, IID_PPV_ARGS(&m_commandList)));

    m_crateModel = Model(m_device.Get());
    {
        std::string _path = WStringToString(GetAssetFullPath(L"crate\\crate_mesh.obj"));
        m_crateModel.Load(_path, m_commandList.Get());
        m_crateModel.UploadGPU(m_commandList.Get(), m_commandQueue.Get());
    }

    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(m_device->CreateFence(m_fenceGeneration, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceGeneration++;

        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        WaitForGPU();
    }

    m_crateModel.ResetUploadHeaps();
}
void app::PopulateCommandList()
{
    ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());

    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_pipeline.Get()));

    //ID3D12DescriptorHeap* ppHeaps[] = { m_srvHeap.Get()};

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

    const float clearColor[] = { .18f, .2f, .41f, 1.f};
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

    m_commandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ConstantBuffer cbParams{};
    DirectX::XMStoreFloat4x4(&cbParams.worldMatrix, DirectX::XMMatrixTranspose(m_worldMatrix));
    DirectX::XMStoreFloat4x4(&cbParams.viewMatrix, DirectX::XMMatrixTranspose(m_viewMatrix));
    DirectX::XMStoreFloat4x4(&cbParams.projectionMatrix, DirectX::XMMatrixTranspose(m_projectionMatrix));
    DirectX::XMStoreFloat4(&cbParams.lightDir,  m_lightDir);
    DirectX::XMStoreFloat4(&cbParams.lightColor, m_lightColor);

    UINT constantBufferIndex = (m_frameIndex % FrameCount) * c_maxObjects;
    memcpy(&m_constantDataCpuAddr[constantBufferIndex], &cbParams, sizeof(cbParams));

    auto baseGpuAddr = m_constantDataGpuVirtualAddr + sizeof(PaddedConstantBuffer) * constantBufferIndex;
    m_commandList->SetGraphicsRootConstantBufferView(0, baseGpuAddr);

    m_crateModel.Draw(m_commandList.Get());

    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTarget[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        m_commandList->ResourceBarrier(1, &barrier);
    }

    ThrowIfFailed(m_commandList->Close());
}


void app::OnKeyDown(UINT8 key) {}
void app::OnKeyUp(UINT8 key)
{
    if (key == VK_ESCAPE)
    {
        PostQuitMessage(0);
    }
}
