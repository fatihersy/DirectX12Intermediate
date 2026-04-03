#pragma once

#include "Pipeline.h"

class Model;

namespace NSRenderPass
{
    class GeometryPass : public IRenderPass
    {
    public:
        GeometryPass(ID3D12Device14* device, Blackboard& blackboard, NSRenderer::Ctx rendererCtx);
        ~GeometryPass() override;

        void OnInit(Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList) override;

        void Execute(Scene& scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList) override;
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

    class AtmospherePass : public IRenderPass
    {
    public:
        AtmospherePass(ID3D12Device14* device, Blackboard& blackboard, NSRenderer::Ctx rendererCtx);
        ~AtmospherePass() override;

        void OnInit(Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList) override;

        void Execute(Scene& scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList) override;
        void OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx rendererCtx) override;

        atmosphereConstants m_constantsUpload{};
    private:
        ComPtr<ID3D12RootSignature> m_rootSignature;
        GraphicsPipeline m_graphics;
        ComputePipeline m_transmittance;
        ComputePipeline m_scattering;

        ComPtr<ID3D12Resource2> m_transmittanceLUT;
        ComPtr<ID3D12Resource2> m_scatteringLUT;

        void UpdateAtmosphere(NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList);

        static constexpr UINT IDX_ROOT_CBV_FRAME = 0u;
        static constexpr UINT IDX_ROOT_CBV_MESH = 1u;
        static constexpr UINT IDX_ROOT_CBV_ATMOSPHERE = 2u;
        static constexpr UINT IDX_ROOT_DESC_TABLE_UAV = 3u;
        static constexpr UINT IDX_ROOT_DESC_TABLE_SRV = 4u;

        float m_timeOfDayDefault{};
        atmosphereConstants m_constantsDefault{};
        static constexpr UINT IDX_CBV_FRAME = 0u;
        static constexpr UINT IDX_CBV_MESH = 1u;
        static constexpr UINT IDX_CBV_ATMOSPHERE = 2u;

        NSDescriptor::Handle m_srvHandle{};
        static constexpr UINT IDX_UAV_TRANSMITTANCE = 0u;
        static constexpr UINT IDX_UAV_SCATTERING = 1u;
        static constexpr UINT IDX_SRV_TRANSMITTANCE = 2u;
        static constexpr UINT IDX_SRV_SCATTERING = 3u;
    };

    class EnvironmentCubemapPass : public IRenderPass
    {
    public:
        EnvironmentCubemapPass(ID3D12Device14* device, Blackboard& blackboard, NSRenderer::Ctx rendererCtx);
        ~EnvironmentCubemapPass() override;

        void OnInit(Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList) override;
        void OnDestroy() override;

        void Execute(Scene& scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList) override;
        void OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx rendererCtx) override;

    private:
        ComPtr<ID3D12RootSignature> m_rootSignature;
        GraphicsPipeline m_atmosPipeline;
        GraphicsPipeline m_geomPipeline;
        atmosphereConstants m_atmosConstantsDefault{};

        ComPtr<ID3D12RootSignature> m_prefilterRoot;
        ComputePipeline m_prefilterPipeline;

        ComPtr<ID3D12RootSignature> m_brdfRoot;
        ComputePipeline m_brdfPipeline;
        ComPtr<ID3D12Resource2> m_brdfLUT;
        NSDescriptor::Handle m_brdfSRVhandle;
        NSDescriptor::Handle m_brdfUAVhandle;

        void Capture(Model& model, Scene& scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList);
        void PrefilterCubemap(NSRenderer::EnvironmentCubemap& envMap, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList);
        void GenerateBRDFLUT(NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList);

        NSDescriptor::Handle m_srvHandle{};
        static constexpr UINT IDX_ROOT_CBV_CAPTURE = 0u;
        static constexpr UINT IDX_ROOT_CBV_MESH = 1u;
        static constexpr UINT IDX_ROOT_DESC_MODEL_SRV = 2u;
        static constexpr UINT IDX_ROOT_CBV_ATMOSPHERE = 3u;
        static constexpr UINT IDX_ROOT_DESC_ATMOS_SRV = 4u;

        static constexpr UINT IDX_CBV_CAPTURE = 0u;
        static constexpr UINT IDX_CBV_MESH = 1u;
        static constexpr UINT IDX_CBV_ATMOSPHERE = 2u;

        static constexpr UINT BRDF_LUT_SIZE = 256u;
    };
}


