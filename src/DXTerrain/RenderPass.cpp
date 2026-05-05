#include "stdafx.h"
#include "RenderPass.h"

#include "IApp.h"
#include "DXSampleHelper.h"
#include "Blackboard.h"
#include "Scene.h"
#include "Terrain.h"

using namespace NSRenderPass;

GeometryPass::GeometryPass(ID3D12Device14* device, Blackboard& blackboard, NSRenderer::Ctx rendererCtx)
    : m_pipeline(GraphicsPipeline(device, L"GeometryPass::Graphics", [](D3D_ROOT_SIGNATURE_VERSION version, ComPtr<ID3D10Blob>& signature, ComPtr<ID3D10Blob>& error) -> HRESULT
    {
        constexpr UINT kModelTexCount = static_cast<UINT>(NSTexture::EType::EType_MAX);
        CD3DX12_DESCRIPTOR_RANGE1 srvRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, kModelTexCount, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        CD3DX12_DESCRIPTOR_RANGE1 envCubemapRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, kModelTexCount, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
        CD3DX12_DESCRIPTOR_RANGE1 brdfLUTRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, kModelTexCount + 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

        CD3DX12_ROOT_PARAMETER1 rp[5]{};
        rp[IDX_ROOT_CBV_FRAME].InitAsConstantBufferView(IDX_CBV_FRAME, 0);
        rp[IDX_ROOT_CBV_MESH].InitAsConstantBufferView(IDX_CBV_MESH, 0);
        rp[IDX_ROOT_DESC_MODEL_TEX_SRV].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);
        rp[IDX_ROOT_DESC_ENV_CUBEMAP_SRV].InitAsDescriptorTable(1, &envCubemapRange, D3D12_SHADER_VISIBILITY_PIXEL);
        rp[IDX_ROOT_DESC_ENV_BRDF_LUT_SRV].InitAsDescriptorTable(1, &brdfLUTRange, D3D12_SHADER_VISIBILITY_PIXEL);

        D3D12_STATIC_SAMPLER_DESC texSampler{};
        texSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        texSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        texSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        texSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        texSampler.MipLODBias = 0;
        texSampler.MaxAnisotropy = 0;
        texSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        texSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        texSampler.MinLOD = 0.f;
        texSampler.MaxLOD = D3D12_FLOAT32_MAX;
        texSampler.ShaderRegister = 0;
        texSampler.RegisterSpace = 0;
        texSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC envSampler{};
        envSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        envSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        envSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        envSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        envSampler.MipLODBias = 0;
        envSampler.MaxAnisotropy = 0;
        envSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        envSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        envSampler.MinLOD = 0.f;
        envSampler.MaxLOD = D3D12_FLOAT32_MAX;
        envSampler.ShaderRegister = 1;
        envSampler.RegisterSpace = 0;
        envSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        const D3D12_STATIC_SAMPLER_DESC samplers[] = { texSampler, envSampler };

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc{};
        desc.Init_1_1(_countof(rp), rp, _countof(samplers), samplers, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        return D3DX12SerializeVersionedRootSignature(&desc, version, &signature, &error);
    }))
{
    // Pipeline
    {
        D3D12_INPUT_ELEMENT_DESC inputElements[] =
        {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(NSModel::Vertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(NSModel::Vertex, normal),   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(NSModel::Vertex, tangent),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"BITANGENT",0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(NSModel::Vertex, bitangent),D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(NSModel::Vertex, texCoord), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
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

        CD3DX12_BLEND_DESC blendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
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
            {
                L"GeometryPassVS.hlsl",
                {
                    L"-E", L"mainVS",
                    L"-T", L"vs_6_0",
                    L"-Zi",
                    L"-Od"
                }
            },
            {
                L"GeometryPassPS.hlsl",
                {
                    L"-E", L"mainPS",
                    L"-T", L"ps_6_0",
                    L"-Zi",
                    L"-Od"
                }
            }
        );
    }
}
void GeometryPass::OnInit(Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
{}
void GeometryPass::OnDestroy()
{
    m_pipeline.Reset();
};
GeometryPass::~GeometryPass()
{}

void GeometryPass::Execute(Scene& scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
{
    if (not IsEnabled()) return;

    m_pipeline.Bind(cmdList);

    NSAllocator::Ctx frameCBAC = rendererCtx.constAlloc(sizeof(FrameConstants));
    {
        using namespace DirectX;

        FrameConstants& frameCB = frameCBAC.As<FrameConstants>();

        XMStoreFloat4x4(&frameCB.view, scene.m_camera.viewMatrix);
        XMStoreFloat4x4(&frameCB.proj, scene.m_camera.projMatrix);
        XMStoreFloat3(&frameCB.eye, scene.m_camera.camEye);
        XMStoreFloat4(&frameCB.lightDir, scene.m_lightDir);
        XMStoreFloat4(&frameCB.lightColor, scene.m_lightColor);
    }
    cmdList.SetGraphicsRootConstantBufferView(IDX_ROOT_CBV_FRAME, frameCBAC.gpuAddr);

    UINT width = IApp::GetInstance()->im_width;
    UINT height = IApp::GetInstance()->im_height;
    CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.f, 0.f, static_cast<FLOAT>(width), static_cast<FLOAT>(height));
    RECT scissor = CD3DX12_RECT(0L, 0L, static_cast<LONG>(width), static_cast<LONG>(height));

    cmdList.RSSetViewports(1, &viewport);
    cmdList.RSSetScissorRects(1, &scissor);

    auto brdfLUTsrv = blackboard.GetOpt<NSDescriptor::Handle>(NSRenderer::kEnvCubemap_brdfLUTsrv);
    assert(brdfLUTsrv.has_value());

    cmdList.SetGraphicsRootDescriptorTable(IDX_ROOT_DESC_ENV_BRDF_LUT_SRV, brdfLUTsrv->get().gpuAddr);

    auto optRegModels = blackboard.GetOpt<std::vector<NSRenderer::Model>>(NSRenderer::kRenderer_models);
    assert(optRegModels.has_value());

    std::vector<NSRenderer::Model>& regModels = optRegModels->get();

    for (size_t itr{1u}; itr < scene.m_models.size(); itr++)
    {
        Model& sceneModel = scene.m_models[itr];

        assert(regModels.size() > sceneModel.m_registerKey.index);

        NSRenderer::Model& regModel = regModels[sceneModel.m_registerKey.index];

        assert(scene.ValidateKeys(regModel.sceneKey, sceneModel.m_registerKey));

        cmdList.SetGraphicsRootDescriptorTable(IDX_ROOT_DESC_ENV_CUBEMAP_SRV, regModel.m_envCubemap.srvHandle.gpuAddr);

        sceneModel.Draw([this, &rendererCtx, &cmdList, &sceneModel](Mesh& mesh, UINT meshIndex, DirectX::XMMATRIX worldMatrix)
        {
            NSAllocator::Ctx allocCtx = rendererCtx.constAlloc(sizeof(MeshConstants));
            MeshConstants& meshCB = allocCtx.As<MeshConstants>();

            DirectX::XMStoreFloat4x4(&meshCB.worldMatrix, worldMatrix);
            DirectX::XMVECTOR det;
            DirectX::XMMATRIX worldInverse = DirectX::XMMatrixInverse(&det, worldMatrix);
            DirectX::XMStoreFloat3x4(&meshCB.normalMatrix, worldInverse);

            meshCB.baseColor = mesh.material.m_baseColor;
            meshCB.metallic = mesh.material.m_metallic;
            meshCB.roughness = mesh.material.m_roughness;
            meshCB.opacity = mesh.material.m_opacity;
            meshCB.textureFlags = mesh.material.GetFlags();

            cmdList.SetGraphicsRootConstantBufferView(IDX_ROOT_CBV_MESH, allocCtx.gpuAddr);

            if (mesh.material.m_isOnGPU) {
                cmdList.SetGraphicsRootDescriptorTable(IDX_ROOT_DESC_MODEL_TEX_SRV, mesh.material.m_srvHandle.gpuAddr);
            }

            cmdList.IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
            cmdList.IASetIndexBuffer(&mesh.indexBufferView);
            cmdList.DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
        });
    }
}
void GeometryPass::OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx rendererCtx)
{}

AtmospherePass::AtmospherePass(ID3D12Device14* device, Blackboard& blackboard, NSRenderer::Ctx rendererCtx)
{
    MakeRootSignature(device, L"", m_rootSignature, [](D3D_ROOT_SIGNATURE_VERSION version, ComPtr<ID3D10Blob>& signature, ComPtr<ID3D10Blob>& error) -> HRESULT
    {
        CD3DX12_DESCRIPTOR_RANGE1 uavRange{};
        uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

        CD3DX12_DESCRIPTOR_RANGE1 srvRange{};
        srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

        CD3DX12_ROOT_PARAMETER1 rp[5]{};
        rp[IDX_ROOT_CBV_FRAME].InitAsConstantBufferView(IDX_CBV_FRAME, 0);
        rp[IDX_ROOT_CBV_MESH].InitAsConstantBufferView(IDX_CBV_MESH, 0);
        rp[IDX_ROOT_CBV_ATMOSPHERE].InitAsConstantBufferView(IDX_CBV_ATMOSPHERE, 0);
        rp[IDX_ROOT_DESC_TABLE_UAV].InitAsDescriptorTable(1, &uavRange);
        rp[IDX_ROOT_DESC_TABLE_SRV].InitAsDescriptorTable(1, &srvRange);

        D3D12_STATIC_SAMPLER_DESC sampler{};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rsDesc{};
        rsDesc.Init_1_1(_countof(rp), rp, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        return D3DX12SerializeVersionedRootSignature(&rsDesc, version, &signature, &error);
    });

    this->m_transmittance = ComputePipeline(device, L"AtmospherePass::m_transmittance", m_rootSignature).Init(
        D3D12_PIPELINE_STATE_FLAG_NONE,
        {
            L"SkyDome.hlsl",
            {
                L"-E", L"CS_Transmittance",
                L"-T", L"cs_6_0",
                L"-Zi",
                L"-Od",
                L"-D", L"COMPUTE_SHADER=1"
            }
        }
    );

    this->m_scattering = ComputePipeline(device, L"AtmospherePass::m_scattering", m_rootSignature).Init(
        D3D12_PIPELINE_STATE_FLAG_NONE,
        {
            L"SkyDome.hlsl",
            {
                L"-E", L"CS_Scattering",
                L"-T", L"cs_6_0",
                L"-Zi",
                L"-Od",
                L"-D", L"COMPUTE_SHADER=1"
            }
        }
    );

    {
        D3D12_INPUT_ELEMENT_DESC inputElements[] =
        {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(NSModel::Vertex, position),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(NSModel::Vertex, normal),    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(NSModel::Vertex, tangent),   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"BITANGENT",0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(NSModel::Vertex, bitangent), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(NSModel::Vertex, texCoord),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
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

        this->m_graphics = GraphicsPipeline(device, L"AtmospherePass::m_graphics", m_rootSignature).Init(
            desc,
            rasterDesc, dsDesc, blendDesc,
            {
                L"SkyDome.hlsl",
                {
                    L"-E", L"VS_Sky",
                    L"-T", L"vs_6_0",
                    L"-Zi",
                    L"-Od",
                },
            },
            {
                L"SkyDome.hlsl",
                {
                    L"-E", L"PS_Sky",
                    L"-T", L"ps_6_0",
                    L"-Zi",
                    L"-Od",
                }
            }
        );

    }

    {
        CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);

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
        device->CreateCommittedResource(
            &defaultHeap,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&m_transmittanceLUT)
        );
        m_transmittanceLUT->SetName(L"AtmospherePass::m_transmittanceLUT");

        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
        desc.Width = 32;
        desc.Height = 128;
        desc.DepthOrArraySize = 32 * 8;
        device->CreateCommittedResource(
            &defaultHeap,
            D3D12_HEAP_FLAGS::D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&m_scatteringLUT)
        );
        m_scatteringLUT->SetName(L"AtmospherePass::m_scatteringLUT");
    }

    // 0u : UAV : Transmittance
    // 1u : UAV : Scattering
    // 2u : SRV : Transmittance
    // 3u : SRV : Scattering
    m_srvHandle = rendererCtx.allocSRVStatic(4u);

    NSDescriptor::Offset transmittanceSRVoffset = rendererCtx.offsetSRV(m_srvHandle, IDX_SRV_TRANSMITTANCE);
    NSDescriptor::Offset scatteringSRVoffset = rendererCtx.offsetSRV(m_srvHandle, IDX_SRV_SCATTERING);

    blackboard.Set(NSRenderer::kAtmosphere_transmitScatterSRV, transmittanceSRVoffset);

    device->CreateUnorderedAccessView(
        m_transmittanceLUT.Get(),
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
        device->CreateShaderResourceView(m_transmittanceLUT.Get(), &desc, transmittanceSRVoffset.cpuAddr);
    }
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
        desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        desc.Texture3D.MipLevels = 1;
        device->CreateShaderResourceView(m_scatteringLUT.Get(), &desc, scatteringSRVoffset.cpuAddr);
    }

    {
        m_timeOfDayDefault = 12.f;
        float hourAngle = (m_timeOfDayDefault / 24.f) * DirectX::XM_2PI - DirectX::XM_PIDIV2;

        DirectX::XMFLOAT3 sunDir{};
        sunDir.x = cos(hourAngle);
        sunDir.y = sin(hourAngle);

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
void AtmospherePass::OnInit(Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
{}
void AtmospherePass::OnDestroy()
{
    m_transmittanceLUT.Reset();
    m_scatteringLUT.Reset();
    m_rootSignature.Reset();
    m_graphics.Reset();
    m_transmittance.Reset();
    m_scattering.Reset();
};
AtmospherePass::~AtmospherePass()
{};
void AtmospherePass::Execute(Scene& scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
{
    if (not IsEnabled()) return;
    UINT width = IApp::GetInstance()->im_width;
    UINT height = IApp::GetInstance()->im_height;

    CD3DX12_VIEWPORT viewport(0.f, 0.f, static_cast<FLOAT>(width), static_cast<FLOAT>(height));
    CD3DX12_RECT scissor(0L, 0L, static_cast<LONG>(width), static_cast<LONG>(height));

    cmdList.RSSetViewports(1, &viewport);
    cmdList.RSSetScissorRects(1, &scissor);

    NSAllocator::Ctx atmosCBAC{};
    if (m_constantsUpload != m_constantsDefault or scene.m_timeOfDay != m_timeOfDayDefault)
    {
        const float hourAngle = (scene.m_timeOfDay / 24.f) * DirectX::XM_2PI - DirectX::XM_PIDIV2;

        DirectX::XMFLOAT3 sunDir = { cosf(hourAngle), sinf(hourAngle), 0.f };
        DirectX::XMVECTOR vSunDir = DirectX::XMLoadFloat3(&sunDir);

        scene.m_lightDir = DirectX::XMVectorNegate(vSunDir);
        DirectX::XMStoreFloat3(&m_constantsUpload.SunDir, vSunDir);

        m_constantsDefault = m_constantsUpload;
        m_timeOfDayDefault = scene.m_timeOfDay;

        blackboard.Set<AtmosphereConstants&>(NSRenderer::kAtmosphere_constants, m_constantsDefault);

        atmosCBAC = rendererCtx.constAlloc(sizeof(AtmosphereConstants));
        atmosCBAC.As<AtmosphereConstants>() = m_constantsDefault;

        UpdateAtmosphere(atmosCBAC.gpuAddr, rendererCtx, cmdList);
    }
    else
    {
        atmosCBAC = rendererCtx.constAlloc(sizeof(AtmosphereConstants));
        atmosCBAC.As<AtmosphereConstants>() = m_constantsDefault;
    }

    NSAllocator::Ctx frameCBAC = rendererCtx.constAlloc(sizeof(FrameConstants));
    {
        using namespace DirectX;

        FrameConstants& frameCB = frameCBAC.As<FrameConstants>();

        XMStoreFloat4x4(&frameCB.view, scene.m_camera.viewMatrix);
        XMStoreFloat4x4(&frameCB.proj, scene.m_camera.projMatrix);
        XMStoreFloat3(&frameCB.eye, scene.m_camera.camEye);
        XMStoreFloat4(&frameCB.lightDir, scene.m_lightDir);
        XMStoreFloat4(&frameCB.lightColor, scene.m_lightColor);
    }

    m_graphics.Bind(cmdList);
    cmdList.SetGraphicsRootConstantBufferView(IDX_ROOT_CBV_FRAME, frameCBAC.gpuAddr);
    cmdList.SetGraphicsRootConstantBufferView(IDX_ROOT_CBV_ATMOSPHERE, atmosCBAC.gpuAddr);
    cmdList.SetGraphicsRootDescriptorTable(IDX_ROOT_DESC_TABLE_SRV, rendererCtx.offsetSRV(m_srvHandle, IDX_SRV_TRANSMITTANCE).gpuAddr);

    scene.m_models[0].Draw([this, &rendererCtx, &cmdList](Mesh& mesh, UINT meshIndex, DirectX::XMMATRIX worldMatrix)
    {
        NSAllocator::Ctx allocCtx = rendererCtx.constAlloc(sizeof(MeshConstants));
        MeshConstants meshCB = allocCtx.As<MeshConstants>();
        cmdList.SetGraphicsRootConstantBufferView(IDX_ROOT_CBV_MESH, allocCtx.gpuAddr);

        DirectX::XMStoreFloat4x4(&meshCB.worldMatrix, worldMatrix);
        DirectX::XMVECTOR det;
        DirectX::XMMATRIX worldInverse = DirectX::XMMatrixInverse(&det, worldMatrix);
        DirectX::XMStoreFloat3x4(&meshCB.normalMatrix, worldInverse);

        meshCB.baseColor = mesh.material.m_baseColor;
        meshCB.metallic = mesh.material.m_metallic;
        meshCB.roughness = mesh.material.m_roughness;
        meshCB.opacity = mesh.material.m_opacity;
        meshCB.textureFlags = mesh.material.GetFlags();

        cmdList.IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
        cmdList.IASetIndexBuffer(&mesh.indexBufferView);
        cmdList.DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
    });
}
void AtmospherePass::OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx rendererCtx)
{}
void AtmospherePass::UpdateAtmosphere(D3D12_GPU_VIRTUAL_ADDRESS constantsGpuAddr, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
{
    {
        CD3DX12_RESOURCE_BARRIER barriers[] =
        {
            CD3DX12_RESOURCE_BARRIER::Transition(
                m_transmittanceLUT.Get(),
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

    m_transmittance.Bind(cmdList);
    cmdList.SetComputeRootConstantBufferView(IDX_ROOT_CBV_ATMOSPHERE, constantsGpuAddr);
    cmdList.SetComputeRootDescriptorTable(IDX_ROOT_DESC_TABLE_UAV, rendererCtx.offsetSRV(m_srvHandle, IDX_UAV_TRANSMITTANCE).gpuAddr);
    cmdList.Dispatch(
        (256 + 7) / 8,
        (64 + 7) / 8,
        1
    );

    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_transmittanceLUT.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
        );
        cmdList.ResourceBarrier(1, &barrier);
    }

    m_scattering.Bind(cmdList);
    cmdList.SetComputeRootConstantBufferView(IDX_ROOT_CBV_ATMOSPHERE, constantsGpuAddr);
    cmdList.SetComputeRootDescriptorTable(IDX_ROOT_DESC_TABLE_UAV, rendererCtx.offsetSRV(m_srvHandle, IDX_UAV_SCATTERING).gpuAddr);
    cmdList.SetComputeRootDescriptorTable(IDX_ROOT_DESC_TABLE_SRV, rendererCtx.offsetSRV(m_srvHandle, IDX_SRV_TRANSMITTANCE).gpuAddr);
    cmdList.Dispatch(
        (32  + 3) / 4,
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

EnvironmentCubemapPass::EnvironmentCubemapPass(ID3D12Device14* device, Blackboard& blackboard, NSRenderer::Ctx rendererCtx)
{
    MakeRootSignature(device, L"EnvironmentCubemapPass::RootSingature", m_graphicsRoot, [](D3D_ROOT_SIGNATURE_VERSION version, ComPtr<ID3D10Blob>& signature, ComPtr<ID3D10Blob>& error) -> HRESULT
    {
        CD3DX12_DESCRIPTOR_RANGE1 atmosRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
        CD3DX12_DESCRIPTOR_RANGE1 modelRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 28, 2, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

        CD3DX12_ROOT_PARAMETER1 rp[5]{};
        rp[IDX_ROOT_CBV_CAPTURE].InitAsConstantBufferView(IDX_CBV_CAPTURE, 0);
        rp[IDX_ROOT_CBV_MESH].InitAsConstantBufferView(IDX_ROOT_CBV_MESH, 0);
        rp[IDX_ROOT_CBV_ATMOSPHERE].InitAsConstantBufferView(IDX_CBV_ATMOSPHERE, 0);
        rp[IDX_ROOT_DESC_MODEL_SRV].InitAsDescriptorTable(1, &modelRange, D3D12_SHADER_VISIBILITY_PIXEL);
        rp[IDX_ROOT_DESC_ATMOS_SRV].InitAsDescriptorTable(1, &atmosRange, D3D12_SHADER_VISIBILITY_PIXEL);

        D3D12_STATIC_SAMPLER_DESC samplers[2]{};
        samplers[0].ShaderRegister = 0;
        samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

        samplers[1].ShaderRegister = 1;
        samplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        samplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rsDesc{};
        rsDesc.Init_1_1(
          _countof(rp), rp, _countof(samplers), samplers, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
        );

        return D3DX12SerializeVersionedRootSignature(&rsDesc, version, &signature, &error);
    });

    // Sky sub-pass pipeline
    {
        CD3DX12_RASTERIZER_DESC rasterizer(D3D12_DEFAULT);
        rasterizer.CullMode = D3D12_CULL_MODE_NONE;

        CD3DX12_DEPTH_STENCIL_DESC ds(D3D12_DEFAULT);
        ds.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        ds.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

        CD3DX12_BLEND_DESC blend(D3D12_DEFAULT);

        GRAPHICS_PIPELINE_STATE_DESC desc{};
        desc.InputLayout = {nullptr, 0};
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        desc.NumRenderTargets = 1;
        desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
        desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        desc.SampleDesc.Count = 1;
        desc.SampleMask = UINT32_MAX;

        m_atmosPipeline = GraphicsPipeline(device, L"EnvironmentCubemapPass::m_atmosPipeline", m_graphicsRoot).Init(
            desc, rasterizer, ds, blend,
            {
                L"EnvCaptureSky.hlsl",
                {
                    L"-E", L"VS_EnvSky",
                    L"-T", L"vs_6_0",
                    L"-Zi",
                    L"-Od"
                },
            },
            {
                L"EnvCaptureSky.hlsl",
                {
                    L"-E", L"PS_EnvSky",
                    L"-T", L"ps_6_0",
                    L"-Zi",
                    L"-Od"
                }
            }
        );
    }

    // Geometry sub-pass pipeline
    {
        D3D12_INPUT_ELEMENT_DESC inputElements[] =
        {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(NSModel::Vertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(NSModel::Vertex, normal),   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(NSModel::Vertex, tangent),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"BITANGENT",0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(NSModel::Vertex, bitangent),D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(NSModel::Vertex, texCoord), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };

        CD3DX12_RASTERIZER_DESC rasterizer(D3D12_DEFAULT);
        CD3DX12_DEPTH_STENCIL_DESC ds(D3D12_DEFAULT);
        CD3DX12_BLEND_DESC blend(D3D12_DEFAULT);

        GRAPHICS_PIPELINE_STATE_DESC desc{};
        desc.InputLayout = {inputElements, _countof(inputElements) };
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        desc.NumRenderTargets = 1;
        desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
        desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        desc.SampleDesc.Count = 1;
        desc.SampleMask = UINT32_MAX;

        m_geomPipeline = GraphicsPipeline(device, L"EnvironmentCubemapPass::m_geomPipeline", m_graphicsRoot).Init(
            desc, rasterizer, ds, blend,
            {
                L"EnvCaptureGeo.hlsl",
                {
                    L"-E", L"VS_EnvGeo",
                    L"-T", L"vs_6_0",
                    L"-Zi",
                    L"-Od"
                },
            },
            {
                L"EnvCaptureGeo.hlsl",
                {
                    L"-E", L"PS_EnvGeo",
                    L"-T", L"ps_6_0",
                    L"-Zi",
                    L"-Od"
                }
            }
        );
    }

    // Prefilter compute pipeline
    {
        MakeRootSignature(device, L"EnvironmentCubemapPass::m_prefilterRoot", m_prefilterRoot, [](D3D_ROOT_SIGNATURE_VERSION version, ComPtr<ID3D10Blob>& signature, ComPtr<ID3D10Blob>& error) -> HRESULT
        {
            CD3DX12_DESCRIPTOR_RANGE1 srvRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
            CD3DX12_DESCRIPTOR_RANGE1 uavRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

            CD3DX12_ROOT_PARAMETER1 rp[3]{};
            rp[0].InitAsConstants(4, 0); // roughness, faceSize, numSamples, pad
            rp[1].InitAsDescriptorTable(1, &srvRange);
            rp[2].InitAsDescriptorTable(1, &uavRange);

            D3D12_STATIC_SAMPLER_DESC sampler{};
            sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            sampler.ShaderRegister = 0;

            CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rsDesc{};
            rsDesc.Init_1_1(_countof(rp), rp, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_NONE);

            return D3DX12SerializeVersionedRootSignature(&rsDesc, version, &signature, &error);
        });

        m_prefilterPipeline = ComputePipeline(device, L"EnvironmentCubemapPass::m_prefilterPipeline", m_prefilterRoot).Init(
            D3D12_PIPELINE_STATE_FLAG_NONE,
            {
                L"PrefilterEnvMap.hlsl",
                {
                    L"-E", L"main",
                    L"-T", L"cs_6_0",
                    L"-Zi",
                    L"-Od"
                }
            }
        );
    }

    // BRDF integration LUT compute pipeline
    {
        MakeRootSignature(device, L"EnvironmentCubemapPass::m_brdfRoot", m_brdfRoot, [](D3D_ROOT_SIGNATURE_VERSION version, ComPtr<ID3D10Blob>& signature, ComPtr<ID3D10Blob>& error) -> HRESULT
        {
            CD3DX12_DESCRIPTOR_RANGE1 uavRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

            CD3DX12_ROOT_PARAMETER1 rp[2]{};
            rp[0].InitAsConstants(4, 0); // resolution, numSamples, pad, pad
            rp[1].InitAsDescriptorTable(1, &uavRange);

            CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rsDesc{};
            rsDesc.Init_1_1(_countof(rp), rp, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

            return D3DX12SerializeVersionedRootSignature(&rsDesc, version, &signature, &error);
        });

        m_brdfPipeline = ComputePipeline(device, L"EnvironmentCubemapPass::m_brdfPipeline", m_brdfRoot).Init(
            D3D12_PIPELINE_STATE_FLAG_NONE,
            {
                L"IntegrateBRDF.hlsl",
                {
                    L"-E", L"main",
                    L"-T", L"cs_6_0",
                    L"-Zi",
                    L"-Od",
                }
            }
        );

        // Create BRDF LUT Texture
        {
            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            desc.Width = BRDF_LUT_SIZE;
            desc.Height = BRDF_LUT_SIZE;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.Format = DXGI_FORMAT_R16G16_FLOAT;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

            CD3DX12_HEAP_PROPERTIES props(D3D12_HEAP_TYPE_DEFAULT);

            ThrowIfFailed(device->CreateCommittedResource(
                &props,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_COMMON,
                nullptr,
                IID_PPV_ARGS(&m_brdfLUT)
            ));
            m_brdfLUT->SetName(L"EnvironmentCubemapPass::m_brdfLUT");
        }

        // BRDF LUT UAV
        {
            m_brdfUAVhandle = rendererCtx.allocSRVStatic(1u);

            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
            uavDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            uavDesc.Texture2D.MipSlice = 0;

            device->CreateUnorderedAccessView(m_brdfLUT.Get(), nullptr, &uavDesc, m_brdfUAVhandle.cpuAddr);
        }

        // BRDF LUT SRV
        {
            m_brdfSRVhandle = rendererCtx.allocSRVStatic(1u);

            D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
            desc.Format = DXGI_FORMAT_R16G16_FLOAT;
            desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            desc.Texture2D.MipLevels = 1;
            desc.Texture2D.MostDetailedMip = 0;

            device->CreateShaderResourceView(m_brdfLUT.Get(), &desc, m_brdfSRVhandle.cpuAddr);
        }
    }
}
EnvironmentCubemapPass::~EnvironmentCubemapPass(){};

void EnvironmentCubemapPass::OnInit(Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
{
    GenerateBRDFLUT(rendererCtx, cmdList);
}
void EnvironmentCubemapPass::OnDestroy()
{
    m_brdfLUT.Reset();
    m_graphicsRoot.Reset();
    m_prefilterRoot.Reset();
    m_brdfRoot.Reset();
    m_atmosPipeline.Reset();
    m_geomPipeline.Reset();
    m_prefilterPipeline.Reset();
    m_brdfPipeline.Reset();
}
void EnvironmentCubemapPass::Execute(Scene& scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
{
    if (not IsEnabled()) return;

    blackboard.Set<NSDescriptor::Handle&>(NSRenderer::kEnvCubemap_brdfLUTsrv, m_brdfSRVhandle);

    using namespace DirectX;

    auto optRegModels = blackboard.GetOpt<std::vector<NSRenderer::Model>>(NSRenderer::kRenderer_models);
    assert(optRegModels.has_value());

    std::vector<NSRenderer::Model>& regModels = optRegModels->get();

    for (Model& sceneModel : scene.m_models)
    {
        assert(regModels.size() > sceneModel.m_registerKey.index);

        NSRenderer::Model& regModel = regModels[sceneModel.m_registerKey.index];

        assert(scene.ValidateKeys(regModel.sceneKey, sceneModel.m_registerKey));

        if (sceneModel.TestFlag(NSModel::EModelFlag::MODEL_FLAG_NO_ENV_CUBEMAP)) continue;

        if (regModel.isDirty)
        {
            Capture(sceneModel, scene, blackboard, rendererCtx, cmdList);
            continue;
        }
        else
        {
            for (NSRenderer::Model::Neighbor& neighbor : regModel.objsInFrustum)
            {
                if (not NSMath::Float3Equals(neighbor.position, sceneModel.GetPosition()))
                {
                    regModel.isDirty = true;
                }
            }
        }
    }
}
void EnvironmentCubemapPass::OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx rendererCtx){}
void EnvironmentCubemapPass::Capture(Model& inModel, Scene& scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
{
    auto optRegModels = blackboard.GetOpt<std::vector<NSRenderer::Model>>(NSRenderer::kRenderer_models);
    std::vector<NSRenderer::Model>& regModels = optRegModels->get();
    NSRenderer::Model& regInModel = regModels[inModel.m_registerKey.index];

    DirectX::XMFLOAT3 pos = inModel.GetPosition();
    const DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV2, 1.f, scene.NEAR_CLIP, scene.FAR_CLIP);
    NSScene::Camera cams[regInModel.m_envCubemap.NUM_FACES] =
    {
        NSScene::Camera(proj, pos, { 1, 0, 0, 0 }, { 0, 1, 0, 0 }),
        NSScene::Camera(proj, pos, {-1, 0, 0, 0 }, { 0, 1, 0, 0 }),
        NSScene::Camera(proj, pos, { 0, 1, 0, 0 }, { 0, 0,-1, 0 }),
        NSScene::Camera(proj, pos, { 0,-1, 0, 0 }, { 0, 0, 1, 0 }),
        NSScene::Camera(proj, pos, { 0, 0, 1, 0 }, { 0, 1, 0, 0 }),
        NSScene::Camera(proj, pos, { 0, 0,-1, 0 }, { 0, 1, 0, 0 })
    };

    {
        D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            regInModel.m_envCubemap.cubemapTexture.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );
        cmdList.ResourceBarrier(1, &barrier);
    }

    CD3DX12_VIEWPORT viewport(0.f, 0.f, regInModel.m_envCubemap.PER_FACE_RESOLUTION, regInModel.m_envCubemap.PER_FACE_RESOLUTION);
    CD3DX12_RECT scissor(0L, 0L, regInModel.m_envCubemap.PER_FACE_RESOLUTION, regInModel.m_envCubemap.PER_FACE_RESOLUTION);

    cmdList.RSSetViewports(1, &viewport);
    cmdList.RSSetScissorRects(1, &scissor);

    for (size_t face{}; face < regInModel.m_envCubemap.NUM_FACES; face++)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rendererCtx.offsetRTV(regInModel.m_envCubemap.rtvHandle, static_cast<UINT>(face)).cpuAddr;
        cmdList.OMSetRenderTargets(1, &rtvHandle, FALSE, &regInModel.m_envCubemap.dsvHandle.cpuAddr);

        const float clearColor[] = {0.f, 0.f, 0.f, 1.f};
        cmdList.ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
        cmdList.ClearDepthStencilView(regInModel.m_envCubemap.dsvHandle.cpuAddr, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

        NSScene::Camera& cam = cams[face];

        NSAllocator::Ctx captureCBAC = rendererCtx.constAlloc(sizeof(EnvCaptureConstants));
        {
            EnvCaptureConstants& constants = captureCBAC.As<EnvCaptureConstants>();
            DirectX::XMStoreFloat4x4(&constants.view, cam.viewMatrix);
            DirectX::XMStoreFloat4x4(&constants.proj, cam.projMatrix);
            DirectX::XMStoreFloat4(&constants.lightDir, scene.m_lightDir);
            DirectX::XMStoreFloat4(&constants.lightColor, scene.m_lightColor);
            DirectX::XMFLOAT3 pos = inModel.GetPosition();
            constants.capturePos = pos;
            constants.camPos = pos;
        }

        // Sky pass
        {
            m_atmosPipeline.Bind(cmdList);

            auto optAtmosConstDefault = blackboard.GetOpt<AtmosphereConstants>(NSRenderer::kAtmosphere_constants);
            assert(optAtmosConstDefault.has_value());

            NSAllocator::Ctx atmosCBCA = rendererCtx.constAlloc(sizeof(AtmosphereConstants));
            AtmosphereConstants& atmosCB = atmosCBCA.As<AtmosphereConstants>();
            atmosCB = optAtmosConstDefault->get();

            auto atmosphereSRVs = blackboard.GetOpt<NSDescriptor::Offset>(NSRenderer::kAtmosphere_transmitScatterSRV);
            assert(atmosphereSRVs.has_value());

            cmdList.SetGraphicsRootConstantBufferView(IDX_ROOT_CBV_CAPTURE, captureCBAC.gpuAddr);
            cmdList.SetGraphicsRootConstantBufferView(IDX_ROOT_CBV_ATMOSPHERE, atmosCBCA.gpuAddr);
            cmdList.SetGraphicsRootDescriptorTable(IDX_ROOT_DESC_ATMOS_SRV, atmosphereSRVs->get().gpuAddr);

            cmdList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            cmdList.DrawInstanced(3, 1, 0, 0);
        }

        // Geom Pass
        {
            m_geomPipeline.Bind(cmdList);
            cmdList.SetGraphicsRootConstantBufferView(IDX_ROOT_CBV_CAPTURE, captureCBAC.gpuAddr);

            scene.CullScene(&cam, inModel.m_sceneKey);

            for (NSModel::SceneModelKey& sceneKey : scene.m_modelsCulled)
            {
                Model& sceneModel = scene.m_models[sceneKey.index];

                assert(regModels.size() > sceneModel.m_registerKey.index);

                NSRenderer::Model& regModel = regModels[sceneModel.m_registerKey.index];

                assert(scene.ValidateKeys(regModel.sceneKey, sceneModel.m_registerKey));

                if (regModel.TestFlag(NSModel::ERegModelFlag::MODEL_FLAG_UNSEEN_TO_ENV_CAPTURE)) continue;

                sceneModel.Draw([this, &rendererCtx, &cmdList](Mesh& mesh, UINT meshIndex, DirectX::XMMATRIX worldMatrix)
                {
                    NSAllocator::Ctx allocCtx = rendererCtx.constAlloc(sizeof(MeshConstants));
                    MeshConstants& meshCB = allocCtx.As<MeshConstants>();
                    cmdList.SetGraphicsRootConstantBufferView(IDX_ROOT_CBV_MESH, allocCtx.gpuAddr);

                    DirectX::XMStoreFloat4x4(&meshCB.worldMatrix, worldMatrix);
                    DirectX::XMVECTOR det;
                    DirectX::XMMATRIX worldInverse = DirectX::XMMatrixInverse(&det, worldMatrix);
                    DirectX::XMStoreFloat3x4(&meshCB.normalMatrix, worldInverse);

                    meshCB.baseColor = mesh.material.m_baseColor;
                    meshCB.metallic = mesh.material.m_metallic;
                    meshCB.roughness = mesh.material.m_roughness;
                    meshCB.opacity = mesh.material.m_opacity;
                    meshCB.textureFlags = mesh.material.GetFlags();

                    cmdList.IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
                    cmdList.IASetIndexBuffer(&mesh.indexBufferView);
                    cmdList.DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
                });
            }
        }
    }

    {
        D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            regInModel.m_envCubemap.cubemapTexture.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );
        cmdList.ResourceBarrier(1, &barrier);
    }

    PrefilterCubemap(regInModel.m_envCubemap, rendererCtx, cmdList);

    regInModel.isDirty = false;

    auto mainRTV = blackboard.GetOpt<D3D12_CPU_DESCRIPTOR_HANDLE>(NSRenderer::kRenderer_mainRTV);
    auto mainDSV = blackboard.GetOpt<D3D12_CPU_DESCRIPTOR_HANDLE>(NSRenderer::kRenderer_mainDSV);
    assert(mainRTV.has_value() and mainDSV.has_value());

    cmdList.OMSetRenderTargets(1, &mainRTV->get(), FALSE, &mainDSV->get());
}
void EnvironmentCubemapPass::PrefilterCubemap(NSRenderer::EnvironmentCubemap& envMap, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
{
    std::vector<D3D12_RESOURCE_BARRIER> barriers;
    barriers.reserve(envMap.NUM_FACES * envMap.MIP_COUNT);

    {
        for (UINT face{}; face < envMap.NUM_FACES; face++)
        {
            barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
                envMap.cubemapTexture.Get(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                face * envMap.MIP_COUNT + 0u
            ));

            for (UINT mip = 1u; mip < envMap.MIP_COUNT; mip++)
            {
                barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
                    envMap.cubemapTexture.Get(),
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                    face * envMap.MIP_COUNT + mip
                ));
            }
        }
        cmdList.ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
    }

    m_prefilterPipeline.Bind(cmdList);

    cmdList.SetComputeRootDescriptorTable(1, envMap.srvHandle.gpuAddr);

    constexpr UINT numSamples = 1024;

    for (UINT mip{1u}; mip < envMap.MIP_COUNT; mip++)
    {
        UINT mipSize = envMap.PER_FACE_RESOLUTION >> mip;
        float roughness = static_cast<float>(mip) / static_cast<float>(envMap.MIP_COUNT - 1u);

        UINT constants[4]{};
        memcpy(&constants[0], &roughness, sizeof(float)); // Prevent from conversion
        constants[1] = mipSize;
        constants[2] = numSamples;
        constants[3] = 0u; // Pad

        cmdList.SetComputeRoot32BitConstants(0, 4, constants, 0);
        cmdList.SetComputeRootDescriptorTable(2, rendererCtx.offsetSRV(envMap.uavHandle, mip - 1u).gpuAddr);

        cmdList.Dispatch(
            (mipSize + 7u) / 8u,
            (mipSize + 7u) / 8u,
            envMap.NUM_FACES
        );
    }

    {
        barriers.clear();

        for (UINT face{}; face < envMap.NUM_FACES; face++)
        {
            barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
                envMap.cubemapTexture.Get(),
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                face * envMap.MIP_COUNT + 0u
            ));

            for (UINT mip = 1u; mip < envMap.MIP_COUNT; mip++)
            {
                barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
                    envMap.cubemapTexture.Get(),
                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                    face * envMap.MIP_COUNT + mip
                ));
            }
        }
        cmdList.ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
    }
}
void EnvironmentCubemapPass::GenerateBRDFLUT(NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
{
    {
        D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_brdfLUT.Get(),
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS
        );
        cmdList.ResourceBarrier(1, &barrier);
    }

    m_brdfPipeline.Bind(cmdList);

    constexpr UINT numSamples = 1024u;
    UINT constants[4] = { BRDF_LUT_SIZE, numSamples, 0u, 0u}; // resolution,
    cmdList.SetComputeRoot32BitConstants(0, 4, constants, 0);
    cmdList.SetComputeRootDescriptorTable(1, m_brdfUAVhandle.gpuAddr);

    cmdList.Dispatch(
        (BRDF_LUT_SIZE + 7u) / 8u,
        (BRDF_LUT_SIZE + 7u) / 8u,
        1
    );

    {
        D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_brdfLUT.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );
        cmdList.ResourceBarrier(1, &barrier);
    }
}

TerrainPass::TerrainPass(ID3D12Device14* device, Blackboard& blackboard, NSRenderer::Ctx rendererCtx)
{
    MakeRootSignature(device, L"TerrainPass::m_rootSignature", m_rootSignature, [](D3D_ROOT_SIGNATURE_VERSION version, ComPtr<ID3D10Blob>& signature, ComPtr<ID3D10Blob>& error) -> HRESULT
    {
        CD3DX12_ROOT_PARAMETER1 rp[2]{};
        rp[IDX_ROOT_CBV_FRAME].InitAsConstantBufferView(IDX_CBV_FRAME, 0);
        rp[IDX_ROOT_CBV_TERRAIN].InitAsConstantBufferView(IDX_CBV_TERRAIN, 0);

        D3D12_STATIC_SAMPLER_DESC samplers[2]{};
        samplers[0].ShaderRegister = 0;
        samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplers[0].MipLODBias = 0.f;
        samplers[0].MaxAnisotropy = 0;
        samplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        samplers[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        samplers[0].MinLOD = 0.f;
        samplers[0].MaxLOD = D3D12_FLOAT32_MAX;
        samplers[0].RegisterSpace = 0;

        samplers[1].ShaderRegister = 1;
        samplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        samplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplers[1].MipLODBias = 0.f;
        samplers[1].MaxAnisotropy = 0;
        samplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        samplers[1].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        samplers[1].MinLOD = 0.f;
        samplers[1].MaxLOD = D3D12_FLOAT32_MAX;
        samplers[1].RegisterSpace = 0;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rsDesc{};
        rsDesc.Init_1_1(
            _countof(rp), rp, _countof(samplers), samplers, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED
        );

        return D3DX12SerializeVersionedRootSignature(&rsDesc, version, &signature, &error);
    });

    m_solidPipeline = TessellationPipeline(device, L"", m_rootSignature);
    m_wireframePipeline = TessellationPipeline(device, L"", m_rootSignature);

    D3D12_INPUT_ELEMENT_DESC inputElements[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(NSTerrain::Vertex, position),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(NSTerrain::Vertex, texCoord),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };

    CD3DX12_RASTERIZER_DESC rasterDesc(D3D12_DEFAULT);
    rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterDesc.CullMode = D3D12_CULL_MODE_BACK;

    CD3DX12_DEPTH_STENCIL_DESC dsDesc(D3D12_DEFAULT);
    dsDesc.DepthEnable = TRUE;
    dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    CD3DX12_BLEND_DESC blendDesc(D3D12_DEFAULT);

    TESSELLATION_PIPELINE_STATE_DESC desc{};
    desc.SampleMask = UINT_MAX;
    desc.InputLayout = { inputElements, _countof(inputElements) };
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    desc.SampleDesc.Count = 1;

    this->m_solidPipeline = TessellationPipeline(device, L"TerrainPass::m_solidPipeline", m_rootSignature);
}
TerrainPass::~TerrainPass()
{

};

void TerrainPass::OnInit(Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
{

};
void TerrainPass::OnDestroy()
{

};

void TerrainPass::Execute(Scene& scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
{

};
void TerrainPass::OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx rendererCtx)
{

};
