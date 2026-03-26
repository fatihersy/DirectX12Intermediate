#include "stdafx.h"
#include "Pipeline.h"

#include "IApp.h"
#include "DXSampleHelper.h"

#include "ShaderCompiler.h"

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
        if (not error)
        {
            const char * errorMsg = reinterpret_cast<const char*>(error->GetBufferPointer());
            OutputDebugStringA(errorMsg);
        }
        throw std::runtime_error("Failed to serialize root signature");
    }

    ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&outSignature)));
    outSignature->SetName(name);
}

IPipeline::IPipeline(ID3D12Device14* device, LPCWSTR name, ComPtr<ID3D12RootSignature>& rootSignature)
{
    im_rootSignature = rootSignature;
    im_name = name;
}

IPipeline::IPipeline(ID3D12Device14* device, LPCWSTR name, FnSetRootSignature SetRootSignature)
{
    MakeRootSignature(
        device,
        KTool::wformat(L"%s::%s", name, L"im_rootSignature").c_str(),
        im_rootSignature,
        SetRootSignature
    );
    im_name = name;
}
IPipeline::~IPipeline()
{
    this->im_device = nullptr;
    this->im_rootSignature = nullptr;
    this->im_pipeline.Reset();
}

GraphicsPipeline::GraphicsPipeline(ID3D12Device14* device, LPCWSTR name, ComPtr<ID3D12RootSignature>& rootSignature) : IPipeline(device, name, rootSignature)
{}
GraphicsPipeline::GraphicsPipeline(ID3D12Device14* device, LPCWSTR name, FnSetRootSignature SetRootSignature) : IPipeline(device, name, SetRootSignature)
{}

GraphicsPipeline&& GraphicsPipeline::Init(
    GRAPHICS_PIPELINE_STATE_DESC inDesc,
    CD3DX12_RASTERIZER_DESC raster,
    CD3DX12_DEPTH_STENCIL_DESC ds,
    CD3DX12_BLEND_DESC blend,
    LPCWSTR vertexShaderFileName,
    std::vector<LPCWSTR> vertexShaderArgs,
    LPCWSTR indexShaderFileName,
    std::vector<LPCWSTR> indexShaderArgs
)
{
    ComPtr<IDxcBlob> vertexShader;
    ComPtr<IDxcBlob> indexShader;

    ShaderCompiler::GetInstance()->CompileShader(vertexShaderFileName, vertexShader, vertexShaderArgs);
    ShaderCompiler::GetInstance()->CompileShader(indexShaderFileName, indexShader, indexShaderArgs);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = im_rootSignature.Get();
    desc.VS = CD3DX12_SHADER_BYTECODE(vertexShader->GetBufferPointer(), vertexShader->GetBufferSize());
    desc.PS = CD3DX12_SHADER_BYTECODE(indexShader->GetBufferPointer(), indexShader->GetBufferSize());
    desc.RasterizerState = raster;
    desc.DepthStencilState = ds;
    desc.BlendState = blend;

    desc.StreamOutput = inDesc.StreamOutput;
    desc.SampleMask = inDesc.SampleMask;
    desc.InputLayout = inDesc.InputLayout;
    desc.IBStripCutValue = inDesc.IBStripCutValue;
    desc.PrimitiveTopologyType = inDesc.PrimitiveTopologyType;
    desc.NumRenderTargets = inDesc.NumRenderTargets;
    memcpy(desc.RTVFormats, inDesc.RTVFormats, sizeof(DXGI_FORMAT) * 8u);
    desc.DSVFormat = inDesc.DSVFormat;
    desc.SampleDesc = inDesc.SampleDesc;
    desc.NodeMask = inDesc.NodeMask;
    desc.CachedPSO = inDesc.CachedPSO;
    desc.Flags = inDesc.Flags;

    ThrowIfFailed(im_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&im_pipeline)));
    im_pipeline->SetName(KTool::wformat(L"%s::%s", im_name.data(), L"im_pipeline").c_str());

    return std::move(*this);
}

ComputePipeline::ComputePipeline(ID3D12Device14* device, LPCWSTR name, ComPtr<ID3D12RootSignature>& rootSignature) : IPipeline(device, name, rootSignature)
{}
ComputePipeline::ComputePipeline(ID3D12Device14* device, LPCWSTR name, FnSetRootSignature SetRootSignature) : IPipeline(device, name, SetRootSignature)
{}

ComputePipeline&& ComputePipeline::Init(D3D12_PIPELINE_STATE_FLAGS flags, LPCWSTR shaderFileName, std::vector<LPCWSTR> shaderArgs)
{
    ComPtr<IDxcBlob> shader;
    ShaderCompiler::GetInstance()->CompileShader(shaderFileName, shader, shaderArgs);

    D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = im_rootSignature.Get();
    desc.CS = CD3DX12_SHADER_BYTECODE(shader->GetBufferPointer(), shader->GetBufferSize());
    desc.Flags = flags;

    ThrowIfFailed(im_device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&im_pipeline)));
    im_pipeline->SetName(KTool::wformat(L"%s::%s", im_name.data(), L"im_pipeline").c_str());

    return std::move(*this);
}
