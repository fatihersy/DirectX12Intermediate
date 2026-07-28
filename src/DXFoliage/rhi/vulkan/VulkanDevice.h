#pragma once

#include "VulkanCommon.h"
#include "rhi/IDevice.h"

#include <cstdint>

struct wl_display;
struct wl_surface;

// Vulkan implementation of the exposed IDevice (the resource factory), and
// the internal holder of the VkInstance / VkPhysicalDevice / VkDevice /
// queue. Mirrors DX12Device: bootstrapping happens in the constructor,
// and the renderer backend owns one and exposes it via GetDevice().
//
// The VkInstance is roughly DXGI's factory, VkPhysicalDevice its adapter,
// and VkDevice the ID3D12Device — DXGI just bundles the first two
// differently.
namespace NSRHIVulkan
{
    class VulkanDevice final : public NSRHI::IDevice
    {
    public:
        // Creating the surface needs the window, and picking a queue family
        // needs the surface (we require one that can actually present), so
        // both happen up front rather than later.
        VulkanDevice(wl_display* display, wl_surface* surface);
        ~VulkanDevice() override;

        VulkanDevice(const VulkanDevice&) = delete;
        VulkanDevice& operator=(const VulkanDevice&) = delete;

        bool IsValid() const { return m_device != VK_NULL_HANDLE; }

        // --- IDevice (exposed factory) ---
        std::unique_ptr<NSRHI::IBuffer> CreateBuffer(const NSRHI::BufferDesc& desc) override;
        std::unique_ptr<NSRHI::ITexture> CreateTexture(const NSRHI::TextureDesc& desc) override;
        std::unique_ptr<NSRHI::IPipelineLayout> CreatePipelineLayout(const NSRHI::PipelineLayoutDesc& desc) override;
        std::unique_ptr<NSRHI::IPipeline> CreateGraphicsPipeline(const NSRHI::GraphicsPipelineDesc& desc) override;

        // --- Internal accessors (backend-only, not exposed) ---
        VkInstance Instance() const { return m_instance; }
        VkPhysicalDevice PhysicalDevice() const { return m_physicalDevice; }
        VkDevice Device() const { return m_device; }
        VkQueue Queue() const { return m_queue; }
        uint32_t QueueFamilyIndex() const { return m_queueFamily; }
        VkSurfaceKHR Surface() const { return m_surface; }

        // Picks a memory type satisfying both the resource's requirements
        // and the desired properties — Vulkan has no equivalent of D3D12's
        // fixed UPLOAD/DEFAULT heap types, so callers must resolve this
        // per allocation.
        uint32_t FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) const;

    private:
        bool CreateInstance();
        bool CreateSurface(wl_display* display, wl_surface* surface);
        bool PickPhysicalDevice();
        bool CreateLogicalDevice();

        VkInstance m_instance{ VK_NULL_HANDLE };
        VkDebugUtilsMessengerEXT m_debugMessenger{ VK_NULL_HANDLE };
        VkSurfaceKHR m_surface{ VK_NULL_HANDLE };
        VkPhysicalDevice m_physicalDevice{ VK_NULL_HANDLE };
        VkDevice m_device{ VK_NULL_HANDLE };
        VkQueue m_queue{ VK_NULL_HANDLE };
        uint32_t m_queueFamily{ UINT32_MAX };
        bool m_validationEnabled{ false };
    };
}
