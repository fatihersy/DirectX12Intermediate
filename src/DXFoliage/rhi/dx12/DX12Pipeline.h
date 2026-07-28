#pragma once

#include "PlatformHeaders_DX12.h"
#include "DirectXTypes.h"
#include "rhi/IPipeline.h"

// DX12 implementation of IPipeline. Builds the PSO (compiling its shaders
// from source, see DX12Pipeline.cpp) and binds it. Bind() sets the PSO,
// root signature, AND the primitive topology — topology lives with the
// pipeline here because that's where Vulkan puts it (baked into the
// VkPipeline), so the neutral ICommandList never carries a topology arg.
// Bind() is called by DX12CommandList::SetPipeline.
namespace NSRHIDX12
{
    class DX12Pipeline final : public NSRHI::IPipeline
    {
    public:
        DX12Pipeline(ID3D12Device14* device, const NSRHI::GraphicsPipelineDesc& desc);
        ~DX12Pipeline() override = default;

        void Bind(NSDX12::GraphicsCommandList cmdList) const;

    private:
        ComPtr<ID3D12PipelineState> m_pipeline;
        ID3D12RootSignature* m_rootSignature{ nullptr }; // non-owning; owned by desc.layout
        D3D12_PRIMITIVE_TOPOLOGY m_topology{ D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
    };
}
