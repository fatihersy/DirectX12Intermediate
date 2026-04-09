#pragma once

#include "Material.h"

struct aiNode;
struct aiScene;
struct aiMesh;

class Mesh
{
public:
    Mesh(IWICImagingFactory2* wicFactory) : material(wicFactory) {};
    std::wstring m_name;

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
    DirectX::XMFLOAT3 m_scale{ 1.f, 1.f, 1.f };
};

class Model
{
public:
    Model();
    Model(ID3D12Device14* device, IWICImagingFactory2* wicFactory, std::wstring_view name);

    std::wstring m_name;
    NSModel::SceneModelKey m_sceneKey;
    NSModel::RegisterModelKey m_registerKey;

    void RotateAdd(DirectX::XMFLOAT3 rotation);
    void Move(DirectX::XMFLOAT3 vector, float delta);
    Model&& SetPosition(DirectX::XMFLOAT3 position)
    {
        m_position.x = position.x;
        m_position.y = position.y;
        m_position.z = position.z;
        return std::move(*this);
    }
    Model&& SetMetallic(float val)
    {
        for (Mesh& mesh : m_meshes) {
            mesh.material.m_metallic = val;
        }
        return std::move(*this);
    }
    Model&& SetRoughness(float val)
    {
        for (Mesh& mesh : m_meshes) {
            mesh.material.m_roughness = val;
        }
        return std::move(*this);
    }
    Model&& SetOpacity(float val)
    {
        for (Mesh& mesh : m_meshes) {
            mesh.material.m_opacity = val;
        }
        return std::move(*this);
    }

    DirectX::XMFLOAT3 GetPosition() const { return m_position; }
    DirectX::XMFLOAT3 GetRotation() const { return m_rotation; }
    DirectX::XMFLOAT3 GetScale() const { return m_scale; }
    const std::vector<Mesh>& GetMeshes() const { return m_meshes; }
    float GetMetallic() const
    {
        assert(not m_meshes.empty());

        return m_meshes[0].material.m_metallic;
    }
    float GetRoughness() const
    {
        assert(not m_meshes.empty());

        return m_meshes[0].material.m_roughness;
    }
    float GetOpacity() const
    {
        assert(not m_meshes.empty());

        return m_meshes[0].material.m_opacity;
    }

    bool Load(NSRenderer::Ctx rendererCtx, const std::filesystem::path& path);
    void UploadGPU(NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList);
    void UnloadGPU(NSRenderer::Ctx rendererCtx);
    void ResetUploadHeaps();

    template<typename T> requires NSModel::IsPrimitiveMesh<T>
    Model&& As(NSRenderer::Ctx rendererCtx, NSModel::PrimitiveTraits<T>& desc) {
        return _As(rendererCtx, L"self", desc.type, static_cast<void*>(std::addressof(desc)));
    }

    void Draw(std::function<void(Mesh& mesh, UINT meshIndex, DirectX::XMMATRIX worldMatrix)> forEach);

    std::filesystem::path m_assetsPath;
    bool isOnGPU{};
    bool isOnCPU{};
    NSModel::SphereCollision collision;

    bool TestFlag(NSModel::EModelFlag flag) const {
        return m_flags.test(static_cast<uint32_t>(flag));
    }
    void SetFlag(NSModel::EModelFlag flag) {
        m_flags.set(static_cast<uint32_t>(flag));
    }
    void ResetFlag(NSModel::EModelFlag flag) {
        m_flags.reset(static_cast<uint32_t>(flag));
    }
    void FlipFlag(NSModel::EModelFlag flag) {
        m_flags.flip(static_cast<uint32_t>(flag));
    }

private:
    IWICImagingFactory2* m_wicFactory = nullptr;
    ID3D12Device14* m_device = nullptr;

    std::vector<Mesh> m_meshes;
    DirectX::XMFLOAT3 m_position{};
    DirectX::XMFLOAT3 m_rotation{};
    DirectX::XMFLOAT3 m_scale{ 1.f, 1.f, 1.f };

    void ProcessNode(NSRenderer::Ctx rendererCtx, aiNode* node, const aiScene* scene);
    void ProcessMesh(aiMesh* pAiMesh, const aiScene* scene, aiNode* node, Mesh& outMesh);

    Mesh& CreateMeshFromMemory(std::wstring_view name, const std::vector<NSModel::Vertex>& inVertices, const std::vector<UINT>& inIndices);
    Model&& _As(NSRenderer::Ctx rendererCtx, std::wstring_view name, NSModel::EPrimitive type, void* pDesc);

    std::bitset<32> m_flags{};
};
