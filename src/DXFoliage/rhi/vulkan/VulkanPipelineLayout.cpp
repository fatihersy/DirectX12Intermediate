#include "stdafx.h"
#include "VulkanPipelineLayout.h"

namespace NSRHIVulkan
{
    VulkanPipelineLayout::VulkanPipelineLayout(VkDevice device, const NSRHI::PipelineLayoutDesc& desc,
                                               VkDescriptorSetLayout bindlessSetLayout)
        : m_device(device), m_usesBindlessSet(desc.usesBindlessDescriptorTable)
    {
        ASSERT(not desc.usesBindlessDescriptorTable or bindlessSetLayout != VK_NULL_HANDLE,
            "usesBindlessDescriptorTable was set but no descriptor set layout was supplied");

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

        // Set 0, matching the [[vk::binding(N, 0)]] attributes in the
        // shaders. One set is the whole bindless model: everything
        // sampleable lives in it and is reached by index.
        if (desc.usesBindlessDescriptorTable)
        {
            info.setLayoutCount = 1;
            info.pSetLayouts = &bindlessSetLayout;
        }

        VK_CHECK(vkCreatePipelineLayout(device, &info, nullptr, &m_layout));
    }

    VulkanPipelineLayout::~VulkanPipelineLayout()
    {
        if (m_layout) vkDestroyPipelineLayout(m_device, m_layout, nullptr);
    }
}
