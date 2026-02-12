#pragma once

#include <assimp/scene.h>
#include "Material.h"

class Mesh
{
public:
    Mesh(IWICImagingFactory2* wicFactory) : material(wicFactory) {}
    std::string name;

    Material material;
    ComPtr<ID3D12Resource> defaultVertexBuffer;
    ComPtr<ID3D12Resource> defaultIndexBuffer;
    ComPtr<ID3D12Resource> uploadVertexBuffer;
    ComPtr<ID3D12Resource> uploadIndexBuffer;

    D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
    D3D12_INDEX_BUFFER_VIEW indexBufferView{};
    UINT vertexCount{};
    UINT indexCount{};

    DirectX::XMFLOAT3 m_position{};
    DirectX::XMFLOAT4 m_rotationQ{};
    DirectX::XMFLOAT3 m_scale{};
};

class Model
{
public:
    Model();
    Model(_In_ ID3D12Device* device, _In_ IWICImagingFactory2* wicFactory);
    std::string m_name;

    DirectX::XMFLOAT3 m_position{};
    DirectX::XMFLOAT3 m_rotation{};
    DirectX::XMFLOAT3 m_scale{1.f, 1.f, 1.f};

    void RotateAdd(DirectX::XMFLOAT3 rotation);
    void Draw(_In_ DrawContext ctx);

    bool Load(_In_ const std::filesystem::path& path, _In_ ID3D12GraphicsCommandList* cmdList);
    void UploadGPU(_In_ ID3D12GraphicsCommandList* cmdList, _In_ ID3D12CommandQueue* cmdQueue);
    void UnloadGPU();
    void ResetUploadHeaps();
    inline const std::vector<Mesh>& GetMeshes() { return meshes; };

    std::filesystem::path m_assetPath;
    bool isOnGPU{};
    bool isOnCPU{};
private:
    IWICImagingFactory2* m_wicFactory;
    ID3D12Device* m_device;
    std::vector<Mesh> meshes;
    void ProcessNode(_In_ aiNode* node, _In_  const aiScene* scene, _In_ ID3D12GraphicsCommandList* cmdList);
    void ProcessMesh(_In_ aiMesh* pAiMesh, _In_ const aiScene* scene, _In_ aiNode* node, _Out_ Mesh& outMesh);

    inline aiMatrix4x4 GetGlobalNodeTransformation(aiNode* node) {
        aiMatrix4x4 transform = node->mTransformation;
        aiNode* parent = node->mParent;
        while (parent) {
            transform = parent->mTransformation * transform;
            parent = parent->mParent;
        }
        return transform;
    }
};

