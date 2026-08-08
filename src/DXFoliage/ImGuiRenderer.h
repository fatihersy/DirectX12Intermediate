#pragma once

#include "Allocator.h"
#include "Descriptor.h"
#include "rhi/ICommandList.h"
#include "rhi/IDevice.h"

#include <cstdint>
#include <memory>
#include <unordered_map>

struct ImTextureData;

// Dear ImGui's RENDERER BACKEND, written against the neutral RHI instead
// of the vendored imgui_impl_vulkan/imgui_impl_dx12 — one implementation
// serving both APIs. See the decision note in PLAN.md for why the vendored
// route was rejected (it would make the graphics backend learn what a
// shader and a font atlas are, which is the one thing the seam exists to
// prevent).
//
// NOT called "Pass", despite being invoked from inside the rendering
// scope: it is not one, and task #14 will introduce a real IRenderPass
// interface that this class does not implement. "Renderer backend" is
// ImGui's own term for exactly this object. It also cannot be called
// plain "ImGui" — that name is taken by Dear ImGui's own namespace, and
// C++ forbids a class and a namespace sharing a name in one scope.
//
// This class owns the two halves of a renderer backend:
//   1. TEXTURES — ImGui 1.92's ImTextureData lifecycle: WantCreate /
//      WantUpdates / WantDestroy, with ImTextureID carrying our bindless
//      descriptor index. That index IS the binding, so ImDrawCmd's
//      texture reference costs a root constant and nothing else.
//   2. GEOMETRY — walking ImDrawData into RHI calls (lands with step D).
//
// Deliberately NOT owning: the ImGui context lifetime or NewFrame, which
// belong with whoever also feeds input.
class ImGuiRenderer
{
public:
    ImGuiRenderer() = default;
    ~ImGuiRenderer();

    ImGuiRenderer(const ImGuiRenderer&) = delete;
    ImGuiRenderer& operator=(const ImGuiRenderer&) = delete;

    // heap/descriptors: THE bindless heap and its allocator — atlas slots
    // come from the static region, since a font atlas outlives every
    // frame. uploads: a staging ring, 512-aligned for D3D12's
    // placed-footprint rule.
    bool Initialize(NSRHI::IDevice& device,
                    NSRHI::IDescriptorHeap& heap,
                    NSDescriptor::RingHeap& descriptors,
                    NSAllocator::RingAllocator& uploads,
                    NSAllocator::RingAllocator& vertices,
                    NSAllocator::RingAllocator& indices,
                    NSRHI::EFormat colorTargetFormat,
                    NSRHI::EFormat depthTargetFormat);
    void Shutdown();

    // Honours every pending ImTextureData request. MUST be called before
    // BeginRendering — copies are illegal inside a dynamic-rendering
    // pass, and this records them into the frame's command list rather
    // than spinning up a private one and stalling, which is what the
    // vendored backends have to do for lack of access to the frame.
    void UpdateTextures(NSRHI::ICommandList& cmd);

    // Records the UI. Call INSIDE the rendering scope, last, so the UI
    // sits on top of the scene. Assumes the caller has already set the
    // descriptor heap for the frame.
    void Render(NSRHI::ICommandList& cmd);

    // The atlas's slot in the bindless heap, or an invalid handle before
    // the first UpdateTextures. Exposed so the checker cube can sample it
    // as step B's proof, ahead of any ImGui geometry existing.
    NSRHI::DescriptorHandle FontAtlasSlot() const { return m_fontAtlasSlot; }

private:
    void CreateTexture(NSRHI::ICommandList& cmd, ImTextureData* tex);
    void UploadRegion(NSRHI::ICommandList& cmd, ImTextureData* tex,
                      uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    void DestroyTexture(ImTextureData* tex);

    NSRHI::IDevice* m_device{ nullptr };
    NSRHI::IDescriptorHeap* m_heap{ nullptr };
    NSDescriptor::RingHeap* m_descriptors{ nullptr };
    NSAllocator::RingAllocator* m_uploads{ nullptr };
    NSAllocator::RingAllocator* m_vertices{ nullptr };
    NSAllocator::RingAllocator* m_indices{ nullptr };

    std::unique_ptr<NSRHI::IPipelineLayout> m_layout;
    std::unique_ptr<NSRHI::IPipeline> m_pipeline;

    // Keyed by descriptor index, which is what ImTextureID carries.
    struct BackedTexture
    {
        std::unique_ptr<NSRHI::ITexture> texture;
        NSRHI::DescriptorHandle slot;
    };
    std::unordered_map<uint32_t, BackedTexture> m_textures;

    NSRHI::DescriptorHandle m_fontAtlasSlot;
};
