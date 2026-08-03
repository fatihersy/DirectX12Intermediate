#pragma once

#include "VulkanCommon.h"
#include "rhi/IPipelineLayout.h"

namespace NSRHIVulkan
{
    // Vulkan implementation of IPipelineLayout — the counterpart of
    // DX12PipelineLayout's root signature. The neutral description maps
    // almost directly:
    //   num32BitRootConstants       -> one VkPushConstantRange
    //   usesBindlessDescriptorTable -> set 0 = the named heap's layout
    //   numConstantBufferSlots      -> set 1 = N UNIFORM_BUFFER_DYNAMIC
    //                                  bindings, one per slot
    //
    // SET 1 IS OWNED HERE, whole cloth: layout, a one-set pool, and the
    // set itself with the ConstantAllocator's buffer written into every
    // binding at a fixed 16 KB window (see kConstantBufferWindowBytes).
    // After creation nothing in it ever changes — per draw, only the
    // DYNAMIC OFFSETS move, supplied by VulkanCommandList at bind time.
    // That is the whole point of the dynamic-UBO mapping: D3D12's
    // "root CBV = base VA + offset, rebound per draw" becomes "same
    // descriptor, new offset", with zero descriptor writes per draw.
    //
    // Dynamic-UBO descriptors are BANNED from UPDATE_AFTER_BIND by the
    // spec, which is why this is its own set and not two more bindings on
    // the bindless heap's set. (Unreal splits the same way: its
    // BindlessUniformBufferSet is separate from BindlessSampledImageSet.)
    //
    // A layout with no constants and no bindless table is the zero-sets
    // case of the same code rather than a special path.
    class VulkanPipelineLayout final : public NSRHI::IPipelineLayout
    {
    public:
        // bindlessSetLayout is the named heap's layout, passed in rather
        // than looked up so this class stays ignorant of where
        // descriptors live. VK_NULL_HANDLE when the desc doesn't ask.
        VulkanPipelineLayout(VkDevice device, const NSRHI::PipelineLayoutDesc& desc,
                             VkDescriptorSetLayout bindlessSetLayout);
        ~VulkanPipelineLayout() override;

        VulkanPipelineLayout(const VulkanPipelineLayout&) = delete;
        VulkanPipelineLayout& operator=(const VulkanPipelineLayout&) = delete;

        // Not part of IPipelineLayout — VulkanPipeline needs the raw layout
        // to build its VkPipeline, and the command list needs it to push
        // constants.
        VkPipelineLayout Raw() const { return m_layout; }

        // The command list must NOT bind the bindless set to a layout that
        // was built without it — vkCmdBindDescriptorSets against a layout
        // with zero sets is a validation error, and the demo quad's layout
        // is exactly that case.
        bool UsesBindlessSet() const { return m_usesBindlessSet; }

        // Set 1, for the command list's lazy dynamic-offset bind at draw.
        // VK_NULL_HANDLE when the layout declares no constant slots.
        VkDescriptorSet ConstantSet() const { return m_constantSet; }
        uint32_t NumConstantSlots() const { return m_numConstantSlots; }

    private:
        VkDevice m_device{ VK_NULL_HANDLE };
        VkPipelineLayout m_layout{ VK_NULL_HANDLE };
        bool m_usesBindlessSet{ false };

        // Constant-slot machinery (set 1). The empty set-0 layout exists
        // only for the constants-without-bindless case: pSetLayouts is
        // positional and set 1 cannot be declared without SOMETHING at 0.
        VkDescriptorSetLayout m_constantSetLayout{ VK_NULL_HANDLE };
        VkDescriptorSetLayout m_emptySet0Layout{ VK_NULL_HANDLE };
        VkDescriptorPool m_constantPool{ VK_NULL_HANDLE };
        VkDescriptorSet m_constantSet{ VK_NULL_HANDLE };
        uint32_t m_numConstantSlots{ 0 };
    };
}
