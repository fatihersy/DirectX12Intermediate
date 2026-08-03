#pragma once

#include "VulkanCommon.h"
#include "rhi/IPipelineLayout.h"

namespace NSRHIVulkan
{
    // Vulkan implementation of IPipelineLayout — the counterpart of
    // DX12PipelineLayout's root signature. The neutral description maps
    // almost directly:
    //   num32BitRootConstants     -> one VkPushConstantRange
    //   usesBindlessDescriptorTable -> set 0 = the device's global
    //                                  bindless descriptor set layout
    //
    // A layout with neither is the zero-ranges/zero-sets case of the same
    // code rather than a special path.
    class VulkanPipelineLayout final : public NSRHI::IPipelineLayout
    {
    public:
        // bindlessSetLayout is the device's global heap layout, passed in
        // rather than looked up so this class stays ignorant of where
        // descriptors live. VK_NULL_HANDLE when the desc doesn't ask for
        // bindless.
        VulkanPipelineLayout(VkDevice device, const NSRHI::PipelineLayoutDesc& desc,
                             VkDescriptorSetLayout bindlessSetLayout);
        ~VulkanPipelineLayout() override;

        VulkanPipelineLayout(const VulkanPipelineLayout&) = delete;
        VulkanPipelineLayout& operator=(const VulkanPipelineLayout&) = delete;

        // Not part of IPipelineLayout — VulkanPipeline needs the raw layout
        // to build its VkPipeline, and the command list needs it to push
        // constants.
        VkPipelineLayout Raw() const { return m_layout; }

        // The command list must NOT bind the bindless set to a layout that
        // was built without it — vkCmdBindDescriptorSets against a layout
        // with zero sets is a validation error, and the demo cube's layout
        // is exactly that case.
        bool UsesBindlessSet() const { return m_usesBindlessSet; }

    private:
        VkDevice m_device{ VK_NULL_HANDLE };
        VkPipelineLayout m_layout{ VK_NULL_HANDLE };
        bool m_usesBindlessSet{ false };
    };
}
