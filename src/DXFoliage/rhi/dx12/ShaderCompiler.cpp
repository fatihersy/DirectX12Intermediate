#include "stdafx.h"
#include "ShaderCompiler.h"

#include "IApp.h"
#include "DXSampleHelper.h"

#include "Tools.h"
#include "Logger.h"

ShaderCompiler* ShaderCompiler::s_instance = nullptr;

ShaderCompiler::ShaderCompiler()
{
    ThrowIfFailed(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&m_dxcLibrary)));
    ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_dxcCompiler)));
    ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_dxcUtils)));
    ThrowIfFailed(DxcCreateInstance(CLSID_DxcValidator, IID_PPV_ARGS(&m_dxcValidator)));
    ThrowIfFailed(m_dxcUtils->CreateDefaultIncludeHandler(&m_dxcIncludeHandler));

    s_instance = this;
}
ShaderCompiler::~ShaderCompiler()
{
    s_instance = nullptr;
}

void ShaderCompiler::CompileShader(LPCWSTR pFilenamem, ComPtr<IDxcBlob>& outShader, std::vector<LPCWSTR>& args)
{
    ASSERT(pFilenamem != nullptr, "Invalid filename");

    ComPtr<IDxcOperationResult> opResult;
    HRESULT hr{};

    ComPtr<IDxcBlobEncoding> shaderSource;
    std::wstring filepath = NSTool::wformat(L"%s/%s", SHADERS_FOLDER, pFilenamem);
    ThrowIfFailed(ShaderCompiler::GetInstance()->m_dxcUtils->LoadFile(filepath.c_str(), nullptr, &shaderSource));

    DxcBuffer sourceBuffer{};
    sourceBuffer.Encoding = DXC_CP_ACP;
    sourceBuffer.Ptr = shaderSource->GetBufferPointer();
    sourceBuffer.Size = shaderSource->GetBufferSize();

    ComPtr<IDxcResult> compileResult;
    ThrowIfFailed(m_dxcCompiler->Compile(
        &sourceBuffer,
        args.data(),
        static_cast<UINT>(args.size()),
        m_dxcIncludeHandler.Get(),
        IID_PPV_ARGS(&compileResult)
    ));

    ComPtr<IDxcBlobUtf8> error;
    ThrowIfFailed(compileResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&error), nullptr));

    if (error and error->GetStringLength() > 0)
    {
        const char* errstr = reinterpret_cast<const char*>(error->GetStringPointer());
        int32_t errlen = static_cast<int32_t>(error->GetBufferSize());

        g_FError("%.*s\n", errlen, errstr);
    }

    compileResult->GetStatus(&hr);
    ThrowIfFailed(hr);

    ThrowIfFailed(compileResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&outShader), nullptr));

    ComPtr<IDxcBlob> pPDB;
    ComPtr<IDxcBlobWide> pdbPath;
    ThrowIfFailed(compileResult->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pPDB), nullptr));

    if (pPDB and pPDB->GetBufferSize() > 0 and pdbPath and pdbPath->GetBufferSize() > 0)
    {
        // WARN: Hardcodded object path
        std::filesystem::path path = IApp::GetInstance()->im_executablePath
            .append("..\\..\\obj\\")
            .append(PROJECT_NAME)
            .append(L"\\");
        if (std::filesystem::exists(path))
        {
            std::wstring fullPath {
                NSTool::wformat(L"%s%.*s", path.wstring(), static_cast<int32_t>(pdbPath->GetStringLength()), pdbPath->GetStringPointer())
            };

            std::ofstream file(fullPath, std::ios::binary | std::ios::trunc);
            ASSERT(file.is_open(), "Failed to write PDB data");

            file.write(
                reinterpret_cast<const char*>(pPDB->GetBufferPointer()),
                static_cast<std::streamsize>(pPDB->GetBufferSize())
            );
            ASSERT(file.good(), "Failed to finish writing PDB data");
        }
    }

    ASSERT(SUCCEEDED(m_dxcValidator->Validate(outShader.Get(), DxcValidatorFlags_Default, &opResult)));

    ComPtr<IDxcBlobEncoding> errorBlob;
    if (SUCCEEDED(opResult->GetErrorBuffer(&errorBlob)))
    {
        ComPtr<IDxcBlobEncoding> errorBlobUtf8;
        if (errorBlob and errorBlob->GetBufferSize() > 0)
        {
            ThrowIfFailed(m_dxcLibrary->GetBlobAsUtf8(errorBlob.Get(), &errorBlobUtf8));
            if (errorBlobUtf8)
            {
                const char* errstr = reinterpret_cast<const char*>(errorBlobUtf8->GetBufferPointer());
                int32_t errlen = static_cast<int32_t>(errorBlobUtf8->GetBufferSize());

                g_FError("%.*s\n", errlen, errstr);
            }
        }
    }
}
