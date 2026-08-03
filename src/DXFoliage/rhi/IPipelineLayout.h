#pragma once

#include <cstdint>

// The neutral version of a D3D12 root signature / Vulkan pipeline layout:
// describes what data a pipeline expects to receive when it's bound, not
// the data itself. Two kinds of input cover what the existing D3D12 code
// (and DXTerrain, which already exercises this fully) actually uses:
//   - root constants: a handful of raw 32-bit values pushed directly per
//     draw call (-> vkCmdPushConstants on the Vulkan backend)
//   - one descriptor-table binding pointing at the global bindless heap
//     from IDescriptorHeap.h (-> one VkDescriptorSet bound once per frame)
//
// Today's DXFoliage pipeline has a genuinely empty root signature — zero
// root parameters, zero static samplers. That's just the
// zero-constants/no-bindless-table case of this same interface, not a
// separate "trivial" path that gets thrown away once real shaders with
// textures/materials are added.
namespace NSRHI
{
    class IBuffer;
    class IDescriptorHeap;

    // At most this many per-draw constant buffer slots in one layout.
    // DXTerrain never exceeds 3 simultaneous root CBVs in any of its 11
    // root signatures; 4 leaves one spare. Kept small on purpose: D3D12
    // root-signature cost scales with declared parameters, and Vulkan's
    // dynamic offsets are supplied for every declared slot at each bind.
    inline constexpr uint32_t kMaxConstantBufferSlots = 4;

    struct PipelineLayoutDesc
    {
        uint32_t num32BitRootConstants{ 0 };
        bool usesBindlessDescriptorTable{ false };

        // Which heap the bindless table refers to. Required when
        // usesBindlessDescriptorTable is set, ignored otherwise.
        //
        // Explicit rather than "the device's heap", because there is no
        // longer a single device-owned heap to mean — the front-end
        // creates them. Vulkan needs the matching VkDescriptorSetLayout at
        // pipeline-layout creation, so this cannot be deferred to bind
        // time the way D3D12 could.
        IDescriptorHeap* bindlessHeap{ nullptr };

        // Per-draw constant buffer slots (the neutral form of DXTerrain's
        // root CBVs — 23 SetGraphicsRootConstantBufferView sites). Slot N
        // is register b{N+1} in HLSL (b0 stays the root-constant block)
        // and [[vk::binding(N, 1)]] on Vulkan. `constantBuffer` is the
        // ConstantAllocator's buffer, named here for the same reason as
        // bindlessHeap: the DX12 backend stashes its base GPU address,
        // and Vulkan writes it into the layout's dynamic-UBO descriptor
        // set — both fixed at layout creation, only OFFSETS move per draw.
        uint32_t numConstantBufferSlots{ 0 };
        IBuffer* constantBuffer{ nullptr };
    };

    class IPipelineLayout
    {
    public:
        virtual ~IPipelineLayout() = default;
    };
}
