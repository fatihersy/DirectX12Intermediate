#include "stdafx.h"
#include "VulkanCommandList.h"

#include "VulkanBuffer.h"
#include "VulkanDescriptorHeap.h"
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

    }

    void VulkanCommandList::TransitionTexture(NSRHI::ITexture* texture, NSRHI::EResourceState before, NSRHI::EResourceState after)
    {
        auto* vkTexture = static_cast<VulkanTexture*>(texture);
        if (not vkTexture) return;

        const ImageBarrierState src = ToBarrierState(before);
        const ImageBarrierState dst = ToBarrierState(after);

        VkImageMemoryBarrier2 barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.srcStageMask = src.stage;
        barrier.srcAccessMask = src.access;
        barrier.dstStageMask = dst.stage;
        barrier.dstAccessMask = dst.access;
        barrier.oldLayout = src.layout;
        barrier.newLayout = dst.layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = vkTexture->Image();
        // From the texture, not from the states being transitioned
        // between. Inferring it happens to work for Present->RenderTarget
        // but not for Undefined->DepthWrite, and a combined depth/stencil
        // format needs BOTH aspects named or the stencil half is left
        // behind in the wrong layout.
        barrier.subresourceRange.aspectMask = vkTexture->Aspect();
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;

        VkDependencyInfo dependency{};
        dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
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

            VkRenderingAttachmentInfo info{};
            info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            info.imageView = target->View();
            info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            info.loadOp = attachment.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
            info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            info.clearValue.color = { { attachment.clearColor.r, attachment.clearColor.g,
                                        attachment.clearColor.b, attachment.clearColor.a } };
            colors.push_back(info);
        }

        VkRenderingAttachmentInfo depth{};
        depth.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
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

        VkRenderingInfo info{};
        info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
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
        m_boundLayoutUsesBindless = vkPipeline->UsesBindlessSet();
        m_boundConstantSet = vkPipeline->ConstantSet();
        m_boundConstantSlots = vkPipeline->NumConstantSlots();
        // Offsets are stale for the new layout until the front-end sets
        // them again; marking dirty re-binds set 1 with whatever the next
        // SetConstantBuffer calls provide before the draw.
        m_constantsDirty = m_boundConstantSet != VK_NULL_HANDLE;

        // Descriptor sets are bound per pipeline layout on Vulkan, so a
        // heap set once per frame has to be re-bound whenever the layout
        // changes. D3D12 needs no equivalent - its heap survives PSO
        // changes - which is why the neutral interface has no "rebind".
        BindDescriptorSet();
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

    void VulkanCommandList::SetDescriptorHeap(NSRHI::IDescriptorHeap* heap)
    {
        auto* vkHeap = static_cast<VulkanDescriptorHeap*>(heap);
        if (not vkHeap) return;

        // Deferred, not bound here. vkCmdBindDescriptorSets needs the
        // pipeline layout, and D3D12's SetDescriptorHeaps does not - so
        // the neutral call can legally arrive BEFORE any pipeline is
        // bound, which is how the front-end naturally writes it (heap once
        // per frame, pipelines per pass). Remembering it and binding at
        // SetPipeline keeps both orderings working.
        m_boundHeap = vkHeap;
        BindDescriptorSet();
    }

    void VulkanCommandList::BindDescriptorSet()
    {
        if (not m_boundHeap or m_boundLayout == VK_NULL_HANDLE) return;
        // Binding set 0 against a layout that declares no sets is a
        // validation error, and the cube pipeline's layout is exactly that.
        if (not m_boundLayoutUsesBindless) return;

        const VkDescriptorSet set = m_boundHeap->Set();
        vkCmdBindDescriptorSets(m_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_boundLayout, 0, 1, &set, 0, nullptr);
    }

    void VulkanCommandList::SetConstantBuffer(uint32_t slot, uint64_t offsetBytes)
    {
        ASSERT(slot < m_boundConstantSlots,
            "SetConstantBuffer slot not declared by the bound pipeline's layout");
        ASSERT((offsetBytes % NSRHI::kConstantBufferAlignment) == 0,
            "Offset not from ConstantAllocator::Allocate");

        m_constantOffsets[slot] = static_cast<uint32_t>(offsetBytes);
        m_constantsDirty = true;
    }

    void VulkanCommandList::FlushConstantBinds()
    {
        if (not m_constantsDirty or m_boundConstantSet == VK_NULL_HANDLE) return;

        // Set 1 with every slot's dynamic offset in one call. Binding a
        // HIGHER set never disturbs a lower one, so set 0 (the bindless
        // heap, bound at SetPipeline) survives this.
        vkCmdBindDescriptorSets(m_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_boundLayout, 1, 1, &m_boundConstantSet,
            m_boundConstantSlots, m_constantOffsets);
        m_constantsDirty = false;
    }

    void VulkanCommandList::SetVertexBuffer(NSRHI::IBuffer* buffer, uint32_t,
                                            uint64_t offsetBytes, uint64_t)
    {
        auto* vkBuffer = static_cast<VulkanBuffer*>(buffer);
        if (not vkBuffer) return;

        // Two arguments ignored here, for opposite reasons. The STRIDE is
        // baked into the pipeline's VkVertexInputBindingDescription,
        // whereas D3D12 carries it on the buffer view. The SIZE is
        // likewise implicit — Vulkan binds to the end of the buffer,
        // while D3D12's view is an explicit (location, size) pair. Both
        // stay in the neutral signature because DX12 needs them.
        const VkBuffer handle = vkBuffer->Raw();
        const VkDeviceSize offset = offsetBytes;
        vkCmdBindVertexBuffers(m_cmd, 0, 1, &handle, &offset);
    }

    void VulkanCommandList::SetIndexBuffer(NSRHI::IBuffer* buffer, bool is32Bit,
                                           uint64_t offsetBytes, uint64_t)
    {
        auto* vkBuffer = static_cast<VulkanBuffer*>(buffer);
        if (not vkBuffer) return;

        vkCmdBindIndexBuffer(m_cmd, vkBuffer->Raw(), offsetBytes,
            is32Bit ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
    }

    void VulkanCommandList::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex)
    {
        FlushConstantBinds();
        vkCmdDraw(m_cmd, vertexCount, instanceCount, firstVertex, 0);
    }

    void VulkanCommandList::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset)
    {
        FlushConstantBinds();
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

    void VulkanCommandList::CopyBufferToTexture(NSRHI::ITexture* destination, NSRHI::IBuffer* source,
                                                const NSRHI::TextureRegion& region,
                                                uint32_t srcRowPitchBytes,
                                                uint64_t srcOffsetBytes)
    {
        auto* dst = static_cast<VulkanTexture*>(destination);
        auto* src = static_cast<VulkanBuffer*>(source);
        if (not dst or not src) return;

        ASSERT(region.width > 0 and region.height > 0, "Empty copy region");
        ASSERT(region.x + region.width <= dst->Width() and
               region.y + region.height <= dst->Height(),
            "Copy region extends past the texture");

        // bufferRowLength is in TEXELS, not bytes — the one place this
        // differs from D3D12's PlacedFootprint.RowPitch, which is in
        // bytes. Zero would mean "tightly packed to imageExtent", which
        // is wrong whenever the caller padded rows for DX12's benefit or
        // is uploading a sub-rect out of a wider atlas.
        const uint32_t bytesPerTexel = NSRHI::BytesPerTexel(dst->Format());
        ASSERT(bytesPerTexel > 0, "Texture format has no defined texel size");
        ASSERT(srcRowPitchBytes % bytesPerTexel == 0,
            "Row pitch must be a whole number of texels for Vulkan's bufferRowLength");

        VkBufferImageCopy copy{};
        copy.bufferOffset = srcOffsetBytes;
        copy.bufferRowLength = srcRowPitchBytes / bytesPerTexel;
        copy.bufferImageHeight = 0;  // tightly packed rows within the region
        copy.imageSubresource.aspectMask = dst->Aspect();
        copy.imageSubresource.mipLevel = 0;
        copy.imageSubresource.baseArrayLayer = 0;
        copy.imageSubresource.layerCount = 1;
        copy.imageOffset = { static_cast<int32_t>(region.x), static_cast<int32_t>(region.y), 0 };
        copy.imageExtent = { region.width, region.height, 1 };

        vkCmdCopyBufferToImage(m_cmd, src->Raw(), dst->Image(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    }
}
