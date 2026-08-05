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
        VkBufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size = desc.sizeBytes;
        info.usage = ToUsage(desc.usage);
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        // Describe the INTENT and let VMA choose the memory type, rather
        // than naming heap properties and searching for a match by hand.
        // AUTO inspects the usage flags above; HOST_ACCESS_SEQUENTIAL_WRITE
        // additionally says "the CPU will write this, front to back, and
        // never read it" - which is what an upload buffer does, and lets
        // VMA prefer write-combined memory where that exists.
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        if (desc.cpuVisible)
        {
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                            | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }

        // One call replaces vkCreateBuffer + vkGetBufferMemoryRequirements
        // + memory-type search + vkAllocateMemory + vkBindBufferMemory.
        VK_CHECK(vmaCreateBuffer(device.Allocator(), &info, &allocInfo,
            &m_buffer, &m_allocation, &m_allocationInfo));
    }

    VulkanBuffer::~VulkanBuffer()
    {
        // Frees the suballocation and destroys the buffer together; there
        // is no separate vkFreeMemory to forget.
        if (m_buffer) vmaDestroyBuffer(m_device.Allocator(), m_buffer, m_allocation);
    }

    void* VulkanBuffer::Map()
    {
        ASSERT(m_cpuVisible, "Buffer isn't CPU-visible");

        // MAPPED_BIT above means VMA already holds a persistent mapping,
        // so this is a pointer read rather than a driver call. Mapping and
        // unmapping per frame was never free.
        return m_allocationInfo.pMappedData;
    }

    void VulkanBuffer::Unmap()
    {
        // Nothing to do: the mapping is persistent for the buffer's
        // lifetime. Kept so callers kept the symmetric pattern and so the
        // DX12 backend, which does need it, can stay identical.
    }
}
