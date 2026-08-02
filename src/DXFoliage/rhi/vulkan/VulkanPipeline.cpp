#include "stdafx.h"
#include "VulkanPipeline.h"

#include "VulkanFormat.h"
#include "VulkanPipelineLayout.h"
#include "VulkanShaderCompiler.h"

namespace NSRHIVulkan
{
    namespace
    {
        VkShaderModule CreateModule(VkDevice device, const CompiledShader& shader)
        {
            if (not shader.IsValid()) return VK_NULL_HANDLE;

            VkShaderModuleCreateInfo info{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
            info.codeSize = shader.spirv.size() * sizeof(uint32_t);
            info.pCode = shader.spirv.data();

            VkShaderModule module = VK_NULL_HANDLE;
            VK_CHECK(vkCreateShaderModule(device, &info, nullptr, &module));
            return module;
        }
    }

    VulkanPipeline::VulkanPipeline(VkDevice device, const NSRHI::GraphicsPipelineDesc& desc)
        : m_device(device)
    {
        ASSERT(desc.layout != nullptr, "GraphicsPipelineDesc needs a layout");
        m_layout = static_cast<VulkanPipelineLayout*>(desc.layout)->Raw();

        // Both results are kept alive for the whole function: pName below
        // points straight into their entryPoint strings, and the pipeline
        // isn't created until the end.
        const CompiledShader vsShader = CompileHLSLToSPIRV(desc.vertexShader);
        const CompiledShader psShader = desc.pixelShader.IsValid()
            ? CompileHLSLToSPIRV(desc.pixelShader)
            : CompiledShader{};

        VkShaderModule vs = CreateModule(device, vsShader);
        VkShaderModule ps = CreateModule(device, psShader);

        std::vector<VkPipelineShaderStageCreateInfo> stages;

        VkPipelineShaderStageCreateInfo vsStage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        vsStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vsStage.module = vs;
        vsStage.pName = vsShader.entryPoint.c_str();
        stages.push_back(vsStage);

        if (ps != VK_NULL_HANDLE)
        {
            VkPipelineShaderStageCreateInfo psStage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
            psStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            psStage.module = ps;
            psStage.pName = psShader.entryPoint.c_str();
            stages.push_back(psStage);
        }

        // D3D12 matches vertex inputs by HLSL semantic name; Vulkan matches
        // them by numeric location. DXC assigns locations to an HLSL
        // entry point's inputs in declaration order, which is the same
        // order the neutral vertexAttributes list is written in, so the
        // index here IS the location. (Verified against the compiled
        // SPIR-V: POSITION -> 0, COLOR -> 1 - and identically under the
        // previous glslang frontend, so this survived the compiler swap.)
        std::vector<VkVertexInputAttributeDescription> attributes;
        attributes.reserve(desc.vertexAttributes.size());
        for (size_t i = 0; i < desc.vertexAttributes.size(); ++i)
        {
            const NSRHI::VertexAttribute& attr = desc.vertexAttributes[i];
            attributes.push_back(VkVertexInputAttributeDescription{
                .location = static_cast<uint32_t>(i),
                .binding = 0,
                .format = ToVkFormat(attr.format),
                .offset = attr.offsetBytes,
            });
        }

        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = desc.vertexStrideBytes;
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkPipelineVertexInputStateCreateInfo vertexInput{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        if (not attributes.empty())
        {
            vertexInput.vertexBindingDescriptionCount = 1;
            vertexInput.pVertexBindingDescriptions = &binding;
            vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
            vertexInput.pVertexAttributeDescriptions = attributes.data();
        }

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        inputAssembly.topology = (desc.topology == NSRHI::EPrimitiveTopology::LineList)
            ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST
            : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // Counts must be set even though the values come from
        // vkCmdSetViewport/vkCmdSetScissor at record time.
        VkPipelineViewportStateCreateInfo viewportState{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo raster{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_BACK_BIT;
        // D3D12's default rasterizer treats clockwise as front-facing;
        // matching it here means the same mesh data draws identically on
        // both backends without per-backend winding fixups.
        raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
        raster.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisample{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
        depthStencil.depthTestEnable = desc.depthTestEnabled;
        depthStencil.depthWriteEnable = desc.depthWriteEnabled;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

        // Applied to every colour attachment. DXTerrain never exceeds one
        // (NumRenderTargets is 0 or 1 at all 27 sites), so per-attachment
        // blend modes would be untestable as well as unused.
        const bool blend = (desc.blendMode == NSRHI::EBlendMode::AlphaBlend);
        std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(
            desc.colorTargetFormats.size(),
            VkPipelineColorBlendAttachmentState{
                .blendEnable = blend ? VK_TRUE : VK_FALSE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            });

        VkPipelineColorBlendStateCreateInfo colorBlend{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        colorBlend.attachmentCount = static_cast<uint32_t>(blendAttachments.size());
        colorBlend.pAttachments = blendAttachments.data();

        const VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamic{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        dynamic.dynamicStateCount = 2;
        dynamic.pDynamicStates = dynamicStates;

        std::vector<VkFormat> colorFormats;
        colorFormats.reserve(desc.colorTargetFormats.size());
        for (const NSRHI::EFormat format : desc.colorTargetFormats)
        {
            colorFormats.push_back(ToVkFormat(format));
        }

        // The dynamic-rendering replacement for a VkRenderPass: instead of
        // pointing at a render-pass object, the pipeline declares the
        // attachment formats it is compatible with.
        VkPipelineRenderingCreateInfo rendering{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
        rendering.colorAttachmentCount = static_cast<uint32_t>(colorFormats.size());
        rendering.pColorAttachmentFormats = colorFormats.data();
        rendering.depthAttachmentFormat = ToVkFormat(desc.depthTargetFormat);

        VkGraphicsPipelineCreateInfo info{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        info.pNext = &rendering;
        info.stageCount = static_cast<uint32_t>(stages.size());
        info.pStages = stages.data();
        info.pVertexInputState = &vertexInput;
        info.pInputAssemblyState = &inputAssembly;
        info.pViewportState = &viewportState;
        info.pRasterizationState = &raster;
        info.pMultisampleState = &multisample;
        info.pDepthStencilState = &depthStencil;
        info.pColorBlendState = &colorBlend;
        info.pDynamicState = &dynamic;
        info.layout = m_layout;
        info.renderPass = VK_NULL_HANDLE; // dynamic rendering

        VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &m_pipeline));

        // The modules are only needed while the pipeline is being built —
        // the driver has already consumed them by now.
        if (vs) vkDestroyShaderModule(device, vs, nullptr);
        if (ps) vkDestroyShaderModule(device, ps, nullptr);
    }

    VulkanPipeline::~VulkanPipeline()
    {
        if (m_pipeline) vkDestroyPipeline(m_device, m_pipeline, nullptr);
    }

    void VulkanPipeline::Bind(VkCommandBuffer cmd) const
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    }
}
