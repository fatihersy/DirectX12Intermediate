#pragma once

#include "PlatformHeaders_DX12.h"
#include "rhi/IDevice.h"

// DX12 implementation of the exposed IDevice (the resource factory). Also
// the internal helper that holds the native ID3D12Device / DXGI factory /
// command queue — bootstrapping (adapter enumeration, device/queue/
// info-queue creation) all happens in the constructor. Owned by
// RendererBackend_DX12, which exposes it via GetDevice() and hands the
// raw device/queue/factory to its own swapchain/command-list.
namespace NSRHIDX12
{
    class DX12Device final : public NSRHI::IDevice
    {
    public:
        DX12Device();
        ~DX12Device() override = default;

        DX12Device(const DX12Device&) = delete;
        DX12Device& operator=(const DX12Device&) = delete;

        // --- IDevice (exposed factory) ---
        std::unique_ptr<NSRHI::IBuffer> CreateBuffer(const NSRHI::BufferDesc& desc) override;
        std::unique_ptr<NSRHI::ITexture> CreateTexture(const NSRHI::TextureDesc& desc) override;
        std::unique_ptr<NSRHI::IPipelineLayout> CreatePipelineLayout(const NSRHI::PipelineLayoutDesc& desc) override;
        std::unique_ptr<NSRHI::IPipeline> CreateGraphicsPipeline(const NSRHI::GraphicsPipelineDesc& desc) override;

        std::unique_ptr<NSRHI::IDescriptorHeap> CreateDescriptorHeap(const NSRHI::DescriptorHeapDesc& desc) override;
        void CreateShaderResourceView(NSRHI::IDescriptorHeap& heap, NSRHI::DescriptorOffset where,
                                      NSRHI::ITexture* texture) override;

        // D3D12_TEXTURE_DATA_PITCH_ALIGNMENT. Every row of a texture
        // upload must start on this boundary; Vulkan has no such rule.
        uint32_t TextureRowPitchAlignment() const override
        {
            return D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
        }

        // --- Internal accessors (used by the backend, not exposed) ---
        ID3D12Device14* Raw() const { return m_device.Get(); }
        IDXGIFactory7* Factory() const { return m_factory.Get(); }
        ID3D12CommandQueue* Queue() const { return m_commandQueue.Get(); }

    private:
        ComPtr<IDXGIFactory7> m_factory;
        ComPtr<ID3D12Device14> m_device;
        ComPtr<ID3D12CommandQueue> m_commandQueue;

        ComPtr<ID3D12InfoQueue1> m_infoQueue1;
        DWORD m_infoQueueCookie{};
    };
}
