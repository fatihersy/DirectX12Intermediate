#pragma once

#include <assimp/scene.h>

#include "Material.h"

class Mesh
{
public:
    Mesh(IWICImagingFactory2 * wicFactory) : material(wicFactory) {}
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
    DirectX::XMFLOAT3 m_scale{1.f, 1.f, 1.f};
};

class Model
{
public:
    Model();
    Model(_In_ const char* name, _In_ ID3D12Device* device, _In_ IWICImagingFactory2* wicFactory);
    std::string m_name;
    SceneModelKey m_sceneKey;
    RegisterModelKey m_registerKey;

    void RotateAdd(DirectX::XMFLOAT3 rotation);
    void Move(DirectX::XMFLOAT3 vector, double delta);
    Model&& SetPosition(DirectX::XMFLOAT3 position)
    {
        m_position.x = position.x;
        m_position.y = position.y;
        m_position.z = position.z;
        return std::move(*this);
    }
    Model&& SetMetallic(float value) {
        meshes[0].material.m_metallic = value;
        return std::move(*this);
    }
    Model&& SetRoughness(float value) {
        meshes[0].material.m_roughness = value;
        return std::move(*this);
    }
    Model&& SetOpacity(float value) {
        meshes[0].material.m_opacity = value;
        return std::move(*this);
    }

    DirectX::XMFLOAT3 GetPosition() const { return m_position; }
    const std::vector<Mesh>& GetMeshes() { return meshes; };
    float GetMetallic() const {
        if (not meshes.empty())
            return meshes[0].material.m_metallic;
        else return -1;
    }
    float GetRoughness() const {
        if (not meshes.empty())
            return meshes[0].material.m_roughness;
        else return -1;
    }
    float GetOpacity() const {
        if (not meshes.empty())
            return meshes[0].material.m_opacity;
        else return -1;
    }

    bool Load(_In_ NSRenderer::Ctx rendererCtx, _In_ const std::filesystem::path& path);
    void UploadGPU(_In_ NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList);
    void UnloadGPU(NSRenderer::Ctx rendererCtx);
    void ResetUploadHeaps();

    template<typename T> requires IsPrimitiveMesh<T>
    Model&& As(NSRenderer::Ctx rendererCtx, PrimitiveTraits<T>& desc) {
        return _As(rendererCtx, "self", PrimitiveTraits<T>::type, &desc);
    }

    void Draw(NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList);
    void Draw(std::function<void(Mesh& mesh, UINT meshIndex, DirectX::XMMATRIX worldMatrix)> forEach);

    std::filesystem::path m_assetPath;
    bool isOnGPU{};
    bool isOnCPU{};
    SphereCollision collision;

private:
    IWICImagingFactory2* m_wicFactory;
    ID3D12Device* m_device;

    std::vector<Mesh> meshes;
    DirectX::XMFLOAT3 m_position{};
    DirectX::XMFLOAT3 m_rotation{};
    DirectX::XMFLOAT3 m_scale{ 1.f, 1.f, 1.f };

    void ProcessNode(_In_ NSRenderer::Ctx rendererCtx, _In_ aiNode* node, _In_  const aiScene* scene);
    void ProcessMesh(_In_ aiMesh* pAiMesh, _In_ const aiScene* scene, _In_ aiNode* node, _Out_ Mesh& outMesh);

    Mesh& CreateMeshFromMemory(const char* name, const std::vector<Vertex>& inVertices, const std::vector<UINT>& inIndices);
    Model&& _As(NSRenderer::Ctx rendererCtx, const char* name, EPrimitive type, void* pDesc);

    aiMatrix4x4 GetGlobalNodeTransformation(aiNode* node) {
        aiMatrix4x4 transform = node->mTransformation;
        aiNode* parent = node->mParent;
        while (parent) {
            transform = parent->mTransformation * transform;
            parent = parent->mParent;
        }
        return transform;
    }
};

