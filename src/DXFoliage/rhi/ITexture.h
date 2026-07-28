#pragma once

#include "RHITypes.h"

#include <cstdint>

// A 2D image on the GPU — a swapchain backbuffer, a depth buffer, or (once
// DXFoliage grows past its current triangle-only stage) a loaded texture.
// Mirrors NSTexture::Texture's default/upload-heap pattern from DXTerrain,
// generalized to also cover swapchain-owned render targets.
namespace NSRHI
{
    struct TextureDesc
    {
        uint32_t width{};
        uint32_t height{};
        EFormat format{ EFormat::R8G8B8A8_UNORM };
        bool isRenderTarget{ false };
        bool isDepthStencil{ false };
    };

    class ITexture
    {
    public:
        virtual ~ITexture() = default;

        virtual uint32_t Width() const = 0;
        virtual uint32_t Height() const = 0;
        virtual EFormat Format() const = 0;
    };
}
