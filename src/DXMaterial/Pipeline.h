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

using FnSetFootSignature = std::function<void(CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC& desc)>;

class ShaderCompiler
{
public:
    ShaderCompiler();
    ~ShaderCompiler();

    ComPtr<IDxcCompiler3> m_dxcCompiler;
    ComPtr<IDxcLibrary> m_dxcLibrary;
    ComPtr<IDxcUtils> m_dxcUtils;
    ComPtr<IDxcValidator2> m_dxcValidator;
    ComPtr<IDxcIncludeHandler> m_dxcIncludeHandler;

    static ShaderCompiler* GetInstance() {
        assert(s_instance != nullptr);
        return s_instance;
    }

    void CompileShader(IDxcBlobEncoding* sourceBlob, ComPtr<IDxcBlob>& shader, std::vector<LPCWSTR>& args);
private:
    static ShaderCompiler* s_instance;
};

class Pipeline
{
public:
    Pipeline(){};
    Pipeline(ID3D12Device* device, LPCWSTR pipelineName, ID3D12RootSignature* rootSignature);
    Pipeline(ID3D12Device* device, LPCWSTR pipelineName, FnSetFootSignature SetRootSignature);
    ~Pipeline();

protected:
    ID3D12Device* im_device = nullptr;
    ComPtr<ID3D12PipelineState> im_pipeline;
    ID3D12RootSignature* im_rootSignature = nullptr;
    std::wstring im_name;

private:
    std::variant<ID3D12RootSignature*, ComPtr<ID3D12RootSignature>> m_rsVar;
};

class GraphicsPipeline : public Pipeline
{
public:
    GraphicsPipeline(){};
    GraphicsPipeline(ID3D12Device* device, LPCWSTR pipelineName, ID3D12RootSignature* rootSignature);
    GraphicsPipeline(ID3D12Device* device, LPCWSTR pipelineName, FnSetFootSignature setRootSignature);

    inline void Bind(ID3D12GraphicsCommandList10* cmdList) const {
        if (im_pipeline and im_rootSignature)
        {
            cmdList->SetPipelineState(im_pipeline.Get());
            cmdList->SetGraphicsRootSignature(im_rootSignature);
            return;
        }
        throw std::runtime_error("Invalid use");
    }

    GraphicsPipeline&& Init(
        GRAPHICS_PIPELINE_STATE_DESC desc,
        D3D12_RASTERIZER_DESC raster,
        D3D12_DEPTH_STENCIL_DESC ds,
        D3D12_BLEND_DESC blend,
        LPCWSTR vertexShaderFileName,
        std::vector<LPCWSTR> vertexShaderArgs,
        LPCWSTR indexShaderFileName,
        std::vector<LPCWSTR> indexShaderArgs
    );
};

class ComputePipeline : public Pipeline
{
public:
    ComputePipeline(){};
    ComputePipeline(ID3D12Device* device, LPCWSTR pipelineName, ID3D12RootSignature* rootSignature);
    ComputePipeline(ID3D12Device* device, LPCWSTR pipelineName, FnSetFootSignature SetRootSignature);

    inline void BindPipe(ID3D12GraphicsCommandList10* cmdList) const
    {
        if (im_pipeline)
        {
            cmdList->SetPipelineState(im_pipeline.Get());
            return;
        }
        throw std::runtime_error("Invalid use");
    }
    inline void BindRoot(ID3D12GraphicsCommandList10* cmdList) const
    {
        if (im_rootSignature)
        {
            cmdList->SetComputeRootSignature(im_rootSignature);
            return;
        }
        throw std::runtime_error("Invalid use");
    }

    ComputePipeline&& Init(
        D3D12_PIPELINE_STATE_FLAGS flags,
        LPCWSTR shaderFileName,
        std::vector<LPCWSTR> shaderArgs
    );
};
