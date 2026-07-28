#include "stdafx.h"
#include "DX12PipelineLayout.h"

#include "DXSampleHelper.h"
#include "Logger.h"

namespace NSRHIDX12
{
    namespace
    {
        using FnSetRootSignature = std::function<HRESULT(D3D_ROOT_SIGNATURE_VERSION version, ComPtr<ID3D10Blob>& signature, ComPtr<ID3D10Blob>& error)>;

        // Moved verbatim from the old Pipeline.cpp, which is gone — its
        // GraphicsPipeline/ComputePipeline/TessellationPipeline classes
        // were superseded by DX12Pipeline, and this helper was the only
        // part still in use.
        void MakeRootSignature(ID3D12Device14* device, LPCWSTR name, ComPtr<ID3D12RootSignature>& outSignature, FnSetRootSignature SetRootSignature)
        {
            D3D12_FEATURE_DATA_ROOT_SIGNATURE featureVersion{};
            featureVersion.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

            if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureVersion, sizeof(featureVersion))))
            {
                featureVersion.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
            }

            ComPtr<ID3D10Blob> signature;
            ComPtr<ID3D10Blob> error;

            if (FAILED(SetRootSignature(featureVersion.HighestVersion, signature, error)))
            {
                if (error and error->GetBufferSize() > 0)
                {
                    g_FError(std::string(reinterpret_cast<const char*>(error->GetBufferPointer()), error->GetBufferSize()));
                }
                PANIC("Unable to set root signature");
            }

            ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&outSignature)));
            outSignature->SetName(name);
        }
    }

    DX12PipelineLayout::DX12PipelineLayout(ID3D12Device14* device, const NSRHI::PipelineLayoutDesc& desc)
    {
        MakeRootSignature(device, L"DX12PipelineLayout::m_rootSignature", m_rootSignature,
            [&desc](D3D_ROOT_SIGNATURE_VERSION version, ComPtr<ID3D10Blob>& signature, ComPtr<ID3D10Blob>& error) -> HRESULT
            {
                std::vector<CD3DX12_ROOT_PARAMETER1> rootParams;
                std::vector<CD3DX12_DESCRIPTOR_RANGE1> ranges;

                if (desc.num32BitRootConstants > 0)
                {
                    rootParams.emplace_back();
                    rootParams.back().InitAsConstants(desc.num32BitRootConstants, 0, 0);
                }

                if (desc.usesBindlessDescriptorTable)
                {
                    // One big descriptor-indexing-style SRV table — see
                    // rhi/IDescriptorHeap.h for why this mirrors the
                    // existing NSDescriptor::StaticHeap/RingHeap pattern.
                    ranges.emplace_back();
                    ranges.back().Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UINT_MAX, 0, 1, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

                    rootParams.emplace_back();
                    rootParams.back().InitAsDescriptorTable(1, &ranges.back());
                }

                CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rsDesc{};
                rsDesc.Init_1_1(
                    static_cast<UINT>(rootParams.size()), rootParams.empty() ? nullptr : rootParams.data(),
                    0, nullptr,
                    D3D12_ROOT_SIGNATURE_FLAGS::D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
                );

                return D3DX12SerializeVersionedRootSignature(&rsDesc, version, &signature, &error);
            }
        );
    }
}
