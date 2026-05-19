#pragma once

namespace NSTexture
{
    enum class EType : UINT {
        EType_NONE = 0,
        EType_DIFFUSE = 1,
        EType_SPECULAR = 2,
        EType_AMBIENT = 3,
        EType_EMISSIVE = 4,
        EType_HEIGHT = 5,
        EType_NORMALS = 6,
        EType_SHININESS = 7,
        EType_OPACITY = 8,
        EType_DISPLACEMENT = 9,
        EType_LIGHTMAP = 10,
        EType_REFLECTION = 11,
        EType_BASE_COLOR = 12,
        EType_NORMAL_CAMERA = 13,
        EType_EMISSION_COLOR = 14,
        EType_METALNESS = 15,
        EType_DIFFUSE_ROUGHNESS = 16,
        EType_AMBIENT_OCCLUSION = 17,
        EType_UNKNOWN = 18,
        EType_SHEEN = 19,
        EType_CLEARCOAT = 20,
        EType_TRANSMISSION = 21,
        EType_MAYA_BASE = 22,
        EType_MAYA_SPECULAR = 23,
        EType_MAYA_SPECULAR_COLOR = 24,
        EType_MAYA_SPECULAR_ROUGHNESS = 25,
        EType_ANISOTROPY = 26,
        EType_GLTF_METALLIC_ROUGHNESS = 27,
        EType_MAX = 28,
        EType_Force32Bit = UINT_MAX
    };

    struct LoadTextureDesc
    {
        EType textureType = EType::EType_UNKNOWN;
        DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
        WICPixelFormatGUID wicPixelFormat = GUID_WICPixelFormat32bppRGBA;
        UINT bytesPerPixel = 4u;
        WICBitmapDitherType dither = WICBitmapDitherTypeNone;
        IWICPalette* palette = nullptr;
        double alphaThresholdPercent = 0.f;
        WICBitmapPaletteType paletteTranslate = WICBitmapPaletteTypeCustom;
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;
        UINT16 mipLevels = 1u;
        bool keepCpuData = false;
    };

    struct Texture
    {
        LoadTextureDesc desc;
        EType textureType = EType::EType_NONE;
        ComPtr<ID3D12Resource2> defaultBuffer;
        ComPtr<ID3D12Resource2> uploadBuffer;
        NSDescriptor::Offset srvOffset;
        DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
        UINT width{};
        UINT height{};
        UINT RowPitch{};
        std::vector<std::byte> cpuData;
        bool m_isOnGPU{};
        bool m_isOnCPU{};
    };

    inline const char* TextureTypeToString(NSTexture::EType tType)
    {
        switch (tType)
        {
        case NSTexture::EType::EType_DIFFUSE: return "Diffuse";
        case NSTexture::EType::EType_SPECULAR: return "Specular";
        case NSTexture::EType::EType_AMBIENT: return "Ambient";
        case NSTexture::EType::EType_EMISSIVE: return "Emissive";
        case NSTexture::EType::EType_HEIGHT: return "Height";
        case NSTexture::EType::EType_NORMALS: return "Normals";
        case NSTexture::EType::EType_SHININESS: return "Shininess";
        case NSTexture::EType::EType_OPACITY: return "Opacity";
        case NSTexture::EType::EType_DISPLACEMENT: return "Displacement";
        case NSTexture::EType::EType_LIGHTMAP: return "Lightmap";
        case NSTexture::EType::EType_REFLECTION: return "Reflection";
        case NSTexture::EType::EType_BASE_COLOR: return "Base Color";
        case NSTexture::EType::EType_NORMAL_CAMERA: return "Normal Camera";
        case NSTexture::EType::EType_EMISSION_COLOR: return "Emission Color";
        case NSTexture::EType::EType_METALNESS: return "Metalness";
        case NSTexture::EType::EType_DIFFUSE_ROUGHNESS: return "Diffuse Roughness";
        case NSTexture::EType::EType_AMBIENT_OCCLUSION: return "Ambient Occlusion";
        case NSTexture::EType::EType_GLTF_METALLIC_ROUGHNESS: return "GLTF Metallic Roughness";
        default: return "Unknown";
        }
    }
    inline const wchar_t* TextureTypeToWString(NSTexture::EType tType)
    {
        switch (tType)
        {
        case NSTexture::EType::EType_DIFFUSE: return L"Diffuse";
        case NSTexture::EType::EType_SPECULAR: return L"Specular";
        case NSTexture::EType::EType_AMBIENT: return L"Ambient";
        case NSTexture::EType::EType_EMISSIVE: return L"Emissive";
        case NSTexture::EType::EType_HEIGHT: return L"Height";
        case NSTexture::EType::EType_NORMALS: return L"Normals";
        case NSTexture::EType::EType_SHININESS: return L"Shininess";
        case NSTexture::EType::EType_OPACITY: return L"Opacity";
        case NSTexture::EType::EType_DISPLACEMENT: return L"Displacement";
        case NSTexture::EType::EType_LIGHTMAP: return L"Lightmap";
        case NSTexture::EType::EType_REFLECTION: return L"Reflection";
        case NSTexture::EType::EType_BASE_COLOR: return L"Base Color";
        case NSTexture::EType::EType_NORMAL_CAMERA: return L"Normal Camera";
        case NSTexture::EType::EType_EMISSION_COLOR: return L"Emission Color";
        case NSTexture::EType::EType_METALNESS: return L"Metalness";
        case NSTexture::EType::EType_DIFFUSE_ROUGHNESS: return L"Diffuse Roughness";
        case NSTexture::EType::EType_AMBIENT_OCCLUSION: return L"Ambient Occlusion";
        case NSTexture::EType::EType_GLTF_METALLIC_ROUGHNESS: return L"GLTF Metallic Roughness";
        default: return L"Unknown";
        }
    }
    inline DXGI_FORMAT TypeToFormat(NSTexture::EType tType)
    {
        switch (tType)
        {
        case NSTexture::EType::EType_DIFFUSE:
        case NSTexture::EType::EType_BASE_COLOR:
        case NSTexture::EType::EType_SPECULAR:
        case NSTexture::EType::EType_AMBIENT:
        case NSTexture::EType::EType_EMISSIVE:
        case NSTexture::EType::EType_EMISSION_COLOR:
        case NSTexture::EType::EType_MAYA_BASE:
        case NSTexture::EType::EType_MAYA_SPECULAR_COLOR:
            return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

        case NSTexture::EType::EType_NORMALS:
        case NSTexture::EType::EType_NORMAL_CAMERA:
        case NSTexture::EType::EType_METALNESS:
        case NSTexture::EType::EType_DIFFUSE_ROUGHNESS:
        case NSTexture::EType::EType_AMBIENT_OCCLUSION:
        case NSTexture::EType::EType_SHININESS:
        case NSTexture::EType::EType_OPACITY:
        case NSTexture::EType::EType_LIGHTMAP:
        case NSTexture::EType::EType_REFLECTION:
        case NSTexture::EType::EType_SHEEN:
        case NSTexture::EType::EType_CLEARCOAT:
        case NSTexture::EType::EType_TRANSMISSION:
        case NSTexture::EType::EType_MAYA_SPECULAR:
        case NSTexture::EType::EType_MAYA_SPECULAR_ROUGHNESS:
        case NSTexture::EType::EType_ANISOTROPY:
        case NSTexture::EType::EType_GLTF_METALLIC_ROUGHNESS:
            return DXGI_FORMAT_R8G8B8A8_UNORM;

        case NSTexture::EType::EType_HEIGHT:
        case NSTexture::EType::EType_DISPLACEMENT:
            return DXGI_FORMAT_R16_UNORM;

        case NSTexture::EType::EType_UNKNOWN:
        case NSTexture::EType::EType_NONE:

        default:
            return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        }
    }

    inline LoadTextureDesc GetTextureDesc(DXGI_FORMAT format)
    {
        LoadTextureDesc desc{};
        desc.format = format;

        switch (format)
        {
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            desc.wicPixelFormat = GUID_WICPixelFormat32bppRGBA;
            desc.bytesPerPixel = 4u;
            break;

        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
            desc.wicPixelFormat = GUID_WICPixelFormat32bppBGRA;
            desc.bytesPerPixel = 4u;
            break;

        case DXGI_FORMAT_R8_UNORM:
            desc.wicPixelFormat = GUID_WICPixelFormat8bppGray;
            desc.bytesPerPixel = 1u;
            break;

        case DXGI_FORMAT_R16_UNORM:
            desc.wicPixelFormat = GUID_WICPixelFormat16bppGray;
            desc.bytesPerPixel = 2u;
            break;

        default:
            ASSERT(false, "Unsupported WIC texture load format");
            break;
        }

        return desc;
    }

    inline LoadTextureDesc GetTextureDesc(EType type)
    {
        LoadTextureDesc desc = GetTextureDesc(TypeToFormat(type));
        desc.textureType = type;

        if (type == EType::EType_HEIGHT or type == EType::EType_DISPLACEMENT)
        {
            desc.keepCpuData = true;
        }

        return desc;
    }

    Texture LoadTexture(std::wstring_view name, std::wstring_view filename, EType type);
    Texture LoadTexture(std::wstring_view name, std::wstring_view filename, LoadTextureDesc desc);
    Texture LoadTexture(std::wstring_view name, IWICBitmapDecoder* decoder, EType type);
    Texture LoadTexture(std::wstring_view name, IWICBitmapDecoder* decoder, LoadTextureDesc desc);

    void UploadGPU(NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList, NSTexture::Texture& texture,
        bool createSRV = false,
        bool barrierTransition = true,
        D3D12_RESOURCE_STATES stateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );
}
