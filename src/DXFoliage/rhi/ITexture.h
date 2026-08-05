#pragma once

#include "RHITypes.h"

#include "core/Defines.h"  // Flag<>

#include <cstdint>

// A 2D image on the GPU — a swapchain backbuffer, a depth buffer, or (once
// DXFoliage grows past its current triangle-only stage) a loaded texture.
// Mirrors NSTexture::Texture's default/upload-heap pattern from DXTerrain,
// generalized to also cover swapchain-owned render targets.
namespace NSRHI
{
    // What a texture will be used for — a BITMASK, not a single choice.
    // Textures genuinely combine: DXTerrain samples its depth buffer for
    // post-processing (PostProcess.hlsl reads depthSrvIndex, so
    // DepthStencil | Sampled), samples its scene colour target the same
    // way (RenderTarget | Sampled), and writes its atmosphere LUTs as
    // UAVs before reading them as SRVs (UnorderedAccess | Sampled). Both
    // native APIs agree: D3D12_RESOURCE_FLAGS and VkImageUsageFlags are
    // masks, not enums.
    //
    // Declaring the full set matters in both directions. Under-declaring
    // is a validation error. OVER-declaring costs real performance: both
    // drivers pick tiling and compression from the usage set, and D3D12
    // has DENY_SHADER_RESOURCE specifically so a depth buffer that is
    // never read can be told so — which is impossible to know unless the
    // caller states the whole set. The two-bool version this replaced did
    // both at once: every non-depth texture silently got SAMPLED and
    // TRANSFER_DST whether or not anything used them, while a sampled
    // depth buffer could not be expressed at all.
    enum class ETextureUsage : uint32_t
    {
        None            = 0,
        Sampled         = 1u << 0,  // SRV          / VK_IMAGE_USAGE_SAMPLED_BIT
        RenderTarget    = 1u << 1,  // RTV          / COLOR_ATTACHMENT_BIT
        DepthStencil    = 1u << 2,  // DSV          / DEPTH_STENCIL_ATTACHMENT_BIT
        UnorderedAccess = 1u << 3,  // UAV          / STORAGE_BIT
        // D3D12 has no resource flag for copies — any resource is a legal
        // copy endpoint there — so these two are Vulkan-only in effect,
        // and ignored by the DX12 backend rather than mapped.
        CopySource      = 1u << 4,  //              / TRANSFER_SRC_BIT
        CopyDestination = 1u << 5,  //              / TRANSFER_DST_BIT

        Force32Bit = UINT32_MAX,    // required by Flag<> (core/Defines.h)
    };

    struct TextureDesc
    {
        uint32_t width{};
        uint32_t height{};
        EFormat format{ EFormat::R8G8B8A8_UNORM };
        Flag<ETextureUsage> usage{ ETextureUsage::Sampled };
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
