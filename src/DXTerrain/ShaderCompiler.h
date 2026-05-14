#pragma once

class ShaderCompiler
{
public:
    ShaderCompiler();
    ~ShaderCompiler();

    ComPtr<IDxcCompiler3> m_dxcCompiler;
    ComPtr<IDxcLibrary> m_dxcLibrary;
    ComPtr<IDxcUtils> m_dxcUtils;
    ComPtr<IDxcValidator2> m_dxcValidator;
    ComPtr<IDxcIncludeHandler> m_dxcIncludeHandler;

    static ShaderCompiler* GetInstance()
    {
        ASSERT(s_instance);
        return s_instance;
    }

    void CompileShader(LPCWSTR pFileName, ComPtr<IDxcBlob>& outShader, std::vector<LPCWSTR>& args);
private:
    static ShaderCompiler* s_instance;
};
