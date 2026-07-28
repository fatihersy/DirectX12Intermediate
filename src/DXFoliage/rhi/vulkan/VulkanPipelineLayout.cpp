#include "stdafx.h"
#include "VulkanPipelineLayout.h"

namespace NSRHIVulkan
{
    VulkanPipelineLayout::VulkanPipelineLayout(VkDevice device, const NSRHI::PipelineLayoutDesc& desc)
        : m_device(device)
    {
        ASSERT(not desc.usesBindlessDescriptorTable,
            "Bindless descriptor sets aren't implemented on the Vulkan backend yet");

        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushRange.offset = 0;
        pushRange.size = desc.num32BitRootConstants * sizeof(uint32_t);

        VkPipelineLayoutCreateInfo info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        if (desc.num32BitRootConstants > 0)
        {
            info.pushConstantRangeCount = 1;
            info.pPushConstantRanges = &pushRange;
        }

        VK_CHECK(vkCreatePipelineLayout(device, &info, nullptr, &m_layout));
    }

    VulkanPipelineLayout::~VulkanPipelineLayout()
    {
        if (m_layout) vkDestroyPipelineLayout(m_device, m_layout, nullptr);
    }
}
