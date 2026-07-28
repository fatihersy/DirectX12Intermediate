#include "stdafx.h"
#include "VulkanBuffer.h"
#include "VulkanDevice.h"

namespace NSRHIVulkan
{
    namespace
    {
        VkBufferUsageFlags ToUsage(NSRHI::EBufferUsage usage)
        {
            switch (usage)
            {
                case NSRHI::EBufferUsage::Vertex:   return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                case NSRHI::EBufferUsage::Index:    return VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                case NSRHI::EBufferUsage::Constant: return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
                case NSRHI::EBufferUsage::Upload:   return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
                default:                            return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            }
        }
    }

    VulkanBuffer::VulkanBuffer(VulkanDevice& device, const NSRHI::BufferDesc& desc)
        : m_device(device), m_size(desc.sizeBytes), m_cpuVisible(desc.cpuVisible)
    {
        VkBufferCreateInfo info{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        info.size = desc.sizeBytes;
        info.usage = ToUsage(desc.usage);
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VK_CHECK(vkCreateBuffer(device.Device(), &info, nullptr, &m_buffer));

        VkMemoryRequirements reqs{};
        vkGetBufferMemoryRequirements(device.Device(), m_buffer, &reqs);

        // HOST_VISIBLE|HOST_COHERENT is the equivalent of D3D12's UPLOAD
        // heap: mappable, and writes are visible to the GPU without an
        // explicit flush. DEVICE_LOCAL matches the DEFAULT heap.
        const VkMemoryPropertyFlags props = desc.cpuVisible
            ? (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
            : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        VkMemoryAllocateInfo alloc{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        alloc.allocationSize = reqs.size;
        alloc.memoryTypeIndex = device.FindMemoryType(reqs.memoryTypeBits, props);
        VK_CHECK(vkAllocateMemory(device.Device(), &alloc, nullptr, &m_memory));

        VK_CHECK(vkBindBufferMemory(device.Device(), m_buffer, m_memory, 0));
    }

    VulkanBuffer::~VulkanBuffer()
    {
        if (m_buffer) vkDestroyBuffer(m_device.Device(), m_buffer, nullptr);
        if (m_memory) vkFreeMemory(m_device.Device(), m_memory, nullptr);
    }

    void* VulkanBuffer::Map()
    {
        ASSERT(m_cpuVisible, "Buffer isn't CPU-visible");

        void* data = nullptr;
        VK_CHECK(vkMapMemory(m_device.Device(), m_memory, 0, m_size, 0, &data));
        return data;
    }

    void VulkanBuffer::Unmap()
    {
        vkUnmapMemory(m_device.Device(), m_memory);
    }
}
