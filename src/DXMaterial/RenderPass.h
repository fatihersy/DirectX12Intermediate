#pragma once

#include "Pipeline.h"

namespace RenderPass
{
    class GeometryPass : public IRenderPass
    {
    public:
        GeometryPass(ID3D12Device14* device, NSRenderer::Ctx& rendererCtx);
        ~GeometryPass() override;

        void Execute(Scene& scene, NSRenderer::Ctx& rendererCtx) override;
        void OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx& rendererCtx) override;

    private:

        GraphicsPipeline m_pipeline;
    };

    class SkyDomePass : public IRenderPass
    {
    public:
        SkyDomePass(ID3D12Device14* device, NSRenderer::Ctx& rendererCtx);
        ~SkyDomePass();

        void Execute(Scene& scene, NSRenderer::Ctx& rendererCtx) override;
        void OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx& rendererCtx) override;

        skyDomeConstants m_constantsUpload{};
    private:
        ComPtr<ID3D12RootSignature> m_rootSignature;
        GraphicsPipeline m_graphics;
        ComputePipeline m_transmittance;
        ComputePipeline m_scattering;

        ComPtr<ID3D12Resource2> m_trasmittanceLUT;
        ComPtr<ID3D12Resource2> m_scatteringLUT;

        void UpdateSkyDome(NSRenderer::Ctx& rendererCtx);

        static constexpr UINT IDX_ROOT_CBV_FRAME = 0u;
        static constexpr UINT IDX_ROOT_CBV_MESH = 1u;
        static constexpr UINT IDX_ROOT_CBV_SKYDOME = 2u;
        static constexpr UINT IDX_ROOT_DESC_TABLE_UAV = 3u;
        static constexpr UINT IDX_ROOT_DESC_TABLE_SRV = 4u;

        float m_timeOfDayDefault{};
        skyDomeConstants m_constantsDefault{};
        static constexpr UINT IDX_CBV_FRAME = 0u;
        static constexpr UINT IDX_CBV_MESH = 1u;
        static constexpr UINT IDX_CBV_SKYDOME = 2u;

        Descriptor::Handle m_srvHandle;
        static constexpr UINT IDX_UAV_TRANSMITTANCE = 0u;
        static constexpr UINT IDX_UAV_SCATTERING = 1u;
        static constexpr UINT IDX_SRV_TRANSMITTANCE = 2u;
        static constexpr UINT IDX_SRV_SCATTERING = 3u;
    };
}
