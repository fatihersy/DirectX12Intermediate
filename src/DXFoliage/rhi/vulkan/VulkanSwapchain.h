#pragma once

#include "VulkanCommon.h"
#include "rhi/ISwapchain.h"

#include <memory>
#include <vector>

namespace NSRHIVulkan
{
    class VulkanDevice;
    class VulkanTexture;

    // Vulkan implementation of ISwapchain — the counterpart of
    // DX12Swapchain.
    //
    // The interface is the same, but the mechanics underneath differ more
    // than anywhere else in this backend. DXGI hands you the current
    // backbuffer index synchronously; Vulkan's vkAcquireNextImageKHR is
    // asynchronous and signals a semaphore when the image is actually
    // usable, so the GPU work must be made to wait on it. Those semaphores
    // are owned here and exposed through backend-only accessors (the same
    // pattern as DX12Swapchain::GetBackBufferTexture) rather than widening
    // ISwapchain, which would drag a Vulkan concept into the neutral layer.
    //
    // The other asymmetry: on Vulkan a resize is not the only reason to
    // rebuild. The compositor can invalidate the swapchain at any time
    // (VK_ERROR_OUT_OF_DATE_KHR), so acquire/present report that and the
    // backend rebuilds on the next frame.
    class VulkanSwapchain final : public NSRHI::ISwapchain
    {
    public:
        static constexpr uint32_t kInvalidImage = UINT32_MAX;

        VulkanSwapchain(VulkanDevice& device, uint32_t width, uint32_t height);
        ~VulkanSwapchain() override;

        VulkanSwapchain(const VulkanSwapchain&) = delete;
        VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;

        // Returns kInvalidImage when the swapchain is out of date; the
        // caller should skip the frame and let the next one rebuild.
        uint32_t AcquireNextImage() override;
        void Present() override;
        void Resize(uint32_t width, uint32_t height) override;

        NSRHI::ITexture* GetBackBuffer(uint32_t index) override;
        uint32_t BackBufferCount() const override { return static_cast<uint32_t>(m_images.size()); }

        // --- Backend-only ---
        VkFormat Format() const { return m_format; }
        VkExtent2D Extent() const { return m_extent; }
        bool IsOutOfDate() const { return m_outOfDate; }

        // The semaphore the current acquire will signal — GPU work writing
        // to this image must wait on it.
        VkSemaphore ImageAvailableSemaphore() const { return m_currentAcquireSemaphore; }
        // The semaphore Present() waits on — submission must signal it.
        VkSemaphore RenderFinishedSemaphore() const;

        VulkanTexture* GetBackBufferTexture(uint32_t index) const { return m_images[index].get(); }

    private:
        void Create(uint32_t width, uint32_t height);
        void Destroy();

        VulkanDevice& m_device;
        VkSwapchainKHR m_swapchain{ VK_NULL_HANDLE };
        VkFormat m_format{ VK_FORMAT_UNDEFINED };
        VkExtent2D m_extent{};

        std::vector<std::unique_ptr<VulkanTexture>> m_images;

        // Acquire semaphores are a small ring rather than one per image:
        // which image an acquire returns isn't known until it completes,
        // so they can't be indexed by image. Present/render-finished
        // semaphores are per-image, which is what avoids reusing one that
        // a still-pending present is waiting on.
        std::vector<VkSemaphore> m_acquireSemaphores;
        std::vector<VkSemaphore> m_renderFinishedSemaphores;
        uint32_t m_acquireIndex{ 0 };
        VkSemaphore m_currentAcquireSemaphore{ VK_NULL_HANDLE };

        uint32_t m_currentImage{ kInvalidImage };
        bool m_outOfDate{ false };
    };
}
