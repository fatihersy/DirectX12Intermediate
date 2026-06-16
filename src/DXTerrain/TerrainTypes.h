#pragma once
#include "TextureTypes.h"

namespace NSTerrain
{
    struct ChunkIndex
    {
        uint32_t gridX{};
        uint32_t gridZ{};
    };
    struct PageIndex
    {
        uint32_t gridX{};
        uint32_t gridZ{};
    };

    struct TerrainSourceTextureDesc
    {
        std::filesystem::path relativePath;
        DXGI_FORMAT format{};
        std::vector<NSTexture::EChannel> channels;
        uint32_t width{};
        uint32_t height{};
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

        TerrainSourceTextureDesc heightmapDesc;
        TerrainSourceTextureDesc diffuseDesc;
    };
}
