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
        OutputDebugStringA(importer.GetErrorString());
        throw std::runtime_error("\n");
    }

    m_assetPath = path;
    ProcessNode(scene->mRootNode, scene, cmdList);

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
        Mesh& mesh = meshes.emplace_back(Mesh());
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

    std::vector<Vertex> vertices;
    std::vector<UINT> indices;

    aiMatrix4x4 aiGlobalTransform = GetGlobalNodeTransformation(node);
    DirectX::XMFLOAT4X4 globalFloat;
    memcpy(&globalFloat, &aiGlobalTransform, sizeof(FLOAT) * 16);
    DirectX::XMMATRIX globalMatrix = DirectX::XMLoadFloat4x4(&globalFloat);

    DirectX::XMVECTOR det = DirectX::XMMatrixDeterminant(globalMatrix);
    DirectX::XMMATRIX invTranspose = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(&det, globalMatrix));

    for (UINT i = 0; i < pAiMesh->mNumVertices; i++)
    {
         Vertex v{};

         DirectX::XMVECTOR pos = DirectX::XMVectorSet(pAiMesh->mVertices[i].x, pAiMesh->mVertices[i].y, pAiMesh->mVertices[i].z, 1.f);
         pos = DirectX::XMVector3TransformCoord(pos, globalMatrix);
         DirectX::XMStoreFloat3(&v.position, pos);

         if (pAiMesh->HasNormals())
         {
            DirectX::XMVECTOR norm = DirectX::XMVectorSet(pAiMesh->mNormals[i].x, pAiMesh->mNormals[i].y, pAiMesh->mNormals[i].z, 0.f);
            norm = DirectX::XMVector3Transform(norm, invTranspose);
            norm = DirectX::XMVector3Normalize(norm);
            DirectX::XMStoreFloat3(&v.normal, norm);
         }
         else {
             v.normal = DirectX::XMFLOAT3{ 0.f, 0.f, 0.f };
         }

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

    OutputDebugStringA(std::format("Mesh loaded: {} vertices, {} indices\n",
        static_cast<UINT>(vertices.size()),
        static_cast<UINT>(indices.size())).c_str()
    );

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

    if (pAiMesh->mMaterialIndex >= 0)
    {
        aiMaterial* material = scene->mMaterials[pAiMesh->mMaterialIndex];

        /*
        for (unsigned int type = 0; type < AI_TEXTURE_TYPE_MAX; ++type) {
            aiTextureType texType = static_cast<aiTextureType>(type);
            unsigned int count = material->GetTextureCount(texType);
            if (count > 0) {
                OutputDebugStringA(std::format("Found {} textures of type {}\n", count, type).c_str());
                aiString path;
                if (material->GetTexture(texType, 0, &path) == aiReturn_SUCCESS) {
                    OutputDebugStringA(std::format("Texture path: {}\n", path.C_Str()).c_str());
                }
            }
        }
        */

        aiString texPath;
        if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == aiReturn_SUCCESS)
        {
            std::string pathStr = texPath.C_Str();

            if (not pathStr.empty())
            {
                const aiTexture* embeddedTex = scene->GetEmbeddedTexture(texPath.C_Str());

                ComPtr<IWICBitmapDecoder> decoder;

                if (embeddedTex != nullptr)
                {
                    if (embeddedTex->mHeight == 0)
                    {
                        ComPtr<IWICStream> stream;
                        if (FAILED(m_wicFactory->CreateStream(&stream)))
                        {
                            OutputDebugStringA("Failed to create WIC stream\n");
                            goto __material_process_end__;
                        }
                        if (FAILED(stream->InitializeFromMemory(reinterpret_cast<BYTE*>(embeddedTex->pcData), embeddedTex->mWidth)))
                        {
                            OutputDebugStringA("Failed to initialize stream from memory\n");
                            goto __material_process_end__;
                        }

                        if (FAILED(m_wicFactory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &decoder)))
                        {
                            OutputDebugStringA("Failed to create WIC decoder\n");
                            goto __material_process_end__;
                        }
                    }
                }
                else {
                    std::wstring directory = m_assetPath.parent_path().generic_wstring() + L"/" + std::wstring(pathStr.begin(), pathStr.end());

                    if (FAILED(m_wicFactory->CreateDecoderFromFilename(directory.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder)))
                    {
                        OutputDebugStringW(std::format(L"Failed to create decoder from file: {}\n", directory).c_str());
                    }
                }

                ComPtr<IWICBitmapFrameDecode> frame;
                if (FAILED(decoder->GetFrame(0, &frame)))
                {
                    OutputDebugStringA("Failed to get frame from decoder\n");
                    goto __material_process_end__;
                }
                if (FAILED(frame->GetSize(&outMesh.textureWidth, &outMesh.textureHeight)))
                {
                    OutputDebugStringA("Failed to get texture dimensions\n");
                    goto __material_process_end__;
                }

                const UINT bpp = 4;
                const UINT alignment = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
                UINT rowPitch = (static_cast<UINT64>(outMesh.textureWidth) * bpp + alignment - 1) & ~(alignment - 1); // round up to the next multiple of alignment
                UINT uploadSize = rowPitch * outMesh.textureHeight;
                outMesh.textureRowPitch = rowPitch;

                ComPtr<IWICFormatConverter> converter;
                if (FAILED(m_wicFactory->CreateFormatConverter(&converter)))
                {
                    OutputDebugStringA("Failed to create format converter\n");
                    goto __material_process_end__;
                }
                if (FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.f, WICBitmapPaletteTypeCustom)))
                {
                    OutputDebugStringA("Failed to initialize format converter\n");
                    goto __material_process_end__;
                }

                D3D12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
                D3D12_HEAP_PROPERTIES uploadHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

                if (FAILED(m_device->CreateCommittedResource(
                    &uploadHeapProp,
                    D3D12_HEAP_FLAG_NONE,
                    &uploadBufferDesc,
                    D3D12_RESOURCE_STATE_GENERIC_READ,
                    nullptr,
                    IID_PPV_ARGS(&outMesh.uploadDiffuseBuffer)))) throw std::runtime_error("Failed to create texture upload buffer\n");

                void* pMappedData = nullptr;
                if (FAILED(outMesh.uploadDiffuseBuffer->Map(0, nullptr, &pMappedData)))
                {
                    OutputDebugStringA("Failed to map texture upload buffer\n");
                    goto __material_process_end__;
                }

                if (FAILED(converter->CopyPixels(
                    nullptr,
                    rowPitch,
                    uploadSize,
                    reinterpret_cast<BYTE*>(pMappedData)
                )))
                {
                    outMesh.uploadDiffuseBuffer->Unmap(0, nullptr);
                    OutputDebugStringA("Failed to copy pixels\n");
                    goto __material_process_end__;
                }

                outMesh.uploadDiffuseBuffer->Unmap(0, nullptr);

                //OutputDebugStringA(std::format("Texture loaded: {0}x{1}\n", outMesh.textureWidth, outMesh.textureHeight).c_str());
            }
        }
        else OutputDebugStringA("No diffuse texture");
    }
    __material_process_end__:

    if (outMesh.uploadDiffuseBuffer and outMesh.textureWidth > 0 and outMesh.textureHeight > 0)
    {
        D3D12_RESOURCE_DESC texDesc {};
        texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Width = outMesh.textureWidth;
        texDesc.Height = outMesh.textureHeight;
        texDesc.DepthOrArraySize = 1;
        texDesc.MipLevels = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 1;
        texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        D3D12_HEAP_PROPERTIES defaultHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

        if (FAILED(m_device->CreateCommittedResource(
            &defaultHeapProp,
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&outMesh.defaultDiffuseTexture)))) throw std::runtime_error("Failed to create default diffuse heap");
    }

    isOnCPU = true;
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
        if (mesh.defaultDiffuseTexture)
        {
            barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
                mesh.defaultDiffuseTexture.Get(),
                D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_STATE_COPY_DEST
            ));
        }
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

        if (mesh.defaultDiffuseTexture and mesh.uploadDiffuseBuffer)
        {
            D3D12_TEXTURE_COPY_LOCATION srcLoc{};
            srcLoc.pResource = mesh.uploadDiffuseBuffer.Get();
            srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            srcLoc.PlacedFootprint.Offset = 0;
            srcLoc.PlacedFootprint.Footprint.Format = mesh.defaultDiffuseTexture->GetDesc().Format;
            srcLoc.PlacedFootprint.Footprint.Width = mesh.textureWidth;
            srcLoc.PlacedFootprint.Footprint.Height = mesh.textureHeight;
            srcLoc.PlacedFootprint.Footprint.Depth = 1;
            srcLoc.PlacedFootprint.Footprint.RowPitch = mesh.textureRowPitch;

            D3D12_TEXTURE_COPY_LOCATION dstLoc{};
            dstLoc.pResource = mesh.defaultDiffuseTexture.Get();
            dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dstLoc.SubresourceIndex = 0;

            cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

            barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
                mesh.defaultDiffuseTexture.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
            ));
        }
    }

    cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

    ThrowIfFailed(cmdList->Close());
    ID3D12CommandList* ppCommandLists[] = { cmdList };
    cmdQueue->ExecuteCommandLists(1, ppCommandLists);

    isOnGPU = true;
}

_Use_decl_annotations_
void Model::Draw(ID3D12GraphicsCommandList* cmdList, _In_ CD3DX12_GPU_DESCRIPTOR_HANDLE& srvGPUHandle, UINT srvDescriptorSize)
{
    if (not cmdList)
    {
        throw std::runtime_error("At least one of the pointers are invalid");
    }

    UINT textureIndex{};
    for (const Mesh& mesh : meshes)
    {
        CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle(srvGPUHandle, textureIndex * srvDescriptorSize);
        cmdList->SetGraphicsRootDescriptorTable(1, texHandle);

        cmdList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
        cmdList->IASetIndexBuffer(&mesh.indexBufferView);
        cmdList->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);

        textureIndex++;
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
        mesh.uploadDiffuseBuffer.Reset();
    }
    isOnCPU = false;
}
