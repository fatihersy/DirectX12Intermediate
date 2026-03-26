#pragma once

#include "IApp.h"

#include "Descriptor.h"
#include "Allocator.h"
#include "Blackboard.h"
#include "ShaderCompiler.h"

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
    RegisterModelKey RegisterModel(SceneModelKey key, NSRenderer::GraphicsCommandList cmdList);
    void UnloadModel(RegisterModelKey key);

    void Resize(UINT width, UINT height);
    void WaitForGPU();

    void OnDestroy();

    void Execute(FnRendererExecutionBody Record);

    template<typename T, typename... Args> requires std::derived_from<T, RenderPass::IRenderPass>
    RenderPass::IRenderPass& AddPass(Args&&... args) {
        return *m_passes.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
    }

    Descriptor::Handle AllocSRVRing(uint32_t amount = 1u) {
        return m_srvHeap.AllocateRing(amount);
    };
    Descriptor::Handle AllocSRVStatic(uint32_t amount = 1u) {
        return m_srvHeap.AllocateStatic(amount);
    };
    Descriptor::Handle AllocRTVStatic(uint32_t amount = 1u) {
        return m_rtvHeap.Allocate(amount);
    };
    Descriptor::Handle AllocDSVStatic(uint32_t amount = 1u) {
        return m_dsvHeap.Allocate(amount);
    };
    void FreeSRVStatic(Descriptor::Handle handle) {
        m_srvHeap.FreeStatic(handle);
    };
    void FreeRTVStatic(Descriptor::Handle handle) {
        m_rtvHeap.Free(handle);
    };
    void FreeDSVStatic(Descriptor::Handle handle) {
        m_dsvHeap.Free(handle);
    };
    Descriptor::hOffset OffsetSRV(const Descriptor::Handle& handle, uint32_t offset) const {
        return m_srvHeap.Offset(handle, offset);
    }
    Descriptor::hOffset OffsetRTV(const Descriptor::Handle& handle, uint32_t offset) const {
        return m_rtvHeap.Offset(handle, offset);
    }
    Descriptor::hOffset OffsetDSV(const Descriptor::Handle& handle, uint32_t offset) const {
        return m_dsvHeap.Offset(handle, offset);
    }

    NSRenderer::Ctx GetCtx() {
        return NSRenderer::Ctx(
            [this](uint32_t amount) { return this->AllocSRVRing(amount); },
            [this](uint32_t amount) { return this->AllocSRVStatic(amount); },
            [this](uint32_t amount) { return this->AllocRTVStatic(amount); },
            [this](uint32_t amount) { return this->AllocDSVStatic(amount); },
            [this](Descriptor::Handle& handle) { this->FreeSRVStatic(handle); },
            [this](Descriptor::Handle& handle) { this->FreeRTVStatic(handle); },
            [this](Descriptor::Handle& handle) { this->FreeDSVStatic(handle); },
            [this](const Descriptor::Handle& handle, uint32_t offset) { return this->OffsetSRV(handle, offset); },
            [this](const Descriptor::Handle& handle, uint32_t offset) { return this->OffsetRTV(handle, offset); },
            [this](const Descriptor::Handle& handle, uint32_t offset) { return this->OffsetDSV(handle, offset); },
            [this](size_t size) { return this->m_allocator.Allocate(size); },
            [this](SceneModelKey key, NSRenderer::GraphicsCommandList cmdList) { return this->RegisterModel(key, cmdList); },
            [this](RegisterModelKey key) { return this->UnloadModel(key); },
            m_fallbackTextureSRVHandle
        );
    }

    ID3D12CommandQueue* ImGui_getCmdQueue() {
        return m_commandQueue.Get();
    }

    bool bbModelTestFlag(Scene& scene, SceneModelKey key, ERegModelFlag flag)
    {
        auto bbModels = m_blackboard.GetOpt<std::vector<NSRenderer::Model>>(NSRenderer::kRenderer_models);
        NSRenderer::Model& bbModel = bbModels->get()[key.index];

        return bbModel.TestFlag(flag);
    }
    void bbModelSetFlag(RegisterModelKey key, ERegModelFlag flag)
    {
        auto bbModels = m_blackboard.GetOpt<std::vector<NSRenderer::Model>>(NSRenderer::kRenderer_models);
        NSRenderer::Model& bbModel = bbModels->get()[key.index];

        bbModel.SetFlag(flag);
    }
    void bbModelResetFlag(Scene& scene, SceneModelKey key, ERegModelFlag flag)
    {
        auto bbModels = m_blackboard.GetOpt<std::vector<NSRenderer::Model>>(NSRenderer::kRenderer_models);
        NSRenderer::Model& bbModel = bbModels->get()[key.index];

        bbModel.ResetFlag(flag);
    }
    void bbModelFlipFlag(Scene& scene, SceneModelKey key, ERegModelFlag flag)
    {
        auto bbModels = m_blackboard.GetOpt<std::vector<NSRenderer::Model>>(NSRenderer::kRenderer_models);
        NSRenderer::Model& bbModel = bbModels->get()[key.index];

        bbModel.FlipFlag(flag);
    }

private:
    ID3D12Device14* m_device = nullptr;
    IDXGIFactory7* m_factory = nullptr;

    ComPtr<IDXGISwapChain4> m_swapchain;

    Descriptor::Handle m_rtHandle;
    ComPtr<ID3D12Resource2> m_renderTarget[IApp::ic_frameCount];

    Descriptor::Handle m_dsHandle;
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

    Blackboard m_blackboard;

    std::vector<std::unique_ptr<RenderPass::IRenderPass>> m_passes;

    std::unique_ptr<ShaderCompiler> m_shaderCompiler;

    uint32_t m_width{};
    uint32_t m_height{};

    void CreateSwapChain(IDXGIFactory7* factory, HWND hwnd, UINT width, UINT height);
    void CreateDepthStencil(LPCWSTR name, NSRenderer::DepthStencilCreateDescription desc);
    void CreateDefaultTexture();

    void MoveToNextFrame();

    static constexpr uint32_t FALLBACK_TEXTURE_SRV_INDEX = 0;
    static constexpr float CLEAR_COLOR[] = { 0.176f, 0.203f, 0.211f, 1.f };
};

