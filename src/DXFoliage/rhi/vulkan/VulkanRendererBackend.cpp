#include "stdafx.h"
#include "VulkanRendererBackend.h"

#include "VulkanCommandList.h"
#include "VulkanCommon.h"
#include "VulkanDevice.h"
#include "VulkanSwapchain.h"
#include "VulkanTexture.h"

#include <array>

namespace NSRHIVulkan
{
    namespace
    {
        // How many frames the CPU may run ahead of the GPU. Two is what
        // the DX12 path uses, and it's enough to keep the queue fed
        // without the latency of a deeper chain. Note this is independent
        // of the swapchain's image count, which the driver chooses.
        constexpr uint32_t kFramesInFlight = 2;

        class VulkanRendererBackend final : public NSRHI::IRendererBackend
        {
        public:
            ~VulkanRendererBackend() override { Shutdown(); }

            bool Initialize(NSPlatform::IWindow& window, uint32_t width, uint32_t height) override;
            void Shutdown() override;
            void Resize(uint32_t width, uint32_t height) override;

            NSRHI::IDevice& GetDevice() override { return *m_device; }

            NSRHI::ICommandList& BeginFrame() override;
            void EndFrame() override;

            NSRHI::ITexture& CurrentBackBuffer() override;

        private:
            struct FrameResources
            {
                VkCommandBuffer commandBuffer{ VK_NULL_HANDLE };
                VkFence inFlight{ VK_NULL_HANDLE };
                // Whether this slot has work the fence will actually
                // signal. A frame whose acquire failed is never submitted,
                // so waiting on its fence would hang forever.
                bool submitted{ false };
            };

            std::unique_ptr<VulkanDevice> m_device;
            std::unique_ptr<VulkanSwapchain> m_swapchain;

            VkCommandPool m_commandPool{ VK_NULL_HANDLE };
            std::array<FrameResources, kFramesInFlight> m_frames{};
            uint32_t m_frameIndex{ 0 };

            VulkanCommandList m_cmdList;
            uint32_t m_currentImage{ VulkanSwapchain::kInvalidImage };
            bool m_frameValid{ false };

            uint32_t m_width{};
            uint32_t m_height{};
        };

        bool VulkanRendererBackend::Initialize(NSPlatform::IWindow& window, uint32_t width, uint32_t height)
        {
            m_width = width;
            m_height = height;

            // NativeWindowHandle's two pointers mean (wl_display*,
            // wl_surface*) for the Wayland window — see
            // WaylandWindow::GetNativeHandle. The backend is allowed to
            // know this; the front-end is not.
            const NSPlatform::NativeWindowHandle handle = window.GetNativeHandle();
            auto* display = static_cast<wl_display*>(handle.a);
            auto* surface = static_cast<wl_surface*>(handle.b);
            if (not display or not surface)
            {
                g_FError("Vulkan: window provided no native Wayland handle");
                return false;
            }

            m_device = std::make_unique<VulkanDevice>(display, surface);
            if (not m_device->IsValid())
            {
                m_device.reset();
                return false;
            }

            m_swapchain = std::make_unique<VulkanSwapchain>(*m_device, width, height);

            VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
            // RESET_COMMAND_BUFFER lets each frame's buffer be re-recorded
            // individually, which is the equivalent of D3D12 resetting a
            // per-frame command allocator.
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            poolInfo.queueFamilyIndex = m_device->QueueFamilyIndex();
            VK_CHECK(vkCreateCommandPool(m_device->Device(), &poolInfo, nullptr, &m_commandPool));

            VkCommandBufferAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
            allocInfo.commandPool = m_commandPool;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = 1;

            VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };

            for (FrameResources& frame : m_frames)
            {
                VK_CHECK(vkAllocateCommandBuffers(m_device->Device(), &allocInfo, &frame.commandBuffer));
                VK_CHECK(vkCreateFence(m_device->Device(), &fenceInfo, nullptr, &frame.inFlight));
                frame.submitted = false;
            }

            return true;
        }

        void VulkanRendererBackend::Shutdown()
        {
            if (not m_device) return;

            // Nothing may be destroyed while the GPU might still be using
            // it, and there's no "wait for last fence" shortcut that covers
            // every resource here.
            vkDeviceWaitIdle(m_device->Device());

            for (FrameResources& frame : m_frames)
            {
                if (frame.inFlight) vkDestroyFence(m_device->Device(), frame.inFlight, nullptr);
                frame = {};
            }

            if (m_commandPool)
            {
                // Frees the command buffers allocated from it too.
                vkDestroyCommandPool(m_device->Device(), m_commandPool, nullptr);
                m_commandPool = VK_NULL_HANDLE;
            }

            m_swapchain.reset();
            m_device.reset();
        }

        void VulkanRendererBackend::Resize(uint32_t width, uint32_t height)
        {
            if (not m_swapchain or width == 0 or height == 0) return;

            m_width = width;
            m_height = height;
            m_swapchain->Resize(width, height);

            // Every frame slot's fence was waited on inside Resize's
            // vkDeviceWaitIdle, so none of them has pending work anymore.
            for (FrameResources& frame : m_frames)
            {
                frame.submitted = false;
            }
        }

        NSRHI::ICommandList& VulkanRendererBackend::BeginFrame()
        {
            FrameResources& frame = m_frames[m_frameIndex];

            if (frame.submitted)
            {
                VK_CHECK(vkWaitForFences(m_device->Device(), 1, &frame.inFlight, VK_TRUE, UINT64_MAX));
                VK_CHECK(vkResetFences(m_device->Device(), 1, &frame.inFlight));
                frame.submitted = false;
            }

            // A compositor can invalidate the swapchain without any resize
            // reaching us, so rebuild on demand rather than only on Resize.
            if (m_swapchain->IsOutOfDate())
            {
                m_swapchain->Resize(m_width, m_height);
            }

            m_currentImage = m_swapchain->AcquireNextImage();
            m_frameValid = (m_currentImage != VulkanSwapchain::kInvalidImage);

            // Recording proceeds even on a failed acquire: the front-end
            // has already been handed a command list and will record into
            // it regardless, so it's simpler to let that happen and drop
            // the buffer unsubmitted in EndFrame than to hand back
            // something that can't be recorded into.
            VK_CHECK(vkResetCommandBuffer(frame.commandBuffer, 0));

            VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            VK_CHECK(vkBeginCommandBuffer(frame.commandBuffer, &beginInfo));

            m_cmdList.SetCommandBuffer(frame.commandBuffer);

            if (m_frameValid)
            {
                // Undefined rather than Present as the source state: the
                // previous contents are cleared anyway, and declaring them
                // undefined lets the driver skip preserving them.
                m_cmdList.TransitionTexture(
                    m_swapchain->GetBackBufferTexture(m_currentImage),
                    NSRHI::EResourceState::Undefined,
                    NSRHI::EResourceState::RenderTarget);
            }

            return m_cmdList;
        }

        void VulkanRendererBackend::EndFrame()
        {
            FrameResources& frame = m_frames[m_frameIndex];

            if (m_frameValid)
            {
                m_cmdList.TransitionTexture(
                    m_swapchain->GetBackBufferTexture(m_currentImage),
                    NSRHI::EResourceState::RenderTarget,
                    NSRHI::EResourceState::Present);
            }

            VK_CHECK(vkEndCommandBuffer(frame.commandBuffer));

            if (not m_frameValid)
            {
                // Nothing was acquired, so there's nothing to present.
                // The buffer is simply dropped and re-recorded next frame.
                return;
            }

            const VkSemaphore waitSemaphore = m_swapchain->ImageAvailableSemaphore();
            const VkSemaphore signalSemaphore = m_swapchain->RenderFinishedSemaphore();
            // Only the stage that writes colour has to wait for the image;
            // vertex work can start before the presentation engine is done
            // with it.
            const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

            VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
            submit.waitSemaphoreCount = 1;
            submit.pWaitSemaphores = &waitSemaphore;
            submit.pWaitDstStageMask = &waitStage;
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &frame.commandBuffer;
            submit.signalSemaphoreCount = 1;
            submit.pSignalSemaphores = &signalSemaphore;

            VK_CHECK(vkQueueSubmit(m_device->Queue(), 1, &submit, frame.inFlight));
            frame.submitted = true;

            m_swapchain->Present();

            m_frameIndex = (m_frameIndex + 1) % kFramesInFlight;
            m_currentImage = VulkanSwapchain::kInvalidImage;
            m_frameValid = false;
        }

        NSRHI::ITexture& VulkanRendererBackend::CurrentBackBuffer()
        {
            // On a failed acquire there's no current image, but the
            // front-end will still ask for one. Image 0 is a valid texture
            // to hand back — nothing recorded this frame gets submitted.
            const uint32_t index = m_frameValid ? m_currentImage : 0;
            return *m_swapchain->GetBackBufferTexture(index);
        }
    }

    std::unique_ptr<NSRHI::IRendererBackend> CreateVulkanBackend()
    {
        return std::make_unique<VulkanRendererBackend>();
    }
}
