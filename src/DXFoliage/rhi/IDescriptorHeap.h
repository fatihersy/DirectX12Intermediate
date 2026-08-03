#pragma once

#include <cstdint>
#include <limits>

// The neutral form of NSDescriptor::IDescriptor (DXTerrain/Descriptor.h):
// a block of descriptor storage plus the addressing over it. Deliberately
// PASSIVE — it hands out addresses and answers questions about them, and
// that is all.
//
// Two things it explicitly does NOT do, both of which were tried and were
// wrong:
//
//   - It does not allocate. Free lists and per-frame rings are index
//     arithmetic with no API surface in them at all (see
//     NSDescriptor::StaticHeap/RingHeap: every line is uint32_t maths), so
//     they are FRONT-END code written once, not a virtual implemented
//     twice. A heap has an allocator; it is not one.
//
//   - It does not write descriptors. That is a device operation in both
//     APIs — D3D12 spells it device->CreateShaderResourceView(res, &desc,
//     cpuHandle) and Vulkan spells it vkUpdateDescriptorSets(device, ...),
//     with the heap or set as the destination in each case. See
//     IDevice::CreateShaderResourceView.
//
// ADDRESS REPRESENTATION. NSDescriptor::Offset carries a cpuAddr/gpuAddr
// pair because that is what D3D12 needs at the call site. Neutrally an
// offset is just an INDEX: the DX12 backend rebuilds the handle pair from
// cpuStart + index * descriptorSize whenever it needs one, and Vulkan uses
// the index directly as dstArrayElement. No native address ever crosses
// the seam.
//
// That costs one thing worth replacing. IDescriptor::Validate/Owns work by
// recomputing the address from the index and comparing pointers, which
// catches a handle built by a different heap. With bare indices there is
// no redundancy left to check, so handles carry a heap id instead and the
// same question is answered by comparing that.
namespace NSRHI
{
    inline constexpr uint32_t kInvalidDescriptorIndex = std::numeric_limits<uint32_t>::max();
    inline constexpr uint32_t kInvalidDescriptorHeapId = 0;

    // Handed out by a front-end allocator; describes a RANGE.
    struct DescriptorHandle
    {
        uint32_t index{ kInvalidDescriptorIndex };
        uint32_t amount{ 1 };
        uint32_t heapId{ kInvalidDescriptorHeapId };

        bool IsValid() const
        {
            return index != kInvalidDescriptorIndex and heapId != kInvalidDescriptorHeapId;
        }
    };

    // A single slot within a heap — the result of At()/OffsetOf(), and what
    // a descriptor write targets.
    struct DescriptorOffset
    {
        uint32_t index{ kInvalidDescriptorIndex };
        uint32_t heapId{ kInvalidDescriptorHeapId };

        bool IsValid() const
        {
            return index != kInvalidDescriptorIndex and heapId != kInvalidDescriptorHeapId;
        }
    };

    // Only the two shader-facing kinds. RTV and DSV are deliberately
    // absent: Vulkan's dynamic rendering takes a VkImageView straight into
    // vkCmdBeginRendering with no heap involved, and DX12Texture already
    // carries its own private one-entry RTV/DSV heap. Exposing them here
    // would import a D3D12 constraint into an interface that does not have
    // it, and give the front-end a knob that means nothing on one backend.
    enum class EDescriptorHeapType : uint8_t
    {
        ShaderResource,  // SRV/UAV/CBV -> D3D12 CBV_SRV_UAV, Vulkan sampled-image array
        Sampler,
    };

    struct DescriptorHeapDesc
    {
        EDescriptorHeapType type{ EDescriptorHeapType::ShaderResource };
        uint32_t capacity{ 0 };

        // D3D12 distinguishes heaps the GPU can read from staging heaps it
        // cannot, because CPU writes into a shader-visible heap go to
        // write-combined memory and are slow to read back. Vulkan has no
        // such split and ignores this.
        bool shaderVisible{ true };
    };

    // Process-wide heap ids, starting at 1 so a zero-initialised handle is
    // invalid rather than pointing at the first heap.
    inline uint32_t NextDescriptorHeapId()
    {
        static uint32_t next = kInvalidDescriptorHeapId + 1;
        return next++;
    }

    class IDescriptorHeap
    {
    public:
        virtual ~IDescriptorHeap() = default;

        // Non-virtual on purpose: with index-based offsets these are the
        // same arithmetic on every backend, so making them virtual would
        // be two identical overrides and two places for a bug.
        bool Validate(DescriptorOffset offset) const
        {
            return offset.heapId == im_heapId and offset.index < im_capacity;
        }

        bool Owns(DescriptorHandle handle) const
        {
            return handle.heapId == im_heapId
                and handle.amount > 0
                and handle.index <= im_capacity - handle.amount;
        }

        DescriptorOffset At(uint32_t index) const
        {
            ASSERT(index < im_capacity, "Descriptor index past the end of the heap");
            return DescriptorOffset{ .index = index, .heapId = im_heapId };
        }

        DescriptorOffset OffsetOf(const DescriptorHandle& from, uint32_t offset) const
        {
            ASSERT(offset < from.amount, "Offset past the end of the allocated range");
            ASSERT(Owns(from), "Handle does not belong to this heap");
            return DescriptorOffset{ .index = from.index + offset, .heapId = im_heapId };
        }

        uint32_t Capacity() const { return im_capacity; }
        uint32_t HeapId() const { return im_heapId; }
        EDescriptorHeapType Type() const { return im_type; }

        // Releases the underlying storage. The counterpart of
        // IDescriptor::Reset(); virtual because what there is to release
        // differs (an ID3D12DescriptorHeap vs a pool, set and layout).
        virtual void Reset() = 0;

    protected:
        uint32_t im_capacity{};
        uint32_t im_heapId{ kInvalidDescriptorHeapId };
        EDescriptorHeapType im_type{ EDescriptorHeapType::ShaderResource };
    };
}
