#pragma once

namespace NSTerrain
{
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

    class Terrain
    {
    public:
        Terrain();
        Terrain(ID3D12Device14* device);

        Terrain(const Terrain&) = delete;
        Terrain& operator=(const Terrain&) = delete;
        Terrain(Terrain&& other) noexcept;
        Terrain& operator=(Terrain&& other) noexcept;

        void OnInit(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx, NSTerrain::TerrainDesc desc);
        void OnDestroy(NSRenderer::Ctx rendererCtx);

        void Generate(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx, NSTerrain::TerrainDesc desc);
        void Destroy(NSRenderer::Ctx rendererCtx);

        void Upload(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx);
        void Unload(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx);

        const TerrainDesc& GetDesc() const {
            return m_desc;
        }
        const std::vector<TerrainChunk>& GetChunks() const {
            return m_chunks;
        }
        const NSDescriptor::Handle& GetHeightmapSRV() const {
            return m_heightmapSRV;
        }

    private:
        ID3D12Device14* m_device = nullptr;
        TerrainDesc m_desc;
        std::vector<TerrainChunk> m_chunks;

        std::vector<uint16_t> m_heightR16;
        uint32_t m_hmWidth{};
        uint32_t m_hmHeight{};

        ComPtr<ID3D12Resource> m_heightmapTextureDefault;
        ComPtr<ID3D12Resource> m_heightmapTextureUpload;
        NSDescriptor::Handle m_heightmapSRV;

        void InitAndGenerateHeightmap(uint32_t seed = 0);
        void BuildChunks(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx);
        float SampleHeight(float u, float v) const;

        bool m_isOnGPU{};
        bool m_initialized{};
    };
}
