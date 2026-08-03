#include "stdafx.h"
#include "DX12DescriptorHeap.h"

#include "DXSampleHelper.h"

namespace NSRHIDX12
{
    namespace
    {
        D3D12_DESCRIPTOR_HEAP_TYPE ToD3D12HeapType(NSRHI::EDescriptorHeapType type)
        {
            switch (type)
            {
                case NSRHI::EDescriptorHeapType::Sampler: return D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
                case NSRHI::EDescriptorHeapType::ShaderResource:
                default:                                  return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            }
        }
    }

    DX12DescriptorHeap::DX12DescriptorHeap(ID3D12Device14* device, const NSRHI::DescriptorHeapDesc& desc)
    {
        ASSERT(desc.capacity > 0, "Descriptor heap needs a non-zero capacity");

        im_capacity = desc.capacity;
        im_type = desc.type;
        im_heapId = NSRHI::NextDescriptorHeapId();
        m_shaderVisible = desc.shaderVisible;

        const D3D12_DESCRIPTOR_HEAP_TYPE nativeType = ToD3D12HeapType(desc.type);

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.Type = nativeType;
        heapDesc.NumDescriptors = im_capacity;
        heapDesc.Flags = m_shaderVisible
            ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
            : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_heap)));

        // The stride is a per-device, per-type property and is NOT a
        // compile-time constant - it varies by hardware, which is exactly
        // why every handle has to be computed rather than assumed.
        m_descriptorSize = device->GetDescriptorHandleIncrementSize(nativeType);

        m_cpuStart = m_heap->GetCPUDescriptorHandleForHeapStart();

        // Only a shader-visible heap has a GPU handle at all; asking a
        // non-shader-visible one is invalid and the debug layer says so.
        if (m_shaderVisible)
        {
            m_gpuStart = m_heap->GetGPUDescriptorHandleForHeapStart();
        }
    }

    DX12DescriptorHeap::~DX12DescriptorHeap()
    {
        Reset();
    }

    void DX12DescriptorHeap::Reset()
    {
        m_heap.Reset();
        m_cpuStart = {};
        m_gpuStart = {};
        m_descriptorSize = 0;
        im_capacity = 0;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorHeap::CpuHandle(uint32_t index) const
    {
        ASSERT(index < im_capacity, "Descriptor index past the end of the heap");
        return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_cpuStart, static_cast<INT>(index), m_descriptorSize);
    }

    D3D12_GPU_DESCRIPTOR_HANDLE DX12DescriptorHeap::GpuHandle(uint32_t index) const
    {
        ASSERT(index < im_capacity, "Descriptor index past the end of the heap");
        ASSERT(m_shaderVisible, "A non-shader-visible heap has no GPU handle");
        return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_gpuStart, static_cast<INT>(index), m_descriptorSize);
    }
}
