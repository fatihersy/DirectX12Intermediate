#include "stdafx.h"
#include "Material.h"

#include "DXSampleHelper.h"

#include "IApp.h"


Material::Material(IWICImagingFactory2* wicFactory) : m_isOnCPU{}, m_isOnGPU{} {
    m_wicFactory = wicFactory;
}

HRESULT Material::LoadTexture(ID3D12Device* device, IWICBitmapDecoder* decoder, INT tType)
{
    if (tType >= static_cast<INT>(FTextureType::FTextureType_MAX) or not device or not decoder)
        return E_FAIL;

    FTexture& tex = m_textures.emplace_back(FTexture());
    tex.textureType = static_cast<FTextureType>(tType);
    tex.format = FormatTOtype(tex.textureType);

    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) {
        g_FError("Failed to get frame from decoder\n");
        return E_FAIL;
    }

    if (FAILED(frame->GetSize(&tex.width, &tex.height))) {
        g_FError("Failed to get texture dimensions\n");
        return E_FAIL;
    }

    const UINT bpp = 4;
    const UINT alignment = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
    UINT rowPitch = (static_cast<UINT64>(tex.width) * bpp + alignment - 1) & ~(alignment - 1); // round up to the next multiple of alignment
    UINT uploadSize = rowPitch * tex.height;
    tex.RowPitch = rowPitch;

    ComPtr<IWICFormatConverter> converter;
    if (FAILED(m_wicFactory->CreateFormatConverter(&converter))) {
        g_FError("Failed to create format converter\n");
        return E_FAIL;
    }
    
    if (FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.f, WICBitmapPaletteTypeCustom))) {
        g_FError("Failed to initialize format converter\n");
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
        IID_PPV_ARGS(&tex.uploadBuffer))))
    {
        g_FError("Failed to create upload buffer\n");
        return E_FAIL;
    }


    void* pMappedData = nullptr;
    if (FAILED(tex.uploadBuffer->Map(0, nullptr, &pMappedData))) 
    {
        g_FError("Failed to map upload buffer\n");
        return E_FAIL;
    }
    
    if (FAILED(converter->CopyPixels(nullptr, rowPitch, uploadSize, reinterpret_cast<BYTE*>(pMappedData))))
    {
        tex.uploadBuffer->Unmap(0, nullptr);
        g_FError("Failed to copy pixels\n");
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
        texDesc.SampleDesc.Quality = 0;
        texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        D3D12_HEAP_PROPERTIES defaultHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

        if (FAILED(device->CreateCommittedResource(
            &defaultHeapProp,
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&tex.defaultBuffer))))
        {
            g_FError("Failed to create default resource heap\n");
            return E_FAIL;
        }

        tex.defaultBuffer->SetName(FString::wformat("%s::%s::defaultBuffer", m_name, TextureTypeToString(tex.textureType)).c_str());
        tex.uploadBuffer->SetName(FString::wformat("%s::%s::uploadBuffer", m_name, TextureTypeToString(tex.textureType)).c_str());
    }

    m_isOnCPU = true;

    m_textureFlags |= ( 1u << static_cast<UINT>(tType));

    return S_OK;
}

void Material::Bind(ID3D12GraphicsCommandList* cmdList) const
{
    if (not m_isOnGPU) return;

    cmdList->SetGraphicsRootDescriptorTable(2, m_baseGPUhandle);
    return;
}

void Material::UploadGPU(ID3D12Device* device, ID3D12CommandQueue* cmdQueue, ID3D12GraphicsCommandList* cmdList)
{
    if (not device or not cmdQueue or not cmdList) {
        g_FError("At least one of the parameters are invalid\n");
        return;
    }
    if (m_textures.empty()) return;
    
    bool invalidTexture = false;
    std::for_each(m_textures.begin(), m_textures.end(), [&invalidTexture](FTexture& tex) {
        if (not tex.defaultBuffer or not tex.uploadBuffer) {
            invalidTexture = true;
            return;
        }
    });
    if (invalidTexture) {
        g_FError("There is an invalid texture in the material\n");
        return;
    }

    IApp* appInfo = IApp::GetInstance();

    CD3DX12_CPU_DESCRIPTOR_HANDLE baseCpuHandle;
    CD3DX12_GPU_DESCRIPTOR_HANDLE baseGpuHandle;

    appInfo->modelSrvAlloc(&baseCpuHandle, &baseGpuHandle, static_cast<INT>(FTextureType::FTextureType_MAX));

    for (UINT i = 0; i < static_cast<UINT>(FTextureType::FTextureType_MAX); i++) // Binding the fallback texture
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE slotHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(baseCpuHandle, i, appInfo->GetModelSrvDescriptorSize());
        device->CopyDescriptorsSimple(1, slotHandle, appInfo->im_fallbackTextureCpuHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    std::vector<CD3DX12_RESOURCE_BARRIER> barriers;

    for (FTexture& tex : m_textures)
    {
        barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
            tex.defaultBuffer.Get(),
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COPY_DEST
        ));
    }

    cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
    barriers.clear();

    for (FTexture& tex : m_textures)
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

        cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

        barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
            tex.defaultBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        ));
        
        tex.cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(baseCpuHandle, static_cast<INT>(tex.textureType), appInfo->GetModelSrvDescriptorSize());
        tex.gpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(baseGpuHandle, static_cast<INT>(tex.textureType), appInfo->GetModelSrvDescriptorSize());

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Format = tex.format;
        device->CreateShaderResourceView(tex.defaultBuffer.Get(), &srvDesc, tex.cpuHandle);
    }

    if (not barriers.empty())
    {
        cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
    }

    m_isOnGPU = true;

    m_baseGPUhandle = baseGpuHandle;
}

void Material::UnloadGPU()
{
    if (not m_isOnGPU)
    {
        g_FError("GPU resource is already empty");
        return;
    }

    for (FTexture& texture : m_textures)
    {
        IApp::GetInstance()->modelSrvFree(texture.cpuHandle, texture.gpuHandle);
        texture.defaultBuffer.Reset();
    }

    m_isOnGPU = false;
}

void Material::ResetUploadHeaps()
{
    if (not m_isOnCPU)
    {
        g_FError("No CPU resoruce");
        return;
    }

    for (FTexture& texture : m_textures)
    {
        texture.uploadBuffer.Reset();
    }

    m_isOnCPU = false;
}

const char* Material::TextureTypeToString(FTextureType tType)
{
    switch (tType)
    {
        case FTextureType::FTextureType_DIFFUSE: return "Diffuse";
        case FTextureType::FTextureType_SPECULAR: return "Specular";
        case FTextureType::FTextureType_AMBIENT: return "Ambient";
        case FTextureType::FTextureType_EMISSIVE: return "Emissive";
        case FTextureType::FTextureType_HEIGHT: return "Height";
        case FTextureType::FTextureType_NORMALS: return "Normals";
        case FTextureType::FTextureType_SHININESS: return "Shininess";
        case FTextureType::FTextureType_OPACITY: return "Opacity";
        case FTextureType::FTextureType_DISPLACEMENT: return "Displacement";
        case FTextureType::FTextureType_LIGHTMAP: return "Lightmap";
        case FTextureType::FTextureType_REFLECTION: return "Reflection";
        case FTextureType::FTextureType_BASE_COLOR: return "Base Color";
        case FTextureType::FTextureType_NORMAL_CAMERA: return "Normal Camera";
        case FTextureType::FTextureType_EMISSION_COLOR: return "Emission Color";
        case FTextureType::FTextureType_METALNESS: return "Metalness";
        case FTextureType::FTextureType_DIFFUSE_ROUGHNESS: return "Diffuse Roughness";
        case FTextureType::FTextureType_AMBIENT_OCCLUSION: return "Ambient Occlusion";
        case FTextureType::FTextureType_GLTF_METALLIC_ROUGHNESS: return "GLTF Metallic Roughness";
        default: return "Unknown";
    }
}
