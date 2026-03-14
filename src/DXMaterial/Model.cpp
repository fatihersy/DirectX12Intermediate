#include "stdafx.h"
#include <stdexcept>

#include "DXSampleHelper.h"
#include "IApp.h"
#include "Model.h"
#include "Primitives.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/version.h>

Model::Model() : m_device(nullptr), m_wicFactory(nullptr) {}

_Use_decl_annotations_
Model::Model(_In_ const char* name, ID3D12Device* device, _In_ IWICImagingFactory2* wicFactory) : m_name(name), m_device(device), m_wicFactory(wicFactory)
{
    if (not device or not wicFactory)
    {
        throw std::runtime_error("At least one of the pointers are invalid");
    }
}

_Use_decl_annotations_
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
    if (not scene or scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE or not scene->mRootNode)
    {
        g_FError(importer.GetErrorString());
        throw std::runtime_error("\n");
    }

    m_assetPath = path;
    ProcessNode(rendererCtx, scene->mRootNode, scene);

    isOnCPU = true;
    return true;
}

_Use_decl_annotations_
void Model::ProcessNode(NSRenderer::Ctx rendererCtx, aiNode* node, const aiScene* scene)
{
    assert(node and scene and node);

    for (UINT i = 0; i < node->mNumMeshes; ++i) {
        aiMesh* pAiMesh = scene->mMeshes[node->mMeshes[i]];

        Mesh& mesh = meshes.emplace_back(Mesh(m_wicFactory));

        mesh.name = FString::format("%s::mesh_%s", m_name, pAiMesh->mName.C_Str());
        mesh.material.m_name = FString::format("%s::material", mesh.name);

        ProcessMesh(pAiMesh, scene, node, mesh);
    }
    if (meshes.size() > IApp::GetInstance()->ic_maxObjects)
    {
        throw std::out_of_range("Meshes got out of range");
    }
    for (UINT i = 0; i < node->mNumChildren; ++i) {
        ProcessNode(rendererCtx, node->mChildren[i], scene);
    }
}

_Use_decl_annotations_
void Model::ProcessMesh(aiMesh* pAiMesh, const aiScene* scene, _In_ aiNode* node, Mesh& outMesh)
{
    assert(m_device and pAiMesh and scene);

    std::wstring meshName = std::wstring(outMesh.name.begin(), outMesh.name.end());

    std::vector<Vertex> vertices;
    std::vector<UINT> indices;

    aiMatrix4x4 aiGlobalTransform = GetGlobalNodeTransformation(node);

    DirectX::XMMATRIX globalMatrix = DirectX::XMMatrixSet(
        aiGlobalTransform.a1, aiGlobalTransform.b1, aiGlobalTransform.c1, aiGlobalTransform.d1,
        aiGlobalTransform.a2, aiGlobalTransform.b2, aiGlobalTransform.c2, aiGlobalTransform.d2,
        aiGlobalTransform.a3, aiGlobalTransform.b3, aiGlobalTransform.c3, aiGlobalTransform.d3,
        aiGlobalTransform.a4, aiGlobalTransform.b4, aiGlobalTransform.c4, aiGlobalTransform.d4
    );

    DirectX::XMVECTOR outScale, outRotQ, outPos;
    if (not DirectX::XMMatrixDecompose(&outScale, &outRotQ, &outPos, globalMatrix))
    {
        throw std::runtime_error("Failed to decompose matrix");
    }

    DirectX::XMStoreFloat3(&outMesh.m_position, outPos);
    DirectX::XMStoreFloat4(&outMesh.m_rotationQ, outRotQ);
    DirectX::XMStoreFloat3(&outMesh.m_scale, outScale);

    for (UINT i = 0; i < pAiMesh->mNumVertices; i++)
    {
        Vertex v{};

        v.position = DirectX::XMFLOAT3{
            pAiMesh->mVertices[i].x,
            pAiMesh->mVertices[i].y,
            pAiMesh->mVertices[i].z
        };

        v.normal = pAiMesh->HasNormals() ? DirectX::XMFLOAT3{
            pAiMesh->mNormals[i].x,
            pAiMesh->mNormals[i].y,
            pAiMesh->mNormals[i].z
        }
        : DirectX::XMFLOAT3{ 0.f, 0.f, 0.f };

        v.texCoord = pAiMesh->mTextureCoords[0] ? DirectX::XMFLOAT2{
            pAiMesh->mTextureCoords[0][i].x,
            pAiMesh->mTextureCoords[0][i].y
        }
        : DirectX::XMFLOAT2{ 0.f, 0.f };

        v.tangent = pAiMesh->HasTangentsAndBitangents() ? DirectX::XMFLOAT3{
            pAiMesh->mTangents[i].x,
            pAiMesh->mTangents[i].y,
            pAiMesh->mTangents[i].z
        }
        : DirectX::XMFLOAT3{ 1.f, 0.f, 0.f };

        v.bitangent = pAiMesh->HasTangentsAndBitangents() ? DirectX::XMFLOAT3{
            pAiMesh->mBitangents[i].x,
            pAiMesh->mBitangents[i].y,
            pAiMesh->mBitangents[i].z
        }
        : DirectX::XMFLOAT3{ 0.f, 1.f, 0.f };

        {
            using namespace DirectX;
            XMVECTOR T = XMLoadFloat3(&v.tangent);
            XMVECTOR B = XMLoadFloat3(&v.bitangent);
            XMVECTOR N = XMLoadFloat3(&v.normal);
            XMVECTOR det = XMVector3Dot(N, XMVector3Cross(T, B));
            if (XMVectorGetX(XMVectorLess(det, XMVectorZero())) > 0) // Check left handedness
            {
                v.bitangent.x = -v.bitangent.x;
                v.bitangent.y = -v.bitangent.y;
                v.bitangent.z = -v.bitangent.z;
            }
        }

        vertices.push_back(v);
    }

    for (UINT i = 0; i < pAiMesh->mNumFaces; i++)
    {
        aiFace face = pAiMesh->mFaces[i];
        for (UINT j = 0; j < face.mNumIndices; j++)
        {
            indices.push_back(face.mIndices[j]);
        }
    }

    g_FDebug("Mesh '%s' load begin with %u vertices, %u indices", outMesh.name, static_cast<UINT>(vertices.size()), static_cast<UINT>(indices.size()));

    outMesh.vertexCount = static_cast<UINT>(vertices.size());
    outMesh.indexCount = static_cast<UINT>(indices.size());
    const UINT vbByteSize = outMesh.vertexCount * sizeof(Vertex);
    const UINT ibByteSize = outMesh.indexCount * sizeof(UINT);

    D3D12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vbByteSize);
    D3D12_RESOURCE_DESC indexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(ibByteSize);
    D3D12_HEAP_PROPERTIES uploadHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    if (FAILED(m_device->CreateCommittedResource(
        &uploadHeapProp,
        D3D12_HEAP_FLAG_NONE,
        &vertexBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&outMesh.uploadVertexBuffer)))) throw std::runtime_error("Failed to create vertex buffer");

    outMesh.uploadVertexBuffer->SetName(meshName.append(L"defaultVertexBuffer").c_str());

    void* mappedVertexBuffer = nullptr;
    if (FAILED(outMesh.uploadVertexBuffer->Map(0u, nullptr, reinterpret_cast<void**>(&mappedVertexBuffer)))) throw std::runtime_error("Failed to map vertex upload buffer");

    memcpy(mappedVertexBuffer, vertices.data(), vbByteSize);
    outMesh.uploadVertexBuffer->Unmap(0, nullptr);

    if (FAILED(m_device->CreateCommittedResource(
        &uploadHeapProp,
        D3D12_HEAP_FLAG_NONE,
        &indexBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&outMesh.uploadIndexBuffer)))) throw std::runtime_error("Failed to create index buffer");

    outMesh.uploadIndexBuffer->SetName(meshName.append(L"defaultIndexBuffer").c_str());

    void* mappedIndexBuffer = nullptr;
    if (FAILED(outMesh.uploadIndexBuffer->Map(0u, nullptr, reinterpret_cast<void**>(&mappedIndexBuffer)))) throw std::runtime_error("Failed to map index upload buffer");

    memcpy(mappedIndexBuffer, indices.data(), ibByteSize);
    outMesh.uploadIndexBuffer->Unmap(0, nullptr);

    D3D12_HEAP_PROPERTIES defaultHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    if (FAILED(m_device->CreateCommittedResource(
        &defaultHeapProp,
        D3D12_HEAP_FLAG_NONE,
        &vertexBufferDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&outMesh.defaultVertexBuffer)))) throw std::runtime_error("Failed to create index buffer");

    if (FAILED(m_device->CreateCommittedResource(
        &defaultHeapProp,
        D3D12_HEAP_FLAG_NONE,
        &indexBufferDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&outMesh.defaultIndexBuffer)))) throw std::runtime_error("Failed to create index buffer");

    outMesh.defaultIndexBuffer->SetName(meshName.append(L"defaultIndexBuffer").c_str());
    outMesh.defaultVertexBuffer->SetName(meshName.append(L"defaultVertexBuffer").c_str());

    outMesh.vertexBufferView.BufferLocation = outMesh.defaultVertexBuffer->GetGPUVirtualAddress();
    outMesh.vertexBufferView.SizeInBytes = vbByteSize;
    outMesh.vertexBufferView.StrideInBytes = sizeof(Vertex);

    outMesh.indexBufferView.BufferLocation = outMesh.defaultIndexBuffer->GetGPUVirtualAddress();
    outMesh.indexBufferView.SizeInBytes = ibByteSize;
    outMesh.indexBufferView.Format = DXGI_FORMAT_R32_UINT;

    if (pAiMesh->mMaterialIndex >= 0)
    {
        aiMaterial* material = scene->mMaterials[pAiMesh->mMaterialIndex];

        aiString matName;
        if (material->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS) {
            const std::string materialName = outMesh.material.m_name;
            outMesh.material.m_name = FString::format("%s::%s", materialName, std::string(matName.C_Str(), matName.C_Str() + matName.length).c_str());
        }

        aiColor4D baseColor;
        if (material->Get(AI_MATKEY_COLOR_DIFFUSE, baseColor) == AI_SUCCESS) {
            outMesh.material.m_baseColor = DirectX::XMFLOAT4(baseColor.r, baseColor.g, baseColor.b, baseColor.a);
        }

        float metallic{};
        if (material->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS) {
            outMesh.material.m_metallic = metallic;
        }

        float roughness{};
        if (material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS) {
            outMesh.material.m_roughness = roughness;
        }

        float opacity{};
        if (material->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS) {
            outMesh.material.m_opacity = opacity;
        }

        for (UINT type = 0u; type < AI_TEXTURE_TYPE_MAX; ++type) {
            if (material->GetTextureCount(static_cast<aiTextureType>(type)) > 0) {
                aiString path;
                if (material->GetTexture(static_cast<aiTextureType>(type), 0u, &path) == aiReturn_SUCCESS) {
                    std::string pathStr = path.C_Str();

                    if (not pathStr.empty())
                    {
                        const aiTexture* embeddedTex = scene->GetEmbeddedTexture(path.C_Str());

                        ComPtr<IWICBitmapDecoder> decoder;

                        if (embeddedTex != nullptr)
                        {
                            if (embeddedTex->mHeight == 0)
                            {
                                ComPtr<IWICStream> stream;
                                if (FAILED(m_wicFactory->CreateStream(&stream)))
                                {
                                    g_FError("Failed to create WIC stream\n");
                                    continue;
                                }
                                if (FAILED(stream->InitializeFromMemory(reinterpret_cast<BYTE*>(embeddedTex->pcData), embeddedTex->mWidth)))
                                {
                                    g_FError("Failed to initialize stream from memory\n");
                                    continue;
                                }

                                if (FAILED(m_wicFactory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &decoder)))
                                {
                                    g_FError("Failed to create WIC decoder\n");
                                    continue;
                                }
                            }
                        }
                        else {
                            std::wstring directory = m_assetPath.parent_path().generic_wstring() + L"/" + std::wstring(pathStr.begin(), pathStr.end());

                            if (FAILED(m_wicFactory->CreateDecoderFromFilename(directory.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder)))
                            {
                                g_FError("Failed to create decoder from file: %s\n", WStringToString(directory).c_str());
                                continue;
                            }
                        }

                        outMesh.material.LoadTexture(m_device, decoder.Get(), static_cast<aiTextureType>(type));
                    }
                    else throw std::runtime_error("Failed to get path from aiString");
                }
                else throw std::runtime_error("Failed to get texture from material");
            }
        }
    }
    else g_FWarn("\n\t-- No Material Found");

    g_FDebug("\n\t -- loaded\n");
}

_Use_decl_annotations_
void Model::UploadGPU(NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
{
    assert(isOnCPU);

    if (isOnGPU) return;

    std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
    barriers.reserve(meshes.size() * 3u);

    for (Mesh& mesh : meshes)
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

    for (Mesh& mesh : meshes)
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

    for (Mesh& mesh : meshes)
    {
        mesh.material.UploadGPU(m_device, rendererCtx, cmdList);
    }

    isOnGPU = true;
}

void Model::Draw(NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
{
    DirectX::XMMATRIX globalRotation = DirectX::XMMatrixRotationRollPitchYaw(m_rotation.x, m_rotation.y, m_rotation.z);
    DirectX::XMMATRIX globalPosition = DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&m_position));

    for (Mesh& mesh : meshes)
    {
        Allocator::AllocCtx allocCtx = rendererCtx.allocConstBuff(sizeof(meshConstants));
        meshConstants& meshCB = allocCtx.As<meshConstants>();

        const DirectX::XMMATRIX scaleMatrix = DirectX::XMMatrixScalingFromVector(DirectX::XMLoadFloat3(&mesh.m_scale));
        const DirectX::XMMATRIX rotQMatrix = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&mesh.m_rotationQ));
        const DirectX::XMMATRIX posMatrix = DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&mesh.m_position));
        const DirectX::XMMATRIX worldMatrix = scaleMatrix * rotQMatrix * posMatrix * globalRotation * globalPosition;

        DirectX::XMStoreFloat4x4(&meshCB.worldMatrix, worldMatrix);
        DirectX::XMVECTOR det;
        DirectX::XMMATRIX worldInverse = DirectX::XMMatrixInverse(&det, worldMatrix);
        DirectX::XMStoreFloat3x4(&meshCB.normalMatrix, worldInverse);

        meshCB.baseColor = mesh.material.m_baseColor;
        meshCB.metallic = mesh.material.m_metallic;
        meshCB.roughness = mesh.material.m_roughness;
        meshCB.opacity = mesh.material.m_opacity;
        meshCB.textureFlags = mesh.material.m_textureFlags;

        cmdList.SetGraphicsRootConstantBufferView(1, allocCtx.gpuAddr);

        mesh.material.Bind(cmdList);

        cmdList.IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
        cmdList.IASetIndexBuffer(&mesh.indexBufferView);
        cmdList.DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
    }
}
void Model::Draw(std::function<void(Mesh& mesh, UINT meshIndex, DirectX::XMMATRIX worldMatrix)> forEach)
{
    DirectX::XMMATRIX globalRotation = DirectX::XMMatrixRotationRollPitchYaw(m_rotation.x, m_rotation.y, m_rotation.z);
    DirectX::XMMATRIX globalPosition = DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&m_position));

    UINT meshIndex{};
    for (Mesh& mesh : meshes)
    {
        const DirectX::XMMATRIX scaleMatrix = DirectX::XMMatrixScalingFromVector(DirectX::XMLoadFloat3(&mesh.m_scale));
        const DirectX::XMMATRIX rotQMatrix = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&mesh.m_rotationQ));
        const DirectX::XMMATRIX posMatrix = DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&mesh.m_position));
        const DirectX::XMMATRIX worldMatrix = scaleMatrix * rotQMatrix * posMatrix * globalRotation * globalPosition;

        forEach(mesh, meshIndex, worldMatrix);

        meshIndex++;
    }
}

void Model::RotateAdd(DirectX::XMFLOAT3 rotation)
{
    m_rotation.x = fmod(m_rotation.x + DirectX::XMConvertToRadians(rotation.x), DirectX::XM_2PI);
    m_rotation.y = fmod(m_rotation.y + DirectX::XMConvertToRadians(rotation.y), DirectX::XM_2PI);
    m_rotation.z = fmod(m_rotation.z + DirectX::XMConvertToRadians(rotation.z), DirectX::XM_2PI);
}

void Model::Move(DirectX::XMFLOAT3 vector, double delta)
{
    delta = std::clamp(
        delta,
        static_cast<double>(std::numeric_limits<float>::lowest()),
        static_cast<double>(std::numeric_limits<float>::max())
    );

    m_position.x += static_cast<float>(static_cast<double>(vector.x) * delta);
    m_position.y += static_cast<float>(static_cast<double>(vector.y) * delta);
    m_position.z += static_cast<float>(static_cast<double>(vector.z) * delta);
}

void Model::ResetUploadHeaps() {
    if (not isOnCPU) return;

    for (Mesh& mesh : meshes)
    {
        mesh.uploadIndexBuffer.Reset();
        mesh.uploadVertexBuffer.Reset();
        mesh.material.ResetUploadHeaps();
    }
    isOnCPU = false;
}

void Model::UnloadGPU(NSRenderer::Ctx rendererCtx)
{
    for (Mesh& mesh : meshes)
    {
        mesh.uploadVertexBuffer.Reset();
        mesh.uploadIndexBuffer.Reset();
        mesh.defaultVertexBuffer.Reset();
        mesh.defaultIndexBuffer.Reset();
        mesh.material.UnloadGPU(rendererCtx);
    }

    isOnGPU = false;
    isOnCPU = false;
}

Model&& Model::_As(NSRenderer::Ctx rendererCtx, const char* name, EPrimitive type, void* pDesc)
{
    assert(name and pDesc and type > EPrimitive::PRIMITIVE_TYPE_NONE and type < EPrimitive::PRIMITIVE_TYPE_MAX);

    this->UnloadGPU(rendererCtx);
    this->ResetUploadHeaps();
    std::vector<Vertex> vertices;
    std::vector<UINT> indices;

    switch (type)
    {
        case EPrimitive::PRIMITIVE_TYPE_DOME: {
            const SDome* desc = reinterpret_cast<const SDome*>(pDesc);
            Primitives::CreateDome(desc->radius, desc->sliceCount, desc->stackCount, vertices, indices);
            Mesh& mesh = CreateMeshFromMemory(name, vertices, indices);
            return std::move(*this);
        }
        case EPrimitive::PRIMITIVE_TYPE_SPHERE: {
            const SSphere* desc = reinterpret_cast<const SSphere*>(pDesc);
            Primitives::CreateSphere(desc->radius, desc->sliceCount, desc->stackCount, vertices, indices);
            Mesh& mesh = CreateMeshFromMemory(name, vertices, indices);
            return std::move(*this);
        }
        case EPrimitive::PRIMITIVE_TYPE_CUBE: {
            const SCube* desc = reinterpret_cast<const SCube*>(pDesc);
            Primitives::CreateCube(desc->width, desc->height, desc->depth, vertices, indices);
            Mesh& mesh = CreateMeshFromMemory(name, vertices, indices);
            return std::move(*this);
        }
        case EPrimitive::PRIMITIVE_TYPE_PLANE: {
            const SPlane* desc = reinterpret_cast<const SPlane*>(pDesc);
            Primitives::CreatePlane(desc->width, desc->depth, desc->widthSubdivisions, desc->depthSubdivisions, vertices, indices);
            Mesh& mesh = CreateMeshFromMemory(name, vertices, indices);
            return std::move(*this);
        }
        case EPrimitive::PRIMITIVE_TYPE_CYLINDER: {
            const SCylinder* desc = reinterpret_cast<const SCylinder*>(pDesc);
            Primitives::CreateCylinder(desc->topRadius, desc->bottomRadius, desc->height, desc->sliceCount, desc->stackCount, vertices, indices);
            Mesh& mesh = CreateMeshFromMemory(name, vertices, indices);
            return std::move(*this);
        }
        case EPrimitive::PRIMITIVE_TYPE_CONE: {
            const SCone* desc = reinterpret_cast<const SCone*>(pDesc);
            Primitives::CreateCone(desc->bottomRadius, desc->height, desc->sliceCount, desc->stackCount, vertices, indices);
            Mesh& mesh = CreateMeshFromMemory(name, vertices, indices);
            return std::move(*this);
        }
    }

    throw std::runtime_error("Unsupported primitive type");
}

Mesh& Model::CreateMeshFromMemory(const char* name, const std::vector<Vertex>& inVertices, const std::vector<UINT>& inIndices)
{
    IApp* appInfo = IApp::GetInstance();

    assert(name and not inVertices.empty() or not inIndices.empty());

    Mesh& mesh = meshes.emplace_back(Mesh(m_wicFactory));
    mesh.name = FString::format("%s::%s", m_name, name) ;
    mesh.material.m_name = FString::format("%s::native", mesh.name);
    std::wstring meshName = std::wstring(mesh.name.begin(), mesh.name.end());

    mesh.vertexCount = static_cast<UINT>(inVertices.size());
    mesh.indexCount = static_cast<UINT>(inIndices.size());
    const size_t vbSize = inVertices.size() * sizeof(Vertex);
    const size_t ibSize = inIndices.size() * sizeof(UINT);

    D3D12_RESOURCE_DESC vbDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);
    D3D12_RESOURCE_DESC ibDesc = CD3DX12_RESOURCE_DESC::Buffer(ibSize);

    D3D12_HEAP_PROPERTIES uploadHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    ThrowIfFailed(m_device->CreateCommittedResource(
        &uploadHeapProp,
        D3D12_HEAP_FLAG_NONE,
        &vbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mesh.uploadVertexBuffer)
    ));
    mesh.uploadVertexBuffer->SetName(FString::wformat(L"%s::%s", std::wstring(mesh.name.begin(), mesh.name.end()), L"uploadVertexBuffer").c_str());

    ThrowIfFailed(m_device->CreateCommittedResource(
        &uploadHeapProp,
        D3D12_HEAP_FLAG_NONE,
        &ibDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mesh.uploadIndexBuffer)
    ));
    mesh.uploadIndexBuffer->SetName(FString::wformat(L"%s::%s", std::wstring(mesh.name.begin(), mesh.name.end()), L"uploadIndexBuffer").c_str());

    void* mappedVertexBuffer = nullptr;
    ThrowIfFailed(mesh.uploadVertexBuffer->Map(0u, nullptr, reinterpret_cast<void**>(&mappedVertexBuffer)));
    memcpy(mappedVertexBuffer, inVertices.data(), vbSize);
    mesh.uploadVertexBuffer->Unmap(0, nullptr);

    void* mappedIndexBuffer = nullptr;
    ThrowIfFailed(mesh.uploadIndexBuffer->Map(0u, nullptr, reinterpret_cast<void**>(&mappedIndexBuffer)));
    memcpy(mappedIndexBuffer, inIndices.data(), ibSize);
    mesh.uploadIndexBuffer->Unmap(0, nullptr);

    D3D12_HEAP_PROPERTIES defaultHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    ThrowIfFailed(m_device->CreateCommittedResource(
        &defaultHeapProp,
        D3D12_HEAP_FLAG_NONE,
        &vbDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&mesh.defaultVertexBuffer)
    ));
    mesh.defaultVertexBuffer->SetName(FString::wformat(L"%s::%s", std::wstring(mesh.name.begin(), mesh.name.end()), L"defaultVertexBuffer").c_str());

    mesh.vertexBufferView.BufferLocation = mesh.defaultVertexBuffer->GetGPUVirtualAddress();
    mesh.vertexBufferView.SizeInBytes = static_cast<UINT>(vbSize);
    mesh.vertexBufferView.StrideInBytes = sizeof(Vertex);

    ThrowIfFailed(m_device->CreateCommittedResource(
        &defaultHeapProp,
        D3D12_HEAP_FLAG_NONE,
        &ibDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&mesh.defaultIndexBuffer)
    ));
    mesh.defaultIndexBuffer->SetName(FString::wformat(L"%s::%s", std::wstring(mesh.name.begin(), mesh.name.end()), L"defaultIndexBuffer").c_str());

    mesh.indexBufferView.BufferLocation = mesh.defaultIndexBuffer->GetGPUVirtualAddress();
    mesh.indexBufferView.SizeInBytes = static_cast<UINT>(ibSize);
    mesh.indexBufferView.Format = DXGI_FORMAT_R32_UINT;

    isOnCPU = true;
    return mesh;
}
