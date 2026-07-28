#pragma once

#include "IBuffer.h"
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
    };
}
