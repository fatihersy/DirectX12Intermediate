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
    , m_hmWidth(other.m_hmWidth)
    , m_hmHeight(other.m_hmHeight)
    , m_heightmapTextureDefault(std::move(other.m_heightmapTextureDefault))
    , m_heightmapTextureUpload(std::move(other.m_heightmapTextureUpload))
    , m_heightmapSRV(other.m_heightmapSRV)
    , m_isOnGPU(other.m_isOnGPU)
    , m_initialized(other.m_initialized)
{
    other.m_device = nullptr;
    other.m_desc = {};
    other.m_hmWidth = 0;
    other.m_hmHeight = 0;
    other.m_heightmapSRV = {};
    other.m_isOnGPU = false;
    other.m_initialized = false;
}

Terrain& Terrain::operator=(Terrain&& other) noexcept
{
    if (this != &other)
    {
        m_device = other.m_device;
        m_desc = other.m_desc;
        m_chunks = std::move(other.m_chunks);
        m_heightR16 = std::move(other.m_heightR16);
        m_hmWidth = other.m_hmWidth;
        m_hmHeight = other.m_hmHeight;
        m_heightmapTextureDefault = std::move(other.m_heightmapTextureDefault);
        m_heightmapTextureUpload = std::move(other.m_heightmapTextureUpload);
        m_heightmapSRV = other.m_heightmapSRV;
        m_isOnGPU = other.m_isOnGPU;
        m_initialized = other.m_initialized;

        other.m_device = nullptr;
        other.m_desc = {};
        other.m_hmWidth = 0;
        other.m_hmHeight = 0;
        other.m_heightmapSRV = {};
        other.m_isOnGPU = false;
        other.m_initialized = false;
    }

    return *this;
}

void Terrain::OnInit(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx, NSTerrain::TerrainDesc desc)
{
    if (m_initialized) {
        g_FWarn("Terrain::OnInit::Calling OnInit function twice without Destroy first");
        return;
    }

    Generate(cmdList, rendererCtx, desc);

    Upload(cmdList, rendererCtx);

    m_initialized = true;
}
void Terrain::OnDestroy(NSRenderer::Ctx rendererCtx)
{
    Destroy(rendererCtx);
}

void Terrain::Generate(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx, NSTerrain::TerrainDesc desc)
{
    this->m_desc = desc;

    InitAndGenerateHeightmap();
}
void Terrain::Destroy(NSRenderer::Ctx rendererCtx)
{
    m_device = nullptr;

    rendererCtx.freeSRVStatic(m_heightmapSRV);
    m_heightmapSRV = NSDescriptor::Handle{};

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
    m_heightmapTextureDefault.Reset();
    m_heightmapTextureUpload.Reset();
    m_hmWidth = 0;
    m_hmHeight = 0;
    m_desc = {};

    m_isOnGPU = false;
    m_initialized = false;
}

void Terrain::InitAndGenerateHeightmap(uint32_t seed)
{
    m_hmWidth = m_desc.heightMapResolution;
    m_hmHeight = m_desc.heightMapResolution;

    m_heightR16.clear();
    m_heightR16.resize(size_t(m_hmWidth) * size_t(m_hmHeight));

    constexpr float baseFrequency = 8.0f;

    for (uint32_t z = 0; z < m_hmHeight; ++z)
    {
        for (uint32_t x = 0; x < m_hmWidth; ++x)
        {
            const float u = float(x) / float(m_hmWidth - 1);
            const float v = float(z) / float(m_hmHeight - 1);

            float h = NSMath::FBm(u * baseFrequency, v * baseFrequency, seed);

            // Pow is an optional shaping: more lowlands, slightly sharper peaks.
            h = NSMath::Saturate(std::pow(h, 1.35f)) ;

            const float hScaled = h * std::numeric_limits<uint16_t>::max();

            const uint16_t hInt = static_cast<uint16_t>(std::round(hScaled));

            m_heightR16[size_t(z) * size_t(m_hmWidth) + size_t(x)] = hInt;
        }
    }

    if(not m_heightmapTextureDefault)
    {
        CD3DX12_RESOURCE_DESC desc(CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16_UNORM, m_hmWidth, m_hmHeight, 1, 1));
        CD3DX12_HEAP_PROPERTIES defaultProps(D3D12_HEAP_TYPE_DEFAULT);

        ThrowIfFailed(m_device->CreateCommittedResource(
            &defaultProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&m_heightmapTextureDefault)
        ));
        m_heightmapTextureDefault->SetName(L"NSTerrain::Terrain::m_heightmapTextureDefault");
    }

    if(not m_heightmapTextureUpload)
    {
        const UINT64 dataSize = GetRequiredIntermediateSize(m_heightmapTextureDefault.Get(), 0, 1);
        const CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(dataSize);
        const CD3DX12_HEAP_PROPERTIES props(D3D12_HEAP_TYPE_UPLOAD);

        ThrowIfFailed(m_device->CreateCommittedResource(
            &props,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_heightmapTextureUpload)
        ));
        m_heightmapTextureUpload->SetName(L"NSTerrain::Terrain::m_heightmapTextureUpload");
    }
}
void Terrain::Upload(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx)
{
    if (m_isOnGPU) {
        g_FWarn("Terrain::Upload::Calling Upload function twice without Unload first");
        return;
    };
    assert(m_heightmapTextureDefault and m_heightmapTextureUpload and "Heightmap should be generated first");

    D3D12_SUBRESOURCE_DATA data{};
    data.pData = m_heightR16.data();
    data.RowPitch = static_cast<LONG_PTR>(m_hmWidth * sizeof(uint16_t));
    data.SlicePitch = data.RowPitch * m_hmHeight;

    CD3DX12_RESOURCE_BARRIER toCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(
        m_heightmapTextureDefault.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST
    );
    cmdList.ResourceBarrier(1, &toCopyDest);

    cmdList.UpdateSubresources(m_heightmapTextureDefault.Get(), m_heightmapTextureUpload.Get(), 0, 0, 1, &data);

    CD3DX12_RESOURCE_BARRIER toShaderRead = CD3DX12_RESOURCE_BARRIER::Transition(
        m_heightmapTextureDefault.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );
    cmdList.ResourceBarrier(1, &toShaderRead);

    m_heightmapSRV = rendererCtx.allocSRVStatic(1u);
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        desc.Format = DXGI_FORMAT_R16_UNORM;
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MostDetailedMip = 0;
        desc.Texture2D.MipLevels = 1;
        desc.Texture2D.PlaneSlice = 0;
        desc.Texture2D.ResourceMinLODClamp = 0.f;

        m_device->CreateShaderResourceView(m_heightmapTextureDefault.Get(), &desc, m_heightmapSRV.cpuAddr);
    }

    BuildChunks(cmdList, rendererCtx);

    m_isOnGPU = true;
}

void Terrain::Unload(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx)
{
    rendererCtx.freeSRVStatic(m_heightmapSRV);
    m_heightmapSRV = {};

    m_heightmapTextureDefault.Reset();

    m_heightmapTextureUpload.Reset();

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

void Terrain::BuildChunks(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx)
{
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

                ThrowIfFailed(m_device->CreateCommittedResource(
                    &props,
                    D3D12_HEAP_FLAG_NONE,
                    &desc,
                    D3D12_RESOURCE_STATE_GENERIC_READ,
                    nullptr,
                    IID_PPV_ARGS(&buffer)
                ));
                buffer->SetName(name.data());

                void* ppData = nullptr;
                ThrowIfFailed(buffer->Map(0u, nullptr, reinterpret_cast<void**>(&ppData)));
                memcpy(ppData, data, dataSize);
                buffer->Unmap(0u, nullptr);
            };
            auto createDefBuffer = [this](std::wstring_view name, size_t dataSize, ComPtr<ID3D12Resource>& buffer)
            {
                const CD3DX12_HEAP_PROPERTIES props(D3D12_HEAP_TYPE_DEFAULT);
                const CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(dataSize);

                ThrowIfFailed(m_device->CreateCommittedResource(
                    &props,
                    D3D12_HEAP_FLAG_NONE,
                    &desc,
                    D3D12_RESOURCE_STATE_COMMON,
                    nullptr,
                    IID_PPV_ARGS(&buffer)
                ));
                buffer->SetName(name.data());
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

            assert(vertices.size() == expectedVertexCount);
            assert(triIndices.size() == expectedTriIndexCount);
            assert(patchIndices.size() == expectedPatchIndexCount);
            assert(aabbMin.x <= aabbMax.x);
            assert(aabbMin.y <= aabbMax.y);
            assert(aabbMin.z <= aabbMax.z);

            m_chunks.push_back(std::move(chunk));
        }
    }

    assert(m_chunks.size() == static_cast<size_t>(m_desc.chunkCountX) * m_desc.chunkCountZ);

    TerrainChunk& fstchunk = m_chunks[0];
    DirectX::XMFLOAT3& fstAabbMin = fstchunk.bounds.aabb.min;
    DirectX::XMFLOAT3& fstAabbMax = fstchunk.bounds.aabb.max;
    assert(NSMath::fLessEqual(fstAabbMax.y, m_desc.maxHeight));
    assert(NSMath::fGreaterThan(fstAabbMax.y, fstAabbMin.y));

    TerrainChunk& lstchunk = m_chunks[m_chunks.size() - 1u];
    DirectX::XMFLOAT3& lstAabbMin = lstchunk.bounds.aabb.min;
    DirectX::XMFLOAT3& lstAabbMax = lstchunk.bounds.aabb.max;
    assert(NSMath::fLessEqual(lstAabbMax.y, m_desc.maxHeight));
    assert(NSMath::fGreaterThan(lstAabbMax.y, lstAabbMin.y));
}
float Terrain::SampleHeight(float u, float v) const
{
    assert(not m_heightR16.empty());
    if (m_hmWidth == 0 || m_hmHeight == 0) return 0.0f;

    u = NSMath::Saturate(u);
    v = NSMath::Saturate(v);

    const float x = u * static_cast<float>(m_hmWidth - 1);
    const float z = v * static_cast<float>(m_hmHeight - 1);

    const uint32_t x0 = static_cast<uint32_t>(std::floor(x));
    const uint32_t z0 = static_cast<uint32_t>(std::floor(z));

    const uint32_t x1 = std::min(x0 + 1, m_hmWidth - 1);
    const uint32_t z1 = std::min(z0 + 1, m_hmHeight - 1);

    const float tx = x - static_cast<float>(x0);
    const float tz = z - static_cast<float>(z0);

    const auto At = [this](uint32_t sampleX, uint32_t sampleZ)
    {
        const uint16_t h = m_heightR16[size_t(sampleZ) * size_t(m_hmWidth) + size_t(sampleX)];
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
