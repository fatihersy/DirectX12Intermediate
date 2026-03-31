#include "stdafx.h"
#include "Model.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/version.h>
#include <assimp/scene.h>

#include "IApp.h"
#include "Primitives.h"
#include "DXSampleHelper.h"
#include "Model.h"

aiMatrix4x4 GetGlobalNodeTransformation(aiNode* node)
{
    aiMatrix4x4 transform = node->mTransformation;
    aiNode* parent = node->mParent;
    while (parent) 
    {
        transform = parent->mTransformation * transform;
        parent = parent->mParent;
    }
    return transform;
}

Model::Model(ID3D12Device14* device, IWICImagingFactory2* wicFactory, const wchar_t* name) : m_device(device), m_wicFactory(wicFactory), m_name(name)
{
    assert(device and wicFactory);
}

void Model::RotateAdd(DirectX::XMFLOAT3 rotation)
{
    m_rotation.x = fmod(m_rotation.x + DirectX::XMConvertToRadians(rotation.x), DirectX::XM_2PI);
    m_rotation.y = fmod(m_rotation.y + DirectX::XMConvertToRadians(rotation.y), DirectX::XM_2PI);
    m_rotation.z = fmod(m_rotation.z + DirectX::XMConvertToRadians(rotation.z), DirectX::XM_2PI);
}

void Model::Move(DirectX::XMFLOAT3 vector, float delta)
{
    using namespace DirectX;
    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR dir = XMLoadFloat3(&vector);
    pos = XMVectorAdd(pos, XMVectorScale(dir, delta));
    XMStoreFloat3(&m_position, pos);
}

bool Model::Load(NSRenderer::Ctx rendererCtx, const std::filesystem::path& path)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path.generic_string(),
        aiProcess_Triangulate |
        aiProcess_ConvertToLeftHanded |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_JoinIdenticalVertices
    );

    assert(scene and not scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE and scene->mRootNode);

    m_assetsPath = path;

    ProcessNode(rendererCtx, scene->mRootNode, scene);

    isOnCPU = true;

    return true;
}
void Model::UploadGPU(NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
{
    assert(isOnCPU);

    if (isOnGPU) return;

    std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
    barriers.reserve(m_meshes.size() * 2u);

    for (Mesh& mesh : m_meshes)
    {
        barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
            mesh.defaultVertexBuffer.Get(),
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COPY_DEST
        ));
        barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
            mesh.defaultIndexBuffer.Get(),
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COPY_DEST
        ));
    }

    cmdList.ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

    barriers.clear();

    for (Mesh& mesh : m_meshes)
    {
        cmdList.CopyResource(mesh.defaultVertexBuffer.Get(), mesh.uploadVertexBuffer.Get());
        cmdList.CopyResource(mesh.defaultIndexBuffer.Get(), mesh.uploadIndexBuffer.Get());

        barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
            mesh.defaultVertexBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
        ));
        barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
            mesh.defaultIndexBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_INDEX_BUFFER
        ));
    }

    cmdList.ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

    for (Mesh& mesh : m_meshes)
    {
        mesh.material.UploadGPU(m_device, rendererCtx, cmdList);
    }

    m_registerKey = rendererCtx.registerModel(m_sceneKey, cmdList);

    isOnGPU = true;
}
void Model::UnloadGPU(NSRenderer::Ctx rendererCtx)
{
    if (not isOnGPU) return;

    for (Mesh& mesh : m_meshes)
    {
        mesh.defaultVertexBuffer.Reset();
        mesh.defaultIndexBuffer.Reset();
        mesh.material.UnloadGPU(rendererCtx);
    }

    rendererCtx.unloadModel(m_registerKey);
    m_registerKey = RegisterModelKey();

    isOnGPU = false;
}
void Model::ResetUploadHeaps()
{
    if (not isOnCPU) return;

    for (Mesh& mesh : m_meshes)
    {
        mesh.uploadIndexBuffer.Reset();
        mesh.uploadVertexBuffer.Reset();
        mesh.material.ResetUploadHeaps();
    }

    isOnCPU = false;
}

void Model::Draw(NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
{
    DirectX::XMMATRIX globalRotation = DirectX::XMMatrixRotationRollPitchYaw(m_rotation.x, m_rotation.y, m_rotation.z);
    DirectX::XMMATRIX globalPosition = DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&m_position));

    for (Mesh& mesh : m_meshes)
    {
        NSAllocator::Ctx allocCtx = rendererCtx.constAlloc(sizeof(meshConstants));
        meshConstants& meshCb = allocCtx.As<meshConstants>();

        const DirectX::XMMATRIX scaleMatrix = DirectX::XMMatrixScalingFromVector(DirectX::XMLoadFloat3(&mesh.m_scale));
        const DirectX::XMMATRIX rotQMatrix = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&mesh.m_rotationQ));
        const DirectX::XMMATRIX posMatrix = DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&mesh.m_position));
        const DirectX::XMMATRIX worldMatrix = scaleMatrix * rotQMatrix * posMatrix * globalRotation * globalPosition;

        DirectX::XMStoreFloat4x4(&meshCb.worldMatrix, worldMatrix);
        DirectX::XMVECTOR det;
        DirectX::XMMATRIX worldInverse = DirectX::XMMatrixInverse(&det, worldMatrix);
        DirectX::XMStoreFloat3x4(&meshCb.normalMatrix, worldInverse);

        meshCb.baseColor = mesh.material.m_baseColor;
        meshCb.metallic = mesh.material.m_metallic;
        meshCb.roughness = mesh.material.m_roughness;
        meshCb.opacity = mesh.material.m_opacity;
        meshCb.textureFlags = mesh.material.GetFlags();

        cmdList.IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
        cmdList.IASetIndexBuffer(&mesh.indexBufferView);
        cmdList.DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
    }
}
void Model::Draw(std::function<void(Mesh& mesh, UINT meshIndex, DirectX::XMMATRIX worldMatrix)> forEach)
{
    DirectX::XMMATRIX globalRotation = DirectX::XMMatrixRotationRollPitchYaw(m_rotation.x, m_rotation.y, m_rotation.z);
    DirectX::XMMATRIX globalPosition = DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&m_position));

    uint32_t meshIndex{};
    for (Mesh& mesh : m_meshes)
    {
        const DirectX::XMMATRIX scaleMatrix = DirectX::XMMatrixScalingFromVector(DirectX::XMLoadFloat3(&mesh.m_scale));
        const DirectX::XMMATRIX rotQMatrix = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&mesh.m_rotationQ));
        const DirectX::XMMATRIX posMatrix = DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&mesh.m_position));
        const DirectX::XMMATRIX worldMatrix = scaleMatrix * rotQMatrix * posMatrix * globalRotation * globalPosition;

        forEach(mesh, meshIndex, worldMatrix);

        meshIndex++;
    }
}

void Model::ProcessNode(NSRenderer::Ctx rendererCtx, aiNode* node, const aiScene* scene)
{
    assert(node and scene and m_wicFactory and "Invalid Parameter");

    if (m_meshes.size() >= IApp::ic_maxObjects)
    {
        OutputDebugStringA("Mesh count is exceeding\n");
    }

    for (uint32_t itr_000{}; itr_000 < node->mNumMeshes; itr_000++)
    {
        aiMesh* pAiMesh = scene->mMeshes[node->mMeshes[itr_000]];
        Mesh& mesh = m_meshes.emplace_back(Mesh(m_wicFactory));

        std::string meshNameStdStr = pAiMesh->mName.C_Str();
        mesh.m_name = NSTool::wformat(L"%s::mesh_%s", m_name, std::wstring(meshNameStdStr.begin(), meshNameStdStr.end()));
        mesh.material.m_name = NSTool::wformat(L"%s::material", mesh.m_name);

        ProcessMesh(pAiMesh, scene, node, mesh);
    }

    for (uint32_t itr_000{}; itr_000 < node->mNumChildren; itr_000++)
    {
        ProcessNode(rendererCtx, node->mChildren[itr_000], scene);
    }
}

void Model::ProcessMesh(aiMesh* pAiMesh, const aiScene* scene, aiNode* node, Mesh& outMesh)
{

}

Mesh& Model::CreateMeshFromMemory(const char* name, const std::vector<Vertex>& inVertices, const std::vector<UINT>& inIndices)
{

    return m_meshes.back();
}

Model&& Model::_As(NSRenderer::Ctx rendererCtx, const wchar_t* name, EPrimitive type, void* pDesc)
{

    return std::move(*this);
}
