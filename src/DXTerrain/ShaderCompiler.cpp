#include "stdafx.h"
#include "ShaderCompiler.h"

#include "IApp.h"
#include "DXSampleHelper.h"

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

void ShaderCompiler::CompileShader(LPCWSTR pFileName, ComPtr<IDxcBlob>& outShader, std::vector<LPCWSTR>& args)
{
    ComPtr<IDxcOperationResult> opResult;
    HRESULT hr{};

    ComPtr<IDxcBlobEncoding> shaderSource;
    ThrowIfFailed(ShaderCompiler::GetInstance()->m_dxcUtils->LoadFile(pFileName, nullptr, &shaderSource));

    DxcBuffer sourceBuffer{};
    sourceBuffer.Encoding = DXC_CP_ACP;
    sourceBuffer.Ptr = shaderSource.Get();

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

    if (error and error->GetStringLength() > 0) OutputDebugStringA(error->GetStringPointer());

    compileResult->GetStatus(&hr);
    ThrowIfFailed(hr);

    ThrowIfFailed(compileResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&outShader), nullptr));

    ComPtr<IDxcBlob> pPDB;
    ComPtr<IDxcBlobWide> pdbPath;
    ThrowIfFailed(compileResult->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pPDB), pdbPath.GetAddressOf()));

    if (pPDB and pPDB->GetBufferSize() > 0 and pdbPath and pdbPath->GetBufferSize() > 0)
    {
        // WARN: Hardcodded object path
        std::filesystem::path path = IApp::GetInstance()->im_executablePath.append(L"..\\..\\obj\\").append(PROJECT_NAME).append(L"\\");
        if (std::filesystem::exists(path))
        {
            std::wstring fullPath = KTool::wformat(L"%s%s", path.wstring(), pdbPath->GetStringPointer());

            FILE* pFile = nullptr;
            _wfopen_s(&pFile, fullPath.c_str(), L"wb");
            if (pFile)
            {
                fwrite(pPDB->GetBufferPointer(), 1, pPDB->GetBufferSize(), pFile);
                fclose(pFile);
            }
        }
    }

    if (FAILED(m_dxcValidator->Validate(outShader.Get(), DxcValidatorFlags_Default, &opResult)))
    {
        throw std::runtime_error("Cannot validate");
    }

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
                size_t errlen = errorBlobUtf8->GetBufferSize();
                OutputDebugStringA(errstr);
            }
        }
    }
}

