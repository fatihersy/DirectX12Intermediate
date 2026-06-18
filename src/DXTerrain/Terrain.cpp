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
Terrain::Terrain(ID3D12Device14* device) : m_device(device)
{

}

Terrain::Terrain(Terrain&& other) noexcept
    : m_device(other.m_device)
    , m_desc(other.m_desc)
    , m_pages(std::move(other.m_pages))
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
        m_pages = std::move(other.m_pages);
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

bool Terrain::OnInit(NSDX12::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx, const std::string_view root, NSTerrain::TerrainDesc desc)
{
    if (m_isInitialized) {
        g_FWarn("Terrain::OnInit::Calling OnInit function twice without Destroy first");
        return false;
    }

    m_root = IApp::GetInstance()->im_assetsPath / root;

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
            .channels = m_desc.heightmapDesc.channels
        };
        srcManifest.diffuse =
        {
            .relativePath = m_desc.diffuseDesc.relativePath,
            .format = m_desc.diffuseDesc.format,
            .channels = m_desc.diffuseDesc.channels
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

    if (m_desc.vertsPerChunkEdge == 0u)
    {
        m_desc.vertsPerChunkEdge = ((pageManifest.height.pageWidth - 1u) / m_desc.chunkCountX) + 1u;
    }

    BuildPageLayout(m_pages);

    ASSERT(pageManifest.isPresent);
    ASSERT(m_pages.Size() == static_cast<size_t>(pageManifest.pageCountX) * pageManifest.pageCountZ);

    // Dynamic Pool Calculation
    uint32_t streamingDistance = rendererCtx.rendererDesc.streamingDistance;
    uint32_t poolSize = 58; // Default to 58 slots (for R = 2000m)
    if (streamingDistance > 0)
    {
        float pArea = 512.f * 512.f;
        float sArea = DirectX::XM_PI * static_cast<float>(streamingDistance * streamingDistance);
        poolSize = static_cast<uint32_t>(std::ceil(sArea / pArea) * 1.2f);
    }

    m_gpuPool.Clear();
    m_slotKeys.clear();
    m_slotKeys.reserve(poolSize);
    m_pageToSlot.assign(static_cast<size_t>(pageManifest.pageCountX) * pageManifest.pageCountZ, -1);

    const UINT vertexBufferSize = pageManifest.height.pageWidth * pageManifest.height.pageHeight * sizeof(Vertex);

    for (uint32_t i = 0; i < poolSize; ++i)
    {
        auto slot = m_gpuPool.Add();
        m_slotKeys.push_back(slot->m_id);

        slot->virtualPageIndex = -1;
        slot->lastFrameVisible = 0;
        slot->isReady = false;

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
                D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
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
                D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
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
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
                nullptr,
                IID_PPV_ARGS(&slot->vertexDefault)
            ));
            slot->vertexDefault->SetName(NSTool::wformat(L"%s::VertexDefault", slotName.c_str()).c_str());

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

    // Shared Patch Index Buffer Creation
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

    m_gpuPool.ForEach([&](EntityID, std::shared_ptr<GPUSlot> slot) -> LoopCondition
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
    m_slotKeys.clear();
    m_pageToSlot.clear();

    m_sharedPatchIndexDefault.Reset();
    m_sharedPatchIndexUpload.Reset();
    m_sharedPatchIndexBufferView = {};
    m_sharedPatchIndexCount = 0;

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
    m_heightR16.clear();
    m_heightR16.shrink_to_fit();
    m_desc = {};

    m_isOnGPU = false;
    m_isOnCPU = false;
    m_isInitialized = false;
}

bool Terrain::Load(const std::wstring_view _path)
{
    ASSERT(m_desc.worldWidth > 0.f and m_desc.worldDepth > 0.f and m_desc.maxHeight > 0.f and m_desc.chunkCountX > 0u and m_desc.chunkCountZ > 0u and m_desc.vertsPerChunkEdge > 1u);

    NSTexture::Texture& heightmap = m_textures[static_cast<size_t>(TextureIDs::HEIGHTMAP)];

    std::wstring path(_path);
    size_t dotPos = path.find_last_of('.');
    ASSERT(dotPos != path.npos, "Extension extraction failed from the file");

    if (_wcsicmp(path.c_str() + dotPos, L".exr") == 0)
    {
        heightmap = NSTexture::LoadTextureEXR(L"NSTerrain::Terrain::Heightmap", path, NSTexture::EXRLoadDesc {
            .texDesc {
                .textureType = NSTexture::EType::HEIGHT,
                .format = DXGI_FORMAT_R16_UNORM,
                .localRowPitchMode = NSTexture::ERowPitchMode::TIGHT,
                .uploadRowPitchMode = NSTexture::ERowPitchMode::DX_ALIGN
            },
            .channels = {"Y"},
        });
    }
    else {
        heightmap = NSTexture::LoadTexture(L"NSTerrain::Terrain::Heightmap", path, NSTexture::EType::HEIGHT);
    }

    ASSERT(heightmap.desc.format == DXGI_FORMAT_R16_UNORM);
    ASSERT(heightmap.width > 1u);
    ASSERT(heightmap.height > 1u);

    ASSERT(heightmap.localRowPitch >= (heightmap.width * heightmap.desc.bytesPerPixel));
    ASSERT(heightmap.uploadRowPitch % D3D12_TEXTURE_DATA_PITCH_ALIGNMENT == 0);

    ASSERT(!heightmap.chunks.empty());

    m_desc.heightmapDesc.width = heightmap.width;
    m_desc.heightmapDesc.height = heightmap.height;

    m_heightR16.resize(size_t(heightmap.width) * size_t(heightmap.height));

    const size_t heightRowBytes = static_cast<size_t>(heightmap.width) * sizeof(uint16_t);
    heightmap.CopyPixels(reinterpret_cast<std::byte*>(m_heightR16.data()), heightRowBytes, heightmap.localRowPitch);
    heightmap.PopulateCPU(true);
    m_isOnCPU = true;

    return true;
}

void Terrain::Upload(NSDX12::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx)
{
    ASSERT(m_isOnCPU, "No CPU data to upload");

    NSTexture::Texture& heightmap = m_textures[static_cast<size_t>(TextureIDs::HEIGHTMAP)];
    ASSERT(heightmap.uploadBuffer, "m_isOnCPU set but upload buffer is not valid.");

    m_texSrvHandle = rendererCtx.allocSRVStatic(1u);
    heightmap.srvOffset = rendererCtx.offsetSRV(m_texSrvHandle, 0u);
    heightmap.PopulateGPU(
        cmdList, true,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );

    m_isOnGPU = true;
}

void Terrain::Free(NSDX12::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx)
{
    if (m_texSrvHandle.amount > 0) rendererCtx.freeSRVStatic(m_texSrvHandle);
    m_texSrvHandle = {};

    for (NSTexture::Texture& tex : m_textures) tex = NSTexture::Texture{};

    m_pages.ForEach([&](EntityID, std::shared_ptr<NSTerrain::TerrainPage> page) -> LoopCondition
    {
        return ELoopConditionFlag::CONTINUE;
    });

    m_pages.Clear();
    m_pages = {};

    m_isOnGPU = false;
}
void Terrain::Unload(NSDX12::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx)
{
    Free(cmdList, rendererCtx);
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

    m_isOnCPU = false;
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

    ASSERT(pageManifest.height.pageWidth > 1u);
    ASSERT(pageManifest.height.pageHeight > 1u);
    ASSERT(pageManifest.diffuse.pageWidth > 0u);
    ASSERT(pageManifest.diffuse.pageHeight > 0u);

    const uint32_t heightQuadsPerPageX = pageManifest.height.pageWidth - 1u;
    const uint32_t heightQuadsPerPageZ = pageManifest.height.pageHeight - 1u;
    ASSERT(heightQuadsPerPageX * pageManifest.pageCountX + 1u == pageManifest.height.sourceWidth);
    ASSERT(heightQuadsPerPageZ * pageManifest.pageCountZ + 1u == pageManifest.height.sourceHeight);
    ASSERT(pageManifest.diffuse.pageWidth * pageManifest.pageCountX == pageManifest.diffuse.sourceWidth);
    ASSERT(pageManifest.diffuse.pageHeight * pageManifest.pageCountZ == pageManifest.diffuse.sourceHeight);

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
                .width = pageManifest.height.pageWidth,
                .height = pageManifest.height.pageHeight
            };
            page->diffuseSourceRect =
            {
                .x = pageX * pageManifest.diffuse.pageWidth,
                .y = pageZ * pageManifest.diffuse.pageHeight,
                .width = pageManifest.diffuse.pageWidth,
                .height = pageManifest.diffuse.pageHeight
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
float Terrain::SampleHeight(float u, float v) const
{
    ASSERT(not m_heightR16.empty());
    const NSTexture::Texture& heightmap = m_textures[static_cast<size_t>(TextureIDs::HEIGHTMAP)];
    ASSERT(heightmap.width > 1u and heightmap.height > 1u, "Invalid heightmap dimensions");

    u = NSMath::Saturate(u);
    v = NSMath::Saturate(v);

    const uint32_t width = heightmap.width;
    const uint32_t depth = heightmap.height;
    const float widthF = static_cast<float>(width);
    const float depthF = static_cast<float>(depth);

    const float x = u * (widthF - 1.f);
    const float z = v * (depthF - 1.f);

    const uint32_t x0 = static_cast<uint32_t>(std::floor(x));
    const uint32_t z0 = static_cast<uint32_t>(std::floor(z));

    const uint32_t x1 = std::min(x0 + 1u, width - 1u);
    const uint32_t z1 = std::min(z0 + 1u, depth - 1u);

    const float tx = x - static_cast<float>(x0);
    const float tz = z - static_cast<float>(z0);

    const auto At = [this, width](uint32_t sampleX, uint32_t sampleZ)
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

    return pageManifest.sourceManifestHash == ComputeSourceManifestHash();
}
std::string Terrain::ComputeSourceManifestHash()
{
     uint64_t hash = 14695981039346656037ull;

    const std::filesystem::path path = m_root / kSourceManifestFilesName;

    std::ifstream file(path, std::ios::binary);
    ASSERT(file.is_open(), "Failed to open terrain source manifest for hashing");

    char c{};

    while (file.get(c))
    {
        hash ^= static_cast<unsigned char>(c);
        hash *= 1099511628211ull;
    }

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
    pageManifest.height = GenerateHeightBinsFromEXR();
    pageManifest.diffuse = GenerateDiffuseBinsFromEXR();

    pageManifest.isPresent = true;

    SavePagesManifest();
}

PageTextureManifest Terrain::GenerateHeightBinsFromEXR()
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

            WriteTexturePageBin(heightTexture, pagePathStr,
                {
                    .x = pageX * quadsPerPageX,
                    .y = pageZ * quadsPerPageZ,
                    .width = pageWidth,
                    .height = pageHeight
                },
                NSTexture::ECopyPixelEdgeMode::CLAMP
            );
        }
    }

    return PageTextureManifest
    {
        .sourceWidth = quadsPerPageX * srcManifest.pageCountX + 1u,
        .sourceHeight = quadsPerPageZ * srcManifest.pageCountZ + 1u,
        .format = srcManifest.heightmap.format,
        .pageWidth = pageWidth,
        .pageHeight = pageHeight,
        .bytesPerPixel = NSTexture::BytesPerPixel(srcManifest.heightmap.format),
        .haloPixels = 0u
    };
}

PageTextureManifest Terrain::GenerateDiffuseBinsFromEXR()
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

    const uint32_t pageWidth = diffuseTexture.width / srcManifest.pageCountX;
    const uint32_t pageHeight = diffuseTexture.height / srcManifest.pageCountZ;

    if (diffuseTexture.width % srcManifest.pageCountX != 0u || diffuseTexture.height % srcManifest.pageCountZ != 0u)
    {
        g_FWarn("Diffuse texture size adjusted during page generation (original: %u x %u, page-covered: %u x %u)",
            diffuseTexture.width, diffuseTexture.height, pageWidth * srcManifest.pageCountX, pageHeight * srcManifest.pageCountZ);
    }

    PageTextureManifest manifest {
        .sourceWidth = diffuseTexture.width,
        .sourceHeight = diffuseTexture.height,
        .format = srcManifest.diffuse.format,
        .pageWidth = pageWidth,
        .pageHeight = pageHeight,
        .bytesPerPixel = NSTexture::BytesPerPixel(srcManifest.diffuse.format),
        .haloPixels = 0u
    };

    for (uint32_t pageZ{}; pageZ < srcManifest.pageCountZ; pageZ++)
    {
        for (uint32_t pageX{}; pageX < srcManifest.pageCountX; pageX++)
        {
            WritePageName(pagePathStr, pageNameOffset, pageX, pageZ);

            WriteTexturePageBin(diffuseTexture, pagePathStr,
                {
                    .x = pageX * manifest.pageWidth,
                    .y = pageZ * manifest.pageHeight,
                    .width = manifest.pageWidth,
                    .height = manifest.pageHeight
                },
                NSTexture::ECopyPixelEdgeMode::REPEAT
            );
        }
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
    ASSERT(file.is_open(), "Failed to open terrain page texture");

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

std::shared_ptr<GPUSlot> Terrain::GetGPUSlot(ObserverKey pageKey, NSDX12::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx)
{
    ASSERT(m_pages.Contains(pageKey));
    auto page = m_pages.Get(pageKey);

    m_currentFrame++;

    if (page->gpuSlotKey.entID == InvalidEntityID)
    {
        page->gpuSlotKey = ClaimSlotForPage(pageKey);
    }

    auto slot = m_gpuPool.Get(page->gpuSlotKey);
    slot->lastFrameVisible = m_currentFrame;

    if (not slot->isReady)
    {
        if (page->residency == TerrainPage::EPageResidency::Present || page->residency == TerrainPage::EPageResidency::Missing)
        {
            page->residency = TerrainPage::EPageResidency::Loading;

            std::wstring pageNameHeightmap = NSTool::wformat(L"page_%02u_%02u::HeightmapBin", page->index.gridX, page->index.gridZ);
            std::wstring pageNameDiffuse = NSTool::wformat(L"page_%02u_%02u::DiffuseBin", page->index.gridX, page->index.gridZ);

            std::shared_ptr<TerrainPage> pagePtr = page;

            std::thread([this, pagePtr, pageNameHeightmap, pageNameDiffuse]()
            {
                pagePtr->heightTexturePage = this->LoadPageTextureBin(
                    pageNameHeightmap,
                    pagePtr->heightTexturePage.path,
                    this->pageManifest.height,
                    NSTexture::EType::HEIGHT
                );

                pagePtr->diffuseTexturePage = this->LoadPageTextureBin(
                    pageNameDiffuse,
                    pagePtr->diffuseTexturePage.path,
                    this->pageManifest.diffuse,
                    NSTexture::EType::DIFFUSE
                );

                const uint32_t pageWidth = this->pageManifest.height.pageWidth;
                const uint32_t pageHeight = this->pageManifest.height.pageHeight;
                pagePtr->vertices.resize(static_cast<size_t>(pageWidth) * pageHeight);

                const float worldPageWidth = this->srcManifest.worldWidth / static_cast<float>(this->pageManifest.pageCountX);
                const float worldPageDepth = this->srcManifest.worldDepth / static_cast<float>(this->pageManifest.pageCountZ);

                for (uint32_t z = 0; z < pageHeight; ++z)
                {
                    for (uint32_t x = 0; x < pageWidth; ++x)
                    {
                        const float uLocal = static_cast<float>(x) / static_cast<float>(pageWidth - 1);
                        const float vLocal = static_cast<float>(z) / static_cast<float>(pageHeight - 1);

                        const float uGlobal = pagePtr->uvRect.x + uLocal * pagePtr->uvRect.width;
                        const float vGlobal = pagePtr->uvRect.y + vLocal * pagePtr->uvRect.height;

                        const float worldX = pagePtr->worldRect.x + uLocal * worldPageWidth;
                        const float worldZ = pagePtr->worldRect.y + vLocal * worldPageDepth;
                        const float worldY = this->SampleHeight(uGlobal, vGlobal) * this->m_desc.maxHeight;

                        Vertex& v = pagePtr->vertices[static_cast<size_t>(z) * pageWidth + x];
                        v.position = { worldX, worldY, worldZ };
                        v.texCoord = { uGlobal, vGlobal };
                    }
                }

                pagePtr->residency = TerrainPage::EPageResidency::Loaded;
            }).detach();
        }
        else if (page->residency == TerrainPage::EPageResidency::Loaded)
        {
            CopyPageToGPU(page, slot, cmdList, rendererCtx);
        }
    }

    return slot;
}

ObserverKey Terrain::ClaimSlotForPage(ObserverKey pageKey)
{
    std::shared_ptr<GPUSlot> chosenSlot = nullptr;
    m_gpuPool.ForEach([&](EntityID, std::shared_ptr<GPUSlot> slot) -> LoopCondition
    {
        if (slot->virtualPageIndex == -1)
        {
            chosenSlot = slot;
            return ELoopConditionFlag::BREAK;
        }
        return ELoopConditionFlag::CONTINUE;
    });

    if (chosenSlot == nullptr)
    {
        uint64_t oldestTime = std::numeric_limits<uint64_t>::max();
        m_gpuPool.ForEach([&](EntityID, std::shared_ptr<GPUSlot> slot) -> LoopCondition
        {
            if (slot->lastFrameVisible < oldestTime)
            {
                oldestTime = slot->lastFrameVisible;
                chosenSlot = slot;
            }
            return ELoopConditionFlag::CONTINUE;
        });

        ASSERT(chosenSlot != nullptr);
        EvictSlot(chosenSlot);
    }

    auto page = m_pages.Get(pageKey);
    chosenSlot->virtualPageIndex = static_cast<int>(page->index.gridZ * pageManifest.pageCountX + page->index.gridX);
    chosenSlot->mappedPageKey = pageKey;
    chosenSlot->isReady = false;

    return chosenSlot->m_id;
}

void Terrain::EvictSlot(std::shared_ptr<GPUSlot> slot)
{
    if (m_pages.Contains(slot->mappedPageKey))
    {
        auto oldPage = m_pages.Get(slot->mappedPageKey);
        oldPage->gpuSlotKey = {};
        oldPage->isOnGPU = false;

        if (oldPage->residency == TerrainPage::EPageResidency::Loaded)
        {
            oldPage->vertices.clear();
            oldPage->vertices.shrink_to_fit();
            oldPage->heightTexturePage.defaultBuffer.Reset();
            oldPage->diffuseTexturePage.defaultBuffer.Reset();
            oldPage->residency = TerrainPage::EPageResidency::Present;
        }
    }
    slot->virtualPageIndex = -1;
    slot->mappedPageKey = {};
    slot->isReady = false;
}

void Terrain::CopyPageToGPU(std::shared_ptr<TerrainPage> page, std::shared_ptr<GPUSlot> slot, NSDX12::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx)
{
    ASSERT(page->residency == TerrainPage::EPageResidency::Loaded);

    // 1. Upload Heightmap Texture
    {
        NSTexture::Texture& heightmap = page->heightTexturePage;
        heightmap.defaultBuffer = slot->heightmapDefault;
        heightmap.srvOffset = rendererCtx.offsetSRV(slot->srvHandle, 0u);

        heightmap.PopulateGPU(
            cmdList, true,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );
        heightmap.uploadBuffer.Reset();
    }

    // 2. Upload Diffuse Texture
    {
        NSTexture::Texture& diffuse = page->diffuseTexturePage;
        diffuse.defaultBuffer = slot->diffuseDefault;
        diffuse.srvOffset = rendererCtx.offsetSRV(slot->srvHandle, 1u);

        diffuse.PopulateGPU(
            cmdList, true,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );
        diffuse.uploadBuffer.Reset();
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

    page->residency = TerrainPage::EPageResidency::Loaded;
    page->isOnGPU = true;
    slot->isReady = true;
}
