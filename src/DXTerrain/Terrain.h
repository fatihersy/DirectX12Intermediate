#pragma once

namespace NSTerrain
{
    struct Vertex
    {
        DirectX::XMFLOAT3 position{};
        DirectX::XMFLOAT2 texCoord{};
    };

    struct TerrainChunk
    {
        ComPtr<ID3D12Resource> vertexDefault;
        ComPtr<ID3D12Resource> vertexUpload;
        D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};

        ComPtr<ID3D12Resource> triangleIndexDefault;
        ComPtr<ID3D12Resource> triangleIndexUpload;
        D3D12_INDEX_BUFFER_VIEW triangleIndexBufferView{};
        uint32_t triangleIndexCount{};

        ComPtr<ID3D12Resource> patchIndexDefault;
        ComPtr<ID3D12Resource> patchIndexUpload;
        D3D12_INDEX_BUFFER_VIEW patchIndexBufferView{};
        uint32_t patchIndexCount{};

        DirectX::XMFLOAT2 chunkUVOffset{};
        DirectX::XMFLOAT2 chunkUVScale{};

        ChunkKey key;
        ChunkBounds bounds;
    };

    struct ITerrainView
    {
    public:
        virtual ~ITerrainView() = default;

        virtual const TerrainDesc& GetDesc() const = 0;
        virtual const std::vector<TerrainChunk>& GetChunks() const = 0;
        virtual NSDescriptor::Offset GetHeightmapSRV() const = 0;
        virtual bool IsOnCPU() const = 0;
        virtual bool IsOnGPU() const = 0;
    };

    class Terrain : public ITerrainView
    {
    public:
        Terrain();
        Terrain(ID3D12Device14* device);

        Terrain(const Terrain&) = delete;
        Terrain& operator=(const Terrain&) = delete;
        Terrain(Terrain&& other) noexcept;
        Terrain& operator=(Terrain&& other) noexcept;

        void OnInit(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx, uint32_t seed, NSTerrain::TerrainDesc desc);
        bool OnInit(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx, const std::wstring_view path);
        bool OnInit(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx, const std::wstring_view path, NSTerrain::TerrainDesc desc);
        void OnDestroy(NSRenderer::Ctx rendererCtx);

        void Generate(uint32_t seed, NSTerrain::TerrainDesc desc);
        bool Load(const std::wstring_view path);
        void Free(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx);

        void Upload(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx);
        void Unload(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx);

        void ReleaseUploadBuffers();

        const TerrainDesc& GetDesc() const override {
            return m_desc;
        }
        const std::vector<TerrainChunk>& GetChunks() const override {
            return m_chunks;
        }
        NSDescriptor::Offset GetHeightmapSRV() const override {
            return m_textures[static_cast<size_t>(TextureIDs::HEIGHTMAP)].srvOffset;
        }
        bool IsOnCPU() const override {
            return m_isOnCPU;
        };
        bool IsOnGPU() const override {
            return m_isOnGPU;
        };

    private:
        ID3D12Device14* m_device = nullptr;
        TerrainDesc m_desc{};
        std::vector<TerrainChunk> m_chunks;

        std::vector<uint16_t> m_heightR16;

        enum class TextureIDs : uint32_t { HEIGHTMAP = 0, DIFFUSE, MAX };
        NSDescriptor::Handle m_texSrvHandle{};
        std::array<NSTexture::Texture, static_cast<size_t>(TextureIDs::MAX)> m_textures;

        void OnInit(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx);
        void BuildChunks(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx);
        float SampleHeight(float u, float v) const;

        bool m_isOnGPU{};
        bool m_isOnCPU{};
        bool m_isInitialized{};
    };
}
