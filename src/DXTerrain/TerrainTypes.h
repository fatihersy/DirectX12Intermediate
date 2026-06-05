#pragma once

namespace NSTerrain
{
    struct ChunkKey
    {
        uint32_t gridX{};
        uint32_t gridZ{};
        size_t index = SIZE_MAX;
    };
    struct ChunkBounds
    {
        NSMath::SBoundAABB aabb;
    };
    struct PageKey
    {
        uint32_t gridX{};
        uint32_t gridZ{};
        size_t index = SIZE_MAX;
    };
    struct PageBounds
    {
        NSMath::SBoundAABB aabb;
    };

    struct TerrainSourceTextureDesc
    {
        std::filesystem::path file;
        DXGI_FORMAT format{};
        std::vector<NSTexture::EChannel> channels;
        uint32_t width;
        uint32_t height;
    };

    struct TerrainDesc
    {
        float worldWidth{};
        float worldDepth{};
        float maxHeight{};

        uint32_t pageCountX{};
        uint32_t pageCountZ{};

        uint32_t chunkCountX{};
        uint32_t chunkCountZ{};
        uint32_t vertsPerChunkEdge{};

        uint32_t dimention{};

        TerrainSourceTextureDesc heightmapDesc;
        TerrainSourceTextureDesc diffuseDesc;
    };
}
