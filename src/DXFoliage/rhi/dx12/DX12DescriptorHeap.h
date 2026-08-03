#pragma once

#include "PlatformHeaders_DX12.h"
#include "rhi/IDescriptorHeap.h"

namespace NSRHIDX12
{
    // D3D12 storage for IDescriptorHeap. This is NSDescriptor::IDescriptor
    // from DXTerrain, moved below the RHI seam and with the allocation
    // policy taken out — StaticHeap/RingHeap are index arithmetic and now
    // live in the front-end, written once for both backends.
    //
    // The cpuStart/gpuStart/descriptorSize triple stays PRIVATE. The
    // neutral DescriptorOffset carries only an index, and the handle pair
    // is rebuilt here on demand (CpuHandle/GpuHandle below) exactly as
    // IDescriptor::At did. No D3D12 address ever crosses the seam.
    class DX12DescriptorHeap final : public NSRHI::IDescriptorHeap
    {
    public:
        DX12DescriptorHeap(ID3D12Device14* device, const NSRHI::DescriptorHeapDesc& desc);
        ~DX12DescriptorHeap() override;

        DX12DescriptorHeap(const DX12DescriptorHeap&) = delete;
        DX12DescriptorHeap& operator=(const DX12DescriptorHeap&) = delete;

        void Reset() override;

        // Backend-internal. The device writes descriptors at CpuHandle();
        // the command list binds the heap and roots tables at GpuHandle().
        D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle(uint32_t index) const;
        D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle(uint32_t index) const;

        ID3D12DescriptorHeap* Raw() const { return m_heap.Get(); }
        bool IsShaderVisible() const { return m_shaderVisible; }

    private:
        ComPtr<ID3D12DescriptorHeap> m_heap;
        D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStart{};
        D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStart{};
        uint32_t m_descriptorSize{};
        bool m_shaderVisible{ false };
    };
}
