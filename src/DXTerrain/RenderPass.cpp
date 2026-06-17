#include "stdafx.h"
#include "RenderPass.h"

#include "IApp.h"
#include "DXSampleHelper.h"
#include "Blackboard.h"
#include "Scene.h"
#include "Terrain.h"
#include "Texture.h"

using namespace NSRenderPass;

GeometryPass::GeometryPass(ID3D12Device14* device, Blackboard& blackboard, NSRenderer::Ctx rendererCtx)
    : m_pipeline(GraphicsPipeline(device, L"GeometryPass::Graphics", [](D3D_ROOT_SIGNATURE_VERSION version, ComPtr<ID3D10Blob>& signature, ComPtr<ID3D10Blob>& error) -> HRESULT
    {
        constexpr UINT kModelTexCount = static_cast<UINT>(NSTexture::EType::MAX);
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
{}
GeometryPass& GeometryPass::OnInit(Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSDX12::GraphicsCommandList cmdList)
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

        CD3DX12_RASTERIZER_DESC rasterDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

        CD3DX12_DEPTH_STENCIL_DESC dsDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;

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
            rasterDesc,
            dsDesc,
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

    return *this;
}
void GeometryPass::OnDestroy(NSRenderer::Ctx rendererCtx)
{
    m_pipeline.Reset();
};
GeometryPass::~GeometryPass()
{}

void GeometryPass::Execute(NSScene::IScene& _scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSDX12::GraphicsCommandList cmdList)
{
    if (not im_isEnabled) return;

    Scene& scene = static_cast<Scene&>(_scene);

    m_pipeline.Bind(cmdList);
    cmdList.IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    UINT width = IApp::GetInstance()->im_width;
    UINT height = IApp::GetInstance()->im_height;
    CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.f, 0.f, static_cast<FLOAT>(width), static_cast<FLOAT>(height));
    RECT scissor = CD3DX12_RECT(0L, 0L, static_cast<LONG>(width), static_cast<LONG>(height));

    cmdList.RSSetViewports(1, &viewport);
    cmdList.RSSetScissorRects(1, &scissor);

    NSAllocator::Ctx frameCBAC = rendererCtx.constAlloc(sizeof(FrameConstants));
    {
        using namespace DirectX;

        FrameConstants& frameCB = frameCBAC.As<FrameConstants>();

        std::shared_ptr<NSScene::Camera> mainCam =scene.GetMainCamera();

        XMStoreFloat4x4(&frameCB.view, mainCam->viewMatrix);
        XMStoreFloat4x4(&frameCB.proj, mainCam->projMatrix);
        XMStoreFloat3(&frameCB.eye, mainCam->camEye);
        XMStoreFloat4(&frameCB.lightDir, scene.m_lightDir);
        XMStoreFloat4(&frameCB.lightColor, scene.m_lightColor);
    }
    cmdList.SetGraphicsRootConstantBufferView(IDX_ROOT_CBV_FRAME, frameCBAC.gpuAddr);

    auto brdfLUTsrv = blackboard.GetOpt<NSDescriptor::Handle>(NSRenderer::kEnvCubemap_brdfLUTsrv);
    ASSERT(brdfLUTsrv.has_value());

    cmdList.SetGraphicsRootDescriptorTable(IDX_ROOT_DESC_ENV_BRDF_LUT_SRV, brdfLUTsrv->get().gpuAddr);

    auto optRegModels = blackboard.GetOpt<EntityMap<NSRenderer::Model>>(NSRenderer::kRenderer_models);
    ASSERT(optRegModels.has_value());

    EntityMap<NSRenderer::Model>& regModels = optRegModels->get();

    std::weak_ptr<NSScene::Camera> wpMainCamera = scene.GetMainCamera();
    ASSERT(wpMainCamera.lock(), "Main camera is invalid");

    auto mainCam = wpMainCamera.lock();

    std::shared_ptr<NSScene::CullResult> cullResult = scene.Cull(mainCam->m_id, NSScene::EIncCullFlag::STATIC_OBJECTS);

    for (NSMath::ICullable& culled : cullResult->m_culledObjects)
    {
        ASSERT(scene.m_models.Contains(culled.ICullable_Id));
        std::shared_ptr<Model> sceModel = scene.m_models.Get(culled.ICullable_Id);

        if (not sceModel->m_flags.HasLeastOne(NSModel::EModelFlag::PBR_MODEL)) continue;

        ASSERT(regModels.Contains(sceModel->m_registerKey));
        std::shared_ptr<NSRenderer::Model> regModel = regModels.Get(sceModel->m_registerKey);

        cmdList.SetGraphicsRootDescriptorTable(IDX_ROOT_DESC_ENV_CUBEMAP_SRV, regModel->m_envCubemap.srvHandle.gpuAddr);

        sceModel->Draw([this, &rendererCtx, &cmdList, &sceModel](Mesh& mesh, UINT meshIndex, DirectX::XMMATRIX worldMatrix) -> LoopCondition
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

            return ELoopConditionFlag::CONTINUE;
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
        dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;

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
AtmospherePass& AtmospherePass::OnInit(Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSDX12::GraphicsCommandList cmdList)
{
    return *this;
}
void AtmospherePass::OnDestroy(NSRenderer::Ctx rendererCtx)
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
void AtmospherePass::Execute(NSScene::IScene& _scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSDX12::GraphicsCommandList cmdList)
{
    if (not im_isEnabled) return;
    UINT width = IApp::GetInstance()->im_width;
    UINT height = IApp::GetInstance()->im_height;

    Scene& scene = static_cast<Scene&>(_scene);

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

    std::shared_ptr<NSScene::Camera> mainCam = scene.GetMainCamera();

    NSAllocator::Ctx frameCBAC = rendererCtx.constAlloc(sizeof(FrameConstants));
    {
        using namespace DirectX;

        FrameConstants& frameCB = frameCBAC.As<FrameConstants>();

        XMStoreFloat4x4(&frameCB.view, mainCam->viewMatrix);
        XMStoreFloat4x4(&frameCB.proj, mainCam->projMatrix);
        XMStoreFloat3(&frameCB.eye, mainCam->camEye);
        XMStoreFloat4(&frameCB.lightDir, scene.m_lightDir);
        XMStoreFloat4(&frameCB.lightColor, scene.m_lightColor);
    }

    m_graphics.Bind(cmdList);
    cmdList.IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY::D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList.SetGraphicsRootConstantBufferView(IDX_ROOT_CBV_FRAME, frameCBAC.gpuAddr);
    cmdList.SetGraphicsRootConstantBufferView(IDX_ROOT_CBV_ATMOSPHERE, atmosCBAC.gpuAddr);
    cmdList.SetGraphicsRootDescriptorTable(IDX_ROOT_DESC_TABLE_SRV, rendererCtx.offsetSRV(m_srvHandle, IDX_SRV_TRANSMITTANCE).gpuAddr);

    std::shared_ptr<Model> skyDome;

    scene.m_models.ForEach([&skyDome](EntityID, std::shared_ptr<Model> model) -> LoopCondition
    {
        if (model->m_flags.HasLeastAll(NSModel::EModelFlag::ATMOSPHERE) and model->isOnGPU)
        {
            skyDome = model;
            return ELoopConditionFlag::BREAK;
        }
        return ELoopConditionFlag::CONTINUE;
    });

    ASSERT(skyDome, "Unable to find sky dome or skydome didn't uploaded to gpu yet");

    skyDome->Draw([this, &rendererCtx, &cmdList](Mesh& mesh, UINT meshIndex, DirectX::XMMATRIX worldMatrix) -> LoopCondition
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

        return ELoopConditionFlag::CONTINUE;
    });
}
void AtmospherePass::OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx rendererCtx)
{}
void AtmospherePass::UpdateAtmosphere(D3D12_GPU_VIRTUAL_ADDRESS constantsGpuAddr, NSRenderer::Ctx rendererCtx, NSDX12::GraphicsCommandList cmdList)
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

EnvironmentCubemapPass& EnvironmentCubemapPass::OnInit(Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSDX12::GraphicsCommandList cmdList)
{
    GenerateBRDFLUT(rendererCtx, cmdList);

    return *this;
}
void EnvironmentCubemapPass::OnDestroy(NSRenderer::Ctx rendererCtx)
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
void EnvironmentCubemapPass::Execute(NSScene::IScene& _scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSDX12::GraphicsCommandList cmdList)
{
    if (not im_isEnabled) return;

    Scene& scene = static_cast<Scene&>(_scene);

    blackboard.Set<NSDescriptor::Handle&>(NSRenderer::kEnvCubemap_brdfLUTsrv, m_brdfSRVhandle);

    using namespace DirectX;

    auto optRegModels = blackboard.GetOpt<EntityMap<NSRenderer::Model>>(NSRenderer::kRenderer_models);
    ASSERT(optRegModels.has_value());

    EntityMap<NSRenderer::Model>& regModels = optRegModels->get();

    scene.m_models.ForEach([&](EntityID, std::shared_ptr<::Model> model) -> LoopCondition
    {
        std::shared_ptr<NSRenderer::Model> regModel = regModels.Get(model->m_registerKey);

        if (not model->m_flags.HasLeastOne(NSModel::EModelFlag::GENERATE_ENV_CUBEMAP)) return ELoopConditionFlag::CONTINUE;

        if (regModel->isDirty)
        {
            Capture(DE_REF(model), scene, blackboard, rendererCtx, cmdList);
            return ELoopConditionFlag::CONTINUE;
        }

        for (NSRenderer::Model::Neighbor& neighbor : regModel->objsInFrustum)
        {
            if (not NSMath::Float3Equals(neighbor.position, model->GetPosition()))
            {
                regModel->isDirty = true;
            }
        }

        return ELoopConditionFlag::CONTINUE;
    });
}

void EnvironmentCubemapPass::OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx rendererCtx){}

void EnvironmentCubemapPass::Capture(Model& inModel, NSScene::IScene& _scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSDX12::GraphicsCommandList cmdList)
{
    Scene& scene = static_cast<Scene&>(_scene);

    auto optRegModels = blackboard.GetOpt<EntityMap<NSRenderer::Model>>(NSRenderer::kRenderer_models);
    ASSERT(optRegModels.has_value(), "Renderer models is invalid");

    EntityMap<NSRenderer::Model>& regModels = optRegModels->get();
    std::shared_ptr<NSRenderer::Model> regInModel = regModels.Get(inModel.m_registerKey);

    DirectX::XMFLOAT3 pos = inModel.GetPosition();

    {
        D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            regInModel->m_envCubemap.cubemapTexture.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );
        cmdList.ResourceBarrier(1, &barrier);
    }

    CD3DX12_VIEWPORT viewport(0.f, 0.f, regInModel->m_envCubemap.PER_FACE_RESOLUTION, regInModel->m_envCubemap.PER_FACE_RESOLUTION);
    CD3DX12_RECT scissor(0L, 0L, regInModel->m_envCubemap.PER_FACE_RESOLUTION, regInModel->m_envCubemap.PER_FACE_RESOLUTION);

    cmdList.RSSetViewports(1, &viewport);
    cmdList.RSSetScissorRects(1, &scissor);

    for (size_t face{}; face < regInModel->m_envCubemap.NUM_FACES; face++)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rendererCtx.offsetRTV(regInModel->m_envCubemap.rtvHandle, static_cast<UINT>(face)).cpuAddr;
        cmdList.OMSetRenderTargets(1, &rtvHandle, FALSE, &regInModel->m_envCubemap.dsvHandle.cpuAddr);

        const float clearColor[] = {0.f, 0.f, 0.f, 1.f};
        cmdList.ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
        cmdList.ClearDepthStencilView(regInModel->m_envCubemap.dsvHandle.cpuAddr, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

        std::shared_ptr<NSScene::Camera> camera = scene.m_cameras.Get(inModel.envCameraKeys[face]);

        NSAllocator::Ctx captureCBAC = rendererCtx.constAlloc(sizeof(EnvCaptureConstants));
        {
            EnvCaptureConstants& constants = captureCBAC.As<EnvCaptureConstants>();
            DirectX::XMStoreFloat4x4(&constants.view, camera->viewMatrix);
            DirectX::XMStoreFloat4x4(&constants.proj, camera->projMatrix);
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
            ASSERT(optAtmosConstDefault.has_value());

            NSAllocator::Ctx atmosCBCA = rendererCtx.constAlloc(sizeof(AtmosphereConstants));
            AtmosphereConstants& atmosCB = atmosCBCA.As<AtmosphereConstants>();
            atmosCB = optAtmosConstDefault->get();

            auto atmosphereSRVs = blackboard.GetOpt<NSDescriptor::Offset>(NSRenderer::kAtmosphere_transmitScatterSRV);
            ASSERT(atmosphereSRVs.has_value());

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

            std::shared_ptr<NSScene::CullResult> cullResult = scene.Cull(camera->m_id, NSScene::EIncCullFlag::STATIC_OBJECTS | NSScene::EIncCullFlag::DYNAMIC_OBJECTS, inModel.m_id);

            for (NSMath::ICullable culled : cullResult->m_culledObjects)
            {
                ASSERT(scene.m_models.Contains(culled.ICullable_Id));
                std::shared_ptr<::Model> sceneModel = scene.m_models.Get(culled.ICullable_Id);

                ASSERT(regModels.Contains(sceneModel->m_registerKey));
                std::shared_ptr<NSRenderer::Model> regModel = regModels.Get(sceneModel->m_registerKey);

                if (regModel->m_flags.HasLeastOne(NSRenderer::ERegModelFlag::UNSEEN_TO_ENV_CAPTURE)) continue;

                sceneModel->Draw([this, &rendererCtx, &cmdList](Mesh& mesh, UINT meshIndex, DirectX::XMMATRIX worldMatrix) -> LoopCondition
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

                    return ELoopConditionFlag::CONTINUE;
                });
            }
        }
    }

    {
        D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            regInModel->m_envCubemap.cubemapTexture.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );
        cmdList.ResourceBarrier(1, &barrier);
    }

    PrefilterCubemap(regInModel->m_envCubemap, rendererCtx, cmdList);

    regInModel->isDirty = false;

    auto mainRTV = blackboard.GetOpt<D3D12_CPU_DESCRIPTOR_HANDLE>(NSRenderer::kRenderer_mainRTV);
    auto mainDSV = blackboard.GetOpt<D3D12_CPU_DESCRIPTOR_HANDLE>(NSRenderer::kRenderer_mainDSV);
    ASSERT(mainRTV.has_value() and mainDSV.has_value());

    cmdList.OMSetRenderTargets(1, &mainRTV->get(), FALSE, &mainDSV->get());
}
void EnvironmentCubemapPass::PrefilterCubemap(NSRenderer::EnvironmentCubemap& envMap, NSRenderer::Ctx rendererCtx, NSDX12::GraphicsCommandList cmdList)
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
void EnvironmentCubemapPass::GenerateBRDFLUT(NSRenderer::Ctx rendererCtx, NSDX12::GraphicsCommandList cmdList)
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

TerrainPass::TerrainPass(ID3D12Device14* device, Blackboard& blackboard, NSRenderer::Ctx rendererCtx) : m_device(device)
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
    dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;

    CD3DX12_BLEND_DESC blendDesc(D3D12_DEFAULT);

    TESSELLATION_PIPELINE_STATE_DESC desc{};
    desc.SampleMask = UINT_MAX;
    desc.InputLayout = { inputElements, _countof(inputElements) };
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    desc.SampleDesc.Count = 1;

    ShaderMetadata vertexMetadata = { L"TerrainVS.hlsl",
        {
            L"-E", L"VS_Terrain",
            L"-T", L"vs_6_6",
            L"-Zi",
            L"-Od"
        },
    };
    ShaderMetadata hullMetadata = { L"TerrainHS.hlsl",
        {
            L"-E", L"HS_Terrain",
            L"-T", L"hs_6_6",
            L"-Zi",
            L"-Od"
        },
    };
    ShaderMetadata domainMetadata = { L"TerrainDS.hlsl",
        {
            L"-E", L"DS_Terrain",
            L"-T", L"ds_6_6",
            L"-Zi",
            L"-Od"
        },
    };
    ShaderMetadata pixelMetadata = {
        L"TerrainPS.hlsl",
        {
            L"-E", L"PS_Terrain",
            L"-T", L"ps_6_6",
            L"-Zi",
            L"-Od"
        },
    };
    this->m_solidPipeline = TessellationPipeline(device, L"", m_rootSignature).Init(
        desc,
        rasterDesc, dsDesc, blendDesc,
        vertexMetadata, hullMetadata, domainMetadata, pixelMetadata
    );

    rasterDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
    rasterDesc.CullMode = D3D12_CULL_MODE_NONE;
    m_wireframePipeline = TessellationPipeline(device, L"", m_rootSignature).Init(
        desc,
        rasterDesc, dsDesc, blendDesc,
        vertexMetadata, hullMetadata, domainMetadata, pixelMetadata
    );
}
TerrainPass::~TerrainPass()
{

};

TerrainPass& TerrainPass::OnInit(Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSDX12::GraphicsCommandList cmdList, IWICImagingFactory2* wicFactory)
{

    m_textures[static_cast<size_t>(ETexture::GRASS)] = LoadTexture(L"TerrainPass::GrassTex", L"terrain/grass.jpg", NSTexture::EType::DIFFUSE);
    m_textures[static_cast<size_t>(ETexture::ROCK)] = LoadTexture(L"TerrainPass::RockTex", L"terrain/rock.jpg", NSTexture::EType::DIFFUSE);
    m_textures[static_cast<size_t>(ETexture::SNOW)] = LoadTexture(L"TerrainPass::SnowTex", L"terrain/snow.jpg", NSTexture::EType::DIFFUSE);
    m_textures[static_cast<size_t>(ETexture::DIRT)] = LoadTexture(L"TerrainPass::DirtTex", L"terrain/dirt.jpg", NSTexture::EType::DIFFUSE);
    //m_textures[static_cast<size_t>(ETexture::TERRAIN_DIFFUSE)] = NSTexture::LoadTextureWIC(L"TerrainPass::TerrainDiffuseTex", L"terrain/diffuse.png", NSTexture::WICLoadDesc {
    //    .texDesc = {
    //        .textureType = NSTexture::EType::EType_DIFFUSE,
    //        .localRowPitchMode = NSTexture::ERowPitchMode::TIGHT,
    //        .uploadRowPitchMode = NSTexture::ERowPitchMode::DX_ALIGN,
    //    }
    //});

    m_srvHandle = rendererCtx.allocSRVStatic(static_cast<uint32_t>(ETexture::MAX));
    m_textures[static_cast<size_t>(ETexture::GRASS)].srvOffset = rendererCtx.offsetSRV(m_srvHandle, static_cast<uint32_t>(ETexture::GRASS));
    m_textures[static_cast<size_t>(ETexture::ROCK)].srvOffset = rendererCtx.offsetSRV(m_srvHandle, static_cast<uint32_t>(ETexture::ROCK));
    m_textures[static_cast<size_t>(ETexture::SNOW)].srvOffset = rendererCtx.offsetSRV(m_srvHandle, static_cast<uint32_t>(ETexture::SNOW));
    m_textures[static_cast<size_t>(ETexture::DIRT)].srvOffset = rendererCtx.offsetSRV(m_srvHandle, static_cast<uint32_t>(ETexture::DIRT));
    //m_textures[static_cast<size_t>(ETexture::TERRAIN_DIFFUSE)].srvOffset = rendererCtx.offsetSRV(m_srvHandle, static_cast<uint32_t>(ETexture::TERRAIN_DIFFUSE));

    m_textures[static_cast<size_t>(ETexture::GRASS)].PopulateCPU(true).PopulateGPU(cmdList);
    m_textures[static_cast<size_t>(ETexture::ROCK)].PopulateCPU(true).PopulateGPU(cmdList);
    m_textures[static_cast<size_t>(ETexture::SNOW)].PopulateCPU(true).PopulateGPU(cmdList);
    m_textures[static_cast<size_t>(ETexture::DIRT)].PopulateCPU(true).PopulateGPU(cmdList);
    //m_textures[static_cast<size_t>(ETexture::TERRAIN_DIFFUSE)].PopulateCPU(true).PopulateGPU(cmdList);

    //NSTexture::Texture& diffuse = m_textures[static_cast<size_t>(ETexture::TERRAIN_DIFFUSE)];
    //ASSERT(diffuse.defaultBuffer);
    //ASSERT(diffuse.uploadBuffer);
    //ASSERT(diffuse.m_isOnCPU);
    //ASSERT(diffuse.desc.format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

    //ASSERT(diffuse.width == 4097u);
    //ASSERT(diffuse.height == 4097u);

    //ASSERT(diffuse.localRowPitch >= diffuse.width * diffuse.desc.bytesPerPixel);
    //ASSERT(diffuse.uploadRowPitch % D3D12_TEXTURE_DATA_PITCH_ALIGNMENT == 0);

    return *this;
};
void TerrainPass::OnDestroy(NSRenderer::Ctx rendererCtx)
{
    rendererCtx.freeSRVStatic(m_srvHandle);
    m_srvHandle = {};

    std::for_each(m_textures.begin(), m_textures.end(), [](NSTexture::Texture& tex)
    {
        tex = {};
    });

    m_rootSignature.Reset();
    m_solidPipeline.Reset();
    m_wireframePipeline.Reset();
};

void TerrainPass::Execute(NSScene::IScene& _scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSDX12::GraphicsCommandList cmdList)
{
    if (not this->im_isEnabled) return;

    Scene& scene = static_cast<Scene&>(_scene);

    if (not scene.m_terrain.isInitialized) return;

    auto optTerrainRef = blackboard.GetOpt
        <std::reference_wrapper
        <const NSTerrain::ITerrainView>>(NSRenderer::kRenderer_terrain
    );

    ASSERT(optTerrainRef.has_value(), "TerrainPass must have terrain access through blackboard");

    UINT width = IApp::GetInstance()->im_width;
    UINT height = IApp::GetInstance()->im_height;
    CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.f, 0.f, static_cast<FLOAT>(width), static_cast<FLOAT>(height));
    RECT scissor = CD3DX12_RECT(0L, 0L, static_cast<LONG>(width), static_cast<LONG>(height));

    cmdList.RSSetViewports(1, &viewport);
    cmdList.RSSetScissorRects(1, &scissor);

    std::shared_ptr<NSScene::Camera> camera = scene.GetMainCamera();
    std::shared_ptr<NSScene::CullResult> cullResult = scene.Cull(camera->m_id, NSScene::EIncCullFlag::TERRAIN);

    const NSTerrain::ITerrainView& terrain = optTerrainRef->get().get();
    const EntityMap<NSTerrain::TerrainPage>& regPages = terrain.GetPages();
    const EntityMap<NSScene::TerrainPage>& scePages = scene.m_terrain.pages;

    if (rendererCtx.rendererFlagHasLeastOne(NSRenderer::ERendererFlag::MODE_WIREFRAME))
    {
        m_wireframePipeline.Bind(cmdList);
    }
    else
    {
        m_solidPipeline.Bind(cmdList);
    }

    cmdList.IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);

    NSAllocator::Ctx frameCBAC = rendererCtx.constAlloc(sizeof(FrameConstants));
    {
        using namespace DirectX;

        FrameConstants& frameCB = frameCBAC.As<FrameConstants>();

        XMStoreFloat4x4(&frameCB.view, camera->viewMatrix);
        XMStoreFloat4x4(&frameCB.proj, camera->projMatrix);
        XMStoreFloat3(&frameCB.eye, camera->camEye);
        XMStoreFloat4(&frameCB.lightDir, scene.m_lightDir);
        XMStoreFloat4(&frameCB.lightColor, scene.m_lightColor);
    }
    cmdList.SetGraphicsRootConstantBufferView(IDX_ROOT_CBV_FRAME, frameCBAC.gpuAddr);

    const NSTerrain::TerrainDesc& desc = terrain.GetDesc();
    const float worldTexelSpacingX = 0.f; // TODO: Placeholder. Fix it later desc.worldWidth / static_cast<float>(desc.dimention - 1u);
    const float worldTexelSpacingZ = 0.f; // TODO: Placeholder. Fix it later desc.worldDepth / static_cast<float>(desc.dimention - 1u);
    const uint32_t heightmapSrvIndex = terrain.GetHeightmapSRV().index;

    for (NSMath::ICullable& culled : cullResult->m_culledObjects)
    {
        ASSERT(scePages.Size() == regPages.Size(), "Scene and Reg vectors both must have their equavalent elements");
        ASSERT(scePages.Contains(culled.ICullable_Id), "Culled element doesn't meet with a scene page");

        std::shared_ptr<const NSScene::TerrainPage> scePage = scePages.Get(culled.ICullable_Id);

        ASSERT(regPages.Contains(scePage->m_registerKey), "Every Page keys must meet with an element");

        std::shared_ptr<const NSTerrain::TerrainPage> regPage = regPages.Get(scePage->m_registerKey);

        regPage->chunks.ForEach([&](EntityID, std::shared_ptr<const NSTerrain::TerrainChunk> chunk) -> LoopCondition
        {
            NSAllocator::Ctx allocCtx = rendererCtx.constAlloc(sizeof(TerrainConstants));
            {
                TerrainConstants& cbuffer = allocCtx.As<TerrainConstants>();
                DirectX::XMStoreFloat4x4(&cbuffer.worldMatrix, DirectX::XMMatrixIdentity());
                cbuffer.maxHeight = desc.maxHeight;
                cbuffer.worldTexelSpacingX = worldTexelSpacingX;
                cbuffer.worldTexelSpacingZ = worldTexelSpacingZ;
                cbuffer.tessFactorScale = 1.f;
                cbuffer.textureTilingFactor = 64.f;
                cbuffer.chunkUVOffset = chunk->chunkUVOffset;
                cbuffer.chunkUVScale = chunk->chunkUVScale;
                cbuffer.heightmapSrvIndex = heightmapSrvIndex;
                cbuffer.terrainDiffuseSrvIndex = m_textures[static_cast<size_t>(ETexture::TERRAIN_DIFFUSE)].srvOffset.index;
                cbuffer.splatSrvIndices[0] = m_textures[static_cast<size_t>(ETexture::GRASS)].srvOffset.index;
                cbuffer.splatSrvIndices[1] = m_textures[static_cast<size_t>(ETexture::ROCK)].srvOffset.index;
                cbuffer.splatSrvIndices[2] = m_textures[static_cast<size_t>(ETexture::SNOW)].srvOffset.index;
                cbuffer.splatSrvIndices[3] = m_textures[static_cast<size_t>(ETexture::DIRT)].srvOffset.index;
            };

            cmdList.SetGraphicsRootConstantBufferView(IDX_ROOT_CBV_TERRAIN, allocCtx.gpuAddr);

            cmdList.IASetVertexBuffers(0, 1, &chunk->vertexBufferView);
            cmdList.IASetIndexBuffer(&chunk->patchIndexBufferView);
            cmdList.DrawIndexedInstanced(chunk->patchIndexCount, 1, 0, 0, 0);

            return ELoopConditionFlag::CONTINUE;
        });
    }

};
void TerrainPass::OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx rendererCtx)
{

};

DebugPass::DebugPass(ID3D12Device14* device, Blackboard& blackboard, NSRenderer::Ctx rendererCtx) : m_device(device)
{}
DebugPass::~DebugPass()
{}
DebugPass& DebugPass::OnInit(Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSDX12::GraphicsCommandList cmdList, std::reference_wrapper<DebugUtils> utils)
{
    this->m_utils = utils;

    {
        D3D12_INPUT_ELEMENT_DESC inputElements[] =
        {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(NSDebug::Vertex, position),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"COLOR",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(NSDebug::Vertex, color),    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };

        CD3DX12_RASTERIZER_DESC rasterDesc(D3D12_DEFAULT);

        CD3DX12_DEPTH_STENCIL_DESC dsDesc(D3D12_DEFAULT);
        dsDesc.DepthEnable = FALSE;
        dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;

        CD3DX12_BLEND_DESC blendDesc(D3D12_DEFAULT);

        GRAPHICS_PIPELINE_STATE_DESC desc{};
        desc.InputLayout = { inputElements, _countof(inputElements) };
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        desc.NumRenderTargets = 1;
        desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        desc.SampleDesc.Count = 1;
        desc.SampleMask = UINT_MAX;

        this->m_linePipeline = GraphicsPipeline(m_device, L"DebugPass::m_linePipeline", [](D3D_ROOT_SIGNATURE_VERSION version, ComPtr<ID3D10Blob>& signature, ComPtr<ID3D10Blob>& error) -> HRESULT
        {
            CD3DX12_ROOT_PARAMETER1 rp[1]{};
            rp[0].InitAsConstantBufferView(0, 0);

            CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc{};
            desc.Init_1_1(_countof(rp), rp, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

            return D3DX12SerializeVersionedRootSignature(&desc, version, &signature, &error);
        }).Init(
            desc,
            rasterDesc, dsDesc, blendDesc,
            {
                L"DebugLine.hlsl",
                {
                    L"-E", L"VSMain",
                    L"-T", L"vs_6_0",
                    L"-Zi",
                    L"-Od"
                },
            },
            {
                L"DebugLine.hlsl",
                {
                    L"-E", L"PSMain",
                    L"-T", L"ps_6_0",
                    L"-Zi",
                    L"-Od"
                },
            }
        );
    }

    {
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = m_width;
        desc.Height = m_height;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE clear{};
        clear.Format = desc.Format;
        clear.Color[0] = 0.02f;
        clear.Color[1] = 0.02f;
        clear.Color[2] = 0.02f;
        clear.Color[3] = 1.00f;

        CD3DX12_HEAP_PROPERTIES props(D3D12_HEAP_TYPE_DEFAULT);
        ThrowIfFailed(m_device->CreateCommittedResource(
            &props,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COMMON,
            &clear,
            IID_PPV_ARGS(&m_color))
        );
        m_color->SetName(L"DebugPass::m_color");

        m_colorRtv = rendererCtx.allocRTVStatic(1u);

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.Format = desc.Format;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;

        m_device->CreateRenderTargetView(m_color.Get(), &rtvDesc, m_colorRtv.cpuAddr);

        {
            CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_color.Get(),
                D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
            );
            cmdList.ResourceBarrier(1, &barrier);
        }
    }

    {
        m_utils->get().ImGuiSrvDescriptorAllocFn(
            &imGuiDrawSrvCpuAddr,
            &imGuiDrawSrvGpuAddr
        );

        D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        desc.Texture2D.MipLevels = 1;

        m_device->CreateShaderResourceView(m_color.Get(), &desc, imGuiDrawSrvCpuAddr);
    }

    {
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

        m_depthDsv = rendererCtx.allocDSVStatic(1u);

        D3D12_HEAP_PROPERTIES props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        D3D12_CLEAR_VALUE clear = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 0.f, 0);
        D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, m_width, m_height, 1, 0, 1, 0,
            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL |
            D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE
        );

        ThrowIfFailed(m_device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear, IID_PPV_ARGS(&m_depth)));
        m_device->CreateDepthStencilView(m_depth.Get(), &dsvDesc, m_depthDsv.cpuAddr);
    }

    this->m_utils->get().debugPassImageSrv = imGuiDrawSrvGpuAddr;
    this->m_utils->get().debugPassImageWidth = m_width;
    this->m_utils->get().debugPassImageHeight = m_height;
    this->m_utils->get().isActive = true;

    return *this;
}
void DebugPass::OnDestroy(NSRenderer::Ctx rendererCtx)
{
    ASSERT(m_utils.has_value());

    if(m_colorRtv.amount > 0) rendererCtx.freeRTVStatic(m_colorRtv);
    m_colorRtv = {};

    if(m_depthDsv.amount > 0) rendererCtx.freeDSVStatic(m_depthDsv);
    m_depthDsv = {};

    m_color.Reset();
    m_depth.Reset();

    if (imGuiDrawSrvCpuAddr.ptr)
    {
        m_utils->get().ImGuiSrvDescriptorFreeFn(imGuiDrawSrvCpuAddr, imGuiDrawSrvGpuAddr);
    }
    this->imGuiDrawSrvCpuAddr = {};
    this->imGuiDrawSrvGpuAddr = {};

    m_utils->get().debugPassImageSrv = {};
    m_utils->get().debugPassImageWidth = 0u;
    m_utils->get().debugPassImageHeight = 0u;
    m_utils->get().isActive = false;
}

void DebugPass::Execute(NSScene::IScene& _scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSDX12::GraphicsCommandList cmdList)
{
    if (not im_isEnabled) return;

    ASSERT(m_utils);
    DebugUtils& utils = m_utils->get();
    Scene& scene = static_cast<Scene&>(_scene);

    const std::shared_ptr<NSScene::Camera> mainCam = scene.GetMainCamera();
    NSScene::Terrain& terrain = scene.GetTerrain();
    {
        using namespace DirectX;

        auto Store = [](XMVECTOR v)
        {
            XMFLOAT3 out{};
            XMStoreFloat3(&out, v);
            return out;
        };
        auto AddLine = [&utils](XMFLOAT3 a, XMFLOAT3 b, XMFLOAT4 color)
        {
            utils.m_debugLines.push_back({ { a, color }, { b, color } });
        };
        auto AddAabbTop = [&AddLine](const NSMath::SBoundAABB& aabb, XMFLOAT4 color)
        {
            const float y = aabb.max.y + 2.0f;
            const XMFLOAT3 p0{ aabb.min.x, y, aabb.min.z };
            const XMFLOAT3 p1{ aabb.max.x, y, aabb.min.z };
            const XMFLOAT3 p2{ aabb.max.x, y, aabb.max.z };
            const XMFLOAT3 p3{ aabb.min.x, y, aabb.max.z };

            AddLine(p0, p1, color);
            AddLine(p1, p2, color);
            AddLine(p2, p3, color);
            AddLine(p3, p0, color);
        };

        std::shared_ptr<NSScene::CullResult> cullResult = scene.Cull(mainCam->m_id, NSScene::EIncCullFlag::TERRAIN);

        const XMFLOAT4 hiddenChunkColor{ 0.65f, 0.12f, 0.10f, 1.0f };
        const XMFLOAT4 visibleChunkColor{ 0.10f, 0.85f, 0.25f, 1.0f };
        utils.m_debugLines.clear();

        for (NSMath::ICullable culled : cullResult->m_culledObjects)
        {
            ASSERT(terrain.pages.Contains(culled.ICullable_Id), "Invalid Terrain page key");
            std::shared_ptr<NSScene::TerrainPage> page = terrain.pages.Get(culled.ICullable_Id);

            ASSERT(std::holds_alternative<NSMath::SBoundAABB>(page->ICullable_Bound), "Terrain page has an unsupported bounding type");

            auto& aabb = std::get<NSMath::SBoundAABB>(page->ICullable_Bound);
            page->isVisibleTEMP = true;

            AddAabbTop(aabb, visibleChunkColor);
        }
        terrain.pages.ForEach([&](EntityID, std::shared_ptr<const NSScene::TerrainPage> page) -> LoopCondition
        {
            if (page->isVisibleTEMP) return ELoopConditionFlag::CONTINUE;

            auto& bound = std::get<NSMath::SBoundAABB>(page->ICullable_Bound);

            AddAabbTop(bound, hiddenChunkColor);

            return ELoopConditionFlag::CONTINUE;
        });

        const XMVECTOR frustumEye = mainCam->camEye;
        const XMVECTOR frustumFwd = XMVector3Normalize(mainCam->camFwd);
        const XMVECTOR frustumUp = XMVector3Normalize(mainCam->camUp);
        const XMVECTOR frustumRight = XMVector3Normalize(XMVector3Cross(frustumUp, frustumFwd));

        const float terrainRadius = std::sqrt(
            terrain.desc.worldWidth * terrain.desc.worldWidth +
            terrain.desc.worldDepth * terrain.desc.worldDepth
        ) * 0.5f;
        const float nearDist = std::max(XMVectorGetZ(mainCam->projMatrix.r[3]), 1.0f);
        const float farDist = std::max(terrainRadius * 1.5f, nearDist + 10.0f);
        const float projY = XMVectorGetY(mainCam->projMatrix.r[1]);
        const float projX = XMVectorGetX(mainCam->projMatrix.r[0]);
        const float aspect = projX != 0.0f ? projY / projX : 1.0f;

        auto FrustumCorner = [&](float xSign, float ySign, float dist)
        {
            const float halfH = projY != 0.0f ? dist / projY : dist;
            const float halfW = halfH * aspect;
            XMVECTOR center = XMVectorAdd(frustumEye, XMVectorScale(frustumFwd, dist));
            center = XMVectorAdd(center, XMVectorScale(frustumRight, xSign * halfW));
            center = XMVectorAdd(center, XMVectorScale(frustumUp, ySign * halfH));
            return Store(center);
        };

        const XMFLOAT3 ntl = FrustumCorner(-1.0f,  1.0f, nearDist);
        const XMFLOAT3 ntr = FrustumCorner( 1.0f,  1.0f, nearDist);
        const XMFLOAT3 nbl = FrustumCorner(-1.0f, -1.0f, nearDist);
        const XMFLOAT3 nbr = FrustumCorner( 1.0f, -1.0f, nearDist);
        const XMFLOAT3 ftl = FrustumCorner(-1.0f,  1.0f, farDist);
        const XMFLOAT3 ftr = FrustumCorner( 1.0f,  1.0f, farDist);
        const XMFLOAT3 fbl = FrustumCorner(-1.0f, -1.0f, farDist);
        const XMFLOAT3 fbr = FrustumCorner( 1.0f, -1.0f, farDist);

        const XMFLOAT4 nearColor{ 1.0f, 0.95f, 0.20f, 1.0f };
        const XMFLOAT4 farColor{ 1.0f, 0.55f, 0.10f, 1.0f };
        const XMFLOAT4 sideColor{ 1.0f, 0.80f, 0.20f, 1.0f };
        const XMFLOAT4 camColor{ 0.95f, 0.95f, 1.0f, 1.0f };

        AddLine(ntl, ntr, nearColor);
        AddLine(ntr, nbr, nearColor);
        AddLine(nbr, nbl, nearColor);
        AddLine(nbl, ntl, nearColor);
        AddLine(ftl, ftr, farColor);
        AddLine(ftr, fbr, farColor);
        AddLine(fbr, fbl, farColor);
        AddLine(fbl, ftl, farColor);
        AddLine(ntl, ftl, sideColor);
        AddLine(ntr, ftr, sideColor);
        AddLine(nbl, fbl, sideColor);
        AddLine(nbr, fbr, sideColor);

        const XMFLOAT3 camPos = Store(frustumEye);
        const XMFLOAT3 camTip = Store(XMVectorAdd(frustumEye, XMVectorScale(frustumFwd, nearDist * 8.0f)));
        const XMFLOAT3 camLeft = Store(XMVectorAdd(frustumEye, XMVectorScale(frustumRight, -nearDist * 2.0f)));
        const XMFLOAT3 camRight = Store(XMVectorAdd(frustumEye, XMVectorScale(frustumRight, nearDist * 2.0f)));
        AddLine(camLeft, camRight, camColor);
        AddLine(camPos, camTip, camColor);
    }

    if (utils.m_debugLines.empty()) return;

    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_color.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );
        cmdList.ResourceBarrier(1, &barrier);
    }

    const float clearColor[4] = { 0.02f, 0.02f, 0.02f, 1.00f };
    cmdList.ClearRenderTargetView(m_colorRtv.cpuAddr, clearColor, 0, nullptr);
    cmdList.ClearDepthStencilView(m_depthDsv.cpuAddr, D3D12_CLEAR_FLAG_DEPTH, 0.f, 0, 0, nullptr);

    CD3DX12_VIEWPORT viewport(0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height));
    CD3DX12_RECT scissor(0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height));
    cmdList.RSSetViewports(1, &viewport);
    cmdList.RSSetScissorRects(1, &scissor);

    m_linePipeline.Bind(cmdList);

    NSAllocator::Ctx frameAllocCtx = rendererCtx.constAlloc(sizeof(FrameConstants));
    FrameConstants& frameCB = frameAllocCtx.As<FrameConstants>();

    const float terrainWidth = std::max(terrain.desc.worldWidth, 1.0f);
    const float terrainDepth = std::max(terrain.desc.worldDepth, 1.0f);
    const float terrainMaxHeight = std::max(terrain.desc.maxHeight, 1.0f);

    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::lowest();

    terrain.pages.ForEach([&minX, &maxX, &minZ, &maxZ](EntityID, std::shared_ptr<const NSScene::TerrainPage> page) -> LoopCondition
    {
        ASSERT(std::holds_alternative<NSMath::SBoundAABB>(page->ICullable_Bound), "Terrain page has an unsupported bounding type");
        auto boundingBox = std::get<NSMath::SBoundAABB>(page->ICullable_Bound);

        minX = std::min(minX, boundingBox.min.x);
        maxX = std::max(maxX, boundingBox.max.x);
        minZ = std::min(minZ, boundingBox.min.z);
        maxZ = std::max(maxZ, boundingBox.max.z);

        return ELoopConditionFlag::CONTINUE;
    });

    const float centerX = (minX + maxX) * 0.5f;
    const float centerZ = (minZ + maxZ) * 0.5f;
    const float mapWidth = std::max(maxX - minX, 1.0f);
    const float mapDepth = std::max(maxZ - minZ, 1.0f);
    const float debugAspect = static_cast<float>(m_width) / static_cast<float>(m_height);
    const float orthoHeight = std::max(mapDepth, mapWidth / debugAspect) * 1.10f;
    const float orthoWidth = orthoHeight * debugAspect;
    const float debugHeight = terrainMaxHeight + std::max(orthoWidth, orthoHeight);
    const DirectX::XMVECTOR debugEye = DirectX::XMVectorSet(centerX, debugHeight, centerZ, 1.0f);
    const DirectX::XMVECTOR debugTarget = DirectX::XMVectorSet(centerX, 0.0f, centerZ, 1.0f);
    const DirectX::XMVECTOR debugUp = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    const DirectX::XMMATRIX debugView = DirectX::XMMatrixLookAtLH(debugEye, debugTarget, debugUp);
    const DirectX::XMMATRIX debugProj = DirectX::XMMatrixOrthographicLH(
        orthoWidth,
        orthoHeight,
        0.1f,
        debugHeight + terrainMaxHeight + 10.0f
    );

    DirectX::XMStoreFloat4x4(&frameCB.view, debugView);
    DirectX::XMStoreFloat4x4(&frameCB.proj, debugProj);
    cmdList.SetGraphicsRootConstantBufferView(0u, frameAllocCtx.gpuAddr);

    cmdList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

    std::vector<NSDebug::Vertex> vertices;
    vertices.reserve(utils.m_debugLines.size() * 2u);

    for (NSDebug::Line& line : utils.m_debugLines)
    {
        vertices.insert(vertices.end(), {line.a, line.b});
    }

    size_t dataSize = sizeof(NSDebug::Vertex) * vertices.size();

    NSAllocator::Ctx allocCtx = rendererCtx.constAlloc(dataSize);
    memcpy(allocCtx.cpuAddr, vertices.data(), dataSize);

    D3D12_VERTEX_BUFFER_VIEW vbv{};
    vbv.BufferLocation = allocCtx.gpuAddr;
    vbv.SizeInBytes = static_cast<UINT>(dataSize);
    vbv.StrideInBytes = sizeof(NSDebug::Vertex);

    cmdList.OMSetRenderTargets(1, &m_colorRtv.cpuAddr, false, &m_depthDsv.cpuAddr);
    cmdList.IASetVertexBuffers(0u, 1u, &vbv);
    cmdList.DrawInstanced(static_cast<UINT>(vertices.size()), 1u, 0u, 0u);

    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_color.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );
        cmdList.ResourceBarrier(1, &barrier);
    }

    auto mainRTV = blackboard.GetOpt<D3D12_CPU_DESCRIPTOR_HANDLE>(NSRenderer::kRenderer_mainRTV);
    auto mainDSV = blackboard.GetOpt<D3D12_CPU_DESCRIPTOR_HANDLE>(NSRenderer::kRenderer_mainDSV);

    ASSERT(mainRTV.has_value() and mainDSV.has_value());

    cmdList.OMSetRenderTargets(1, &mainRTV->get(), false, &mainDSV->get());

    utils.m_debugLines.clear();
}
void DebugPass::OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx rendererCtx)
{

}

ZPrePass::ZPrePass(ID3D12Device14* device, Blackboard& blackboard, NSRenderer::Ctx rendererCtx)
    : m_modelDepthPipeline(GraphicsPipeline(device, L"ZPrePass::m_modelDepthpipeline", [](D3D_ROOT_SIGNATURE_VERSION version, ComPtr<ID3D10Blob>& signature, ComPtr<ID3D10Blob>& error) -> HRESULT
    {
        CD3DX12_ROOT_PARAMETER1 rp[2]{};
        rp[IDX_MODEL_ROOT_CBV_FRAME].InitAsConstantBufferView(IDX_MODEL_CBV_FRAME, 0u);
        rp[IDX_MODEL_ROOT_CBV_MESH].InitAsConstantBufferView(IDX_MODEL_CBV_MESH, 0u);

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc{};
        desc.Init_1_1(_countof(rp), rp, 0u, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        return D3DX12SerializeVersionedRootSignature(&desc, version, &signature, &error);
    })),
    m_terrDepthPipeline(TessellationPipeline(device, L"ZPrePass::m_terrDepthPipeline", [](D3D_ROOT_SIGNATURE_VERSION version, ComPtr<ID3D10Blob>& signature, ComPtr<ID3D10Blob>& error) -> HRESULT
    {
        CD3DX12_ROOT_PARAMETER1 rp[2]{};
        rp[IDX_TERRAIN_ROOT_CBV_FRAME].InitAsConstantBufferView(IDX_TERRAIN_CBV_FRAME, 0u);
        rp[IDX_TERRAIN_ROOT_CBV_TERRAIN].InitAsConstantBufferView(IDX_TERRAIN_CBV_TERRAIN, 0u);

        D3D12_STATIC_SAMPLER_DESC sampler{};
        sampler.ShaderRegister = 1u;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.MipLODBias = 0.f;
        sampler.MaxAnisotropy = 0;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        sampler.MinLOD = 0.f;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.RegisterSpace = 0;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc{};
        desc.Init_1_1(_countof(rp), rp, 1u, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED);

        return D3DX12SerializeVersionedRootSignature(&desc, version, &signature, &error);
    }))
{}
ZPrePass::~ZPrePass()
{}

ZPrePass& ZPrePass::OnInit(Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSDX12::GraphicsCommandList cmdList)
{
    // Model Pipeline
    {
        D3D12_INPUT_ELEMENT_DESC inputElements[] =
        {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(NSModel::Vertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
        };

        CD3DX12_RASTERIZER_DESC rasterDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

        CD3DX12_DEPTH_STENCIL_DESC dsDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        dsDesc.DepthEnable = TRUE;
        dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;

        CD3DX12_BLEND_DESC blendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

        GRAPHICS_PIPELINE_STATE_DESC desc{};
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        desc.NumRenderTargets = 0;
        desc.InputLayout = { inputElements, _countof(inputElements) };
        desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        desc.SampleDesc.Count = 1;
        desc.SampleMask = UINT_MAX;

        m_modelDepthPipeline.Init(
            desc,
            rasterDesc,
            dsDesc,
            blendDesc,
            {
                L"GeometryPassVS.hlsl",
                {
                    L"-E", L"mainVS",
                    L"-T", L"vs_6_0",
                    L"-Zi",
                    L"-Od",
                    L"-D", L"DEPTH_PREPASS=1"
                }
            }
        );
    }

    // Terrain Pipeline
    {
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
        dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;

        CD3DX12_BLEND_DESC blendDesc(D3D12_DEFAULT);

        TESSELLATION_PIPELINE_STATE_DESC desc{};
        desc.SampleMask = UINT_MAX;
        desc.InputLayout = { inputElements, _countof(inputElements) };
        desc.NumRenderTargets = 0;
        desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        desc.SampleDesc.Count = 1;

        ShaderMetadata vertexMetadata = { L"TerrainVS.hlsl",
            {
                L"-E", L"VS_Terrain",
                L"-T", L"vs_6_6",
                L"-Zi",
                L"-Od",
                L"-D", L"DEPTH_PREPASS=1"
            },
        };
        ShaderMetadata hullMetadata = { L"TerrainHS.hlsl",
            {
                L"-E", L"HS_Terrain",
                L"-T", L"hs_6_6",
                L"-Zi",
                L"-Od",
                L"-D", L"DEPTH_PREPASS=1"
            },
        };
        ShaderMetadata domainMetadata = { L"TerrainDS.hlsl",
            {
                L"-E", L"DS_Terrain",
                L"-T", L"ds_6_6",
                L"-Zi",
                L"-Od",
                L"-D", L"DEPTH_PREPASS=1"
            },
        };
        this->m_terrDepthPipeline.Init(
            desc,
            rasterDesc, dsDesc, blendDesc,
            vertexMetadata, hullMetadata, domainMetadata
        );
    }

    return *this;
}
void ZPrePass::OnDestroy(NSRenderer::Ctx rendererCtx)
{
    this->m_modelDepthPipeline.Reset();
    this->m_terrDepthPipeline.Reset();
}

void ZPrePass::Execute(NSScene::IScene& _scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSDX12::GraphicsCommandList cmdList)
{
    if (not im_isEnabled) return;

    UINT width = IApp::GetInstance()->im_width;
    UINT height = IApp::GetInstance()->im_height;
    CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.f, 0.f, static_cast<FLOAT>(width), static_cast<FLOAT>(height));
    RECT scissor = CD3DX12_RECT(0L, 0L, static_cast<LONG>(width), static_cast<LONG>(height));

    cmdList.RSSetViewports(1, &viewport);
    cmdList.RSSetScissorRects(1, &scissor);

    Scene& scene = static_cast<Scene&>(_scene);

    std::shared_ptr<NSScene::Camera> mainCam = scene.GetMainCamera();

    NSAllocator::Ctx frameCBAC = rendererCtx.constAlloc(sizeof(FrameConstantsZPrepass));
    {
        using namespace DirectX;

        FrameConstantsZPrepass& frameCB = frameCBAC.As<FrameConstantsZPrepass>();


        XMStoreFloat4x4(&frameCB.view, mainCam->viewMatrix);
        XMStoreFloat4x4(&frameCB.proj, mainCam->projMatrix);
        XMStoreFloat3(&frameCB.eye, mainCam->camEye);
        frameCB.PADDING_0 = 0u;
    }

    // Terrain Pass
    {
        m_terrDepthPipeline.Bind(cmdList);
        cmdList.IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
        cmdList.SetGraphicsRootConstantBufferView(IDX_TERRAIN_ROOT_CBV_FRAME, frameCBAC.gpuAddr);

        auto optTerrainRef = blackboard.GetOpt
            <std::reference_wrapper
            <const NSTerrain::ITerrainView>>(NSRenderer::kRenderer_terrain
        );
        ASSERT(optTerrainRef.has_value(), "TerrainPass must have terrain access through blackboard");

        std::shared_ptr<NSScene::CullResult> cullResult = scene.Cull(mainCam->m_id, NSScene::EIncCullFlag::TERRAIN);

        const NSTerrain::ITerrainView& terrain = optTerrainRef->get().get();
        const EntityMap<NSTerrain::TerrainPage>& regPages = terrain.GetPages();
        const EntityMap<NSScene::TerrainPage>& scePages = scene.m_terrain.pages;

        const NSTerrain::TerrainDesc& desc = terrain.GetDesc();
        const float worldTexelSpacingX = 0.f; // TODO: Placeholder. Fix it later desc.worldWidth / static_cast<float>(desc.dimention - 1u);
        const float worldTexelSpacingZ = 0.f; // TODO: Placeholder. Fix it later desc.worldDepth / static_cast<float>(desc.dimention - 1u);
        const uint32_t heightmapSrvIndex = terrain.GetHeightmapSRV().index;

        for (NSMath::ICullable& culled : cullResult->m_culledObjects)
        {
            ASSERT(scePages.Size() == regPages.Size(), "Scene and Reg vectors both must have their equavalent elements");
            ASSERT(scePages.Contains(culled.ICullable_Id), "Culled element doesn't meet with a scene page");

            std::shared_ptr<const NSScene::TerrainPage> scePage = scePages.Get(culled.ICullable_Id);

            ASSERT(regPages.Contains(scePage->m_registerKey), "Every Page keys must meet with an element");

            std::shared_ptr<const NSTerrain::TerrainPage> regPage = regPages.Get(scePage->m_registerKey);

            regPage->chunks.ForEach([&](EntityID, std::shared_ptr<const NSTerrain::TerrainChunk> chunk) -> LoopCondition
            {
                NSAllocator::Ctx allocCtx = rendererCtx.constAlloc(sizeof(TerrainConstants));
                {
                    TerrainConstants& cbuffer = allocCtx.As<TerrainConstants>();
                    DirectX::XMStoreFloat4x4(&cbuffer.worldMatrix, DirectX::XMMatrixIdentity());
                    cbuffer.maxHeight = desc.maxHeight;
                    cbuffer.worldTexelSpacingX = worldTexelSpacingX;
                    cbuffer.worldTexelSpacingZ = worldTexelSpacingZ;
                    cbuffer.tessFactorScale = 1.f;
                    cbuffer.textureTilingFactor = 64.f;
                    cbuffer.chunkUVOffset = chunk->chunkUVOffset;
                    cbuffer.chunkUVScale = chunk->chunkUVScale;
                    cbuffer.heightmapSrvIndex = heightmapSrvIndex;
                    cbuffer.terrainDiffuseSrvIndex = rendererCtx.fallbackSRV.get().index;
                    cbuffer.splatSrvIndices[0] = rendererCtx.fallbackSRV.get().index;
                    cbuffer.splatSrvIndices[1] = rendererCtx.fallbackSRV.get().index;
                    cbuffer.splatSrvIndices[2] = rendererCtx.fallbackSRV.get().index;
                    cbuffer.splatSrvIndices[3] = rendererCtx.fallbackSRV.get().index;
                };

                cmdList.SetGraphicsRootConstantBufferView(IDX_TERRAIN_ROOT_CBV_TERRAIN, allocCtx.gpuAddr);

                cmdList.IASetVertexBuffers(0, 1, &chunk->vertexBufferView);
                cmdList.IASetIndexBuffer(&chunk->patchIndexBufferView);
                cmdList.DrawIndexedInstanced(chunk->patchIndexCount, 1, 0, 0, 0);

                return ELoopConditionFlag::CONTINUE;
            });
        }
    }

    // Model Pass
    {
        m_modelDepthPipeline.Bind(cmdList);
        cmdList.IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmdList.SetGraphicsRootConstantBufferView(IDX_MODEL_ROOT_CBV_FRAME, frameCBAC.gpuAddr);

        auto optRegModels = blackboard.GetOpt<EntityMap<NSRenderer::Model>>(NSRenderer::kRenderer_models);
        ASSERT(optRegModels.has_value());

        EntityMap<NSRenderer::Model>& regModels = optRegModels->get();

        std::shared_ptr<NSScene::CullResult> cullResult = scene.Cull(mainCam->m_id, NSScene::EIncCullFlag::STATIC_OBJECTS | NSScene::EIncCullFlag::DYNAMIC_OBJECTS);

        for (NSMath::ICullable& culled : cullResult->m_culledObjects)
        {
            ASSERT(scene.m_models.Contains(culled.ICullable_Id), "Invalid model key");
            std::shared_ptr<Model> sceneModel = scene.m_models.Get(culled.ICullable_Id);

            if (not sceneModel->m_flags.HasLeastOne(NSModel::EModelFlag::PBR_MODEL)) continue;

            ASSERT(regModels.Contains(sceneModel->m_registerKey), "Invalid model key");
            std::shared_ptr<NSRenderer::Model> regModel = regModels.Get(sceneModel->m_registerKey);

            sceneModel->Draw([this, &rendererCtx, &cmdList, &sceneModel](Mesh& mesh, UINT meshIndex, DirectX::XMMATRIX worldMatrix) -> LoopCondition
            {
                NSAllocator::Ctx allocCtx = rendererCtx.constAlloc(sizeof(MeshConstantsZPrepass));
                MeshConstantsZPrepass& meshCB = allocCtx.As<MeshConstantsZPrepass>();

                DirectX::XMStoreFloat4x4(&meshCB.worldMatrix, worldMatrix);

                cmdList.SetGraphicsRootConstantBufferView(IDX_MODEL_ROOT_CBV_MESH, allocCtx.gpuAddr);

                cmdList.IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
                cmdList.IASetIndexBuffer(&mesh.indexBufferView);
                cmdList.DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);

                return ELoopConditionFlag::CONTINUE;
            });
        }
    }
}
void ZPrePass::OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx rendererCtx)
{

}
