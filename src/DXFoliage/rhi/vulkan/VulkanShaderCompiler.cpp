#include "stdafx.h"
#include "VulkanShaderCompiler.h"

#include "core/Defines.h"
#include "platform/PlatformUtils.h"
#include "Logger.h"

// DXC's public header. On Linux it pulls in WinAdapter.h, which supplies
// the COM vocabulary the API is built on - IID, HRESULT, CComPtr - so the
// same calls compile here and on Windows.
#include <dxcapi.h>

namespace NSRHIVulkan
{
    namespace
    {
        // ShaderSource carries wide strings because DXC's interface is
        // LPCWSTR everywhere. On Linux wchar_t is 4 bytes rather than 2,
        // which WinAdapter accounts for, so a plain widening copy is fine
        // for the ASCII names this project uses.
        std::wstring Widen(std::string_view narrow)
        {
            std::wstring result;
            result.reserve(narrow.size());
            for (const char c : narrow) result.push_back(static_cast<wchar_t>(c));
            return result;
        }

        std::string Narrow(std::wstring_view wide)
        {
            std::string result;
            result.reserve(wide.size());
            for (const wchar_t c : wide) result.push_back(static_cast<char>(c));
            return result;
        }

        std::filesystem::path ResolveShaderPath(const std::string& filename)
        {
            const std::filesystem::path relative =
                std::filesystem::path(Narrow(SHADERS_FOLDER)) / filename;

            const std::filesystem::path nextToBinary =
                NSPlatform::GetExecutableDirectory() / relative;
            if (std::filesystem::exists(nextToBinary)) return nextToBinary;

            return relative;
        }

        // Shader Model 6.7. The whole reason for moving off glslang, which
        // stops around 6.0 - it rejects 16-bit types and templates.
        const wchar_t* ProfileFor(NSRHI::EShaderStage stage)
        {
            return (stage == NSRHI::EShaderStage::Pixel) ? L"ps_6_7" : L"vs_6_7";
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

        CComPtr<IDxcCompiler3> compiler;
        CComPtr<IDxcUtils> utils;
        if (FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler))) or
            FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils))))
        {
            g_FError("DXC: could not create compiler instance");
            return {};
        }

        // An include handler is needed even with no #includes today: DXC
        // errors rather than assuming when it is null and a file uses one.
        CComPtr<IDxcIncludeHandler> includeHandler;
        utils->CreateDefaultIncludeHandler(&includeHandler);

        const std::wstring wideEntry = Widen(entryPoint);
        const std::wstring wideName = Widen(filename);

        std::vector<LPCWSTR> args{
            wideName.c_str(),               // shows up in diagnostics
            L"-T", ProfileFor(source.stage),
            L"-E", wideEntry.c_str(),
            L"-spirv",
            L"-fspv-target-env=vulkan1.3",
            // Matches the DX12 path's -Zi -Od: readable output, and
            // validation messages that point at real source lines.
            L"-Zi",
            L"-Od",
        };

        DxcBuffer sourceBuffer{};
        sourceBuffer.Ptr = hlsl.data();
        sourceBuffer.Size = hlsl.size();
        sourceBuffer.Encoding = DXC_CP_UTF8;

        CComPtr<IDxcResult> result;
        if (FAILED(compiler->Compile(&sourceBuffer, args.data(),
                                     static_cast<UINT32>(args.size()),
                                     includeHandler, IID_PPV_ARGS(&result))))
        {
            g_FError("DXC: Compile() failed for %s", filename);
            return {};
        }

        // Diagnostics come back even on success (warnings), so report them
        // before deciding whether the compile actually failed.
        CComPtr<IDxcBlobUtf8> errors;
        result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
        if (errors and errors->GetStringLength() > 0)
        {
            g_FError("DXC (%s): %s", filename, errors->GetStringPointer());
        }

        HRESULT status{};
        result->GetStatus(&status);
        if (FAILED(status)) return {};

        CComPtr<IDxcBlob> spirvBlob;
        if (FAILED(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&spirvBlob), nullptr)) or not spirvBlob)
        {
            g_FError("DXC: no SPIR-V produced for %s", filename);
            return {};
        }

        // SPIR-V is a stream of 32-bit words; the blob hands back bytes.
        const auto* words = static_cast<const uint32_t*>(spirvBlob->GetBufferPointer());
        const size_t wordCount = spirvBlob->GetBufferSize() / sizeof(uint32_t);

        // DXC keeps the HLSL entry point name in the emitted SPIR-V, same
        // as glslang did (verified: OpEntryPoint Vertex %mainVS "mainVS"),
        // so VkPipelineShaderStageCreateInfo::pName must carry it rather
        // than assuming "main".
        return CompiledShader{ { words, words + wordCount }, entryPoint };
    }
}
