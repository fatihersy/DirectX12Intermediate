#pragma once

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

#include <assimp/scene.h>

class Mesh
{
public:
    ComPtr<ID3D12Resource> defaultVertexBuffer;
    ComPtr<ID3D12Resource> defaultIndexBuffer;
    ComPtr<ID3D12Resource> uploadVertexBuffer;
    ComPtr<ID3D12Resource> uploadIndexBuffer;

    D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
    D3D12_INDEX_BUFFER_VIEW indexBufferView{};
    UINT vertexCount{};
    UINT indexCount{};

    std::vector<ComPtr<ID3D12Resource>> textures;
};

class Model
{
public:
    Model();
    Model(ID3D12Device* device);

    std::string directory;
    bool isOnGPU = false;
    bool isOnCPU = false;

    void Draw(ID3D12GraphicsCommandList* cmdList);

    bool Load(const std::string& path, ID3D12GraphicsCommandList* cmdList);
    void UploadGPU(ID3D12GraphicsCommandList* cmdList, ID3D12CommandQueue* cmdQueue);
    void ResetUploadHeaps();
private:
    ID3D12Device* m_device;
    std::vector<Mesh> meshes;
    void ProcessNode(aiNode* node, const aiScene* scene, ID3D12GraphicsCommandList* cmdList);
    void ProcessMesh(aiMesh* pAiMesh, const aiScene* scene, Mesh& outMesh);
};

