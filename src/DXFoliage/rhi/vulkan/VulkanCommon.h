#pragma once

// Vulkan headers + shared helpers for the Vulkan backend. The counterpart
// of rhi/dx12/PlatformHeaders_DX12.h: included only by rhi/vulkan/, never
// by the neutral layers or the platform code.
//
// VK_USE_PLATFORM_WAYLAND_KHR exposes vkCreateWaylandSurfaceKHR. Wayland
// is the only Linux windowing system this project targets, so there's no
// X11 equivalent here.
#define VK_USE_PLATFORM_WAYLAND_KHR
#include <vulkan/vulkan.h>

#include "Logger.h"

namespace NSRHIVulkan
{
    const char* VkResultToString(VkResult result);

    // Vulkan reports failures by return code rather than exceptions.
    // Logging the call site matters because a VkResult on its own rarely
    // says which call produced it.
    #define VK_CHECK(expr)                                                          \
        do {                                                                        \
            const VkResult vkCheckResult = (expr);                                  \
            if (vkCheckResult != VK_SUCCESS)                                        \
            {                                                                       \
                g_FError("Vulkan: %s failed with %s", #expr,                        \
                    NSRHIVulkan::VkResultToString(vkCheckResult));                  \
            }                                                                       \
        } while (false)
}
