#include "stdafx.h"
#include "VulkanDevice.h"

#include "VulkanBuffer.h"
#include "VulkanDescriptorHeap.h"
#include "VulkanPipeline.h"
#include "VulkanTexture.h"
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
            // Ordered most severe first: the severity parameter is a single
            // bit in practice, but testing high-to-low keeps this correct
            // even if that ever stops being true.
            if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)        g_FError("%s", data->pMessage);
            else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) g_FWarn("%s", data->pMessage);
            else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)    g_FInfo("%s", data->pMessage);
            else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) g_FTrace("%s", data->pMessage);
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
        if (not CreateLogicalDevice()) return;
        CreateAllocator();
    }

    VulkanDevice::~VulkanDevice()
    {
        // Before the device: every allocation it holds belongs to that
        // device, and vkDestroyDevice with live allocations is undefined.
        if (m_allocator) vmaDestroyAllocator(m_allocator);
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
            // All four severities. INFO and VERBOSE are noisy — the loader
            // and layer narrate object creation at those levels — but
            // subscribing to only WARNING/ERROR meant a clean log could not
            // be distinguished from a messenger that was never wired up.
            dbg.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
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

        // Descriptor indexing — the Vulkan half of the bindless model (see
        // rhi/IDescriptorHeap.h). Core in 1.2, but like dynamic rendering
        // every bit is off until asked for, and asking for the wrong subset
        // fails at descriptor-set-layout creation rather than here, which
        // is a confusing place to find out.
        //
        // Why each one:
        //   runtimeDescriptorArray        - lets the shader declare
        //                                   Texture2D g_textures[] with no
        //                                   size, which is the whole point
        //   ...PartiallyBound             - most slots in a big heap are
        //                                   empty; without this, every
        //                                   descriptor must be written
        //                                   before the set can be bound
        //   ...UpdateAfterBind            - textures get written into slots
        //                                   while earlier frames are still
        //                                   in flight using the same set
        //   ...NonUniformIndexing         - the index comes from a push
        //                                   constant, so it is uniform in
        //                                   practice today, but stops being
        //                                   so the moment a shader indexes
        //                                   per-pixel (material IDs)
        //   ...VariableDescriptorCount    - allocate the real capacity at
        //                                   run time instead of baking a
        //                                   maximum into the layout
        VkPhysicalDeviceVulkan12Features features12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
        features12.descriptorIndexing = VK_TRUE;
        features12.runtimeDescriptorArray = VK_TRUE;
        features12.descriptorBindingPartiallyBound = VK_TRUE;
        features12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
        features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        features12.descriptorBindingVariableDescriptorCount = VK_TRUE;
        features13.pNext = &features12;

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

    bool VulkanDevice::CreateAllocator()
    {
        VmaAllocatorCreateInfo info{};
        info.instance = m_instance;
        info.physicalDevice = m_physicalDevice;
        info.device = m_device;
        // Must agree with the instance's apiVersion; VMA gates optional
        // behaviour on it and assumes 1.0 otherwise.
        info.vulkanApiVersion = VK_API_VERSION_1_3;

        const VkResult result = vmaCreateAllocator(&info, &m_allocator);
        if (result != VK_SUCCESS)
        {
            g_FError("Vulkan: vmaCreateAllocator failed with %s", VkResultToString(result));
            return false;
        }
        return true;
    }

    std::unique_ptr<NSRHI::IBuffer> VulkanDevice::CreateBuffer(const NSRHI::BufferDesc& desc)
    {
        return std::make_unique<VulkanBuffer>(*this, desc);
    }

    std::unique_ptr<NSRHI::ITexture> VulkanDevice::CreateTexture(const NSRHI::TextureDesc& desc)
    {
        return std::make_unique<VulkanTexture>(*this, desc);
    }

    std::unique_ptr<NSRHI::IPipelineLayout> VulkanDevice::CreatePipelineLayout(const NSRHI::PipelineLayoutDesc& desc)
    {
        // The set layout comes from whichever heap the caller named, so
        // this class has no opinion about where descriptors live.
        VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
        if (desc.usesBindlessDescriptorTable)
        {
            ASSERT(desc.bindlessHeap != nullptr,
                "usesBindlessDescriptorTable needs a bindlessHeap to take its set layout from");
            setLayout = static_cast<VulkanDescriptorHeap*>(desc.bindlessHeap)->Layout();
        }

        return std::make_unique<VulkanPipelineLayout>(m_device, desc, setLayout);
    }

    std::unique_ptr<NSRHI::IDescriptorHeap> VulkanDevice::CreateDescriptorHeap(const NSRHI::DescriptorHeapDesc& desc)
    {
        return std::make_unique<VulkanDescriptorHeap>(*this, desc);
    }

    void VulkanDevice::CreateShaderResourceView(NSRHI::IDescriptorHeap& heap,
                                                NSRHI::DescriptorOffset where,
                                                NSRHI::ITexture* texture)
    {
        // Validate is the neutral replacement for IDescriptor's pointer
        // comparison: it catches an offset built by a different heap, which
        // would otherwise write a descriptor somewhere the caller never
        // named and fail much later at draw time.
        ASSERT(heap.Validate(where), "Descriptor offset does not belong to this heap");

        auto* vkTexture = static_cast<VulkanTexture*>(texture);
        if (not vkTexture) return;

        auto& vkHeap = static_cast<VulkanDescriptorHeap&>(heap);

        VkDescriptorImageInfo imageInfo{};
        // No sampler: this binding is SAMPLED_IMAGE and the sampler is
        // immutable at the other binding.
        imageInfo.imageView = vkTexture->View();
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        write.dstSet = vkHeap.Set();
        write.dstBinding = 0;  // kBindingResources
        write.dstArrayElement = where.index;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
    }

    std::unique_ptr<NSRHI::IPipeline> VulkanDevice::CreateGraphicsPipeline(const NSRHI::GraphicsPipelineDesc& desc)
    {
        return std::make_unique<VulkanPipeline>(m_device, desc);
    }
}
