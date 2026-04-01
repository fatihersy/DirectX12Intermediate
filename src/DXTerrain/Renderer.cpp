#include "stdafx.h"
#include "Renderer.h"

#include "DXSampleHelper.h"

void Renderer::Init(IDXGIFactory7* factory, ID3D12Device14* device, HWND wnd, UINT width, UINT height)
{
    m_factory = factory;
    m_device = device;
    m_width = width;
    m_height = height;

    m_shaderCompiler = std::make_unique<ShaderCompiler>();

    constexpr size_t oneMb = 1u * 1024u * 1024u;
    m_constantAllocator = ConstantAllocator(device, oneMb, IApp::ic_framesInFlight);

    // Command Queue
    {
        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        ThrowIfFailed(m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_commandQueue)));
        m_commandQueue->SetName(L"Renderer::m_commandQueue");
    }

    CreateSwapChain(wnd, m_width, m_height);

    m_rtvHeap = NSDescriptor::StaticHeap(device, L"", D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1024u, false);
    m_dsvHeap = NSDescriptor::StaticHeap(device, L"", D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1024u, false);
    m_srvHeap = NSDescriptor::RingHeap(
        device,
        L"",
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        1024u,
        IApp::ic_framesInFlight,
        1024u
    );

    // Frame resources
    {
        m_rtHandle = m_rtvHeap.Allocate(IApp::ic_framesInFlight);
        for (UINT frame = 0; frame < IApp::ic_framesInFlight; frame++)
        {
            ThrowIfFailed(m_swapChain->GetBuffer(frame, IID_PPV_ARGS(&m_renderTargets[frame])));
            m_device->CreateRenderTargetView(m_renderTargets[frame].Get(), nullptr, m_rtvHeap.OffsetOf(m_rtHandle, frame).cpuAddr);

            ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[frame])));
            m_commandAllocators[frame]->SetName(NSTool::wformat(L"%s[%u]", L"Renderer::m_commandAllocators", frame).c_str());
        }
    }

    CreateDepthStencil(L"Renderer::m_depthStencil", NSRenderer::DepthStencilCreateDescription {
        .format = DXGI_FORMAT_D32_FLOAT,
        .flags = D3D12_DSV_FLAG_NONE,
        .dimention = D3D12_DSV_DIMENSION_TEXTURE2D,
        .width = m_width,
        .height = m_height,
        .outDSV = m_depthStencil
    });

    // Command List
    {
        ThrowIfFailed(m_device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&m_commandList)));
        m_commandList->SetName(L"Renderer::m_commandList");
    }

    // Create synchronization objects
    {
        ThrowIfFailed(m_device->CreateFence(m_fenceGeneration, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fence->SetName(L"Renderer::m_fence");
        m_fenceGeneration++;

        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (not m_fenceEvent)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }
    }

    CreateFallbackTexture();

    m_blackboard.Set<UINT&>(NSRenderer::kRenderer_width, width);
    m_blackboard.Set<UINT&>(NSRenderer::kRenderer_height, height);

    {
        std::vector<NSRenderer::Model> models;
        m_blackboard.Set<std::vector<NSRenderer::Model>>(NSRenderer::kRenderer_models, std::move(models));
    }
}

Renderer::~Renderer(){}
void Renderer::OnDestroy(){}

void Renderer::CreateSwapChain(HWND hwnd, UINT width, UINT height)
{
    assert(not m_swapChain and "CreateSwapChain() Supposed to use once");

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.BufferCount = IApp::ic_framesInFlight;
    desc.Width = width;
    desc.Height = height;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(m_factory->CreateSwapChainForHwnd(m_commandQueue.Get(), hwnd, &desc, nullptr, nullptr, &swapChain));
    ThrowIfFailed(swapChain.As(&m_swapChain));
}
void Renderer::CreateDepthStencil(LPCWSTR name, NSRenderer::DepthStencilCreateDescription inDesc)
{
    D3D12_DEPTH_STENCIL_VIEW_DESC desc{};
    desc.Format = inDesc.format;
    desc.Flags = inDesc.flags;
    desc.ViewDimension = inDesc.dimention;

    m_dsHandle = AllocDSVStatic(1u);

    D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_CLEAR_VALUE clearVal = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.f, 0);
    D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_D32_FLOAT,
        inDesc.width, inDesc.height,
        1, 0, 1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE
    );

    ThrowIfFailed(m_device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearVal, IID_PPV_ARGS(&m_depthStencil)));
    m_device->CreateDepthStencilView(inDesc.outDSV.Get(), &desc, m_dsHandle.cpuAddr);
    inDesc.outDSV->SetName(name);
}
void Renderer::CreateFallbackTexture()
{
    NSTexture::Texture& fbTex = m_fallbackTexture;
    NSDescriptor::Handle& fbHandle = m_fallbackTextureSRVhandle;
    ID3D12Device14* device = m_device;

    Execute([&fbTex, &fbHandle, &device](NSRenderer::Ctx ctx, NSRenderer::GraphicsCommandList cmdList)
    {
        fbTex.textureType = NSTexture::EType::EType_DIFFUSE;
        fbTex.width = 64;
        fbTex.height = 64;
        fbTex.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        const UINT squareSize = fbTex.width / 8u;

        D3D12_RESOURCE_DESC dstDesc{};
        dstDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        dstDesc.Width = fbTex.width;
        dstDesc.Height = fbTex.height;
        dstDesc.DepthOrArraySize = 1;
        dstDesc.MipLevels = 1;
        dstDesc.Format = fbTex.format;
        dstDesc.SampleDesc.Count = 1;
        dstDesc.SampleDesc.Quality = 0;
        dstDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        dstDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        CD3DX12_HEAP_PROPERTIES defaultHeapProp(D3D12_HEAP_TYPE_DEFAULT);
        ThrowIfFailed(device->CreateCommittedResource(
            &defaultHeapProp,
            D3D12_HEAP_FLAG_NONE,
            &dstDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&fbTex.defaultBuffer)
        ));
        fbTex.defaultBuffer->SetName(L"Renderer::m_fallbackTexture.defaultBuffer");

        {
            CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                fbTex.defaultBuffer.Get(),
                D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_STATE_COPY_DEST
            );
            cmdList.ResourceBarrier(1, &barrier);
        }

        fbTex.RowPitch = fbTex.width * 4;
        const UINT dataSize = fbTex.RowPitch * fbTex.height;

        CD3DX12_HEAP_PROPERTIES uploadHeapProp(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC srcDesc = CD3DX12_RESOURCE_DESC::Buffer(dataSize);
        ThrowIfFailed(device->CreateCommittedResource(
            &uploadHeapProp,
            D3D12_HEAP_FLAG_NONE,
            &srcDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&fbTex.uploadBuffer)
        ));
        fbTex.uploadBuffer->SetName(L"Renderer::m_fallbackTexture.uploadBuffer");

        uint8_t* mappedData = nullptr;
        ThrowIfFailed(fbTex.uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData)));

        for (UINT y = 0; y < fbTex.height; y++)
        {
            uint32_t* row = reinterpret_cast<uint32_t*>(mappedData + y * fbTex.RowPitch);

            for (UINT x = 0; x < fbTex.width; x++)
            {
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

        {
            CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                fbTex.defaultBuffer.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
            );
            cmdList.ResourceBarrier(1, &barrier);
        }

        fbHandle = ctx.allocSRVStatic(1u);
        fbTex.srvOffset = ctx.offsetSRV(fbHandle, 0u);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Format = fbTex.format;
        device->CreateShaderResourceView(fbTex.defaultBuffer.Get(), &srvDesc, fbHandle.cpuAddr);
    });
}

void Renderer::Execute(FnRendererExecutionBody Record)
{
    MoveToNextFrame();

    UINT frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    ThrowIfFailed(m_commandAllocators[frameIndex]->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[frameIndex].Get(), nullptr));

    ID3D12DescriptorHeap* heaps[] = {
        const_cast<ID3D12DescriptorHeap*>(m_srvHeap.Raw())
    };
    m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    Record(GetCtx(), NSRenderer::GraphicsCommandList(m_commandList.Get()));

    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* const lists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(lists), lists);

    WaitForGPU();
}

void Renderer::MoveToNextFrame()
{
    UINT64 fenceGen = m_fenceGeneration;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fenceGen));
    m_fenceGeneration++;

    if (m_fence->GetCompletedValue() < fenceGen)
    {
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

void Renderer::BeginFrame()
{

    UINT frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    ThrowIfFailed(m_commandAllocators[frameIndex]->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[frameIndex].Get(), nullptr));

    {
        CD3DX12_RESOURCE_BARRIER barriers[] =
        {
            CD3DX12_RESOURCE_BARRIER::Transition(
                m_renderTargets[frameIndex].Get(),
                D3D12_RESOURCE_STATE_PRESENT,
                D3D12_RESOURCE_STATE_RENDER_TARGET
            )
        };
        m_commandList->ResourceBarrier(_countof(barriers), barriers);
    }

    m_srvHeap.BeginFrame(frameIndex);
    m_constantAllocator.BeginFrame(frameIndex);

    D3D12_CPU_DESCRIPTOR_HANDLE rtCpuHandle = OffsetRTV(m_rtHandle, frameIndex).cpuAddr;
    m_commandList->OMSetRenderTargets(1, &rtCpuHandle, FALSE, &m_dsHandle.cpuAddr);

    m_commandList->ClearRenderTargetView(OffsetRTV(m_rtHandle, frameIndex).cpuAddr, CLEAR_COLOR, 0, nullptr);
    m_commandList->ClearDepthStencilView(m_dsHandle.cpuAddr, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

    ID3D12DescriptorHeap* heaps[] = {
        const_cast<ID3D12DescriptorHeap*>(m_srvHeap.Raw())
    };
    m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    m_commandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}
void Renderer::EndFrame()
{
    {
        CD3DX12_RESOURCE_BARRIER barriers[] =
        {
            CD3DX12_RESOURCE_BARRIER::Transition(
                m_renderTargets[m_swapChain->GetCurrentBackBufferIndex()].Get(),
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_PRESENT
            )
        };
        m_commandList->ResourceBarrier(_countof(barriers), barriers);
    }

    ThrowIfFailed(m_commandList->Close());

    ID3D12CommandList *const lists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(lists), lists);

    ThrowIfFailed(m_swapChain->Present(1, 0));

    MoveToNextFrame();
}

NSMesh::RegisterModelKey Renderer::RegisterModel(NSMesh::SceneModelKey sceneKey, NSRenderer::GraphicsCommandList cmdList)
{
    std::vector<NSRenderer::Model>* models = m_blackboard.GetMut<std::vector<NSRenderer::Model>>(NSRenderer::kRenderer_models);
    assert(models);

    NSMesh::RegisterModelKey outKey{ sceneKey.id, models->size() };
    NSRenderer::Model& rendererModel = models->emplace_back();
    rendererModel.sceneKey = sceneKey;

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
            for (UINT face = 0; face < rendererModel.m_envCubemap.NUM_FACES; face++)
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

            m_device->CreateDepthStencilView(
                rendererModel.m_envCubemap.cubemapTexture.Get(),
                &desc,
                rendererModel.m_envCubemap.srvHandle.cpuAddr
            );
        }

        // UAVs
        {
            constexpr UINT uavCount = rendererModel.m_envCubemap.MIP_COUNT - 1u;
            rendererModel.m_envCubemap.uavHandle = AllocSRVStatic(uavCount);

            for (UINT mip = 0; mip < rendererModel.m_envCubemap.MIP_COUNT; mip++)
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
void Renderer::UnloadModel(NSMesh::RegisterModelKey key)
{
    auto modelsOpt = m_blackboard.GetOpt<std::vector<NSRenderer::Model>>(NSRenderer::kRenderer_models);
    assert(modelsOpt.has_value() and "Blackboard key is has no value");

    auto models = modelsOpt.value().get();

    assert(models.size() > key.index and models[key.index].sceneKey.id == key.id and "Renderer::UnloadModel::Invalid Register model key");

    NSRenderer::Model& model = models[key.index];

    model.m_envCubemap.cubemapDepth.Reset();
    model.m_envCubemap.cubemapTexture.Reset();
    model.m_envCubemap.isDirty = false;
    model.m_envCubemap.isOnGPU = false;
    model.m_envCubemap.generation = 0;

    FreeSRVStatic(model.m_envCubemap.srvHandle);
    FreeSRVStatic(model.m_envCubemap.uavHandle);
    FreeRTVStatic(model.m_envCubemap.rtvHandle);
    FreeSRVStatic(model.m_envCubemap.dsvHandle);
}

void Renderer::Resize(UINT width, UINT height)
{
    WaitForGPU();

    for (UINT i = 0; i < IApp::ic_framesInFlight; i++)
    {
        m_renderTargets[i].Reset();
    }
    m_depthStencil.Reset();

    {
        DXGI_SWAP_CHAIN_DESC1 desc{};
        m_swapChain->GetDesc1(&desc);
        ThrowIfFailed(m_swapChain->ResizeBuffers(IApp::ic_framesInFlight, width, height, desc.Format, desc.Flags));
    }

    UINT frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    for (UINT i = 0; i < IApp::ic_framesInFlight; i++)
    {
        NSDescriptor::Offset rtvHandle = m_rtvHeap.At(i);

        ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle.cpuAddr);
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
        m_device->CreateDepthStencilView(m_depthStencil.Get(), &desc, m_dsHandle.cpuAddr);
    }

    m_blackboard.Set<UINT&>(NSRenderer::kRenderer_width, m_width);
    m_blackboard.Set<UINT&>(NSRenderer::kRenderer_height, m_height);

    {
        // Resize passes as well
    }
}
