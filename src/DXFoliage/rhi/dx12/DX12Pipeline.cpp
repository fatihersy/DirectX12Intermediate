#include "stdafx.h"
#include "DX12Pipeline.h"

#include "DXSampleHelper.h"
#include "ShaderCompiler.h"

#include "DX12Format.h"
#include "DX12PipelineLayout.h"

namespace NSRHIDX12
{
    namespace
    {
        // The backend owns shader compilation: derive the DXIL profile
        // from the neutral stage and hand DXC the source. (The Vulkan
        // backend's equivalent will add -spirv — same seam, per the
        // dual-target task.)
        ComPtr<IDxcBlob> CompileFromSource(const NSRHI::ShaderSource& src)
        {
            LPCWSTR profile = (src.stage == NSRHI::EShaderStage::Pixel) ? L"ps_6_0" : L"vs_6_0";
            std::vector<LPCWSTR> args{ L"-E", src.entryPoint, L"-T", profile, L"-Zi", L"-Od" };

            ComPtr<IDxcBlob> blob;
            ShaderCompiler::GetInstance()->CompileShader(src.filename, blob, args);
            return blob;
        }
    }

    DX12Pipeline::DX12Pipeline(ID3D12Device14* device, const NSRHI::GraphicsPipelineDesc& desc)
    {
        ASSERT(desc.layout != nullptr, "GraphicsPipelineDesc needs a layout");

        auto* dx12Layout = static_cast<DX12PipelineLayout*>(desc.layout);
        m_rootSignature = dx12Layout->Raw();

        std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
        inputElements.reserve(desc.vertexAttributes.size());
        for (const NSRHI::VertexAttribute& attr : desc.vertexAttributes)
        {
            inputElements.push_back(D3D12_INPUT_ELEMENT_DESC{
                attr.semanticName, 0, ToDXGIFormat(attr.format), 0, attr.offsetBytes,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
            });
        }

        // Blobs must outlive CreateGraphicsPipelineState (the PSO desc
        // points into their memory) — kept alive as locals here.
        ComPtr<IDxcBlob> vsBlob = CompileFromSource(desc.vertexShader);
        ComPtr<IDxcBlob> psBlob;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
        psoDesc.pRootSignature = m_rootSignature;
        psoDesc.InputLayout = { inputElements.data(), static_cast<UINT>(inputElements.size()) };
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize());

        if (desc.pixelShader.IsValid())
        {
            psBlob = CompileFromSource(desc.pixelShader);
            psoDesc.PS = CD3DX12_SHADER_BYTECODE(psBlob->GetBufferPointer(), psBlob->GetBufferSize());
        }

        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

        // D3D12_DEFAULT is already "no blending", so Opaque needs no edits.
        // Only RenderTarget[0] is touched: NumRenderTargets never exceeds 1
        // here or in DXTerrain, and IndependentBlendEnable stays FALSE, so
        // slot 0 is the one that applies.
        CD3DX12_BLEND_DESC blendDesc(D3D12_DEFAULT);
        if (desc.blendMode == NSRHI::EBlendMode::AlphaBlend)
        {
            D3D12_RENDER_TARGET_BLEND_DESC& rt = blendDesc.RenderTarget[0];
            rt.BlendEnable = TRUE;
            rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
            rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
            rt.BlendOp = D3D12_BLEND_OP_ADD;
            rt.SrcBlendAlpha = D3D12_BLEND_ONE;
            rt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
            rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        }
        psoDesc.BlendState = blendDesc;

        CD3DX12_DEPTH_STENCIL_DESC depthDesc(D3D12_DEFAULT);
        depthDesc.DepthEnable = desc.depthTestEnabled;
        depthDesc.DepthWriteMask = desc.depthWriteEnabled ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        psoDesc.DepthStencilState = depthDesc;

        psoDesc.SampleMask = UINT_MAX;
        const bool isLineList = (desc.topology == NSRHI::EPrimitiveTopology::LineList);
        psoDesc.PrimitiveTopologyType = isLineList
            ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE
            : D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        m_topology = isLineList ? D3D_PRIMITIVE_TOPOLOGY_LINELIST : D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

        psoDesc.NumRenderTargets = static_cast<UINT>(desc.colorTargetFormats.size());
        for (size_t i = 0; i < desc.colorTargetFormats.size() && i < 8; ++i)
        {
            psoDesc.RTVFormats[i] = ToDXGIFormat(desc.colorTargetFormats[i]);
        }
        psoDesc.DSVFormat = ToDXGIFormat(desc.depthTargetFormat);
        psoDesc.SampleDesc.Count = 1;

        ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipeline)));
    }

    void DX12Pipeline::Bind(NSDX12::GraphicsCommandList cmdList) const
    {
        cmdList.SetPipelineState(m_pipeline.Get());
        cmdList.SetGraphicsRootSignature(m_rootSignature);
        cmdList.IASetPrimitiveTopology(m_topology);
    }
}
