#pragma once

#include "IApp.h"

#include "Descriptor.h"
#include "Allocator.h"
#include "Blackboard.h"
#include "ShaderCompiler.h"
#include "Terrain.h"

class Scene;

using FnRendererExecutionBody = std::function<void(NSRenderer::Ctx ctx, NSRenderer::GraphicsCommandList cmdList)>;

class Renderer
{
public:
    Renderer(){};
    ~Renderer();

    void Init(IDXGIFactory7* factory, ID3D12Device14* device, HWND wnd, UINT width, UINT height);

    void BeginFrame();
    void DrawScene(Scene& scene);
    void EndFrame();
    NSRenderer::Model& RegisterModel(std::wstring_view modelName, NSModel::SceneModelKey sceneKey, NSRenderer::GraphicsCommandList cmdList, NSModel::ERegModelFlag flag);
    void UnloadModel(NSModel::RegisterModelKey key);

    void Resize(UINT width, UINT height);

    void OnDestroy();

    void Execute(FnRendererExecutionBody Record);

    template<typename T, typename... Args> requires std::derived_from<T, NSRenderPass::IRenderPass>
    NSRenderPass::IRenderPass& AddPass(Args&&... args) {
        return *m_passes.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
    }

    NSDescriptor::Handle AllocSRVRing(uint32_t amount = 1u) {
        return m_srvHeap.AllocateRing(amount);
    }
    NSDescriptor::Handle AllocSRVStatic(uint32_t amount = 1u) {
        return m_srvHeap.AllocateStatic(amount);
    }
    NSDescriptor::Handle AllocRTVStatic(uint32_t amount = 1u) {
        return m_rtvHeap.Allocate(amount);
    }
    NSDescriptor::Handle AllocDSVStatic(uint32_t amount = 1u) {
        return m_dsvHeap.Allocate(amount);
    }
    void FreeSRVStatic(NSDescriptor::Handle handle) {
        return m_srvHeap.FreeStatic(handle);
    }
    void FreeRTVStatic(NSDescriptor::Handle handle) {
        return m_rtvHeap.Free(handle);
    }
    void FreeDSVStatic(NSDescriptor::Handle handle) {
        return m_dsvHeap.Free(handle);
    }
    NSDescriptor::Offset OffsetSRV(const NSDescriptor::Handle& handle, uint32_t offset) {
        return m_srvHeap.OffsetOf(handle, offset);
    }
    NSDescriptor::Offset OffsetRTV(const NSDescriptor::Handle& handle, uint32_t offset) {
        return m_rtvHeap.OffsetOf(handle, offset);
    }
    NSDescriptor::Offset OffsetDSV(const NSDescriptor::Handle& handle, uint32_t offset) {
        return m_dsvHeap.OffsetOf(handle, offset);
    }

    std::reference_wrapper<NSBarrier::IBarrierBatch> GetBarrierBatch() {
        assert(m_barrierBatch);
        return std::ref(*m_barrierBatch);
    }

    void CreateTerrain(NSRenderer::GraphicsCommandList cmdList, NSTerrain::TerrainDesc desc)
    {
        m_terrain.OnInit(cmdList, GetCtx(), desc);
    }

    NSRenderer::Ctx GetCtx() {
        return NSRenderer::Ctx(
            [this](uint32_t amount)                                                    -> NSDescriptor::Handle { return this->AllocSRVRing(amount);               },
            [this](uint32_t amount)                                                    -> NSDescriptor::Handle { return this->AllocSRVStatic(amount);             },
            [this](uint32_t amount)                                                    -> NSDescriptor::Handle { return this->AllocRTVStatic(amount);             },
            [this](uint32_t amount)                                                    -> NSDescriptor::Handle { return this->AllocDSVStatic(amount);             },
            [this](NSDescriptor::Handle& handle) { return this->FreeSRVStatic(handle);              },
            [this](NSDescriptor::Handle& handle) { return this->FreeRTVStatic(handle);              },
            [this](NSDescriptor::Handle& handle) { return this->FreeDSVStatic(handle);              },
            [this](const NSDescriptor::Handle& handle, uint32_t offset)                -> NSDescriptor::Offset { return this->OffsetSRV(handle, offset);          },
            [this](const NSDescriptor::Handle& handle, uint32_t offset)                -> NSDescriptor::Offset { return this->OffsetRTV(handle, offset);          },
            [this](const NSDescriptor::Handle& handle, uint32_t offset)                -> NSDescriptor::Offset { return this->OffsetDSV(handle, offset);          },
            [this](size_t size)                                                        -> NSAllocator::Ctx { return this->m_constantAllocator.Allocate(size); },
            [this](std::wstring_view modelName, NSModel::SceneModelKey key, NSRenderer::GraphicsCommandList cmdList, NSModel::ERegModelFlag flag)-> NSRenderer::Model&
            {
                return this->RegisterModel(modelName, key, cmdList, flag);
            },
            [this](NSModel::RegisterModelKey key) { this->UnloadModel(key); },
            m_fallbackTextureSRVhandle,
            GetBarrierBatch()
        );
    }

    bool TestFlagForRegModel(Scene& scene, NSModel::RegisterModelKey& key, NSModel::ERegModelFlag flag)
    {
        auto optRegModels = m_blackboard.GetOpt<std::vector<NSRenderer::Model>>(NSRenderer::kRenderer_models);
        assert(optRegModels.has_value() and optRegModels->get().size() > key.index);

        return optRegModels->get()[key.index].TestFlag(flag);
    }
    void SetFlagForRegModel(Scene& scene, NSModel::RegisterModelKey& key, NSModel::ERegModelFlag flag)
    {
        auto optRegModels = m_blackboard.GetOpt<std::vector<NSRenderer::Model>>(NSRenderer::kRenderer_models);
        assert(optRegModels.has_value() and optRegModels->get().size() > key.index);

        optRegModels->get()[key.index].SetFlag(flag);
    }
    void ResetFlagForRegModel(Scene& scene, NSModel::RegisterModelKey& key, NSModel::ERegModelFlag flag)
    {
        auto optRegModels = m_blackboard.GetOpt<std::vector<NSRenderer::Model>>(NSRenderer::kRenderer_models);
        assert(optRegModels.has_value() and optRegModels->get().size() > key.index);

        optRegModels->get()[key.index].ResetFlag(flag);
    }
    void FlipFlagForRegModel(Scene& scene, NSModel::RegisterModelKey& key, NSModel::ERegModelFlag flag)
    {
        auto optRegModels = m_blackboard.GetOpt<std::vector<NSRenderer::Model>>(NSRenderer::kRenderer_models);
        assert(optRegModels.has_value() and optRegModels->get().size() > key.index);

        optRegModels->get()[key.index].FlipFlag(flag);
    }
private:
    IDXGIFactory7* m_factory = nullptr;
    ID3D12Device14* m_device = nullptr;

    ComPtr<IDXGISwapChain4> m_swapChain;

    NSDescriptor::Handle m_rtHandle;
    ComPtr<ID3D12Resource> m_renderTargets[IApp::ic_framesInFlight];

    NSDescriptor::Handle m_dsHandle;
    ComPtr<ID3D12Resource2> m_depthStencil;

    ComPtr<ID3D12CommandAllocator> m_commandAllocators[IApp::ic_framesInFlight];
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12GraphicsCommandList10> m_commandList;

    ComPtr<ID3D12Fence1> m_fence;
    HANDLE m_fenceEvent = nullptr;
    UINT32 m_fenceGeneration{};

    ConstantAllocator m_constantAllocator;

    NSDescriptor::StaticHeap m_rtvHeap;
    NSDescriptor::StaticHeap m_dsvHeap;
    NSDescriptor::RingHeap m_srvHeap;

    NSTexture::Texture m_fallbackTexture;
    NSDescriptor::Handle m_fallbackTextureSRVhandle;

    Blackboard m_blackboard;

    std::unique_ptr<NSBarrier::IBarrierBatch> m_barrierBatch;

    std::vector<std::unique_ptr<NSRenderPass::IRenderPass>> m_passes;

    std::unique_ptr<ShaderCompiler> m_shaderCompiler;

    uint32_t m_width{};
    uint32_t m_height{};

    void CreateSwapChain(HWND hwnd, UINT width, UINT height);
    void CreateDepthStencil(LPCWSTR name, NSRenderer::DepthStencilCreateDescription desc);
    void CreateFallbackTexture();

    void MoveToNextFrame();
    void WaitForGPU();

    constexpr static float CLEAR_COLOR[4] = { .0f, .0f, .0f, 1.f };

    NSTerrain::Terrain m_terrain;

    //ImGui
    ComPtr<ID3D12DescriptorHeap> m_imGuiSrvHeap;
    std::vector<INT> m_freeImGuiSRVIndices;
    UINT m_imGuiSrvDescriptorSize{};
};
