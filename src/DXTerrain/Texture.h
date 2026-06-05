#pragma once

namespace NSTexture
{
    enum EChannel : uint32_t
    {
        ChUnknown = 0,
        ChR,
        ChG,
        ChB,
        ChA,
        ChY,
        ChForce32Bit = UINT32_MAX
    };
    constexpr size_t ChBegin = static_cast<size_t>(ChUnknown);
    constexpr size_t ChEnd = static_cast<size_t>(ChY) + 1u;

    constexpr EChannel ChFront = ChR;
    constexpr EChannel ChBack = ChY;

    constexpr std::array<std::string_view, ChEnd> channelNames =
    {
        "",
        "R",
        "G",
        "B",
        "A",
        "Y"
    };

    template<EChannel name>
    constexpr std::string_view ChName = channelNames[static_cast<size_t>(name)];

    constexpr std::string_view GetChannelName(EChannel channel)
    {
        return channelNames[static_cast<size_t>(channel)];
    }

    struct SHADER_RESOURCE_VIEW_DESC
    {
        D3D12_SRV_DIMENSION ViewDimension;
        UINT Shader4ComponentMapping;
        union
        {
            D3D12_BUFFER_SRV Buffer;
            D3D12_TEX1D_SRV Texture1D;
            D3D12_TEX1D_ARRAY_SRV Texture1DArray;
            D3D12_TEX2D_SRV Texture2D;
            D3D12_TEX2D_ARRAY_SRV Texture2DArray;
            D3D12_TEX2DMS_SRV Texture2DMS;
            D3D12_TEX2DMS_ARRAY_SRV Texture2DMSArray;
            D3D12_TEX3D_SRV Texture3D;
            D3D12_TEXCUBE_SRV TextureCube;
            D3D12_TEXCUBE_ARRAY_SRV TextureCubeArray;
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_SRV RaytracingAccelerationStructure;
            D3D12_BUFFER_SRV_BYTE_OFFSET BufferByteOffset;
        };
    };

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

    enum class ERowPitchMode
    {
        UNDEFINED,
        DX_ALIGN,
        TIGHT
    };

    inline UINT BytesPerPixel (DXGI_FORMAT format)
    {
        switch (format)
        {
            case DXGI_FORMAT::DXGI_FORMAT_R8_UNORM:
            return 1u;

            case DXGI_FORMAT::DXGI_FORMAT_R16_UNORM:
            case DXGI_FORMAT::DXGI_FORMAT_R16_FLOAT:
            return 2u;

            case DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT:
            case DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM:
            case DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            case DXGI_FORMAT::DXGI_FORMAT_R16G16_FLOAT:
            return 4u;

            case DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT:
            case DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_FLOAT:
            return 8u;

            case DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT:
            return 16u;

            default: {
                ASSERT(false, "Unsupported format");
                return std::numeric_limits<UINT>::max();
            };
        }
    };

    inline UINT ComponentCount(DXGI_FORMAT format)
    {
        switch (format)
        {
            case DXGI_FORMAT::DXGI_FORMAT_R8_UNORM:
            case DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT:
            case DXGI_FORMAT::DXGI_FORMAT_R16_UNORM:
            case DXGI_FORMAT::DXGI_FORMAT_R16_FLOAT:
            return 1u;

            case DXGI_FORMAT::DXGI_FORMAT_R16G16_FLOAT:
            case DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT:
            return 2u;

            case DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_FLOAT:
            case DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM:
            case DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            case DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT:
            return 4u;

            default: {
                ASSERT(false, "Unsupported format");
                return std::numeric_limits<UINT>::max();
            };
        }
    };

    struct TextureMetadata
    {
        uint32_t width;
        uint32_t height;
        std::vector<EChannel> channels;
        bool success{};
    };

    struct TextureDesc
    {
        EType textureType = EType::EType_UNKNOWN;
        DXGI_FORMAT format = DXGI_FORMAT::DXGI_FORMAT_UNKNOWN;
        UINT bytesPerPixel = 0u;
        double alphaThresholdPercent = 0.f;
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;
        UINT16 mipLevels = 1u;
        ERowPitchMode localRowPitchMode = ERowPitchMode::UNDEFINED;
        ERowPitchMode uploadRowPitchMode = ERowPitchMode::DX_ALIGN;
    };

    struct WICLoadDesc
    {
        TextureDesc texDesc;
        WICPixelFormatGUID wicPixelFormat = GUID_WICPixelFormat32bppRGBA;
        WICBitmapDitherType dither = WICBitmapDitherTypeNone;
        IWICPalette* palette = nullptr;
        WICBitmapPaletteType paletteTranslate = WICBitmapPaletteTypeCustom;
    };

    struct EXRLoadDesc
    {
        TextureDesc texDesc;
        std::array<std::string_view, 4> channels = {
            NSTexture::ChName<NSTexture::ChR>,
            NSTexture::ChName<NSTexture::ChG>,
            NSTexture::ChName<NSTexture::ChB>,
            NSTexture::ChName<NSTexture::ChA> };
        std::array<float, 4> defaultValues = { 0.f, 0.f, 0.f, 1.f };
    };

    struct MemoryLoadDesc
    {
        TextureDesc texDesc;
        UINT width{};
        UINT height{};
    };

    constexpr UINT ROWS_AT_A_TIME = 1000;

    struct TextureChunk
    {
        UINT firstRow;
        UINT rowCount;
        std::vector<std::byte> bytes;
    };
    using TextureData = std::vector<TextureChunk>;

    struct Texture
    {
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
        void CopyPixels(std::byte* dst, size_t dstRowPitch, NSMath::SRectU32 srcRect);

        Texture&& PopulateCPU(bool consumeLocalData);
        Texture&& PopulateGPU(
            NSRenderer::GraphicsCommandList cmdList,
            bool barrierTransition = true,
            D3D12_RESOURCE_STATES stateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            SHADER_RESOURCE_VIEW_DESC srvDesc = SHADER_RESOURCE_VIEW_DESC
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

    Texture LoadTexture(std::wstring_view name, std::wstring_view filename, EType type);
    Texture LoadTexture(std::wstring_view name, std::wstring_view filename, EXRLoadDesc desc);
    Texture LoadTextureWIC(std::wstring_view name, IWICBitmapDecoder* decoder, EType type);
    Texture LoadTextureWIC(std::wstring_view name, std::wstring_view filename, WICLoadDesc desc);
    Texture LoadTextureWIC(std::wstring_view name, IWICBitmapDecoder* decoder, WICLoadDesc desc);

    Texture LoadTextureEXR(std::wstring_view name, std::wstring_view filename, EXRLoadDesc desc);

    Texture LoadTextureMemory(std::wstring_view name, const std::byte* data, UINT srcRowPitch, size_t dataSize, MemoryLoadDesc desc);
}
