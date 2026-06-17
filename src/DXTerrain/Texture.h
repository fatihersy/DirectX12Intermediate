#pragma once
#include "core/Defines.h"
#include "core/Math.h"

#include "TextureTypes.h"

#include "DescriptorTypes.h"

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
        case NSTexture::EType::DIFFUSE: return "Diffuse";
        case NSTexture::EType::SPECULAR: return "Specular";
        case NSTexture::EType::AMBIENT: return "Ambient";
        case NSTexture::EType::EMISSIVE: return "Emissive";
        case NSTexture::EType::HEIGHT: return "Height";
        case NSTexture::EType::NORMALS: return "Normals";
        case NSTexture::EType::SHININESS: return "Shininess";
        case NSTexture::EType::OPACITY: return "Opacity";
        case NSTexture::EType::DISPLACEMENT: return "Displacement";
        case NSTexture::EType::LIGHTMAP: return "Lightmap";
        case NSTexture::EType::REFLECTION: return "Reflection";
        case NSTexture::EType::BASE_COLOR: return "Base Color";
        case NSTexture::EType::NORMAL_CAMERA: return "Normal Camera";
        case NSTexture::EType::EMISSION_COLOR: return "Emission Color";
        case NSTexture::EType::METALNESS: return "Metalness";
        case NSTexture::EType::DIFFUSE_ROUGHNESS: return "Diffuse Roughness";
        case NSTexture::EType::AMBIENT_OCCLUSION: return "Ambient Occlusion";
        case NSTexture::EType::GLTF_METALLIC_ROUGHNESS: return "GLTF Metallic Roughness";
        default: return "Unknown";
        }
    }
    inline const wchar_t* TextureTypeToWString(NSTexture::EType tType)
    {
        switch (tType)
        {
        case NSTexture::EType::DIFFUSE: return L"Diffuse";
        case NSTexture::EType::SPECULAR: return L"Specular";
        case NSTexture::EType::AMBIENT: return L"Ambient";
        case NSTexture::EType::EMISSIVE: return L"Emissive";
        case NSTexture::EType::HEIGHT: return L"Height";
        case NSTexture::EType::NORMALS: return L"Normals";
        case NSTexture::EType::SHININESS: return L"Shininess";
        case NSTexture::EType::OPACITY: return L"Opacity";
        case NSTexture::EType::DISPLACEMENT: return L"Displacement";
        case NSTexture::EType::LIGHTMAP: return L"Lightmap";
        case NSTexture::EType::REFLECTION: return L"Reflection";
        case NSTexture::EType::BASE_COLOR: return L"Base Color";
        case NSTexture::EType::NORMAL_CAMERA: return L"Normal Camera";
        case NSTexture::EType::EMISSION_COLOR: return L"Emission Color";
        case NSTexture::EType::METALNESS: return L"Metalness";
        case NSTexture::EType::DIFFUSE_ROUGHNESS: return L"Diffuse Roughness";
        case NSTexture::EType::AMBIENT_OCCLUSION: return L"Ambient Occlusion";
        case NSTexture::EType::GLTF_METALLIC_ROUGHNESS: return L"GLTF Metallic Roughness";
        default: return L"Unknown";
        }
    }
    inline DXGI_FORMAT TypeToFormat(NSTexture::EType tType)
    {
        switch (tType)
        {
        case NSTexture::EType::DIFFUSE:
        case NSTexture::EType::BASE_COLOR:
        case NSTexture::EType::SPECULAR:
        case NSTexture::EType::AMBIENT:
        case NSTexture::EType::EMISSIVE:
        case NSTexture::EType::EMISSION_COLOR:
        case NSTexture::EType::MAYA_BASE:
        case NSTexture::EType::MAYA_SPECULAR_COLOR:
            return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

        case NSTexture::EType::NORMALS:
        case NSTexture::EType::NORMAL_CAMERA:
        case NSTexture::EType::METALNESS:
        case NSTexture::EType::DIFFUSE_ROUGHNESS:
        case NSTexture::EType::AMBIENT_OCCLUSION:
        case NSTexture::EType::SHININESS:
        case NSTexture::EType::OPACITY:
        case NSTexture::EType::LIGHTMAP:
        case NSTexture::EType::REFLECTION:
        case NSTexture::EType::SHEEN:
        case NSTexture::EType::CLEARCOAT:
        case NSTexture::EType::TRANSMISSION:
        case NSTexture::EType::MAYA_SPECULAR:
        case NSTexture::EType::MAYA_SPECULAR_ROUGHNESS:
        case NSTexture::EType::ANISOTROPY:
        case NSTexture::EType::GLTF_METALLIC_ROUGHNESS:
            return DXGI_FORMAT_R8G8B8A8_UNORM;

        case NSTexture::EType::HEIGHT:
        case NSTexture::EType::DISPLACEMENT:
            return DXGI_FORMAT_R16_UNORM;

        case NSTexture::EType::UNKNOWN:
        case NSTexture::EType::NONE:

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
