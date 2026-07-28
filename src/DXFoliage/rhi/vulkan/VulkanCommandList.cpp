#include "stdafx.h"
#include "VulkanCommandList.h"

#include "VulkanBuffer.h"
#include "VulkanPipeline.h"
#include "VulkanTexture.h"

namespace NSRHIVulkan
{
    namespace
    {
        // D3D12 describes a resource's use with a single state enum and
        // works out the barrier itself. Vulkan needs three things: the
        // image layout, which pipeline stages are involved, and which
        // accesses. This is where the neutral EResourceState is expanded
        // into all three.
        struct ImageBarrierState
        {
            VkImageLayout layout;
            VkPipelineStageFlags2 stage;
            VkAccessFlags2 access;
        };

        ImageBarrierState ToBarrierState(NSRHI::EResourceState state)
        {
            switch (state)
            {
                case NSRHI::EResourceState::RenderTarget:
                    return { VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                             VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT };
                case NSRHI::EResourceState::DepthWrite:
                    return { VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                             VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                             VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT };
                case NSRHI::EResourceState::DepthRead:
                    return { VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                             VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                             VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT };
                case NSRHI::EResourceState::Present:
                    // Nothing reads a presented image from our side; the
                    // presentation engine synchronises via the semaphore,
                    // so no access mask is needed here.
                    return { VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                             VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                             VK_ACCESS_2_NONE };
                case NSRHI::EResourceState::CopySource:
                    return { VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             VK_PIPELINE_STAGE_2_COPY_BIT,
                             VK_ACCESS_2_TRANSFER_READ_BIT };
                case NSRHI::EResourceState::CopyDestination:
                    return { VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_PIPELINE_STAGE_2_COPY_BIT,
                             VK_ACCESS_2_TRANSFER_WRITE_BIT };
                case NSRHI::EResourceState::ShaderResource:
                    return { VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                             VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                             VK_ACCESS_2_SHADER_READ_BIT };
                case NSRHI::EResourceState::Undefined:
                default:
                    return { VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                             VK_ACCESS_2_NONE };
            }
        }

        bool IsDepthState(NSRHI::EResourceState state)
        {
            return state == NSRHI::EResourceState::DepthWrite
                or state == NSRHI::EResourceState::DepthRead;
        }
    }

    void VulkanCommandList::TransitionTexture(NSRHI::ITexture* texture, NSRHI::EResourceState before, NSRHI::EResourceState after)
    {
        auto* vkTexture = static_cast<VulkanTexture*>(texture);
        if (not vkTexture) return;

        const ImageBarrierState src = ToBarrierState(before);
        const ImageBarrierState dst = ToBarrierState(after);

        VkImageMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
        barrier.srcStageMask = src.stage;
        barrier.srcAccessMask = src.access;
        barrier.dstStageMask = dst.stage;
        barrier.dstAccessMask = dst.access;
        barrier.oldLayout = src.layout;
        barrier.newLayout = dst.layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = vkTexture->Image();
        barrier.subresourceRange.aspectMask = (IsDepthState(before) or IsDepthState(after))
            ? VK_IMAGE_ASPECT_DEPTH_BIT
            : VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;

        VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        dependency.imageMemoryBarrierCount = 1;
        dependency.pImageMemoryBarriers = &barrier;

        vkCmdPipelineBarrier2(m_cmd, &dependency);
    }

    void VulkanCommandList::BeginRendering(const std::vector<NSRHI::RenderingAttachment>& colorAttachments, const NSRHI::DepthAttachment* depthAttachment)
    {
        std::vector<VkRenderingAttachmentInfo> colors;
        colors.reserve(colorAttachments.size());

        uint32_t width{};
        uint32_t height{};

        for (const NSRHI::RenderingAttachment& attachment : colorAttachments)
        {
            auto* target = static_cast<VulkanTexture*>(attachment.target);
            if (not target) continue;

            width = std::max(width, target->Width());
            height = std::max(height, target->Height());

            VkRenderingAttachmentInfo info{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
            info.imageView = target->View();
            info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            info.loadOp = attachment.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
            info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            info.clearValue.color = { { attachment.clearColor.r, attachment.clearColor.g,
                                        attachment.clearColor.b, attachment.clearColor.a } };
            colors.push_back(info);
        }

        VkRenderingAttachmentInfo depth{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        const bool hasDepth = depthAttachment != nullptr and depthAttachment->target != nullptr;
        if (hasDepth)
        {
            auto* target = static_cast<VulkanTexture*>(depthAttachment->target);
            width = std::max(width, target->Width());
            height = std::max(height, target->Height());

            depth.imageView = target->View();
            depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depth.loadOp = depthAttachment->clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
            depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            depth.clearValue.depthStencil.depth = depthAttachment->clearDepth;
        }

        VkRenderingInfo info{ VK_STRUCTURE_TYPE_RENDERING_INFO };
        info.renderArea.extent = { width, height };
        info.layerCount = 1;
        info.colorAttachmentCount = static_cast<uint32_t>(colors.size());
        info.pColorAttachments = colors.data();
        info.pDepthAttachment = hasDepth ? &depth : nullptr;

        vkCmdBeginRendering(m_cmd, &info);
    }

    void VulkanCommandList::EndRendering()
    {
        vkCmdEndRendering(m_cmd);
    }

    void VulkanCommandList::SetViewport(const NSRHI::Viewport& viewport)
    {
        // Negative height with y flipped to the bottom puts Vulkan's
        // clip space in the same orientation as D3D12's, so the same
        // shaders and the same vertex data draw right-side-up on both
        // backends without a projection-matrix fixup. (Requires Vulkan
        // 1.1+, which this backend already does.)
        VkViewport vp{};
        vp.x = viewport.x;
        vp.y = viewport.y + viewport.height;
        vp.width = viewport.width;
        vp.height = -viewport.height;
        vp.minDepth = viewport.minDepth;
        vp.maxDepth = viewport.maxDepth;

        vkCmdSetViewport(m_cmd, 0, 1, &vp);
    }

    void VulkanCommandList::SetScissor(const NSRHI::ScissorRect& scissor)
    {
        VkRect2D rect{};
        rect.offset = { scissor.left, scissor.top };
        rect.extent = { static_cast<uint32_t>(scissor.right - scissor.left),
                        static_cast<uint32_t>(scissor.bottom - scissor.top) };

        vkCmdSetScissor(m_cmd, 0, 1, &rect);
    }

    void VulkanCommandList::SetPipeline(NSRHI::IPipeline* pipeline)
    {
        auto* vkPipeline = static_cast<VulkanPipeline*>(pipeline);
        if (not vkPipeline) return;

        vkPipeline->Bind(m_cmd);
        m_boundLayout = vkPipeline->Layout();
    }

    void VulkanCommandList::SetRootConstants(uint32_t offsetIn32BitValues, uint32_t num32BitValues, const void* data)
    {
        ASSERT(m_boundLayout != VK_NULL_HANDLE, "SetRootConstants before SetPipeline");

        vkCmdPushConstants(m_cmd, m_boundLayout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            offsetIn32BitValues * sizeof(uint32_t),
            num32BitValues * sizeof(uint32_t),
            data);
    }

    void VulkanCommandList::SetDescriptorHeap(NSRHI::IDescriptorHeap*)
    {
        // No Vulkan equivalent to bind: D3D12's single global heap becomes
        // a VkDescriptorSet bound per pipeline layout, which arrives with
        // the deferred bindless design.
    }

    void VulkanCommandList::SetVertexBuffer(NSRHI::IBuffer* buffer, uint32_t)
    {
        auto* vkBuffer = static_cast<VulkanBuffer*>(buffer);
        if (not vkBuffer) return;

        // The stride argument is ignored: Vulkan bakes it into the
        // pipeline's VkVertexInputBindingDescription, whereas D3D12
        // carries it on the vertex buffer view. The neutral signature
        // keeps it because DX12 needs it.
        const VkBuffer handle = vkBuffer->Raw();
        const VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(m_cmd, 0, 1, &handle, &offset);
    }

    void VulkanCommandList::SetIndexBuffer(NSRHI::IBuffer* buffer, bool is32Bit)
    {
        auto* vkBuffer = static_cast<VulkanBuffer*>(buffer);
        if (not vkBuffer) return;

        vkCmdBindIndexBuffer(m_cmd, vkBuffer->Raw(), 0,
            is32Bit ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
    }

    void VulkanCommandList::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex)
    {
        vkCmdDraw(m_cmd, vertexCount, instanceCount, firstVertex, 0);
    }

    void VulkanCommandList::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset)
    {
        vkCmdDrawIndexed(m_cmd, indexCount, instanceCount, firstIndex, vertexOffset, 0);
    }

    void VulkanCommandList::CopyBuffer(NSRHI::IBuffer* destination, NSRHI::IBuffer* source, size_t sizeBytes)
    {
        auto* dst = static_cast<VulkanBuffer*>(destination);
        auto* src = static_cast<VulkanBuffer*>(source);
        if (not dst or not src) return;

        VkBufferCopy region{};
        region.size = sizeBytes;
        vkCmdCopyBuffer(m_cmd, src->Raw(), dst->Raw(), 1, &region);
    }

    void VulkanCommandList::CopyBufferToTexture(NSRHI::ITexture*, NSRHI::IBuffer*)
    {
        // Paired with VulkanDevice::CreateTexture — arrives with the
        // sampled-texture/model work.
        ASSERT(false, "CopyBufferToTexture: not implemented on the Vulkan backend yet");
    }
}
