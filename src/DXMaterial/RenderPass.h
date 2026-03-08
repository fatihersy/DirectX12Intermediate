#pragma once

#include "Renderer.h"
#include "Pipeline.h"

namespace RenderPass
{
    struct GpuResource {
        ComPtr<ID3D12Resource2> m_default;
        Descriptor::Handle m_descriptor;

        inline ID3D12Resource2* Get() const { return m_default.Get(); };
    };

    class IRenderPass
    {
    public:
        virtual ~IRenderPass() = default;

        virtual void Execute(Renderer& renderer, Scene& scene, ID3D12GraphicsCommandList10* cmdList) = 0;
        virtual void OnResize(Renderer& renderer, uint32_t width, uint32_t height) = 0;
        virtual void Destroy() = 0;

        bool IsEnabled() const { return m_isEnabled; }
        void SetEnabled(bool newVal) { m_isEnabled = newVal; }
    private:
        bool m_isEnabled{};
    };

    class GeometryPass : public IRenderPass
    {
    public:
        GeometryPass(Renderer& renderer, ID3D12Device14* device);

        void Execute(Renderer& renderer, Scene& scene, ID3D12GraphicsCommandList10* cmdList) override;
        void OnResize(Renderer& renderer, uint32_t width, uint32_t height) override;

    private:
        GraphicsPipeline m_pipeline;
    };

    class SkyDomePass : public IRenderPass
    {
    public:
        SkyDomePass(Renderer& renderer, ID3D12Device14* device);

        void Execute(Renderer& renderer, Scene& scene, ID3D12GraphicsCommandList10* cmdList) override;
        void OnResize(Renderer& renderer, uint32_t width, uint32_t height) override;


        float m_timeOfDay{};
        skyDomeConstants m_constantsUpload{};
    private:
        ComPtr<ID3D12RootSignature> m_rootSignature;
        GraphicsPipeline m_graphics;
        ComputePipeline m_transmittance;
        ComputePipeline m_scattering;

        GpuResource m_trasmittanceLUT;
        GpuResource m_scatteringLUT;

        bool m_isSkyDomeDirty{};

        void UpdateSkyDome(ID3D12GraphicsCommandList10* cmdList, skyDomeConstants& constants);

        static constexpr UINT IDX_ROOT_CBV_FRAME = 0u;
        static constexpr UINT IDX_ROOT_CBV_MESH = 1u;
        static constexpr UINT IDX_ROOT_CBV_SKYDOME = 2u;
        static constexpr UINT IDX_ROOT_DESC_TABLE_UAV = 3u;
        static constexpr UINT IDX_ROOT_DESC_TABLE_SRV = 4u;

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
