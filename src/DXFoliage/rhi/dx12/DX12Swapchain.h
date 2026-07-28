#pragma once

#include "PlatformHeaders_DX12.h"
#include "rhi/ISwapchain.h"
#include "DX12Texture.h"

#include <memory>
#include <vector>

// DX12 implementation of ISwapchain. Owns its backbuffer textures
// internally (each a DX12Texture wrapping swapchain->GetBuffer()'s
// resource) — Renderer no longer keeps its own m_renderTargets[] array,
// it just asks the swapchain for the current one each frame.
namespace NSRHIDX12
{
    class DX12Swapchain final : public NSRHI::ISwapchain
    {
    public:
        DX12Swapchain(ID3D12Device14* device, IDXGIFactory7* factory, ID3D12CommandQueue* queue, HWND hwnd, uint32_t width, uint32_t height, uint32_t bufferCount);
        ~DX12Swapchain() override = default;

        // DX12 has no explicit acquire step the way Vulkan does — this
        // is just GetCurrentBackBufferIndex(), a non-blocking query, safe
        // to call more than once per frame (it doesn't change until
        // Present()).
        uint32_t AcquireNextImage() override;
        void Present() override;
        void Resize(uint32_t width, uint32_t height) override;

        NSRHI::ITexture* GetBackBuffer(uint32_t index) override { return GetBackBufferTexture(index); }
        uint32_t BackBufferCount() const override { return static_cast<uint32_t>(m_backBuffers.size()); }

        // Not part of ISwapchain — DX12-specific accessor for Renderer
        // code that still talks to D3D12 directly today (resource-barrier
        // transitions, OMSetRenderTargets) until drawing itself moves
        // behind ICommandList.
        IDXGISwapChain4* Raw() const { return m_swapChain.Get(); }
        DX12Texture* GetBackBufferTexture(uint32_t index) const { return m_backBuffers[index].get(); }

    private:
        void CreateBackBuffers(uint32_t width, uint32_t height);

        ID3D12Device14* m_device{ nullptr };
        ComPtr<IDXGISwapChain4> m_swapChain;
        std::vector<std::unique_ptr<DX12Texture>> m_backBuffers;
    };
}
