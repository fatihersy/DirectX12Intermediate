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
        uint32_t width{};
        uint32_t height{};
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

    enum class ECopyPixelEdgeMode : uint8_t
    {
        UNDEFINED,
        ZERO,
        CLAMP,
        MIRROR,
        REPEAT
    };
}
