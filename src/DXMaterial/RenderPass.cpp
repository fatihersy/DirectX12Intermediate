#include "stdafx.h"
#include "RenderPass.h"
#include "Scene.h"

#include "DXSampleHelper.h"
#include "Model.h"
#include "Tool.h"
#include "Renderer.h"
#include "Pipeline.h"

using namespace RenderPass;

GeometryPass::GeometryPass(ID3D12Device14* device, NSRenderer::Ctx rendererCtx)
    : m_pipeline(GraphicsPipeline(device, L"GeometryPass::Graphics", [](D3D_ROOT_SIGNATURE_VERSION version, ComPtr<ID3D10Blob>& signature, ComPtr<ID3D10Blob>& error)
    {
        CD3DX12_DESCRIPTOR_RANGE1 srvRange[1]{};
        srvRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, static_cast<INT>(FTextureType::FTextureType_MAX), 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

        CD3DX12_ROOT_PARAMETER1 rp[3]{};
        rp[0].InitAsConstantBufferView(0, 0);
        rp[1].InitAsConstantBufferView(1, 0);
        rp[2].InitAsDescriptorTable(1, &srvRange[0], D3D12_SHADER_VISIBILITY_PIXEL);

        D3D12_STATIC_SAMPLER_DESC sampler{};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.MipLODBias = 0;
        sampler.MaxAnisotropy = 0;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        sampler.MinLOD = 0.f;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc;
        desc.Init_1_1(_countof(rp), rp, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        D3DX12SerializeVersionedRootSignature(&desc, version, &signature, &error);
    }))
{
    // Pipeline
    {
        D3D12_INPUT_ELEMENT_DESC inputElements[] = {
            {"POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, position),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TANGENT",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, tangent), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, bitangent), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(Vertex, texCoord), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };

        D3D12_RENDER_TARGET_BLEND_DESC rtBlendDesc{};
        rtBlendDesc.BlendEnable = TRUE;
        rtBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        rtBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        rtBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
        rtBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
        rtBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
        rtBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        rtBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        D3D12_BLEND_DESC blendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        blendDesc.RenderTarget[0] = rtBlendDesc;

        GRAPHICS_PIPELINE_STATE_DESC desc{};
        desc.InputLayout = { inputElements, _countof(inputElements) };
        desc.NumRenderTargets = 1;
        desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        desc.SampleDesc.Count = 1;
        desc.SampleMask = UINT_MAX;
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        m_pipeline.Init(
            desc,
            CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
            CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
            blendDesc,
            L"GeometryPassVS.hlsl",
            {
                L"-E", L"mainVS",
                L"-T", L"vs_6_0",
                L"-Zi",
                L"-Od"
            },
            L"GeometryPassPS.hlsl",
            {
                L"-E", L"mainPS",
                L"-T", L"ps_6_0",
                L"-Zi",
                L"-Od"
            }
        );
    }
}

GeometryPass::~GeometryPass(){
    
}

void GeometryPass::Execute(Scene& scene, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
{
    if (not IsEnabled()) return;

    m_pipeline.Bind(cmdList);

    // Per-frame constants
    Allocator::AllocCtx frameCBAC = rendererCtx.allocConstBuff(sizeof(frameConstants));
    {
        using namespace DirectX;

        frameConstants& frameCB = frameCBAC.As<frameConstants>();

        XMStoreFloat4x4(&frameCB.viewMatrix, scene.m_camera.viewMatrix);
        XMStoreFloat4x4(&frameCB.projectionMatrix, scene.m_camera.projectionMatrix);
        XMStoreFloat3(&frameCB.camPos, scene.m_camera.camEye);
        XMStoreFloat4(&frameCB.lightDir, scene.m_lightDir);
        XMStoreFloat4(&frameCB.lightColor, scene.m_lightColor);
    }
    cmdList.SetGraphicsRootConstantBufferView(0, frameCBAC.gpuAddr);

    UINT width = IApp::GetInstance()->im_width;
    UINT height = IApp::GetInstance()->im_height;
    CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.f, 0.f, static_cast<FLOAT>(width), static_cast<FLOAT>(height));
    RECT scissor = CD3DX12_RECT(0L, 0L, static_cast<LONG>(width), static_cast<LONG>(height));

    cmdList.RSSetViewports(1, &viewport);
    cmdList.RSSetScissorRects(1, &scissor);

    for (size_t i = 1u; i < scene.m_models.size(); i++)
    {
        scene.m_models[i].Draw(rendererCtx, cmdList);
    }
}
void GeometryPass::OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx rendererCtx)
{}

SkyDomePass::SkyDomePass(ID3D12Device14* device, NSRenderer::Ctx rendererCtx)
{
    // Sky Dome Pipeline Root
    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE rsData{};
        rsData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rsData, sizeof(rsData)))) {
            rsData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        CD3DX12_DESCRIPTOR_RANGE1 uavRange{};
        uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

        CD3DX12_DESCRIPTOR_RANGE1 srvRange{};
        srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

        CD3DX12_ROOT_PARAMETER1 rp[5]{};
        rp[0].InitAsConstantBufferView(0, 0); // Frame
        rp[1].InitAsConstantBufferView(1, 0); // Mesh
        rp[2].InitAsConstantBufferView(2, 0); // Sky dome constants
        rp[3].InitAsDescriptorTable(1, &uavRange);
        rp[4].InitAsDescriptorTable(1, &srvRange);

        D3D12_STATIC_SAMPLER_DESC sampler{};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rsDesc;
        rsDesc.Init_1_1(_countof(rp), rp, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3D10Blob> signature;
        ComPtr<ID3D10Blob> error;
        HRESULT hr = D3DX12SerializeVersionedRootSignature(&rsDesc, rsData.HighestVersion, &signature, &error);
        if (FAILED(hr)) {
            if (error) {
                const char* errorMsg = reinterpret_cast<const char*>(error->GetBufferPointer());
                g_FError(errorMsg);
            }
            throw std::runtime_error("Failed to serialize sky dome root signature");
        }
        ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
        m_rootSignature->SetName(L"SkyDomePass::m_rootSignature");
    }

    this->m_transmittance = ComputePipeline(device, L"SkyDomePass::m_transmittance", m_rootSignature.Get()).Init(
        D3D12_PIPELINE_STATE_FLAG_NONE,
        L"SkyDome.hlsl",
        {
            L"-E", L"CS_Transmittance",
            L"-T", L"cs_6_0",
            L"-Zi",
            L"-Od",
            L"-D", L"COMPUTE_SHADER=1"
        }
    );

    this->m_scattering = ComputePipeline(device, L"SkyDomePass::m_scattering", m_rootSignature.Get()).Init(
        D3D12_PIPELINE_STATE_FLAG_NONE,
        L"SkyDome.hlsl",
        {
            L"-E", L"CS_Scattering",
            L"-T", L"cs_6_0",
            L"-Zi",
            L"-Od",
            L"-D", L"COMPUTE_SHADER=1"
        }
    );

    {
        D3D12_INPUT_ELEMENT_DESC inputElements[] = {
            {"POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, position),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, normal),    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TANGENT",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, tangent),   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, bitangent), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(Vertex, texCoord),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };

        CD3DX12_RASTERIZER_DESC rasterDesc(D3D12_DEFAULT);
        rasterDesc.CullMode = D3D12_CULL_MODE_FRONT;

        CD3DX12_DEPTH_STENCIL_DESC dsDesc(D3D12_DEFAULT);
        dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

        CD3DX12_BLEND_DESC blendDesc(D3D12_DEFAULT);

        GRAPHICS_PIPELINE_STATE_DESC desc{};
        desc.InputLayout = { inputElements, _countof(inputElements) };
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        desc.NumRenderTargets = 1;
        desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        desc.SampleDesc.Count = 1;
        desc.SampleMask = UINT_MAX;
        this->m_graphics = GraphicsPipeline(device, L"SkyDomePass::m_graphics", m_rootSignature.Get()).Init(
            desc,
            rasterDesc, dsDesc, blendDesc,
            L"SkyDome.hlsl",
            {
                L"-E", L"VS_Sky",
                L"-T", L"vs_6_0",
                L"-Zi",
                L"-Od"
            },
            L"SkyDome.hlsl",
            {
                L"-E", L"PS_Sky",
                L"-T", L"ps_6_0",
                L"-Zi",
                L"-Od"
            }
        );
    }

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = 256;
    desc.Height = 64;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
    device->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&m_trasmittanceLUT)
    );
    m_trasmittanceLUT->SetName(L"TransmittenceLUT");

    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    desc.Width = 32;
    desc.Height = 128;
    desc.DepthOrArraySize = 32 * 8;
    device->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&m_scatteringLUT)
    );
    m_scatteringLUT->SetName(L"ScatteringLUT");

    m_srvHandle = rendererCtx.allocSRVStatic(4u);

    device->CreateUnorderedAccessView(
        m_trasmittanceLUT.Get(),
        nullptr,
        nullptr,
        rendererCtx.offsetSRV(m_srvHandle, IDX_UAV_TRANSMITTANCE).cpuAddr
    );
    device->CreateUnorderedAccessView(
        m_scatteringLUT.Get(),
        nullptr,
        nullptr,
        rendererCtx.offsetSRV(m_srvHandle, IDX_UAV_SCATTERING).cpuAddr
    );
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
        desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        desc.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(
            m_trasmittanceLUT.Get(),
            &desc,
            rendererCtx.offsetSRV(m_srvHandle, IDX_SRV_TRANSMITTANCE).cpuAddr
        );
    }
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
        desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        desc.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(
            m_scatteringLUT.Get(),
            &desc,
            rendererCtx.offsetSRV(m_srvHandle, IDX_SRV_SCATTERING).cpuAddr
        );
    }

    {
        m_timeOfDayDefault = 12.f;
        float hourAngle = (m_timeOfDayDefault / 24.f) * DirectX::XM_2PI - DirectX::XM_PIDIV2;

        DirectX::XMFLOAT3 sunDir;
        sunDir.x = cos(hourAngle);
        sunDir.y = sin(hourAngle);
        sunDir.z = 0.f;

        DirectX::XMVECTOR vSunDir = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&sunDir));

        m_constantsUpload.BetaR = { 5.802e-6f, 13.558e-6f, 33.1e-6f };
        m_constantsUpload.BetaMScatter = 3.996e-6f;
        m_constantsUpload.BetaMExtinct = 3.996e-6f / 0.9f;
        m_constantsUpload.MieG = 0.8f;
        m_constantsUpload.HR = 8000.0f;
        m_constantsUpload.HM = 1200.0f;
        m_constantsUpload.Rg = 6360000.0f;
        m_constantsUpload.Rt = 6420000.0f;
        m_constantsUpload.SunIntensity = 20.0f;
        DirectX::XMStoreFloat3(&m_constantsUpload.SunDir, vSunDir);
    }
}

SkyDomePass::~SkyDomePass()
{
}

void SkyDomePass::Execute(Scene& scene, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
{
    if (not IsEnabled()) return;


    UINT width = IApp::GetInstance()->im_width;
    UINT height = IApp::GetInstance()->im_height;
    CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.f, 0.f, static_cast<FLOAT>(width), static_cast<FLOAT>(height));
    RECT scissor = CD3DX12_RECT(0L, 0L, static_cast<LONG>(width), static_cast<LONG>(height));

    cmdList.RSSetViewports(1, &viewport);
    cmdList.RSSetScissorRects(1, &scissor);

    if (m_constantsUpload != m_constantsDefault or scene.m_timeOfDay != m_timeOfDayDefault)
    {
        float hourAngle = (scene.m_timeOfDay / 24.f) * DirectX::XM_2PI - DirectX::XM_PIDIV2;

        DirectX::XMFLOAT3 sunDir;
        sunDir.x = cos(hourAngle);
        sunDir.y = sin(hourAngle);
        sunDir.z = 0.f;
        DirectX::XMVECTOR vSunDir = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&sunDir));

        scene.m_lightDir = DirectX::XMVectorNegate(vSunDir);
        DirectX::XMStoreFloat3(&m_constantsUpload.SunDir, vSunDir);

        m_constantsDefault = m_constantsUpload;
        m_timeOfDayDefault = scene.m_timeOfDay;

        UpdateSkyDome(rendererCtx, cmdList);
    }

    Allocator::AllocCtx skyDomeCBAC = rendererCtx.allocConstBuff(sizeof(skyDomeConstants));
    skyDomeConstants& skyDomeCB = skyDomeCBAC.As<skyDomeConstants>();
    skyDomeCB = m_constantsDefault;

    Allocator::AllocCtx frameCBAC = rendererCtx.allocConstBuff(sizeof(frameConstants));
    {
        using namespace DirectX;

        frameConstants& frameCB = frameCBAC.As<frameConstants>();

        XMStoreFloat4x4(&frameCB.viewMatrix, scene.m_camera.viewMatrix);
        XMStoreFloat4x4(&frameCB.projectionMatrix, scene.m_camera.projectionMatrix);
        XMStoreFloat3(&frameCB.camPos, scene.m_camera.camEye);
        XMStoreFloat4(&frameCB.lightDir, scene.m_lightDir);
        XMStoreFloat4(&frameCB.lightColor, scene.m_lightColor);
    }

    m_graphics.Bind(cmdList);
    cmdList.SetGraphicsRootConstantBufferView(IDX_CBV_FRAME, frameCBAC.gpuAddr);
    cmdList.SetGraphicsRootConstantBufferView(IDX_CBV_SKYDOME, skyDomeCBAC.gpuAddr);
    cmdList.SetGraphicsRootDescriptorTable(IDX_ROOT_DESC_TABLE_SRV, rendererCtx.offsetSRV(m_srvHandle, IDX_SRV_TRANSMITTANCE).gpuAddr);

    scene.m_models[0].Draw([this, &rendererCtx, &cmdList](Mesh& mesh, UINT meshIndex, DirectX::XMMATRIX worldMatrix)
    {
        Allocator::AllocCtx allocCtx = rendererCtx.allocConstBuff(sizeof(meshConstants));
        meshConstants& meshCB = allocCtx.As<meshConstants>();
        cmdList.SetGraphicsRootConstantBufferView(IDX_CBV_MESH, allocCtx.gpuAddr);

        DirectX::XMStoreFloat4x4(&meshCB.worldMatrix, worldMatrix);
        DirectX::XMVECTOR det;
        DirectX::XMMATRIX worldInverse = DirectX::XMMatrixInverse(&det, worldMatrix);
        DirectX::XMStoreFloat3x4(&meshCB.normalMatrix, worldInverse);

        meshCB.baseColor = mesh.material.m_baseColor;
        meshCB.metallic = mesh.material.m_metallic;
        meshCB.roughness = mesh.material.m_roughness;
        meshCB.opacity = mesh.material.m_opacity;
        meshCB.textureFlags = mesh.material.m_textureFlags;

        cmdList.IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
        cmdList.IASetIndexBuffer(&mesh.indexBufferView);
        cmdList.DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
    });
}

void SkyDomePass::UpdateSkyDome(NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
{
    {
        CD3DX12_RESOURCE_BARRIER barriers[] = {
            CD3DX12_RESOURCE_BARRIER::Transition(
                m_trasmittanceLUT.Get(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS
            ),
            CD3DX12_RESOURCE_BARRIER::Transition(
                m_scatteringLUT.Get(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS
            )
        };
        cmdList.ResourceBarrier(_countof(barriers), barriers);
    }

    Allocator::AllocCtx constantsAC = rendererCtx.allocConstBuff(sizeof(skyDomeConstants));
    skyDomeConstants& cb = constantsAC.As<skyDomeConstants>();
    cb = m_constantsDefault;

    m_transmittance.Bind(cmdList);
    cmdList.SetComputeRootConstantBufferView(IDX_CBV_SKYDOME, constantsAC.gpuAddr);
    cmdList.SetComputeRootDescriptorTable(IDX_ROOT_DESC_TABLE_UAV, rendererCtx.offsetSRV(m_srvHandle, IDX_UAV_TRANSMITTANCE).gpuAddr);
    cmdList.Dispatch(
        (256 + 7) / 8,
        (64 + 7) / 8,
        1
    );

    // Transition transmittance UAV -> SRV so scattering compute can read it
    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_trasmittanceLUT.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
        );
        cmdList.ResourceBarrier(1, &barrier);
    }

    m_scattering.Bind(cmdList);
    cmdList.SetComputeRootConstantBufferView(IDX_CBV_SKYDOME, constantsAC.gpuAddr);
    cmdList.SetComputeRootDescriptorTable(IDX_ROOT_DESC_TABLE_UAV, rendererCtx.offsetSRV(m_srvHandle, IDX_UAV_SCATTERING).gpuAddr);
    cmdList.SetComputeRootDescriptorTable(IDX_ROOT_DESC_TABLE_SRV, rendererCtx.offsetSRV(m_srvHandle, IDX_SRV_TRANSMITTANCE).gpuAddr);
    cmdList.Dispatch(
        (32 + 3) / 4,
        (128 + 3) / 4,
        (256 + 3) / 4
    );

    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_scatteringLUT.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
        );
        cmdList.ResourceBarrier(1, &barrier);
    }
}

void SkyDomePass::OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx rendererCtx)
{

}

