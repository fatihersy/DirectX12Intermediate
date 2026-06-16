#pragma once
#include "core/Defines.h"
#include "core/Math.h"

#include "TextureTypes.h"

#include "DescripterTypes.h"

namespace NSTexture
{
    struct Texture
    {
        std::filesystem::path path;
        std::wstring name;
        TextureDesc desc;
        ComPtr<ID3D12Resource2> defaultBuffer;
        ComPtr<ID3D12Resource2> uploadBuffer;
        NSDescriptor::Offset srvOffset;
        UINT width{};
        UINT height{};
        UINT localRowPitch{};
        UINT uploadRowPitch{};
        TextureData chunks;
        bool m_isOnGPU{};
        bool m_isOnCPU{};

        void CopyPixels(std::byte* dst, size_t dstRowPitch, size_t bytesPerRow, bool consumeLocalData = false);
        void CopyPixels(std::byte* dst, size_t dstRowPitch, NSMath::SRectU32 cpyRect);
        void CopyPixels(std::byte* dst, size_t dstRowPitch, NSMath::SRectU32 cpyRect, ECopyPixelEdgeMode edgeMode);

        Texture&& PopulateCPU(bool consumeLocalData);
        Texture&& PopulateGPU(
            NSDX12::GraphicsCommandList cmdList,
            bool barrierTransition = true,
            D3D12_RESOURCE_STATES stateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            NSDX12::SHADER_RESOURCE_VIEW_DESC srvDesc = NSDX12::SHADER_RESOURCE_VIEW_DESC
            {
                .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
                .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                .Texture2D = {
                    .MostDetailedMip = 0u,
                    .MipLevels = 1u,
                    .PlaneSlice = 0u,
                    .ResourceMinLODClamp = 0.f
                }
            }
        );
        void UnloadCPU();
        void UnloadGPU();
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

    inline WICLoadDesc GetWICTextureDesc(DXGI_FORMAT format)
    {
        WICLoadDesc wicDesc{};
        wicDesc.texDesc.format = format;

        switch (format)
        {
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            wicDesc.wicPixelFormat = GUID_WICPixelFormat32bppRGBA;
            wicDesc.texDesc.bytesPerPixel = 4u;
            break;

        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
            wicDesc.wicPixelFormat = GUID_WICPixelFormat32bppBGRA;
            wicDesc.texDesc.bytesPerPixel = 4u;
            break;

        case DXGI_FORMAT_R8_UNORM:
            wicDesc.wicPixelFormat = GUID_WICPixelFormat8bppGray;
            wicDesc.texDesc.bytesPerPixel = 1u;
            break;

        case DXGI_FORMAT_R16_UNORM:
            wicDesc.wicPixelFormat = GUID_WICPixelFormat16bppGray;
            wicDesc.texDesc.bytesPerPixel = 2u;
            break;

        default:
            ASSERT(false, "Unsupported WIC texture load format");
            break;
        }

        return wicDesc;
    }
    EChannel ChannelDeduction(unsigned char name);
    TextureMetadata ProbeTexture(std::filesystem::path path);

    inline WICLoadDesc GetWICTextureDesc(EType type)
    {
        WICLoadDesc wicDesc = GetWICTextureDesc(TypeToFormat(type));
        wicDesc.texDesc.textureType = type;
        wicDesc.texDesc.localRowPitchMode = ERowPitchMode::TIGHT;
        wicDesc.texDesc.uploadRowPitchMode = ERowPitchMode::DX_ALIGN;

        return wicDesc;
    }
    UINT CalculateRowPitch(UINT width, UINT bytesPerPixel, NSTexture::ERowPitchMode mode);

    Texture LoadTexture(std::wstring_view name, std::filesystem::path path, EType type);
    Texture LoadTexture(std::wstring_view name, std::filesystem::path path, EXRLoadDesc desc);
    Texture LoadTextureWIC(std::wstring_view name, IWICBitmapDecoder* decoder, EType type);
    Texture LoadTextureWIC(std::wstring_view name, std::filesystem::path path, WICLoadDesc desc);
    Texture LoadTextureWIC(std::wstring_view name, IWICBitmapDecoder* decoder, WICLoadDesc desc);

    Texture LoadTextureEXR(std::wstring_view name, std::filesystem::path path, EXRLoadDesc desc);

    Texture LoadTextureMemory(std::wstring_view name, const std::byte* data, UINT srcRowPitch, size_t dataSize, MemoryLoadDesc desc);
}
