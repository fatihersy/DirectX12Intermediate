#pragma once

struct GRAPHICS_PIPELINE_STATE_DESC
{
    D3D12_STREAM_OUTPUT_DESC StreamOutput;
    UINT SampleMask;
    D3D12_INPUT_LAYOUT_DESC InputLayout;
    D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue;
    D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType;
    UINT NumRenderTargets;
    DXGI_FORMAT RTVFormats[8];
    DXGI_FORMAT DSVFormat;
    DXGI_SAMPLE_DESC SampleDesc;
    UINT NodeMask;
    D3D12_CACHED_PIPELINE_STATE CachedPSO;
    D3D12_PIPELINE_STATE_FLAGS Flags;
};

using FnSetRootSignature = std::function<HRESULT(D3D_ROOT_SIGNATURE_VERSION version, ComPtr<ID3D10Blob>& signature, ComPtr<ID3D10Blob>& error)>;

void MakeRootSignature(ID3D12Device14* device, LPCWSTR name, ComPtr<ID3D12RootSignature>& outSignature, FnSetRootSignature SetRootSignature);

class IPipeline
{
public:
    IPipeline() {};
    IPipeline(ID3D12Device14* device, LPCWSTR name, ComPtr<ID3D12RootSignature>& rootSignature);
    IPipeline(ID3D12Device14* device, LPCWSTR name, FnSetRootSignature SetRootSignature);
    ~IPipeline();

    void Reset()
    {
        im_pipeline.Reset();
        im_rootSignature.Reset();
    }

protected:
    ID3D12Device14* im_device = nullptr;
    ComPtr<ID3D12PipelineState> im_pipeline;
    ComPtr<ID3D12RootSignature> im_rootSignature;
    std::wstring im_name;
};

class GraphicsPipeline : public IPipeline
{
public:
    GraphicsPipeline(){};
    GraphicsPipeline(ID3D12Device14* device, LPCWSTR name, ComPtr<ID3D12RootSignature>& rootSignature);
    GraphicsPipeline(ID3D12Device14* device, LPCWSTR name, FnSetRootSignature SetRootSignature);

    void Bind(NSRenderer::GraphicsCommandList cmdList) const
    {
        assert(im_pipeline);

        cmdList.SetPipelineState(im_pipeline.Get());
        cmdList.SetGraphicsRootSignature(im_rootSignature.Get());
    }

    GraphicsPipeline&& Init(
        GRAPHICS_PIPELINE_STATE_DESC inDesc,
        CD3DX12_RASTERIZER_DESC raster,
        CD3DX12_DEPTH_STENCIL_DESC ds,
        CD3DX12_BLEND_DESC blend,
        LPCWSTR vertexShaderFileName,
        std::vector<LPCWSTR> vertexShaderArgs,
        LPCWSTR indexShaderFileName,
        std::vector<LPCWSTR> indexShaderArgs
    );
};

class ComputePipeline : public IPipeline
{
public:
    ComputePipeline() {};
    ComputePipeline(ID3D12Device14* device, LPCWSTR name, ComPtr<ID3D12RootSignature>& rootSignature);
    ComputePipeline(ID3D12Device14* device, LPCWSTR name, FnSetRootSignature SetRootSignature);

    void Bind(NSRenderer::GraphicsCommandList cmdList) const
    {
        assert(im_pipeline);

        cmdList.SetPipelineState(im_pipeline.Get());
        cmdList.SetComputeRootSignature(im_rootSignature.Get());
    }

    ComputePipeline&& Init(
        D3D12_PIPELINE_STATE_FLAGS flags,
        LPCWSTR shaderFileName,
        std::vector<LPCWSTR> shaderArgs
    );
};
