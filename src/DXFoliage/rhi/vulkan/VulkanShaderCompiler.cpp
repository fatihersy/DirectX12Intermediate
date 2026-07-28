#include "stdafx.h"
#include "VulkanShaderCompiler.h"

#include "core/Defines.h"
#include "platform/PlatformUtils.h"
#include "Logger.h"

#include <shaderc/shaderc.hpp>

namespace NSRHIVulkan
{
    namespace
    {
        // ShaderSource carries wide strings because the DXC path takes
        // LPCWSTR. shaderc is UTF-8, and these are ASCII filenames and
        // entry points, so a narrowing copy is enough.
        std::string Narrow(std::wstring_view wide)
        {
            std::string result;
            result.reserve(wide.size());
            for (const wchar_t c : wide)
            {
                result.push_back(static_cast<char>(c));
            }
            return result;
        }

        // The shaders are copied next to the binary at build time, but a
        // run from the repo's app/ directory should work too — check both
        // rather than making the launch directory load-bearing.
        std::filesystem::path ResolveShaderPath(const std::string& filename)
        {
            const std::filesystem::path relative =
                std::filesystem::path(Narrow(SHADERS_FOLDER)) / filename;

            const std::filesystem::path nextToBinary =
                NSPlatform::GetExecutableDirectory() / relative;
            if (std::filesystem::exists(nextToBinary)) return nextToBinary;

            return relative;
        }

        shaderc_shader_kind ToShaderKind(NSRHI::EShaderStage stage)
        {
            return (stage == NSRHI::EShaderStage::Pixel)
                ? shaderc_fragment_shader
                : shaderc_vertex_shader;
        }
    }

    CompiledShader CompileHLSLToSPIRV(const NSRHI::ShaderSource& source)
    {
        ASSERT(source.IsValid(), "Compiling an empty ShaderSource");

        const std::string filename = Narrow(source.filename);
        const std::string entryPoint = Narrow(source.entryPoint);
        const std::filesystem::path path = ResolveShaderPath(filename);

        std::ifstream file(path, std::ios::binary);
        if (not file)
        {
            g_FError("Shader not found: %s", path.string());
            return {};
        }
        const std::string hlsl{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };

        shaderc::CompileOptions options;
        options.SetSourceLanguage(shaderc_source_language_hlsl);
        // Vulkan 1.3 because the backend requires dynamic rendering and
        // synchronization2, both core there.
        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
        // Matches the DX12 path's -Zi -Od: readable SPIR-V, and validation
        // messages that point at real source lines.
        options.SetOptimizationLevel(shaderc_optimization_level_zero);
        options.SetGenerateDebugInfo();

        const shaderc::Compiler compiler;
        const shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(
            hlsl, ToShaderKind(source.stage), filename.c_str(), entryPoint.c_str(), options);

        if (result.GetCompilationStatus() != shaderc_compilation_status_success)
        {
            g_FError("Shader compilation failed (%s): %s", filename, result.GetErrorMessage());
            return {};
        }

        return CompiledShader{ { result.cbegin(), result.cend() }, entryPoint };
    }
}
