#pragma once

#include "VulkanCommon.h"
#include "rhi/IDescriptorHeap.h"

#include <cstdint>

namespace NSRHIVulkan
{
    class VulkanDevice;

    // Vulkan storage for IDescriptorHeap: ONE VkDescriptorSet holding a
    // large array of descriptors, indexed by the shader.
    //
    // Storage only. Allocation is the front-end's (NSDescriptor::StaticHeap
    // / RingHeap), and writes are VulkanDevice's (vkUpdateDescriptorSets
    // takes a VkDevice, not a set). What is left here is the set, the
    // layout that describes it, and the identity/addressing the base class
    // provides.
    //
    // TWO BINDINGS, not one combined image+sampler, because that is what
    // the HLSL compiles to. DXC lowers
    //   Texture2D    g_textures[] -> VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
    //   SamplerState g_sampler    -> VK_DESCRIPTOR_TYPE_SAMPLER
    // as separate descriptors, mirroring D3D12's separate SRV and sampler
    // heaps. Combining them would not match the shader, and that fails at
    // draw time rather than at creation.
    //
    // The sampler is IMMUTABLE (baked into the set layout) — the direct
    // equivalent of the static samplers in every one of DXTerrain's root
    // signatures.
    class VulkanDescriptorHeap final : public NSRHI::IDescriptorHeap
    {
    public:
        VulkanDescriptorHeap(VulkanDevice& device, const NSRHI::DescriptorHeapDesc& desc);
        ~VulkanDescriptorHeap() override;

        VulkanDescriptorHeap(const VulkanDescriptorHeap&) = delete;
        VulkanDescriptorHeap& operator=(const VulkanDescriptorHeap&) = delete;

        void Reset() override;

        // Backend-internal. The pipeline layout needs the set layout at
        // creation, the command list needs the set at bind time, and the
        // device needs both to write descriptors into it.
        VkDescriptorSetLayout Layout() const { return m_setLayout; }
        VkDescriptorSet Set() const { return m_set; }

    private:
        VkDevice m_device{ VK_NULL_HANDLE };
        VkDescriptorSetLayout m_setLayout{ VK_NULL_HANDLE };
        VkDescriptorPool m_pool{ VK_NULL_HANDLE };
        VkDescriptorSet m_set{ VK_NULL_HANDLE };
        VkSampler m_sampler{ VK_NULL_HANDLE };
    };
}
