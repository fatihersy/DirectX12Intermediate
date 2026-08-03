#include "stdafx.h"
#include "VulkanPipelineLayout.h"

#include "VulkanBuffer.h"
#include "rhi/RHITypes.h"

namespace NSRHIVulkan
{
    VulkanPipelineLayout::VulkanPipelineLayout(VkDevice device, const NSRHI::PipelineLayoutDesc& desc,
                                               VkDescriptorSetLayout bindlessSetLayout)
        : m_device(device)
        , m_usesBindlessSet(desc.usesBindlessDescriptorTable)
        , m_numConstantSlots(desc.numConstantBufferSlots)
    {
        ASSERT(not desc.usesBindlessDescriptorTable or bindlessSetLayout != VK_NULL_HANDLE,
            "usesBindlessDescriptorTable was set but no descriptor set layout was supplied");
        ASSERT(desc.numConstantBufferSlots <= NSRHI::kMaxConstantBufferSlots,
            "More constant slots than kMaxConstantBufferSlots");
        ASSERT(desc.numConstantBufferSlots == 0 or desc.constantBuffer != nullptr,
            "numConstantBufferSlots without a constantBuffer to point them at");

        // --- Set 1: one dynamic-UBO binding per constant slot ---
        if (m_numConstantSlots > 0)
        {
            VkDescriptorSetLayoutBinding bindings[NSRHI::kMaxConstantBufferSlots]{};
            for (uint32_t slot = 0; slot < m_numConstantSlots; ++slot)
            {
                bindings[slot].binding = slot;  // [[vk::binding(slot, 1)]]
                bindings[slot].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                bindings[slot].descriptorCount = 1;
                // ALL_GRAPHICS to match the heap's bindings: DXTerrain
                // reads frame constants in hull/domain shaders too.
                bindings[slot].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
            }

            VkDescriptorSetLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
            layoutInfo.bindingCount = m_numConstantSlots;
            layoutInfo.pBindings = bindings;
            VK_CHECK(vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_constantSetLayout));

            VkDescriptorPoolSize poolSize{};
            poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            poolSize.descriptorCount = m_numConstantSlots;

            VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
            poolInfo.maxSets = 1;
            poolInfo.poolSizeCount = 1;
            poolInfo.pPoolSizes = &poolSize;
            VK_CHECK(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_constantPool));

            VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
            allocInfo.descriptorPool = m_constantPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &m_constantSetLayout;
            VK_CHECK(vkAllocateDescriptorSets(m_device, &allocInfo, &m_constantSet));

            // Write the allocator's buffer into every slot ONCE, at the
            // fixed window. From here on only dynamic offsets change —
            // never this set.
            auto* vkBuffer = static_cast<VulkanBuffer*>(desc.constantBuffer);

            VkDescriptorBufferInfo bufferInfos[NSRHI::kMaxConstantBufferSlots]{};
            VkWriteDescriptorSet writes[NSRHI::kMaxConstantBufferSlots]{};
            for (uint32_t slot = 0; slot < m_numConstantSlots; ++slot)
            {
                bufferInfos[slot].buffer = vkBuffer->Raw();
                bufferInfos[slot].offset = 0;  // the dynamic offset supplies the rest
                bufferInfos[slot].range = NSRHI::kConstantBufferWindowBytes;

                writes[slot] = VkWriteDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
                writes[slot].dstSet = m_constantSet;
                writes[slot].dstBinding = slot;
                writes[slot].descriptorCount = 1;
                writes[slot].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                writes[slot].pBufferInfo = &bufferInfos[slot];
            }
            vkUpdateDescriptorSets(m_device, m_numConstantSlots, writes, 0, nullptr);
        }

        // --- The pipeline layout: [set 0 = bindless][set 1 = constants] ---
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushRange.offset = 0;
        pushRange.size = desc.num32BitRootConstants * sizeof(uint32_t);

        // pSetLayouts is positional: declaring set 1 requires SOMETHING
        // at index 0, so a constants-only layout gets an empty set-0
        // placeholder rather than silently renumbering the sets out from
        // under the shaders' [[vk::binding(N, 1)]] attributes.
        VkDescriptorSetLayout sets[2]{ bindlessSetLayout, m_constantSetLayout };
        uint32_t setCount = 0;
        if (m_numConstantSlots > 0)
        {
            if (bindlessSetLayout == VK_NULL_HANDLE)
            {
                VkDescriptorSetLayoutCreateInfo emptyInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
                VK_CHECK(vkCreateDescriptorSetLayout(m_device, &emptyInfo, nullptr, &m_emptySet0Layout));
                sets[0] = m_emptySet0Layout;
            }
            setCount = 2;
        }
        else if (desc.usesBindlessDescriptorTable)
        {
            setCount = 1;
        }

        VkPipelineLayoutCreateInfo info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        if (desc.num32BitRootConstants > 0)
        {
            info.pushConstantRangeCount = 1;
            info.pPushConstantRanges = &pushRange;
        }
        info.setLayoutCount = setCount;
        info.pSetLayouts = setCount > 0 ? sets : nullptr;

        VK_CHECK(vkCreatePipelineLayout(device, &info, nullptr, &m_layout));
    }

    VulkanPipelineLayout::~VulkanPipelineLayout()
    {
        if (m_layout) vkDestroyPipelineLayout(m_device, m_layout, nullptr);
        // The set is freed with its pool.
        if (m_constantPool) vkDestroyDescriptorPool(m_device, m_constantPool, nullptr);
        if (m_constantSetLayout) vkDestroyDescriptorSetLayout(m_device, m_constantSetLayout, nullptr);
        if (m_emptySet0Layout) vkDestroyDescriptorSetLayout(m_device, m_emptySet0Layout, nullptr);
    }
}
