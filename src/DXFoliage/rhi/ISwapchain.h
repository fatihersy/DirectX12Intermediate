#pragma once

#include "ITexture.h"

#include <cstdint>

// Owns the chain of backbuffers presented to the window and hands the
// current one to the renderer each frame. Mirrors what Renderer.cpp's
// m_swapChain (an IDXGISwapChain4) already does, minus the concrete
// DXGI/D3D12 types.
namespace NSRHI
{
    class ISwapchain
    {
    public:
        virtual ~ISwapchain() = default;

        // Acquires the next available backbuffer index. DX12 and Vulkan
        // differ slightly in exactly when this blocks internally
        // (GetCurrentBackBufferIndex() vs. vkAcquireNextImageKHR), but
        // callers on the renderer side don't need to care about that
        // difference.
        virtual uint32_t AcquireNextImage() = 0;
        virtual void Present() = 0;

        virtual void Resize(uint32_t width, uint32_t height) = 0;

        virtual ITexture* GetBackBuffer(uint32_t index) = 0;
        virtual uint32_t BackBufferCount() const = 0;
    };
}
