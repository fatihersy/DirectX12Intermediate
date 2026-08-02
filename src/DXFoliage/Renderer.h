#pragma once

#include "rhi/IRendererBackend.h"

#include <cstdint>
#include <memory>

namespace NSScene { class IScene; }

// The renderer FRONT-END: API-agnostic, thin. Owns an IRendererBackend
// (whichever the factory picked at startup) and drives the frame by
// recording into the command list the backend hands back. Contains no
// DirectX/Vulkan types and includes nothing from rhi/dx12/ or rhi/vulkan/
// — that's the litmus test for this layer.
//
// Engine-level render passes (shadow/gbuffer/post) belong here too when
// they arrive: an ordered list this class iterates each frame, each pass
// recording into the same ICommandList. (The IRenderPass scaffolding in
// RendererTypes.h still carries DX12 types in its signatures, so it needs
// its own neutralizing pass before it can be adopted here.)
class Renderer
{
public:
    Renderer();
    ~Renderer();

    bool Initialize(NSPlatform::IWindow& window, uint32_t width, uint32_t height);
    void Shutdown();
    void Resize(uint32_t width, uint32_t height);

    void BeginFrame();
    void DrawScene(std::shared_ptr<NSScene::IScene> scene);
    void EndFrame();

private:
    void CreateTriangleResources();
    void CreateFrameTargets();

    std::unique_ptr<NSRHI::IRendererBackend> m_backend;

    // Valid only between BeginFrame() and EndFrame().
    NSRHI::ICommandList* m_cmd{ nullptr };

    // Temporary demo content, front-end-owned (created through
    // m_backend->GetDevice()). Replaced by real scene/model resources
    // as the renderer grows.
    std::unique_ptr<NSRHI::IPipelineLayout> m_pipelineLayout;
    std::unique_ptr<NSRHI::IPipeline> m_pipeline;
    std::unique_ptr<NSRHI::IBuffer> m_vertexBuffer;
    std::unique_ptr<NSRHI::IBuffer> m_indexBuffer;

    // Recreated on resize: both must match the backbuffer extent, and
    // unlike the swapchain images nothing else owns them. The scene is
    // rendered into m_renderTarget and the backend blits it across at
    // EndFrame - the front-end never touches a swapchain image.
    std::unique_ptr<NSRHI::ITexture> m_renderTarget;
    std::unique_ptr<NSRHI::ITexture> m_depthBuffer;
    uint32_t m_vertexStride{};
    uint32_t m_indexCount{};

    uint32_t m_width{};
    uint32_t m_height{};
};
