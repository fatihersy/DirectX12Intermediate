#pragma once

#include "VulkanCommon.h"
#include "rhi/ICommandList.h"

namespace NSRHIVulkan
{
    class VulkanDescriptorHeap;

    // Vulkan implementation of the exposed ICommandList — the counterpart
    // of DX12CommandList. Each method downcasts the I* it's handed to the
    // Vulkan concrete (safe by the backend-affinity invariant) and forwards
    // to the matching vkCmd* call.
    //
    // Like DX12CommandList this is a thin, rebindable view: the
    // VkCommandBuffer itself is owned by VulkanRendererBackend, which
    // points this wrapper at the right per-frame buffer via SetCommandBuffer
    // before recording starts.
    class VulkanCommandList final : public NSRHI::ICommandList
    {
    public:
        VulkanCommandList() = default;

        void SetCommandBuffer(VkCommandBuffer cmd) { m_cmd = cmd; }

        void TransitionTexture(NSRHI::ITexture* texture, NSRHI::EResourceState before, NSRHI::EResourceState after) override;

        void BeginRendering(const std::vector<NSRHI::RenderingAttachment>& colorAttachments, const NSRHI::DepthAttachment* depthAttachment) override;
        void EndRendering() override;

        void SetViewport(const NSRHI::Viewport& viewport) override;
        void SetScissor(const NSRHI::ScissorRect& scissor) override;

        void SetPipeline(NSRHI::IPipeline* pipeline) override;
        void SetRootConstants(uint32_t offsetIn32BitValues, uint32_t num32BitValues, const void* data) override;
        void SetDescriptorHeap(NSRHI::IDescriptorHeap* heap) override;
        void SetConstantBuffer(uint32_t slot, uint64_t offsetBytes) override;

        void SetVertexBuffer(NSRHI::IBuffer* buffer, uint32_t strideBytes) override;
        void SetIndexBuffer(NSRHI::IBuffer* buffer, bool is32Bit) override;

        void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex) override;
        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset) override;

        void CopyBuffer(NSRHI::IBuffer* destination, NSRHI::IBuffer* source, size_t sizeBytes) override;
        void CopyBufferToTexture(NSRHI::ITexture* destination, NSRHI::IBuffer* source) override;

    private:
        void BindDescriptorSet();
        void FlushConstantBinds();

        VkCommandBuffer m_cmd{ VK_NULL_HANDLE };

        // The heap the front-end last set. Bound lazily rather than
        // immediately, because vkCmdBindDescriptorSets needs a pipeline
        // layout and SetDescriptorHeap can legally be called first.
        VulkanDescriptorHeap* m_boundHeap{ nullptr };
        bool m_boundLayoutUsesBindless{ false };

        // Constant-slot state for the bound layout. Offsets accumulate as
        // SetConstantBuffer calls arrive and go to the GPU in ONE
        // vkCmdBindDescriptorSets at the next draw — the API takes every
        // dynamic offset for the set together, so per-slot immediate
        // binds are not expressible. DX12 has no equivalent state: its
        // root CBVs rebind one at a time.
        VkDescriptorSet m_boundConstantSet{ VK_NULL_HANDLE };
        uint32_t m_boundConstantSlots{ 0 };
        uint32_t m_constantOffsets[NSRHI::kMaxConstantBufferSlots]{};
        bool m_constantsDirty{ false };

        // vkCmdPushConstants needs the pipeline layout, which D3D12's
        // SetGraphicsRoot32BitConstants doesn't (the root signature is
        // already bound). Remembered from the last SetPipeline so the
        // neutral SetRootConstants signature doesn't have to carry it.
        VkPipelineLayout m_boundLayout{ VK_NULL_HANDLE };
    };
}
