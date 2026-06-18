#pragma once
#include "core/EntityTypes.h"

#include "TerrainTypes.h"
#include "RendererTypes.h"

namespace NSTerrain
{
    class Terrain : public ITerrainView
    {
    public:
        Terrain();
        Terrain(ID3D12Device14* device);

        Terrain(const Terrain&) = delete;
        Terrain& operator=(const Terrain&) = delete;
        Terrain(Terrain&& other) noexcept;
        Terrain& operator=(Terrain&& other) noexcept;

        bool OnInit(NSDX12::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx, const std::string_view root, NSTerrain::TerrainDesc desc);
        void OnDestroy(NSRenderer::Ctx rendererCtx);

        bool Load(const std::wstring_view path);
        void Free(NSDX12::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx);

        void Upload(NSDX12::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx);
        void Unload(NSDX12::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx);

        void ReleaseUploadBuffers();

        const TerrainDesc& GetDesc() const override {
            return m_desc;
        }
        const EntityMap<TerrainPage>& GetPages() const override {
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

        // ITerrainView overrides
        D3D12_INDEX_BUFFER_VIEW GetSharedPatchIndexBufferView() const override {
            return m_sharedPatchIndexBufferView;
        }
        uint32_t GetSharedPatchIndexCount() const override {
            return m_sharedPatchIndexCount;
        }
        std::shared_ptr<GPUSlot> GetGPUSlot(ObserverKey pageKey, NSDX12::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx) override;

        void BuildPageLayout(EntityMap<TerrainPage>& out_Pages);

    private:
        ID3D12Device14* m_device = nullptr;
        TerrainDesc m_desc{};
        SourceManifest srcManifest;
        PageManifest pageManifest;
        EntityMap<TerrainPage> m_pages;
        std::vector<uint16_t> m_heightR16;

        enum class TextureIDs : uint32_t { HEIGHTMAP = 0, DIFFUSE, MAX };
        NSDescriptor::Handle m_texSrvHandle{};
        std::array<NSTexture::Texture, static_cast<size_t>(TextureIDs::MAX)> m_textures;

        float SampleHeight(float u, float v) const;

        void SaveSourceManifest();
        bool LoadSourceManifest();
        std::string ComputeSourceManifestHash();

        void SavePagesManifest();
        void LoadPagesManifest();

        bool PagesExists();
        bool PageMatchesSource();
        void GeneratePageFromEXR();
        PageTextureManifest GenerateHeightBinsFromEXR();
        PageTextureManifest GenerateDiffuseBinsFromEXR();
        NSTexture::Texture LoadPageTextureBin(std::wstring_view name, const std::filesystem::path& path, const PageTextureManifest& manifest, NSTexture::EType type);

        std::filesystem::path m_root;
        bool m_isOnGPU{};
        bool m_isOnCPU{};
        bool m_isInitialized{};

        // GPU slot pool
        EntityMap<GPUSlot> m_gpuPool;
        std::vector<ObserverKey> m_slotKeys; // For static indexing of pre-allocated slots
        std::vector<int> m_pageToSlot;       // Maps pageIndex -> slot index
        uint64_t m_currentFrame = 0;

        // Shared Index Buffer
        ComPtr<ID3D12Resource> m_sharedPatchIndexDefault;
        ComPtr<ID3D12Resource> m_sharedPatchIndexUpload;
        D3D12_INDEX_BUFFER_VIEW m_sharedPatchIndexBufferView{};
        uint32_t m_sharedPatchIndexCount = 0;

        // Residency and streaming logic
        void CopyPageToGPU(std::shared_ptr<TerrainPage> page, std::shared_ptr<GPUSlot> slot, NSDX12::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx);
        void EvictSlot(std::shared_ptr<GPUSlot> slot);
        ObserverKey ClaimSlotForPage(ObserverKey pageKey);
    };
}
