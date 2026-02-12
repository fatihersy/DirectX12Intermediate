#include "stdafx.h"
#include <stdexcept>

#include "Model.h"
#include "DXSampleHelper.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/version.h>

Model::Model() : m_device(nullptr), m_wicFactory(nullptr) {}

_Use_decl_annotations_
Model::Model(ID3D12Device* device, _In_ IWICImagingFactory2* wicFactory) : m_device(device), m_wicFactory(wicFactory)
{
    if (not device or not wicFactory)
    {
        throw std::runtime_error("At least one of the pointers are invalid");
    }
}

_Use_decl_annotations_
bool Model::Load(const std::filesystem::path& path, ID3D12GraphicsCommandList* cmdList)
{
    if (not cmdList)
    {
        throw std::runtime_error("At least one of the pointers are invalid");
    }

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path.generic_string(),
        aiProcess_Triangulate |
        aiProcess_ConvertToLeftHanded |
        aiProcess_GenSmoothNormals |
        aiProcess_JoinIdenticalVertices
    );
    if (not scene or scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE or not scene->mRootNode)
    {
        g_FError(importer.GetErrorString());
        throw std::runtime_error("\n");
    }

    m_assetPath = path;
    ProcessNode(scene->mRootNode, scene, cmdList);

    isOnCPU = true;
    return true;
}

_Use_decl_annotations_
void Model::ProcessNode(aiNode* node, const aiScene* scene, ID3D12GraphicsCommandList* cmdList) {

    if (not node or not scene or not node or not cmdList)
    {
        throw std::runtime_error("At least one of the pointers are invalid");
    }

    for (UINT i = 0; i < node->mNumMeshes; ++i) {
        aiMesh* pAiMesh = scene->mMeshes[node->mMeshes[i]];
        Mesh& mesh = meshes.emplace_back(Mesh(m_wicFactory));

        mesh.name = FString::format("%s::mesh%d", m_name, i);
        mesh.material.m_name = FString::format("%s::material", mesh.name);

        ProcessMesh(pAiMesh, scene, node, mesh);
    }
    for (UINT i = 0; i < node->mNumChildren; ++i) {
        ProcessNode(node->mChildren[i], scene, cmdList);
    }
}

_Use_decl_annotations_
void Model::ProcessMesh(aiMesh* pAiMesh, const aiScene* scene, _In_ aiNode* node, Mesh& outMesh)
{
    if (not m_device or not pAiMesh or not scene)
    {
        throw std::runtime_error("At least one of the pointers are invalid");
    }

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

        v.position = DirectX::XMFLOAT3 { pAiMesh->mVertices[i].x, pAiMesh->mVertices[i].y, pAiMesh->mVertices[i].z };
        v.normal   = pAiMesh->HasNormals() ? DirectX::XMFLOAT3 { pAiMesh->mNormals[i].x, pAiMesh->mNormals[i].y, pAiMesh->mNormals[i].z } : DirectX::XMFLOAT3{ 0.f, 0.f, 0.f };
        v.texCoord = pAiMesh->mTextureCoords[0] ? DirectX::XMFLOAT2{ pAiMesh->mTextureCoords[0][i].x, pAiMesh->mTextureCoords[0][i].y   } : DirectX::XMFLOAT2{ 0.f, 0.f };

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

        float metallic {};
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

    g_FDebug("Mesh loaded: {} vertices, {} indices\n", static_cast<UINT>(vertices.size()), static_cast<UINT>(indices.size()));
}

_Use_decl_annotations_
void Model::UploadGPU(ID3D12GraphicsCommandList* cmdList, ID3D12CommandQueue* cmdQueue)
{
    if (not cmdList or not cmdQueue)
    {
        throw std::runtime_error("At least one of the pointers are invalid");
    }
    if (not isOnCPU)
    {
        throw std::runtime_error("Upload heaps are empty");
    }
    if (isOnGPU)
    {
        return;
    }
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

    cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

    barriers.clear();

    for (Mesh& mesh : meshes)
    {
        cmdList->CopyResource(mesh.defaultVertexBuffer.Get(), mesh.uploadVertexBuffer.Get());
        cmdList->CopyResource(mesh.defaultIndexBuffer.Get(), mesh.uploadIndexBuffer.Get());

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

    cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

    for (Mesh& mesh : meshes)
    {
        mesh.material.UploadGPU(m_device, cmdQueue, cmdList);
    }

    ThrowIfFailed(cmdList->Close());
    ID3D12CommandList* ppCommandLists[] = { cmdList };
    cmdQueue->ExecuteCommandLists(1, ppCommandLists);

    isOnGPU = true;
}

_Use_decl_annotations_
void Model::Draw(DrawContext ctx)
{
    if (not ctx.cmdList)
    {
        throw std::runtime_error("At least one of the pointers are invalid");
    }

    DirectX::XMMATRIX globalRotation = DirectX::XMMatrixRotationRollPitchYaw(m_rotation.x, m_rotation.y, m_rotation.z);

    UINT meshIndex{};
    for (Mesh& mesh : meshes)
    {
        const DirectX::XMMATRIX scaleMatrix = DirectX::XMMatrixScalingFromVector(DirectX::XMLoadFloat3(&mesh.m_scale));
        const DirectX::XMMATRIX rotQMatrix  = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&mesh.m_rotationQ));
        const DirectX::XMMATRIX posMatrix   = DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&mesh.m_position));
        const DirectX::XMMATRIX worldMatrix = scaleMatrix * rotQMatrix * posMatrix * globalRotation;

        auto meshConstantGpuAddrBase = ctx.meshConstantsGpuVirtualAddr + sizeof(PaddedMeshConstants) * ctx.bufferIndex;

        meshConstants constants{};
        DirectX::XMStoreFloat4x4(&constants.worldMatrix, DirectX::XMMatrixTranspose(worldMatrix));
        constants.baseColor = mesh.material.m_baseColor;
        constants.metallic = mesh.material.m_metallic;
        constants.roughness = mesh.material.m_roughness;
        constants.opacity = mesh.material.m_opacity;
        constants.textureFlags = mesh.material.m_textureFlags;

        memcpy(&ctx.meshConstantsCpuAddr[meshIndex].constant, &constants, sizeof(meshConstants));

        ctx.cmdList->SetGraphicsRootConstantBufferView(1, meshConstantGpuAddrBase);

        mesh.material.Bind(ctx.cmdList);

        ctx.cmdList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
        ctx.cmdList->IASetIndexBuffer(&mesh.indexBufferView);
        ctx.cmdList->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);

        meshIndex++;
    }
}

void Model::RotateAdd(DirectX::XMFLOAT3 rotation)
{
    m_rotation.x += DirectX::XMConvertToRadians(rotation.x);
    m_rotation.y += DirectX::XMConvertToRadians(rotation.y);
    m_rotation.z += DirectX::XMConvertToRadians(rotation.z);

    if (m_rotation.x >= DirectX::XM_2PI)
    {
        m_rotation.x -= DirectX::XM_2PI;
    }
    if (m_rotation.y >= DirectX::XM_2PI)
    {
        m_rotation.y -= DirectX::XM_2PI;
    }
    if (m_rotation.z >= DirectX::XM_2PI)
    {
        m_rotation.z -= DirectX::XM_2PI;
    }
}

void Model::ResetUploadHeaps() {
    if (not isOnCPU)
    {
        g_FError("No CPU resoruce");
        return;
    }

    for (Mesh& mesh : meshes)
    {
        mesh.uploadIndexBuffer.Reset();
        mesh.uploadVertexBuffer.Reset();
        mesh.material.ResetUploadHeaps();
    }
    isOnCPU = false;
}

void Model::UnloadGPU()
{
    if (not isOnGPU)
    {
        g_FError("GPU resource is already empty");
        return;
    }

    for(Mesh& mesh : meshes)
    {
        mesh.defaultIndexBuffer.Reset();
        mesh.defaultVertexBuffer.Reset();
        mesh.material.UnloadGPU();
    }

    isOnGPU = false;
}
