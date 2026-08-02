#include "stdafx.h"
#include "DX12CommandList.h"

#include "DXSampleHelper.h"

#include "DX12Buffer.h"
#include "DX12Pipeline.h"
#include "DX12Texture.h"

namespace NSRHIDX12
{
    namespace
    {
        D3D12_RESOURCE_STATES ToD3D12State(NSRHI::EResourceState state)
        {
            switch (state)
            {
                case NSRHI::EResourceState::RenderTarget:    return D3D12_RESOURCE_STATE_RENDER_TARGET;
                case NSRHI::EResourceState::DepthWrite:      return D3D12_RESOURCE_STATE_DEPTH_WRITE;
                case NSRHI::EResourceState::DepthRead:       return D3D12_RESOURCE_STATE_DEPTH_READ;
                case NSRHI::EResourceState::Present:         return D3D12_RESOURCE_STATE_PRESENT;
                case NSRHI::EResourceState::CopySource:      return D3D12_RESOURCE_STATE_COPY_SOURCE;
                case NSRHI::EResourceState::CopyDestination: return D3D12_RESOURCE_STATE_COPY_DEST;
                case NSRHI::EResourceState::ShaderResource:  return D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
                // KNOWN GAP - this mapping is wrong and will trip the
                // debug layer. See TransitionTexture below.
                case NSRHI::EResourceState::Undefined:       return D3D12_RESOURCE_STATE_COMMON;
                default:                                     return D3D12_RESOURCE_STATE_COMMON;
            }
        }
    }

    // KNOWN GAP, untested because this path has never been compiled.
    //
    // EResourceState::Undefined means "discard the contents" - see
    // rhi/RHITypes.h. Vulkan expresses that directly as an oldLayout of
    // VK_IMAGE_LAYOUT_UNDEFINED. D3D12's LEGACY resource barriers have no
    // equivalent: the `before` state must match the resource's ACTUAL
    // current state or the debug layer reports a mismatch.
    //
    // So the COMMON mapping above is wrong in a specific, reproducible
    // way. Renderer::BeginFrame transitions the depth buffer from
    // Undefined every frame; after the first, the resource is really in
    // DEPTH_WRITE, and COMMON -> DEPTH_WRITE will error.
    //
    // The fix is Enhanced Barriers (ID3D12GraphicsCommandList7::Barrier),
    // which added D3D12_BARRIER_LAYOUT_UNDEFINED plus explicit sync and
    // access masks - deliberately modelled on Vulkan, so it maps onto this
    // interface far better than legacy barriers do. This backend already
    // requires ID3D12GraphicsCommandList10, so the API is available; it
    // simply has not been rewritten. Do that before trusting this path.
    void DX12CommandList::TransitionTexture(NSRHI::ITexture* texture, NSRHI::EResourceState before, NSRHI::EResourceState after)
    {
        auto* dx = static_cast<DX12Texture*>(texture);
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(dx->Raw(), ToD3D12State(before), ToD3D12State(after));
        m_cmd.ResourceBarrier(1, &barrier);
    }

    void DX12CommandList::BeginRendering(const std::vector<NSRHI::RenderingAttachment>& colorAttachments, const NSRHI::DepthAttachment* depthAttachment)
    {
        // Dynamic-rendering-style: bind the attachments and clear. The
        // caller transitions the textures to the right state beforehand
        // (via TransitionTexture) — same split as Vulkan (barrier, then
        // vkCmdBeginRendering).
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles;
        rtvHandles.reserve(colorAttachments.size());
        for (const NSRHI::RenderingAttachment& att : colorAttachments)
        {
            rtvHandles.push_back(static_cast<DX12Texture*>(att.target)->View());
        }

        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle{};
        const D3D12_CPU_DESCRIPTOR_HANDLE* dsvPtr = nullptr;
        if (depthAttachment && depthAttachment->target)
        {
            dsvHandle = static_cast<DX12Texture*>(depthAttachment->target)->View();
            dsvPtr = &dsvHandle;
        }

        m_cmd.OMSetRenderTargets(static_cast<UINT>(rtvHandles.size()), rtvHandles.data(), FALSE, dsvPtr);

        for (size_t i = 0; i < colorAttachments.size(); ++i)
        {
            const NSRHI::RenderingAttachment& att = colorAttachments[i];
            if (att.clear)
            {
                const FLOAT color[4]{ att.clearColor.r, att.clearColor.g, att.clearColor.b, att.clearColor.a };
                m_cmd.ClearRenderTargetView(rtvHandles[i], color, 0, nullptr);
            }
        }

        if (depthAttachment && depthAttachment->target && depthAttachment->clear)
        {
            m_cmd.ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, depthAttachment->clearDepth, 0, 0, nullptr);
        }
    }

    void DX12CommandList::EndRendering()
    {
        // No-op on D3D12: OMSetRenderTargets has no matching "end", and the
        // transition back out of the render-target state is issued
        // explicitly via TransitionTexture. On Vulkan this becomes
        // vkCmdEndRendering.
    }

    void DX12CommandList::SetViewport(const NSRHI::Viewport& viewport)
    {
        CD3DX12_VIEWPORT vp(viewport.x, viewport.y, viewport.width, viewport.height, viewport.minDepth, viewport.maxDepth);
        m_cmd.RSSetViewports(1, &vp);
    }

    void DX12CommandList::SetScissor(const NSRHI::ScissorRect& scissor)
    {
        D3D12_RECT rect{ scissor.left, scissor.top, scissor.right, scissor.bottom };
        m_cmd.RSSetScissorRects(1, &rect);
    }

    void DX12CommandList::SetPipeline(NSRHI::IPipeline* pipeline)
    {
        // DX12Pipeline::Bind sets PSO + root signature + primitive topology.
        static_cast<DX12Pipeline*>(pipeline)->Bind(m_cmd);
    }

    void DX12CommandList::SetRootConstants(uint32_t offsetIn32BitValues, uint32_t num32BitValues, const void* data)
    {
        // Root parameter 0 is the 32-bit-constants slot (see
        // DX12PipelineLayout — constants come first when present).
        m_cmd.SetGraphicsRoot32BitConstants(0, num32BitValues, data, offsetIn32BitValues);
    }

    void DX12CommandList::SetDescriptorHeap(NSRHI::IDescriptorHeap*)
    {
        // Reserved for later — bindless descriptor heaps are deferred
        // (see IDescriptorHeap.h). Lands with the texture/model work.
        ASSERT(false, "SetDescriptorHeap: bindless not implemented yet");
    }

    void DX12CommandList::SetVertexBuffer(NSRHI::IBuffer* buffer, uint32_t strideBytes)
    {
        auto* dx = static_cast<DX12Buffer*>(buffer);
        D3D12_VERTEX_BUFFER_VIEW vbv{};
        vbv.BufferLocation = dx->GPUAddress();
        vbv.SizeInBytes = static_cast<UINT>(dx->Size());
        vbv.StrideInBytes = strideBytes;
        m_cmd.IASetVertexBuffers(0, 1, &vbv);
    }

    void DX12CommandList::SetIndexBuffer(NSRHI::IBuffer* buffer, bool is32Bit)
    {
        auto* dx = static_cast<DX12Buffer*>(buffer);
        D3D12_INDEX_BUFFER_VIEW ibv{};
        ibv.BufferLocation = dx->GPUAddress();
        ibv.SizeInBytes = static_cast<UINT>(dx->Size());
        ibv.Format = is32Bit ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
        m_cmd.IASetIndexBuffer(&ibv);
    }

    void DX12CommandList::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex)
    {
        m_cmd.DrawInstanced(vertexCount, instanceCount, firstVertex, 0);
    }

    void DX12CommandList::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset)
    {
        m_cmd.DrawIndexedInstanced(indexCount, instanceCount, firstIndex, vertexOffset, 0);
    }

    void DX12CommandList::CopyBuffer(NSRHI::IBuffer* destination, NSRHI::IBuffer* source, size_t sizeBytes)
    {
        auto* dst = static_cast<DX12Buffer*>(destination);
        auto* src = static_cast<DX12Buffer*>(source);
        m_cmd.CopyBufferRegion(dst->Raw(), 0, src->Raw(), 0, sizeBytes);
    }

    void DX12CommandList::CopyBufferToTexture(NSRHI::ITexture*, NSRHI::IBuffer*)
    {
        // Reserved for later — needs the placed-footprint / UpdateSubresources
        // dance; lands with the texture-upload (model) work.
        ASSERT(false, "CopyBufferToTexture: not implemented yet");
    }
}
