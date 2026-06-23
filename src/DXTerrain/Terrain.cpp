#include "stdafx.h"
#include "Terrain.h"
#include "json.hpp"

#include "IApp.h"
#include "DXSampleHelper.h"

#include "Logger.h"

using namespace NSTerrain;
using Json = nlohmann::json;

TextureManifest JsonToTextureManifest(const Json& json)
{
    TextureManifest manifest{};
    manifest.relativePath = json.at(manifest.kJsonObj_file).get<std::string>();
    manifest.format = StringToDXGI_Format(json.at(manifest.kJsonObj_format).get<std::string>());
    manifest.width = json.at(manifest.kJsonObj_width).get<uint32_t>();
    manifest.height = json.at(manifest.kJsonObj_height).get<uint32_t>();

    const auto& channelsJson = json.at(manifest.kJsonObj_channels);
    ASSERT(channelsJson.is_array(), "Channels must be a JSON array");

    for (const auto& item : channelsJson)
    {
        std::string chStr = item.get<std::string>();
        ASSERT(not chStr.empty(), "Empty channel");

        manifest.channels.push_back(NSTexture::ChannelDeduction(chStr[0]));

        ASSERT(manifest.channels.back() != NSTexture::EChannel::ChUnknown, "Unknown channel");
    }

    ASSERT(!manifest.relativePath.empty(), "Terrain source texture file is empty");
    ASSERT(manifest.format != DXGI_FORMAT_UNKNOWN, "Terrain source texture format is invalid");
    ASSERT(!manifest.channels.empty(), "Terrain source texture channels are empty");

    return manifest;
};
PageTextureManifest JsonToPageTextureManifest(const Json& json)
{
    PageTextureManifest manifest{};
    manifest.sourceWidth = json.at(manifest.kJsonObj_sourceWidth).get<uint32_t>();
    manifest.sourceHeight = json.at(manifest.kJsonObj_sourceHeight).get<uint32_t>();
    manifest.format = StringToDXGI_Format(json.at(manifest.kJsonObj_format).get<std::string>());
    manifest.pageWidth = json.at(manifest.kJsonObj_pageWidth).get<uint32_t>();
    manifest.pageHeight = json.at(manifest.kJsonObj_pageHeight).get<uint32_t>();
    manifest.bytesPerPixel = json.at(manifest.kJsonObj_bytesPerPixel).get<uint32_t>();
    manifest.haloPixels = json.value(manifest.kJsonObj_haloPixels, static_cast<uint32_t>(0u));

    ASSERT(manifest.sourceWidth > 0, "Terrain source width cannot be 0");
    ASSERT(manifest.sourceHeight > 0, "Terrain source height cannot be 0");
    ASSERT(manifest.format != DXGI_FORMAT_UNKNOWN, "Terrain source texture format is invalid");
    ASSERT(manifest.pageWidth > 0, "Terrain page width cannot be 0");
    ASSERT(manifest.pageHeight > 0, "Terrain page width cannot be 0");
    ASSERT(manifest.bytesPerPixel == NSTexture::BytesPerPixel(manifest.format), "Terrain pixel bytes cannot be 0");

    return manifest;
};
Json TextureManifestToJson(const TextureManifest& manifest)
{
    Json json;
    json[manifest.kJsonObj_file] = manifest.relativePath.generic_string();
    json[manifest.kJsonObj_format] = DXGI_FormatToString(manifest.format);
    json[manifest.kJsonObj_width] = manifest.width;
    json[manifest.kJsonObj_height] = manifest.height;

    std::vector<std::string_view> channelsStr;

    for (NSTexture::EChannel channel : manifest.channels)
    {
        if (channel != NSTexture::ChUnknown and static_cast<size_t>(channel) < NSTexture::ChEnd)
        {
            channelsStr.push_back(NSTexture::GetChannelName(channel));
        }
    }

    json[manifest.kJsonObj_channels] = channelsStr;

    return json;
};
Json PageTextureManifestToJson(const PageTextureManifest& manifest)
{
    Json json;
    json[manifest.kJsonObj_sourceWidth] = manifest.sourceWidth;
    json[manifest.kJsonObj_sourceHeight] = manifest.sourceHeight;
    json[manifest.kJsonObj_format] = DXGI_FormatToString(manifest.format);
    json[manifest.kJsonObj_pageWidth] = manifest.pageWidth;
    json[manifest.kJsonObj_pageHeight] = manifest.pageHeight;
    json[manifest.kJsonObj_bytesPerPixel] = manifest.bytesPerPixel;
    json[manifest.kJsonObj_haloPixels] = manifest.haloPixels;

    return json;
};
NSTexture::EXRLoadDesc BuildEXRLoadDesc(const TextureManifest& manifest, NSTexture::EType textureType)
{
    const uint32_t componentCount = NSTexture::ComponentCount(manifest.format);

    ASSERT(manifest.channels.size() == componentCount, "Terrain source channel count must match the requested DXGI format");

    NSTexture::EXRLoadDesc desc{};
    desc.texDesc.textureType = textureType;
    desc.texDesc.format = manifest.format;
    desc.texDesc.localRowPitchMode = NSTexture::ERowPitchMode::TIGHT;
    desc.texDesc.uploadRowPitchMode = NSTexture::ERowPitchMode::DX_ALIGN;

    for (uint32_t component{}; component < componentCount; ++component)
    {
        desc.channels[component] = NSTexture::GetChannelName(manifest.channels[component]);
    }

    return desc;
}
void WriteTexturePageBin(NSTexture::Texture& texture, const std::string& path, NSMath::SRectU32 sourceRect, NSTexture::ECopyPixelEdgeMode edgeMode = NSTexture::ECopyPixelEdgeMode::UNDEFINED)
{
    ASSERT(texture.desc.bytesPerPixel == NSTexture::BytesPerPixel(texture.desc.format));
    ASSERT(sourceRect.width > 0u and sourceRect.height > 0u);

    std::filesystem::path _path = path;

    if(not std::filesystem::is_directory(_path.parent_path()))
    {
        std::filesystem::create_directory(_path.parent_path());
    }

    const UINT tightRowPitch = NSTexture::CalculateRowPitch(sourceRect.width, texture.desc.bytesPerPixel, NSTexture::ERowPitchMode::TIGHT);
    std::vector<std::byte> pageBytes(static_cast<size_t>(tightRowPitch) * sourceRect.height);

    if (edgeMode == NSTexture::ECopyPixelEdgeMode::UNDEFINED)
    {
        texture.CopyPixels(pageBytes.data(), tightRowPitch, sourceRect);
    }
    else {
        texture.CopyPixels(pageBytes.data(), tightRowPitch, sourceRect, edgeMode);
    }

    std::ofstream file(_path, std::ios::binary | std::ios::trunc);
    ASSERT(file.is_open(), "Failed to write terrain page texture");

    file.write(reinterpret_cast<const char*>(pageBytes.data()), static_cast<std::streamsize>(pageBytes.size()));
    ASSERT(file.good(), "Failed to finish writing terrain page texture");
}

static void WriteTexturePageBinHalo(
    NSTexture::Texture& src,
    const std::string& path,
    int64_t coreX, int64_t coreY,
    uint32_t coreW, uint32_t coreH,
    uint32_t halo)
{
    ASSERT(src.desc.bytesPerPixel == NSTexture::BytesPerPixel(src.desc.format));
    ASSERT(coreW > 0u and coreH > 0u);
    ASSERT(not src.chunks.empty());

    std::filesystem::path _path = path;
    if (not std::filesystem::is_directory(_path.parent_path()))
    {
        std::filesystem::create_directory(_path.parent_path());
    }

    const size_t bpp = src.desc.bytesPerPixel;
    const uint32_t fullW = coreW + 2u * halo;
    const uint32_t fullH = coreH + 2u * halo;
    const size_t rowPitch = static_cast<size_t>(fullW) * bpp;

    std::vector<std::byte> pageBytes(rowPitch * fullH);

    auto FindChunkRow = [&src](uint32_t sy) -> const std::byte*
    {
        for (const NSTexture::TextureChunk& chunk : src.chunks)
        {
            if (sy >= chunk.firstRow and sy - chunk.firstRow < chunk.rowCount)
            {
                return chunk.bytes.data() + static_cast<size_t>(sy - chunk.firstRow) * src.localRowPitch;
            }
        }
        return nullptr;
    };

    const int64_t srcMaxX = static_cast<int64_t>(src.width) - 1;
    const int64_t srcMaxY = static_cast<int64_t>(src.height) - 1;

    for (uint32_t r = 0u; r < fullH; ++r)
    {
        const int64_t syRaw = coreY - static_cast<int64_t>(halo) + static_cast<int64_t>(r);
        const uint32_t sy = static_cast<uint32_t>(std::clamp<int64_t>(syRaw, 0, srcMaxY));
        const std::byte* srcRow = FindChunkRow(sy);
        ASSERT(srcRow, "Halo extraction could not resolve a source row chunk");

        std::byte* dstRow = pageBytes.data() + static_cast<size_t>(r) * rowPitch;

        for (uint32_t c = 0u; c < fullW; ++c)
        {
            const int64_t sxRaw = coreX - static_cast<int64_t>(halo) + static_cast<int64_t>(c);
            const uint32_t sx = static_cast<uint32_t>(std::clamp<int64_t>(sxRaw, 0, srcMaxX));
            memcpy(dstRow + static_cast<size_t>(c) * bpp, srcRow + static_cast<size_t>(sx) * bpp, bpp);
        }
    }

    std::ofstream file(_path, std::ios::binary | std::ios::trunc);
    ASSERT(file.is_open(), "Failed to write terrain page texture");

    file.write(reinterpret_cast<const char*>(pageBytes.data()), static_cast<std::streamsize>(pageBytes.size()));
    ASSERT(file.good(), "Failed to finish writing terrain page texture");
}

static void WriteImpostorBin(
    NSTexture::Texture& src,
    const std::string& path,
    uint32_t targetW, uint32_t targetH)
{
    ASSERT(src.desc.bytesPerPixel == NSTexture::BytesPerPixel(src.desc.format));
    ASSERT(targetW > 0u and targetH > 0u);
    ASSERT(targetW <= src.width and targetH <= src.height);
    ASSERT(not src.chunks.empty());

    std::filesystem::path _path = path;
    if (not std::filesystem::is_directory(_path.parent_path()))
    {
        std::filesystem::create_directory(_path.parent_path());
    }

    const uint32_t bpp = src.desc.bytesPerPixel;
    const uint32_t componentCount = NSTexture::ComponentCount(src.desc.format);
    ASSERT(componentCount > 0u and componentCount <= 4u);
    const uint32_t bytesPerComponent = bpp / componentCount;
    ASSERT(bytesPerComponent == 1u or bytesPerComponent == 2u, "Impostor downsample supports 8- or 16-bit UNORM components");

    const size_t rowPitch = static_cast<size_t>(targetW) * bpp;
    std::vector<std::byte> outBytes(rowPitch * targetH);

    auto FindChunkRow = [&src](uint32_t sy) -> const std::byte*
    {
        for (const NSTexture::TextureChunk& chunk : src.chunks)
        {
            if (sy >= chunk.firstRow and sy - chunk.firstRow < chunk.rowCount)
            {
                return chunk.bytes.data() + static_cast<size_t>(sy - chunk.firstRow) * src.localRowPitch;
            }
        }
        return nullptr;
    };

    auto ReadComponent = [bytesPerComponent](const std::byte* p) -> uint32_t
    {
        if (bytesPerComponent == 2u)
        {
            uint16_t v{};
            memcpy(&v, p, sizeof(v));
            return v;
        }
        return static_cast<uint32_t>(static_cast<uint8_t>(p[0]));
    };
    auto WriteComponent = [bytesPerComponent](std::byte* p, uint32_t v)
    {
        if (bytesPerComponent == 2u)
        {
            const uint16_t out = static_cast<uint16_t>(v);
            memcpy(p, &out, sizeof(out));
        }
        else
        {
            const uint8_t out = static_cast<uint8_t>(v);
            memcpy(p, &out, sizeof(out));
        }
    };

    for (uint32_t absoluteY{}; absoluteY < targetH; ++absoluteY)
    {
        const uint32_t sampleYbegin = static_cast<uint32_t>(static_cast<uint64_t>(absoluteY) * src.height / targetH);
        uint32_t sampleYend = static_cast<uint32_t>(static_cast<uint64_t>(absoluteY + 1u) * src.height / targetH);
        if (sampleYend <= sampleYbegin) sampleYend = sampleYbegin + 1u;

        std::byte* dstRow = outBytes.data() + static_cast<size_t>(absoluteY) * rowPitch;

        for (uint32_t absoluteX{}; absoluteX < targetW; ++absoluteX)
        {
            const uint32_t sampleXbegin = static_cast<uint32_t>(static_cast<uint64_t>(absoluteX) * src.width / targetW);
            uint32_t sampleXend = static_cast<uint32_t>(static_cast<uint64_t>(absoluteX + 1u) * src.width / targetW);
            if (sampleXend <= sampleXbegin) sampleXend = sampleXbegin + 1u;

            uint64_t pixel[4] = { 0ull, 0ull, 0ull, 0ull };
            uint32_t sampleCount = 0u;

            for (uint32_t sampleY = sampleYbegin; sampleY < sampleYend; ++sampleY)
            {
                const std::byte* srcRow = FindChunkRow(sampleY);
                ASSERT(srcRow, "Impostor downsample could not resolve a source row chunk");

                for (uint32_t sampleX = sampleXbegin; sampleX < sampleXend; ++sampleX)
                {
                    const std::byte* px = srcRow + static_cast<size_t>(sampleX) * bpp;
                    for (uint32_t c = 0u; c < componentCount; ++c)
                    {
                        pixel[c] += ReadComponent(px + static_cast<size_t>(c) * bytesPerComponent);
                    }
                    ++sampleCount;
                }
            }

            ASSERT(sampleCount > 0u);
            std::byte* dstPx = dstRow + static_cast<size_t>(absoluteX) * bpp;
            for (uint32_t c = 0u; c < componentCount; ++c)
            {
                WriteComponent(dstPx + static_cast<size_t>(c) * bytesPerComponent, static_cast<uint32_t>(pixel[c] / sampleCount));
            }
        }
    }

    std::ofstream file(_path, std::ios::binary | std::ios::trunc);
    ASSERT(file.is_open(), "Failed to write terrain impostor texture");

    file.write(reinterpret_cast<const char*>(outBytes.data()), static_cast<std::streamsize>(outBytes.size()));
    ASSERT(file.good(), "Failed to finish writing terrain impostor texture");
}

template<typename StrClass>
static void WritePageName(StrClass& pagePathStr, size_t pageNameOffset, uint32_t coordX, uint32_t coordZ)
{
    ASSERT(pageNameOffset != std::string::npos, "Failed to parse path");

    const std::size_t pageXOffset = pageNameOffset + 5;
    const std::size_t pageZOffset = pageNameOffset + 8;

    auto WriteCoords = [&pagePathStr, &pageNameOffset,  &pageXOffset, &pageZOffset](uint32_t coordX, uint32_t coordZ)
    {
        if (typeid(StrClass) == typeid(std::string))
        {
            pagePathStr[pageXOffset + 0] = static_cast<char>('0' + (coordX / 10) % 10);
            pagePathStr[pageXOffset + 1] = static_cast<char>('0' + coordX % 10);

            pagePathStr[pageZOffset + 0] = static_cast<char>('0' + (coordZ / 10) % 10);
            pagePathStr[pageZOffset + 1] = static_cast<char>('0' + coordZ % 10);
        }
        else if (typeid(StrClass) == typeid(std::wstring))
        {
            pagePathStr[pageXOffset + 0] = static_cast<wchar_t>('0' + (coordX / 10) % 10);
            pagePathStr[pageXOffset + 1] = static_cast<wchar_t>('0' + coordX % 10);

            pagePathStr[pageZOffset + 0] = static_cast<wchar_t>('0' + (coordZ / 10) % 10);
            pagePathStr[pageZOffset + 1] = static_cast<wchar_t>('0' + coordZ % 10);
        }
        else {
            ASSERT(false, "Unsupported type");
        }
    };

    WriteCoords(coordX, coordZ);
}

Terrain::Terrain() {}
Terrain::Terrain(ID3D12Device14* device) : m_device(device) {}

Terrain::Terrain(Terrain&& other) noexcept
    : m_device(other.m_device)
    , m_desc(other.m_desc)
    , m_pages(std::move(other.m_pages))
    , m_textures(std::move(other.m_textures))
    , m_isInitialized(other.m_isInitialized)
{
    other.m_device = nullptr;
    other.m_desc = {};
    other.m_textures.fill({});
    other.m_isInitialized = false;
}

Terrain& Terrain::operator=(Terrain&& other) noexcept
{
    if (this != &other)
    {
        m_device = other.m_device;
        m_desc = other.m_desc;
        m_pages = std::move(other.m_pages);
        m_textures = std::move(other.m_textures);
        m_isInitialized = other.m_isInitialized;

        other.m_device = nullptr;
        other.m_desc = {};
        other.m_textures.fill({});
        other.m_isInitialized = false;
    }

    return *this;
}

bool Terrain::OnInit(NSDX12::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx, const std::string_view root, NSTerrain::TerrainDesc desc)
{
    if (m_isInitialized) {
        g_FWarn("Terrain::OnInit::Calling OnInit function twice without Destroy first");
        return false;
    }

    m_root = IApp::GetInstance()->im_assetsPath / root;
    NSBarrier::IBarrierBatch& barrierBatch = rendererCtx.barrierBatch.get();

    ASSERT(std::filesystem::is_directory(m_root), "Couldn't find the Terrain root: %s", m_root.generic_string());

    m_desc = desc;

    if (not LoadSourceManifest())
    {
        NSTexture::TextureMetadata heightmapMetadata = NSTexture::ProbeTexture(m_root / desc.heightmapDesc.relativePath);
        ASSERT(heightmapMetadata.success, "Couldn't find the heightmap texture: %s", m_root / kSourceFileHeightmap);

        NSTexture::TextureMetadata diffuseMetadata = NSTexture::ProbeTexture(m_root / desc.diffuseDesc.relativePath);
        ASSERT(diffuseMetadata.success, "Couldn't find the diffuse texture: %s", m_root / kSourceFileDiffuse);

        ASSERT(heightmapMetadata.width > 1u and heightmapMetadata.height > 1u);

        bool heightmapWidthValid = ((heightmapMetadata.width - 1u) % m_desc.pageCountX == 0u) || (heightmapMetadata.width % m_desc.pageCountX == 0u);
        bool heightmapHeightValid = ((heightmapMetadata.height - 1u) % m_desc.pageCountZ == 0u) || (heightmapMetadata.height % m_desc.pageCountZ == 0u);
        ASSERT(heightmapWidthValid, "Heightmap width must divide cleanly into terrain pages (either N*pageCount+1 or N*pageCount)");
        ASSERT(heightmapHeightValid, "Heightmap height must divide cleanly into terrain pages (either N*pageCount+1 or N*pageCount)");

        const uint32_t quadsPerPageX = (heightmapMetadata.width % m_desc.pageCountX == 0u)
            ? (heightmapMetadata.width / m_desc.pageCountX)
            : ((heightmapMetadata.width - 1u) / m_desc.pageCountX);
        const uint32_t quadsPerPageZ = (heightmapMetadata.height % m_desc.pageCountZ == 0u)
            ? (heightmapMetadata.height / m_desc.pageCountZ)
            : ((heightmapMetadata.height - 1u) / m_desc.pageCountZ);

        const uint32_t expectedWidth = quadsPerPageX * m_desc.pageCountX + 1u;
        const uint32_t expectedHeight = quadsPerPageZ * m_desc.pageCountZ + 1u;

        const float baseWidth = desc.worldWidth > 0.f ? desc.worldWidth : static_cast<float>(heightmapMetadata.width - 1u);
        const float baseDepth = desc.worldDepth > 0.f ? desc.worldDepth : static_cast<float>(heightmapMetadata.height - 1u);

        m_desc.worldWidth = baseWidth * (static_cast<float>(heightmapMetadata.width - 1u)  / static_cast<float>(expectedWidth - 1u));
        m_desc.worldDepth = baseDepth * (static_cast<float>(heightmapMetadata.height - 1u) / static_cast<float>(expectedHeight - 1u));
        m_desc.heightmapDesc.width = heightmapMetadata.width;
        m_desc.heightmapDesc.height = heightmapMetadata.height;
        m_desc.diffuseDesc.width = diffuseMetadata.width;
        m_desc.diffuseDesc.height = diffuseMetadata.height;

        for (NSTexture::EChannel reqChannel : desc.heightmapDesc.channels)
        {
            bool found{};
            for (NSTexture::EChannel channelFound : heightmapMetadata.channels)
            {
                if(reqChannel != channelFound) continue;

                found = true;
                break;
            }
            ASSERT(found, "Requested channel couldn't found in heightmap texture");
        }
        for (NSTexture::EChannel reqChannel : desc.diffuseDesc.channels)
        {
            bool found{};
            for (NSTexture::EChannel channelFound : diffuseMetadata.channels)
            {
                if(reqChannel == channelFound)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                g_FWarn("Diffuse channel %s not found in texture, falling back to default values", NSTexture::GetChannelName(reqChannel).data());
            }
        }

        srcManifest.name = root;
        srcManifest.pageCountX = m_desc.pageCountX;
        srcManifest.pageCountZ = m_desc.pageCountZ;
        srcManifest.worldWidth = m_desc.worldWidth;
        srcManifest.worldDepth = m_desc.worldDepth;
        srcManifest.maxHeight = m_desc.maxHeight;
        srcManifest.heightmap =
        {
            .relativePath = m_desc.heightmapDesc.relativePath,
            .format = m_desc.heightmapDesc.format,
            .channels = m_desc.heightmapDesc.channels,
            .width = m_desc.heightmapDesc.width,
            .height = m_desc.heightmapDesc.height
        };
        srcManifest.diffuse =
        {
            .relativePath = m_desc.diffuseDesc.relativePath,
            .format = m_desc.diffuseDesc.format,
            .channels = m_desc.diffuseDesc.channels,
            .width = m_desc.diffuseDesc.width,
            .height = m_desc.diffuseDesc.height
        };

        srcManifest.isPresent = true;

        SaveSourceManifest();
    }
    else
    {
        m_desc.worldWidth = srcManifest.worldWidth;
        m_desc.worldDepth = srcManifest.worldDepth;
        m_desc.maxHeight = srcManifest.maxHeight;
        m_desc.pageCountX = srcManifest.pageCountX;
        m_desc.pageCountZ = srcManifest.pageCountZ;
        m_desc.heightmapDesc.width = srcManifest.heightmap.width;
        m_desc.heightmapDesc.height = srcManifest.heightmap.height;
        m_desc.diffuseDesc.width = srcManifest.diffuse.width;
        m_desc.diffuseDesc.height = srcManifest.diffuse.height;
    }

    ASSERT(srcManifest.pageCountX < 100 and srcManifest.pageCountZ < 100, "Too many pages tried to create");

    if (PagesExists())
    {
        LoadPagesManifest();
    }
    if (not PageMatchesSource())
    {
        GeneratePageFromEXR();
    }

    const uint32_t heightHalo = pageManifest.height.haloPixels;
    const uint32_t heightCoreW = pageManifest.height.pageWidth - 2u * heightHalo;
    const uint32_t heightCoreH = pageManifest.height.pageHeight - 2u * heightHalo;

    if (m_desc.vertsPerChunkEdge == 0u)
    {
        m_desc.vertsPerChunkEdge = ((heightCoreW - 1u) / m_desc.chunkCountX) + 1u;
    }

    BuildPageLayout(m_pages);

    ASSERT(pageManifest.isPresent);
    ASSERT(m_pages.Size() == static_cast<size_t>(pageManifest.pageCountX) * pageManifest.pageCountZ);

    uint32_t streamingDistance = rendererCtx.rendererDesc.streamingDistance;
    ASSERT(streamingDistance > 0);

    const float worldPageWidth = m_desc.worldWidth / static_cast<float>(m_desc.pageCountX);
    const float worldPageDepth = m_desc.worldDepth / static_cast<float>(m_desc.pageCountZ);
    const int pageRadius = std::max(
        static_cast<int>(std::ceil(static_cast<float>(streamingDistance) / worldPageWidth)),
        static_cast<int>(std::ceil(static_cast<float>(streamingDistance) / worldPageDepth))
    );
    const int poolSide = (2 * pageRadius + 1) + 2; // resident block + one margin ring
    const uint32_t totalPages = m_desc.pageCountX * m_desc.pageCountZ;
    uint32_t poolSize = std::min(static_cast<uint32_t>(poolSide * poolSide), totalPages);

    m_gpuPool.Reserve(poolSize);
    m_poolView.Reserve(poolSize);

    const UINT vertexBufferSize = heightCoreW * heightCoreH * sizeof(Vertex);

    m_pendingCopyCount = poolSize;

    for (uint32_t i = 0; i < poolSize; ++i)
    {
        auto slot = m_gpuPool.Add();
        auto slotView = m_poolView.Add(std::make_shared<StreamSlotView>(StreamSlotView(slot->m_id)));
        slotView->ICullable_Id = slotView->m_id;

        slot->srvHandle = rendererCtx.allocSRVStatic(2u);
        slot->heightmapSRVIndex = rendererCtx.offsetSRV(slot->srvHandle, 0u).index;
        slot->diffuseSRVIndex = rendererCtx.offsetSRV(slot->srvHandle, 1u).index;

        std::wstring slotName = NSTool::wformat(L"NSTerrain::Terrain::GPUSlot_%u", i);

        // 1. Heightmap Default Heap Texture
        {
            const CD3DX12_HEAP_PROPERTIES props(D3D12_HEAP_TYPE_DEFAULT);
            D3D12_RESOURCE_DESC resDesc{};
            resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            resDesc.Width = pageManifest.height.pageWidth;
            resDesc.Height = pageManifest.height.pageHeight;
            resDesc.DepthOrArraySize = 1;
            resDesc.MipLevels = 1;
            resDesc.Format = pageManifest.height.format;
            resDesc.SampleDesc.Count = 1;
            resDesc.SampleDesc.Quality = 0;
            resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

            ThrowIfFailed(m_device->CreateCommittedResource(
                &props,
                D3D12_HEAP_FLAG_NONE,
                &resDesc,
                D3D12_RESOURCE_STATE_COMMON,
                nullptr,
                IID_PPV_ARGS(&slot->heightmapDefault)
            ));
            slot->heightmapDefault->SetName(NSTool::wformat(L"%s::HeightmapDefault", slotName.c_str()).c_str());

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = pageManifest.height.format;
            srvDesc.Texture2D.MostDetailedMip = 0u;
            srvDesc.Texture2D.MipLevels = 1u;
            srvDesc.Texture2D.PlaneSlice = 0u;
            srvDesc.Texture2D.ResourceMinLODClamp = 0.f;

            m_device->CreateShaderResourceView(slot->heightmapDefault.Get(), &srvDesc, rendererCtx.offsetSRV(slot->srvHandle, 0u).cpuAddr);
        }

        // 2. Diffuse Default Heap Texture
        {
            const CD3DX12_HEAP_PROPERTIES props(D3D12_HEAP_TYPE_DEFAULT);
            D3D12_RESOURCE_DESC resDesc{};
            resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            resDesc.Width = pageManifest.diffuse.pageWidth;
            resDesc.Height = pageManifest.diffuse.pageHeight;
            resDesc.DepthOrArraySize = 1;
            resDesc.MipLevels = 1;
            resDesc.Format = pageManifest.diffuse.format;
            resDesc.SampleDesc.Count = 1;
            resDesc.SampleDesc.Quality = 0;
            resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

            ThrowIfFailed(m_device->CreateCommittedResource(
                &props,
                D3D12_HEAP_FLAG_NONE,
                &resDesc,
                D3D12_RESOURCE_STATE_COMMON,
                nullptr,
                IID_PPV_ARGS(&slot->diffuseDefault)
            ));
            slot->diffuseDefault->SetName(NSTool::wformat(L"%s::DiffuseDefault", slotName.c_str()).c_str());

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = pageManifest.diffuse.format;
            srvDesc.Texture2D.MostDetailedMip = 0u;
            srvDesc.Texture2D.MipLevels = 1u;
            srvDesc.Texture2D.PlaneSlice = 0u;
            srvDesc.Texture2D.ResourceMinLODClamp = 0.f;

            m_device->CreateShaderResourceView(slot->diffuseDefault.Get(), &srvDesc, rendererCtx.offsetSRV(slot->srvHandle, 1u).cpuAddr);
        }

        // 3. Vertex Default Heap Buffer
        {
            const CD3DX12_HEAP_PROPERTIES props(D3D12_HEAP_TYPE_DEFAULT);
            const CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

            ThrowIfFailed(m_device->CreateCommittedResource(
                &props,
                D3D12_HEAP_FLAG_NONE,
                &resDesc,
                D3D12_RESOURCE_STATE_COMMON,
                nullptr,
                IID_PPV_ARGS(&slot->vertexDefault)
            ));
            slot->vertexDefault->SetName(NSTool::wformat(L"%s::VertexDefault", slotName.c_str()).c_str());

            barrierBatch.Add(NSBarrier::kTerrain_terrainOnInit, CD3DX12_RESOURCE_BARRIER::Transition(
                slot->vertexDefault.Get(),
                D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
            ));

            slot->vertexBufferView.BufferLocation = slot->vertexDefault->GetGPUVirtualAddress();
            slot->vertexBufferView.SizeInBytes = vertexBufferSize;
            slot->vertexBufferView.StrideInBytes = sizeof(Vertex);
        }

        // 4. Vertex Upload Heap Buffer
        {
            const CD3DX12_HEAP_PROPERTIES props(D3D12_HEAP_TYPE_UPLOAD);
            const CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

            ThrowIfFailed(m_device->CreateCommittedResource(
                &props,
                D3D12_HEAP_FLAG_NONE,
                &resDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&slot->vertexUpload)
            ));
            slot->vertexUpload->SetName(NSTool::wformat(L"%s::VertexUpload", slotName.c_str()).c_str());
        }
    }

    barrierBatch.Execute(NSBarrier::kTerrain_terrainOnInit, cmdList);

    // Each page have the same layout so we can use the same indices for all.
    std::vector<uint32_t> patchIndices;
    patchIndices.reserve(512 * 512 * 4);
    for (uint32_t z = 0; z < 512; ++z)
    {
        for (uint32_t x = 0; x < 512; ++x)
        {
            const uint32_t bottomLeft  = z * 513 + x;
            const uint32_t bottomRight = bottomLeft + 1;
            const uint32_t topLeft     = bottomLeft + 513;
            const uint32_t topRight    = topLeft + 1;

            patchIndices.push_back(bottomLeft);
            patchIndices.push_back(bottomRight);
            patchIndices.push_back(topRight);
            patchIndices.push_back(topLeft);
        }
    }
    m_sharedPatchIndexCount = static_cast<uint32_t>(patchIndices.size());
    const UINT indexBufferSize = m_sharedPatchIndexCount * sizeof(uint32_t);

    // Create Upload Index Buffer
    {
        const CD3DX12_HEAP_PROPERTIES props(D3D12_HEAP_TYPE_UPLOAD);
        const CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
        ThrowIfFailed(m_device->CreateCommittedResource(
            &props,
            D3D12_HEAP_FLAG_NONE,
            &resDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_sharedPatchIndexUpload)
        ));
        m_sharedPatchIndexUpload->SetName(L"NSTerrain::Terrain::m_sharedPatchIndexUpload");

        void* ppData = nullptr;
        ThrowIfFailed(m_sharedPatchIndexUpload->Map(0u, nullptr, &ppData));
        memcpy(ppData, patchIndices.data(), indexBufferSize);
        m_sharedPatchIndexUpload->Unmap(0u, nullptr);
    }

    // Create Default Index Buffer
    {
        const CD3DX12_HEAP_PROPERTIES props(D3D12_HEAP_TYPE_DEFAULT);
        const CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
        ThrowIfFailed(m_device->CreateCommittedResource(
            &props,
            D3D12_HEAP_FLAG_NONE,
            &resDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&m_sharedPatchIndexDefault)
        ));
        m_sharedPatchIndexDefault->SetName(L"NSTerrain::Terrain::m_sharedPatchIndexDefault");

        m_sharedPatchIndexBufferView.BufferLocation = m_sharedPatchIndexDefault->GetGPUVirtualAddress();
        m_sharedPatchIndexBufferView.SizeInBytes = indexBufferSize;
        m_sharedPatchIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
    }

    // Copy to Default and Transition
    {
        CD3DX12_RESOURCE_BARRIER preCopy = CD3DX12_RESOURCE_BARRIER::Transition(
            m_sharedPatchIndexDefault.Get(),
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COPY_DEST
        );
        cmdList.ResourceBarrier(1u, &preCopy);

        cmdList.CopyResource(m_sharedPatchIndexDefault.Get(), m_sharedPatchIndexUpload.Get());

        CD3DX12_RESOURCE_BARRIER postCopy = CD3DX12_RESOURCE_BARRIER::Transition(
            m_sharedPatchIndexDefault.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_INDEX_BUFFER
        );
        cmdList.ResourceBarrier(1u, &postCopy);
    }

    LoadImpostor(cmdList, rendererCtx);

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

    m_gpuPool.ForEach([&](EntityID, std::shared_ptr<StreamSlot> slot) -> LoopCondition
    {
        if (slot->srvHandle.amount > 0) rendererCtx.freeSRVStatic(slot->srvHandle);
        slot->srvHandle = {};

        slot->heightmapDefault.Reset();
        slot->diffuseDefault.Reset();
        slot->vertexDefault.Reset();
        slot->vertexUpload.Reset();

        return ELoopConditionFlag::CONTINUE;
    });

    m_gpuPool.Clear();

    m_sharedPatchIndexDefault.Reset();
    m_sharedPatchIndexUpload.Reset();
    m_sharedPatchIndexBufferView = {};
    m_sharedPatchIndexCount = 0;

    if (m_impostorSrvHandle.amount > 0) rendererCtx.freeSRVStatic(m_impostorSrvHandle);
    m_impostorSrvHandle = {};
    m_impostorHeight = NSTexture::Texture{};
    m_impostorDiffuse = NSTexture::Texture{};
    m_impostorVertexDefault.Reset();
    m_impostorVertexUpload.Reset();
    m_impostorIndexDefault.Reset();
    m_impostorIndexUpload.Reset();
    m_impostorVertexBufferView = {};
    m_impostorIndexBufferView = {};
    m_impostorIndexCount = 0;
    m_impostorWorldCell = {};

    m_pages.ForEach([&](EntityID, std::shared_ptr<NSTerrain::TerrainPage> page) -> LoopCondition
    {
        page->heightTexturePage.defaultBuffer.Reset();
        page->heightTexturePage.uploadBuffer.Reset();
        page->diffuseTexturePage.defaultBuffer.Reset();
        page->diffuseTexturePage.uploadBuffer.Reset();
        page->vertices.clear();

        return ELoopConditionFlag::CONTINUE;
    });

    m_pages.Clear();
    m_pages = {};
    m_desc = {};

    m_isInitialized = false;
}

void Terrain::Update(std::shared_ptr<NSDX12::CopyCommandList> cmdList, NSRenderer::Ctx rendererCtx)
{
    Iterate();

    uint32_t uploadsThisFrame{0u};

    if (m_pendingCopyCount)
    {
        m_poolView.ForEach([&](EntityID, std::shared_ptr<StreamSlotView> view) -> LoopCondition
        {
            ASSERT(view->residency != StreamSlot::ESlotResidency::Unknown or view->residency != StreamSlot::ESlotResidency::Failed);

            if (view->residency == StreamSlot::ESlotResidency::Loading)
            {
                auto page = m_pages.Get(view->pageKey);
                auto slot = m_gpuPool.Get(view->slotKey);

                if (page->residency == TerrainPage::EPageResidency::Present || page->residency == TerrainPage::EPageResidency::Loading)
                    return ELoopConditionFlag::CONTINUE;

                if (page->residency == TerrainPage::EPageResidency::Loaded)
                {
                    if (uploadsThisFrame >= m_maxUploadsPerFrame) return ELoopConditionFlag::CONTINUE;

                    CopyPageToGPU(page, slot, cmdList->Detach(), rendererCtx);
                    view->residency = StreamSlot::ESlotResidency::Loaded;
                    page->residency = TerrainPage::EPageResidency::Present;
                    uploadsThisFrame++;
                    cmdList->m_HasPendingCopy = true;

                    return ELoopConditionFlag::CONTINUE;
                }

                ASSERT(false, "Unable to load page");
            }

            if (view->residency == StreamSlot::ESlotResidency::Missing)
            {
                auto page = m_pages.Get(view->pageKey);

                m_gpuPool.Get(view->slotKey)->residency = StreamSlot::ESlotResidency::Unknown;

                view->residency = StreamSlot::ESlotResidency::Loading;
                page->residency = TerrainPage::EPageResidency::Loading;

                m_streamPool.Submit([this, page]
                {
                    page->heightTexturePage = this->LoadPageTextureBin(
                        page->heightTexturePage.name,
                        page->heightTexturePage.path,
                        this->pageManifest.height,
                        NSTexture::EType::HEIGHT
                    );
                    page->diffuseTexturePage = this->LoadPageTextureBin(
                        page->diffuseTexturePage.name,
                        page->diffuseTexturePage.path,
                        this->pageManifest.diffuse,
                        NSTexture::EType::DIFFUSE
                    );

                    // pageWidth/Height from the manifest are the texture sizes includes halo
                    const uint32_t halo = this->pageManifest.height.haloPixels;
                    const uint32_t pageWidth = this->pageManifest.height.pageWidth - 2u * halo;
                    const uint32_t pageHeight = this->pageManifest.height.pageHeight - 2u * halo;
                    const float worldPageWidth = this->srcManifest.worldWidth / static_cast<float>(this->pageManifest.pageCountX);
                    const float worldPageDepth = this->srcManifest.worldDepth / static_cast<float>(this->pageManifest.pageCountZ);

                    page->vertices.resize(static_cast<size_t>(pageWidth) * pageHeight);

                    for (uint32_t z = 0; z < pageHeight; ++z)
                    {
                        for (uint32_t x = 0; x < pageWidth; ++x)
                        {
                            Vertex& v = page->vertices[static_cast<size_t>(z) * pageWidth + x];

                            const float uLocal = static_cast<float>(x) / static_cast<float>(pageWidth - 1);
                            const float vLocal = static_cast<float>(z) / static_cast<float>(pageHeight - 1);
                            const UINT srcRow = z + halo;
                            const UINT srcCol = x + halo;
                            const UINT chunkIdx = srcRow / NSTexture::ROWS_AT_A_TIME;
                            const UINT localRow = srcRow % NSTexture::ROWS_AT_A_TIME;

                            const std::byte* pixelPtr = page->heightTexturePage.chunks[chunkIdx].bytes.data()
                                + localRow * page->heightTexturePage.localRowPitch
                                + srcCol   * sizeof(uint16_t);
                            const uint16_t rawHeight = *reinterpret_cast<const uint16_t*>(pixelPtr);
                            const float heightScale = static_cast<float>(rawHeight) / std::numeric_limits<uint16_t>::max();

                            const float worldY = heightScale * this->m_desc.maxHeight;
                            const float worldX = page->worldRect.x + uLocal * worldPageWidth;
                            const float worldZ = page->worldRect.y + vLocal * worldPageDepth;
                            v.position = { worldX, worldY, worldZ };

                            const float uGlobal = page->uvRect.x + uLocal * page->uvRect.width;
                            const float vGlobal = page->uvRect.y + vLocal * page->uvRect.height;
                            v.texCoord = { uGlobal, vGlobal };
                        }
                    }
                    page->heightTexturePage.PopulateCPU(true);
                    page->diffuseTexturePage.PopulateCPU(true);
                    page->residency = TerrainPage::EPageResidency::Loaded;
                });
            }

            return ELoopConditionFlag::CONTINUE;
        });
    }
}
void Terrain::ReleaseUploadBuffers()
{
    for (NSTexture::Texture& tex : m_textures)
    {
        tex.uploadBuffer.Reset();
        tex.chunks.clear();
        tex.chunks.shrink_to_fit();
    }

    m_pages.ForEach([](EntityID, std::shared_ptr<NSTerrain::TerrainPage> page) -> LoopCondition
    {
        return ELoopConditionFlag::CONTINUE;
    });
}

void Terrain::BuildPageLayout(EntityMap<TerrainPage>& out_Pages)
{
    ASSERT(srcManifest.isPresent);
    ASSERT(pageManifest.isPresent);

    ASSERT(pageManifest.pageCountX > 0u);
    ASSERT(pageManifest.pageCountZ > 0u);
    ASSERT(srcManifest.pageCountX == pageManifest.pageCountX);
    ASSERT(srcManifest.pageCountZ == pageManifest.pageCountZ);
    ASSERT(pageManifest.pageCountX < 100u and pageManifest.pageCountZ < 100u);

    const uint32_t heightHalo = pageManifest.height.haloPixels;
    const uint32_t diffuseHalo = pageManifest.diffuse.haloPixels;
    const uint32_t heightCoreW = pageManifest.height.pageWidth - 2u * heightHalo;
    const uint32_t heightCoreH = pageManifest.height.pageHeight - 2u * heightHalo;
    const uint32_t diffuseCoreW = pageManifest.diffuse.pageWidth - 2u * diffuseHalo;
    const uint32_t diffuseCoreH = pageManifest.diffuse.pageHeight - 2u * diffuseHalo;

    ASSERT(heightCoreW > 1u);
    ASSERT(heightCoreH > 1u);
    ASSERT(diffuseCoreW > 0u);
    ASSERT(diffuseCoreH > 0u);

    const uint32_t heightQuadsPerPageX = heightCoreW - 1u;
    const uint32_t heightQuadsPerPageZ = heightCoreH - 1u;
    ASSERT(heightQuadsPerPageX * pageManifest.pageCountX + 1u == pageManifest.height.sourceWidth);
    ASSERT(heightQuadsPerPageZ * pageManifest.pageCountZ + 1u == pageManifest.height.sourceHeight);
    ASSERT(diffuseCoreW * pageManifest.pageCountX == pageManifest.diffuse.sourceWidth);
    ASSERT(diffuseCoreH * pageManifest.pageCountZ == pageManifest.diffuse.sourceHeight);

    const float worldPageWidth = srcManifest.worldWidth / static_cast<float>(pageManifest.pageCountX);
    const float worldPageDepth = srcManifest.worldDepth / static_cast<float>(pageManifest.pageCountZ);
    const float uvPageWidth = 1.f / static_cast<float>(pageManifest.pageCountX);
    const float uvPageHeight = 1.f / static_cast<float>(pageManifest.pageCountZ);

    out_Pages.Reserve(static_cast<size_t>(pageManifest.pageCountX) * pageManifest.pageCountZ);

    std::wstring folderName = NSTool::wformat(L"page_%02u_%02u", 0u, 0u);
    std::filesystem::path folderPath = (m_root / kSourcePagesFolderName / folderName);

    std::string pathToHeightmapBin = (folderPath / kSourceHeightmapBinFilesName).generic_string();
    std::string pathToDiffuseBin = (folderPath / kSourceDiffuseBinFilesName).generic_string();

    const std::size_t folderNameOffset = folderPath.generic_string().find("page_00_00");

    for (uint32_t pageZ{}; pageZ < pageManifest.pageCountZ; pageZ++)
    {
        for (uint32_t pageX{}; pageX < pageManifest.pageCountX; pageX++)
        {
            const size_t pageIndex = static_cast<size_t>(pageZ) * pageManifest.pageCountX + pageX;
            std::shared_ptr<TerrainPage> page = out_Pages.Add();

            page->index = { pageX, pageZ };
            page->ICullable_Id = page->m_id;

            page->heightSourceRect =
            {
                .x = pageX * heightQuadsPerPageX,
                .y = pageZ * heightQuadsPerPageZ,
                .width = heightCoreW,
                .height = heightCoreH
            };
            page->diffuseSourceRect =
            {
                .x = pageX * diffuseCoreW,
                .y = pageZ * diffuseCoreH,
                .width = diffuseCoreW,
                .height = diffuseCoreH
            };
            page->worldRect =
            {
                .x = -0.5f * srcManifest.worldWidth + static_cast<float>(pageX) * worldPageWidth,
                .y = -0.5f * srcManifest.worldDepth + static_cast<float>(pageZ) * worldPageDepth,
                .width = worldPageWidth,
                .height = worldPageDepth,
            };
            page->uvRect =
            {
                .x = static_cast<float>(pageX) * uvPageWidth,
                .y = static_cast<float>(pageZ) * uvPageHeight,
                .width = uvPageWidth,
                .height = uvPageHeight
            };
            page->ICullable_Bound = NSMath::SBoundAABB
            {
                .min = {
                    page->worldRect.x,
                    0.f,
                    page->worldRect.y
                },
                .max = {
                    page->worldRect.x + page->worldRect.width,
                    srcManifest.maxHeight,
                    page->worldRect.y + page->worldRect.height,
                }
            };

            page->isOnCPU = false;
            page->isOnGPU = false;

            WritePageName(pathToHeightmapBin, folderNameOffset, page->index.gridX, page->index.gridZ);
            WritePageName(pathToDiffuseBin, folderNameOffset, page->index.gridX, page->index.gridZ);

            page->heightTexturePage.path = pathToHeightmapBin;
            page->diffuseTexturePage.path = pathToDiffuseBin;

            if (std::filesystem::is_regular_file(page->heightTexturePage.path) and std::filesystem::is_regular_file(page->diffuseTexturePage.path))
            {
                page->residency = TerrainPage::EPageResidency::Present;
                continue;
            }
            else page->residency = TerrainPage::EPageResidency::Missing;
        }
    }
}
bool Terrain::LoadSourceManifest()
{
    std::filesystem::path path = m_root / kSourceManifestFilesName;

    if (not std::filesystem::is_regular_file(path)) return false;

    std::ifstream file(path);
    ASSERT(file.is_open(), "Failed to open terrain manifest");

    Json json{};
    file >> json;

    srcManifest.name = json.at(srcManifest.kJsonObj_name).get<std::string>();

    srcManifest.pageCountX = json.at(srcManifest.kJsonObj_pageCountX).get<uint32_t>();
    srcManifest.pageCountZ = json.at(srcManifest.kJsonObj_pageCountZ).get<uint32_t>();

    srcManifest.worldWidth = json.at(srcManifest.kJsonObj_worldWidth).get<float>();
    srcManifest.worldDepth = json.at(srcManifest.kJsonObj_worldDepth).get<float>();
    srcManifest.maxHeight = json.at(srcManifest.kJsonObj_maxHeight).get<float>();

    srcManifest.heightmap = JsonToTextureManifest(json.at(srcManifest.kJsonObj_heightmap));
    srcManifest.diffuse = JsonToTextureManifest(json.at(srcManifest.kJsonObj_diffuse));

    if (json.contains(srcManifest.kJsonObj_impostorHeight))
    {
        srcManifest.impostorHeight = JsonToPageTextureManifest(json.at(srcManifest.kJsonObj_impostorHeight));
    }
    if (json.contains(srcManifest.kJsonObj_impostorDiffuse))
    {
        srcManifest.impostorDiffuse = JsonToPageTextureManifest(json.at(srcManifest.kJsonObj_impostorDiffuse));
    }

    const bool isNameValid = !srcManifest.name.empty();
    const bool isPageCountXValid = (srcManifest.pageCountX > 0u);
    const bool isPageCountZValid = (srcManifest.pageCountZ > 0u);
    const bool isWorldWidthValid = (srcManifest.worldWidth > 0.f);
    const bool isWorldDepthValid = (srcManifest.worldDepth > 0.f);
    const bool isMaxHeightValid = (srcManifest.maxHeight > 0.f);
    const bool isValid = isNameValid and isPageCountXValid and isPageCountZValid and isWorldWidthValid and isWorldDepthValid and isMaxHeightValid;

    srcManifest.isPresent = isValid;

    return isValid;
}
void Terrain::SaveSourceManifest()
{
    const std::filesystem::path path = m_root / kSourceManifestFilesName;
    ASSERT(std::filesystem::is_directory(path.parent_path()), "Page manifest file must be under the page folder, please create first.");

    Json json;
    json[srcManifest.kJsonObj_name] = srcManifest.name;
    json[srcManifest.kJsonObj_pageCountX]  = srcManifest.pageCountX;
    json[srcManifest.kJsonObj_pageCountZ] = srcManifest.pageCountZ;
    json[srcManifest.kJsonObj_worldWidth] = srcManifest.worldWidth;
    json[srcManifest.kJsonObj_worldDepth] = srcManifest.worldDepth;
    json[srcManifest.kJsonObj_maxHeight] = srcManifest.maxHeight;
    json[srcManifest.kJsonObj_heightmap] = TextureManifestToJson(srcManifest.heightmap);
    json[srcManifest.kJsonObj_diffuse] = TextureManifestToJson(srcManifest.diffuse);
    json[srcManifest.kJsonObj_impostorHeight] = PageTextureManifestToJson(srcManifest.impostorHeight);
    json[srcManifest.kJsonObj_impostorDiffuse] = PageTextureManifestToJson(srcManifest.impostorDiffuse);

    std::ofstream file(path);
    ASSERT(file.is_open(), "Failed to write terrain page cache manifest");

    file << json.dump(4);
}

void Terrain::LoadPagesManifest()
{
    std::ifstream file(m_root / kSourcePagesFolderName / kSourceManifestFilesName);

    ASSERT(file.is_open(), "Failed to open terrain page cache manifest");

    Json json;
    file >> json;

    pageManifest.cacheVersion = json.at(pageManifest.kJsonObj_cacheVersion).get<uint32_t>();
    pageManifest.sourceManifestHash = json.at(pageManifest.kJsonObj_sourceManifestHash).get<std::string>();
    pageManifest.pageCountX = json.at(pageManifest.kJsonObj_pageCountX).get<uint32_t>();
    pageManifest.pageCountZ = json.at(pageManifest.kJsonObj_pageCountZ).get<uint32_t>();

    pageManifest.height = JsonToPageTextureManifest(json.at(pageManifest.kJsonObj_height));
    pageManifest.diffuse = JsonToPageTextureManifest(json.at(pageManifest.kJsonObj_diffuse));

    pageManifest.isPresent = true;
}
void Terrain::SavePagesManifest()
{
    const std::filesystem::path path = m_root / kSourcePagesFolderName / kSourceManifestFilesName;
    if(not std::filesystem::is_directory(path.parent_path()))
    {
        std::filesystem::create_directory(path.parent_path());
    }

    Json json;
    json[pageManifest.kJsonObj_cacheVersion] = pageManifest.cacheVersion;
    json[pageManifest.kJsonObj_sourceManifestHash] = pageManifest.sourceManifestHash;
    json[pageManifest.kJsonObj_pageCountX] = pageManifest.pageCountX;
    json[pageManifest.kJsonObj_pageCountZ] = pageManifest.pageCountZ;

    json[pageManifest.kJsonObj_height] = PageTextureManifestToJson(pageManifest.height);
    json[pageManifest.kJsonObj_diffuse] = PageTextureManifestToJson(pageManifest.diffuse);

    std::ofstream file(path);
    ASSERT(file.is_open(), "Failed to write terrain page cache manifest");

    file << json.dump(4);
}
bool Terrain::PagesExists()
{
    const std::filesystem::path path = m_root / kSourcePagesFolderName / kSourceManifestFilesName;

    if (not std::filesystem::is_regular_file(path))
    {
        g_FError("Couldn't find a pages manifest at: %s", path.generic_string());
        return false;
    }

    return true;
}
bool Terrain::PageMatchesSource()
{
    if(not pageManifest.isPresent) return false;

    if(pageManifest.cacheVersion != kPageGeneration) return false;

    return pageManifest.sourceManifestHash == ComputeSourceManifestHash();
}
std::string Terrain::ComputeSourceManifestHash()
{
    uint64_t hash = 14695981039346656037ull;

    auto Mix = [&hash](const void* data, size_t size)
    {
        const unsigned char* bytes = static_cast<const unsigned char*>(data);
        for (size_t i = 0u; i < size; ++i)
        {
            hash ^= bytes[i];
            hash *= 1099511628211ull;
        }
    };
    auto MixStr = [&Mix](std::string_view s) { Mix(s.data(), s.size()); };
    auto MixTex = [&Mix, &MixStr](const TextureManifest& tex)
    {
        MixStr(tex.relativePath.generic_string());
        Mix(&tex.format, sizeof(tex.format));
        Mix(&tex.width, sizeof(tex.width));
        Mix(&tex.height, sizeof(tex.height));
        for (NSTexture::EChannel ch : tex.channels) Mix(&ch, sizeof(ch));
    };

    MixStr(srcManifest.name);
    Mix(&srcManifest.pageCountX, sizeof(srcManifest.pageCountX));
    Mix(&srcManifest.pageCountZ, sizeof(srcManifest.pageCountZ));
    Mix(&srcManifest.worldWidth, sizeof(srcManifest.worldWidth));
    Mix(&srcManifest.worldDepth, sizeof(srcManifest.worldDepth));
    Mix(&srcManifest.maxHeight, sizeof(srcManifest.maxHeight));
    MixTex(srcManifest.heightmap);
    MixTex(srcManifest.diffuse);

    return NSTool::format("%016llx", hash);
}
void Terrain::GeneratePageFromEXR()
{
    ASSERT(std::filesystem::is_directory(m_root), "Failed to find terrain root: %s", m_root.generic_string());
    ASSERT(srcManifest.pageCountX > 0u and srcManifest.pageCountZ > 0u);

    const std::filesystem::path pagesFolderPath = m_root / kSourcePagesFolderName;
    if (not std::filesystem::is_directory(pagesFolderPath))
    {
        std::filesystem::create_directory(pagesFolderPath);
    }

    pageManifest.cacheVersion = kPageGeneration;
    pageManifest.sourceManifestHash = ComputeSourceManifestHash();
    pageManifest.pageCountX = srcManifest.pageCountX;
    pageManifest.pageCountZ = srcManifest.pageCountZ;
    pageManifest.height = BakeHeightmapEXR();
    pageManifest.diffuse = BakeDiffuseEXR();
    pageManifest.isPresent = true;

    SavePagesManifest();
    SaveSourceManifest();
}

PageTextureManifest Terrain::BakeHeightmapEXR()
{
    ASSERT(srcManifest.pageCountX > 0u and srcManifest.pageCountZ > 0u);
    ASSERT(srcManifest.heightmap.format == DXGI_FORMAT::DXGI_FORMAT_R16_UNORM, "Terrain height page cache expects DXGI_FORMAT_R16_UNORM");

    std::filesystem::path heightmapPath = m_root / srcManifest.heightmap.relativePath;
    ASSERT(std::filesystem::is_directory(m_root / kSourcePagesFolderName), "Couldn't find the folder in the Terrain root: %s", kSourcePagesFolderName);
    ASSERT(std::filesystem::is_regular_file(heightmapPath), "Couldn't find the heightmap file");

    std::string pagePathStr = (m_root
        / kSourcePagesFolderName
        / NSTool::format("page_%02u_%02u", 0u, 0u) // kSourcePageFolderNameFormatter
        / kSourceHeightmapBinFilesName).generic_string();

    const std::size_t pageNameOffset = pagePathStr.find("page_00_00");

    ASSERT(srcManifest.pageCountX < 100 and srcManifest.pageCountZ < 100, "Too many pages tried to create");

    NSTexture::Texture heightTexture = NSTexture::LoadTextureEXR(
        L"NSTerrain::Terrain::Heightmap",
        heightmapPath.generic_wstring(),
        BuildEXRLoadDesc(srcManifest.heightmap, NSTexture::EType::HEIGHT)
    );

    ASSERT(heightTexture.width > 1u and heightTexture.height > 1u);
    ASSERT(((heightTexture.width - 1u) % srcManifest.pageCountX == 0u) || (heightTexture.width % srcManifest.pageCountX == 0u), "Heightmap width must divide cleanly into terrain pages");
    ASSERT(((heightTexture.height - 1u) % srcManifest.pageCountZ == 0u) || (heightTexture.height % srcManifest.pageCountZ == 0u), "Heightmap height must divide cleanly into terrain pages");

    const uint32_t quadsPerPageX = (heightTexture.width % srcManifest.pageCountX == 0u)
        ? (heightTexture.width / srcManifest.pageCountX)
        : ((heightTexture.width - 1u) / srcManifest.pageCountX);
    const uint32_t quadsPerPageZ = (heightTexture.height % srcManifest.pageCountZ == 0u)
        ? (heightTexture.height / srcManifest.pageCountZ)
        : ((heightTexture.height - 1u) / srcManifest.pageCountZ);
    const uint32_t pageWidth = quadsPerPageX + 1u;
    const uint32_t pageHeight = quadsPerPageZ + 1u;

    for (uint32_t pageZ{}; pageZ < srcManifest.pageCountZ; pageZ++)
    {
        for (uint32_t pageX{}; pageX < srcManifest.pageCountX; pageX++)
        {
            WritePageName(pagePathStr, pageNameOffset, pageX, pageZ);

            WriteTexturePageBinHalo(heightTexture, pagePathStr,
                static_cast<int64_t>(pageX) * quadsPerPageX,
                static_cast<int64_t>(pageZ) * quadsPerPageZ,
                pageWidth, pageHeight,
                kHaloPixels
            );
        }
    }

    // Baking impostor terrain heights
    {
        const uint32_t impostorW = std::min(kImpostorResolution, heightTexture.width);
        const uint32_t impostorH = std::min(kImpostorResolution, heightTexture.height);

        const std::filesystem::path impostorPath = m_root / kImpostorFolderName / kImpostorHeightmapBinFilesName;
        WriteImpostorBin(heightTexture, impostorPath.generic_string(), impostorW, impostorH);

        srcManifest.impostorHeight = PageTextureManifest
        {
            .sourceWidth = heightTexture.width,
            .sourceHeight = heightTexture.height,
            .format = srcManifest.heightmap.format,
            .pageWidth = impostorW,
            .pageHeight = impostorH,
            .bytesPerPixel = NSTexture::BytesPerPixel(srcManifest.heightmap.format),
            .haloPixels = 0u
        };
    }

    return PageTextureManifest
    {
        .sourceWidth = quadsPerPageX * srcManifest.pageCountX + 1u,
        .sourceHeight = quadsPerPageZ * srcManifest.pageCountZ + 1u,
        .format = srcManifest.heightmap.format,
        .pageWidth = pageWidth + 2u * kHaloPixels,
        .pageHeight = pageHeight + 2u * kHaloPixels,
        .bytesPerPixel = NSTexture::BytesPerPixel(srcManifest.heightmap.format),
        .haloPixels = kHaloPixels
    };
}

PageTextureManifest Terrain::BakeDiffuseEXR()
{
    ASSERT(srcManifest.pageCountX > 0u and srcManifest.pageCountZ > 0u);
    ASSERT(
        srcManifest.diffuse.format == DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM
        or
        srcManifest.diffuse.format == DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        "Terrain diffuse page cache expects a supported R8G8B8A8_UNORM format");

    std::filesystem::path diffusePath = m_root / srcManifest.diffuse.relativePath;
    ASSERT(std::filesystem::is_directory(m_root / kSourcePagesFolderName), "Couldn't find the folder in the Terrain root: %s", kSourcePagesFolderName);
    ASSERT(std::filesystem::is_regular_file(diffusePath), "Couldn't find the diffuse file");

    std::string pagePathStr = (m_root
        / kSourcePagesFolderName
        / NSTool::format("page_%02u_%02u", 0u, 0u) // kSourcePageFolderNameFormatter
        / kSourceDiffuseBinFilesName).generic_string();

    const std::size_t pageNameOffset = pagePathStr.find("page_00_00");

    ASSERT(srcManifest.pageCountX < 100 and srcManifest.pageCountZ < 100, "Too many pages tried to create");

    NSTexture::Texture diffuseTexture = NSTexture::LoadTextureEXR(
        L"NSTerrain::Terrain::DiffuseSource",
        diffusePath.generic_wstring(),
        BuildEXRLoadDesc(srcManifest.diffuse, NSTexture::EType::DIFFUSE)
    );

    ASSERT(diffuseTexture.width > 1u and diffuseTexture.height > 1u);

    const uint32_t coreWidth = diffuseTexture.width / srcManifest.pageCountX;
    const uint32_t coreHeight = diffuseTexture.height / srcManifest.pageCountZ;

    if (diffuseTexture.width % srcManifest.pageCountX != 0u || diffuseTexture.height % srcManifest.pageCountZ != 0u)
    {
        g_FWarn("Diffuse texture size adjusted during page generation (original: %u x %u, page-covered: %u x %u)",
            diffuseTexture.width, diffuseTexture.height, coreWidth * srcManifest.pageCountX, coreHeight * srcManifest.pageCountZ);
    }

    PageTextureManifest manifest {
        .sourceWidth = diffuseTexture.width,
        .sourceHeight = diffuseTexture.height,
        .format = srcManifest.diffuse.format,
        .pageWidth = coreWidth + 2u * kHaloPixels,
        .pageHeight = coreHeight + 2u * kHaloPixels,
        .bytesPerPixel = NSTexture::BytesPerPixel(srcManifest.diffuse.format),
        .haloPixels = kHaloPixels
    };

    for (uint32_t pageZ{}; pageZ < srcManifest.pageCountZ; pageZ++)
    {
        for (uint32_t pageX{}; pageX < srcManifest.pageCountX; pageX++)
        {
            WritePageName(pagePathStr, pageNameOffset, pageX, pageZ);

            WriteTexturePageBinHalo(diffuseTexture, pagePathStr,
                static_cast<int64_t>(pageX) * coreWidth,
                static_cast<int64_t>(pageZ) * coreHeight,
                coreWidth, coreHeight,
                kHaloPixels
            );
        }
    }

    // Baking impostor terrain diffuse
    {
        const uint32_t impostorW = std::min(kImpostorResolution, diffuseTexture.width);
        const uint32_t impostorH = std::min(kImpostorResolution, diffuseTexture.height);

        const std::filesystem::path impostorPath = m_root / kImpostorFolderName / kImpostorDiffuseBinFilesName;
        WriteImpostorBin(diffuseTexture, impostorPath.generic_string(), impostorW, impostorH);

        srcManifest.impostorDiffuse = PageTextureManifest
        {
            .sourceWidth = diffuseTexture.width,
            .sourceHeight = diffuseTexture.height,
            .format = srcManifest.diffuse.format,
            .pageWidth = impostorW,
            .pageHeight = impostorH,
            .bytesPerPixel = NSTexture::BytesPerPixel(srcManifest.diffuse.format),
            .haloPixels = 0u
        };
    }

    return manifest;
}

NSTexture::Texture Terrain::LoadPageTextureBin(std::wstring_view name, const std::filesystem::path& path, const PageTextureManifest& manifest, NSTexture::EType type)
{
    ASSERT(std::filesystem::is_regular_file(path), "Terrain page texture file does not exist");
    ASSERT(manifest.pageWidth > 0u);
    ASSERT(manifest.pageHeight > 0u);
    ASSERT(manifest.format != DXGI_FORMAT::DXGI_FORMAT_UNKNOWN);
    ASSERT(manifest.bytesPerPixel == NSTexture::BytesPerPixel(manifest.format));

    const size_t expectedFileSize = static_cast<size_t>(manifest.pageWidth) * manifest.pageHeight * manifest.bytesPerPixel;
    const uintmax_t fileSize = std::filesystem::file_size(path);
    ASSERT(fileSize < static_cast<uintmax_t>(std::numeric_limits<size_t>::max()));
    ASSERT(fileSize == expectedFileSize, "Page texture bin size mismatch: expected %llu bytes, got %llu bytes", expectedFileSize, fileSize);

    std::vector<std::byte> bytes(static_cast<size_t>(fileSize));

    std::ifstream file(path, std::ios::binary);
    if (not file.is_open()) ASSERT(errno);

    file.read(
        reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size())
    );
    ASSERT(file.good(), "Failed to read terrain page texture");

    const UINT sourceRowPitch = manifest.pageWidth * manifest.bytesPerPixel;

    NSTexture::Texture tex = NSTexture::LoadTextureMemory(name, bytes.data(), sourceRowPitch, bytes.size(), NSTexture::MemoryLoadDesc
    {
        .texDesc {
            .textureType = type,
            .format = manifest.format,
            .bytesPerPixel = manifest.bytesPerPixel,
            .localRowPitchMode = NSTexture::ERowPitchMode::TIGHT,
            .uploadRowPitchMode = NSTexture::ERowPitchMode::DX_ALIGN
        },
        .width = manifest.pageWidth,
        .height = manifest.pageHeight
    });
    tex.path = path;

    return tex;
}

void Terrain::CopyPageToGPU(std::shared_ptr<TerrainPage> page, std::shared_ptr<StreamSlot> slot, NSDX12::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx)
{
    ASSERT(page->residency == TerrainPage::EPageResidency::Loaded);
    ASSERT(slot->residency != StreamSlot::ESlotResidency::Loaded);

    const D3D12_RESOURCE_STATES reuseState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    // Heightmap Texture
    {
        page->heightTexturePage.defaultBuffer = slot->heightmapDefault;
        page->heightTexturePage.srvOffset = rendererCtx.offsetSRV(slot->srvHandle, 0u);

        if (slot->gpuInitialized) page->heightTexturePage.desc.initialState = reuseState;

        page->heightTexturePage.PopulateGPU(cmdList, true, reuseState);
    }

    // Diffuse Texture
    {
        page->diffuseTexturePage.defaultBuffer = slot->diffuseDefault;
        page->diffuseTexturePage.srvOffset = rendererCtx.offsetSRV(slot->srvHandle, 1u);

        if (slot->gpuInitialized) page->diffuseTexturePage.desc.initialState = reuseState;

        page->diffuseTexturePage.PopulateGPU(cmdList, true, reuseState);
    }

    // 3. Upload Vertex Buffer
    {
        ASSERT(not page->vertices.empty());
        const UINT vertexBufferSize = static_cast<UINT>(page->vertices.size() * sizeof(Vertex));

        void* ppData = nullptr;
        ThrowIfFailed(slot->vertexUpload->Map(0u, nullptr, &ppData));
        memcpy(ppData, page->vertices.data(), vertexBufferSize);
        slot->vertexUpload->Unmap(0u, nullptr);

        CD3DX12_RESOURCE_BARRIER preCopy = CD3DX12_RESOURCE_BARRIER::Transition(
            slot->vertexDefault.Get(),
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            D3D12_RESOURCE_STATE_COPY_DEST
        );
        cmdList.ResourceBarrier(1u, &preCopy);

        cmdList.CopyResource(slot->vertexDefault.Get(), slot->vertexUpload.Get());

        CD3DX12_RESOURCE_BARRIER postCopy = CD3DX12_RESOURCE_BARRIER::Transition(
            slot->vertexDefault.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
        );
        cmdList.ResourceBarrier(1u, &postCopy);
    }

    page->vertices.clear();
    page->vertices.shrink_to_fit();

    slot->gpuInitialized = true;
    slot->residency = StreamSlot::ESlotResidency::Loaded;
    page->isOnGPU = true;
}

void Terrain::LoadImpostor(NSDX12::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx)
{
    const std::filesystem::path heightPath = m_root / kImpostorFolderName / kImpostorHeightmapBinFilesName;
    const std::filesystem::path diffusePath = m_root / kImpostorFolderName / kImpostorDiffuseBinFilesName;

    if (not std::filesystem::is_regular_file(heightPath) or not std::filesystem::is_regular_file(diffusePath))
    {
        g_FWarn("Terrain impostor bins missing; skipping impostor load");
        return;
    }
    ASSERT(srcManifest.impostorHeight.pageWidth > 1u and srcManifest.impostorDiffuse.pageWidth > 0u, "Impostor manifest not present");

    // Always-resident impostor textures. Single tile, no halo!
    m_impostorHeight = LoadPageTextureBin(L"NSTerrain::Terrain::Impostor::Heightmap", heightPath, srcManifest.impostorHeight, NSTexture::EType::HEIGHT);
    m_impostorDiffuse = LoadPageTextureBin(L"NSTerrain::Terrain::Impostor::Diffuse", diffusePath, srcManifest.impostorDiffuse, NSTexture::EType::DIFFUSE);

    m_impostorSrvHandle = rendererCtx.allocSRVStatic(2u);
    m_impostorHeight.srvOffset = rendererCtx.offsetSRV(m_impostorSrvHandle, 0u);
    m_impostorDiffuse.srvOffset = rendererCtx.offsetSRV(m_impostorSrvHandle, 1u);

    m_impostorHeight.PopulateCPU(true).PopulateGPU(cmdList);
    m_impostorDiffuse.PopulateCPU(true).PopulateGPU(cmdList);

    const uint32_t vertsX = srcManifest.impostorHeight.pageWidth;
    const uint32_t vertsZ = srcManifest.impostorHeight.pageHeight;
    ASSERT(vertsX > 1u and vertsZ > 1u);

    const float worldWidth = m_desc.worldWidth;
    const float worldDepth = m_desc.worldDepth;
    const float originX = -0.5f * worldWidth;
    const float originZ = -0.5f * worldDepth;

    m_impostorWorldCell = {
        worldWidth / static_cast<float>(vertsX - 1u),
        worldDepth / static_cast<float>(vertsZ - 1u)
    };

    std::vector<Vertex> vertices;
    vertices.reserve(static_cast<size_t>(vertsX) * vertsZ);
    for (uint32_t z = 0u; z < vertsZ; ++z)
    {
        const float v = static_cast<float>(z) / static_cast<float>(vertsZ - 1u);
        for (uint32_t x = 0u; x < vertsX; ++x)
        {
            const float u = static_cast<float>(x) / static_cast<float>(vertsX - 1u);
            Vertex vert{};
            vert.position = { originX + u * worldWidth, 0.f, originZ + v * worldDepth };
            vert.texCoord = { u, v };
            vertices.push_back(vert);
        }
    }

    std::vector<uint32_t> indices;
    indices.reserve(static_cast<size_t>(vertsX - 1u) * (vertsZ - 1u) * 6u);
    for (uint32_t z = 0u; z < vertsZ - 1u; ++z)
    {
        for (uint32_t x = 0u; x < vertsX - 1u; ++x)
        {
            const uint32_t i0 = z * vertsX + x;
            const uint32_t i1 = i0 + 1u;
            const uint32_t i2 = i0 + vertsX;
            const uint32_t i3 = i2 + 1u;

            indices.push_back(i0); indices.push_back(i2); indices.push_back(i1);
            indices.push_back(i1); indices.push_back(i2); indices.push_back(i3);
        }
    }
    m_impostorIndexCount = static_cast<uint32_t>(indices.size());

    const UINT vbSize = static_cast<UINT>(vertices.size() * sizeof(Vertex));
    const UINT ibSize = static_cast<UINT>(indices.size() * sizeof(uint32_t));

    auto UploadBuffer = [&](UINT size, const void* data, ComPtr<ID3D12Resource>& outUpload, ComPtr<ID3D12Resource>& outDefault, D3D12_RESOURCE_STATES finalState, LPCWSTR uploadName, LPCWSTR defaultName)
    {
        const CD3DX12_HEAP_PROPERTIES upProps(D3D12_HEAP_TYPE_UPLOAD);
        const CD3DX12_RESOURCE_DESC bufDesc = CD3DX12_RESOURCE_DESC::Buffer(size);

        ThrowIfFailed(m_device->CreateCommittedResource(&upProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&outUpload)));
        outUpload->SetName(uploadName);

        void* p = nullptr;
        ThrowIfFailed(outUpload->Map(0u, nullptr, &p));
        memcpy(p, data, size);
        outUpload->Unmap(0u, nullptr);

        const CD3DX12_HEAP_PROPERTIES defProps(D3D12_HEAP_TYPE_DEFAULT);
        ThrowIfFailed(m_device->CreateCommittedResource(&defProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&outDefault)));
        outDefault->SetName(defaultName);

        CD3DX12_RESOURCE_BARRIER pre = CD3DX12_RESOURCE_BARRIER::Transition(outDefault.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        cmdList.ResourceBarrier(1u, &pre);
        cmdList.CopyResource(outDefault.Get(), outUpload.Get());
        CD3DX12_RESOURCE_BARRIER post = CD3DX12_RESOURCE_BARRIER::Transition(outDefault.Get(), D3D12_RESOURCE_STATE_COPY_DEST, finalState);
        cmdList.ResourceBarrier(1u, &post);
    };

    UploadBuffer(vbSize, vertices.data(), m_impostorVertexUpload, m_impostorVertexDefault,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, L"NSTerrain::Terrain::Impostor::VertexUpload", L"NSTerrain::Terrain::Impostor::VertexDefault");
    m_impostorVertexBufferView.BufferLocation = m_impostorVertexDefault->GetGPUVirtualAddress();
    m_impostorVertexBufferView.SizeInBytes = vbSize;
    m_impostorVertexBufferView.StrideInBytes = sizeof(Vertex);

    UploadBuffer(ibSize, indices.data(), m_impostorIndexUpload, m_impostorIndexDefault,
        D3D12_RESOURCE_STATE_INDEX_BUFFER, L"NSTerrain::Terrain::Impostor::IndexUpload", L"NSTerrain::Terrain::Impostor::IndexDefault");
    m_impostorIndexBufferView.BufferLocation = m_impostorIndexDefault->GetGPUVirtualAddress();
    m_impostorIndexBufferView.SizeInBytes = ibSize;
    m_impostorIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
}
