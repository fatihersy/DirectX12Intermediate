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

Model::Model() : m_device(nullptr), m_wicFactory(nullptr) {};
Model::Model(ID3D12Device14* device, IWICImagingFactory2* wicFactory, std::wstring_view name) : m_device(device), m_wicFactory(wicFactory), m_name(name)
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

    assert(scene and (not (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)) and scene->mRootNode);

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
    m_registerKey = NSModel::RegisterModelKey();

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

        std::string meshNameStdStr(pAiMesh->mName.C_Str());
        std::wstring meshNameStdWStr(meshNameStdStr.begin(), meshNameStdStr.end());

        mesh.m_name = NSTool::wformat(L"%s::mesh_%s", m_name, meshNameStdWStr);
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
    assert(m_device and pAiMesh and scene);

    std::vector<NSModel::Vertex> vertices;
    std::vector<UINT> indices;

    outMesh.vertexCount = static_cast<UINT>(pAiMesh->mNumVertices);
    vertices.reserve(outMesh.vertexCount);

    for (size_t itr_000{}; itr_000 < pAiMesh->mNumFaces; itr_000++)
    {
        outMesh.indexCount += pAiMesh->mFaces[itr_000].mNumIndices;
    }
    indices.reserve(outMesh.indexCount);

    aiMatrix4x4 aiGlobalTransform = GetGlobalNodeTransformation(node);

    DirectX::XMMATRIX globalMatrix = DirectX::XMMatrixSet(
        aiGlobalTransform.a1, aiGlobalTransform.b1, aiGlobalTransform.c1, aiGlobalTransform.d1,
        aiGlobalTransform.a2, aiGlobalTransform.b2, aiGlobalTransform.c2, aiGlobalTransform.d2,
        aiGlobalTransform.a3, aiGlobalTransform.b3, aiGlobalTransform.c3, aiGlobalTransform.d3,
        aiGlobalTransform.a4, aiGlobalTransform.b4, aiGlobalTransform.c4, aiGlobalTransform.d4
    );

    DirectX::XMVECTOR outPos = DirectX::XMVectorZero();
    DirectX::XMVECTOR outRotQ = DirectX::XMVectorZero();
    DirectX::XMVECTOR outScale = DirectX::XMVectorZero();

    assert(DirectX::XMMatrixDecompose(&outScale, &outRotQ, &outPos, globalMatrix) and "Cannot decompose mesh matrix");

    DirectX::XMStoreFloat3(&outMesh.m_position, outPos);
    DirectX::XMStoreFloat4(&outMesh.m_rotationQ, outRotQ);
    DirectX::XMStoreFloat3(&outMesh.m_scale, outScale);

    for (UINT itr_000{}; itr_000 < pAiMesh->mNumVertices; itr_000++)
    {
        NSModel::Vertex v{};

        v.position = DirectX::XMFLOAT3
        {
            pAiMesh->mVertices[itr_000].x,
            pAiMesh->mVertices[itr_000].y,
            pAiMesh->mVertices[itr_000].z
        };
        v.normal = pAiMesh->HasNormals() ? DirectX::XMFLOAT3
        {
            pAiMesh->mNormals[itr_000].x,
            pAiMesh->mNormals[itr_000].y,
            pAiMesh->mNormals[itr_000].z
        }
        : DirectX::XMFLOAT3{};

        v.texCoord = pAiMesh->mTextureCoords[0] ? DirectX::XMFLOAT2
        {
            pAiMesh->mTextureCoords[0][itr_000].x,
            pAiMesh->mTextureCoords[0][itr_000].y
        }
        : DirectX::XMFLOAT2{};

        v.tangent = pAiMesh->HasTangentsAndBitangents() ? DirectX::XMFLOAT3
        {
            pAiMesh->mTangents[itr_000].x,
            pAiMesh->mTangents[itr_000].y,
            pAiMesh->mTangents[itr_000].z
        }
        : DirectX::XMFLOAT3{1.f, 0.f, 0.f};

        v.bitangent = pAiMesh->HasTangentsAndBitangents() ? DirectX::XMFLOAT3
        {
            pAiMesh->mBitangents[itr_000].x,
            pAiMesh->mBitangents[itr_000].y,
            pAiMesh->mBitangents[itr_000].z
        }
        : DirectX::XMFLOAT3{0.f, 1.f, 0.f};

        {
            using namespace DirectX;
            XMVECTOR T = XMLoadFloat3(&v.tangent);
            XMVECTOR B = XMLoadFloat3(&v.bitangent);
            XMVECTOR N = XMLoadFloat3(&v.normal);
            XMVECTOR det = XMVector3Dot(N, XMVector3Cross(T,B));
            if (XMVectorGetX(XMVectorLess(det, XMVectorZero())) > 0) // Check Left handedness
            {
                v.bitangent.x = -v.bitangent.x;
                v.bitangent.y = -v.bitangent.y;
                v.bitangent.z = -v.bitangent.z;
            }
        }

        vertices.push_back(v);
    }

    for (UINT itr_000{}; itr_000 < pAiMesh->mNumFaces; itr_000++)
    {
        aiFace face = pAiMesh->mFaces[itr_000];
        for (UINT itr_111{}; itr_111 < face.mNumIndices; itr_111++)
        {
            indices.push_back(face.mIndices[itr_111]);
        }
    }

    {
        const CD3DX12_HEAP_PROPERTIES uploadProps(D3D12_HEAP_TYPE_UPLOAD);
        const CD3DX12_HEAP_PROPERTIES defaultProps(D3D12_HEAP_TYPE_DEFAULT);

        const size_t dataSize = outMesh.vertexCount * sizeof(NSModel::Vertex);
        const CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(dataSize);

        ThrowIfFailed(m_device->CreateCommittedResource(
            &uploadProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&outMesh.uploadVertexBuffer)
        ));
        outMesh.uploadVertexBuffer->SetName(NSTool::wformat(L"%s::%s", m_name, L"uploadVertexBuffer").c_str());

        void* ppData = nullptr;
        ThrowIfFailed(outMesh.uploadVertexBuffer->Map(0u, nullptr, reinterpret_cast<void**>(&ppData)));

        memcpy(ppData, vertices.data(), dataSize);
        outMesh.uploadVertexBuffer->Unmap(0u, nullptr);

        ThrowIfFailed(m_device->CreateCommittedResource(
            &defaultProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&outMesh.defaultVertexBuffer)
        ));
        outMesh.defaultVertexBuffer->SetName(NSTool::wformat(L"%s::%s", m_name, L"defaultVertexBuffer").c_str());

        outMesh.vertexBufferView.BufferLocation = outMesh.defaultVertexBuffer->GetGPUVirtualAddress();
        outMesh.vertexBufferView.SizeInBytes = static_cast<UINT>(dataSize);
        outMesh.vertexBufferView.StrideInBytes = sizeof(NSModel::Vertex);
    }

    {
        const CD3DX12_HEAP_PROPERTIES uploadProps(D3D12_HEAP_TYPE_UPLOAD);
        const CD3DX12_HEAP_PROPERTIES defaultProps(D3D12_HEAP_TYPE_DEFAULT);

        const size_t dataSize = outMesh.indexCount * sizeof(UINT);
        const CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(dataSize);

        ThrowIfFailed(m_device->CreateCommittedResource(
            &uploadProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&outMesh.uploadIndexBuffer)
        ));
        outMesh.uploadIndexBuffer->SetName(NSTool::wformat(L"%s::%s", m_name, L"uploadIndexBuffer").c_str());

        void* ppData = nullptr;
        ThrowIfFailed(outMesh.uploadIndexBuffer->Map(0u, nullptr, reinterpret_cast<void**>(&ppData)));

        memcpy(ppData, indices.data(), dataSize);
        outMesh.uploadIndexBuffer->Unmap(0u, nullptr);

        ThrowIfFailed(m_device->CreateCommittedResource(
            &defaultProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&outMesh.defaultIndexBuffer)
        ));
        outMesh.defaultIndexBuffer->SetName(NSTool::wformat(L"%s::%s", m_name, L"defaultIndexBuffer").c_str());

        outMesh.indexBufferView.BufferLocation = outMesh.defaultIndexBuffer->GetGPUVirtualAddress();
        outMesh.indexBufferView.SizeInBytes = static_cast<UINT>(dataSize);
        outMesh.indexBufferView.Format = DXGI_FORMAT_R32_UINT;
    }

    if (pAiMesh->mMaterialIndex >= 0)
    {
        aiMaterial* material = scene->mMaterials[pAiMesh->mMaterialIndex];

        aiString matName;
        if (material->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS)
        {
            outMesh.material.m_name = NSTool::wformat(L"", outMesh.material.m_name, std::wstring(matName.C_Str(), matName.C_Str() + matName.length).c_str());
        }

        aiColor4D baseColor{};
        if (material->Get(AI_MATKEY_BASE_COLOR, baseColor) == AI_SUCCESS)
        {
            outMesh.material.m_baseColor = DirectX::XMFLOAT4(baseColor.r, baseColor.g, baseColor.b, baseColor.a);
        }

        float metallic{};
        if (material->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS)
        {
            outMesh.material.m_metallic = metallic;
        }

        float roughness{};
        if (material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS)
        {
            outMesh.material.m_roughness = roughness;
        }

        float opacity{};
        if (material->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS)
        {
            outMesh.material.m_opacity = opacity;
        }

        for (UINT type{}; type < AI_TEXTURE_TYPE_MAX; type++)
        {
            if (not (material->GetTextureCount(static_cast<aiTextureType>(type)) > 0)) continue;
            
            aiString path;
            if (material->GetTexture(static_cast<aiTextureType>(type), 0u, &path) == aiReturn_SUCCESS)
            {
                assert(not path.Empty());

                const aiTexture* embeddedTex = scene->GetEmbeddedTexture(path.C_Str());

                ComPtr<IWICBitmapDecoder> decoder;

                if (embeddedTex)
                {
                    if(embeddedTex->mHeight <= 0) continue;

                    ComPtr<IWICStream> stream;
                    ThrowIfFailed(m_wicFactory->CreateStream(&stream));
                    ThrowIfFailed(stream->InitializeFromMemory(reinterpret_cast<BYTE*>(embeddedTex->pcData), embeddedTex->mWidth));
                    ThrowIfFailed(m_wicFactory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &decoder));
                }
                else {
                    std::wstring directory = m_assetsPath
                        .parent_path()
                        .generic_wstring()
                        .append(L"/")
                        .append(path.C_Str(), path.C_Str() + path.length);

                    ThrowIfFailed(m_wicFactory->CreateDecoderFromFilename(directory.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder));
                }

                outMesh.material.LoadTexture(m_device, decoder.Get(), static_cast<aiTextureType>(type));
            }
        }
    }
}

Mesh& Model::CreateMeshFromMemory(std::wstring_view name, const std::vector<NSModel::Vertex>& inVertices, const std::vector<UINT>& inIndices)
{
    IApp* iApp = IApp::GetInstance();

    assert(iApp and not name.empty() and not inVertices.empty() and not inIndices.empty());

    Mesh& outMesh = m_meshes.emplace_back(Mesh(m_wicFactory));
    outMesh.m_name = NSTool::wformat(L"%s::%s", m_name, name);
    outMesh.material.m_name = NSTool::wformat(L"%s::material", m_name);
    outMesh.vertexCount = static_cast<UINT>(inVertices.size());
    outMesh.indexCount = static_cast<UINT>(inIndices.size());

    {
        const CD3DX12_HEAP_PROPERTIES uploadProps(D3D12_HEAP_TYPE_UPLOAD);
        const CD3DX12_HEAP_PROPERTIES defaultProps(D3D12_HEAP_TYPE_DEFAULT);

        const size_t dataSize = outMesh.vertexCount * sizeof(NSModel::Vertex);
        const CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(dataSize);

        ThrowIfFailed(m_device->CreateCommittedResource(
            &uploadProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&outMesh.uploadVertexBuffer)
        ));
        outMesh.uploadVertexBuffer->SetName(NSTool::wformat(L"%s::%s", outMesh.m_name, L"uploadVertexBuffer").c_str());

        void* ppData = nullptr;
        ThrowIfFailed(outMesh.uploadVertexBuffer->Map(0u, nullptr, reinterpret_cast<void**>(&ppData)));

        memcpy(ppData, inVertices.data(), dataSize);
        outMesh.uploadVertexBuffer->Unmap(0u, nullptr);

        ThrowIfFailed(m_device->CreateCommittedResource(
            &defaultProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&outMesh.defaultVertexBuffer)
        ));
        outMesh.defaultVertexBuffer->SetName(NSTool::wformat(L"%s::%s", outMesh.m_name, L"defaultVertexBuffer").c_str());

        outMesh.vertexBufferView.BufferLocation = outMesh.defaultVertexBuffer->GetGPUVirtualAddress();
        outMesh.vertexBufferView.SizeInBytes = static_cast<UINT>(dataSize);
        outMesh.vertexBufferView.StrideInBytes = sizeof(NSModel::Vertex);
    }

    {
        const CD3DX12_HEAP_PROPERTIES uploadProps(D3D12_HEAP_TYPE_UPLOAD);
        const CD3DX12_HEAP_PROPERTIES defaultProps(D3D12_HEAP_TYPE_DEFAULT);

        const size_t dataSize = outMesh.indexCount * sizeof(UINT);
        const CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(dataSize);

        ThrowIfFailed(m_device->CreateCommittedResource(
            &uploadProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&outMesh.uploadIndexBuffer)
        ));
        outMesh.uploadIndexBuffer->SetName(NSTool::wformat(L"%s::%s", outMesh.m_name, L"uploadIndexBuffer").c_str());

        void* ppData = nullptr;
        ThrowIfFailed(outMesh.uploadIndexBuffer->Map(0u, nullptr, reinterpret_cast<void**>(&ppData)));

        memcpy(ppData, inIndices.data(), dataSize);
        outMesh.uploadIndexBuffer->Unmap(0u, nullptr);

        ThrowIfFailed(m_device->CreateCommittedResource(
            &defaultProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&outMesh.defaultIndexBuffer)
        ));
        outMesh.defaultIndexBuffer->SetName(NSTool::wformat(L"%s::%s", outMesh.m_name, L"defaultIndexBuffer").c_str());

        outMesh.indexBufferView.BufferLocation = outMesh.defaultIndexBuffer->GetGPUVirtualAddress();
        outMesh.indexBufferView.SizeInBytes = static_cast<UINT>(dataSize);
        outMesh.indexBufferView.Format = DXGI_FORMAT_R32_UINT;
    }

    return outMesh;
}

Model&& Model::_As(NSRenderer::Ctx rendererCtx, std::wstring_view name, NSModel::EPrimitive type, void* pDesc)
{
    assert(pDesc and not name.empty() and type > NSModel::EPrimitive::PRIMITIVE_TYPE_NONE and type < NSModel::EPrimitive::PRIMITIVE_TYPE_MAX);

    UnloadGPU(rendererCtx);
    ResetUploadHeaps();
    std::vector<NSModel::Vertex> vertices;
    std::vector<UINT> indices;

    switch (type)
    {
    case NSModel::EPrimitive::PRIMITIVE_TYPE_DOME:
    {
        const NSModel::SDome* desc = reinterpret_cast<const NSModel::SDome*>(pDesc);

        Primitives::CreateDome(desc->radius, desc->sliceCount, desc->stackCount, vertices, indices);
        Mesh& mesh = CreateMeshFromMemory(name, vertices, indices);

        return std::move(*this);
    }
    case NSModel::EPrimitive::PRIMITIVE_TYPE_SPHERE:
    {
        const NSModel::SSphere* desc = reinterpret_cast<const NSModel::SSphere*>(pDesc);

        Primitives::CreateSphere(desc->radius, desc->sliceCount, desc->stackCount, vertices, indices);
        Mesh& mesh = CreateMeshFromMemory(name, vertices, indices);

        return std::move(*this);
    }
    case NSModel::EPrimitive::PRIMITIVE_TYPE_CUBE:
    {
        const NSModel::SCube* desc = reinterpret_cast<const NSModel::SCube*>(pDesc);

        Primitives::CreateCube(desc->width, desc->height, desc->depth, vertices, indices);
        Mesh& mesh = CreateMeshFromMemory(name, vertices, indices);

        return std::move(*this);
    }
    case NSModel::EPrimitive::PRIMITIVE_TYPE_PLANE:
    {
        const NSModel::SPlane* desc = reinterpret_cast<const NSModel::SPlane*>(pDesc);

        Primitives::CreatePlane(desc->width, desc->depth, desc->widthSubdivisions, desc->depthSubdivisions, vertices, indices);
        Mesh& mesh = CreateMeshFromMemory(name, vertices, indices);

        return std::move(*this);
    }
    case NSModel::EPrimitive::PRIMITIVE_TYPE_CYLINDER:
    {
        const NSModel::SCylinder* desc = reinterpret_cast<const NSModel::SCylinder*>(pDesc);

        Primitives::CreateCylinder(desc->topRadius, desc->bottomRadius, desc->height, desc->sliceCount, desc->stackCount, vertices, indices);
        Mesh& mesh = CreateMeshFromMemory(name, vertices, indices);

        return std::move(*this);
    }
    case NSModel::EPrimitive::PRIMITIVE_TYPE_CONE:
    {
        const NSModel::SCone* desc = reinterpret_cast<const NSModel::SCone*>(pDesc);

            Primitives::CreateCone(desc->bottomRadius, desc->height, desc->sliceCount, desc->stackCount, vertices, indices);
            Mesh& mesh = CreateMeshFromMemory(name, vertices, indices);

            return std::move(*this);
        }

        default: OutputDebugStringA("Unable to create mesh\n");
    }

    return std::move(*this);
}
