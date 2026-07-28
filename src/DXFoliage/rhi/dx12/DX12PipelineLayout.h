#pragma once

#include "PlatformHeaders_DX12.h"
#include "rhi/IPipelineLayout.h"

// DX12 implementation of IPipelineLayout — builds a root signature from
// the neutral num32BitRootConstants/usesBindlessDescriptorTable
// description. Reuses the existing, already-proven MakeRootSignature()
// free function (Pipeline.h/.cpp) for the actual feature-version-check +
// serialize + CreateRootSignature dance rather than re-deriving it.
namespace NSRHIDX12
{
    class DX12PipelineLayout final : public NSRHI::IPipelineLayout
    {
    public:
        DX12PipelineLayout(ID3D12Device14* device, const NSRHI::PipelineLayoutDesc& desc);
        ~DX12PipelineLayout() override = default;

        // Not part of IPipelineLayout — DX12Pipeline needs the raw root
        // signature to build its PSO and to bind it at draw time.
        ID3D12RootSignature* Raw() const { return m_rootSignature.Get(); }

    private:
        ComPtr<ID3D12RootSignature> m_rootSignature;
    };
}
