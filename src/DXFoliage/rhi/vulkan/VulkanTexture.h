#pragma once

#include "VulkanCommon.h"
#include "VulkanMemory.h"
#include "rhi/ITexture.h"

namespace NSRHIVulkan
{
    class VulkanDevice;

    // Vulkan implementation of ITexture, in two ownership modes.
    //
    //   1. Wrapping a swapchain image. The VkImage belongs to the
    //      swapchain and must NOT be destroyed here; only the view is ours.
    //   2. Creating its own image through VMA. Then the image, its
    //      allocation, and the view are all ours.
    //
    // The distinction is carried by m_allocation being null or not, because
    // destroying a swapchain image is a use-after-free that the validation
    // layer cannot always catch.
    //
    // Under dynamic rendering there's no framebuffer or RTV heap: the view
    // is handed straight to vkCmdBeginRendering, which is why DX12Texture's
    // private descriptor heap has no counterpart here.
    class VulkanTexture final : public NSRHI::ITexture
    {
    public:
        // Mode 1: adopt a swapchain-owned image.
        VulkanTexture(VkDevice device, VkImage image, VkFormat format,
                      uint32_t width, uint32_t height, NSRHI::EFormat neutralFormat);

        // Mode 2: create and own an image.
        VulkanTexture(VulkanDevice& device, const NSRHI::TextureDesc& desc);

        ~VulkanTexture() override;

        VulkanTexture(const VulkanTexture&) = delete;
        VulkanTexture& operator=(const VulkanTexture&) = delete;

        uint32_t Width() const override { return m_width; }
        uint32_t Height() const override { return m_height; }
        NSRHI::EFormat Format() const override { return m_neutralFormat; }

        VkImage Image() const { return m_image; }
        VkImageView View() const { return m_view; }

        // Barriers and views both need to know whether this is colour or
        // depth. Asking the texture beats inferring it from the resource
        // state being transitioned to, which is only right by coincidence
        // for some transitions.
        VkImageAspectFlags Aspect() const { return m_aspect; }

    private:
        void CreateView(VkFormat format);

        VkDevice m_device{ VK_NULL_HANDLE };
        VmaAllocator m_allocator{ VK_NULL_HANDLE };  // null when wrapping
        VmaAllocation m_allocation{ VK_NULL_HANDLE }; // null when wrapping
        VkImage m_image{ VK_NULL_HANDLE };
        VkImageView m_view{ VK_NULL_HANDLE };
        VkImageAspectFlags m_aspect{ VK_IMAGE_ASPECT_COLOR_BIT };
        uint32_t m_width{};
        uint32_t m_height{};
        NSRHI::EFormat m_neutralFormat{};
    };
}
