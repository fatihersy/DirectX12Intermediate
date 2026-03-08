#pragma once

#include <assimp/scene.h>

#include "Renderer.h"
#include "Material.h"
#include "Allocator.h"

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

    void RotateAdd(DirectX::XMFLOAT3 rotation);
    void Move(DirectX::XMFLOAT3 vector, double delta);
    inline Model&& SetPosition(DirectX::XMFLOAT3 position)
    {
        m_position.x = position.x;
        m_position.y = position.y;
        m_position.z = position.z;
        return std::move(*this);
    }
    inline Model&& SetMetallic(float value) {
        meshes[0].material.m_metallic = value;
        return std::move(*this);
    }
    inline Model&& SetRoughness(float value) {
        meshes[0].material.m_roughness = value;
        return std::move(*this);
    }
    inline Model&& SetOpacity(float value) {
        meshes[0].material.m_opacity = value;
        return std::move(*this);
    }

    inline DirectX::XMFLOAT3 GetPosition() const { return m_position; }
    inline const std::vector<Mesh>& GetMeshes() { return meshes; };
    inline float GetMetallic() const {
        if (not meshes.empty())
            return meshes[0].material.m_metallic;
        else return -1;
    }
    inline float GetRoughness() const {
        if (not meshes.empty())
            return meshes[0].material.m_roughness;
        else return -1;
    }
    inline float GetOpacity() const {
        if (not meshes.empty())
            return meshes[0].material.m_opacity;
        else return -1;
    }

    bool Load(_In_ Renderer& renderer, _In_ const std::filesystem::path& path);
    void UploadGPU(_In_ Renderer& renderer);
    void UnloadGPU(Renderer& renderer);
    void ResetUploadHeaps();

    template<typename T> requires IsPrimitiveMesh<T>
    inline Model&& As(Renderer& renderer, PrimitiveTraits<T>& desc) {
        return _As("self", cmdList, PrimitiveTraits<T>::type, &desc);
    }

    void Draw(Renderer& renderer, CBAllocator& allocator);
    void Draw(std::function<void(Mesh& mesh, UINT meshIndex, DirectX::XMMATRIX worldMatrix)> forEach);

    std::filesystem::path m_assetPath;
    bool isOnGPU{};
    bool isOnCPU{};

private:
    IWICImagingFactory2* m_wicFactory;
    ID3D12Device* m_device;

    std::vector<Mesh> meshes;
    DirectX::XMFLOAT3 m_position{};
    DirectX::XMFLOAT3 m_rotation{};
    DirectX::XMFLOAT3 m_scale{ 1.f, 1.f, 1.f };

    void ProcessNode(_In_ Renderer& renderer, _In_ aiNode* node, _In_  const aiScene* scene);
    void ProcessMesh(_In_ aiMesh* pAiMesh, _In_ const aiScene* scene, _In_ aiNode* node, _Out_ Mesh& outMesh);

    Mesh& CreateMeshFromMemory(const char* name, const std::vector<Vertex>& inVertices, const std::vector<UINT>& inIndices);
    Model&& _As(Renderer& renderer, const char* name, EPrimitive type, void* pDesc);

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

