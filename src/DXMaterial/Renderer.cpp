#include "stdafx.h"
#include "Renderer.h"
#include "Scene.h"

#include "DXSampleHelper.h"
#include "RenderPass.h"

void Renderer::Init(IDXGIFactory7* factory, ID3D12Device14* device, HWND hwnd, UINT width, UINT height)
{
    m_factory = factory;
    m_device = device;
    m_width = width;
    m_height = height;

    constexpr size_t OneMb = 1u * 1024 * 1024;
    m_allocator = ConstBuffAlloc(m_device, OneMb, IApp::ic_frameCount);

    // Describe and create the command queue.
    {
        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        ThrowIfFailed(m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_commandQueue)));
        m_commandQueue->SetName(L"Renderer::m_commandQueue");
    }

    CreateSwapChain(factory, hwnd, width, height);

    m_rtvHeap = Descriptor::StaticHeap(m_device, L"Renderer::m_rtvHeap", D3D12_DESCRIPTOR_HEAP_TYPE_RTV, IApp::ic_frameCount, false);
    m_dsvHeap = Descriptor::StaticHeap(m_device, L"Renderer::m_dsvHeap", D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1u, false);
    m_srvHeap = Descriptor::RingHeap(m_device, L"Renderer::m_srvHeap", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        IApp::ic_maxObjects * static_cast<UINT>(FTextureType::FTextureType_MAX) + 1024,
        IApp::ic_frameCount,
        1024u
    );
    
    // Create frame resources
    {
        Descriptor::Handle handle = m_rtvHeap.Allocate(IApp::ic_frameCount);
        for (UINT n = 0; n < IApp::ic_frameCount; n++)
        {
            ThrowIfFailed(m_swapchain->GetBuffer(n, IID_PPV_ARGS(&m_renderTarget[n])));
            m_device->CreateRenderTargetView(m_renderTarget[n].Get(), nullptr, m_rtvHeap.Offset(handle, n).cpuAddr);

            ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[n])));
        }
    }

    CreateDepthStencil(L"Renderer::m_depthStencil", {
        .format = DXGI_FORMAT_D32_FLOAT,
        .flags = D3D12_DSV_FLAG_NONE,
        .dimention = D3D12_DSV_DIMENSION_TEXTURE2D,
        .width = width,
        .height = height,
        .cpuHandle = m_dsvHeap.GetCpuStart(),
        .outDSV = m_depthStencil
    });
    
    // Command List
    {
        UINT frameIndex = m_swapchain->GetCurrentBackBufferIndex();
        ThrowIfFailed(m_device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&m_commandList)));
        m_commandList->SetName(L"Renderer::m_commandList");
    }

    // Create synchronization objects
    {
        ThrowIfFailed(m_device->CreateFence(m_fenceGeneration, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fence->SetName(L"Renderer::m_fence");
        m_fenceGeneration++;

        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr) {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }
    }

    CreateDefaultTexture();

    m_blackboard.Set<UINT>(NSRenderer::kRenderer_width, width);
    m_blackboard.Set<UINT>(NSRenderer::kRenderer_height, height);
    m_blackboard.Set<Descriptor::hOffset>(NSRenderer::kRenderer_fallbackSRV, OffsetSRV(m_fallbackTextureSRVHandle, 0));
}
Renderer::~Renderer() {}

void Renderer::OnDestroy()
{
    WaitForGPU();

    m_fallbackTexture = {};
    m_allocator = {};

    m_passes.clear();

    m_swapchain.Reset();

    for (UINT i = 0; i < IApp::ic_frameCount; i++) {
        m_renderTarget[i].Reset();
        m_commandAllocators[i].Reset();
    }

    m_depthStencil.Reset();
    m_commandList.Reset();

    m_srvHeap = {};
    m_dsvHeap = {};
    m_rtvHeap = {};

    if (m_fenceEvent) {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }
    m_fence.Reset();
    m_commandQueue.Reset();
}

void Renderer::DrawScene(Scene& scene)
{
    m_blackboard.Set<UINT>(NSRenderer::kRenderer_frameIndex, m_swapchain->GetCurrentBackBufferIndex());

    NSRenderer::Ctx rendererCtx = GetCtx();
    NSRenderer::GraphicsCommandList cmdList = NSRenderer::GraphicsCommandList(m_commandList.Get());

    for (auto& pass : m_passes)
    {
        pass->Execute(scene, rendererCtx, cmdList);
    }
}

void Renderer::BeginFrame()
{
    ThrowIfFailed(m_commandAllocators[m_swapchain->GetCurrentBackBufferIndex()]->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_swapchain->GetCurrentBackBufferIndex()].Get(), nullptr));

    {
        CD3DX12_RESOURCE_BARRIER barriers[] = {
            CD3DX12_RESOURCE_BARRIER::Transition(m_renderTarget[m_swapchain->GetCurrentBackBufferIndex()].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)
        };

        m_commandList->ResourceBarrier(_countof(barriers), barriers);
    }

    m_srvHeap.BeginFrame(m_swapchain->GetCurrentBackBufferIndex());
    m_allocator.BeginFrame(m_swapchain->GetCurrentBackBufferIndex());

    Descriptor::hOffset rtvHandle{};
    Descriptor::hOffset dsvHandle{};
    assert(m_rtvHeap.At(m_swapchain->GetCurrentBackBufferIndex(), rtvHandle));
    assert(m_dsvHeap.At(0u, dsvHandle));

    m_commandList->OMSetRenderTargets(1, &rtvHandle.cpuAddr, FALSE, &dsvHandle.cpuAddr);

    m_commandList->ClearRenderTargetView(rtvHandle.cpuAddr, CLEAR_COLOR, 0, nullptr);
    m_commandList->ClearDepthStencilView(dsvHandle.cpuAddr, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

    ID3D12DescriptorHeap* heaps[] = {
        const_cast<ID3D12DescriptorHeap*>(m_srvHeap.GetHeap())
    };
    m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    m_commandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}
void Renderer::EndFrame()
{
    {
        CD3DX12_RESOURCE_BARRIER barriers[] = {
            CD3DX12_RESOURCE_BARRIER::Transition(m_renderTarget[m_swapchain->GetCurrentBackBufferIndex()].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT)
        };

        m_commandList->ResourceBarrier(_countof(barriers), barriers);
    }

    ThrowIfFailed(m_commandList->Close());

    ID3D12CommandList* const cmdLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    ThrowIfFailed(m_swapchain->Present(1, 0));

    MoveToNextFrame();
}

void Renderer::Resize(UINT width, UINT height)
{
    WaitForGPU();

    for (UINT i = 0; i < IApp::ic_frameCount; i++)
    {
        m_renderTarget[i].Reset();
    }
    m_depthStencil.Reset();

    {
        DXGI_SWAP_CHAIN_DESC1 desc{};
        m_swapchain->GetDesc1(&desc);
        ThrowIfFailed(m_swapchain->ResizeBuffers(IApp::ic_frameCount, width, height, desc.Format, desc.Flags));
    }

    UINT frameIndex = m_swapchain->GetCurrentBackBufferIndex();

    for (UINT i = 0; i < IApp::ic_frameCount; i++)
    {
        Descriptor::hOffset rtvHandle{};
        assert(m_rtvHeap.At(i, rtvHandle));

        ThrowIfFailed(m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_renderTarget[i])));
        m_device->CreateRenderTargetView(m_renderTarget[i].Get(), nullptr, rtvHandle.cpuAddr);
    }

    {
        D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
        D3D12_CLEAR_VALUE clearVal = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.f, 0);
        ThrowIfFailed(m_device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearVal, IID_PPV_ARGS(&m_depthStencil)));
    }

    {
        D3D12_DEPTH_STENCIL_VIEW_DESC desc{};
        desc.Format = DXGI_FORMAT_D32_FLOAT;
        desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        desc.Flags = D3D12_DSV_FLAG_NONE;
        m_device->CreateDepthStencilView(m_depthStencil.Get(), &desc, m_dsvHeap.GetCpuStart());
    }

    m_blackboard.Set<UINT>(NSRenderer::kRenderer_width, width);
    m_blackboard.Set<UINT>(NSRenderer::kRenderer_height, height);

    {
        // Resize passes as well 
    }
}

void Renderer::MoveToNextFrame()
{
    UINT64 fenceGen = m_fenceGeneration;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fenceGen));
    m_fenceGeneration++;

    if (m_fence->GetCompletedValue() < fenceGen) {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fenceGen, m_fenceEvent));
        WaitForSingleObjectEx(m_fenceEvent, INFINITE, false);
    }
}
void Renderer::WaitForGPU()
{
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceGeneration));

    ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceGeneration, m_fenceEvent));
    WaitForSingleObjectEx(m_fenceEvent, INFINITE, false);

    m_fenceGeneration++;
}

void Renderer::CreateSwapChain(IDXGIFactory7* factory, HWND hwnd, UINT width, UINT height)
{
    IDXGIFactory7* pFactory = factory ? factory : m_factory;
    if (not pFactory)
        throw std::invalid_argument("No IDXGIFactory available");

    if (m_swapchain)
        m_swapchain.Reset();

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.BufferCount = IApp::ic_frameCount;
    desc.Width = width;
    desc.Height = height;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(m_factory->CreateSwapChainForHwnd(m_commandQueue.Get(), hwnd, &desc, nullptr, nullptr, &swapChain));

    ThrowIfFailed(m_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
    ThrowIfFailed(swapChain.As(&m_swapchain));
}
void Renderer::CreateDepthStencil(LPCWSTR name, NSRenderer::DepthStencilCreateDescription dsvDesc)
{
    D3D12_DEPTH_STENCIL_VIEW_DESC desc{};
    desc.Format = dsvDesc.format;
    desc.Flags = dsvDesc.flags;
    desc.ViewDimension = dsvDesc.dimention;

    D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, dsvDesc.width, dsvDesc.height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    D3D12_CLEAR_VALUE clearVal = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.f, 0);
    ThrowIfFailed(m_device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearVal, IID_PPV_ARGS(&dsvDesc.outDSV)));
    m_device->CreateDepthStencilView(dsvDesc.outDSV.Get(), &desc, dsvDesc.cpuHandle);
    dsvDesc.outDSV->SetName(name);
}
void Renderer::CreateDefaultTexture()
{
    FTexture& fbTex = m_fallbackTexture;
    Descriptor::Handle& fbHandle = m_fallbackTextureSRVHandle;
    ID3D12Device14* device = m_device;

    Execute([&fbTex, &fbHandle, &device](NSRenderer::Ctx ctx, NSRenderer::GraphicsCommandList cmdList){
        fbTex.textureType = FTextureType::FTextureType_DIFFUSE;
        fbTex.width = 64;
        fbTex.height = 64;
        fbTex.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        const UINT squareSize = fbTex.width / 8;

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = fbTex.width;
        desc.Height = fbTex.height;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = fbTex.format;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        CD3DX12_HEAP_PROPERTIES defaultHeapProp(D3D12_HEAP_TYPE_DEFAULT);
        ThrowIfFailed(device->CreateCommittedResource(
            &defaultHeapProp,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&fbTex.defaultBuffer))
        );
        fbTex.defaultBuffer->SetName(L"Renderer::m_fallbackTexture.defaultBuffer");

        CD3DX12_RESOURCE_BARRIER barrierDefaultBufferToCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(
            fbTex.defaultBuffer.Get(),
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COPY_DEST
        );
        cmdList.ResourceBarrier(1, &barrierDefaultBufferToCopyDest);

        fbTex.RowPitch = fbTex.width * 4;
        const UINT dataSize = fbTex.RowPitch * fbTex.height;
        CD3DX12_HEAP_PROPERTIES uploadHeapProp(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(dataSize);
        ThrowIfFailed(device->CreateCommittedResource(
            &uploadHeapProp,
            D3D12_HEAP_FLAG_NONE,
            &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&fbTex.uploadBuffer))
        );
        fbTex.uploadBuffer->SetName(L"Renderer::m_fallbackTexture.uploadBuffer");

        uint8_t* mappedData = nullptr;
        ThrowIfFailed(fbTex.uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData)));

        for (UINT y = 0; y < fbTex.height; y++) {
            uint32_t* row = reinterpret_cast<uint32_t*>(mappedData + y * fbTex.RowPitch);
            for (UINT x = 0; x < fbTex.width; x++) {
                bool isBlack = ((x / squareSize) + (y / squareSize)) % 2 == 0;
                row[x] = isBlack ? 0xFF000000 : 0xFFFF00FF;
            }
        }
        fbTex.uploadBuffer->Unmap(0, nullptr);

        D3D12_TEXTURE_COPY_LOCATION srcLoc{};
        srcLoc.pResource = fbTex.uploadBuffer.Get();
        srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLoc.PlacedFootprint.Footprint.Width = fbTex.width;
        srcLoc.PlacedFootprint.Footprint.Height = fbTex.height;
        srcLoc.PlacedFootprint.Footprint.Depth = 1;
        srcLoc.PlacedFootprint.Footprint.Format = fbTex.format;
        srcLoc.PlacedFootprint.Footprint.RowPitch = fbTex.RowPitch;

        D3D12_TEXTURE_COPY_LOCATION dstLoc{};
        dstLoc.pResource = fbTex.defaultBuffer.Get();
        dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex = 0;

        cmdList.CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(fbTex.defaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        cmdList.ResourceBarrier(1, &barrier);

        fbHandle = ctx.allocSRVStatic(1);
        fbTex.srvOffset = ctx.offsetSRV(fbHandle, 0u);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Format = desc.Format;
        device->CreateShaderResourceView(fbTex.defaultBuffer.Get(), &srvDesc, fbHandle.cpuAddr);
    });
}

void Renderer::Execute(FnRendererExecutionBody Record)
{
    MoveToNextFrame();

    UINT frameIndex = m_swapchain->GetCurrentBackBufferIndex();

    ThrowIfFailed(m_commandAllocators[frameIndex]->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[frameIndex].Get(), nullptr));

    Record(GetCtx(), NSRenderer::GraphicsCommandList(m_commandList.Get()));

    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList *const lists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(lists), lists);

    WaitForGPU();
}
