#pragma once

#include <cstdint>

// Small, backend-neutral vocabulary shared by every rhi/I*.h interface.
// Both the DX12 backend (rhi/dx12/) and the Vulkan backend (rhi/vulkan/)
// translate these into their own native enums/structs internally — nothing
// outside rhi/ ever needs to know DXGI_FORMAT or VkFormat exist.
namespace NSRHI
{
    // Per-draw constant allocations must start on this boundary. 256 is
    // D3D12's hard requirement (D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_
    // ALIGNMENT); Vulkan asks only for minUniformBufferOffsetAlignment
    // (64 on this GPU) and 256 is a multiple of every power-of-two below
    // it — so one constant serves both backends, and DXTerrain's
    // alignment arithmetic ports unchanged.
    inline constexpr size_t kConstantBufferAlignment = 256;

    // How much of the constant buffer one binding can see. On Vulkan a
    // dynamic-uniform-buffer descriptor's range is FIXED at descriptor
    // write time (VK_WHOLE_SIZE + dynamic offsets is illegal: offset +
    // range must stay inside the buffer), so the descriptor is written
    // with this range and the buffer carries this much slack after the
    // last ring byte. 16 KB is the spec floor for maxUniformBufferRange
    // and dwarfs any constant struct in DXTerrain (largest well under
    // 1 KB). D3D12 root CBVs have no range and ignore this.
    inline constexpr size_t kConstantBufferWindowBytes = 16 * 1024;

    enum class EFormat : uint8_t
    {
        Unknown,

        R8G8B8A8_UNORM,
        B8G8R8A8_UNORM,
        R16G16B16A16_FLOAT,
        R32G32_FLOAT,
        R32G32B32_FLOAT,
        R32G32B32A32_FLOAT,
        D32_FLOAT,
        D24_UNORM_S8_UINT,
    };

    // Bytes one texel occupies. Needed to convert between a row pitch in
    // BYTES (what a caller packs to, and what D3D12's PlacedFootprint
    // wants) and one in TEXELS (what Vulkan's bufferRowLength wants).
    // Neutral rather than per-backend because both need the same number
    // and the caller needs it too, to size its staging allocation.
    //
    // Block-compressed formats would break this - BCn has no meaningful
    // per-texel size - and would need a per-block size plus block
    // dimensions instead. None are in EFormat yet; add that when the
    // first compressed texture arrives, not before.
    inline constexpr uint32_t BytesPerTexel(EFormat format)
    {
        switch (format)
        {
            case EFormat::R8G8B8A8_UNORM:     return 4u;
            case EFormat::B8G8R8A8_UNORM:     return 4u;
            case EFormat::D32_FLOAT:          return 4u;
            case EFormat::D24_UNORM_S8_UINT:  return 4u;
            case EFormat::R32G32_FLOAT:       return 8u;
            case EFormat::R16G16B16A16_FLOAT: return 8u;
            case EFormat::R32G32B32_FLOAT:    return 12u;
            case EFormat::R32G32B32A32_FLOAT: return 16u;
            case EFormat::Unknown:
            default:                          return 0u;
        }
    }

    // A rectangle within a texture's subresource 0. The whole-texture case
    // is just {0, 0, width, height}.
    struct TextureRegion
    {
        uint32_t x{};
        uint32_t y{};
        uint32_t width{};
        uint32_t height{};
    };

    // What a resource is being used for right now — the neutral version
    // of a D3D12_RESOURCE_STATES transition / a Vulkan image layout +
    // pipeline barrier. Each backend maps these onto its own native
    // barrier mechanism internally.
    // This enum is modelled on Vulkan image layouts, which is worth
    // knowing because one member does not survive the trip to D3D12's
    // LEGACY barriers - see the note on Undefined below and
    // DX12CommandList::TransitionTexture.
    enum class EResourceState : uint8_t
    {
        // NOT "the resource is in no particular state". It means "I do not
        // care what is currently here - you may discard it", and is only
        // ever valid as the BEFORE state of a transition. Used when the
        // contents are about to be cleared anyway, which lets a driver skip
        // preserving them.
        Undefined,
        RenderTarget,
        DepthWrite,
        DepthRead,
        Present,
        CopySource,
        CopyDestination,
        ShaderResource,
    };

    struct Viewport
    {
        float x{};
        float y{};
        float width{};
        float height{};
        float minDepth{ 0.0f };
        float maxDepth{ 1.0f };
    };

    struct ScissorRect
    {
        int32_t left{};
        int32_t top{};
        int32_t right{};
        int32_t bottom{};
    };

    struct ClearColor
    {
        float r{};
        float g{};
        float b{};
        float a{};
    };
}
