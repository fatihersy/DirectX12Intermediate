#pragma once

#include "PlatformHeaders_DX12.h"
#include "rhi/IFence.h"

#include <cstdint>

// DX12 implementation of IFence. Ports the fence-related code from
// Renderer.cpp (m_fence/m_fenceEvent, MoveToNextFrame/WaitForGPU) behind
// the interface, near-verbatim — same CreateFence/CreateEvent/Signal/
// SetEventOnCompletion/WaitForSingleObjectEx sequence, just given its own
// class instead of being inlined into Renderer's methods.
namespace NSRHIDX12
{
    class DX12Fence final : public NSRHI::IFence
    {
    public:
        // queue is the command queue this fence gets signaled on — D3D12
        // requires a queue to signal a fence from the GPU side
        // (ID3D12CommandQueue::Signal), unlike the neutral IFence
        // interface, which doesn't need callers to know that.
        DX12Fence(ID3D12Device14* device, ID3D12CommandQueue* queue, uint64_t initialValue);
        ~DX12Fence() override;

        DX12Fence(const DX12Fence&) = delete;
        DX12Fence& operator=(const DX12Fence&) = delete;

        void Signal(uint64_t value) override;
        void Wait(uint64_t value) override;
        uint64_t CompletedValue() const override;

    private:
        ID3D12CommandQueue* m_queue{ nullptr };
        ComPtr<ID3D12Fence1> m_fence;
        HANDLE m_fenceEvent{ nullptr };
    };
}
