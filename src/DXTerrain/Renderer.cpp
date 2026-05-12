#include "stdafx.h"
#include "Renderer.h"

#include "imgui.h"
#include "imgui_impl_dx12.h"

#include "DXSampleHelper.h"
#include "RenderPass.h"

class BarrierBatch : public NSBarrier::IBarrierBatch
{
public:
    void Add(NSBarrier::BarrierKey key, CD3DX12_RESOURCE_BARRIER barrier) override
    {
        std::vector<D3D12_RESOURCE_BARRIER>& queue = m_entries[key.name.data()];

        queue.push_back(barrier);
    };
    void Add(NSBarrier::BarrierKey key, std::vector<CD3DX12_RESOURCE_BARRIER>& inBarriers) override
    {
        std::vector<D3D12_RESOURCE_BARRIER>& queue = m_entries[key.name.data()];

        queue.insert(queue.cend(), inBarriers.begin(), inBarriers.end());
    };
    bool Remove(NSBarrier::BarrierKey key, CD3DX12_RESOURCE_BARRIER barrier) override
    {
        if (m_entries.empty() or not m_entries.contains(key.name.data())) return false;

        std::vector<D3D12_RESOURCE_BARRIER>& batch = m_entries[key.name.data()];

        const auto itr = std::find_if(batch.begin(), batch.end(), [&barrier](D3D12_RESOURCE_BARRIER& itrBarrier)
        {
            return NSBarrier::operator==(barrier, itrBarrier);
        });

        if (itr != batch.end())
        {
            batch.erase(itr);
            return true;
        }

        return false;
    };
    void Flush() override
    {
        for (auto& entry : m_entries)
        {
            const auto itr = std::erase_if(entry.second, [](D3D12_RESOURCE_BARRIER& itrBarrier)
            {
                if (itrBarrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION) {
                    return not itrBarrier.Transition.pResource;
                }
                if (itrBarrier.Type == D3D12_RESOURCE_BARRIER_TYPE_ALIASING) {
                    return not itrBarrier.Aliasing.pResourceBefore;
                }
                if (itrBarrier.Type == D3D12_RESOURCE_BARRIER_TYPE_UAV) {
                    return not itrBarrier.UAV.pResource;
                }
                return true;
            });
        }
    };
    void Clear(NSBarrier::BarrierKey key) override
    {
        if (m_entries.empty() or not m_entries.contains(key.name.data())) return;

        m_entries[key.name.data()].clear();
    };
    bool Execute(NSBarrier::BarrierKey key, NSRenderer::GraphicsCommandList cmdList) override
    {
        if (m_entries.empty() or not m_entries.contains(key.name.data())) return false;

        std::vector<D3D12_RESOURCE_BARRIER>& queue = m_entries[key.name.data()];

        cmdList.ResourceBarrier(static_cast<UINT>(queue.size()), queue.data());

        return true;
    }

private:
    std::map<std::string, std::vector<D3D12_RESOURCE_BARRIER>> m_entries;
};
void Renderer::Init(IDXGIFactory7* factory, ID3D12Device14* device, HWND wnd, UINT width, UINT height)
{
    m_factory = factory;
    m_device = device;
    m_width = width;
    m_height = height;

    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&m_infoQueue1)))) return;

    m_infoQueue1->RegisterMessageCallback(
        [](D3D12_MESSAGE_CATEGORY, D3D12_MESSAGE_SEVERITY severity,
           D3D12_MESSAGE_ID, LPCSTR description, void*)
        {
            FlogLevel level = FlogLevel::FLOG_DEBUG;
            switch (severity) {
            case D3D12_MESSAGE_SEVERITY_CORRUPTION: level = FlogLevel::FLOG_FATAL; break;
            case D3D12_MESSAGE_SEVERITY_ERROR:      level = FlogLevel::FLOG_ERROR; break;
            case D3D12_MESSAGE_SEVERITY_WARNING:    level = FlogLevel::FLOG_WARN;  break;
            case D3D12_MESSAGE_SEVERITY_INFO:       level = FlogLevel::FLOG_INFO;  break;
            case D3D12_MESSAGE_SEVERITY_MESSAGE:    level = FlogLevel::FLOG_DEBUG; break;
            }
            if (g_PlatformConsoleWrite) g_PlatformConsoleWrite(level, description);
        },
        D3D12_MESSAGE_CALLBACK_FLAG_NONE,
        nullptr,
        &m_infoQueueCookie
    );

    m_shaderCompiler = std::make_unique<ShaderCompiler>();

    m_barrierBatch = std::make_unique<BarrierBatch>();

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
            m_renderTargets[frame]->SetName(NSTool::wformat(L"Renderer::m_renderTargets[%u]", frame).c_str());

            ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[frame])));
            m_commandAllocators[frame]->SetName(NSTool::wformat(L"%s[%u]", L"Renderer::m_commandAllocators", frame).c_str());
        }
    }

    CreateDepthStencil(L"Renderer::m_depthStencil", NSRenderer::DepthStencilCreateDescription {
        .format = DXGI_FORMAT_D32_FLOAT,
        .flags = D3D12_DSV_FLAG_NONE,
        .dimension = D3D12_DSV_DIMENSION::D3D12_DSV_DIMENSION_TEXTURE2D,
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

    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        //ImGuiIO& io = ImGui::GetIO();
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui_ImplDX12_InitInfo m_imGuiInitInfo;
        m_imGuiInitInfo.UserData = this;
        m_imGuiInitInfo.Device = m_device;
        m_imGuiInitInfo.CommandQueue = m_commandQueue.Get();
        m_imGuiInitInfo.NumFramesInFlight = IApp::ic_framesInFlight;
        m_imGuiInitInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        m_imGuiInitInfo.DSVFormat = DXGI_FORMAT_D32_FLOAT;

        {
            D3D12_DESCRIPTOR_HEAP_DESC desc{};
            desc.NumDescriptors = 100u;
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            ThrowIfFailed(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_imGuiSrvHeap)));
            m_imGuiSrvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            m_imGuiSrvHeap->SetName(L"app::im_imGuiSrvHeap");

            for (UINT i{}; i < desc.NumDescriptors; i++) m_freeImGuiSRVIndices.push_back(i);
        }

        m_imGuiInitInfo.SrvDescriptorHeap = m_imGuiSrvHeap.Get();
        m_imGuiInitInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo * info, D3D12_CPU_DESCRIPTOR_HANDLE * out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE * out_gpu_desc_handle)
        {
            Renderer* userData = static_cast<Renderer*>(info->UserData);
            if (userData->m_freeImGuiSRVIndices.empty())
            {
                g_FError("No free SRV descriptors available\n");
                *out_cpu_desc_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
                *out_gpu_desc_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
                return;
            }

            INT idx = userData->m_freeImGuiSRVIndices.back();
            userData->m_freeImGuiSRVIndices.pop_back();

            CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(info->SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
            *out_cpu_desc_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuHandle, idx, userData->m_imGuiSrvDescriptorSize);

            CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(info->SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
            *out_gpu_desc_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuHandle, idx, userData->m_imGuiSrvDescriptorSize);
        };
        m_imGuiInitInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo * info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_desc_handle)
        {
            Renderer* userData = static_cast<Renderer*>(info->UserData);

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

            userData->m_freeImGuiSRVIndices.push_back(idx);
        };
        ImGui_ImplDX12_Init(&m_imGuiInitInfo);

        m_debugUtils.ImGuiSrvDescriptorAllocFn = [this](D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle)
        {
            if (m_freeImGuiSRVIndices.empty())
            {
                g_FError("No free SRV descriptors available\n");
                *out_cpu_desc_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
                *out_gpu_desc_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
                return;
            }

            INT idx = m_freeImGuiSRVIndices.back();
            m_freeImGuiSRVIndices.pop_back();

            CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(m_imGuiSrvHeap->GetCPUDescriptorHandleForHeapStart());
            *out_cpu_desc_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuHandle, idx, m_imGuiSrvDescriptorSize);

            CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(m_imGuiSrvHeap->GetGPUDescriptorHandleForHeapStart());
            *out_gpu_desc_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuHandle, idx, m_imGuiSrvDescriptorSize);
        };
        m_debugUtils.ImGuiSrvDescriptorFreeFn = [this](D3D12_CPU_DESCRIPTOR_HANDLE cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_desc_handle)
        {
            CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(m_imGuiSrvHeap->GetCPUDescriptorHandleForHeapStart());
            ptrdiff_t offset = cpu_desc_handle.ptr - cpuHandle.ptr;
            if (offset % m_imGuiSrvDescriptorSize != 0 || offset < 0)
            {
                g_FError("Invalid SRV descriptor handle to free!\n");
                return;
            }

            INT idx = static_cast<INT>(offset / m_imGuiSrvDescriptorSize);
            if (idx >= static_cast<INT>(m_imGuiSrvHeap->GetDesc().NumDescriptors))
            {
                g_FError("SRV descriptor index out of bounds!\n");
                return;
            }

            m_freeImGuiSRVIndices.push_back(idx);
        };
    }

    CreateFallbackTexture();

    {
        NSTerrain::Terrain terrain(device);
        m_terrain = std::move(terrain);
    }

    m_blackboard.Set<UINT&>(NSRenderer::kRenderer_width, width);
    m_blackboard.Set<UINT&>(NSRenderer::kRenderer_height, height);
    m_blackboard.Set<std::reference_wrapper<const NSTerrain::ITerrainView>>(NSRenderer::kRenderer_terrain, std::cref(m_terrain));

    {
        std::vector<NSRenderer::Model> models;
        m_blackboard.Set<std::vector<NSRenderer::Model>>(NSRenderer::kRenderer_models, std::move(models));
    }

    Execute([this, &device](NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
    {
        AddPass<NSRenderPass::AtmospherePass>(device, m_blackboard, GetCtx())
            .OnInit(m_blackboard, GetCtx(), NSRenderer::GraphicsCommandList(cmdList))
            .SetIsEnabled(true);

        AddPass<NSRenderPass::EnvironmentCubemapPass>(device, m_blackboard, GetCtx())
            .OnInit(m_blackboard, GetCtx(), NSRenderer::GraphicsCommandList(cmdList))
            .SetIsEnabled(true);

        AddPass<NSRenderPass::GeometryPass>(device, m_blackboard, GetCtx())
            .OnInit(m_blackboard, GetCtx(), NSRenderer::GraphicsCommandList(cmdList))
            .SetIsEnabled(true);

        AddPass<NSRenderPass::TerrainPass>(device, m_blackboard, GetCtx())
            .OnInit(m_blackboard, GetCtx(), NSRenderer::GraphicsCommandList(cmdList))
            .SetIsEnabled(true);

        AddPass<NSRenderPass::DebugPass>(device, m_blackboard, GetCtx())
            .OnInit(m_blackboard, GetCtx(), NSRenderer::GraphicsCommandList(cmdList), m_debugUtils)
            .SetIsEnabled(true);
    });
}
Renderer::~Renderer()
{
}
void Renderer::OnDestroy()
{
    WaitForGPU();

    NSRenderer::Ctx rendererCtx = GetCtx();

    m_terrain.OnDestroy(rendererCtx);

    FreeSRVStatic(m_fallbackTextureSRVhandle);
    m_fallbackTexture.defaultBuffer.Reset();
    m_fallbackTexture.uploadBuffer.Reset();

    for (auto& pass : m_passes) pass->OnDestroy(rendererCtx);
    m_passes.clear();

    for (UINT i = 0; i < IApp::ic_framesInFlight; i++)
    {
        m_renderTargets[i].Reset();
    }
    m_depthStencil.Reset();

    m_swapChain.Reset();

    m_commandList.Reset();
    for (UINT i = 0; i < IApp::ic_framesInFlight; i++)
    {
        m_commandAllocators[i].Reset();
    }
    m_commandQueue.Reset();

    m_fence.Reset();
    if (m_fenceEvent)
    {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }

    m_shaderCompiler.reset();

    m_terrain = NSTerrain::Terrain{};

    m_srvHeap = NSDescriptor::RingHeap{};
    m_rtvHeap = NSDescriptor::StaticHeap{};
    m_dsvHeap = NSDescriptor::StaticHeap{};
    m_constantAllocator = ConstantAllocator{};
    m_barrierBatch.reset();

    m_infoQueue1.Reset();

    ImGui_ImplDX12_Shutdown();
    ImGui::DestroyContext();
    m_imGuiSrvHeap.Reset();
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
    ImGui_ImplDX12_NewFrame();
    ImGui::NewFrame();

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

    m_blackboard.Set<D3D12_CPU_DESCRIPTOR_HANDLE&>(NSRenderer::kRenderer_mainRTV, rtCpuHandle);
    m_blackboard.Set<D3D12_CPU_DESCRIPTOR_HANDLE&>(NSRenderer::kRenderer_mainDSV, m_dsHandle.cpuAddr);

    m_commandList->ClearRenderTargetView(OffsetRTV(m_rtHandle, frameIndex).cpuAddr, CLEAR_COLOR, 0, nullptr);
    m_commandList->ClearDepthStencilView(m_dsHandle.cpuAddr, D3D12_CLEAR_FLAG_DEPTH, 0.f, 0, 0, nullptr); // Clear Depth 1.f -> 0.f

    ID3D12DescriptorHeap* heaps[] = {
        const_cast<ID3D12DescriptorHeap*>(m_srvHeap.Raw())
    };
    m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    m_commandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}
void Renderer::DrawScene(NSScene::IScene& scene)
{
    m_blackboard.Set(NSRenderer::kRenderer_frameIndex, m_swapChain->GetCurrentBackBufferIndex());

    NSRenderer::Ctx rendererCtx = GetCtx();
    NSRenderer::GraphicsCommandList cmdList = NSRenderer::GraphicsCommandList(m_commandList.Get());

    for (auto& pass : m_passes)
    {
        pass->Execute(scene, m_blackboard, rendererCtx, cmdList);
    }

    DrawDebugImage(scene);
}
void Renderer::EndFrame()
{
    ImGui::Render();
    ID3D12DescriptorHeap* imGuiDescHeap[] = { m_imGuiSrvHeap.Get() };
    m_commandList->SetDescriptorHeaps(_countof(imGuiDescHeap), imGuiDescHeap);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());

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

NSRenderer::Model& Renderer::RegisterModel(std::wstring_view modelName, NSModel::SceneModelKey sceneKey, NSRenderer::GraphicsCommandList cmdList, NSModel::ERegModelFlag flag)
{
    std::vector<NSRenderer::Model>* models = m_blackboard.GetMut<std::vector<NSRenderer::Model>>(NSRenderer::kRenderer_models);
    assert(models);

    NSRenderer::Model& rendererModel = models->emplace_back();
    rendererModel.registerKey = { sceneKey.id, models->size() - 1u };
    rendererModel.sceneKey = sceneKey;
    rendererModel.SetFlag(flag);

    // Environment Cubemap
    {
        // Cubemap Texture
        {
            D3D12_RESOURCE_FLAGS flags = rendererModel.TestFlag(NSModel::ERegModelFlag::MODEL_FLAG_UNSEEN_TO_ENV_CAPTURE)
                ? D3D12_RESOURCE_FLAG_NONE
                : D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            desc.Width = rendererModel.m_envCubemap.PER_FACE_RESOLUTION;
            desc.Height = rendererModel.m_envCubemap.PER_FACE_RESOLUTION;
            desc.DepthOrArraySize = rendererModel.m_envCubemap.NUM_FACES;
            desc.MipLevels = rendererModel.m_envCubemap.MIP_COUNT;
            desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            desc.Flags = flags;

            CD3DX12_HEAP_PROPERTIES props(D3D12_HEAP_TYPE_DEFAULT);

            constexpr float clearColor[] = { 0.f, 0.f, 0.f, 1.f };
            D3D12_CLEAR_VALUE clearVal = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R16G16B16A16_FLOAT, clearColor);
            const D3D12_CLEAR_VALUE* pOptimizedClearValue = rendererModel.TestFlag(NSModel::ERegModelFlag::MODEL_FLAG_UNSEEN_TO_ENV_CAPTURE)
                ? nullptr
                : &clearVal;

            ThrowIfFailed(m_device->CreateCommittedResource(
                &props,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_COMMON,
                pOptimizedClearValue,
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
            desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

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
        if (not rendererModel.TestFlag(NSModel::ERegModelFlag::MODEL_FLAG_UNSEEN_TO_ENV_CAPTURE)) {
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
                rendererModel.m_envCubemap.cubemapTexture->SetName(NSTool::wformat(L"%s::m_envCubemap.cubemapTexture", modelName.data()).c_str());
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
                rendererModel.m_envCubemap.cubemapDepth.Get(),
                &desc,
                rendererModel.m_envCubemap.dsvHandle.cpuAddr
            );
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
        if (not rendererModel.TestFlag(NSModel::ERegModelFlag::MODEL_FLAG_UNSEEN_TO_ENV_CAPTURE)) {
            constexpr UINT uavCount = rendererModel.m_envCubemap.MIP_COUNT - 1u;
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

        rendererModel.m_envCubemap.isOnGPU = false;
        rendererModel.m_envCubemap.isDirty = true;
        rendererModel.m_envCubemap.generation = 0u;

        rendererModel.isDirty = true;
    }
    return rendererModel;
}
void Renderer::UnloadModel(NSModel::RegisterModelKey key)
{
    auto modelsOpt = m_blackboard.GetOpt<std::vector<NSRenderer::Model>>(NSRenderer::kRenderer_models);
    assert(modelsOpt.has_value() and "Blackboard key is has no value");

    auto& models = modelsOpt.value().get();

    assert(models.size() > key.index and models[key.index].sceneKey.id == key.id and "Renderer::UnloadModel::Invalid Register model key");

    NSRenderer::Model& model = models[key.index];

    if (m_srvHeap.Owns(model.m_envCubemap.srvHandle))
    {
        FreeSRVStatic(model.m_envCubemap.srvHandle);
    }
    if(m_srvHeap.Owns(model.m_envCubemap.uavHandle))
    {
        FreeSRVStatic(model.m_envCubemap.uavHandle);
    }
    if (m_rtvHeap.Owns(model.m_envCubemap.rtvHandle))
    {
        FreeRTVStatic(model.m_envCubemap.rtvHandle);
    }
    if (m_dsvHeap.Owns(model.m_envCubemap.dsvHandle))
    {
        FreeDSVStatic(model.m_envCubemap.dsvHandle);
    }

    model.m_envCubemap.cubemapDepth.Reset();
    model.m_envCubemap.cubemapTexture.Reset();
    model.m_envCubemap.isDirty = false;
    model.m_envCubemap.isOnGPU = false;
    model.m_envCubemap.generation = 0;
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
        D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
        D3D12_CLEAR_VALUE clearVal = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 0.f, 0); // Clear value 1.f -> 0.f
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

    for (auto& pass : m_passes)
    {
        pass->OnResize(m_width, m_height, GetCtx());
    }
}
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
    desc.ViewDimension = inDesc.dimension;

    m_dsHandle = AllocDSVStatic(1u);

    D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_CLEAR_VALUE clearVal = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 0.f, 0); // Clear value 1.f -> 0.f
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

void Renderer::DrawDebugImage(NSScene::IScene& scene)
{
    static bool terrainCullingDebug = true;

    if (!ImGui::Begin("Terrain Culling", &terrainCullingDebug))
    {
        ImGui::End();
        return;
    }

    if (m_debugUtils.isActive and m_debugUtils.debugPassImageSrv.ptr)
    {
        ImGui::Text("Debug Pass");
        ImGui::Image(
            static_cast<ImTextureID>(m_debugUtils.debugPassImageSrv.ptr),
            ImVec2(
                static_cast<float>(m_debugUtils.debugPassImageWidth),
                static_cast<float>(m_debugUtils.debugPassImageHeight)
            )
        );
    }

    ImGui::End();
}
