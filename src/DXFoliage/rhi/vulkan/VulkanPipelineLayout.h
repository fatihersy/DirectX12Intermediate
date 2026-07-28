#pragma once

#include "VulkanCommon.h"
#include "rhi/IPipelineLayout.h"

namespace NSRHIVulkan
{
    // Vulkan implementation of IPipelineLayout — the counterpart of
    // DX12PipelineLayout's root signature. The neutral description maps
    // almost directly:
    //   num32BitRootConstants     -> one VkPushConstantRange
    //   usesBindlessDescriptorTable -> one VkDescriptorSetLayout
    //                                  (deferred with the bindless design)
    //
    // Today's DXFoliage layout is empty on both counts, which is the
    // zero-ranges/zero-sets case of the same code rather than a special
    // path.
    class VulkanPipelineLayout final : public NSRHI::IPipelineLayout
    {
    public:
        VulkanPipelineLayout(VkDevice device, const NSRHI::PipelineLayoutDesc& desc);
        ~VulkanPipelineLayout() override;

        VulkanPipelineLayout(const VulkanPipelineLayout&) = delete;
        VulkanPipelineLayout& operator=(const VulkanPipelineLayout&) = delete;

        // Not part of IPipelineLayout — VulkanPipeline needs the raw layout
        // to build its VkPipeline, and the command list needs it to push
        // constants.
        VkPipelineLayout Raw() const { return m_layout; }

    private:
        VkDevice m_device{ VK_NULL_HANDLE };
        VkPipelineLayout m_layout{ VK_NULL_HANDLE };
    };
}
