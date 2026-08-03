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

        // Constant-slot plumbing for SetConstantBuffer. The base VA and
        // the first root-parameter index are fixed at creation (root
        // params are laid out [constants?][bindless table?][CBV slots]),
        // so binding slot N is one call:
        //   SetGraphicsRootConstantBufferView(base + N, baseVA + offset).
        D3D12_GPU_VIRTUAL_ADDRESS ConstantBaseVA() const { return m_constantBaseVA; }
        uint32_t ConstantRootParamBase() const { return m_constantRootParamBase; }
        uint32_t NumConstantSlots() const { return m_numConstantSlots; }

    private:
        ComPtr<ID3D12RootSignature> m_rootSignature;
        D3D12_GPU_VIRTUAL_ADDRESS m_constantBaseVA{};
        uint32_t m_constantRootParamBase{ 0 };
        uint32_t m_numConstantSlots{ 0 };
    };
}
