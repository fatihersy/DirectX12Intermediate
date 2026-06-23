#pragma once
#include "core/EntityTypes.h"

#include "TerrainTypes.h"
#include "RendererTypes.h"

namespace NSTerrain
{
    class TerrainStreamPool
    {
        std::vector<std::thread>          m_workers;
        std::queue<std::function<void()>> m_jobs;
        std::mutex                        m_mutex;
        std::condition_variable           m_cv;
        std::atomic<bool>                 m_running{true};

    public:
        TerrainStreamPool(int threadCount = std::max(1u, static_cast<uint32_t>(std::ceil(std::thread::hardware_concurrency() / 4.f))))
        {
            for (int32_t itr{}; itr < threadCount; itr++)
            {
                m_workers.emplace_back([this]
                {
                    while (m_running)
                    {
                        std::function<void()> job;
                        {
                            std::unique_lock lock(m_mutex);

                            m_cv.wait(lock, [this]
                            {
                                return not m_jobs.empty() or not m_running;
                            });
                            if (not m_running) return;

                            job = std::move(m_jobs.front());
                            m_jobs.pop();
                        }
                        job();
                    }
                });
            }
        }

        void Submit(std::function<void()> job)
        {
            {
                std::lock_guard lock(m_mutex);
                m_jobs.push(std::move(job));
            }
            m_cv.notify_one();
        }

        ~TerrainStreamPool()
        {
            m_running = false;
            m_cv.notify_all();
            for (auto& w : m_workers) w.join();
        }
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

        bool OnInit(NSDX12::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx, const std::string_view root, NSTerrain::TerrainDesc desc);
        void OnDestroy(NSRenderer::Ctx rendererCtx);

        void Update(std::shared_ptr<NSDX12::CopyCommandList> cmdList, NSRenderer::Ctx rendererCtx);
        bool isInitialized() const override {
            return m_isInitialized;
        };

        void ReleaseUploadBuffers();

        void Iterate()
        {
            generation++;
        };
        const TerrainDesc& GetDesc() const override {
            return m_desc;
        }
        NSDescriptor::Offset GetHeightmapSRV() const override {
            return m_textures[static_cast<size_t>(TextureIDs::HEIGHTMAP)].srvOffset;
        }

        // ITerrainView overrides
        D3D12_INDEX_BUFFER_VIEW GetSharedPatchIndexBufferView() const override {
            return m_sharedPatchIndexBufferView;
        }
        uint32_t GetSharedPatchIndexCount() const override {
            return m_sharedPatchIndexCount;
        }
        const EntityMap<TerrainPage>& GetPageLayout() const override {
            return m_pages;
        }
        const EntityMap<StreamSlot>& GetSlots() const override {
            return m_gpuPool;
        }
        EntityMap<StreamSlotView>& GetSlotsView() override {
            return m_poolView;
        };

        bool HasImpostor() const override {
            return m_impostorIndexCount > 0u;
        }
        uint32_t GetImpostorHeightmapSRVIndex() const override {
            return m_impostorHeight.srvOffset.index;
        }
        uint32_t GetImpostorDiffuseSRVIndex() const override {
            return m_impostorDiffuse.srvOffset.index;
        }
        D3D12_VERTEX_BUFFER_VIEW GetImpostorVertexBufferView() const override {
            return m_impostorVertexBufferView;
        }
        D3D12_INDEX_BUFFER_VIEW GetImpostorIndexBufferView() const override {
            return m_impostorIndexBufferView;
        }
        uint32_t GetImpostorIndexCount() const override {
            return m_impostorIndexCount;
        }
        DirectX::XMFLOAT2 GetImpostorWorldCell() const override {
            return m_impostorWorldCell;
        }

        void BuildPageLayout(EntityMap<TerrainPage>& out_Pages);

    private:
        ID3D12Device14* m_device = nullptr;
        TerrainDesc m_desc{};
        SourceManifest srcManifest;
        PageManifest pageManifest;
        EntityMap<TerrainPage> m_pages;
        TerrainStreamPool m_streamPool;
        uint64_t generation{};
        uint32_t m_maxUploadsPerFrame{1u};
        uint32_t m_pendingCopyCount{};

        enum class TextureIDs : uint32_t { HEIGHTMAP = 0, DIFFUSE, MAX };
        NSDescriptor::Handle m_texSrvHandle{};
        std::array<NSTexture::Texture, static_cast<size_t>(TextureIDs::MAX)> m_textures;

        void SaveSourceManifest();
        bool LoadSourceManifest();
        std::string ComputeSourceManifestHash();

        void SavePagesManifest();
        void LoadPagesManifest();

        bool PagesExists();
        bool PageMatchesSource();
        void GeneratePageFromEXR();
        PageTextureManifest BakeHeightmapEXR();
        PageTextureManifest BakeDiffuseEXR();
        NSTexture::Texture LoadPageTextureBin(std::wstring_view name, const std::filesystem::path& path, const PageTextureManifest& manifest, NSTexture::EType type);
        void CopyPageToGPU(std::shared_ptr<TerrainPage> page, std::shared_ptr<StreamSlot> slot, NSDX12::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx);

        void LoadImpostor(NSDX12::GraphicsCommandList cmdList, NSRenderer::Ctx rendererCtx);

        std::filesystem::path m_root;
        bool m_isInitialized{};

        EntityMap<StreamSlotView> m_poolView;
        EntityMap<StreamSlot> m_gpuPool;

        ComPtr<ID3D12Resource> m_sharedPatchIndexDefault;
        ComPtr<ID3D12Resource> m_sharedPatchIndexUpload;
        D3D12_INDEX_BUFFER_VIEW m_sharedPatchIndexBufferView{};
        uint32_t m_sharedPatchIndexCount = 0;

        NSDescriptor::Handle m_impostorSrvHandle{};
        NSTexture::Texture m_impostorHeight{};
        NSTexture::Texture m_impostorDiffuse{};
        ComPtr<ID3D12Resource> m_impostorVertexDefault;
        ComPtr<ID3D12Resource> m_impostorVertexUpload;
        ComPtr<ID3D12Resource> m_impostorIndexDefault;
        ComPtr<ID3D12Resource> m_impostorIndexUpload;
        D3D12_VERTEX_BUFFER_VIEW m_impostorVertexBufferView{};
        D3D12_INDEX_BUFFER_VIEW m_impostorIndexBufferView{};
        uint32_t m_impostorIndexCount = 0;
        DirectX::XMFLOAT2 m_impostorWorldCell{};
    };
}
