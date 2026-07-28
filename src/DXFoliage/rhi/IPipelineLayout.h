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
    struct PipelineLayoutDesc
    {
        uint32_t num32BitRootConstants{ 0 };
        bool usesBindlessDescriptorTable{ false };
    };

    class IPipelineLayout
    {
    public:
        virtual ~IPipelineLayout() = default;
    };
}
