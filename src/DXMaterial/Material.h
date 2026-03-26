#pragma once

class Material
{
public:
    std::string m_name;
    std::vector<FTexture> m_textures;
    Descriptor::Handle m_srvHandle;

    DirectX::XMFLOAT4 m_baseColor{ 1.f, 0.f, 1.f, 1.f };
    FLOAT m_metallic{};
    FLOAT m_roughness{};
    FLOAT m_opacity{ 1.f };

    UINT m_textureFlags{};
    bool m_isOnGPU{};
    bool m_isOnCPU{};
    
    Material(IWICImagingFactory2* wicFactory);
    
    HRESULT LoadTexture(ID3D12Device* device, IWICBitmapDecoder* decoder, INT tType);

    void UploadGPU(ID3D12Device* device, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList);
    void UnloadGPU(NSRenderer::Ctx rendererCtx);
    void ResetUploadHeaps();

    void Bind(NSRenderer::GraphicsCommandList cmdList) const;

    bool HasTextureType(FTextureType tType) {
        for (FTexture& tex : m_textures) {
            if (tex.textureType == tType)
                return true;
        }
        return false;
    }
    const FTexture* GetTextureByType(FTextureType tType) {
        for (FTexture& tex : m_textures) {
            if (tex.textureType == tType)
                return &tex;
        }
        return nullptr;
    }

    static const char* TextureTypeToString(FTextureType tType);

private:
    IWICImagingFactory2* m_wicFactory;

    static DXGI_FORMAT FormatTOtype(FTextureType tType)
    {
        switch (tType)
        {
            case FTextureType::FTextureType_DIFFUSE:
            case FTextureType::FTextureType_BASE_COLOR:
            case FTextureType::FTextureType_SPECULAR:
            case FTextureType::FTextureType_AMBIENT:
            case FTextureType::FTextureType_EMISSIVE:
            case FTextureType::FTextureType_EMISSION_COLOR:
            case FTextureType::FTextureType_MAYA_BASE:
            case FTextureType::FTextureType_MAYA_SPECULAR_COLOR:
                return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

            case FTextureType::FTextureType_NORMALS:
            case FTextureType::FTextureType_NORMAL_CAMERA:
            case FTextureType::FTextureType_HEIGHT:
            case FTextureType::FTextureType_DISPLACEMENT:
            case FTextureType::FTextureType_METALNESS:
            case FTextureType::FTextureType_DIFFUSE_ROUGHNESS:
            case FTextureType::FTextureType_AMBIENT_OCCLUSION:
            case FTextureType::FTextureType_SHININESS:
            case FTextureType::FTextureType_OPACITY:
            case FTextureType::FTextureType_LIGHTMAP:
            case FTextureType::FTextureType_REFLECTION:
            case FTextureType::FTextureType_SHEEN:
            case FTextureType::FTextureType_CLEARCOAT:
            case FTextureType::FTextureType_TRANSMISSION:
            case FTextureType::FTextureType_MAYA_SPECULAR:
            case FTextureType::FTextureType_MAYA_SPECULAR_ROUGHNESS:
            case FTextureType::FTextureType_ANISOTROPY:
            case FTextureType::FTextureType_GLTF_METALLIC_ROUGHNESS:
                return DXGI_FORMAT_R8G8B8A8_UNORM;

            case FTextureType::FTextureType_UNKNOWN:
            case FTextureType::FTextureType_NONE:

            default:
                return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        }
    }
};

