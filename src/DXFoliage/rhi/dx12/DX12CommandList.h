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

        void SetVertexBuffer(NSRHI::IBuffer* buffer, uint32_t strideBytes) override;
        void SetIndexBuffer(NSRHI::IBuffer* buffer, bool is32Bit) override;

        void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex) override;
        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset) override;

        void CopyBuffer(NSRHI::IBuffer* destination, NSRHI::IBuffer* source, size_t sizeBytes) override;
        void CopyBufferToTexture(NSRHI::ITexture* destination, NSRHI::IBuffer* source) override;

    private:
        NSDX12::GraphicsCommandList m_cmd;
    };
}
