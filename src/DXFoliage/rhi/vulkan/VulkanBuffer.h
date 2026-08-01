#pragma once

#include "VulkanCommon.h"
#include "VulkanMemory.h"
#include "rhi/IBuffer.h"

namespace NSRHIVulkan
{
    class VulkanDevice;

    // Vulkan implementation of IBuffer, allocated through VMA.
    //
    // CPU-visible buffers are created persistently mapped, so Map() is a
    // pointer read rather than a driver round-trip and Unmap() does
    // nothing. That is the normal arrangement for upload buffers: the
    // mapping costs nothing to keep, and mapping per frame does not.
    class VulkanBuffer final : public NSRHI::IBuffer
    {
    public:
        VulkanBuffer(VulkanDevice& device, const NSRHI::BufferDesc& desc);
        ~VulkanBuffer() override;

        VulkanBuffer(const VulkanBuffer&) = delete;
        VulkanBuffer& operator=(const VulkanBuffer&) = delete;

        size_t Size() const override { return m_size; }
        void* Map() override;
        void Unmap() override;

        VkBuffer Raw() const { return m_buffer; }

    private:
        VulkanDevice& m_device;
        VkBuffer m_buffer{ VK_NULL_HANDLE };
        VmaAllocation m_allocation{ VK_NULL_HANDLE };
        VmaAllocationInfo m_allocationInfo{};
        size_t m_size{};
        bool m_cpuVisible{};
    };
}
