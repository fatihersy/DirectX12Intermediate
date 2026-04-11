#pragma once

class Material
{
public:
    std::wstring m_name;
    std::vector<NSTexture::Texture> m_textures;
    NSDescriptor::Handle m_srvHandle;

    DirectX::XMFLOAT4 m_baseColor{ 1.f, 1.f, 0.f, 1.f};
    FLOAT m_metallic{};
    FLOAT m_roughness{};
    FLOAT m_opacity{ 1.f };

    bool m_isOnGPU{};
    bool m_isOnCPU{};

    Material(IWICImagingFactory2* wicFactory);

    HRESULT LoadTexture(ID3D12Device14* device, IWICBitmapDecoder* decoder, INT textureType);

    void UploadGPU(ID3D12Device14* device, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList, bool barrierTransition = true);
    void UnloadGPU(NSRenderer::Ctx rendererCtx)
    {
        if (not m_isOnGPU) return;

        rendererCtx.freeSRVStatic(m_srvHandle);

        for (NSTexture::Texture& tex : m_textures)
        {
            tex.defaultBuffer.Reset();
        }

        m_isOnGPU = false;
    }
    void ResetUploadHeaps()
    {
        if (not m_isOnCPU) return;

        for (NSTexture::Texture& tex : m_textures)
        {
            tex.uploadBuffer.Reset();
        }

        m_isOnCPU = false;
    }
    bool HasTextureType(NSTexture::EType tType)
    {
        for (NSTexture::Texture& tex : m_textures)
        {
            if (tex.textureType == tType) return true;
        }

        return false;
    }
    const NSTexture::Texture* GetTextureByType(NSTexture::EType tType)
    {
        for (NSTexture::Texture& tex : m_textures)
        {
            if (tex.textureType == tType) return &tex;
        }

        return nullptr;
    }

    bool HaveTextureFlag(NSTexture::EType flag) const {
        return m_flags.test(static_cast<uint32_t>(flag));
    }
    void SetTextureFlag(NSTexture::EType flag) {
        m_flags.set(static_cast<uint32_t>(flag));
    }
    void ResetTextureFlag(NSTexture::EType flag) {
        m_flags.reset(static_cast<uint32_t>(flag));
    }
    uint32_t GetFlags() const {
        return static_cast<uint32_t>(m_flags.to_ullong());
    }

    static const char* TextureTypeToString(NSTexture::EType tType);
    const wchar_t* TextureTypeToWString(NSTexture::EType tType);
    static DXGI_FORMAT FormatTOtype(NSTexture::EType tType);
private:
    IWICImagingFactory2* m_wicFactory = nullptr;
    std::bitset<32> m_flags{};
};
