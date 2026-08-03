#pragma once

#include "IBuffer.h"
#include "IDescriptorHeap.h"
#include "IPipeline.h"
#include "ITexture.h"
#include "RHITypes.h"

#include <cstddef>
#include <cstdint>
#include <vector>

// The vocabulary the renderer records once per frame. Shaped after the
// existing NSDX12::GraphicsCommandList wrapper (rhi/dx12/DirectXTypes.h) —
// that class already presents a clean, minimal API over
// ID3D12GraphicsCommandList10, so this interface generalizes it rather
// than inventing something new: rhi/dx12/DX12CommandList.cpp forwards
// each call almost 1:1 to the matching D3D12 method,
// rhi/vulkan/VulkanCommandList.cpp forwards to the matching vkCmd* call.
namespace NSRHI
{
    struct RenderingAttachment
    {
        ITexture* target{ nullptr };
        bool clear{ false };
        ClearColor clearColor;
    };

    struct DepthAttachment
    {
        ITexture* target{ nullptr };
        bool clear{ false };
        float clearDepth{ 1.0f };
    };

    class ICommandList
    {
    public:
        virtual ~ICommandList() = default;

        // --- Resource state ---
        virtual void TransitionTexture(ITexture* texture, EResourceState before, EResourceState after) = 0;

        // --- Rendering scope ---
        // Maps directly to vkCmdBeginRendering/vkCmdEndRendering on the
        // Vulkan backend (see the plan's dynamic-rendering decision — no
        // VkRenderPass/VkFramebuffer objects to manage). The DX12 backend
        // reaches the same result with OMSetRenderTargets + the matching
        // ClearRenderTargetView/ClearDepthStencilView calls internally;
        // D3D12 has no render-pass object of its own to map onto anyway.
        virtual void BeginRendering(const std::vector<RenderingAttachment>& colorAttachments, const DepthAttachment* depthAttachment) = 0;
        virtual void EndRendering() = 0;

        // --- Fixed-function state ---
        virtual void SetViewport(const Viewport& viewport) = 0;
        virtual void SetScissor(const ScissorRect& scissor) = 0;

        // --- Pipeline / resource binding ---
        virtual void SetPipeline(IPipeline* pipeline) = 0;
        virtual void SetRootConstants(uint32_t offsetIn32BitValues, uint32_t num32BitValues, const void* data) = 0;
        virtual void SetDescriptorHeap(IDescriptorHeap* heap) = 0;

        // Point constant slot `slot` at byte `offsetBytes` of the buffer
        // the bound pipeline layout was created with (see
        // PipelineLayoutDesc::constantBuffer). The offset comes from
        // NSAllocator::Ctx. The neutral form of DXTerrain's
        // SetGraphicsRootConstantBufferView(slot, gpuAddr):
        //   - DX12 rebinds immediately — root CBV at baseVA + offset.
        //   - Vulkan records the offset and binds lazily at the next
        //     draw, because every dynamic offset for the set travels in
        //     ONE vkCmdBindDescriptorSets call.
        // Call after SetPipeline; offsets do not survive a pipeline
        // layout change on DX12 (root signature swap clears root state).
        virtual void SetConstantBuffer(uint32_t slot, uint64_t offsetBytes) = 0;

        virtual void SetVertexBuffer(IBuffer* buffer, uint32_t strideBytes) = 0;
        virtual void SetIndexBuffer(IBuffer* buffer, bool is32Bit) = 0;

        // --- Draws ---
        virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0) = 0;
        virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0) = 0;

        // --- Uploads ---
        virtual void CopyBuffer(IBuffer* destination, IBuffer* source, size_t sizeBytes) = 0;
        virtual void CopyBufferToTexture(ITexture* destination, IBuffer* source) = 0;
    };
}
