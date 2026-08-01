#include "stdafx.h"
#include "VulkanTexture.h"

#include "VulkanDevice.h"
#include "VulkanFormat.h"

namespace NSRHIVulkan
{
    namespace
    {
        VkImageAspectFlags AspectFor(NSRHI::EFormat format)
        {
            switch (format)
            {
                case NSRHI::EFormat::D32_FLOAT:
                    return VK_IMAGE_ASPECT_DEPTH_BIT;
                case NSRHI::EFormat::D24_UNORM_S8_UINT:
                    // A combined format has both aspects, and barriers must
                    // name both or the stencil half is left in the wrong
                    // layout.
                    return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
                default:
                    return VK_IMAGE_ASPECT_COLOR_BIT;
            }
        }

        VkImageUsageFlags UsageFor(const NSRHI::TextureDesc& desc)
        {
            if (desc.isDepthStencil) return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

            VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            if (desc.isRenderTarget) usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            return usage;
        }
    }

    VulkanTexture::VulkanTexture(VkDevice device, VkImage image, VkFormat format,
                                 uint32_t width, uint32_t height, NSRHI::EFormat neutralFormat)
        : m_device(device), m_image(image), m_width(width), m_height(height),
          m_neutralFormat(neutralFormat)
    {
        m_aspect = AspectFor(neutralFormat);
        CreateView(format);
    }

    VulkanTexture::VulkanTexture(VulkanDevice& device, const NSRHI::TextureDesc& desc)
        : m_device(device.Device()), m_allocator(device.Allocator()),
          m_width(desc.width), m_height(desc.height), m_neutralFormat(desc.format)
    {
        ASSERT(desc.width > 0 and desc.height > 0, "Texture needs a non-zero extent");

        const VkFormat format = ToVkFormat(desc.format);
        m_aspect = AspectFor(desc.format);

        VkImageCreateInfo info{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = format;
        info.extent = { desc.width, desc.height, 1 };
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        // OPTIMAL, not LINEAR: the driver picks whatever swizzled layout
        // its hardware samples fastest. Uploads go through a staging
        // buffer and a copy rather than by writing pixels directly, which
        // is what LINEAR would be for.
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = UsageFor(desc);
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        // Required to be UNDEFINED (or PREINITIALIZED) at creation. The
        // first barrier moves it somewhere useful.
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        // A depth buffer or render target is never touched by the CPU, so
        // asking for device-local memory is not merely a hint here.
        allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

        VK_CHECK(vmaCreateImage(m_allocator, &info, &allocInfo, &m_image, &m_allocation, nullptr));

        CreateView(format);
    }

    void VulkanTexture::CreateView(VkFormat format)
    {
        VkImageViewCreateInfo info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        info.image = m_image;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = format;
        info.subresourceRange.aspectMask = m_aspect;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(m_device, &info, nullptr, &m_view));
    }

    VulkanTexture::~VulkanTexture()
    {
        if (m_view) vkDestroyImageView(m_device, m_view, nullptr);

        // Only destroy the image if we made it. A swapchain image belongs
        // to the swapchain and goes with vkDestroySwapchainKHR.
        if (m_allocation) vmaDestroyImage(m_allocator, m_image, m_allocation);
    }
}
