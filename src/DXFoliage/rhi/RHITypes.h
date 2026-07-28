#pragma once

#include <cstdint>

// Small, backend-neutral vocabulary shared by every rhi/I*.h interface.
// Both the DX12 backend (rhi/dx12/) and the Vulkan backend (rhi/vulkan/)
// translate these into their own native enums/structs internally — nothing
// outside rhi/ ever needs to know DXGI_FORMAT or VkFormat exist.
namespace NSRHI
{
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

    // What a resource is being used for right now — the neutral version
    // of a D3D12_RESOURCE_STATES transition / a Vulkan image layout +
    // pipeline barrier. Each backend maps these onto its own native
    // barrier mechanism internally.
    enum class EResourceState : uint8_t
    {
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
