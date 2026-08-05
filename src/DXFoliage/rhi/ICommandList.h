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

        // offsetBytes lets the geometry be one allocation inside a shared
        // per-frame ring rather than a dedicated buffer — which is how
        // anything that rebuilds geometry every frame (ImGui, and later
        // foliage instance data) has to work. Both APIs take the offset
        // natively: vkCmdBindVertexBuffers has an offsets array, and
        // D3D12's buffer views are (BufferLocation, SizeInBytes) pairs.
        //
        // Note this is NOT the same knob as DrawIndexed's vertexOffset:
        // that one is in vertices and ImGui needs it for its own
        // per-command sub-ranges, so it cannot double as the allocation
        // offset.
        virtual void SetVertexBuffer(IBuffer* buffer, uint32_t strideBytes,
                                     uint64_t offsetBytes = 0, uint64_t sizeBytes = 0) = 0;
        virtual void SetIndexBuffer(IBuffer* buffer, bool is32Bit,
                                    uint64_t offsetBytes = 0, uint64_t sizeBytes = 0) = 0;

        // --- Draws ---
        virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0) = 0;
        virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0) = 0;

        // --- Uploads ---
        virtual void CopyBuffer(IBuffer* destination, IBuffer* source, size_t sizeBytes) = 0;

        // Upload a rectangle of `source` into `destination`. The whole
        // texture is region = {0, 0, width, height}.
        //
        // srcRowPitchBytes is the stride between rows AS PACKED IN THE
        // BUFFER, and it is explicit because the two APIs disagree: D3D12
        // requires every row aligned to 256 bytes
        // (D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) while Vulkan is happy with
        // tightly packed rows. Ask IDevice::TextureRowPitchAlignment()
        // and pack to that, exactly as ImGui's own backends do — theirs
        // differ on precisely this line and nothing else.
        //
        // srcOffsetBytes lets the source be one allocation inside a
        // shared staging ring rather than a dedicated buffer.
        //
        // The caller transitions the texture to CopyDestination first;
        // this records the copy only, matching CopyBuffer and the barrier
        // calls.
        virtual void CopyBufferToTexture(ITexture* destination, IBuffer* source,
                                         const TextureRegion& region,
                                         uint32_t srcRowPitchBytes,
                                         uint64_t srcOffsetBytes = 0) = 0;
    };
}
