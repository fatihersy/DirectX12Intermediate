#pragma once

#include "Descriptor.h"
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
// recording into the same ICommandList. Deliberately NOT built yet -
// there is one pass, and an abstraction designed over a single instance
// is a guess. An earlier IRenderPass sketch lived in RendererTypes.h;
// see git history before it was deleted.
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
    void CreateCheckerCubeResources();
    void CreateBlendProofResources();  // TEMP-BLEND
    void CreateFrameTargets();

    std::unique_ptr<NSRHI::IRendererBackend> m_backend;

    // Valid only between BeginFrame() and EndFrame().
    NSRHI::ICommandList* m_cmd{ nullptr };

    // THE descriptor heap — singular by constraint, not convenience:
    // D3D12 allows one shader-visible CBV_SRV_UAV heap bound at a time,
    // so ring transients and long-lived statics share it, split by the
    // front-end RingHeap policy (see Descriptor.h and the in-flight
    // contract there).
    std::unique_ptr<NSRHI::IDescriptorHeap> m_descriptorHeap;
    NSDescriptor::RingHeap m_descriptors;

    // Demo content: a 4x4x4 checkerboard cube — the bindless texture
    // path's first consumer (slot from AllocateStatic, index pushed as a
    // root constant, shader reads g_textures[index]). The checker cell
    // size is 1 world unit, so the texture doubles as a ruler: four
    // cells per edge = four units.
    std::unique_ptr<NSRHI::IPipelineLayout> m_pipelineLayout;      // quad's: 16 root constants, no heap
    std::unique_ptr<NSRHI::IPipelineLayout> m_texPipelineLayout;   // cube's: 17 root constants + bindless heap
    std::unique_ptr<NSRHI::IPipeline> m_pipeline;
    std::unique_ptr<NSRHI::IBuffer> m_vertexBuffer;
    std::unique_ptr<NSRHI::IBuffer> m_indexBuffer;

    std::unique_ptr<NSRHI::ITexture> m_checkerTexture;
    // Kept alive for the renderer's lifetime rather than freed after the
    // upload: the copy is recorded into frame 1's command list, and the
    // front-end has no per-resource fence to know when the GPU is done
    // reading it. 256 KB of idle staging is cheaper than the machinery.
    std::unique_ptr<NSRHI::IBuffer> m_checkerStaging;
    NSRHI::DescriptorHandle m_checkerSlot;
    bool m_checkerUploadPending{ false };

    // TEMP-BLEND: proves EBlendMode::AlphaBlend reaches the GPU, ahead of
    // ImGui depending on it. A half-transparent quad drawn over the cubes:
    // if blending works the cubes show through, if it silently does
    // nothing they are hidden behind a solid rectangle. Delete once ImGui
    // renders - that becomes the real consumer.
    std::unique_ptr<NSRHI::IPipeline> m_blendPipeline;
    std::unique_ptr<NSRHI::IBuffer> m_quadVertexBuffer;
    std::unique_ptr<NSRHI::IBuffer> m_quadIndexBuffer;
    uint32_t m_quadIndexCount{};

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
