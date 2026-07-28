#pragma once

#include "VulkanCommon.h"
#include "rhi/ITexture.h"

namespace NSRHIVulkan
{
    // Vulkan implementation of ITexture. Currently only wraps images the
    // swapchain owns, so it does NOT destroy the VkImage — the swapchain
    // does. It does own the VkImageView it creates for that image.
    //
    // Under dynamic rendering there's no framebuffer or RTV heap: the view
    // is handed straight to vkCmdBeginRendering, which is why DX12Texture's
    // private descriptor heap has no counterpart here.
    class VulkanTexture final : public NSRHI::ITexture
    {
    public:
        VulkanTexture(VkDevice device, VkImage image, VkFormat format, uint32_t width, uint32_t height, NSRHI::EFormat neutralFormat);
        ~VulkanTexture() override;

        VulkanTexture(const VulkanTexture&) = delete;
        VulkanTexture& operator=(const VulkanTexture&) = delete;

        uint32_t Width() const override { return m_width; }
        uint32_t Height() const override { return m_height; }
        NSRHI::EFormat Format() const override { return m_neutralFormat; }

        VkImage Image() const { return m_image; }
        VkImageView View() const { return m_view; }

    private:
        VkDevice m_device{ VK_NULL_HANDLE };
        VkImage m_image{ VK_NULL_HANDLE };   // owned by the swapchain
        VkImageView m_view{ VK_NULL_HANDLE };
        uint32_t m_width{};
        uint32_t m_height{};
        NSRHI::EFormat m_neutralFormat{};
    };
}
