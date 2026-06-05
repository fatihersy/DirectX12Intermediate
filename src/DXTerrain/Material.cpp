#include "stdafx.h"
#include "Material.h"
#include "DXSampleHelper.h"

Material::Material(IWICImagingFactory2* wicFactory) : m_wicFactory(wicFactory)
{}

HRESULT Material::LoadTexture(ID3D12Device14* device, IWICBitmapDecoder* decoder, INT textureType)
{
    NSTexture::EType tType = static_cast<NSTexture::EType>(textureType);

    ASSERT(device and decoder and tType > NSTexture::EType::EType_NONE and tType < NSTexture::EType::EType_MAX);

    const std::wstring_view textureName = m_name.empty()
        ? std::wstring_view(L"Material")
        : std::wstring_view(m_name.data(), m_name.size());

    try
    {
        m_textures.emplace_back(NSTexture::LoadTextureWIC(textureName, decoder, tType));
        m_textures.back().UnloadCPU();
    }
    catch (const HrException& e)
    {
        g_FError("Failed to load material texture\n");
        return e.Error();
    }
    catch (...)
    {
        g_FError("Failed to load material texture\n");
        return E_FAIL;
    }

    m_isOnCPU = true;

    SetTextureFlag(tType);

    return S_OK;
}

void Material::UploadGPU(ID3D12Device14* device, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList, bool barrierTransition)
{
    ASSERT(device);

    ASSERT(not m_textures.empty() and m_isOnCPU, "Textures are not ready to upload to GPU");

    std::for_each(m_textures.begin(), m_textures.end(), [](NSTexture::Texture& tex)
    {
        ASSERT(tex.defaultBuffer and tex.uploadBuffer);
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

    m_srvHandle = rendererCtx.allocSRVStatic(static_cast<uint32_t>(NSTexture::EType::EType_MAX));

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> srcRange(static_cast<size_t>(NSTexture::EType::EType_MAX), rendererCtx.fallbackSRV.get().cpuAddr);

    auto destRange = ArraySequence<static_cast<size_t>(NSTexture::EType::EType_MAX)>([&](auto I)
    {
        return rendererCtx.offsetSRV(m_srvHandle, static_cast<uint32_t>(I)).cpuAddr;
    });

    device->CopyDescriptors(
        destRange.size(),
        destRange.data(),
        nullptr,
        srcRange.size(),
        srcRange.data(),
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
        srcLoc.PlacedFootprint.Footprint.Format = tex.desc.format;
        srcLoc.PlacedFootprint.Footprint.Width = tex.width;
        srcLoc.PlacedFootprint.Footprint.Height = tex.height;
        srcLoc.PlacedFootprint.Footprint.Depth = 1;
        srcLoc.PlacedFootprint.Footprint.RowPitch = tex.uploadRowPitch;

        D3D12_TEXTURE_COPY_LOCATION dstLoc{};
        dstLoc.pResource = tex.defaultBuffer.Get();
        dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex = 0;

        cmdList.CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Format = tex.desc.format;
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
