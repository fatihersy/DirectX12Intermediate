#include "stdafx.h"
#include "DX12Fence.h"

#include "DXSampleHelper.h"

namespace NSRHIDX12
{
    DX12Fence::DX12Fence(ID3D12Device14* device, ID3D12CommandQueue* queue, uint64_t initialValue)
        : m_queue(queue)
    {
        ThrowIfFailed(device->CreateFence(initialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fence->SetName(L"DX12Fence");

        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (not m_fenceEvent)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }
    }

    DX12Fence::~DX12Fence()
    {
        if (m_fenceEvent)
        {
            CloseHandle(m_fenceEvent);
            m_fenceEvent = nullptr;
        }
    }

    void DX12Fence::Signal(uint64_t value)
    {
        ThrowIfFailed(m_queue->Signal(m_fence.Get(), value));
    }

    void DX12Fence::Wait(uint64_t value)
    {
        if (m_fence->GetCompletedValue() < value)
        {
            ThrowIfFailed(m_fence->SetEventOnCompletion(value, m_fenceEvent));
            WaitForSingleObjectEx(m_fenceEvent, INFINITE, false);
        }
    }

    uint64_t DX12Fence::CompletedValue() const
    {
        return m_fence->GetCompletedValue();
    }
}
