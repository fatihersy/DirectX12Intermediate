#include "stdafx.h"
#include "DX12Swapchain.h"

#include "DXSampleHelper.h"

namespace NSRHIDX12
{
    DX12Swapchain::DX12Swapchain(ID3D12Device14* device, IDXGIFactory7* factory, ID3D12CommandQueue* queue, HWND hwnd, uint32_t width, uint32_t height, uint32_t bufferCount)
        : m_device(device)
    {
        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.BufferCount = bufferCount;
        desc.Width = width;
        desc.Height = height;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.SwapEffect = DXGI_SWAP_EFFECT::DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.SampleDesc.Count = 1;

        ComPtr<IDXGISwapChain1> dummySc;
        ThrowIfFailed(
            factory->CreateSwapChainForHwnd(
                queue,
                hwnd,
                &desc,
                nullptr, // We created the sc on a windowed window
                nullptr, // There is one monitor to render
                &dummySc
            )
        );

        dummySc.As(&m_swapChain);

        CreateBackBuffers(width, height);
    }

    void DX12Swapchain::CreateBackBuffers(uint32_t width, uint32_t height)
    {
        DXGI_SWAP_CHAIN_DESC1 desc{};
        m_swapChain->GetDesc1(&desc);

        m_backBuffers.resize(desc.BufferCount);
        for (uint32_t i{}; i < desc.BufferCount; ++i)
        {
            ComPtr<ID3D12Resource> backBuffer;
            ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

            m_backBuffers[i] = std::make_unique<DX12Texture>(m_device, backBuffer, NSRHI::TextureDesc{
                .width = width,
                .height = height,
                .format = NSRHI::EFormat::R8G8B8A8_UNORM,
                // Descriptive only — the swapchain already created these
                // images; this desc just records what they are for the
                // wrapping DX12Texture. CopyDestination because EndFrame
                // blits the front-end's target into them.
                .usage = NSRHI::ETextureUsage::RenderTarget | NSRHI::ETextureUsage::CopyDestination
            });
        }
    }

    uint32_t DX12Swapchain::AcquireNextImage()
    {
        return m_swapChain->GetCurrentBackBufferIndex();
    }

    void DX12Swapchain::Present()
    {
        ThrowIfFailed(m_swapChain->Present(1, 0));
    }

    void DX12Swapchain::Resize(uint32_t width, uint32_t height)
    {
        // Release every backbuffer's resource reference first —
        // ResizeBuffers fails if the swapchain's images are still
        // referenced anywhere.
        for (auto& backBuffer : m_backBuffers)
        {
            backBuffer.reset();
        }

        DXGI_SWAP_CHAIN_DESC1 desc{};
        m_swapChain->GetDesc1(&desc);
        ThrowIfFailed(m_swapChain->ResizeBuffers(static_cast<UINT>(m_backBuffers.size()), width, height, desc.Format, desc.Flags));

        CreateBackBuffers(width, height);
    }
}
