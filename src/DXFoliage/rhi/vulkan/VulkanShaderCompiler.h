#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "rhi/IPipeline.h"

// HLSL -> SPIR-V for the Vulkan backend, the counterpart of
// rhi/dx12/ShaderCompiler.h (HLSL -> DXIL). Both use DXC, deliberately:
// one HLSL frontend on both platforms means a shader that builds on one
// builds on the other. The previous shaderc/glslang frontend accepted a
// different subset - it stops around SM6.0 and rejects 16-bit types and
// templates - so a Windows-authored shader could fail only on Linux.
//
// Compilation happens at pipeline-creation time rather than offline, which
// matches what the DX12 path already does and keeps shader authoring a
// single-step edit-and-run loop. An offline/cached path is a later
// optimisation, not a design change.
namespace NSRHIVulkan
{
    struct CompiledShader
    {
        std::vector<uint32_t> spirv;

        // The name of the entry point in the emitted SPIR-V. DXC KEEPS
        // the HLSL entry point name rather than renaming it to "main"
        // (verified: OpEntryPoint Vertex %mainVS "mainVS"), so
        // VkPipelineShaderStageCreateInfo::pName has to carry it through
        // instead of assuming "main". glslang behaved the same way, which
        // is why the compiler swap needed no pipeline changes.
        std::string entryPoint;

        bool IsValid() const { return not spirv.empty(); }
    };

    // Resolves `shaders/<filename>` next to the executable (falling back to
    // the working directory) and compiles it. Returns an invalid result on
    // failure, having logged what went wrong.
    CompiledShader CompileHLSLToSPIRV(const NSRHI::ShaderSource& source);
}
