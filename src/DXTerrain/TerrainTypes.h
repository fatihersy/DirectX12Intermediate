#pragma once
#include "core/Math.h"
#include "core/EntityTypes.h"

#include "TextureTypes.h"
#include "DescriptorTypes.h"

#include "Texture.h"

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

    constexpr std::string_view kSourceManifestFilesName = "manifest.json";
    constexpr std::string_view kSourcePagesFolderName = "pages";
    constexpr std::string_view kSourceHeightmapBinFilesName = "heightmap.bin";
    constexpr std::string_view kSourceDiffuseBinFilesName = "diffuse.bin";
    constexpr uint64_t kPageGeneration = 5u;
    constexpr uint32_t kHaloPixels = 2u;

    constexpr std::string_view kImpostorFolderName = "impostor";
    constexpr std::string_view kImpostorHeightmapBinFilesName = "heightmap.bin";
    constexpr std::string_view kImpostorDiffuseBinFilesName = "diffuse.bin";
    constexpr uint32_t kImpostorResolution = 512u;

    //constexpr std::string_view kSourcePageFolderNameFormatter = {}; // "page_%02d_%02d";

    constexpr std::string_view kSourceFileHeightmap = "heightmap.exr";
    constexpr std::string_view kSourceFileDiffuse = "diffuse.exr";

    constexpr std::string_view kTerrFormStr_DXGI_FORMAT_R16_UNORM = "DXGI_FORMAT_R16_UNORM";
    constexpr std::string_view kTerrFormStr_DXGI_FORMAT_R8G8B8A8_UNORM = "DXGI_FORMAT_R8G8B8A8_UNORM";
    constexpr std::string_view kTerrFormStr_DXGI_FORMAT_R8G8B8A8_UNORM_SRGB = "DXGI_FORMAT_R8G8B8A8_UNORM_SRGB";
    constexpr std::string_view kTerrFormStr_DXGI_FORMAT_UNKNOWN = "DXGI_FORMAT_UNKNOWN";

    static DXGI_FORMAT StringToDXGI_Format(std::string format)
    {
        if (format == kTerrFormStr_DXGI_FORMAT_R16_UNORM)           return DXGI_FORMAT_R16_UNORM;
        if (format == kTerrFormStr_DXGI_FORMAT_R8G8B8A8_UNORM)      return DXGI_FORMAT_R8G8B8A8_UNORM;
        if (format == kTerrFormStr_DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

        ASSERT(false, "Unsupported terrain DXGI format");

        return DXGI_FORMAT_UNKNOWN;
    };
    static std::string_view DXGI_FormatToString(DXGI_FORMAT format)
    {
        if (format ==  DXGI_FORMAT_R16_UNORM) return kTerrFormStr_DXGI_FORMAT_R16_UNORM;
        if (format ==  DXGI_FORMAT_R8G8B8A8_UNORM) return kTerrFormStr_DXGI_FORMAT_R8G8B8A8_UNORM;
        if (format ==  DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) return kTerrFormStr_DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

        ASSERT(false, "Unsupported terrain DXGI format");

        return kTerrFormStr_DXGI_FORMAT_UNKNOWN;
    };

    struct TextureManifest
    {
        std::filesystem::path relativePath;
        DXGI_FORMAT format{};
        std::vector<NSTexture::EChannel> channels;
        uint32_t width{};
        uint32_t height{};

        static constexpr std::string_view kJsonObj_file = "file";
        static constexpr std::string_view kJsonObj_format = "format";
        static constexpr std::string_view kJsonObj_channels = "channels";
        static constexpr std::string_view kJsonObj_width = "width";
        static constexpr std::string_view kJsonObj_height = "height";
    };
    struct PageTextureManifest
    {
        uint32_t sourceWidth{};
        uint32_t sourceHeight{};
        DXGI_FORMAT format{};
        uint32_t pageWidth{};
        uint32_t pageHeight{};
        uint32_t bytesPerPixel{};
        uint32_t haloPixels{};

        static constexpr std::string_view kJsonObj_sourceWidth = "sourceWidth";
        static constexpr std::string_view kJsonObj_sourceHeight = "sourceHeight";
        static constexpr std::string_view kJsonObj_format = "format";
        static constexpr std::string_view kJsonObj_pageWidth = "pageWidth";
        static constexpr std::string_view kJsonObj_pageHeight = "pageHeight";
        static constexpr std::string_view kJsonObj_bytesPerPixel = "bytesPerPixel";
        static constexpr std::string_view kJsonObj_haloPixels = "haloPixels";
    };
    struct SourceManifest
    {
        std::string name;
        uint32_t pageCountX{};
        uint32_t pageCountZ{};

        float worldWidth{};
        float worldDepth{};
        float maxHeight{};

        TextureManifest heightmap;
        TextureManifest diffuse;

        PageTextureManifest impostorHeight;
        PageTextureManifest impostorDiffuse;

        static constexpr std::string_view kJsonObj_name = "name";
        static constexpr std::string_view kJsonObj_pageCountX = "pageCountX";
        static constexpr std::string_view kJsonObj_pageCountZ = "pageCountZ";
        static constexpr std::string_view kJsonObj_worldWidth = "worldWidth";
        static constexpr std::string_view kJsonObj_worldDepth = "worldDepth";
        static constexpr std::string_view kJsonObj_maxHeight = "maxHeight";
        static constexpr std::string_view kJsonObj_heightmap = "heightmap";
        static constexpr std::string_view kJsonObj_diffuse = "diffuse";
        static constexpr std::string_view kJsonObj_impostorHeight = "impostorHeight";
        static constexpr std::string_view kJsonObj_impostorDiffuse = "impostorDiffuse";

        bool isPresent{};
    };
    struct PageManifest
    {
        uint32_t cacheVersion{};
        std::string sourceManifestHash;
        uint32_t pageCountX{};
        uint32_t pageCountZ{};
        PageTextureManifest height;
        PageTextureManifest diffuse;

        static constexpr std::string_view kJsonObj_cacheVersion = "cacheVersion";
        static constexpr std::string_view kJsonObj_sourceManifestHash = "sourceManifestHash";
        static constexpr std::string_view kJsonObj_pageCountX = "pageCountX";
        static constexpr std::string_view kJsonObj_pageCountZ = "pageCountZ";
        static constexpr std::string_view kJsonObj_height = "height";
        static constexpr std::string_view kJsonObj_diffuse = "diffuse";

        bool isPresent{};
    };

    struct Vertex
    {
        DirectX::XMFLOAT3 position{};
        DirectX::XMFLOAT2 texCoord{};
    };

    struct StreamSlot
    {
        EntityKey<StreamSlot> m_id;
        ObserverKey pageKey;
        uint64_t generation = 0;

        ComPtr<ID3D12Resource2> heightmapDefault;
        ComPtr<ID3D12Resource2> diffuseDefault;

        ComPtr<ID3D12Resource> vertexDefault;
        ComPtr<ID3D12Resource> vertexUpload;

        NSDescriptor::Handle srvHandle;
        uint32_t heightmapSRVIndex = 0;
        uint32_t diffuseSRVIndex = 0;
        D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};

        // re-upload flag. Barrier must transition from SHADER_RESOURCE not COMMON after first upload.
        bool gpuInitialized{};

        enum class ESlotResidency : uint8_t
        {
            Unknown = 0u,
            Missing,
            Loading,
            Loaded,
            Failed,
        } residency = ESlotResidency::Unknown;
    };

    struct StreamSlotView : NSMath::ICullable
    {
        EntityKey<StreamSlotView> m_id;

        StreamSlotView(ObserverKey inSlotKey) : slotKey(inSlotKey) {};
        const ObserverKey slotKey;

        ObserverKey pageKey;

        enum class StreamSlot::ESlotResidency residency = StreamSlot::ESlotResidency::Unknown;
    };

    struct TerrainPage : public NSMath::ICullable
    {
        uint32_t terrainLod{};
        EntityKey<TerrainPage> m_id;
        PageIndex index;

        NSMath::SRectU32 heightSourceRect{};
        NSMath::SRectU32 diffuseSourceRect{};

        NSMath::SRectF32 worldRect{};
        NSMath::SRectF32 uvRect{};

        NSTexture::Texture heightTexturePage{};
        NSTexture::Texture diffuseTexturePage{};

        std::vector<Vertex> vertices;
        ObserverKey slotKey;

        bool isOnCPU{};
        bool isOnGPU{};

        enum class EPageResidency : uint8_t
        {
            Unknown = 0u,
            Missing,
            Present,
            Loading,
            Loaded,
            Failed,
        } residency = EPageResidency::Unknown;
    };

    struct ITerrainView
    {
    public:
        virtual ~ITerrainView() = default;

        virtual bool isInitialized() const = 0;
        virtual const TerrainDesc& GetDesc() const = 0;
        virtual NSDescriptor::Offset GetHeightmapSRV() const = 0;

        virtual D3D12_INDEX_BUFFER_VIEW GetSharedPatchIndexBufferView() const = 0;
        virtual uint32_t GetSharedPatchIndexCount() const = 0;

        virtual const EntityMap<TerrainPage>& GetPageLayout() const = 0;
        virtual const EntityMap<StreamSlot>& GetSlots() const = 0;
        virtual EntityMap<StreamSlotView>& GetSlotsView() = 0;

        virtual bool HasImpostor() const = 0;
        virtual uint32_t GetImpostorHeightmapSRVIndex() const = 0;
        virtual uint32_t GetImpostorDiffuseSRVIndex() const = 0;
        virtual D3D12_VERTEX_BUFFER_VIEW GetImpostorVertexBufferView() const = 0;
        virtual D3D12_INDEX_BUFFER_VIEW GetImpostorIndexBufferView() const = 0;
        virtual uint32_t GetImpostorIndexCount() const = 0;
        virtual DirectX::XMFLOAT2 GetImpostorWorldCell() const = 0;
    };
}
