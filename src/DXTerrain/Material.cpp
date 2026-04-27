#include "stdafx.h"
#include "Material.h"

Material::Material(IWICImagingFactory2* wicFactory) : m_wicFactory(wicFactory)
{}

HRESULT Material::LoadTexture(ID3D12Device14* device, IWICBitmapDecoder* decoder, INT textureType)
{
    NSTexture::EType tType = static_cast<NSTexture::EType>(textureType);

    assert(device and decoder and tType > NSTexture::EType::EType_NONE and tType < NSTexture::EType::EType_MAX);

    NSTexture::Texture& tex = m_textures.emplace_back(NSTexture::Texture{});

    tex.textureType = tType;
    tex.format = FormatTOtype(tex.textureType);

    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame)))
    {
        OutputDebugStringA("Failed to get frame from decoder\n");
        return E_FAIL;
    }
    if (FAILED(frame->GetSize(&tex.width, &tex.height)))
    {
        OutputDebugStringA("Failed to get texture dimentions\n");
        return E_FAIL;
    }

    const UINT bpp = 4;
    const UINT alignment = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
    const UINT rowPitch = (static_cast<UINT64>(tex.width) * bpp + alignment - 1) & ~(alignment - 1);
    UINT uploadSize = rowPitch * tex.height;
    tex.RowPitch = rowPitch;

    ComPtr<IWICFormatConverter> converter;
    if (FAILED(m_wicFactory->CreateFormatConverter(&converter)))
    {
        OutputDebugStringA("Failed to create format converter\n");
        return E_FAIL;
    }

    if (FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.f, WICBitmapPaletteTypeCustom)))
    {
        OutputDebugStringA("Failed to initialize format converter\n");
        return E_FAIL;
    }

    D3D12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
    D3D12_HEAP_PROPERTIES uploadHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    if (FAILED(device->CreateCommittedResource(
        &uploadHeapProp,
        D3D12_HEAP_FLAG_NONE,
        &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&tex.uploadBuffer)
    )))
    {
        OutputDebugStringA("Failed to create upload buffer\n");
        return E_FAIL;
    }

    void* pMappedData = nullptr;
    if (FAILED(tex.uploadBuffer->Map(0, nullptr, &pMappedData)))
    {
        OutputDebugStringA("Failed to map upload buffer\n");
        return E_FAIL;
    }

    if (FAILED(converter->CopyPixels(nullptr, rowPitch, uploadSize, reinterpret_cast<BYTE*>(pMappedData))))
    {
        tex.uploadBuffer->Unmap(0, nullptr);
        OutputDebugStringA("Failed to copy pixels\n");
        return E_FAIL;
    }

    tex.uploadBuffer->Unmap(0, nullptr);

    if (tex.uploadBuffer and tex.width > 0 and tex.height > 0)
    {
        D3D12_RESOURCE_DESC texDesc{};
        texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Width = tex.width;
        texDesc.Height = tex.height;
        texDesc.DepthOrArraySize = 1;
        texDesc.MipLevels = 1;
        texDesc.Format = tex.format;
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 1;
        texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        D3D12_HEAP_PROPERTIES defaultHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

        if (FAILED(device->CreateCommittedResource(
            &defaultHeapProp,
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&tex.defaultBuffer)
        )))
        {
            OutputDebugStringA("Failed to create default resource heap\n");
            return E_FAIL;
        }

        std::wstring wstrName = std::wstring(m_name.begin(), m_name.end());

        tex.defaultBuffer->SetName(NSTool::wformat(L"%s::%s::defaultBuffer", wstrName.c_str(), TextureTypeToWString(tex.textureType)).c_str());
        tex.uploadBuffer->SetName(NSTool::wformat(L"%s::%s::uploadBuffer", wstrName.c_str(), TextureTypeToWString(tex.textureType)).c_str());
    }

    m_isOnCPU = true;

    SetTextureFlag(tex.textureType);

    return S_OK;
}

void Material::UploadGPU(ID3D12Device14* device, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList, bool barrierTransition)
{
    assert(device);

    if (m_textures.empty() or not m_isOnCPU) return;

    std::for_each(m_textures.begin(), m_textures.end(), [](NSTexture::Texture& tex)
    {
        assert(tex.defaultBuffer and tex.uploadBuffer);
    });

    if (barrierTransition)
    {
        std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
        barriers.reserve(m_textures.size());

        for (NSTexture::Texture& tex : m_textures)
        {
            barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
                tex.defaultBuffer.Get(),
                D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_STATE_COPY_DEST
            ));
        }

        cmdList.ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
    }

    m_srvHandle = rendererCtx.allocSRVStatic(static_cast<INT>(NSTexture::EType::EType_MAX));

    auto handles = []<size_t... N>(std::index_sequence<N...>, NSRenderer::Ctx rendererCtx, NSDescriptor::Handle srv)
    {
        return std::array<D3D12_CPU_DESCRIPTOR_HANDLE, sizeof...(N)> {
            rendererCtx.offsetSRV(srv, static_cast<uint32_t>(N)).cpuAddr...
        };
    }(std::make_index_sequence<static_cast<size_t>(NSTexture::EType::EType_MAX)>{}, rendererCtx, m_srvHandle);

    device->CopyDescriptors(
        static_cast<INT>(NSTexture::EType::EType_MAX),
        handles.data(),
        nullptr,
        1u,
        &rendererCtx.fallbackSRV.cpuAddr,
        nullptr,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
    );

    uint32_t texIndex{};
    for (NSTexture::Texture& tex : m_textures)
    {
        D3D12_TEXTURE_COPY_LOCATION srcLoc{};
        srcLoc.pResource = tex.uploadBuffer.Get();
        srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLoc.PlacedFootprint.Offset = 0;
        srcLoc.PlacedFootprint.Footprint.Format = tex.format;
        srcLoc.PlacedFootprint.Footprint.Width = tex.width;
        srcLoc.PlacedFootprint.Footprint.Height = tex.height;
        srcLoc.PlacedFootprint.Footprint.Depth = 1;
        srcLoc.PlacedFootprint.Footprint.RowPitch = tex.RowPitch;

        D3D12_TEXTURE_COPY_LOCATION dstLoc{};
        dstLoc.pResource = tex.defaultBuffer.Get();
        dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex = 0;

        cmdList.CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Format = tex.format;
        device->CreateShaderResourceView(tex.defaultBuffer.Get(), &srvDesc, rendererCtx.offsetSRV(m_srvHandle, texIndex).cpuAddr);

        texIndex++;
    }

    if (barrierTransition)
    {
        std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
        barriers.reserve(m_textures.size());

        for (NSTexture::Texture& tex : m_textures)
        {
            barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
                tex.defaultBuffer.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
            ));
        }

        cmdList.ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
    }

    m_isOnGPU = true;
}

const char* Material::TextureTypeToString(NSTexture::EType tType)
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
const wchar_t* Material::TextureTypeToWString(NSTexture::EType tType)
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
DXGI_FORMAT Material::FormatTOtype(NSTexture::EType tType)
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
    case NSTexture::EType::EType_HEIGHT:
    case NSTexture::EType::EType_DISPLACEMENT:
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

    case NSTexture::EType::EType_UNKNOWN:
    case NSTexture::EType::EType_NONE:

    default:
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    }
}
