#pragma once

#include "rhi/IBuffer.h"
#include "rhi/RHITypes.h"

#include <cstdint>

// Per-draw constant allocation — DXTerrain's NSAllocator::ConstantAllocator
// (Allocator.h/.cpp) with the D3D12 types removed, same treatment as
// NSDescriptor in Descriptor.h. Constants are never individual buffers:
// they are bump allocations from ONE persistently-mapped upload buffer,
// rung by frame region exactly like the descriptor RingHeap — write
// through cpuAddr, hand offsetBytes to ICommandList::SetConstantBuffer.
//
// This is also what Diligent ships for USAGE_DYNAMIC buffers (one shared
// persistently-mapped storage, Map == bump the offset) and the shape of
// Unreal's FD3D12FastConstantAllocator. The design notes live in the
// ConstantAllocator report; the binding half's mapping (D3D12 root CBV ↔
// Vulkan UNIFORM_BUFFER_DYNAMIC + dynamic offsets) is the backends'
// business, not this file's.
//
// WHAT CHANGED FROM DXTerrain'S Ctx: gpuAddr (a D3D12_GPU_VIRTUAL_ADDRESS)
// cannot cross the RHI seam, so Ctx carries the byte OFFSET instead. The
// DX12 backend rebuilds baseVA + offset at bind time; Vulkan uses the
// offset directly as the dynamic offset — the same move that replaced
// NSDescriptor's cpuAddr/gpuAddr pair with a plain index.
//
// IN-FLIGHT CONTRACT: identical to the descriptor ring's (Descriptor.h).
// An allocation is valid for THIS frame only; reuse of the region is safe
// because the backend's frame fence retired the previous submission using
// this frame index before BeginFrame() returned.
namespace NSAllocator
{
    struct Ctx
    {
        void* cpuAddr = nullptr;
        uint64_t offsetBytes{};  // into the allocator's buffer; feed to SetConstantBuffer

        template<typename T> T& As()
        {
            ASSERT(cpuAddr);
            return *reinterpret_cast<T*>(cpuAddr);
        }

        explicit operator bool() const
        {
            return cpuAddr != nullptr;
        }
    };

    // Generalized when ImGui arrived and needed the identical ring for
    // per-frame VERTEX and INDEX data: it rebuilds its whole geometry
    // stream every frame, which is the same hazard and the same fix as
    // per-draw constants. One class, two alignments, rather than two
    // copies of the bump-and-wrap arithmetic.
    class RingAllocator
    {
    public:
        RingAllocator() = default;

        // buffer     - cpuVisible, created by the front-end, must outlive
        //              this allocator (non-owning, same pattern as
        //              RingHeap over IDescriptorHeap).
        // alignment  - kConstantBufferAlignment (256) for constants; the
        //              vertex stride for vertex data; index size for
        //              indices. Must be a power of two.
        // tailSlack  - bytes reserved after the last ring byte, excluded
        //              from the frame regions. Constants need
        //              kConstantBufferWindowBytes of it because a Vulkan
        //              dynamic-UBO descriptor's range is fixed and
        //              offset+range must stay in-buffer. Vertex and index
        //              bindings take an offset with no range, so they
        //              pass 0.
        RingAllocator(NSRHI::IBuffer& buffer, uint32_t framesInFlight,
                      size_t alignment, size_t tailSlack = 0);

        // Call once per frame with IRendererBackend::FrameIndex(), right
        // next to the descriptor ring's BeginFrame — the same fence wait
        // makes every ring safe.
        void BeginFrame(uint32_t frameIndex);

        Ctx Allocate(size_t size);

        // The buffer allocations come from — needed by any call that
        // takes (buffer, offset) rather than a pointer, e.g.
        // ICommandList::CopyBufferToTexture or SetVertexBuffer.
        NSRHI::IBuffer* Buffer() const { return m_buffer; }

    private:
        NSRHI::IBuffer* m_buffer{ nullptr };
        uint8_t* m_cpuBase{ nullptr };

        size_t m_alignment{ NSRHI::kConstantBufferAlignment };
        size_t m_maxAllocation{};
        size_t m_frameSize{};
        size_t m_frameEnd{};
        size_t m_offset{};
    };
}
