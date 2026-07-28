#include "stdafx.h"
#include "DX12Buffer.h"

#include "DXSampleHelper.h"

namespace NSRHIDX12
{
    DX12Buffer::DX12Buffer(ID3D12Device14* device, const NSRHI::BufferDesc& desc)
        : m_size(desc.sizeBytes)
        , m_cpuVisible(desc.cpuVisible)
    {
        const D3D12_HEAP_TYPE heapType = desc.cpuVisible ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;
        const D3D12_RESOURCE_STATES initialState = desc.cpuVisible ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COMMON;

        CD3DX12_HEAP_PROPERTIES heapProps{ heapType };
        CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(desc.sizeBytes);

        ThrowIfFailed(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            initialState,
            nullptr,
            IID_PPV_ARGS(&m_resource)
        ));
    }

    void* DX12Buffer::Map()
    {
        ASSERT(m_cpuVisible, "Buffer isn't CPU-visible");

        void* data = nullptr;
        CD3DX12_RANGE readRange(0, 0);
        ThrowIfFailed(m_resource->Map(0, &readRange, &data));
        return data;
    }

    void DX12Buffer::Unmap()
    {
        m_resource->Unmap(0, nullptr);
    }
}
