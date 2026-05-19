#pragma once

#include "Pipeline.h"

class Model;

namespace NSRenderPass
{
    class GeometryPass : public RenderPass<NSRenderPass::RenderPassID::PASSID_GEOMETRY>
    {
    public:
        GeometryPass(ID3D12Device14* device, Blackboard& blackboard, NSRenderer::Ctx rendererCtx);
        ~GeometryPass() override;

        GeometryPass& OnInit(Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList);
        void OnDestroy(NSRenderer::Ctx rendererCtx) override;

        void Execute(NSScene::IScene& scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList) override;
        void OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx rendererCtx) override;

    private:
        GraphicsPipeline m_pipeline;

        static constexpr UINT IDX_ROOT_CBV_FRAME = 0u;
        static constexpr UINT IDX_ROOT_CBV_MESH = 1u;
        static constexpr UINT IDX_ROOT_DESC_MODEL_TEX_SRV = 2u;
        static constexpr UINT IDX_ROOT_DESC_ENV_CUBEMAP_SRV = 3u;
        static constexpr UINT IDX_ROOT_DESC_ENV_BRDF_LUT_SRV = 4u;

        static constexpr UINT IDX_CBV_FRAME = 0u;
        static constexpr UINT IDX_CBV_MESH = 1u;
    };

    class AtmospherePass : public RenderPass<NSRenderPass::RenderPassID::PASSID_ATMOSPHERE>
    {
    public:
        AtmospherePass(ID3D12Device14* device, Blackboard& blackboard, NSRenderer::Ctx rendererCtx);
        ~AtmospherePass() override;

        AtmospherePass& OnInit(Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList);
        void OnDestroy(NSRenderer::Ctx rendererCtx) override;

        void Execute(NSScene::IScene& scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList) override;
        void OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx rendererCtx) override;

        AtmosphereConstants m_constantsUpload{};
    private:
        ComPtr<ID3D12RootSignature> m_rootSignature;
        GraphicsPipeline m_graphics;
        ComputePipeline m_transmittance;
        ComputePipeline m_scattering;

        ComPtr<ID3D12Resource2> m_transmittanceLUT;
        ComPtr<ID3D12Resource2> m_scatteringLUT;

        void UpdateAtmosphere(D3D12_GPU_VIRTUAL_ADDRESS constantsGpuAddr, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList);

        static constexpr UINT IDX_ROOT_CBV_FRAME = 0u;
        static constexpr UINT IDX_ROOT_CBV_MESH = 1u;
        static constexpr UINT IDX_ROOT_CBV_ATMOSPHERE = 2u;
        static constexpr UINT IDX_ROOT_DESC_TABLE_UAV = 3u;
        static constexpr UINT IDX_ROOT_DESC_TABLE_SRV = 4u;

        float m_timeOfDayDefault{};
        AtmosphereConstants m_constantsDefault{};
        static constexpr UINT IDX_CBV_FRAME = 0u;
        static constexpr UINT IDX_CBV_MESH = 1u;
        static constexpr UINT IDX_CBV_ATMOSPHERE = 2u;

        NSDescriptor::Handle m_srvHandle{};
        static constexpr UINT IDX_UAV_TRANSMITTANCE = 0u;
        static constexpr UINT IDX_UAV_SCATTERING = 1u;
        static constexpr UINT IDX_SRV_TRANSMITTANCE = 2u;
        static constexpr UINT IDX_SRV_SCATTERING = 3u;
    };

    class EnvironmentCubemapPass : public RenderPass<NSRenderPass::RenderPassID::PASSID_ENVIRONMENTCUBEMAP>
    {
    public:
        EnvironmentCubemapPass(ID3D12Device14* device, Blackboard& blackboard, NSRenderer::Ctx rendererCtx);
        ~EnvironmentCubemapPass() override;

        EnvironmentCubemapPass& OnInit(Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList);
        void OnDestroy(NSRenderer::Ctx rendererCtx) override;

        void Execute(NSScene::IScene& scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList) override;
        void OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx rendererCtx) override;

    private:
        GraphicsPipeline m_atmosPipeline;
        GraphicsPipeline m_geomPipeline;
        AtmosphereConstants m_atmosConstantsDefault{};
        NSDescriptor::Handle m_srvHandle{};
        ComPtr<ID3D12RootSignature> m_graphicsRoot;
        static constexpr UINT IDX_ROOT_CBV_CAPTURE = 0u;
        static constexpr UINT IDX_ROOT_CBV_MESH = 1u;
        static constexpr UINT IDX_ROOT_CBV_ATMOSPHERE = 2u;
        static constexpr UINT IDX_ROOT_DESC_MODEL_SRV = 3u;
        static constexpr UINT IDX_ROOT_DESC_ATMOS_SRV = 4u;

        static constexpr UINT IDX_CBV_CAPTURE = 0u;
        static constexpr UINT IDX_CBV_MESH = 1u;
        static constexpr UINT IDX_CBV_ATMOSPHERE = 2u;

        ComputePipeline m_prefilterPipeline;
        ComPtr<ID3D12RootSignature> m_prefilterRoot;

        ComPtr<ID3D12Resource2> m_brdfLUT;
        NSDescriptor::Handle m_brdfSRVhandle;
        NSDescriptor::Handle m_brdfUAVhandle;
        ComputePipeline m_brdfPipeline;
        ComPtr<ID3D12RootSignature> m_brdfRoot;

        void Capture(Model& model, NSScene::IScene& scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList);
        void PrefilterCubemap(NSRenderer::EnvironmentCubemap& envMap, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList);
        void GenerateBRDFLUT(NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList);

        static constexpr UINT BRDF_LUT_SIZE = 256u;
    };

    class TerrainPass : public RenderPass<NSRenderPass::RenderPassID::PASSID_TERRAIN>
    {
    public:
        TerrainPass(ID3D12Device14* device, Blackboard& blackboard, NSRenderer::Ctx rendererCtx);
        ~TerrainPass() override;

        TerrainPass& OnInit(Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList, IWICImagingFactory2* wicFactory);
        void OnDestroy(NSRenderer::Ctx rendererCtx) override;

        void Execute(NSScene::IScene& scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList) override;
        void OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx rendererCtx) override;

    private:
        ID3D12Device14* m_device = nullptr;
        ComPtr<ID3D12RootSignature> m_rootSignature;
        TessellationPipeline m_solidPipeline;
        TessellationPipeline m_wireframePipeline;

        enum class ETexture : uint32_t { GRASS = 0, ROCK, SNOW, DIRT, TERRAIN_DIFFUSE, MAX };

        NSDescriptor::Handle m_srvHandle{};
        std::array<NSTexture::Texture, static_cast<size_t>(ETexture::MAX)> m_textures{};

        static constexpr UINT IDX_ROOT_CBV_FRAME = 0u;
        static constexpr UINT IDX_ROOT_CBV_TERRAIN = 1u;

        static constexpr UINT IDX_CBV_FRAME = 0u;
        static constexpr UINT IDX_CBV_TERRAIN = 1u;
    };

    class DebugPass : public RenderPass<NSRenderPass::RenderPassID::PASSID_DEBUG>
    {
    public:
        DebugPass(ID3D12Device14* device, Blackboard& blackboard, NSRenderer::Ctx rendererCtx);
        ~DebugPass() override;

        DebugPass& OnInit(Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList, std::reference_wrapper<DebugUtils> utils);
        void OnDestroy(NSRenderer::Ctx rendererCtx) override;

        void Execute(NSScene::IScene& scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList) override;
        void OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx rendererCtx) override;
    private:
        ID3D12Device14* m_device;
        OptRef<DebugUtils> m_utils;

        GraphicsPipeline m_linePipeline;

        NSDescriptor::Handle m_colorRtv;
        NSDescriptor::Handle m_depthDsv;
        ComPtr<ID3D12Resource2> m_color;
        ComPtr<ID3D12Resource2> m_depth;

        D3D12_CPU_DESCRIPTOR_HANDLE imGuiDrawSrvCpuAddr{};
        D3D12_GPU_DESCRIPTOR_HANDLE imGuiDrawSrvGpuAddr{};

        uint32_t m_width = 320;
        uint32_t m_height = 240;
    };

    class ZPrePass : public RenderPass<NSRenderPass::RenderPassID::PASSID_Z>
    {
    public:
        ZPrePass(ID3D12Device14* device, Blackboard& blackboard, NSRenderer::Ctx rendererCtx);
        ~ZPrePass() override;

        ZPrePass& OnInit(Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList);
        void OnDestroy(NSRenderer::Ctx rendererCtx) override;

        void Execute(NSScene::IScene& scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList) override;
        void OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx rendererCtx) override;
    private:
        GraphicsPipeline m_modelDepthPipeline;
        TessellationPipeline m_terrDepthPipeline;

        static constexpr UINT IDX_MODEL_ROOT_CBV_FRAME = 0u;
        static constexpr UINT IDX_MODEL_ROOT_CBV_MESH = 1u;
        static constexpr UINT IDX_MODEL_CBV_FRAME = 0u;
        static constexpr UINT IDX_MODEL_CBV_MESH = 1u;

        static constexpr UINT IDX_TERRAIN_ROOT_CBV_FRAME = 0u;
        static constexpr UINT IDX_TERRAIN_ROOT_CBV_TERRAIN = 1u;
        static constexpr UINT IDX_TERRAIN_CBV_FRAME = 0u;
        static constexpr UINT IDX_TERRAIN_CBV_TERRAIN = 1u;
    };
} // NSRenderPass
