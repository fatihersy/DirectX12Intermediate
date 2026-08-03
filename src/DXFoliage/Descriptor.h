#pragma once

#include "rhi/IDescriptorHeap.h"

#include <cstdint>
#include <vector>

// Descriptor ALLOCATION POLICY — the front-end half of the descriptor
// system, ported from DXTerrain's NSDescriptor (Descriptor.h) with the
// D3D12 types removed. The backend half (NSRHI::IDescriptorHeap) is
// passive storage: it owns the native heap/set and answers addressing
// questions; every decision about WHICH slot to hand out is made here, in
// plain uint32_t arithmetic, written once for both backends.
//
// This split is also what Unreal ships: FRHIDescriptorAllocator lives in
// RHICore (shared, platform-independent) and both the D3D12 and Vulkan
// backends consume it. Diligent and Godot go further and never expose the
// heap at all — but their higher-level binding objects sit on exactly the
// same internal division.
//
// IN-FLIGHT CONTRACT (the one rule that keeps this correct on both APIs):
//   - A ring slot is valid for THIS frame only. Reuse is safe because the
//     backend's frame fence has completed the previous submission that
//     used this frame index by the time BeginFrame() returns.
//   - A static slot is write-once. Freeing (and rewriting) it is legal
//     only when no in-flight frame still references it — after
//     WaitForGPU(), or FramesInFlight() frames after its last use.
//     DXTerrain frees statics only at load/shutdown boundaries, which
//     satisfies this trivially; keep doing that.
// UE enforces the same rule with heavy machinery (a CPU-authoritative
// heap plus versioned GPU heaps, SwitchToNewBindlessResourceHeap) because
// its handles are persistent and updated arbitrarily mid-flight. Ring
// slots per frame plus write-once statics make that machinery
// unnecessary — which is the point of this model.
namespace NSDescriptor
{
    // Long-lived slots handed out from a free list: material textures,
    // terrain maps, anything loaded once and referenced for many frames.
    class StaticHeap
    {
    public:
        StaticHeap() = default;
        explicit StaticHeap(NSRHI::IDescriptorHeap& heap);

        NSRHI::DescriptorHandle Allocate(uint32_t amount = 1u);
        void Free(NSRHI::DescriptorHandle handle);

    private:
        NSRHI::IDescriptorHeap* m_heap{ nullptr }; // non-owning; must outlive this
        std::vector<uint32_t> m_freeList;          // kept sorted ascending
    };

    // Per-frame transients plus a static tail, in one heap — both regions
    // in one because D3D12 allows only ONE shader-visible CBV_SRV_UAV heap
    // bound at a time, so splitting them across heaps is not an option
    // there. Layout mirrors DXTerrain's RingHeap:
    //
    //   [ frame 0 ring ][ frame 1 ring ] ... [ static region ]
    //   0               P                     P * framesInFlight
    //
    // Ring slots have no Free(): the whole per-frame range is reclaimed at
    // once when BeginFrame() comes back around to that index. That is not
    // an omission — it is what makes transient allocation one addition and
    // zero bookkeeping.
    class RingHeap
    {
    public:
        RingHeap() = default;
        RingHeap(NSRHI::IDescriptorHeap& heap, uint32_t perFrameCapacity,
                 uint32_t framesInFlight, uint32_t staticCapacity = 0u);

        // Call once per frame with IRendererBackend::FrameIndex(), after
        // the backend's BeginFrame() — the fence wait inside it is what
        // makes reusing this frame's range safe.
        void BeginFrame(uint32_t frameIndex);

        NSRHI::DescriptorHandle AllocateRing(uint32_t amount = 1u);
        NSRHI::DescriptorHandle AllocateStatic(uint32_t amount = 1u);
        void FreeStatic(NSRHI::DescriptorHandle handle);

    private:
        NSRHI::IDescriptorHeap* m_heap{ nullptr }; // non-owning; must outlive this
        std::vector<uint32_t> m_freeList;          // static region; sorted ascending

        uint32_t m_staticStart{};        // == perFrameCapacity * framesInFlight
        uint32_t m_frameCapacity{};
        uint32_t m_frameOffset{};        // next ring slot this frame
        uint32_t m_frameEnd{};           // one past this frame's range
    };
}
