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

    m_rtvHeap = Descriptor::StaticHeap(m_device, L"Renderer::m_rtvHeap", D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1024u, false);
    m_dsvHeap = Descriptor::StaticHeap(m_device, L"Renderer::m_dsvHeap", D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1024u, false);
    m_srvHeap = Descriptor::RingHeap(m_device, L"Renderer::m_srvHeap", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        IApp::ic_maxObjects * static_cast<UINT>(FTextureType::FTextureType_MAX) + 1024,
        IApp::ic_frameCount,
        1024u
    );
    
    // Create frame resources
    {
        m_rtHandle = m_rtvHeap.Allocate(IApp::ic_frameCount);
        for (UINT n = 0; n < IApp::ic_frameCount; n++)
        {
            ThrowIfFailed(m_swapchain->GetBuffer(n, IID_PPV_ARGS(&m_renderTarget[n])));
            m_device->CreateRenderTargetView(m_renderTarget[n].Get(), nullptr, m_rtvHeap.Offset(m_rtHandle, n).cpuAddr);

            ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[n])));
        }
    }

    CreateDepthStencil(L"Renderer::m_depthStencil", {
        .format = DXGI_FORMAT_D32_FLOAT,
        .flags = D3D12_DSV_FLAG_NONE,
        .dimention = D3D12_DSV_DIMENSION_TEXTURE2D,
        .width = width,
        .height = height,
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

    std::vector<NSRenderer::Model> models;
    m_blackboard.Set<
        std::vector<NSRenderer::Model>>(NSRenderer::kRenderer_models, models
    );


    // Passes. Add in execution order
    {
        AddPass<RenderPass::SkyDomePass>(device, m_blackboard, GetCtx()).SetEnabled(true);

        AddPass<RenderPass::EnvironmentCubemapPass>(device, m_blackboard, GetCtx()).SetEnabled(true);

        AddPass<RenderPass::GeometryPass>(device, m_blackboard, GetCtx()).SetEnabled(true);
    }

    Execute([this](NSRenderer::Ctx ctx, NSRenderer::GraphicsCommandList cmdList)
    {
        for (auto& pass : m_passes)
        {
            pass->OnInit(m_blackboard, GetCtx(), cmdList);
        }
    });
}
Renderer::~Renderer() {}

void Renderer::OnDestroy()
{
    WaitForGPU();

    m_fallbackTexture = {};
    m_allocator = {};

    m_blackboard.Reset();
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
        pass->Execute(scene, m_blackboard, rendererCtx, cmdList);
    }
}
RegisterModelKey Renderer::RegisterModel(SceneModelKey key, NSRenderer::GraphicsCommandList cmdList)
{
    std::vector<NSRenderer::Model>* models = m_blackboard.GetMut<std::vector<NSRenderer::Model>>(NSRenderer::kRenderer_models);

    RegisterModelKey outKey{ key.id, models->size() };
    NSRenderer::Model& rendererModel = models->emplace_back();
    rendererModel.sceneKey = key;

    // Environment Cubemap
    {
        // Cubemap Texture
        {
            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            desc.Width = rendererModel.m_envCubemap.PER_FACE_RESOLUTION;
            desc.Height = rendererModel.m_envCubemap.PER_FACE_RESOLUTION;
            desc.DepthOrArraySize = rendererModel.m_envCubemap.NUM_FACES;
            desc.MipLevels = rendererModel.m_envCubemap.MIP_COUNT;
            desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

            CD3DX12_HEAP_PROPERTIES props(D3D12_HEAP_TYPE_DEFAULT);

            const float clearColor[] = { 0.f, 0.f, 0.f, 1.f };
            D3D12_CLEAR_VALUE clearVal = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R16G16B16A16_FLOAT, clearColor);

            ThrowIfFailed(m_device->CreateCommittedResource(
                &props,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_COMMON,
                &clearVal,
                IID_PPV_ARGS(&rendererModel.m_envCubemap.cubemapTexture)
            ));
        }

        // Cubemap Depth Texture
        {
            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            desc.Width = rendererModel.m_envCubemap.PER_FACE_RESOLUTION;
            desc.Height = rendererModel.m_envCubemap.PER_FACE_RESOLUTION;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.Format = DXGI_FORMAT_D32_FLOAT;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

            CD3DX12_HEAP_PROPERTIES props(D3D12_HEAP_TYPE_DEFAULT);

            D3D12_CLEAR_VALUE clearVal{};
            clearVal.Format = DXGI_FORMAT_D32_FLOAT;
            clearVal.DepthStencil.Depth = 1.f;

            ThrowIfFailed(m_device->CreateCommittedResource(
                &props,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                &clearVal,
                IID_PPV_ARGS(&rendererModel.m_envCubemap.cubemapDepth)
            ));
        }

        // RTVs
        {
            rendererModel.m_envCubemap.rtvHandle = AllocRTVStatic(rendererModel.m_envCubemap.NUM_FACES);
            for (UINT face = 0u; face < rendererModel.m_envCubemap.NUM_FACES; face++)
            {
                D3D12_RENDER_TARGET_VIEW_DESC desc{};
                desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
                desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                desc.Texture2DArray.MipSlice = 0;
                desc.Texture2DArray.FirstArraySlice = face;
                desc.Texture2DArray.ArraySize = 1;

                m_device->CreateRenderTargetView(
                    rendererModel.m_envCubemap.cubemapTexture.Get(),
                    &desc,
                    OffsetRTV(rendererModel.m_envCubemap.rtvHandle, face).cpuAddr
                );
            }
        }

        // DSV
        {
            rendererModel.m_envCubemap.dsvHandle = AllocDSVStatic(1u);

            D3D12_DEPTH_STENCIL_VIEW_DESC desc{};
            desc.Format = DXGI_FORMAT_D32_FLOAT;
            desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            desc.Texture2D.MipSlice = 0;

            m_device->CreateDepthStencilView(rendererModel.m_envCubemap.cubemapDepth.Get(), &desc, rendererModel.m_envCubemap.dsvHandle.cpuAddr);
        }

        // SRV
        {
            rendererModel.m_envCubemap.srvHandle = AllocSRVStatic(1u);

            D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
            desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            desc.TextureCube.MipLevels = rendererModel.m_envCubemap.MIP_COUNT;
            desc.TextureCube.MostDetailedMip = 0;

            m_device->CreateShaderResourceView(
                rendererModel.m_envCubemap.cubemapTexture.Get(),
                &desc,
                rendererModel.m_envCubemap.srvHandle.cpuAddr
            );
        }

        // UAVs
        {
            constexpr UINT uavCount = rendererModel.m_envCubemap.MIP_COUNT - 1u; // Except 0
            rendererModel.m_envCubemap.uavHandle = AllocSRVStatic(uavCount);

            for (UINT mip = 1u; mip < rendererModel.m_envCubemap.MIP_COUNT; mip++)
            {
                D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
                uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
                uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                uavDesc.Texture2DArray.MipSlice = mip;
                uavDesc.Texture2DArray.FirstArraySlice = 0;
                uavDesc.Texture2DArray.ArraySize = rendererModel.m_envCubemap.NUM_FACES;

                m_device->CreateUnorderedAccessView(
                    rendererModel.m_envCubemap.cubemapTexture.Get(),
                    nullptr,
                    &uavDesc,
                    OffsetSRV(rendererModel.m_envCubemap.uavHandle, mip - 1u).cpuAddr
                );
            }
        }

        {
            D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                rendererModel.m_envCubemap.cubemapTexture.Get(),
                D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
            );
            cmdList.ResourceBarrier(1, &barrier);
        }

        rendererModel.m_envCubemap.isOnGPU = false;
        rendererModel.m_envCubemap.isDirty = true;
        rendererModel.m_envCubemap.generation = 0u;

        rendererModel.isDirty = true;
    }

    return outKey;
}
void Renderer::UnloadModel(RegisterModelKey key)
{
    auto models = m_blackboard.GetOpt<std::vector<NSRenderer::Model>>(NSRenderer::kRenderer_models);
    assert(models.has_value());

    if (models->get().at(key.index).sceneKey.id == key.id)
    {
        NSRenderer::Model& model = models->get()[key.index];

        model.m_envCubemap.cubemapDepth.Reset();
        model.m_envCubemap.cubemapTexture.Reset();
        model.m_envCubemap.isDirty = false;
        model.m_envCubemap.isOnGPU = false;
        model.m_envCubemap.generation = 0;

        FreeSRVStatic(model.m_envCubemap.srvHandle);
        FreeSRVStatic(model.m_envCubemap.uavHandle);
        FreeRTVStatic(model.m_envCubemap.rtvHandle);
        FreeDSVStatic(model.m_envCubemap.dsvHandle);
    }
    else throw std::out_of_range("Unable to find the model requested to unload");
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

    D3D12_CPU_DESCRIPTOR_HANDLE rtCpuHandle = OffsetRTV(m_rtHandle, m_swapchain->GetCurrentBackBufferIndex()).cpuAddr;
    m_commandList->OMSetRenderTargets(1, &rtCpuHandle, FALSE, &m_dsHandle.cpuAddr);

    m_blackboard.Set<D3D12_CPU_DESCRIPTOR_HANDLE>(NSRenderer::kRenderer_mainRTV, rtCpuHandle);
    m_blackboard.Set<D3D12_CPU_DESCRIPTOR_HANDLE>(NSRenderer::kRenderer_mainDSV, m_dsHandle.cpuAddr);

    m_commandList->ClearRenderTargetView(OffsetRTV(m_rtHandle, m_swapchain->GetCurrentBackBufferIndex()).cpuAddr, CLEAR_COLOR, 0, nullptr);
    m_commandList->ClearDepthStencilView(m_dsHandle.cpuAddr, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

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

    m_dsHandle = AllocDSVStatic(1u);

    D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, dsvDesc.width, dsvDesc.height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    D3D12_CLEAR_VALUE clearVal = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.f, 0);
    ThrowIfFailed(m_device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearVal, IID_PPV_ARGS(&dsvDesc.outDSV)));
    m_device->CreateDepthStencilView(dsvDesc.outDSV.Get(), &desc, m_dsHandle.cpuAddr);
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

    ID3D12DescriptorHeap* heaps[] = {
        const_cast<ID3D12DescriptorHeap*>(m_srvHeap.GetHeap())
    };
    m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    Record(GetCtx(), NSRenderer::GraphicsCommandList(m_commandList.Get()));

    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList *const lists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(lists), lists);

    WaitForGPU();
}
