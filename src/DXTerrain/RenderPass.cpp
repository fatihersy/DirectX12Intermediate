#include "stdafx.h"
#include "RenderPass.h"

#include "IApp.h"
#include "Scene.h"
#include "Blackboard.h"

using namespace NSRenderPass;

GeometryPass::GeometryPass(ID3D12Device14* device, Blackboard& blackboard, NSRenderer::Ctx rendererCtx)
    : m_pipeline(GraphicsPipeline(device, L"GeometryPass::Graphics", [](D3D_ROOT_SIGNATURE_VERSION version, ComPtr<ID3D10Blob>& signature, ComPtr<ID3D10Blob>& error) -> HRESULT
    {
        int textureRegisters = static_cast<INT>(NSTexture::EType::EType_MAX); // Occupies T0-27
        CD3DX12_DESCRIPTOR_RANGE1 srvRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, textureRegisters, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

        textureRegisters += 1; // Occupies T28
        CD3DX12_DESCRIPTOR_RANGE1 envCubemapRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, textureRegisters, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

        textureRegisters += 1;// Occupies T29
        CD3DX12_DESCRIPTOR_RANGE1 brdfLUTRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, textureRegisters, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

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
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(NSModel::Vertex, texCoord), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
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
GeometryPass::~GeometryPass()
{}
void GeometryPass::OnInit(Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
{}

void GeometryPass::Execute(Scene& scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
{
    if (not IsEnabled()) return;

    m_pipeline.Bind(cmdList);

    NSAllocator::Ctx frameCBAC = rendererCtx.constAlloc(sizeof(frameConstants));
    {
        using namespace DirectX;

        frameConstants& frameCB = frameCBAC.As<frameConstants>();

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

    for (size_t itr{}; itr < scene.m_models.size(); itr++)
    {
        Model& sceneModel = scene.m_models[itr];

        assert(regModels.size() > sceneModel.m_registerKey.index);

        NSRenderer::Model& regModel = regModels[sceneModel.m_registerKey.index];

        assert(scene.ValidateKeys(regModel.sceneKey, sceneModel.m_registerKey));

        cmdList.SetGraphicsRootDescriptorTable(IDX_ROOT_DESC_ENV_CUBEMAP_SRV, regModel.m_envCubemap.srvHandle.gpuAddr);

        sceneModel.Draw([this, &rendererCtx, &cmdList, &sceneModel](Mesh& mesh, UINT meshIndex, DirectX::XMMATRIX worldMatrix)
        {
            NSAllocator::Ctx allocCtx = rendererCtx.constAlloc(sizeof(meshConstants));
            meshConstants meshCB = allocCtx.As<meshConstants>();

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
            cmdList.SetGraphicsRootDescriptorTable(IDX_ROOT_DESC_MODEL_TEX_SRV, mesh.material.m_srvHandle.gpuAddr);

            cmdList.IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
            cmdList.IASetIndexBuffer(&mesh.indexBufferView);
            cmdList.DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
        });
    }
}
void GeometryPass::OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx rendererCtx)
{}
