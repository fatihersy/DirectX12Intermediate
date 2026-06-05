#pragma once

namespace NSTerrain
{
    constexpr std::string_view kSourceManifestFilesName = "manifest.json";
    constexpr std::string_view kSourcePagesFolderName = "pages";
    constexpr std::string_view kSourceHeightmapBinFilesName = "heightmap.bin";
    constexpr std::string_view kSourceDiffuseBinFilesName = "diffuse.bin";
    constexpr uint32_t kPageGeneration = 1u;

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
        std::filesystem::path file;
        DXGI_FORMAT format{};
        std::vector<NSTexture::EChannel> channels;

        static constexpr std::string_view kJsonObj_file = "file";
        static constexpr std::string_view kJsonObj_format = "format";
        static constexpr std::string_view kJsonObj_channels = "channels";
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

        static constexpr std::string_view kJsonObj_name = "name";
        static constexpr std::string_view kJsonObj_pageCountX = "pageCountX";
        static constexpr std::string_view kJsonObj_pageCountZ = "pageCountZ";
        static constexpr std::string_view kJsonObj_worldWidth = "worldWidth";
        static constexpr std::string_view kJsonObj_worldDepth = "worldDepth";
        static constexpr std::string_view kJsonObj_maxHeight = "maxHeight";
        static constexpr std::string_view kJsonObj_heightmap = "heightmap";
        static constexpr std::string_view kJsonObj_diffuse = "diffuse";

        bool isPresent{};
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
    struct TerrainChunk
    {
        ComPtr<ID3D12Resource> vertexDefault;
        ComPtr<ID3D12Resource> vertexUpload;
        D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};

        ComPtr<ID3D12Resource> triangleIndexDefault;
        ComPtr<ID3D12Resource> triangleIndexUpload;
        D3D12_INDEX_BUFFER_VIEW triangleIndexBufferView{};
        uint32_t triangleIndexCount{};

        ComPtr<ID3D12Resource> patchIndexDefault;
        ComPtr<ID3D12Resource> patchIndexUpload;
        D3D12_INDEX_BUFFER_VIEW patchIndexBufferView{};
        uint32_t patchIndexCount{};

        DirectX::XMFLOAT2 chunkUVOffset{};
        DirectX::XMFLOAT2 chunkUVScale{};

        ChunkKey key;
        ChunkBounds bounds;
    };
    struct TerrainPage
    {
        uint32_t terrainLod{};

        NSMath::SRectU32 heightSourceRect{};
        NSMath::SRectU32 diffuseSourceRect{};

        NSMath::SRectF32 worldRect{};
        NSMath::SRectF32 uvRect{};

        NSTexture::Texture heightTexturePage{};
        NSTexture::Texture diffuseTexturePage{};

        std::vector<TerrainChunk> chunks{};

        bool isOnCPU{};
        bool isOnGPU{};

        PageKey key;
        PageBounds bounds{};
    };

    struct ITerrainView
    {
    public:
        virtual ~ITerrainView() = default;

        virtual const TerrainDesc& GetDesc() const = 0;
        virtual const std::vector<TerrainPage>& GetPages() const = 0;
        virtual NSDescriptor::Offset GetHeightmapSRV() const = 0;
        virtual bool IsOnCPU() const = 0;
        virtual bool IsOnGPU() const = 0;
    };

    class Terrain : public ITerrainView
    {
    public:
        Terrain();
        Terrain(ID3D12Device14* device);

        Terrain(const Terrain&) = delete;
        Terrain& operator=(const Terrain&) = delete;
        Terrain(Terrain&& other) noexcept;
        Terrain& operator=(Terrain&& other) noexcept;

        bool OnInit(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx, const std::string_view root, NSTerrain::TerrainDesc desc);
        void OnDestroy(NSRenderer::Ctx rendererCtx);

        void Generate(uint32_t seed, NSTerrain::TerrainDesc desc);
        bool Load(const std::wstring_view path);
        void Free(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx);

        void Upload(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx);
        void Unload(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx);

        void ReleaseUploadBuffers();

        const TerrainDesc& GetDesc() const override {
            return m_desc;
        }
        const std::vector<TerrainPage>& GetPages() const override {
            return m_pages;
        }
        NSDescriptor::Offset GetHeightmapSRV() const override {
            return m_textures[static_cast<size_t>(TextureIDs::HEIGHTMAP)].srvOffset;
        }
        bool IsOnCPU() const override {
            return m_isOnCPU;
        };
        bool IsOnGPU() const override {
            return m_isOnGPU;
        };

        std::vector<TerrainPage> BuildPageLayout(const SourceManifest& source, const PageManifest& pagesManifest, const TerrainDesc& desc);

        void LoadHeightPages(std::vector<TerrainPage>& pages, const std::filesystem::path& pagesRoot, const PageManifest& pagesManifest);
        void LoadDiffusePages(std::vector<TerrainPage>& pages, const std::filesystem::path& pagesRoot, const PageManifest& pagesManifest);
        void UploadResidentPages(std::vector<TerrainPage>& pages, NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx);


    private:
        ID3D12Device14* m_device = nullptr;
        TerrainDesc m_desc{};
        SourceManifest srcManifest;
        PageManifest pageManifest;
        std::vector<TerrainPage> m_pages;
        std::vector<uint16_t> m_heightR16;

        enum class TextureIDs : uint32_t { HEIGHTMAP = 0, DIFFUSE, MAX };
        NSDescriptor::Handle m_texSrvHandle{};
        std::array<NSTexture::Texture, static_cast<size_t>(TextureIDs::MAX)> m_textures;

        void BuildChunks(NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx);
        void BuildGeometryForPages(std::vector<TerrainPage>& pages, NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx);
        float SampleHeight(float u, float v) const;

        void SaveSourceManifest();
        void LoadSourceManifest();
        std::string ComputeSourceManifestHash();

        void SavePagesManifest();
        void LoadPagesManifest();

        bool PagesExists();
        bool PageMatchesSource();
        void GeneratePageFromEXR();
        PageTextureManifest GenerateHeightBinsFromEXR();
        PageTextureManifest GenerateDiffuseBinsFromEXR();
        NSTexture::Texture LoadPageTextureBin(std::wstring_view name, const std::filesystem::path& path, PageTextureManifest& manifest, NSTexture::EType type);

        TerrainChunk BuildChunkForPage(TerrainPage& page, uint32_t localChunkX, uint32_t localChunkZ, NSRenderer::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx);

        std::filesystem::path m_root;
        bool m_isOnGPU{};
        bool m_isOnCPU{};
        bool m_isInitialized{};
    };
}
