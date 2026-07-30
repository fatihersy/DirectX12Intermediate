#include "stdafx.h"
#include "VulkanSwapchain.h"

#include "VulkanDevice.h"
#include "VulkanFormat.h"
#include "VulkanTexture.h"

#include <algorithm>

namespace NSRHIVulkan
{
    namespace
    {
        // B8G8R8A8_UNORM is what virtually every Wayland compositor
        // reports first, and it's what the DX12 path uses too. Fall back
        // to whatever the surface does support rather than failing.
        VkSurfaceFormatKHR ChooseFormat(VkPhysicalDevice gpu, VkSurfaceKHR surface)
        {
            uint32_t count{};
            vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &count, nullptr);
            std::vector<VkSurfaceFormatKHR> formats(count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &count, formats.data());

            for (const VkSurfaceFormatKHR& f : formats)
            {
                if (f.format == VK_FORMAT_B8G8R8A8_UNORM and
                    f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                {
                    return f;
                }
            }
            return formats.empty() ? VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR }
                                   : formats[0];
        }
    }

    VulkanSwapchain::VulkanSwapchain(VulkanDevice& device, uint32_t width, uint32_t height)
        : m_device(device)
    {
        Create(width, height);
    }

    VulkanSwapchain::~VulkanSwapchain()
    {
        Destroy();
    }

    void VulkanSwapchain::Create(uint32_t width, uint32_t height)
    {
        VkSurfaceCapabilitiesKHR caps{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_device.PhysicalDevice(), m_device.Surface(), &caps);

        // 0xFFFFFFFF means "the surface has no fixed size, pick one" —
        // which is the normal answer on Wayland, where the client decides.
        if (caps.currentExtent.width != UINT32_MAX)
        {
            m_extent = caps.currentExtent;
        }
        else
        {
            m_extent.width = std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width);
            m_extent.height = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
        }

        uint32_t imageCount = caps.minImageCount + 1;
        if (caps.maxImageCount > 0 and imageCount > caps.maxImageCount)
        {
            imageCount = caps.maxImageCount;
        }


        const VkSurfaceFormatKHR surfaceFormat = ChooseFormat(m_device.PhysicalDevice(), m_device.Surface());
        m_format = surfaceFormat.format;

        VkSwapchainCreateInfoKHR info{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
        info.surface = m_device.Surface();
        info.minImageCount = imageCount;
        info.imageFormat = m_format;
        info.imageColorSpace = surfaceFormat.colorSpace;
        info.imageExtent = m_extent;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        // One queue family handles both graphics and present (VulkanDevice
        // only accepts a device where that's true), so no sharing needed.
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.preTransform = caps.currentTransform;
        info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        // FIFO is the only mode guaranteed to exist, and it's vsync —
        // equivalent to the DX12 path's Present(1, 0).
        info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        info.clipped = VK_TRUE;

        VK_CHECK(vkCreateSwapchainKHR(m_device.Device(), &info, nullptr, &m_swapchain));

        uint32_t actualCount{};
        vkGetSwapchainImagesKHR(m_device.Device(), m_swapchain, &actualCount, nullptr);
        std::vector<VkImage> rawImages(actualCount);
        vkGetSwapchainImagesKHR(m_device.Device(), m_swapchain, &actualCount, rawImages.data());

        const NSRHI::EFormat neutralFormat = FromVkFormat(m_format);
        m_images.reserve(actualCount);
        for (VkImage image : rawImages)
        {
            m_images.push_back(std::make_unique<VulkanTexture>(
                m_device.Device(), image, m_format, m_extent.width, m_extent.height, neutralFormat));
        }

        const VkSemaphoreCreateInfo semInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        m_acquireSemaphores.resize(actualCount);
        m_renderFinishedSemaphores.resize(actualCount);
        for (uint32_t i{}; i < actualCount; ++i)
        {
            VK_CHECK(vkCreateSemaphore(m_device.Device(), &semInfo, nullptr, &m_acquireSemaphores[i]));
            VK_CHECK(vkCreateSemaphore(m_device.Device(), &semInfo, nullptr, &m_renderFinishedSemaphores[i]));
        }

        m_acquireIndex = 0;
        m_currentImage = kInvalidImage;
        m_outOfDate = false;
    }

    void VulkanSwapchain::Destroy()
    {
        if (m_device.Device() == VK_NULL_HANDLE) return;

        for (VkSemaphore s : m_acquireSemaphores)       vkDestroySemaphore(m_device.Device(), s, nullptr);
        for (VkSemaphore s : m_renderFinishedSemaphores) vkDestroySemaphore(m_device.Device(), s, nullptr);
        m_acquireSemaphores.clear();
        m_renderFinishedSemaphores.clear();

        // Destroys the image views; the images themselves belong to the
        // swapchain and go with vkDestroySwapchainKHR below.
        m_images.clear();

        if (m_swapchain)
        {
            vkDestroySwapchainKHR(m_device.Device(), m_swapchain, nullptr);
            m_swapchain = VK_NULL_HANDLE;
        }
    }

    uint32_t VulkanSwapchain::AcquireNextImage()
    {
        if (m_outOfDate or m_swapchain == VK_NULL_HANDLE) return kInvalidImage;

        m_currentAcquireSemaphore = m_acquireSemaphores[m_acquireIndex];
        m_acquireIndex = (m_acquireIndex + 1) % static_cast<uint32_t>(m_acquireSemaphores.size());

        const VkResult result = vkAcquireNextImageKHR(
            m_device.Device(), m_swapchain, UINT64_MAX, m_currentAcquireSemaphore, VK_NULL_HANDLE, &m_currentImage);

        // SUBOPTIMAL still produced a usable image, so this frame can be
        // drawn; the rebuild happens on the next one.
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            m_outOfDate = true;
            return kInvalidImage;
        }
        if (result == VK_SUBOPTIMAL_KHR)
        {
            m_outOfDate = true;
            return m_currentImage;
        }
        if (result != VK_SUCCESS)
        {
            g_FError("Vulkan: vkAcquireNextImageKHR failed with %s", VkResultToString(result));
            m_outOfDate = true;
            return kInvalidImage;
        }

        return m_currentImage;
    }

    VkSemaphore VulkanSwapchain::RenderFinishedSemaphore() const
    {
        ASSERT(m_currentImage != kInvalidImage, "No image acquired this frame");
        return m_renderFinishedSemaphores[m_currentImage];
    }

    void VulkanSwapchain::Present()
    {
        if (m_currentImage == kInvalidImage) return;

        const VkSemaphore waitSemaphore = m_renderFinishedSemaphores[m_currentImage];

        VkPresentInfoKHR info{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &waitSemaphore;
        info.swapchainCount = 1;
        info.pSwapchains = &m_swapchain;
        info.pImageIndices = &m_currentImage;

        const VkResult result = vkQueuePresentKHR(m_device.Queue(), &info);
        if (result == VK_ERROR_OUT_OF_DATE_KHR or result == VK_SUBOPTIMAL_KHR)
        {
            m_outOfDate = true;
        }
        else if (result != VK_SUCCESS)
        {
            g_FError("Vulkan: vkQueuePresentKHR failed with %s", VkResultToString(result));
        }

        m_currentImage = kInvalidImage;
    }

    void VulkanSwapchain::Resize(uint32_t width, uint32_t height)
    {
        // The old swapchain's images may still be in flight, and its
        // semaphores may still be pending — the simple, always-correct
        // answer is to drain the device before tearing it down. Resizes
        // are rare enough that the stall doesn't matter.
        vkDeviceWaitIdle(m_device.Device());
        Destroy();
        Create(width, height);
    }

    NSRHI::ITexture* VulkanSwapchain::GetBackBuffer(uint32_t index)
    {
        return m_images[index].get();
    }
}