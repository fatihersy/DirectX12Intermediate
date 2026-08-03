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

    class ConstantAllocator
    {
    public:
        ConstantAllocator() = default;
        // buffer: EBufferUsage::Constant, cpuVisible, created by the
        // front-end and named in every PipelineLayoutDesc that declares
        // constant slots. Must outlive this allocator (non-owning — same
        // pattern as RingHeap over IDescriptorHeap). Its size must be
        // ring bytes + kConstantBufferWindowBytes of tail slack; see
        // RHITypes.h for why the slack exists.
        ConstantAllocator(NSRHI::IBuffer& buffer, uint32_t framesInFlight);

        // Call once per frame with IRendererBackend::FrameIndex(), right
        // next to the descriptor ring's BeginFrame — the same fence wait
        // makes both safe.
        void BeginFrame(uint32_t frameIndex);

        Ctx Allocate(size_t size);

    private:
        NSRHI::IBuffer* m_buffer{ nullptr };
        uint8_t* m_cpuBase{ nullptr };

        size_t m_frameSize{};
        size_t m_frameEnd{};
        size_t m_offset{};
    };
}
