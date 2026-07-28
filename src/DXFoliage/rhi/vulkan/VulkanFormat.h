#pragma once

#include "VulkanCommon.h"
#include "rhi/RHITypes.h"

// Shared NSRHI::EFormat -> VkFormat mapping — the Vulkan counterpart of
// rhi/dx12/DX12Format.h. Used for swapchain/attachment formats and for
// vertex attribute formats.
namespace NSRHIVulkan
{
    inline VkFormat ToVkFormat(NSRHI::EFormat format)
    {
        switch (format)
        {
            case NSRHI::EFormat::R8G8B8A8_UNORM:      return VK_FORMAT_R8G8B8A8_UNORM;
            case NSRHI::EFormat::B8G8R8A8_UNORM:      return VK_FORMAT_B8G8R8A8_UNORM;
            case NSRHI::EFormat::R16G16B16A16_FLOAT:  return VK_FORMAT_R16G16B16A16_SFLOAT;
            case NSRHI::EFormat::R32G32_FLOAT:        return VK_FORMAT_R32G32_SFLOAT;
            case NSRHI::EFormat::R32G32B32_FLOAT:     return VK_FORMAT_R32G32B32_SFLOAT;
            case NSRHI::EFormat::R32G32B32A32_FLOAT:  return VK_FORMAT_R32G32B32A32_SFLOAT;
            case NSRHI::EFormat::D32_FLOAT:           return VK_FORMAT_D32_SFLOAT;
            case NSRHI::EFormat::D24_UNORM_S8_UINT:   return VK_FORMAT_D24_UNORM_S8_UINT;
            default:                                  return VK_FORMAT_UNDEFINED;
        }
    }

    inline NSRHI::EFormat FromVkFormat(VkFormat format)
    {
        switch (format)
        {
            case VK_FORMAT_R8G8B8A8_UNORM:      return NSRHI::EFormat::R8G8B8A8_UNORM;
            case VK_FORMAT_B8G8R8A8_UNORM:      return NSRHI::EFormat::B8G8R8A8_UNORM;
            case VK_FORMAT_R16G16B16A16_SFLOAT: return NSRHI::EFormat::R16G16B16A16_FLOAT;
            case VK_FORMAT_D32_SFLOAT:          return NSRHI::EFormat::D32_FLOAT;
            case VK_FORMAT_D24_UNORM_S8_UINT:   return NSRHI::EFormat::D24_UNORM_S8_UINT;
            default:                            return NSRHI::EFormat::Unknown;
        }
    }
}
