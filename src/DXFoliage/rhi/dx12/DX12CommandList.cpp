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
        auto* dx12Pipeline = static_cast<DX12Pipeline*>(pipeline);
        dx12Pipeline->Bind(m_cmd);

        // Root state does not survive a root-signature change, so the
        // front-end must re-issue SetConstantBuffer after SetPipeline —
        // which it does anyway, per draw. These just route the calls.
        m_constantBaseVA = dx12Pipeline->ConstantBaseVA();
        m_constantRootParamBase = dx12Pipeline->ConstantRootParamBase();
        m_numConstantSlots = dx12Pipeline->NumConstantSlots();
    }

    void DX12CommandList::SetConstantBuffer(uint32_t slot, uint64_t offsetBytes)
    {
        ASSERT(slot < m_numConstantSlots,
            "SetConstantBuffer slot not declared by the bound pipeline's layout");
        ASSERT((offsetBytes % NSRHI::kConstantBufferAlignment) == 0,
            "Offset not from ConstantAllocator::Allocate");

        m_cmd.SetGraphicsRootConstantBufferView(
            m_constantRootParamBase + slot, m_constantBaseVA + offsetBytes);
    }

    void DX12CommandList::SetRootConstants(uint32_t offsetIn32BitValues, uint32_t num32BitValues, const void* data)
    {
        // Root parameter 0 is the 32-bit-constants slot (see
        // DX12PipelineLayout — constants come first when present).
        m_cmd.SetGraphicsRoot32BitConstants(0, num32BitValues, data, offsetIn32BitValues);
    }

    void DX12CommandList::SetDescriptorHeap(NSRHI::IDescriptorHeap*)
    {
        // NOT "deferred" any more — that comment was true when written and
        // is now false. The bindless model is decided and implemented:
        // VulkanCommandList binds its set here, DX12DescriptorHeap exists
        // with Raw() and GpuHandle(), and the front-end calls this EVERY
        // FRAME from Renderer::DrawScene. So this assert is reached
        // immediately on DX12, not eventually.
        //
        // What is actually missing is two calls:
        //   1. m_cmd.SetDescriptorHeaps(1, &heap->Raw()) — right here.
        //   2. SetGraphicsRootDescriptorTable(rootParamIndex,
        //      heap->GpuHandle(0)) — which needs the root-parameter index
        //      from the bound pipeline's layout, so it has to be DEFERRED
        //      to SetPipeline exactly as VulkanCommandList defers its
        //      vkCmdBindDescriptorSets for the same reason.
        // DX12PipelineLayout already computes that index (it lays root
        // params out as [constants?][bindless table?][CBV slots] and
        // exposes ConstantRootParamBase()); the table's own index is the
        // one before that base when usesBindlessDescriptorTable is set.
        ASSERT(false, "SetDescriptorHeap: DX12 bindless bind not written yet — see comment");
    }

    void DX12CommandList::SetVertexBuffer(NSRHI::IBuffer* buffer, uint32_t strideBytes,
                                          uint64_t offsetBytes, uint64_t sizeBytes)
    {
        auto* dx = static_cast<DX12Buffer*>(buffer);
        ASSERT(offsetBytes < dx->Size(), "Vertex buffer offset past the end");

        D3D12_VERTEX_BUFFER_VIEW vbv{};
        vbv.BufferLocation = dx->GPUAddress() + offsetBytes;
        // SizeInBytes must describe the range from the offset, not the
        // whole buffer — the view is (location, size) and D3D12 reads
        // vertexOffset relative to BufferLocation. Zero means "the rest".
        vbv.SizeInBytes = static_cast<UINT>(sizeBytes > 0 ? sizeBytes : dx->Size() - offsetBytes);
        vbv.StrideInBytes = strideBytes;
        m_cmd.IASetVertexBuffers(0, 1, &vbv);
    }

    void DX12CommandList::SetIndexBuffer(NSRHI::IBuffer* buffer, bool is32Bit,
                                         uint64_t offsetBytes, uint64_t sizeBytes)
    {
        auto* dx = static_cast<DX12Buffer*>(buffer);
        ASSERT(offsetBytes < dx->Size(), "Index buffer offset past the end");

        D3D12_INDEX_BUFFER_VIEW ibv{};
        ibv.BufferLocation = dx->GPUAddress() + offsetBytes;
        ibv.SizeInBytes = static_cast<UINT>(sizeBytes > 0 ? sizeBytes : dx->Size() - offsetBytes);
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

    void DX12CommandList::CopyBufferToTexture(NSRHI::ITexture* destination, NSRHI::IBuffer* source,
                                              const NSRHI::TextureRegion& region,
                                              uint32_t srcRowPitchBytes,
                                              uint64_t srcOffsetBytes)
    {
        auto* dst = static_cast<DX12Texture*>(destination);
        auto* src = static_cast<DX12Buffer*>(source);
        if (not dst or not src) return;

        ASSERT(region.width > 0 and region.height > 0, "Empty copy region");
        ASSERT(region.x + region.width <= dst->Width() and
               region.y + region.height <= dst->Height(),
            "Copy region extends past the texture");
        // The hard D3D12 rule, and the reason the pitch is an explicit
        // parameter rather than derived from the region. Vulkan has no
        // equivalent constraint — this asymmetry is the whole point of
        // IDevice::TextureRowPitchAlignment().
        ASSERT((srcRowPitchBytes % D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) == 0,
            "Row pitch must be 256-byte aligned; pack to IDevice::TextureRowPitchAlignment()");
        // A placed footprint's offset has its own, coarser alignment.
        ASSERT((srcOffsetBytes % D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT) == 0,
            "Placed-footprint offset must be 512-byte aligned on D3D12");

        // RowPitch is in BYTES here; Vulkan's bufferRowLength is in
        // TEXELS. The neutral parameter is bytes and each backend
        // converts (or does not) on its own side.
        D3D12_TEXTURE_COPY_LOCATION srcLoc{};
        srcLoc.pResource = src->Raw();
        srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLoc.PlacedFootprint.Offset = srcOffsetBytes;
        srcLoc.PlacedFootprint.Footprint.Format = ToDXGIFormat(dst->Format());
        srcLoc.PlacedFootprint.Footprint.Width = region.width;
        srcLoc.PlacedFootprint.Footprint.Height = region.height;
        srcLoc.PlacedFootprint.Footprint.Depth = 1;
        srcLoc.PlacedFootprint.Footprint.RowPitch = srcRowPitchBytes;

        D3D12_TEXTURE_COPY_LOCATION dstLoc{};
        dstLoc.pResource = dst->Raw();
        dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex = 0;

        // The destination offset rides on the call rather than the
        // footprint — the same shape ImGui's own DX12 backend uses.
        m_cmd.CopyTextureRegion(&dstLoc, region.x, region.y, 0, &srcLoc, nullptr);
    }
}
