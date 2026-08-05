#pragma once

#include "IBuffer.h"
#include "IDescriptorHeap.h"
#include "IPipeline.h"
#include "IPipelineLayout.h"
#include "ITexture.h"

#include <memory>

// Diligent-style exposed device: the resource factory, a first-class
// object the front-end talks to directly for maximum flexibility (unlike
// bgfx/Unreal, which hide the device inside the backend). Covers the
// functionality common to both DX12 and Vulkan devices. Owned by
// IRendererBackend and reached via backend->GetDevice(); the concrete
// impl (DX12Device / VulkanDevice) holds the native ID3D12Device /
// VkDevice.
//
// Command recording is deliberately NOT here — it stays backend-private
// for now (frame verbs live on IRendererBackend). Whether to also expose
// a Diligent-style command context, and how backend-specific bits
// (shader-compile target, bindless descriptors) are surfaced, are open
// "distinctions" being decided separately.
namespace NSRHI
{
    class IDevice
    {
    public:
        virtual ~IDevice() = default;

        virtual std::unique_ptr<IBuffer> CreateBuffer(const BufferDesc& desc) = 0;
        virtual std::unique_ptr<ITexture> CreateTexture(const TextureDesc& desc) = 0;
        virtual std::unique_ptr<IPipelineLayout> CreatePipelineLayout(const PipelineLayoutDesc& desc) = 0;
        virtual std::unique_ptr<IPipeline> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) = 0;

        // Descriptor STORAGE. The front-end wraps the returned heap in
        // whatever allocation policy it wants (NSDescriptor::StaticHeap,
        // RingHeap) — those are index arithmetic and live above this line.
        virtual std::unique_ptr<IDescriptorHeap> CreateDescriptorHeap(const DescriptorHeapDesc& desc) = 0;

        // Descriptor WRITES live on the device, matching both APIs:
        // D3D12 is device->CreateShaderResourceView(res, &desc, cpuHandle)
        // and Vulkan is vkUpdateDescriptorSets(device, ...). The heap is
        // the destination, never the actor.
        //
        // The heap is passed explicitly even though D3D12 would not need it
        // (a CPU handle is an absolute address there). Vulkan does: an
        // index alone does not name a VkDescriptorSet. The alternative — a
        // device-side registry mapping heap ids back to heaps — buys
        // nothing and adds a lifetime hazard.
        //
        // `where` must come from that heap's At/OffsetOf; the mismatch is
        // caught by Validate rather than silently writing elsewhere.
        //
        // Whole-texture, default-format SRV only for now. Mip ranges, array
        // slices, cube views and UAVs all want a TextureViewDesc parameter
        // — DXTerrain builds TEXTURE2D/3D/CUBE SRVs and per-mip
        // TEXTURE2D/2DARRAY UAVs — but nothing here has a caller for them
        // yet, and guessing the shape without one is how the last three
        // versions of this interface went wrong.
        virtual void CreateShaderResourceView(IDescriptorHeap& heap, DescriptorOffset where,
                                              ITexture* texture) = 0;

        // Row-pitch alignment a texture upload must be packed to, in
        // bytes. 256 on D3D12 (D3D12_TEXTURE_DATA_PITCH_ALIGNMENT); 1 on
        // Vulkan, which accepts tight packing. Queried rather than
        // assumed so the front-end packs correctly for whichever backend
        // is live — the single place the two APIs genuinely disagree
        // about texture uploads.
        virtual uint32_t TextureRowPitchAlignment() const = 0;
    };
}
