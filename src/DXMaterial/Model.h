#pragma once

#include <assimp/scene.h>

class Mesh
{
public:
    ComPtr<ID3D12Resource> defaultVertexBuffer;
    ComPtr<ID3D12Resource> defaultIndexBuffer;
    ComPtr<ID3D12Resource> defaultDiffuseTexture;
    ComPtr<ID3D12Resource> uploadVertexBuffer;
    ComPtr<ID3D12Resource> uploadIndexBuffer;
    ComPtr<ID3D12Resource> uploadDiffuseBuffer;

    D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
    D3D12_INDEX_BUFFER_VIEW indexBufferView{};
    UINT vertexCount{};
    UINT indexCount{};
    UINT textureWidth{};
    UINT textureHeight{};
    UINT textureRowPitch{};

    DirectX::XMFLOAT3 m_position{};
    DirectX::XMFLOAT4 m_rotationQ{};
    DirectX::XMFLOAT3 m_scale{};
};

class Model
{
public:
    Model();
    Model(_In_ ID3D12Device* device, _In_ IWICImagingFactory2* wicFactory);

    DirectX::XMFLOAT3 m_position{};
    DirectX::XMFLOAT3 m_rotation{};
    DirectX::XMFLOAT3 m_scale{1.f, 1.f, 1.f};

    void RotateAdd(DirectX::XMFLOAT3 rotation);
    void Draw(_In_ DrawContext ctx);

    bool Load(_In_ const std::filesystem::path& path, _In_ ID3D12GraphicsCommandList* cmdList);
    void UploadGPU(_In_ ID3D12GraphicsCommandList* cmdList, _In_ ID3D12CommandQueue* cmdQueue);
    void ResetUploadHeaps();
    inline const std::vector<Mesh>& GetMeshes() { return meshes; };

    std::filesystem::path m_assetPath;
    bool isOnGPU = false;
    bool isOnCPU = false;
private:
    ID3D12Device* m_device;
    IWICImagingFactory2* m_wicFactory;
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

