#pragma once

// dxcapi.h / ComPtr came from the precompiled header before DirectX was
// split out of it; included explicitly now.
#include "PlatformHeaders_DX12.h"

class ShaderCompiler
{
public:
    ShaderCompiler();
    ~ShaderCompiler();

    Microsoft::WRL::ComPtr<IDxcCompiler3> m_dxcCompiler;
    Microsoft::WRL::ComPtr<IDxcLibrary> m_dxcLibrary;
    Microsoft::WRL::ComPtr<IDxcUtils> m_dxcUtils;
    Microsoft::WRL::ComPtr<IDxcValidator2> m_dxcValidator;
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> m_dxcIncludeHandler;

    static ShaderCompiler* GetInstance()
    {
        ASSERT(s_instance != nullptr);
        return s_instance;
    }

    void CompileShader(LPCWSTR pFilename, Microsoft::WRL::ComPtr<IDxcBlob>& outShader, std::vector<LPCWSTR>& args);

private:
    static ShaderCompiler* s_instance;
};
