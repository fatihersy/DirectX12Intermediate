#pragma once

#include "PlatformHeaders_DX12.h"
#include "DirectXTypes.h"
#include "rhi/ICommandList.h"

// DX12 implementation of the exposed ICommandList — the command context
// the front-end records draws into. Thin: each method downcasts the I*
// it's handed to the DX12 concrete (safe by the backend-affinity
// invariant — one live backend per run) and forwards to the existing
// NSDX12::GraphicsCommandList wrapper, which already presents most of
// these D3D12 calls.
//
// Wraps a persistent ID3D12GraphicsCommandList10 owned by
// RendererBackend_DX12 — the backend Resets that list each frame; this
// wrapper points at the same object across frames.
namespace NSRHIDX12
{
    class DX12CommandList final : public NSRHI::ICommandList
    {
    public:
        DX12CommandList() = default;
        explicit DX12CommandList(ID3D12GraphicsCommandList10* cmd) : m_cmd(cmd) {}

        void TransitionTexture(NSRHI::ITexture* texture, NSRHI::EResourceState before, NSRHI::EResourceState after) override;

        void BeginRendering(const std::vector<NSRHI::RenderingAttachment>& colorAttachments, const NSRHI::DepthAttachment* depthAttachment) override;
        void EndRendering() override;

        void SetViewport(const NSRHI::Viewport& viewport) override;
        void SetScissor(const NSRHI::ScissorRect& scissor) override;

        void SetPipeline(NSRHI::IPipeline* pipeline) override;
        void SetRootConstants(uint32_t offsetIn32BitValues, uint32_t num32BitValues, const void* data) override;
        void SetDescriptorHeap(NSRHI::IDescriptorHeap* heap) override;
        void SetConstantBuffer(uint32_t slot, uint64_t offsetBytes) override;

        void SetVertexBuffer(NSRHI::IBuffer* buffer, uint32_t strideBytes,
                             uint64_t offsetBytes = 0, uint64_t sizeBytes = 0) override;
        void SetIndexBuffer(NSRHI::IBuffer* buffer, bool is32Bit,
                            uint64_t offsetBytes = 0, uint64_t sizeBytes = 0) override;

        void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex) override;
        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset) override;

        void CopyBuffer(NSRHI::IBuffer* destination, NSRHI::IBuffer* source, size_t sizeBytes) override;
        void CopyBufferToTexture(NSRHI::ITexture* destination, NSRHI::IBuffer* source,
                                 const NSRHI::TextureRegion& region,
                                 uint32_t srcRowPitchBytes,
                                 uint64_t srcOffsetBytes = 0) override;

    private:
        NSDX12::GraphicsCommandList m_cmd;

        // From the bound pipeline at SetPipeline — root CBV binds need
        // the base VA and the slot->root-parameter mapping, which D3D12's
        // SetGraphicsRootConstantBufferView (unlike Vulkan's set binding)
        // consumes one slot at a time, immediately.
        D3D12_GPU_VIRTUAL_ADDRESS m_constantBaseVA{};
        uint32_t m_constantRootParamBase{ 0 };
        uint32_t m_numConstantSlots{ 0 };
    };
}
