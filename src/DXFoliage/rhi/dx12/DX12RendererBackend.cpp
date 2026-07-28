#include "stdafx.h"
#include "DX12RendererBackend.h"

#include "DXSampleHelper.h"
#include "Logger.h"
#include "ShaderCompiler.h"

#include <dxgidebug.h>

#include "imgui_impl_dx12.h"

#include "DX12CommandList.h"
#include "DX12Device.h"
#include "DX12Fence.h"
#include "DX12Swapchain.h"
#include "DX12Texture.h"

#include <memory>
#include <vector>

namespace NSRHIDX12
{
    // The concrete backend — defined here (not in a header) so its DirectX
    // members never leak into any header. Constructed only via
    // CreateDX12Backend() at the bottom of this file.
    //
    // Owns the whole DX12 renderer bundle (device, swapchain, fence,
    // command allocators + the persistent command list wrapped by
    // DX12CommandList, the imgui-dx12 backend) and drives the frame
    // boundary — acquire/submit/present/fence and the backbuffer state
    // transitions are backend-private; the front-end only records draws
    // into the command list BeginFrame() hands back. Holds NO scene/
    // content (that's the front-end's, created through GetDevice()).
    class DX12RendererBackend final : public NSRHI::IRendererBackend
    {
    public:
        DX12RendererBackend() = default;
        ~DX12RendererBackend() override = default;

        bool Initialize(NSPlatform::IWindow& window, uint32_t width, uint32_t height) override;
        void Shutdown() override;
        void Resize(uint32_t width, uint32_t height) override;

        NSRHI::IDevice& GetDevice() override { return *m_device; }

        NSRHI::ICommandList& BeginFrame() override;
        void EndFrame() override;
        NSRHI::ITexture& CurrentBackBuffer() override;

    private:
        void MoveToNextFrame();
        void WaitForGPU();

        static constexpr uint32_t kFramesInFlight = 2;

        std::unique_ptr<DX12Device> m_device;
        std::unique_ptr<DX12Swapchain> m_swapchain;

        std::unique_ptr<DX12Fence> m_fence;
        uint64_t m_fenceGeneration{};
        uint32_t m_currentFrameIndex{};

        ComPtr<ID3D12CommandAllocator> m_commandAllocators[kFramesInFlight];
        ComPtr<ID3D12GraphicsCommandList10> m_commandList;
        DX12CommandList m_cmdList;

        std::unique_ptr<ShaderCompiler> m_shaderCompiler;

        ComPtr<ID3D12DescriptorHeap> m_imGuiSrvHeap;
        std::vector<uint32_t> m_freeImGuiSRVIndices;
        uint32_t m_imGuiSrvDescriptorSize{};
    };

    bool DX12RendererBackend::Initialize(NSPlatform::IWindow& window, uint32_t width, uint32_t height)
    {
        m_device = std::make_unique<DX12Device>();

        // ShaderCompiler self-registers as a singleton; DX12Pipeline
        // (created later through GetDevice().CreateGraphicsPipeline) uses
        // ShaderCompiler::GetInstance() to compile.
        m_shaderCompiler = std::make_unique<ShaderCompiler>();

        HWND hwnd = reinterpret_cast<HWND>(window.GetNativeHandle().b);
        m_swapchain = std::make_unique<DX12Swapchain>(m_device->Raw(), m_device->Factory(), m_device->Queue(), hwnd, width, height, kFramesInFlight);
        m_fenceGeneration = m_swapchain->AcquireNextImage();

        for (uint32_t frame{}; frame < kFramesInFlight; ++frame)
        {
            ThrowIfFailed(m_device->Raw()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[frame])));
        }

        ThrowIfFailed(m_device->Raw()->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&m_commandList)));
        m_commandList->SetName(L"DX12RendererBackend::m_commandList");
        // The wrapper points at the persistent command list; it's Reset
        // each frame (same object), so this stays valid across frames.
        m_cmdList = DX12CommandList(m_commandList.Get());

        m_fence = std::make_unique<DX12Fence>(m_device->Raw(), m_device->Queue(), m_fenceGeneration);
        m_fenceGeneration++;

        // imgui-dx12
        {
            ImGui_ImplDX12_InitInfo info{};
            info.UserData = this;
            info.Device = m_device->Raw();
            info.CommandQueue = m_device->Queue();
            info.NumFramesInFlight = kFramesInFlight;
            info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
            info.DSVFormat = DXGI_FORMAT_D32_FLOAT;

            D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
            heapDesc.NumDescriptors = 100u;
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            ThrowIfFailed(m_device->Raw()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_imGuiSrvHeap)));
            m_imGuiSrvDescriptorSize = m_device->Raw()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            m_imGuiSrvHeap->SetName(L"DX12RendererBackend::m_imGuiSrvHeap");
            for (uint32_t idx{}; idx < heapDesc.NumDescriptors; ++idx) m_freeImGuiSRVIndices.push_back(idx);

            info.SrvDescriptorHeap = m_imGuiSrvHeap.Get();
            info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* outCpu, D3D12_GPU_DESCRIPTOR_HANDLE* outGpu)
            {
                auto* self = static_cast<DX12RendererBackend*>(info->UserData);
                if (self->m_freeImGuiSRVIndices.empty())
                {
                    g_FError("No free SRV descriptor available");
                    *outCpu = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
                    *outGpu = CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
                    return;
                }

                uint32_t idx = self->m_freeImGuiSRVIndices.back();
                self->m_freeImGuiSRVIndices.pop_back();

                CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(info->SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
                *outCpu = CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuHandle, idx, self->m_imGuiSrvDescriptorSize);

                CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(info->SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
                *outGpu = CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuHandle, idx, self->m_imGuiSrvDescriptorSize);
            };
            info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpuDesc, D3D12_GPU_DESCRIPTOR_HANDLE)
            {
                auto* self = static_cast<DX12RendererBackend*>(info->UserData);

                CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(info->SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
                ptrdiff_t offset = cpuDesc.ptr - cpuHandle.ptr;
                if (offset % self->m_imGuiSrvDescriptorSize != 0 || offset < 0)
                {
                    g_FError("Invalid SRV handle");
                    return;
                }

                uint32_t idx = static_cast<uint32_t>(offset / self->m_imGuiSrvDescriptorSize);
                if (idx >= info->SrvDescriptorHeap->GetDesc().NumDescriptors)
                {
                    g_FError("SRV index is out of bound");
                    return;
                }

                self->m_freeImGuiSRVIndices.push_back(idx);
            };

            ImGui_ImplDX12_Init(&info);
        }

        return true;
    }

    void DX12RendererBackend::Shutdown()
    {
        WaitForGPU();

        ImGui_ImplDX12_Shutdown();
        m_imGuiSrvHeap.Reset();

        m_commandList.Reset();
        for (uint32_t frame{}; frame < kFramesInFlight; ++frame) m_commandAllocators[frame].Reset();

        m_fence.reset();
        m_swapchain.reset();
        m_shaderCompiler.reset();
        m_device.reset();

        // Moved here from app::OnDestroy: this reports leaked *DX12/DXGI*
        // objects, so it belongs with the backend that created them — and
        // it has to run after everything above is released to be
        // meaningful.
        ComPtr<IDXGIDebug1> dxgiDebug;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
        {
            dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
        }
    }

    void DX12RendererBackend::Resize(uint32_t width, uint32_t height)
    {
        WaitForGPU();
        m_swapchain->Resize(width, height);
    }

    NSRHI::ITexture& DX12RendererBackend::CurrentBackBuffer()
    {
        return *m_swapchain->GetBackBufferTexture(m_currentFrameIndex);
    }

    NSRHI::ICommandList& DX12RendererBackend::BeginFrame()
    {
        ImGui_ImplDX12_NewFrame();
        ImGui::NewFrame();

        m_currentFrameIndex = m_swapchain->AcquireNextImage();

        ThrowIfFailed(m_commandAllocators[m_currentFrameIndex]->Reset());
        ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_currentFrameIndex].Get(), nullptr));

        // Transition the backbuffer into the render-target state here (the
        // swapchain's present<->RT cycle is the backend's business); the
        // front-end then just cmd.BeginRendering() it. EndFrame transitions
        // it back to present.
        DX12Texture* backBuffer = m_swapchain->GetBackBufferTexture(m_currentFrameIndex);
        CD3DX12_RESOURCE_BARRIER toRT = CD3DX12_RESOURCE_BARRIER::Transition(
            backBuffer->Raw(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_commandList->ResourceBarrier(1, &toRT);

        return m_cmdList;
    }

    void DX12RendererBackend::EndFrame()
    {
        DX12Texture* backBuffer = m_swapchain->GetBackBufferTexture(m_currentFrameIndex);

        // imgui renders into the backbuffer on top of the front-end's draws.
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = backBuffer->View();
        m_commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

        ImGui::Render();
        ID3D12DescriptorHeap* heaps[] = { m_imGuiSrvHeap.Get() };
        m_commandList->SetDescriptorHeaps(1, heaps);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());

        CD3DX12_RESOURCE_BARRIER toPresent = CD3DX12_RESOURCE_BARRIER::Transition(
            backBuffer->Raw(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        m_commandList->ResourceBarrier(1, &toPresent);

        ThrowIfFailed(m_commandList->Close());

        ID3D12CommandList* lists[] = { m_commandList.Get() };
        m_device->Queue()->ExecuteCommandLists(1, lists);

        m_swapchain->Present();

        MoveToNextFrame();
    }

    void DX12RendererBackend::MoveToNextFrame()
    {
        uint64_t fenceGen = m_fenceGeneration;
        m_fence->Signal(fenceGen);
        m_fenceGeneration++;
        m_fence->Wait(fenceGen);
    }

    void DX12RendererBackend::WaitForGPU()
    {
        m_fence->Signal(m_fenceGeneration);
        m_fence->Wait(m_fenceGeneration);
        m_fenceGeneration++;
    }

    std::unique_ptr<NSRHI::IRendererBackend> CreateDX12Backend()
    {
        return std::make_unique<DX12RendererBackend>();
    }
}
