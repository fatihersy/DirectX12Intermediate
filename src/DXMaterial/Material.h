#pragma once

enum class FTextureType : UINT {
    FTextureType_NONE = 0,
    FTextureType_DIFFUSE = 1,
    FTextureType_SPECULAR = 2,
    FTextureType_AMBIENT = 3,
    FTextureType_EMISSIVE = 4,
    FTextureType_HEIGHT = 5,
    FTextureType_NORMALS = 6,
    FTextureType_SHININESS = 7,
    FTextureType_OPACITY = 8,
    FTextureType_DISPLACEMENT = 9,
    FTextureType_LIGHTMAP = 10,
    FTextureType_REFLECTION = 11,
    FTextureType_BASE_COLOR = 12,
    FTextureType_NORMAL_CAMERA = 13,
    FTextureType_EMISSION_COLOR = 14,
    FTextureType_METALNESS = 15,
    FTextureType_DIFFUSE_ROUGHNESS = 16,
    FTextureType_AMBIENT_OCCLUSION = 17,
    FTextureType_UNKNOWN = 18,
    FTextureType_SHEEN = 19,
    FTextureType_CLEARCOAT = 20,
    FTextureType_TRANSMISSION = 21,
    FTextureType_MAYA_BASE = 22,
    FTextureType_MAYA_SPECULAR = 23,
    FTextureType_MAYA_SPECULAR_COLOR = 24,
    FTextureType_MAYA_SPECULAR_ROUGHNESS = 25,
    FTextureType_ANISOTROPY = 26,
    FTextureType_GLTF_METALLIC_ROUGHNESS = 27,
    FTextureType_MAX = 28,
    FTextureType_Force32Bit = INT_MAX
};

struct FTexture {
    FTextureType textureType = FTextureType::FTextureType_NONE;
    ComPtr<ID3D12Resource2> defaultBuffer;
    ComPtr<ID3D12Resource2> uploadBuffer;
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle;
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle;
    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
    UINT width{};
    UINT height{};
    UINT RowPitch{};
};

class Material
{
public:
    std::string m_name;
    std::vector<FTexture> m_textures;
    CD3DX12_GPU_DESCRIPTOR_HANDLE m_baseGPUhandle;

    DirectX::XMFLOAT4 m_baseColor{ 1.f, 0.f, 1.f, 1.f };
    FLOAT m_metallic{};
    FLOAT m_roughness{};
    FLOAT m_opacity{ 1.f };

    UINT m_textureFlags{};
    bool m_isOnGPU{};
    bool m_isOnCPU{};
    
    Material(IWICImagingFactory2* wicFactory);
    
    HRESULT LoadTexture(ID3D12Device* device, IWICBitmapDecoder* decoder, INT tType);

    void UploadGPU(ID3D12Device* device, ID3D12CommandQueue* cmdQueue, ID3D12GraphicsCommandList* cmdList);
    void UnloadGPU();
    void ResetUploadHeaps();

    void Bind(ID3D12GraphicsCommandList* cmdList) const;

    inline bool HasTextureType(FTextureType tType) {
        for (FTexture& tex : m_textures) {
            if (tex.textureType == tType)
                return true;
        }
        return false;
    }
    inline const FTexture* GetTextureByType(FTextureType tType) {
        for (FTexture& tex : m_textures) {
            if (tex.textureType == tType)
                return &tex;
        }
        return nullptr;
    }

    static const char* TextureTypeToString(FTextureType tType);

private:
    IWICImagingFactory2* m_wicFactory;

    static inline DXGI_FORMAT FormatTOtype(FTextureType tType)
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

