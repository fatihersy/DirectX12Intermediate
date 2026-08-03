#include "stdafx.h"
#include "VulkanDescriptorHeap.h"

#include "VulkanDevice.h"

namespace NSRHIVulkan
{
    namespace
    {
        // Must match the [[vk::binding(N, 0)]] attributes in the shaders.
        // Attributes rather than DXC's -fvk-*-shift flags so one HLSL file
        // serves both backends: DX12 ignores the vk:: namespace entirely,
        // exactly as it already does for [[vk::push_constant]] in vert.hlsl.
        constexpr uint32_t kBindingResources = 0;
        constexpr uint32_t kBindingSampler = 1;
    }

    VulkanDescriptorHeap::VulkanDescriptorHeap(VulkanDevice& device, const NSRHI::DescriptorHeapDesc& desc)
        : m_device(device.Device())
    {
        ASSERT(desc.capacity > 0, "Descriptor heap needs a non-zero capacity");

        im_capacity = desc.capacity;
        im_type = desc.type;
        im_heapId = NSRHI::NextDescriptorHeapId();

        // Clamp against what the driver allows in one set rather than
        // trusting the caller. The update-after-bind limit is the one that
        // applies, and it is a different (usually far larger) number than
        // the ordinary one — 1,048,576 vs 1,000,000 on this RTX 3060.
        VkPhysicalDeviceDescriptorIndexingProperties indexingProps{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES };
        VkPhysicalDeviceProperties2 props2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
        props2.pNext = &indexingProps;
        vkGetPhysicalDeviceProperties2(device.PhysicalDevice(), &props2);

        const uint32_t driverMax = indexingProps.maxDescriptorSetUpdateAfterBindSampledImages;
        if (driverMax > 0 and im_capacity > driverMax)
        {
            g_FWarn("Vulkan: descriptor heap capacity %u exceeds device limit %u; clamping",
                im_capacity, driverMax);
            im_capacity = driverMax;
        }

        // Linear/repeat: what a colour texture wants by default. Point
        // sampling and clamped addressing become a second immutable
        // sampler at another binding when something needs them (DXTerrain
        // uses exactly two).
        VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        VK_CHECK(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_sampler));

        VkDescriptorSetLayoutBinding bindings[2]{};
        bindings[0].binding = kBindingResources;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        bindings[0].descriptorCount = im_capacity;
        // ALL_GRAPHICS, not just FRAGMENT: DXTerrain samples its heightmap
        // in the domain shader, and displacement sampling outside the pixel
        // stage is normal. Restricting it would be a limit found much later
        // and for no gain.
        bindings[0].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;

        bindings[1].binding = kBindingSampler;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
        bindings[1].pImmutableSamplers = &m_sampler;

        // PARTIALLY_BOUND is what makes a mostly-empty heap legal: without
        // it every one of the capacity slots would need a valid descriptor
        // before the set could be bound at all.
        //
        // UPDATE_AFTER_BIND allows writing a slot while the set is bound
        // and other slots are in flight. It does NOT allow overwriting a
        // slot that pending work will read — that is what the front-end's
        // ring, combined with the backend's frame fence, is for.
        const VkDescriptorBindingFlags resourceFlags =
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
            | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
        const VkDescriptorBindingFlags flags[2]{ resourceFlags, 0 };

        VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
        flagsInfo.bindingCount = 2;
        flagsInfo.pBindingFlags = flags;

        VkDescriptorSetLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        layoutInfo.pNext = &flagsInfo;
        layoutInfo.bindingCount = 2;
        layoutInfo.pBindings = bindings;
        layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        VK_CHECK(vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_setLayout));

        VkDescriptorPoolSize poolSizes[2]{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        poolSizes[0].descriptorCount = im_capacity;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLER;
        poolSizes[1].descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 2;
        poolInfo.pPoolSizes = poolSizes;
        VK_CHECK(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_pool));

        VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        allocInfo.descriptorPool = m_pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_setLayout;
        VK_CHECK(vkAllocateDescriptorSets(m_device, &allocInfo, &m_set));

        g_FInfo("Vulkan: descriptor heap %u ready, %u slots", im_heapId, im_capacity);
    }

    VulkanDescriptorHeap::~VulkanDescriptorHeap()
    {
        Reset();
    }

    void VulkanDescriptorHeap::Reset()
    {
        // The set is freed with the pool; freeing it separately would need
        // FREE_DESCRIPTOR_SET_BIT, which this pool deliberately lacks.
        if (m_pool) vkDestroyDescriptorPool(m_device, m_pool, nullptr);
        if (m_setLayout) vkDestroyDescriptorSetLayout(m_device, m_setLayout, nullptr);
        if (m_sampler) vkDestroySampler(m_device, m_sampler, nullptr);

        m_pool = VK_NULL_HANDLE;
        m_setLayout = VK_NULL_HANDLE;
        m_set = VK_NULL_HANDLE;
        m_sampler = VK_NULL_HANDLE;
        im_capacity = 0;
    }
}
