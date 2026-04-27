#pragma once

namespace NSTerrain
{
    struct TerrainChunk
    {
        ComPtr<ID3D12Resource> vertexBuffer;
        ComPtr<ID3D12Resource> vertexUpload;
        D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};

        ComPtr<ID3D12Resource> triangleIndexBuffer;
        ComPtr<ID3D12Resource> triangleIndexUpload;
        D3D12_INDEX_BUFFER_VIEW triangleIndexBufferView{};
        uint32_t triangleIndexCount{};

        ComPtr<ID3D12Resource> patchIndexBuffer;
        ComPtr<ID3D12Resource> patchIndexUpload;
        D3D12_INDEX_BUFFER_VIEW patchIndexBufferView{};
        uint32_t patchIndexCount{};

        DirectX::XMFLOAT2 chunkUVOffset{};
        DirectX::XMFLOAT2 chunkUVScale{};

        ChunkBounds bounds;
    };

    class Terrain
    {
    public:
        Terrain();
        Terrain(ID3D12Device14* device);

        void OnInit(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx, NSTerrain::TerrainDesc desc);
        void OnDestroy();

        const TerrainDesc& GetDesc() const {
            return m_desc;
        }
        const TerrainChunk& GetChunk(NSTerrain::ChunkKey key) const {
            assert(m_chunks.size() > key.index);

            return m_chunks[key.index];
        }

    private:
        ID3D12Device14* m_device = nullptr;
        TerrainDesc m_desc;
        std::vector<TerrainChunk> m_chunks;

        std::vector<float> m_cpuHeightData;
        uint32_t m_hmWidth{};
        uint32_t m_hmHeight{};

        ComPtr<ID3D12Resource> m_heightmapTextureDefault;
        ComPtr<ID3D12Resource> m_heightmapTextureUpload;
        NSDescriptor::Handle m_heightmapSRV;

        void GenerateHeightmap();
        void BuildChunks(NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList);
        float SampleHeight(float u, float v) const;
    };
}
