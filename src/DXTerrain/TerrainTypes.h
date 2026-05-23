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

    struct TerrainDesc
    {
        float worldWidth{};
        float worldDepth{};
        float maxHeight{};
        uint32_t chunkCountX{};
        uint32_t chunkCountZ{};
        uint32_t vertsPerChunkEdge{};
        uint32_t dimention{};
    };
}
