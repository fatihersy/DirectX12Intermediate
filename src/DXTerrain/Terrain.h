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

        std::vector<float> m_cpuHeightData;
        uint32_t m_hmWidth{};
        uint32_t m_hmHeight{};

        ComPtr<ID3D12Resource> m_heightmapBufferDefault;
        ComPtr<ID3D12Resource> m_heightmapBufferUpload;
        NSDescriptor::Handle m_heightmapSRV;

        void GenerateHeightmap();
        void BuildChunks(NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList);
        float SampleHeight(float u, float v) const;
    };
}
