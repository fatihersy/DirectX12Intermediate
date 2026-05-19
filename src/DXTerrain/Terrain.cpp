#include "stdafx.h"
#include "Terrain.h"

using namespace NSTerrain;

#include "DXSampleHelper.h"

Terrain::Terrain() {}
Terrain::Terrain(ID3D12Device14* device) : m_device(device)
{

}

Terrain::Terrain(Terrain&& other) noexcept
    : m_device(other.m_device)
    , m_desc(other.m_desc)
    , m_chunks(std::move(other.m_chunks))
    , m_heightR16(std::move(other.m_heightR16))
    , m_textures(std::move(other.m_textures))
    , m_isOnGPU(other.m_isOnGPU)
    , m_isOnCPU(other.m_isOnCPU)
    , m_isInitialized(other.m_isInitialized)
{
    other.m_device = nullptr;
    other.m_desc = {};
    other.m_textures.fill({});
    other.m_isOnGPU = false;
    other.m_isOnCPU = false;
    other.m_isInitialized = false;
}

Terrain& Terrain::operator=(Terrain&& other) noexcept
{
    if (this != &other)
    {
        m_device = other.m_device;
        m_desc = other.m_desc;
        m_chunks = std::move(other.m_chunks);
        m_heightR16 = std::move(other.m_heightR16);
        m_textures = std::move(other.m_textures);
        m_isOnGPU = other.m_isOnGPU;
        m_isOnCPU = other.m_isOnCPU;
        m_isInitialized = other.m_isInitialized;

        other.m_device = nullptr;
        other.m_desc = {};
        other.m_textures.fill({});
        other.m_isOnGPU = false;
        other.m_isOnCPU = false;
        other.m_isInitialized = false;
    }

    return *this;
}

void Terrain::OnInit(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx, uint32_t seed, NSTerrain::TerrainDesc desc)
{
    if (m_isInitialized) {
        g_FWarn("Terrain::OnInit::Calling OnInit function twice without Destroy first");
        return;
    }

    Generate(seed, desc);

    BuildChunks(cmdList, rendererCtx);
    Upload(cmdList, rendererCtx);

    m_isOnCPU = true;
    m_isInitialized = true;
}
bool Terrain::OnInit(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx, const std::wstring_view path)
{
    if (m_isInitialized) {
        g_FWarn("Terrain::OnInit::Calling OnInit function twice without Destroy first");
        return false;
    }

    if (not Load(path)) return false;

    m_isOnCPU = true;

    BuildChunks(cmdList, rendererCtx);

    Upload(cmdList, rendererCtx);

    m_isInitialized = true;
    return true;
}
bool Terrain::OnInit(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx, const std::wstring_view path, NSTerrain::TerrainDesc desc)
{
    if (m_isInitialized) {
        g_FWarn("Terrain::OnInit::Calling OnInit function twice without Destroy first");
        return false;
    }

    m_desc = desc;

    if (not Load(path)) return false;

    BuildChunks(cmdList, rendererCtx);
    Upload(cmdList, rendererCtx);

    m_isInitialized = true;
    return true;
}
void Terrain::OnDestroy(NSRenderer::Ctx rendererCtx)
{
    m_device = nullptr;

    for (NSTexture::Texture& tex : m_textures)
    {
        tex = NSTexture::Texture{};
    }

    if (m_texSrvHandle.amount > 0) rendererCtx.freeSRVStatic(m_texSrvHandle);
    m_texSrvHandle = {};

    for (auto& chunk : m_chunks)
    {
        chunk.vertexDefault.Reset();
        chunk.vertexUpload.Reset();
        chunk.triangleIndexDefault.Reset();
        chunk.triangleIndexUpload.Reset();
        chunk.patchIndexDefault.Reset();
        chunk.patchIndexUpload.Reset();
    }

    m_chunks.clear();
    m_chunks.shrink_to_fit();
    m_heightR16.clear();
    m_heightR16.shrink_to_fit();
    m_desc = {};

    m_isOnGPU = false;
    m_isOnCPU = false;
    m_isInitialized = false;
}

void Terrain::Generate(uint32_t seed, NSTerrain::TerrainDesc desc)
{
    this->m_desc = desc;

    ASSERT(m_desc.heightMapResolution > 1u);

    const uint32_t resolution = m_desc.heightMapResolution;

    m_heightR16.clear();
    m_heightR16.resize(size_t(resolution) * size_t(resolution));

    constexpr float baseFrequency = 8.0f;

    for (uint32_t z = 0; z < resolution; ++z)
    {
        for (uint32_t x = 0; x < resolution; ++x)
        {
            const float u = float(x) / float(resolution - 1);
            const float v = float(z) / float(resolution - 1);

            float h = NSMath::FBm(u * baseFrequency, v * baseFrequency, seed);

            // Pow is an optional shaping: more lowlands, slightly sharper peaks.
            h = NSMath::Saturate(std::pow(h, 1.35f)) ;

            const float hScaled = h * std::numeric_limits<uint16_t>::max();

            const uint16_t hInt = static_cast<uint16_t>(std::round(hScaled));

            m_heightR16[size_t(z) * size_t(resolution) + size_t(x)] = hInt;
        }
    }

    NSTexture::Texture& heightmap = m_textures[static_cast<size_t>(TextureIDs::HEIGHTMAP)];
    heightmap = {};
    heightmap.desc = NSTexture::GetTextureDesc(NSTexture::EType::EType_HEIGHT);
    heightmap.textureType = NSTexture::EType::EType_HEIGHT;
    heightmap.format = heightmap.desc.format;
    heightmap.width = resolution;
    heightmap.height = resolution;

    const UINT alignment = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
    const UINT64 unalignedRowPitch = static_cast<UINT64>(resolution) * sizeof(uint16_t);
    const UINT64 alignedRowPitch = (unalignedRowPitch + alignment - 1u) & ~(static_cast<UINT64>(alignment) - 1u);
    const UINT64 uploadSize = alignedRowPitch * resolution;

    ASSERT(alignedRowPitch <= UINT_MAX);
    ASSERT(uploadSize <= UINT_MAX);

    heightmap.RowPitch = static_cast<UINT>(alignedRowPitch);
    heightmap.cpuData.resize(static_cast<size_t>(uploadSize));

    for (uint32_t y = 0; y < resolution; ++y)
    {
        std::byte* dst = heightmap.cpuData.data() + static_cast<size_t>(y) * heightmap.RowPitch;
        const std::byte* src = reinterpret_cast<const std::byte*>(m_heightR16.data() + static_cast<size_t>(y) * resolution);
        memcpy(dst, src, static_cast<size_t>(unalignedRowPitch));
    }

    D3D12_HEAP_PROPERTIES uploadHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
    ThrowIfFailed(m_device->CreateCommittedResource(
        &uploadHeapProp,
        D3D12_HEAP_FLAG_NONE,
        &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&heightmap.uploadBuffer)
    ));

    void* mapped = nullptr;
    ThrowIfFailed(heightmap.uploadBuffer->Map(0, nullptr, &mapped));
    memcpy(mapped, heightmap.cpuData.data(), static_cast<size_t>(uploadSize));
    heightmap.uploadBuffer->Unmap(0, nullptr);

    D3D12_HEAP_PROPERTIES defaultHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        heightmap.format,
        heightmap.width,
        heightmap.height,
        1,
        heightmap.desc.mipLevels
    );
    ThrowIfFailed(m_device->CreateCommittedResource(
        &defaultHeapProp,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        heightmap.desc.initialState,
        nullptr,
        IID_PPV_ARGS(&heightmap.defaultBuffer)
    ));

    heightmap.defaultBuffer->SetName(L"NSTerrain::Terrain::GeneratedHeightmap::defaultBuffer");
    heightmap.uploadBuffer->SetName(L"NSTerrain::Terrain::GeneratedHeightmap::uploadBuffer");
    heightmap.m_isOnCPU = true;
    m_isOnCPU = true;
}
bool Terrain::Load(const std::wstring_view path)
{
    ASSERT(m_desc.worldWidth > 0.f and m_desc.worldDepth > 0.f and m_desc.maxHeight > 0.f and m_desc.chunkCountX > 0u and m_desc.chunkCountZ > 0u and m_desc.vertsPerChunkEdge > 1u);

    NSTexture::Texture& heightmap = m_textures[static_cast<size_t>(TextureIDs::HEIGHTMAP)];
    heightmap = NSTexture::LoadTexture(L"NSTerrain::Terrain::Heightmap", path, NSTexture::EType::EType_HEIGHT);

    ASSERT(heightmap.defaultBuffer and heightmap.uploadBuffer and not heightmap.cpuData.empty(), "Failed to load heightmap texture data");
    ASSERT(heightmap.format == DXGI_FORMAT_R16_UNORM);
    ASSERT(heightmap.desc.bytesPerPixel == sizeof(uint16_t));
    ASSERT(heightmap.width == heightmap.height, "Terrain currently expects a square heightmap");

    m_desc.heightMapResolution = heightmap.width;

    m_heightR16.resize(size_t(heightmap.width) * size_t(heightmap.height));

    const size_t srcRowBytes = static_cast<size_t>(heightmap.RowPitch);
    const size_t dstRowBytes = static_cast<size_t>(heightmap.width) * sizeof(uint16_t);

    for (uint32_t y = 0; y < heightmap.height; ++y)
    {
        const std::byte* src = heightmap.cpuData.data() + size_t(y) * srcRowBytes;
        std::byte* dst = reinterpret_cast<std::byte*>(m_heightR16.data() + size_t(y) * heightmap.width);
        memcpy(dst, src, dstRowBytes);
    }

    m_isOnCPU = true;
    return true;
}

void Terrain::Upload(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx)
{
    ASSERT(m_isOnCPU, "No CPU data to upload");

    NSTexture::Texture& heightmap = m_textures[static_cast<size_t>(TextureIDs::HEIGHTMAP)];
    ASSERT(heightmap.defaultBuffer and heightmap.uploadBuffer, "m_isOnCPU set but heightmap buffers are not valid.");

    m_texSrvHandle = rendererCtx.allocSRVStatic(1u);
    heightmap.srvOffset = rendererCtx.offsetSRV(m_texSrvHandle, 0u);

    NSTexture::UploadGPU(
        rendererCtx,
        cmdList,
        heightmap,
        true,
        true,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );

    m_isOnGPU = true;
}

void Terrain::Free(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx)
{
    if (m_texSrvHandle.amount > 0) rendererCtx.freeSRVStatic(m_texSrvHandle);
    m_texSrvHandle = {};

    for (NSTexture::Texture& tex : m_textures) tex = NSTexture::Texture{};

    for (auto& chunk : m_chunks)
    {
        chunk.vertexDefault.Reset();
        chunk.vertexUpload.Reset();
        chunk.triangleIndexDefault.Reset();
        chunk.triangleIndexUpload.Reset();
        chunk.patchIndexDefault.Reset();
        chunk.patchIndexUpload.Reset();
    }

    m_chunks.clear();
    m_chunks.shrink_to_fit();

    m_isOnGPU = false;
}
void Terrain::Unload(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx)
{
    Free(cmdList, rendererCtx);
}
void Terrain::ReleaseUploadBuffers()
{
    for (NSTexture::Texture& tex : m_textures)
    {
        tex.uploadBuffer.Reset();
        tex.cpuData.clear();
        tex.cpuData.shrink_to_fit();
    }

    for (auto& chunk : m_chunks)
    {
        chunk.vertexUpload.Reset();
        chunk.triangleIndexUpload.Reset();
        chunk.patchIndexUpload.Reset();
    }

    m_isOnCPU = false;
}

void Terrain::BuildChunks(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx)
{
    ASSERT(not m_heightR16.empty());

    const uint32_t vertsPerEdge = m_desc.vertsPerChunkEdge;
    const uint32_t quadsPerEdge = vertsPerEdge - 1;

    const uint32_t totalVertsX = m_desc.chunkCountX * quadsPerEdge + 1;
    const uint32_t totalVertsZ = m_desc.chunkCountZ * quadsPerEdge + 1;

    m_chunks.clear();
    m_chunks.shrink_to_fit();

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

            std::vector<NSTerrain::Vertex> vertices;
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

                    NSTerrain::Vertex vertex{};
                    vertex.position = { x, y, z };
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
            chunk.key = ChunkKey {
                .gridX = gx,
                .gridZ = gz,
                .index = chunkIndex
            };
            chunk.bounds.aabb = NSMath::SBoundAABB {
                .min = aabbMin,
                .max = aabbMax
            };

            std::wstring chunkName = NSTool::wformat(L"NSTerrain::Terrain::Chunk%zu", chunkIndex);

            auto createUplBufferAndUpload = [this](std::wstring_view name, size_t dataSize, void* data, ComPtr<ID3D12Resource>& buffer)
            {
                const CD3DX12_HEAP_PROPERTIES props(D3D12_HEAP_TYPE_UPLOAD);
                const CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(dataSize);

                if (not buffer)
                {
                    ThrowIfFailed(m_device->CreateCommittedResource(
                        &props,
                        D3D12_HEAP_FLAG_NONE,
                        &desc,
                        D3D12_RESOURCE_STATE_GENERIC_READ,
                        nullptr,
                        IID_PPV_ARGS(&buffer)
                    ));
                    buffer->SetName(name.data());
                }

                void* ppData = nullptr;
                ThrowIfFailed(buffer->Map(0u, nullptr, reinterpret_cast<void**>(&ppData)));
                memcpy(ppData, data, dataSize);
                buffer->Unmap(0u, nullptr);
            };
            auto createDefBuffer = [this](std::wstring_view name, size_t dataSize, ComPtr<ID3D12Resource>& buffer)
            {
                const CD3DX12_HEAP_PROPERTIES props(D3D12_HEAP_TYPE_DEFAULT);
                const CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(dataSize);

                if (not buffer)
                {
                    ThrowIfFailed(m_device->CreateCommittedResource(
                        &props,
                        D3D12_HEAP_FLAG_NONE,
                        &desc,
                        D3D12_RESOURCE_STATE_COMMON,
                        nullptr,
                        IID_PPV_ARGS(&buffer)
                    ));
                    buffer->SetName(name.data());
                }
            };

            // Vertices
            {
                size_t dataSize = vertices.size() * sizeof(NSTerrain::Vertex);
                createUplBufferAndUpload(NSTool::wformat(L"%s::%s", chunkName.c_str(), L"vertexUpload").c_str(), dataSize, vertices.data(), chunk.vertexUpload);
                createDefBuffer(NSTool::wformat(L"%s::%s", chunkName.c_str(), L"vertexDefault").c_str(), dataSize, chunk.vertexDefault);
                chunk.vertexBufferView.BufferLocation = chunk.vertexDefault->GetGPUVirtualAddress();
                chunk.vertexBufferView.SizeInBytes = static_cast<UINT>(dataSize);
                chunk.vertexBufferView.StrideInBytes = sizeof(NSTerrain::Vertex);
            }

            // Triangle indices
            {
                size_t dataSize = chunk.triangleIndexCount * sizeof(UINT);
                createUplBufferAndUpload(NSTool::wformat(L"%s::%s", chunkName.c_str(), L"triangleIndexUpload").c_str(), dataSize, triIndices.data(), chunk.triangleIndexUpload);
                createDefBuffer(NSTool::wformat(L"%s::%s", chunkName.c_str(), L"triangleIndexDefault").c_str(), dataSize, chunk.triangleIndexDefault);
                chunk.triangleIndexBufferView.BufferLocation = chunk.triangleIndexDefault->GetGPUVirtualAddress();
                chunk.triangleIndexBufferView.SizeInBytes = static_cast<UINT>(dataSize);
                chunk.triangleIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
            }

            // Patch indices
            {
                size_t dataSize = chunk.patchIndexCount * sizeof(UINT);
                createUplBufferAndUpload(NSTool::wformat(L"%s::%s", chunkName.c_str(), L"patchIndexUpload").c_str(), dataSize, patchIndices.data(), chunk.patchIndexUpload);
                createDefBuffer(NSTool::wformat(L"%s::%s", chunkName.c_str(), L"patchIndexDefault").c_str(), dataSize, chunk.patchIndexDefault);
                chunk.patchIndexBufferView.BufferLocation = chunk.patchIndexDefault->GetGPUVirtualAddress();
                chunk.patchIndexBufferView.SizeInBytes = static_cast<UINT>(dataSize);
                chunk.patchIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
            }

            // Barriers
            {
                std::vector<D3D12_RESOURCE_BARRIER> preCopy;
                preCopy.reserve(3);

                preCopy.push_back(CD3DX12_RESOURCE_BARRIER::Transition(chunk.vertexDefault.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
                preCopy.push_back(CD3DX12_RESOURCE_BARRIER::Transition(chunk.triangleIndexDefault.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
                preCopy.push_back(CD3DX12_RESOURCE_BARRIER::Transition(chunk.patchIndexDefault.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));

                cmdList.ResourceBarrier(static_cast<UINT>(preCopy.size()), preCopy.data());
            }

            cmdList.CopyResource(chunk.vertexDefault.Get(), chunk.vertexUpload.Get());
            cmdList.CopyResource(chunk.triangleIndexDefault.Get(), chunk.triangleIndexUpload.Get());
            cmdList.CopyResource(chunk.patchIndexDefault.Get(), chunk.patchIndexUpload.Get());

            // Barriers
            {
                std::vector<D3D12_RESOURCE_BARRIER> postCopy;
                postCopy.reserve(3);

                postCopy.push_back(CD3DX12_RESOURCE_BARRIER::Transition(chunk.vertexDefault.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
                postCopy.push_back(CD3DX12_RESOURCE_BARRIER::Transition(chunk.triangleIndexDefault.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER));
                postCopy.push_back(CD3DX12_RESOURCE_BARRIER::Transition(chunk.patchIndexDefault.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER));

                cmdList.ResourceBarrier(static_cast<UINT>(postCopy.size()), postCopy.data());
            }

            const uint32_t expectedVertexCount = vertsPerEdge * vertsPerEdge;
            const uint32_t expectedQuadCount = (vertsPerEdge - 1) * (vertsPerEdge - 1);
            const uint32_t expectedTriIndexCount = expectedQuadCount * 2 * 3;
            const uint32_t expectedPatchIndexCount = expectedQuadCount * 4;

            ASSERT(vertices.size() == expectedVertexCount);
            ASSERT(triIndices.size() == expectedTriIndexCount);
            ASSERT(patchIndices.size() == expectedPatchIndexCount);
            ASSERT(aabbMin.x <= aabbMax.x);
            ASSERT(aabbMin.y <= aabbMax.y);
            ASSERT(aabbMin.z <= aabbMax.z);

            m_chunks.push_back(std::move(chunk));
        }
    }

    ASSERT(m_chunks.size() == static_cast<size_t>(m_desc.chunkCountX) * m_desc.chunkCountZ);

    TerrainChunk& fstchunk = m_chunks[0];
    DirectX::XMFLOAT3& fstAabbMin = fstchunk.bounds.aabb.min;
    DirectX::XMFLOAT3& fstAabbMax = fstchunk.bounds.aabb.max;
    ASSERT(NSMath::fLessEqual(fstAabbMax.y, m_desc.maxHeight));
    ASSERT(NSMath::fGreaterThan(fstAabbMax.y, fstAabbMin.y));

    TerrainChunk& lstchunk = m_chunks[m_chunks.size() - 1u];
    DirectX::XMFLOAT3& lstAabbMin = lstchunk.bounds.aabb.min;
    DirectX::XMFLOAT3& lstAabbMax = lstchunk.bounds.aabb.max;
    ASSERT(NSMath::fLessEqual(lstAabbMax.y, m_desc.maxHeight));
    ASSERT(NSMath::fGreaterThan(lstAabbMax.y, lstAabbMin.y));
}
float Terrain::SampleHeight(float u, float v) const
{
    ASSERT(not m_heightR16.empty());
    if (m_desc.heightMapResolution == 0) return 0.0f;

    u = NSMath::Saturate(u);
    v = NSMath::Saturate(v);

    float width = static_cast<float>(m_desc.heightMapResolution);
    float depth = static_cast<float>(m_desc.heightMapResolution);

    const float x = u * (width - 1.f);
    const float z = v * (depth - 1.f);

    const uint32_t x0 = static_cast<uint32_t>(std::floor(x));
    const uint32_t z0 = static_cast<uint32_t>(std::floor(z));

    const uint32_t x1 = std::min(x0 + 1, static_cast<uint32_t>(width - 1.f));
    const uint32_t z1 = std::min(z0 + 1, static_cast<uint32_t>(depth - 1.f));

    const float tx = x - static_cast<float>(x0);
    const float tz = z - static_cast<float>(z0);

    const auto At = [this, width, depth](uint32_t sampleX, uint32_t sampleZ)
    {
        const uint16_t h = m_heightR16[size_t(sampleZ) * size_t(width) + size_t(sampleX)];
        return static_cast<float>(h) / std::numeric_limits<uint16_t>::max();
    };

    const float h00 = At(x0, z0);
    const float h10 = At(x1, z0);
    const float h01 = At(x0, z1);
    const float h11 = At(x1, z1);

    const float hx0 = NSMath::Lerp(h00, h10, tx);
    const float hx1 = NSMath::Lerp(h01, h11, tx);

    return NSMath::Lerp(hx0, hx1, tz);
}
