#include "stdafx.h"
#include "RenderPass.h"
#include "Scene.h"

#include "DXSampleHelper.h"
#include "Tool.h"
#include "Renderer.h"
#include "Pipeline.h"

using namespace RenderPass;

GeometryPass::GeometryPass(ID3D12Device14* device, Blackboard& blackboard, NSRenderer::Ctx rendererCtx)
    : m_pipeline(GraphicsPipeline(device, L"GeometryPass::Graphics", [](D3D_ROOT_SIGNATURE_VERSION version, ComPtr<ID3D10Blob>& signature, ComPtr<ID3D10Blob>& error)
    {
        CD3DX12_DESCRIPTOR_RANGE1 srvRange[1]{};
        srvRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, static_cast<INT>(FTextureType::FTextureType_MAX), 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

        CD3DX12_DESCRIPTOR_RANGE1 envCubemapRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 28, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
        CD3DX12_DESCRIPTOR_RANGE1 brdfLUTRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 29, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

        CD3DX12_ROOT_PARAMETER1 rp[5]{};
        rp[0].InitAsConstantBufferView(0, 0);
        rp[1].InitAsConstantBufferView(1, 0);
        rp[2].InitAsDescriptorTable(1, &srvRange[0], D3D12_SHADER_VISIBILITY_PIXEL);
        rp[3].InitAsDescriptorTable(1, &envCubemapRange, D3D12_SHADER_VISIBILITY_PIXEL);
        rp[4].InitAsDescriptorTable(1, &brdfLUTRange, D3D12_SHADER_VISIBILITY_PIXEL);

        D3D12_STATIC_SAMPLER_DESC samplers[2]{};
        samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplers[0].AddressU = samplers[0].AddressV = samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        samplers[0].MipLODBias = 0;
        samplers[0].MaxAnisotropy = 0;
        samplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        samplers[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        samplers[0].MinLOD = 0.f;
        samplers[0].MaxLOD = D3D12_FLOAT32_MAX;
        samplers[0].ShaderRegister = 0;
        samplers[0].RegisterSpace = 0;
        samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        samplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplers[1].AddressU = samplers[1].AddressV = samplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplers[1].MipLODBias = 0;
        samplers[1].MaxAnisotropy = 0;
        samplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        samplers[1].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        samplers[1].MinLOD = 0.f;
        samplers[1].MaxLOD = D3D12_FLOAT32_MAX;
        samplers[1].ShaderRegister = 1;
        samplers[1].RegisterSpace = 0;
        samplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc;
        desc.Init_1_1(_countof(rp), rp, _countof(samplers), samplers, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        return D3DX12SerializeVersionedRootSignature(&desc, version, &signature, &error);
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
GeometryPass::~GeometryPass(){ }
void RenderPass::GeometryPass::OnInit(Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
{
}
void GeometryPass::Execute(Scene& scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
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

    auto brdfLUTSRV = blackboard.GetOpt<Descriptor::Handle>(NSRenderer::kEnvCubemap_brdfLUTSRV);
    assert(brdfLUTSRV.has_value());

    cmdList.SetGraphicsRootDescriptorTable(4, brdfLUTSRV->get().gpuAddr);
    
    auto bbModels = blackboard.GetOpt<std::vector<NSRenderer::Model>>(NSRenderer::kRenderer_models);
    assert(bbModels.has_value());

    for (size_t i = 1u; i < scene.m_models.size(); i++)
    {
        assert(scene.ValidateKeys(scene.m_models[i].m_sceneKey, scene.m_models[i].m_registerKey));

        NSRenderer::Model& bbModel = bbModels->get()[scene.m_models[i].m_registerKey.index];
        cmdList.SetGraphicsRootDescriptorTable(3, bbModel.m_envCubemap.srvHandle.gpuAddr);
        
        scene.m_models[i].Draw(rendererCtx, cmdList);
    }
}
void GeometryPass::OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx rendererCtx){}

SkyDomePass::SkyDomePass(ID3D12Device14* device, Blackboard& blackboard, NSRenderer::Ctx rendererCtx)
{
    MakeRootSignature(device, L"SkyDomePass::m_rootSignature", m_rootSignature, [](D3D_ROOT_SIGNATURE_VERSION version, ComPtr<ID3D10Blob>& signature, ComPtr<ID3D10Blob>& error)
    {
        CD3DX12_DESCRIPTOR_RANGE1 uavRange{};
        uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

        CD3DX12_DESCRIPTOR_RANGE1 srvRange{};
        srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

        CD3DX12_ROOT_PARAMETER1 rp[5]{};
        rp[IDX_ROOT_CBV_FRAME].InitAsConstantBufferView(IDX_ROOT_CBV_FRAME, 0); // Frame
        rp[IDX_ROOT_CBV_MESH].InitAsConstantBufferView(IDX_ROOT_CBV_MESH, 0); // Mesh
        rp[IDX_ROOT_CBV_ATMOSPHERE].InitAsConstantBufferView(IDX_ROOT_CBV_ATMOSPHERE, 0); // Sky dome constants
        rp[IDX_ROOT_DESC_TABLE_UAV].InitAsDescriptorTable(1, &uavRange);
        rp[IDX_ROOT_DESC_TABLE_SRV].InitAsDescriptorTable(1, &srvRange);

        D3D12_STATIC_SAMPLER_DESC sampler{};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rsDesc;
        rsDesc.Init_1_1(_countof(rp), rp, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        return D3DX12SerializeVersionedRootSignature(&rsDesc, version, &signature, &error);
    });

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

    Descriptor::hOffset transmittanceSRVoffset = rendererCtx.offsetSRV(m_srvHandle, IDX_SRV_TRANSMITTANCE);
    Descriptor::hOffset scatteringSRVoffset = rendererCtx.offsetSRV(m_srvHandle, IDX_SRV_SCATTERING);

    // Transmittance + scattering 
    blackboard.Set<Descriptor::hOffset>(NSRenderer::kSkyDome_transmitScatterSRVs, transmittanceSRVoffset);

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
        device->CreateShaderResourceView(m_trasmittanceLUT.Get(), &desc, transmittanceSRVoffset.cpuAddr);
    }
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
        desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        desc.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(m_scatteringLUT.Get(), &desc, scatteringSRVoffset.cpuAddr);
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
SkyDomePass::~SkyDomePass(){}
void RenderPass::SkyDomePass::OnInit(Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList){}

void SkyDomePass::Execute(Scene& scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
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

        blackboard.Set<skyDomeConstants>(NSRenderer::kSkyDome_constants, m_constantsDefault);

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
    cmdList.SetGraphicsRootConstantBufferView(IDX_CBV_ATMOSPHERE, skyDomeCBAC.gpuAddr);
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
    cmdList.SetComputeRootConstantBufferView(IDX_CBV_ATMOSPHERE, constantsAC.gpuAddr);
    cmdList.SetComputeRootDescriptorTable(IDX_ROOT_DESC_TABLE_UAV, rendererCtx.offsetSRV(m_srvHandle, IDX_UAV_TRANSMITTANCE).gpuAddr);
    cmdList.Dispatch(
        (256 + 7) / 8,
        (64 + 7) / 8,
        1
    );

    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_trasmittanceLUT.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
        );
        cmdList.ResourceBarrier(1, &barrier);
    }

    m_scattering.Bind(cmdList);
    cmdList.SetComputeRootConstantBufferView(IDX_CBV_ATMOSPHERE, constantsAC.gpuAddr);
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
void SkyDomePass::OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx rendererCtx){}

EnvironmentCubemapPass::EnvironmentCubemapPass(ID3D12Device14* device, Blackboard& blackboard, NSRenderer::Ctx rendererCtx)
{
    MakeRootSignature(device, L"EnvironmentCubemapPass::RootSignature", m_rootSignature, [](D3D_ROOT_SIGNATURE_VERSION version, ComPtr<ID3D10Blob>& signature, ComPtr<ID3D10Blob>& error)
    {
        CD3DX12_DESCRIPTOR_RANGE1 atmosRange = CD3DX12_DESCRIPTOR_RANGE1(
            D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE
        );
        CD3DX12_DESCRIPTOR_RANGE1 modelRange = CD3DX12_DESCRIPTOR_RANGE1(
            D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 28, 2, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE
        );

        CD3DX12_ROOT_PARAMETER1 rp[5]{};
        rp[IDX_ROOT_CBV_ATMOSPHERE].InitAsConstantBufferView(IDX_CBV_ATMOSPHERE, 0);
        rp[IDX_ROOT_CBV_CAPTURE].InitAsConstantBufferView(IDX_CBV_CAPTURE, 0);
        rp[IDX_ROOT_CBV_MESH].InitAsConstantBufferView(IDX_CBV_MESH, 0);
        rp[IDX_ROOT_DESC_MODEL_SRV].InitAsDescriptorTable(1, &modelRange, D3D12_SHADER_VISIBILITY_PIXEL);
        rp[IDX_ROOT_DESC_ATMOS_SRV].InitAsDescriptorTable(1, &atmosRange, D3D12_SHADER_VISIBILITY_PIXEL);

        D3D12_STATIC_SAMPLER_DESC samplers[2]{};
        samplers[0].ShaderRegister = 0;
        samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplers[0].AddressU = samplers[0].AddressV = samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

        samplers[1].ShaderRegister = 1;
        samplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        samplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplers[1].AddressU = samplers[1].AddressV = samplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

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

        CD3DX12_DEPTH_STENCIL_DESC depthStencil(D3D12_DEFAULT);
        depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        depthStencil.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

        CD3DX12_BLEND_DESC blend(D3D12_DEFAULT);

        GRAPHICS_PIPELINE_STATE_DESC desc{};
        desc.InputLayout = { nullptr, 0};
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        desc.NumRenderTargets = 1;
        desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
        desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        desc.SampleDesc.Count = 1;
        desc.SampleMask = UINT_MAX;

        m_skyPipeline = GraphicsPipeline(device, L"EnvironmentCubemapPass::m_skyPipeline", m_rootSignature.Get()).Init(
            desc, rasterizer, depthStencil, blend,
            L"EnvCaptureSky.hlsl",
            {
                L"-E", L"VS_EnvSky",
                L"-T", L"vs_6_0",
                L"-Zi",
                L"-Od"
            },
            L"EnvCaptureSky.hlsl",
            {
                L"-E", L"PS_EnvSky",
                L"-T", L"ps_6_0",
                L"-Zi",
                L"-Od"
            }
        );
    }

    // Geometry sub-pass pipeline
    {
        D3D12_INPUT_ELEMENT_DESC inputElements[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, normal),   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, tangent),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"BITANGENT",0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, bitangent),D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(Vertex, texCoord), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };

        CD3DX12_RASTERIZER_DESC rasterizer(D3D12_DEFAULT);
        CD3DX12_DEPTH_STENCIL_DESC depthStencil(D3D12_DEFAULT);
        CD3DX12_BLEND_DESC blend(D3D12_DEFAULT);

        GRAPHICS_PIPELINE_STATE_DESC desc{};
        desc.InputLayout = { inputElements, _countof(inputElements)};
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        desc.NumRenderTargets = 1;
        desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
        desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        desc.SampleDesc.Count = 1;
        desc.SampleMask = UINT32_MAX;

        m_geometryPipeline = GraphicsPipeline(device, L"EnvironmentCubemapPass::m_geometryPipeline", m_rootSignature.Get()).Init(
            desc, rasterizer, depthStencil, blend,
            L"EnvCaptureGeo.hlsl",
            {
                L"-E", L"VS_EnvGeo",
                L"-T", L"vs_6_0",
                L"-Zi",
                L"-Od"
            },
            L"EnvCaptureGeo.hlsl",
            {
                L"-E", L"PS_EnvGeo",
                L"-T", L"ps_6_0",
                L"-Zi",
                L"-Od"
            }
        );
    }

    // Prefilter compute pipeline
    {
        MakeRootSignature(device, L"EnvironmentCubemapPass::PrefilterRootSignature", m_prefilterRootSignature, [](D3D_ROOT_SIGNATURE_VERSION version, ComPtr<ID3D10Blob>& signature, ComPtr<ID3D10Blob>& error)
        {
            CD3DX12_DESCRIPTOR_RANGE1 srvRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
            CD3DX12_DESCRIPTOR_RANGE1 uavRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

            CD3DX12_ROOT_PARAMETER1 rp[3]{};
            rp[0].InitAsConstants(4, 0); // roughness, faceSize, numSamples, pad
            rp[1].InitAsDescriptorTable(1, &srvRange);
            rp[2].InitAsDescriptorTable(1, &uavRange);

            D3D12_STATIC_SAMPLER_DESC sampler{};
            sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            sampler.ShaderRegister = 0;

            CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rsDesc{};
            rsDesc.Init_1_1(_countof(rp), rp, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_NONE);

            return D3DX12SerializeVersionedRootSignature(&rsDesc, version, &signature, &error);
        });

        m_prefilterPipeline = ComputePipeline(device, L"EnvironmentCubemapPass::m_prefilterPipeline", m_prefilterRootSignature.Get()).Init(
            D3D12_PIPELINE_STATE_FLAG_NONE,
            L"PrefilterEnvMap.hlsl",
            {
                L"-E", L"main",
                L"-T", L"cs_6_0",
                L"-Zi",
                L"-Od"
            }
        );
    }

    // BRDF integration LUT compute pipeline
    {
        MakeRootSignature(device, L"EnvironmentCubemapPass::BRDFRootSignature", m_brdfRootSignature, [](D3D_ROOT_SIGNATURE_VERSION version, ComPtr<ID3D10Blob>& signature, ComPtr<ID3D10Blob>& error)
        {
            CD3DX12_DESCRIPTOR_RANGE1 uavRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

            CD3DX12_ROOT_PARAMETER1 rp[2]{};
            rp[0].InitAsConstants(4, 0); // resolution, numSamples, pad, pad
            rp[1].InitAsDescriptorTable(1, &uavRange);

            CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rsDesc{};
            rsDesc.Init_1_1(_countof(rp), rp, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

            return D3DX12SerializeVersionedRootSignature(&rsDesc, version, &signature, &error);
        });

        m_brdfPipeline = ComputePipeline(device, L"EnvironmentCubemapPass::m_brdfPipeline", m_brdfRootSignature.Get()).Init(
            D3D12_PIPELINE_STATE_FLAG_NONE,
            L"IntegrateBRDF.hlsl",
            {
                L"-E", L"main",
                L"-T", L"cs_6_0",
                L"-Zi",
                L"-Od"
            }
        );

        // Create BRDF LUT texture
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
            m_brdfLUT->SetName(L"EnvironmentCubemapPass::BRDF_LUT");
        }

        // BRDF LUT UAV
        {
            m_brdfUAVHandle = rendererCtx.allocSRVStatic(1u);

            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
            uavDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            uavDesc.Texture2D.MipSlice = 0;

            device->CreateUnorderedAccessView(m_brdfLUT.Get(), nullptr, &uavDesc, m_brdfUAVHandle.cpuAddr);
        }

        // BRDF LUT SRV
        {
            m_brdfSRVHandle = rendererCtx.allocSRVStatic(1u);

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Texture2D.MipLevels = 1;
            srvDesc.Texture2D.MostDetailedMip = 0;

            device->CreateShaderResourceView(m_brdfLUT.Get(), &srvDesc, m_brdfSRVHandle.cpuAddr);
        }
    }
}
EnvironmentCubemapPass::~EnvironmentCubemapPass(){}
void RenderPass::EnvironmentCubemapPass::OnInit(Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
{
    GenerateBRDFLUT(rendererCtx, cmdList);
}

void EnvironmentCubemapPass::Execute(Scene& scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
{
    if (not IsEnabled()) return;

    blackboard.Set<Descriptor::Handle>(NSRenderer::kEnvCubemap_brdfLUTSRV, m_brdfSRVHandle);

    using namespace DirectX;

    auto bbModels = blackboard.GetOpt<std::vector<NSRenderer::Model>>(NSRenderer::kRenderer_models);
    assert(bbModels.has_value());

    for (size_t itr_000 = 1u; itr_000 < bbModels->get().size(); ++itr_000)
    {
        NSRenderer::Model& bbModel = bbModels->get()[itr_000];
        Model& sceneModel = scene.m_models.at(bbModel.sceneKey.index);
        assert(scene.ValidateKeys(bbModel.sceneKey, sceneModel.m_registerKey));

        if (bbModel.isDirty)
        {
            Capture(sceneModel, scene, blackboard, rendererCtx, cmdList);
            continue;
        }
        else {
            for (size_t itr_111 = 0; itr_111 < bbModel.objsInFrustum.size(); itr_111++)
            {
                NSRenderer::Model::Neighbor& neigbor = bbModel.objsInFrustum[itr_111];

                if (not Float3Equals(neigbor.position, sceneModel.GetPosition()))
                {
                    bbModel.isDirty = true;
                }
            }
        }
    }
}
void EnvironmentCubemapPass::Capture(Model& model, Scene& scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
{
    auto bbModels = blackboard.GetOpt<std::vector<NSRenderer::Model>>(NSRenderer::kRenderer_models);
    NSRenderer::Model& bbModel = bbModels->get()[model.m_registerKey.index];

    NSRenderer::EnvironmentCubemap& envMap = bbModel.m_envCubemap;
    DirectX::XMFLOAT3 pos = model.GetPosition();

    const DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV2, 1.f, scene.NEAR_CLIP, scene.FAR_CLIP);
    Camera cams[envMap.NUM_FACES] = {
        Camera(proj, pos, { 1, 0, 0, 0 }, { 0, 1, 0, 0 }),
        Camera(proj, pos, {-1, 0, 0, 0 }, { 0, 1, 0, 0 }),
        Camera(proj, pos, { 0, 1, 0, 0 }, { 0, 0,-1, 0 }),
        Camera(proj, pos, { 0,-1, 0, 0 }, { 0, 0, 1, 0 }),
        Camera(proj, pos, { 0, 0, 1, 0 }, { 0, 1, 0, 0 }),
        Camera(proj, pos, { 0, 0,-1, 0 }, { 0, 1, 0, 0 })
    };

    for (size_t face = 0; face < envMap.NUM_FACES; ++face)
    {
        {
            D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                envMap.cubemapTexture.Get(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_RENDER_TARGET
            );
            cmdList.ResourceBarrier(1, &barrier);
        }

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rendererCtx.offsetRTV(envMap.rtvHandle, static_cast<UINT>(face)).cpuAddr;
        cmdList.OMSetRenderTargets(1, &rtvHandle, FALSE, &envMap.dsvHandle.cpuAddr);

        const float clearColor[] = { 0.f, 0.f, 0.f, 1.f };
        cmdList.ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
        cmdList.ClearDepthStencilView(envMap.dsvHandle.cpuAddr, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

        CD3DX12_VIEWPORT viewport(0.f, 0.f, envMap.PER_FACE_RESOLUTION, envMap.PER_FACE_RESOLUTION);
        CD3DX12_RECT scissor(0, 0, envMap.PER_FACE_RESOLUTION, envMap.PER_FACE_RESOLUTION);

        cmdList.RSSetViewports(1, &viewport);
        cmdList.RSSetScissorRects(1, &scissor);

        Camera& cam = cams[face];

        Allocator::AllocCtx captureCBAC = rendererCtx.allocConstBuff(sizeof(envCaptureConstants));
        {
            envCaptureConstants& cb = captureCBAC.As<envCaptureConstants>();
            DirectX::XMStoreFloat4x4(&cb.view, cam.viewMatrix);
            DirectX::XMStoreFloat4x4(&cb.proj, cam.projectionMatrix);
            DirectX::XMStoreFloat4(&cb.lightDir, scene.m_lightDir);
            DirectX::XMStoreFloat4(&cb.lightColor, scene.m_lightColor);
            DirectX::XMFLOAT3 pos = model.GetPosition();
            cb.capturePos = pos;
            cb.camPos = pos;
        }

        // Sky pass
        {
            m_skyPipeline.Bind(cmdList);
            cmdList.SetGraphicsRootConstantBufferView(IDX_ROOT_CBV_CAPTURE, captureCBAC.gpuAddr);

            auto atmosDefault = blackboard.GetOpt<skyDomeConstants>(NSRenderer::kSkyDome_constants);
            assert(atmosDefault.has_value());

            Allocator::AllocCtx atmosphereCBAC = rendererCtx.allocConstBuff(sizeof(skyDomeConstants));
            skyDomeConstants& cb = atmosphereCBAC.As<skyDomeConstants>();
            cb = atmosDefault->get();
            cmdList.SetGraphicsRootConstantBufferView(IDX_ROOT_CBV_ATMOSPHERE, atmosphereCBAC.gpuAddr);

            auto atmosphereSRVs = blackboard.GetOpt<Descriptor::hOffset>(NSRenderer::kSkyDome_transmitScatterSRVs);
            assert(atmosphereSRVs.has_value());

            cmdList.SetGraphicsRootDescriptorTable(IDX_ROOT_DESC_ATMOS_SRV, atmosphereSRVs->get().gpuAddr);

            cmdList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            cmdList.DrawInstanced(3, 1, 0, 0);
        }

        // Geometry Pass
        {
            m_geometryPipeline.Bind(cmdList);
            cmdList.SetGraphicsRootConstantBufferView(IDX_ROOT_CBV_CAPTURE, captureCBAC.gpuAddr);

            scene.CullScene(&cam, model.m_sceneKey);

            for (size_t itr = 0; itr < scene.m_modelsCulled.size(); itr++)
            {
                SceneModelKey& key = scene.m_modelsCulled[itr];
                assert(scene.ValidateKey(key));

                scene.m_models[key.index].Draw(rendererCtx, cmdList);
            }
        }

        {
            D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                envMap.cubemapTexture.Get(),
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
            );
            cmdList.ResourceBarrier(1, &barrier);
        }
    }

    PrefilterCubemap(envMap, rendererCtx, cmdList);

    bbModel.isDirty = false;

    auto mainRTV = blackboard.GetOpt<D3D12_CPU_DESCRIPTOR_HANDLE>(NSRenderer::kRenderer_mainRTV);
    auto mainDSV = blackboard.GetOpt<D3D12_CPU_DESCRIPTOR_HANDLE>(NSRenderer::kRenderer_mainDSV);
    assert(mainRTV.has_value() and mainDSV.has_value());

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = mainRTV->get();
    cmdList.OMSetRenderTargets(1, &rtv, FALSE, &mainDSV->get());
}
void EnvironmentCubemapPass::PrefilterCubemap(NSRenderer::EnvironmentCubemap& envMap, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
{
    {
        std::vector<D3D12_RESOURCE_BARRIER> barriers;
        barriers.reserve(envMap.NUM_FACES * envMap.MIP_COUNT);

        for (UINT face = 0u; face < envMap.NUM_FACES; face++)
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

    constexpr UINT numSamples = 1024u;

    for (UINT mip = 1u; mip < envMap.MIP_COUNT; mip++)
    {
        UINT mipSize = envMap.PER_FACE_RESOLUTION >> mip;
        float roughness = static_cast<float>(mip) / static_cast<float>(envMap.MIP_COUNT - 1u);

        UINT constants[4] = {};
        memcpy(&constants[0], &roughness, sizeof(float));
        constants[1] = mipSize;
        constants[2] = numSamples;
        constants[3] = 0u; // pad
        cmdList.SetComputeRoot32BitConstants(0, 4, constants, 0);

        cmdList.SetComputeRootDescriptorTable(2, rendererCtx.offsetSRV(envMap.uavHandle, mip - 1u).gpuAddr);

        UINT groupsXY = (mipSize + 7u) / 8u;
        cmdList.Dispatch(groupsXY, groupsXY, envMap.NUM_FACES);
    }

    {
        std::vector<D3D12_RESOURCE_BARRIER> barriers;
        barriers.reserve(envMap.NUM_FACES * envMap.MIP_COUNT);

        for (UINT face = 0u; face < envMap.NUM_FACES; face++)
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
    UINT constants[4] = { BRDF_LUT_SIZE, numSamples, 0u, 0u }; // resolution, numSamples, pad, pad
    cmdList.SetComputeRoot32BitConstants(0, 4, constants, 0);

    cmdList.SetComputeRootDescriptorTable(1, m_brdfUAVHandle.gpuAddr);

    UINT groups = (BRDF_LUT_SIZE + 7u) / 8u;
    cmdList.Dispatch(groups, groups, 1);

    {
        D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_brdfLUT.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );
        cmdList.ResourceBarrier(1, &barrier);
    }
}

void EnvironmentCubemapPass::OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx rendererCtx) {}
void EnvironmentCubemapPass::Destroy()
{
    m_rootSignature.Reset();
    m_prefilterRootSignature.Reset();
    m_brdfRootSignature.Reset();
    m_brdfLUT.Reset();
}
