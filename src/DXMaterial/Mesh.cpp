#include "stdafx.h"
#include <stdexcept>

#include "Mesh.h"
#include "DXSampleHelper.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <directxtk12/WICTextureLoader.h>

Model::Model() {}

Model::Model(ID3D12Device* device) : m_device(device)
{
    if (not device)
    {
        throw std::runtime_error("Command list or command queue is not valid");
    }
}

bool Model::Load(const std::string& path, ID3D12GraphicsCommandList* cmdList)
{
    if (not cmdList)
    {
        throw std::runtime_error("Command list or command queue is not valid");
    }

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate |
        aiProcess_ConvertToLeftHanded |
        aiProcess_GenSmoothNormals |
        aiProcess_JoinIdenticalVertices
    );
    if (not scene or scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE or not scene->mRootNode)
    {
        OutputDebugStringA(importer.GetErrorString());
        throw std::runtime_error("\n");
    }

    directory = path.substr(0, path.find_last_of('/'));
    ProcessNode(scene->mRootNode, scene, cmdList);
    return true;
}

void Model::ProcessNode(aiNode* node, const aiScene* scene, ID3D12GraphicsCommandList* cmdList) {

    if (not node or not scene or not cmdList)
    {
        throw std::runtime_error("At least one of the pointers are invalid");
    }

    for (UINT i = 0; i < node->mNumMeshes; ++i) {
        aiMesh* pAiMmesh = scene->mMeshes[node->mMeshes[i]];
        Mesh& mesh = meshes.emplace_back(Mesh());
        ProcessMesh(pAiMmesh, scene, mesh);
    }
    for (UINT i = 0; i < node->mNumChildren; ++i) {
        ProcessNode(node->mChildren[i], scene, cmdList);
    }
}

void Model::ProcessMesh(aiMesh* pAiMesh, const aiScene* scene, Mesh& outMesh)
{
    if (not m_device or not pAiMesh or not scene)
    {
        throw std::runtime_error("At least one of the pointers are invalid");
    }

    std::vector<Vertex> vertices;
    std::vector<UINT> indices;

    for (UINT i = 0; i < pAiMesh->mNumVertices; i++)
    {
         Vertex v;
         v.position = { pAiMesh->mVertices[i].x, pAiMesh->mVertices[i].y,  pAiMesh->mVertices[i].z};
         v.normal   = pAiMesh->HasNormals() ? DirectX::XMFLOAT3{ pAiMesh->mNormals[i].x, pAiMesh->mNormals[i].y, pAiMesh->mNormals[i].z } : DirectX::XMFLOAT3{ 0.f, 0.f, 0.f };
         v.texCoord = pAiMesh->mTextureCoords[0] ? DirectX::XMFLOAT2{ pAiMesh->mTextureCoords[0][i].x, pAiMesh->mTextureCoords[0][i].y   } : DirectX::XMFLOAT2{0.f, 0.f};
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

    char buffer[256];
    sprintf_s(buffer, "Mesh loaded: %u vertices, %u indices\n",
        static_cast<UINT>(vertices.size()),
        static_cast<UINT>(indices.size()));
    OutputDebugStringA(buffer);

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

    outMesh.vertexBufferView.BufferLocation = outMesh.defaultVertexBuffer->GetGPUVirtualAddress();
    outMesh.vertexBufferView.SizeInBytes = vbByteSize;
    outMesh.vertexBufferView.StrideInBytes = sizeof(Vertex);

    outMesh.indexBufferView.BufferLocation = outMesh.defaultIndexBuffer->GetGPUVirtualAddress();
    outMesh.indexBufferView.SizeInBytes = ibByteSize;
    outMesh.indexBufferView.Format = DXGI_FORMAT_R32_UINT;

    isOnCPU = true;
}

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
    barriers.reserve(meshes.size() * 2u);

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

    ThrowIfFailed(cmdList->Close());
    ID3D12CommandList* ppCommandLists[] = { cmdList };
    cmdQueue->ExecuteCommandLists(1, ppCommandLists);

    isOnGPU = true;
}

void Model::Draw(ID3D12GraphicsCommandList* cmdList)
{
    if (not cmdList)
    {
        throw std::runtime_error("At least one of the pointers are invalid");
    }

    for (const Mesh& mesh : meshes)
    {
        cmdList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
        cmdList->IASetIndexBuffer(&mesh.indexBufferView);
        cmdList->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
    }
}

void Model::ResetUploadHeaps() {
    if (not isOnCPU)
    {
        throw std::runtime_error("Upload heaps are empty");
    }

    for (Mesh& mesh : meshes)
    {
        mesh.uploadIndexBuffer.Reset();
        mesh.uploadVertexBuffer.Reset();
    }
    isOnCPU = false;
}
