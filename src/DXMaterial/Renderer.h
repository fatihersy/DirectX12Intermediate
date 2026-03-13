#pragma once

#include "IApp.h"

#include "Descriptor.h"
#include "Allocator.h"
#include "Pipeline.h"
#include "Material.h"

class Scene;

using FnRendererExecutionBody = std::function<void(NSRenderer::Ctx ctx, NSRenderer::GraphicsCommandList cmdList)>;

class Renderer
{
public:
    Renderer(){};
    ~Renderer();

    void Init(IDXGIFactory7* factory, ID3D12Device14* device, HWND hwnd, UINT width, UINT height);

    void BeginFrame();
    void EndFrame();
    void DrawScene(Scene& scene);

    void Resize(UINT width, UINT height);
    void WaitForGPU();

    void OnDestroy();

    void Execute(FnRendererExecutionBody Record);

    template<typename T, typename... Args> requires std::derived_from<T, RenderPass::IRenderPass>
    inline RenderPass::IRenderPass& AddPass(Args&&... args) {
        return *m_passes.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
    }

    inline Descriptor::Handle AllocSRVRing(uint32_t amount = 1u) {
        return m_srvHeap.AllocateRing(amount);
    };
    inline Descriptor::Handle AllocSRVStatic(uint32_t amount = 1u) {
        return m_srvHeap.AllocateStatic(amount);
    };
    inline void FreeSRVStatic(Descriptor::Handle handle) {
        m_srvHeap.FreeStatic(handle);
    };
    inline Descriptor::hOffset OffsetSRV(const Descriptor::Handle& handle, uint32_t offset) const {
        return m_srvHeap.Offset(handle, offset);
    }

    inline NSRenderer::Ctx GetCtx() {
        return NSRenderer::Ctx(
            m_fallbackTextureSRVHandle,
            [this](uint32_t amount) { return this->AllocSRVRing(amount); },
            [this](uint32_t amount) { return this->AllocSRVStatic(amount); },
            [this](Descriptor::Handle handle) { this->FreeSRVStatic(handle); },
            [this](const Descriptor::Handle& handle, uint32_t offset) { return this->OffsetSRV(handle, offset); },
            [this](size_t size) { return this->m_allocator.Allocate(size); }
        );
    }

    inline ID3D12CommandQueue* ImGui_getCmdQueue() {
        return m_commandQueue.Get();
    }

private:
    ID3D12Device14* m_device = nullptr;
    IDXGIFactory7* m_factory = nullptr;

    ComPtr<IDXGISwapChain4> m_swapchain;
    ComPtr<ID3D12Resource2> m_renderTarget[IApp::ic_frameCount];
    ComPtr<ID3D12Resource2> m_depthStencil;

    ComPtr<ID3D12CommandAllocator> m_commandAllocators[IApp::ic_frameCount];
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12GraphicsCommandList10> m_commandList;

    ComPtr<ID3D12Fence1> m_fence;
    HANDLE m_fenceEvent = nullptr;
    UINT64 m_fenceGeneration{};

    ConstBuffAlloc m_allocator;

    Descriptor::StaticHeap m_rtvHeap;
    Descriptor::StaticHeap m_dsvHeap;
    Descriptor::RingHeap m_srvHeap;

    FTexture m_fallbackTexture{};
    Descriptor::Handle m_fallbackTextureSRVHandle;

    std::vector<std::unique_ptr<RenderPass::IRenderPass>> m_passes;

    void CreateSwapChain(IDXGIFactory7* factory, HWND hwnd, UINT width, UINT height);
    void CreateDepthStencil(LPCWSTR name, NSRenderer::DepthStencilCreateDescription desc);
    void CreateDefaultTexture();

    void MoveToNextFrame();

    static constexpr uint32_t FALLBACK_TEXTURE_SRV_INDEX = 0;
    static constexpr float CLEAR_COLOR[] = { 0.176f, 0.203f, 0.211f, 1.f };
};

