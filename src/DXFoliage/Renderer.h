#pragma once

#include "IApp.h"
#include "RendererTypes.h"

#include "ShaderCompiler.h"
#include "Pipeline.h"

using FnRendererExecutionBody = std::function<void(NSRenderer::Ctx ctx, NSDX12::GraphicsCommandList cmdList)>;

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void OnInit(IDXGIFactory7* factory, ID3D12Device14* device, IWICImagingFactory2* wicFactory, NSRenderer::RendererDescription desc);
    void OnDestroy();

    void Update();
    void BeginFrame();
    void DrawScene(std::shared_ptr<NSScene::IScene> scene);
    void EndFrame();

    std::shared_ptr<NSRenderer::Model> RegisterModel(std::wstring_view modelName, ObserverKey sceneKey, NSDX12::GraphicsCommandList cmdList, Flag<NSRenderer::ERegModelFlag> flag);
    void UnloadModel(ObserverKey key);

    void Resize(uint32_t width, uint32_t height);
    void Execute(FnRendererExecutionBody Record);

    template<typename T, typename... Args> requires NSRenderPass::IsRenderPass<T>
    MemberRef<T> AddPass(Args&&... args)
    {
        ASSERT_VAL(m_passes[static_cast<size_t>(T::ID)] == nullptr);

        m_passes[static_cast<size_t>(T::ID)] = std::move(std::make_unique<T>(std::forward<Args>(args)...));

        return static_cast<T&>(DE_REF(m_passes[static_cast<size_t>(T::ID)]));
    }

    MemberRef<NSRenderPass::IRenderPass> GetPass(NSRenderPass::RenderPassID id)
    {
        return DE_REF(m_passes[static_cast<size_t>(id)]);
    }

    MemberRef<NSBarrier::IBarrierBatch> GetBarrierBatch()
    {
        return std::ref(DE_REF(m_barrierBatch));
    }

    NSRenderer::Ctx GetCtx()
    {
        return NSRenderer::Ctx
        (
            [this](std::wstring_view name, ObserverKey key, NSDX12::GraphicsCommandList cmdList, Flag<NSRenderer::ERegModelFlag> flag) -> std::shared_ptr<NSRenderer::Model>
            {
                return this->RegisterModel(name, key, cmdList, flag);
            },
            [this](ObserverKey key)
            {
                this->UnloadModel(key);
            },
            GetBarrierBatch(),
            [this](Flag<NSRenderer::ERendererFlag> flags) -> bool { return this->m_flags.HasLeastOne(flags); },
            [this](Flag<NSRenderer::ERendererFlag> flags) -> bool { return this->m_flags.HasLeastAll(flags); },
            [this](Flag<NSRenderer::ERendererFlag> flags) -> bool { return this->m_flags.HasExact(flags); },
            [this]()                                      -> bool { return this->m_flags.Empty(); },
            [this](Flag<NSRenderer::ERendererFlag> flags) -> void { this->m_flags.Set(flags); },
            [this](Flag<NSRenderer::ERendererFlag> flags) -> void { this->m_flags.UnSet(flags); },
            [this](Flag<NSRenderer::ERendererFlag> flags) -> void { this->m_flags.Toggle(flags); },
            m_desc
        );
    }

    Flag<NSRenderer::ERendererFlag> m_flags;
private:
    NSRenderer::RendererDescription m_desc;

    IDXGIFactory7* m_factory{nullptr};
    ID3D12Device14* m_device{nullptr};
    IWICImagingFactory2* m_wicFactory{nullptr};

    ComPtr<IDXGISwapChain4> m_swapChain;
    ComPtr<ID3D12Resource2> m_depthStencil[IApp::ic_framesInFlight];
    ComPtr<ID3D12Resource> m_renderTargets[IApp::ic_framesInFlight];

    static constexpr DXGI_FORMAT SCENE_COLOR_FORMAT = DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_FLOAT;
    ComPtr<ID3D12Resource2> m_sceneColor;
    void CreateSceneColor(uint32_t width, uint32_t height);

    ComPtr<ID3D12CommandAllocator> m_commandAllocators[IApp::ic_framesInFlight];
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12GraphicsCommandList10> m_commandList;

    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    uint32_t m_rtvDescriptorSize;
    uint32_t m_dsvDescriptorSize;

    // Temporary
    GraphicsPipeline m_pipeline;
    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

    ComPtr<ID3D12CommandAllocator> m_copyCommandAllocators[IApp::ic_framesInFlight];
    ComPtr<ID3D12GraphicsCommandList10> m_copyCommandList;
    std::shared_ptr<NSDX12::CopyCommandList> m_copyCmdListPublic;

    ComPtr<ID3D12Fence1> m_fence;
    HANDLE m_fenceEvent{nullptr};
    uint64_t m_fenceGeneration{};

    std::unique_ptr<ShaderCompiler> m_shaderCompiler;
    std::unique_ptr<NSBarrier::IBarrierBatch> m_barrierBatch;

    std::array<
        std::unique_ptr<NSRenderPass::IRenderPass>,
        static_cast<size_t>(NSRenderPass::RenderPassID::PASSID_MAX)
    > m_passes;

    void DrawDebugImage();

    void CreateSwapChain(HWND hwnd, uint32_t width, uint32_t height);
    void CreateDepthStencil(std::wstring_view name, NSRenderer::DepthStencilCreateDescription desc);
    void CreateFallbackTexture();

    void MoveToNextFrame();
    void WaitForGPU();

    constexpr static float CLEAR_COLOR[4]{.0f, .0f, .0f, .0f};

    NSRenderPass::DebugUtils m_debugUtils;

    ComPtr<ID3D12InfoQueue1> m_infoQueue1;
    DWORD m_infoQueueCookie{};

    //  ImGui
    ComPtr<ID3D12DescriptorHeap> m_imGuiSrvHeap;
    std::vector<uint32_t> m_freeImGuiSRVIndices;
    uint32_t m_imGuiSrvDescriptorSize{};
};
