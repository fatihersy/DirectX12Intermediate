#include "stdafx.h"
#include "Renderer.h"

#include "imgui_impl_dx12.h"

#include "DXSampleHelper.h"
#include "Logger.h"

class BarrierBatch : public NSBarrier::IBarrierBatch
{
public:
    void Add(NSBarrier::BarrierKey key, CD3DX12_RESOURCE_BARRIER barrier) override
    {
        std::vector<D3D12_RESOURCE_BARRIER>& queue = m_entries[key.name.data()];

        queue.push_back(barrier);
    }
    void Add(NSBarrier::BarrierKey key, std::vector<CD3DX12_RESOURCE_BARRIER>& barriers) override
    {
        std::vector<D3D12_RESOURCE_BARRIER>& queue = m_entries[key.name.data()];

        queue.insert(queue.cend(), barriers.begin(), barriers.end());
    }
    bool Remove(NSBarrier::BarrierKey key, CD3DX12_RESOURCE_BARRIER barrier) override
    {
        if (m_entries.empty() or not m_entries.contains(key.name.data())) return false;

        std::vector<D3D12_RESOURCE_BARRIER>& batch = m_entries[key.name.data()];

        const auto itr = std::find_if(batch.begin(), batch.end(), [&barrier](D3D12_RESOURCE_BARRIER& rhs)
        {
            return NSBarrier::operator==(barrier, rhs);
        });

        if (itr != batch.end())
        {
            batch.erase(itr);
            return true;
        }

        return false;
    }
    void Flush() override
    {
        for (auto entry : m_entries)
        {
            const auto itr = std::erase_if(entry.second, [](D3D12_RESOURCE_BARRIER& itrBarrier)
            {
                switch (itrBarrier.Type)
                {
                    case D3D12_RESOURCE_BARRIER_TYPE_TRANSITION: return not itrBarrier.Transition.pResource;
                    case D3D12_RESOURCE_BARRIER_TYPE_ALIASING:   return not itrBarrier.Aliasing.pResourceBefore;
                    case D3D12_RESOURCE_BARRIER_TYPE_UAV:        return not itrBarrier.UAV.pResource;

                    default: return false;
                }
            });
        }
    }
    void Clear(NSBarrier::BarrierKey key) override
    {
        if (m_entries.empty() or not m_entries.contains(key.name.data())) return;

        m_entries[key.name.data()].clear();
    }
    bool Execute(NSBarrier::BarrierKey key, NSDX12::GraphicsCommandList cmdList) override
    {
        if (m_entries.empty() or not m_entries.contains(key.name.data())) return false;

        std::vector<D3D12_RESOURCE_BARRIER>& queue = m_entries[key.name.data()];

        cmdList.ResourceBarrier(static_cast<uint32_t>(queue.size()), queue.data());

        return true;
    }

private:
    std::map<std::string, std::vector<D3D12_RESOURCE_BARRIER>> m_entries;
};

Renderer::Renderer(){}
Renderer::~Renderer(){}

void Renderer::OnInit(IDXGIFactory7* factory, ID3D12Device14* device, IWICImagingFactory2* wicFactory, NSRenderer::RendererDescription rendererDesc)
{
    m_factory = factory;
    m_device = device;
    m_wicFactory = wicFactory;
    m_desc = rendererDesc;

    ThrowIfFailed(device->QueryInterface(IID_PPV_ARGS(&m_infoQueue1)));

    m_infoQueue1->RegisterMessageCallback(
        [](D3D12_MESSAGE_CATEGORY, D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID, LPCSTR description, void*)
        {
            ELogLevel level = ELogLevel::EDEBUG;
            switch (severity)
            {
            case D3D12_MESSAGE_SEVERITY_CORRUPTION: level = ELogLevel::EFATAL; break;
            case D3D12_MESSAGE_SEVERITY_ERROR:      level = ELogLevel::EERROR; break;
            case D3D12_MESSAGE_SEVERITY_WARNING:    level = ELogLevel::EWARN;  break;
            case D3D12_MESSAGE_SEVERITY_INFO:       level = ELogLevel::EINFO;  break;
            case D3D12_MESSAGE_SEVERITY_MESSAGE:    level = ELogLevel::EDEBUG; break;
            }
            if (g_PlatformConsoleWrite) {
                g_PlatformConsoleWrite(level, description);
            }
        },
        D3D12_MESSAGE_CALLBACK_FLAG_NONE,
        nullptr,
        &m_infoQueueCookie
    );

    m_barrierBatch = std::make_unique<BarrierBatch>();

    // Command Queue
    {
        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Flags = D3D12_COMMAND_QUEUE_FLAGS::D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        ThrowIfFailed(m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_commandQueue)));
        m_commandQueue->SetName(L"Renderer::m_commandQueue");
    }

    {
        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.BufferCount = IApp::ic_framesInFlight;
        desc.Width = rendererDesc.width;
        desc.Height = rendererDesc.height;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.SwapEffect = DXGI_SWAP_EFFECT::DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.SampleDesc.Count = 1;

        ComPtr<IDXGISwapChain1> dummySc;
        ThrowIfFailed(
            factory->CreateSwapChainForHwnd(
                m_commandQueue.Get(),
                rendererDesc.wnd,
                &desc,
                nullptr, // We created the sc on a windowed window
                nullptr, // There is one monitor to render
                &dummySc
            )
        );

        dummySc.As(&m_swapChain);
        m_fenceGeneration = m_swapChain->GetCurrentBackBufferIndex();
    }

    // Create Descriptor heaps
    {
        D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{};
        rtvDesc.NumDescriptors = IApp::ic_framesInFlight;
        rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&m_rtvHeap)));

        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(rtvDesc.Type);

        D3D12_DESCRIPTOR_HEAP_DESC dsvDesc{};
        dsvDesc.NumDescriptors = IApp::ic_framesInFlight;
        dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(&m_dsvHeap)));

        m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(dsvDesc.Type);
    }

    // Frame resources
    {
        for (uint32_t frame{}; frame < IApp::ic_framesInFlight; frame++)
        {
            CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), frame, m_rtvDescriptorSize);

            // Create a RTV for each frame
            ThrowIfFailed(m_swapChain->GetBuffer(frame, IID_PPV_ARGS(&m_renderTargets[frame])));
            m_device->CreateRenderTargetView(m_renderTargets[frame].Get(), nullptr, rtvHandle);

            ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[frame])));
        }
    }

    // Create Command Lists
    {
        ThrowIfFailed(m_device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&m_commandList)));
        m_commandList->SetName(L"Renderer::m_commandList");
    }

    // Create syncronization objects
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

    // ImGui
    {
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
            m_imGuiSrvHeap->SetName(L"Renderer::m_imGuiSrvHeap");

            for (uint32_t idx{}; idx < desc.NumDescriptors; idx++) m_freeImGuiSRVIndices.push_back(idx);
        }

        m_imGuiInitInfo.SrvDescriptorHeap = m_imGuiSrvHeap.Get();
        m_imGuiInitInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo * info, D3D12_CPU_DESCRIPTOR_HANDLE * out_cpu_desc, D3D12_GPU_DESCRIPTOR_HANDLE * out_gpu_desc)
        {
            Renderer * userData = static_cast<Renderer*>(info->UserData);
            if (userData->m_freeImGuiSRVIndices.empty())
            {
                g_FError("No free SRV descriptor available");
                *out_cpu_desc = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
                *out_gpu_desc = CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
                return;
            }

            int32_t idx = userData->m_freeImGuiSRVIndices.back();
            userData->m_freeImGuiSRVIndices.pop_back();

            CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(info->SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
            *out_cpu_desc = CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuHandle, idx, userData->m_imGuiSrvDescriptorSize);

            CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(info->SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
            *out_gpu_desc = CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuHandle, idx, userData->m_imGuiSrvDescriptorSize);
        };
        m_imGuiInitInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo * info, D3D12_CPU_DESCRIPTOR_HANDLE cpuDesc, D3D12_GPU_DESCRIPTOR_HANDLE gpuDesc)
        {
            Renderer * userData = static_cast<Renderer*>(info->UserData);

            CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(info->SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
            ptrdiff_t offset = cpuDesc.ptr - cpuHandle.ptr;
            if (offset % userData->m_imGuiSrvDescriptorSize != 0 || offset < 0 )
            {
                g_FError("Invalid SRV handle");
                return;
            }

            int32_t idx = static_cast<uint32_t>(offset / userData->m_imGuiSrvDescriptorSize);
            if (idx >= static_cast<int32_t>(info->SrvDescriptorHeap->GetDesc().NumDescriptors))
            {
                g_FError("SRV index is out of bound");
                return;
            }

            userData->m_freeImGuiSRVIndices.push_back(idx);
        };

        ImGui_ImplDX12_Init(&m_imGuiInitInfo);

        m_debugUtils.ImGuiSrvDescriptorAllocFn = [this](D3D12_CPU_DESCRIPTOR_HANDLE * out_cpu_desc, D3D12_GPU_DESCRIPTOR_HANDLE * out_gpu_desc)
        {
            if (m_freeImGuiSRVIndices.empty())
            {
                g_FError("No free SRV descriptor available");
                *out_cpu_desc = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
                *out_gpu_desc = CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
                return;
            }

            int32_t idx = m_freeImGuiSRVIndices.back();
            m_freeImGuiSRVIndices.pop_back();

            CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(m_imGuiSrvHeap->GetCPUDescriptorHandleForHeapStart());
            *out_cpu_desc = CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuHandle, idx, m_imGuiSrvDescriptorSize);

            CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(m_imGuiSrvHeap->GetGPUDescriptorHandleForHeapStart());
            *out_gpu_desc = CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuHandle, idx, m_imGuiSrvDescriptorSize);
        };
        m_imGuiInitInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo * info, D3D12_CPU_DESCRIPTOR_HANDLE cpuDesc, D3D12_GPU_DESCRIPTOR_HANDLE gpuDesc)
        {
            Renderer * userData = static_cast<Renderer*>(info->UserData);

            CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(info->SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
            ptrdiff_t offset = cpuDesc.ptr - cpuHandle.ptr;
            if (offset % userData->m_imGuiSrvDescriptorSize != 0 || offset < 0 )
            {
                g_FError("Invalid SRV handle");
                return;
            }

            int32_t idx = static_cast<uint32_t>(offset / userData->m_imGuiSrvDescriptorSize);
            if (idx >= static_cast<int32_t>(info->SrvDescriptorHeap->GetDesc().NumDescriptors))
            {
                g_FError("SRV index is out of bound");
                return;
            }

            userData->m_freeImGuiSRVIndices.push_back(idx);
        };
    }
}
void Renderer::OnDestroy()
{
    WaitForGPU();

    NSRenderer::Ctx ctx = GetCtx();

    m_sceneColor.Reset();
    m_commandList.Reset();
    m_copyCommandList.Reset();
    m_commandQueue.Reset();
    m_fence.Reset();
    if (m_fenceEvent)
    {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }
    m_rtvHeap.Reset();
    m_dsvHeap.Reset();
    m_pipelineState.Reset();
    m_copyCommandList.Reset();

    for (uint32_t frame{}; frame < IApp::ic_framesInFlight; frame++)
    {
        m_depthStencil[frame].Reset();
        m_renderTargets[frame].Reset();
        m_commandAllocators[frame].Reset();
        m_copyCommandAllocators[frame].Reset();
    }

    m_swapChain.Reset();
    m_infoQueue1.Reset();

    ImGui_ImplDX12_Shutdown();
    m_imGuiSrvHeap.Reset();
}

void Renderer::Update()
{

}
void Renderer::BeginFrame()
{
    ImGui_ImplDX12_NewFrame();
    ImGui::NewFrame();

    uint32_t frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    ThrowIfFailed(m_commandAllocators[frameIndex]->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[frameIndex].Get(), nullptr));

    {
        CD3DX12_RESOURCE_BARRIER barriers[] =
        {
            CD3DX12_RESOURCE_BARRIER::Transition(
                m_renderTargets[frameIndex].Get(),
                D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_PRESENT,
                D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RENDER_TARGET
            )
        };
        m_commandList->ResourceBarrier(_countof(barriers), barriers);
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, m_rtvDescriptorSize);
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, 0);

    m_commandList->ClearRenderTargetView(rtvHandle, CLEAR_COLOR, 0, nullptr);

    uint32_t width = IApp::GetInstance()->im_width;
    uint32_t height = IApp::GetInstance()->im_height;
    CD3DX12_VIEWPORT viewport(0.f, 0.f, static_cast<FLOAT>(width), static_cast<FLOAT>(height));
    CD3DX12_RECT scissor(0L, 0L, static_cast<LONG>(width), static_cast<LONG>(height));

   m_commandList->RSSetViewports(1, &viewport);
   m_commandList->RSSetScissorRects(1, &scissor);
}
void Renderer::DrawScene(std::shared_ptr<NSScene::IScene> scene)
{

}
void Renderer::EndFrame()
{
    ImGui::Render();

    CD3DX12_CPU_DESCRIPTOR_HANDLE backRtv(
        m_rtvHeap->GetCPUDescriptorHandleForHeapStart(),
        m_swapChain->GetCurrentBackBufferIndex(),
        m_rtvDescriptorSize
    );
    m_commandList->OMSetRenderTargets(1, &backRtv, FALSE, nullptr);

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
    ThrowIfFailed(m_swapChain->Present(1, 0));

    MoveToNextFrame();
}

std::shared_ptr<NSRenderer::Model> Renderer::RegisterModel(std::wstring_view modelName, ObserverKey sceneKey, NSDX12::GraphicsCommandList cmdList, Flag<NSRenderer::ERegModelFlag> flag)
{
    ASSERT(false, "Reserved for later");

    return nullptr;
}
void Renderer::UnloadModel(ObserverKey key) {}

void Renderer::Resize(uint32_t width, uint32_t height)
{
    WaitForGPU();

    for (uint32_t frame{}; frame < IApp::ic_framesInFlight; frame++)
    {
        m_renderTargets[frame].Reset();
        m_depthStencil[frame].Reset();
    }
    m_sceneColor.Reset();

    {
        DXGI_SWAP_CHAIN_DESC1 desc{};
        m_swapChain->GetDesc1(&desc);
        ThrowIfFailed(m_swapChain->ResizeBuffers(IApp::ic_framesInFlight, width, height, desc.Format, desc.Flags));
    }

    uint32_t frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    for (uint32_t frame{}; frame < IApp::ic_framesInFlight; frame++)
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), frame);

        ThrowIfFailed(m_swapChain->GetBuffer(frame, IID_PPV_ARGS(&m_renderTargets[frame])));
        m_device->CreateRenderTargetView(m_renderTargets[frame].Get(), nullptr, cpuHandle);

        {
            D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

            CD3DX12_HEAP_PROPERTIES heapProp(D3D12_HEAP_TYPE_DEFAULT);
            CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT::DXGI_FORMAT_D32_FLOAT, width, height);
            CD3DX12_CLEAR_VALUE clearVal(DXGI_FORMAT::DXGI_FORMAT_D32_FLOAT, 0, 0);
            ThrowIfFailed(m_device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearVal, IID_PPV_ARGS(&m_depthStencil[frame])));

            D3D12_DEPTH_STENCIL_VIEW_DESC desc{};
            desc.Format = DXGI_FORMAT_D32_FLOAT;
            desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            desc.Flags = D3D12_DSV_FLAG_NONE;
            m_device->CreateDepthStencilView(m_depthStencil[frame].Get(), &desc, cpuHandle);
        }
    }
}
void Renderer::Execute(FnRendererExecutionBody Record)
{
    MoveToNextFrame();

    uint32_t frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    ThrowIfFailed(m_commandAllocators[frameIndex]->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[frameIndex].Get(), nullptr));

    Record(GetCtx(), NSDX12::GraphicsCommandList(m_commandList.Get()));

    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* const lists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(lists), lists);

    WaitForGPU();
}

void Renderer::DrawDebugImage(){}

void Renderer::CreateSwapChain(HWND hwnd, uint32_t width, uint32_t height)
{
    ASSERT(false, "Reserved for later");
}
void Renderer::CreateDepthStencil(std::wstring_view name, NSRenderer::DepthStencilCreateDescription desc)
{
    ASSERT(false, "Reserved for later");
}
void Renderer::CreateFallbackTexture()
{
    ASSERT(false, "Reserved for later");
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
