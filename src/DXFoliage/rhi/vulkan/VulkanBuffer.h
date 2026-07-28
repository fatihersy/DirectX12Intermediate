#pragma once

#include "VulkanCommon.h"
#include "rhi/IBuffer.h"

namespace NSRHIVulkan
{
    class VulkanDevice;

    // Vulkan implementation of IBuffer.
    //
    // Uses a plain vkAllocateMemory per buffer. That's fine for the handful
    // of buffers here, but it is NOT how a real renderer should allocate —
    // Vulkan drivers cap the number of allocations, so the texture/model
    // work should move this onto a suballocator (VMA).
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
        VkDeviceMemory m_memory{ VK_NULL_HANDLE };
        size_t m_size{};
        bool m_cpuVisible{};
    };
}
