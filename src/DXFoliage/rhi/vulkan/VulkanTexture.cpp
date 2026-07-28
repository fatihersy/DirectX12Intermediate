#include "stdafx.h"
#include "VulkanTexture.h"

namespace NSRHIVulkan
{
    VulkanTexture::VulkanTexture(VkDevice device, VkImage image, VkFormat format, uint32_t width, uint32_t height, NSRHI::EFormat neutralFormat)
        : m_device(device), m_image(image), m_width(width), m_height(height), m_neutralFormat(neutralFormat)
    {
        VkImageViewCreateInfo info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        info.image = image;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = format;
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(device, &info, nullptr, &m_view));
    }

    VulkanTexture::~VulkanTexture()
    {
        // Only the view is ours; the image belongs to the swapchain.
        if (m_view) vkDestroyImageView(m_device, m_view, nullptr);
    }
}
