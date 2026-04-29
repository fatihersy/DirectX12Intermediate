#include "stdafx.h"
#include "Terrain.h"

using namespace NSTerrain;

Terrain::Terrain() {}
Terrain::Terrain(ID3D12Device14* device) : m_device(device)
{

}
void Terrain::OnInit(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx, NSTerrain::TerrainDesc desc)
{
    this->m_desc = desc;

    GenerateHeightmap();

    BuildChunks(rendererCtx, cmdList);
}
void Terrain::OnDestroy()
{
    assert(m_heightmapSRV.amount == 0);

    m_device = nullptr;
    m_chunks.clear();
    m_chunks.resize(0);
    m_cpuHeightData.clear();
    m_cpuHeightData.resize(0);
    m_heightmapTextureDefault.Reset();
    m_heightmapTextureUpload.Reset();
    m_hmWidth = 0;
    m_hmHeight = 0;
    m_desc = {};
}

void Terrain::GenerateHeightmap()
{
    m_hmWidth = m_desc.heightMapResolution;
    m_hmHeight = m_desc.heightMapResolution;

    m_cpuHeightData.resize(size_t(m_hmWidth) * size_t(m_hmHeight));

    constexpr uint32_t seed = 1337u;
    constexpr float baseFrequency = 8.0f;

    for (uint32_t z = 0; z < m_hmHeight; ++z)
    {
        for (uint32_t x = 0; x < m_hmWidth; ++x)
        {
            float u = float(x) / float(m_hmWidth - 1);
            float v = float(z) / float(m_hmHeight - 1);

            float h = NSMath::FBm(u * baseFrequency, v * baseFrequency, seed);

            // Optional shaping: more lowlands, slightly sharper peaks.
            h = std::pow(h, 1.35f);

            m_cpuHeightData[size_t(z) * size_t(m_hmWidth) + size_t(x)] = NSMath::Saturate(h);
        }
    }
}
void Terrain::BuildChunks(NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList)
{
    const uint32_t vertsPerEdge = m_desc.vertsPerChunkEdge;
    const uint32_t quadsPerEdge = vertsPerEdge - 1;

    const uint32_t totalVertsX = m_desc.chunkCountX * quadsPerEdge + 1;
    const uint32_t totalVertsZ = m_desc.chunkCountZ * quadsPerEdge + 1;

    m_chunks.clear();
    m_chunks.reserve(size_t(m_desc.chunkCountX) * m_desc.chunkCountZ);

    for (uint32_t gz = 0; gz < m_desc.chunkCountZ; ++gz)
    {
        for (uint32_t gx = 0; gx < m_desc.chunkCountX; ++gx)
        {
            const size_t chunkIndex = size_t(gz) * m_desc.chunkCountX + gx;

            NSTerrain::ChunkKey key{
                .gridX = gx,
                .gridZ = gz,
                .index = chunkIndex
            };

            std::vector<NSModel::Vertex> vertices;
            std::vector<uint32_t> triIndices;
            std::vector<uint32_t> patchIndices;

            vertices.reserve(size_t(vertsPerEdge) * vertsPerEdge);
            triIndices.reserve(size_t(quadsPerEdge) * quadsPerEdge * 6);
            patchIndices.reserve(size_t(quadsPerEdge) * quadsPerEdge * 4);

            DirectX::XMFLOAT3 aabbMin{
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max()
            };

            DirectX::XMFLOAT3 aabbMax{
                std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest()
            };

            for (uint32_t localZ = 0; localZ < vertsPerEdge; ++localZ)
            {
                for (uint32_t localX = 0; localX < vertsPerEdge; ++localX)
                {
                    const uint32_t globalX = gx * quadsPerEdge + localX;
                    const uint32_t globalZ = gz * quadsPerEdge + localZ;

                    const float u = float(globalX) / float(totalVertsX - 1);
                    const float v = float(globalZ) / float(totalVertsZ - 1);

                    const float x = u * m_desc.worldWidth - 0.5f * m_desc.worldWidth;
                    const float z = v * m_desc.worldDepth - 0.5f * m_desc.worldDepth;
                    const float y = SampleHeight(u, v) * m_desc.maxHeight;

                    NSModel::Vertex vertex{};
                    vertex.position = { x, y, z };
                    vertex.normal = { 0.f, 1.f, 0.f };
                    vertex.tangent = { 1.f, 0.f, 0.f };
                    vertex.bitangent = { 0.f, 0.f, 1.f };
                    vertex.texCoord = { u, v };

                    vertices.push_back(vertex);

                    aabbMin.x = std::min(aabbMin.x, x);
                    aabbMin.y = std::min(aabbMin.y, y);
                    aabbMin.z = std::min(aabbMin.z, z);

                    aabbMax.x = std::max(aabbMax.x, x);
                    aabbMax.y = std::max(aabbMax.y, y);
                    aabbMax.z = std::max(aabbMax.z, z);
                }
            }

            for (uint32_t z = 0; z < quadsPerEdge; ++z)
            {
                for (uint32_t x = 0; x < quadsPerEdge; ++x)
                {
                    const uint32_t bottomLeft  = z * vertsPerEdge + x;
                    const uint32_t bottomRight = bottomLeft + 1;
                    const uint32_t topLeft     = bottomLeft + vertsPerEdge;
                    const uint32_t topRight    = topLeft + 1;

                    // Triangle list, CCW when viewed from above.
                    triIndices.push_back(bottomLeft);
                    triIndices.push_back(topLeft);
                    triIndices.push_back(bottomRight);

                    triIndices.push_back(bottomRight);
                    triIndices.push_back(topLeft);
                    triIndices.push_back(topRight);

                    // 4-control-point patch order:
                    // bottom-left, bottom-right, top-right, top-left.
                    patchIndices.push_back(bottomLeft);
                    patchIndices.push_back(bottomRight);
                    patchIndices.push_back(topRight);
                    patchIndices.push_back(topLeft);
                }
            }

            TerrainChunk chunk{};
            chunk.triangleIndexCount = static_cast<uint32_t>(triIndices.size());
            chunk.patchIndexCount = static_cast<uint32_t>(patchIndices.size());
            chunk.chunkUVOffset = {
                float(gx) / float(m_desc.chunkCountX),
                float(gz) / float(m_desc.chunkCountZ)
            };
            chunk.chunkUVScale = {
                1.f / float(m_desc.chunkCountX),
                1.f / float(m_desc.chunkCountZ)
            };

            // TODO: GPU upload:
            // - create vertex buffer
            // - create triangle index buffer
            // - create patch index buffer
            // - fill VB/IB views

            const uint32_t expectedVertexCount = vertsPerEdge * vertsPerEdge;
            const uint32_t expectedQuadCount = (vertsPerEdge - 1) * (vertsPerEdge - 1);
            const uint32_t expectedTriIndexCount = expectedQuadCount * 2 * 3;
            const uint32_t expectedPatchIndexCount = expectedQuadCount * 4;

            assert(vertices.size() == expectedVertexCount);
            assert(triIndices.size() == expectedTriIndexCount);
            assert(patchIndices.size() == expectedPatchIndexCount);
            assert(aabbMin.x <= aabbMax.x);
            assert(aabbMin.y <= aabbMax.y);
            assert(aabbMin.z <= aabbMax.z);

            chunk.key = ChunkKey {
                .gridX = gx,
                .gridZ = gz,
                .index = chunkIndex
            };
            chunk.bounds.aabb = NSMath::SBoundAABB {
                .min = aabbMin,
                .max = aabbMax
            };
            m_chunks.push_back(std::move(chunk));
        }
    }

    assert(m_chunks.size() == static_cast<size_t>(m_desc.chunkCountX) * m_desc.chunkCountZ);

    // TODO: Remove after
    {
        g_FDebug("m_chunks.size():%zu\n", m_chunks.size());

        {
            TerrainChunk& fstchunk = m_chunks[0];
            DirectX::XMFLOAT3& fstAabbMin = fstchunk.bounds.aabb.min;
            DirectX::XMFLOAT3& fstAabbMax = fstchunk.bounds.aabb.max;

            g_FDebug("chunks%zu : triangleIndexCount:%u\n", fstchunk.key.index, fstchunk.triangleIndexCount);
            g_FDebug("chunks%zu : patchIndexCount:%u\n", fstchunk.key.index,  fstchunk.patchIndexCount);
            g_FDebug("chunks%zu.bounds : aabbMin.y == %.2f\n", fstchunk.key.index,  fstAabbMin.y);
            g_FDebug("chunks%zu.bounds : aabbMax.y <= maxHeight == %.2f <= %.2f == %s\n", fstchunk.key.index,  fstAabbMax.y, m_desc.maxHeight, NSMath::fLessEqual(fstAabbMax.y, m_desc.maxHeight) ? "TRUE" : "FALSE");
            g_FDebug("chunks%zu.bounds : aabbMax.y > aabbMin.y == %.2f > %.2f == %s\n", fstchunk.key.index,  fstAabbMax.y, fstAabbMin.y, NSMath::fGreaterThan(fstAabbMax.y, fstAabbMin.y) ? "TRUE" : "FALSE");
        }

        {
            TerrainChunk& lstchunk = m_chunks[m_chunks.size() - 1u];
            DirectX::XMFLOAT3& lstAabbMin = lstchunk.bounds.aabb.min;
            DirectX::XMFLOAT3& lstAabbMax = lstchunk.bounds.aabb.max;

            g_FDebug("chunks%zu : triangleIndexCount:%u\n", lstchunk.key.index, lstchunk.triangleIndexCount);
            g_FDebug("chunks%zu : patchIndexCount:%u\n", lstchunk.key.index,  lstchunk.patchIndexCount);
            g_FDebug("chunks%zu.bounds : aabbMin.y == %.2f\n", lstchunk.key.index,  lstAabbMin.y);
            g_FDebug("chunks%zu.bounds : aabbMax.y <= maxHeight == %.2f <= %.2f == %s\n", lstchunk.key.index,  lstAabbMax.y, m_desc.maxHeight, NSMath::fLessEqual(lstAabbMax.y, m_desc.maxHeight) ? "TRUE" : "FALSE");
            g_FDebug("chunks%zu.bounds : aabbMax.y > aabbMin.y == %.2f > %.2f == %s\n", lstchunk.key.index,  lstAabbMax.y, lstAabbMin.y, NSMath::fGreaterThan(lstAabbMax.y, lstAabbMin.y) ? "TRUE" : "FALSE");
        }
    }
}
float Terrain::SampleHeight(float u, float v) const
{
    assert(not m_cpuHeightData.empty());
    if (m_hmWidth == 0 || m_hmHeight == 0) return 0.0f;

    u = NSMath::Saturate(u);
    v = NSMath::Saturate(v);

    const float x = u * float(m_hmWidth - 1);
    const float z = v * float(m_hmHeight - 1);

    const uint32_t x0 = static_cast<uint32_t>(std::floor(x));
    const uint32_t z0 = static_cast<uint32_t>(std::floor(z));

    const uint32_t x1 = std::min(x0 + 1, m_hmWidth - 1);
    const uint32_t z1 = std::min(z0 + 1, m_hmHeight - 1);

    const float tx = x - float(x0);
    const float tz = z - float(z0);

    const auto At = [this](uint32_t sampleX, uint32_t sampleZ)
    {
        return m_cpuHeightData[size_t(sampleZ) * size_t(m_hmWidth) + size_t(sampleX)];
    };

    const float h00 = At(x0, z0);
    const float h10 = At(x1, z0);
    const float h01 = At(x0, z1);
    const float h11 = At(x1, z1);

    const float hx0 = NSMath::Lerp(h00, h10, tx);
    const float hx1 = NSMath::Lerp(h01, h11, tx);

    return NSMath::Lerp(hx0, hx1, tz);
}
