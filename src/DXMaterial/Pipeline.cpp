#include "stdafx.h"
#include "Pipeline.h"

#include "DXSampleHelper.h"
#include "IApp.h"
#include "Tool.h"

#include "ShaderCompiler.h"

void MakeRootSignature(ID3D12Device* device, LPCWSTR rootName, ComPtr<ID3D12RootSignature>& outSignature, FnSetRootSignature SetRootSignature)
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
        if (not error)
        {
            const char * errorMsg = reinterpret_cast<const char*>(error->GetBufferPointer());
            g_FError(errorMsg);
        }
        throw std::runtime_error("Failed to serialize root signature");
    }
    ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&outSignature)));
    outSignature->SetName(rootName);
}

Pipeline::Pipeline(ID3D12Device* device, LPCWSTR pipelineName, ID3D12RootSignature* rootSignature) : im_device(device), im_name(pipelineName)
{
    im_rootSignature = rootSignature;
    m_rsVar = rootSignature;
}
Pipeline::Pipeline(ID3D12Device* device, LPCWSTR pipelineName, FnSetRootSignature SetRootSignature) : im_device(device), im_name(pipelineName)
{
    ComPtr<ID3D12RootSignature> rootSig;

    MakeRootSignature(
        device,
        FString::wformat(L"%s::%s", std::wstring(pipelineName), L"RootSignature").c_str(),
        rootSig,
        SetRootSignature
    );

    m_rsVar = std::move(rootSig);
    im_rootSignature = std::get<ComPtr<ID3D12RootSignature>>(m_rsVar).Get();
}
Pipeline::~Pipeline()
{
    this->im_device = nullptr;
    this->im_rootSignature = nullptr;
    this->im_pipeline.Reset();

    //std::visit([](auto&& rs) {
    //    using T = std::decay_t<decltype(rs)>;
    //
    //    if constexpr (std::is_same_v<T, ID3D12RootSignature*>) {
    //        rs = nullptr;
    //    }
    //    else rs.Reset();
    //    
    //}, m_rsVar);
}

ComputePipeline::ComputePipeline(ID3D12Device* device, LPCWSTR pipelineName, ID3D12RootSignature* rootSignature) : Pipeline(device, pipelineName, rootSignature) {};
ComputePipeline::ComputePipeline(ID3D12Device* device, LPCWSTR pipelineName, FnSetRootSignature SetRootSignature)
    : Pipeline(device, pipelineName, SetRootSignature)
{

}

GraphicsPipeline::GraphicsPipeline(ID3D12Device* device, LPCWSTR pipelineName, ID3D12RootSignature* rootSignature) : Pipeline(device, pipelineName, rootSignature) {};
GraphicsPipeline::GraphicsPipeline(ID3D12Device* device, LPCWSTR pipelineName, FnSetRootSignature SetRootSignature)
    : Pipeline(device, pipelineName, SetRootSignature)
{

}

ComputePipeline&& ComputePipeline::Init(
    D3D12_PIPELINE_STATE_FLAGS flags,
    LPCWSTR shaderFileName,
    std::vector<LPCWSTR> shaderArgs
)
{
    ComPtr<IDxcBlob> shader;
    {
        ComPtr<IDxcBlobEncoding> shaderSource;
        ThrowIfFailed(ShaderCompiler::GetInstance()->m_dxcUtils->LoadFile(shaderFileName, nullptr, &shaderSource));
        ShaderCompiler::GetInstance()->CompileShader(shaderSource.Get(), shader, shaderArgs);
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = this->im_rootSignature;
    desc.CS = CD3DX12_SHADER_BYTECODE(shader->GetBufferPointer(), shader->GetBufferSize());
    desc.Flags = flags;

    ThrowIfFailed(im_device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&im_pipeline)));
    im_pipeline->SetName(this->im_name.c_str());

    return std::move(*this);
}

GraphicsPipeline&& GraphicsPipeline::Init(
    GRAPHICS_PIPELINE_STATE_DESC desc,
    D3D12_RASTERIZER_DESC raster,
    D3D12_DEPTH_STENCIL_DESC ds,
    D3D12_BLEND_DESC blend,
    LPCWSTR vertexShaderFileName,
    std::vector<LPCWSTR> vertexShaderArgs,
    LPCWSTR indexShaderFileName,
    std::vector<LPCWSTR> indexShaderArgs
) {
    ComPtr<IDxcBlob> vertexShader;
    ComPtr<IDxcBlob> pixelShader;

    {
        ComPtr<IDxcBlobEncoding> shaderSource;
        ThrowIfFailed(ShaderCompiler::GetInstance()->m_dxcUtils->LoadFile(vertexShaderFileName, nullptr, &shaderSource));
        ShaderCompiler::GetInstance()->CompileShader(shaderSource.Get(), vertexShader, vertexShaderArgs);
    }
    {
        ComPtr<IDxcBlobEncoding> shaderSource;
        ThrowIfFailed(ShaderCompiler::GetInstance()->m_dxcUtils->LoadFile(indexShaderFileName, nullptr, &shaderSource));
        ShaderCompiler::GetInstance()->CompileShader(shaderSource.Get(), pixelShader, indexShaderArgs);
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC d3d12Desc{};
    d3d12Desc.pRootSignature = this->im_rootSignature;
    d3d12Desc.VS = CD3DX12_SHADER_BYTECODE(vertexShader->GetBufferPointer(), vertexShader->GetBufferSize());
    d3d12Desc.PS = CD3DX12_SHADER_BYTECODE(pixelShader->GetBufferPointer(), pixelShader->GetBufferSize());
    d3d12Desc.RasterizerState = raster;
    d3d12Desc.DepthStencilState = ds;
    d3d12Desc.BlendState = blend;

    d3d12Desc.StreamOutput = desc.StreamOutput;
    d3d12Desc.SampleMask = desc.SampleMask;
    d3d12Desc.InputLayout = desc.InputLayout;
    d3d12Desc.IBStripCutValue = desc.IBStripCutValue;
    d3d12Desc.PrimitiveTopologyType = desc.PrimitiveTopologyType;
    d3d12Desc.NumRenderTargets = desc.NumRenderTargets;
    memcpy(d3d12Desc.RTVFormats, desc.RTVFormats, sizeof(DXGI_FORMAT) * 8u);
    d3d12Desc.DSVFormat = desc.DSVFormat;
    d3d12Desc.SampleDesc = desc.SampleDesc;
    d3d12Desc.NodeMask = desc.NodeMask;
    d3d12Desc.CachedPSO = desc.CachedPSO;
    d3d12Desc.Flags = desc.Flags;

    ThrowIfFailed(im_device->CreateGraphicsPipelineState(&d3d12Desc, IID_PPV_ARGS(&im_pipeline)));
    im_pipeline->SetName(this->im_name.c_str());

    return std::move(*this);
}
