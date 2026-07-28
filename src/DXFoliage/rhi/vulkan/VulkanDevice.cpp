#include "stdafx.h"
#include "VulkanDevice.h"

#include "VulkanBuffer.h"
#include "VulkanPipeline.h"
#include "VulkanPipelineLayout.h"

#include <cstring>
#include <vector>

namespace NSRHIVulkan
{
    const char* VkResultToString(VkResult result)
    {
        switch (result)
        {
            case VK_SUCCESS:                        return "VK_SUCCESS";
            case VK_NOT_READY:                      return "VK_NOT_READY";
            case VK_TIMEOUT:                        return "VK_TIMEOUT";
            case VK_SUBOPTIMAL_KHR:                 return "VK_SUBOPTIMAL_KHR";
            case VK_ERROR_OUT_OF_HOST_MEMORY:       return "VK_ERROR_OUT_OF_HOST_MEMORY";
            case VK_ERROR_OUT_OF_DEVICE_MEMORY:     return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
            case VK_ERROR_INITIALIZATION_FAILED:    return "VK_ERROR_INITIALIZATION_FAILED";
            case VK_ERROR_DEVICE_LOST:              return "VK_ERROR_DEVICE_LOST";
            case VK_ERROR_LAYER_NOT_PRESENT:        return "VK_ERROR_LAYER_NOT_PRESENT";
            case VK_ERROR_EXTENSION_NOT_PRESENT:    return "VK_ERROR_EXTENSION_NOT_PRESENT";
            case VK_ERROR_FEATURE_NOT_PRESENT:      return "VK_ERROR_FEATURE_NOT_PRESENT";
            case VK_ERROR_INCOMPATIBLE_DRIVER:      return "VK_ERROR_INCOMPATIBLE_DRIVER";
            case VK_ERROR_OUT_OF_DATE_KHR:          return "VK_ERROR_OUT_OF_DATE_KHR";
            case VK_ERROR_SURFACE_LOST_KHR:         return "VK_ERROR_SURFACE_LOST_KHR";
            default:                                return "VK_ERROR_<unmapped>";
        }
    }

    namespace
    {
        // Routes the validation layer's output into the engine log, the
        // same way DX12Device hooks up ID3D12InfoQueue1.
        VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT,
            const VkDebugUtilsMessengerCallbackDataEXT* data,
            void*)
        {
            if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)        g_FError("%s", data->pMessage);
            else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) g_FWarn("%s", data->pMessage);
            return VK_FALSE;
        }

        bool HasLayer(const char* name)
        {
            uint32_t count{};
            vkEnumerateInstanceLayerProperties(&count, nullptr);
            std::vector<VkLayerProperties> layers(count);
            vkEnumerateInstanceLayerProperties(&count, layers.data());

            for (const VkLayerProperties& l : layers)
            {
                if (std::strcmp(l.layerName, name) == 0) return true;
            }
            return false;
        }
    }

    VulkanDevice::VulkanDevice(wl_display* display, wl_surface* surface)
    {
        if (not CreateInstance()) return;
        if (not CreateSurface(display, surface)) return;
        if (not PickPhysicalDevice()) return;
        CreateLogicalDevice();
    }

    VulkanDevice::~VulkanDevice()
    {
        if (m_device) vkDestroyDevice(m_device, nullptr);
        if (m_surface) vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

        if (m_debugMessenger)
        {
            auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (destroy) destroy(m_instance, m_debugMessenger, nullptr);
        }

        if (m_instance) vkDestroyInstance(m_instance, nullptr);
    }

    bool VulkanDevice::CreateInstance()
    {
        VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
        appInfo.pApplicationName = "DXFoliage";
        appInfo.pEngineName = "DXFoliage";
        // 1.3 for dynamic rendering (vkCmdBeginRendering), which this
        // backend uses instead of VkRenderPass/VkFramebuffer objects.
        appInfo.apiVersion = VK_API_VERSION_1_3;

        std::vector<const char*> extensions{
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
        };

        std::vector<const char*> layers;
        m_validationEnabled = HasLayer("VK_LAYER_KHRONOS_validation");
        if (m_validationEnabled)
        {
            layers.push_back("VK_LAYER_KHRONOS_validation");
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        else
        {
            g_FWarn("Vulkan validation layer unavailable; continuing without it");
        }

        VkInstanceCreateInfo info{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
        info.pApplicationInfo = &appInfo;
        info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        info.ppEnabledExtensionNames = extensions.data();
        info.enabledLayerCount = static_cast<uint32_t>(layers.size());
        info.ppEnabledLayerNames = layers.data();

        const VkResult result = vkCreateInstance(&info, nullptr, &m_instance);
        if (result != VK_SUCCESS)
        {
            g_FError("Vulkan: vkCreateInstance failed with %s", VkResultToString(result));
            return false;
        }

        if (m_validationEnabled)
        {
            VkDebugUtilsMessengerCreateInfoEXT dbg{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
            dbg.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            dbg.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            dbg.pfnUserCallback = DebugCallback;

            auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
            if (create) create(m_instance, &dbg, nullptr, &m_debugMessenger);
        }

        return true;
    }

    bool VulkanDevice::CreateSurface(wl_display* display, wl_surface* surface)
    {
        VkWaylandSurfaceCreateInfoKHR info{ VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR };
        info.display = display;
        info.surface = surface;

        const VkResult result = vkCreateWaylandSurfaceKHR(m_instance, &info, nullptr, &m_surface);
        if (result != VK_SUCCESS)
        {
            g_FError("Vulkan: vkCreateWaylandSurfaceKHR failed with %s", VkResultToString(result));
            return false;
        }
        return true;
    }

    bool VulkanDevice::PickPhysicalDevice()
    {
        uint32_t count{};
        vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
        if (count == 0)
        {
            g_FError("Vulkan: no physical devices found");
            return false;
        }

        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

        // Prefer a discrete GPU, but accept anything that can present —
        // notably lavapipe (software), which is what makes this runnable
        // on machines without a suitable GPU.
        VkPhysicalDevice fallback{ VK_NULL_HANDLE };
        uint32_t fallbackFamily{ UINT32_MAX };

        for (VkPhysicalDevice candidate : devices)
        {
            uint32_t familyCount{};
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, nullptr);
            std::vector<VkQueueFamilyProperties> families(familyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, families.data());

            for (uint32_t i{}; i < familyCount; ++i)
            {
                if (not (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) continue;

                VkBool32 canPresent{ VK_FALSE };
                vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, m_surface, &canPresent);
                if (not canPresent) continue;

                VkPhysicalDeviceProperties props{};
                vkGetPhysicalDeviceProperties(candidate, &props);

                if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                {
                    m_physicalDevice = candidate;
                    m_queueFamily = i;
                    g_FDebug("Vulkan: using %s", props.deviceName);
                    return true;
                }

                if (fallback == VK_NULL_HANDLE)
                {
                    fallback = candidate;
                    fallbackFamily = i;
                }
                break;
            }
        }

        if (fallback == VK_NULL_HANDLE)
        {
            g_FError("Vulkan: no device with a graphics queue that can present");
            return false;
        }

        m_physicalDevice = fallback;
        m_queueFamily = fallbackFamily;

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
        g_FDebug("Vulkan: using %s", props.deviceName);

        return true;
    }

    bool VulkanDevice::CreateLogicalDevice()
    {
        const float priority = 1.0f;
        VkDeviceQueueCreateInfo queueInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        queueInfo.queueFamilyIndex = m_queueFamily;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority;

        const char* deviceExtensions[]{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

        // Dynamic rendering is core in 1.3 but still has to be switched on.
        VkPhysicalDeviceVulkan13Features features13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
        features13.dynamicRendering = VK_TRUE;
        features13.synchronization2 = VK_TRUE;

        VkPhysicalDeviceFeatures2 features2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
        features2.pNext = &features13;

        VkDeviceCreateInfo info{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        info.pNext = &features2;
        info.queueCreateInfoCount = 1;
        info.pQueueCreateInfos = &queueInfo;
        info.enabledExtensionCount = 1;
        info.ppEnabledExtensionNames = deviceExtensions;

        const VkResult result = vkCreateDevice(m_physicalDevice, &info, nullptr, &m_device);
        if (result != VK_SUCCESS)
        {
            g_FError("Vulkan: vkCreateDevice failed with %s", VkResultToString(result));
            return false;
        }

        vkGetDeviceQueue(m_device, m_queueFamily, 0, &m_queue);
        return true;
    }

    uint32_t VulkanDevice::FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) const
    {
        VkPhysicalDeviceMemoryProperties memProps{};
        vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);

        for (uint32_t i{}; i < memProps.memoryTypeCount; ++i)
        {
            const bool typeAllowed = (typeBits & (1u << i)) != 0;
            const bool hasProps = (memProps.memoryTypes[i].propertyFlags & properties) == properties;
            if (typeAllowed and hasProps) return i;
        }

        g_FError("Vulkan: no memory type matching the requested properties");
        return 0;
    }

    std::unique_ptr<NSRHI::IBuffer> VulkanDevice::CreateBuffer(const NSRHI::BufferDesc& desc)
    {
        return std::make_unique<VulkanBuffer>(*this, desc);
    }

    std::unique_ptr<NSRHI::ITexture> VulkanDevice::CreateTexture(const NSRHI::TextureDesc&)
    {
        // Reserved for later, matching the DX12 backend: VulkanTexture
        // currently only wraps swapchain images, which the swapchain
        // creates itself. Sampled textures land with the model work.
        ASSERT(false, "CreateTexture: sampled-texture creation not implemented yet");
        return nullptr;
    }

    std::unique_ptr<NSRHI::IPipelineLayout> VulkanDevice::CreatePipelineLayout(const NSRHI::PipelineLayoutDesc& desc)
    {
        return std::make_unique<VulkanPipelineLayout>(m_device, desc);
    }

    std::unique_ptr<NSRHI::IPipeline> VulkanDevice::CreateGraphicsPipeline(const NSRHI::GraphicsPipelineDesc& desc)
    {
        return std::make_unique<VulkanPipeline>(m_device, desc);
    }
}
