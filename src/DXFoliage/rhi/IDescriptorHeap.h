#pragma once

#include <cstdint>
#include <limits>

// Mirrors the existing (currently unused by DXFoliage's Renderer.cpp, but
// proven in DXTerrain's Descriptor.h) StaticHeap/RingHeap pattern: a big
// pool of texture/buffer slots, each handed out as a plain index rather
// than a specific per-draw binding slot. Shaders reference a resource by
// that index — baked straight into a constant buffer field, exactly like
// DXTerrain's `TerrainConstants::heightmapSrvIndex` already does — instead
// of the CPU rebinding a specific slot before every draw call. This is
// the "bindless" pattern, and it maps onto both APIs without needing a
// new convention on the Vulkan side:
//   - DX12 backend: this literally *is* NSDescriptor::StaticHeap/RingHeap.
//   - Vulkan backend: one big VkDescriptorSet using descriptor indexing
//     (VK_EXT_descriptor_indexing, UPDATE_AFTER_BIND + PARTIALLY_BOUND),
//     an extension specifically designed to mimic this exact D3D12
//     bindless-heap model — so the "index baked into a constant buffer"
//     convention carries over unchanged.
namespace NSRHI
{
    struct DescriptorHandle
    {
        uint32_t index{ std::numeric_limits<uint32_t>::max() };
        uint32_t amount{ 1 };

        bool IsValid() const { return index != std::numeric_limits<uint32_t>::max(); }
    };

    class IDescriptorHeap
    {
    public:
        virtual ~IDescriptorHeap() = default;

        virtual DescriptorHandle Allocate(uint32_t amount = 1) = 0;
        virtual void Free(DescriptorHandle handle) = 0;
    };
}
