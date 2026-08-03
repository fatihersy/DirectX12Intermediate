#pragma once

#include "VulkanCommon.h"
#include "rhi/IPipeline.h"

namespace NSRHIVulkan
{
    // Vulkan implementation of IPipeline — the counterpart of DX12Pipeline.
    // Compiles its shaders from source (see VulkanShaderCompiler) and bakes
    // the fixed-function state into one VkPipeline.
    //
    // Vulkan bakes far more into the pipeline than D3D12 does: the vertex
    // input layout, the topology, AND the attachment formats are all fixed
    // at creation. That's why GraphicsPipelineDesc carries
    // colorTargetFormats even though dynamic rendering has no render-pass
    // object — the driver still needs to know what it will be rendering
    // into. Viewport and scissor are the exception: both are declared
    // dynamic here, so resizing doesn't force a pipeline rebuild.
    class VulkanPipeline final : public NSRHI::IPipeline
    {
    public:
        VulkanPipeline(VkDevice device, const NSRHI::GraphicsPipelineDesc& desc);
        ~VulkanPipeline() override;

        VulkanPipeline(const VulkanPipeline&) = delete;
        VulkanPipeline& operator=(const VulkanPipeline&) = delete;

        void Bind(VkCommandBuffer cmd) const;

        VkPipelineLayout Layout() const { return m_layout; }

        // Copied from the pipeline layout at construction so the command
        // list can ask the pipeline it was just handed, without keeping a
        // pointer to the layout object alive alongside it.
        bool UsesBindlessSet() const { return m_usesBindlessSet; }

    private:
        VkDevice m_device{ VK_NULL_HANDLE };
        VkPipeline m_pipeline{ VK_NULL_HANDLE };
        VkPipelineLayout m_layout{ VK_NULL_HANDLE }; // non-owning; owned by desc.layout
        bool m_usesBindlessSet{ false };
    };
}
