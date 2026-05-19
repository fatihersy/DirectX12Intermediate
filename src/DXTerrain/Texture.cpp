#include "stdafx.h"

#include "IApp.h"
#include "DXSampleHelper.h"

namespace NSTexture
{
    Texture LoadTexture(std::wstring_view name, std::wstring_view path, EType type)
    {
        return LoadTexture(name, path, GetTextureDesc(type));
    }

    Texture LoadTexture(std::wstring_view name, std::wstring_view path, LoadTextureDesc desc)
    {
        IWICImagingFactory2* wicFactory = IApp::GetInstance()->im_wicFactory.Get();
        ASSERT(wicFactory);

        ComPtr<IWICBitmapDecoder> decoder;
        std::wstring filepath(IApp::GetInstance()->im_assetsPath.generic_wstring().append(L"/").append(path));
        ThrowIfFailed(wicFactory->CreateDecoderFromFilename(filepath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder));

        return LoadTexture(name, decoder.Get(), desc);
    }

    Texture LoadTexture(std::wstring_view name, IWICBitmapDecoder* decoder, EType type)
    {
        return LoadTexture(name, decoder, GetTextureDesc(type));
    }

    Texture LoadTexture(std::wstring_view name, IWICBitmapDecoder* decoder, LoadTextureDesc desc)
    {
        ASSERT(static_cast<int>(desc.textureType) < static_cast<int>(EType::EType_MAX));
        ASSERT(desc.format != DXGI_FORMAT_UNKNOWN);
        ASSERT(desc.bytesPerPixel > 0u);
        ASSERT(desc.mipLevels == 1u, "LoadTexture currently uploads only the first mip");
        ASSERT(decoder);

        IWICImagingFactory2* wicFactory = IApp::GetInstance()->im_wicFactory.Get();
        ID3D12Device14* device = IApp::GetInstance()->im_device.Get();

        ASSERT(wicFactory and device);

        Texture tex{};
        tex.textureType = desc.textureType;
        tex.format = desc.format;

        ComPtr<IWICBitmapFrameDecode> frame;
        ThrowIfFailed(decoder->GetFrame(0, &frame));
        ThrowIfFailed(frame->GetSize(&tex.width, &tex.height));

        const UINT alignment = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
        const UINT64 unalignedRowPitch = static_cast<UINT64>(tex.width) * desc.bytesPerPixel;
        const UINT64 alignedRowPitch = (unalignedRowPitch + alignment - 1u) & ~(static_cast<UINT64>(alignment) - 1u);
        const UINT64 uploadSize64 = alignedRowPitch * tex.height;

        ASSERT(alignedRowPitch <= UINT_MAX);
        ASSERT(uploadSize64 <= UINT_MAX);

        const UINT rowPitch = static_cast<UINT>(alignedRowPitch);
        const UINT uploadSize = static_cast<UINT>(uploadSize64);
        tex.RowPitch = rowPitch;

        ComPtr<IWICFormatConverter> converter;
        ThrowIfFailed(wicFactory->CreateFormatConverter(&converter));

        ThrowIfFailed(converter->Initialize(
            frame.Get(),
            desc.wicPixelFormat,
            desc.dither,
            desc.palette,
            desc.alphaThresholdPercent,
            desc.paletteTranslate
        ));

        std::wstring_view texTypeStr(TextureTypeToWString(tex.textureType));

        if (not tex.uploadBuffer)
        {
            D3D12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
            D3D12_HEAP_PROPERTIES uploadHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

            ThrowIfFailed(device->CreateCommittedResource(
                &uploadHeapProp,
                D3D12_HEAP_FLAG_NONE,
                &uploadBufferDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&tex.uploadBuffer)
            ));

            std::wstring uplbufname(NSTool::wformat(L"%s::%s::uploadBuffer", name, texTypeStr));
            tex.uploadBuffer->SetName(uplbufname.c_str());
        }

        if (desc.keepCpuData)
        {
            tex.cpuData.resize(uploadSize);
            ThrowIfFailed(converter->CopyPixels(nullptr, rowPitch, uploadSize, reinterpret_cast<BYTE*>(tex.cpuData.data())));

            void* pMappedData = nullptr;
            ThrowIfFailed(tex.uploadBuffer->Map(0, nullptr, &pMappedData));
            memcpy(pMappedData, tex.cpuData.data(), uploadSize);
            tex.uploadBuffer->Unmap(0, nullptr);
        }
        else
        {
            void* pMappedData = nullptr;
            ThrowIfFailed(tex.uploadBuffer->Map(0, nullptr, &pMappedData));
            ThrowIfFailed(converter->CopyPixels(nullptr, rowPitch, uploadSize, reinterpret_cast<BYTE*>(pMappedData)));
            tex.uploadBuffer->Unmap(0, nullptr);
        }

        if (tex.uploadBuffer and tex.width > 0 and tex.height > 0)
        {
            if (not tex.defaultBuffer)
            {
                D3D12_RESOURCE_DESC texDesc{};
                texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                texDesc.Width = tex.width;
                texDesc.Height = tex.height;
                texDesc.DepthOrArraySize = 1;
                texDesc.MipLevels = desc.mipLevels;
                texDesc.Format = tex.format;
                texDesc.SampleDesc.Count = 1;
                texDesc.SampleDesc.Quality = 0;
                texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
                texDesc.Flags = desc.resourceFlags;

                D3D12_HEAP_PROPERTIES defaultHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

                ThrowIfFailed(device->CreateCommittedResource(
                    &defaultHeapProp,
                    D3D12_HEAP_FLAG_NONE,
                    &texDesc,
                    desc.initialState,
                    nullptr,
                    IID_PPV_ARGS(&tex.defaultBuffer)
                ));
                std::wstring defbufname(NSTool::wformat(L"%s::%s::defaultBuffer", name, texTypeStr));
                tex.defaultBuffer->SetName(defbufname.c_str());
            }
        }

        tex.m_isOnCPU = true;
        tex.desc = desc;
        return tex;
    }

    void UploadGPU(NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList, NSTexture::Texture& texture, bool createSRV, bool barrierTransition, D3D12_RESOURCE_STATES stateAfter)
    {
        ID3D12Device14* device = IApp::GetInstance()->im_device.Get();
        ASSERT(device);

        ASSERT(texture.m_isOnCPU and texture.uploadBuffer, "Trying to upload texture without loading first");
        ASSERT(texture.defaultBuffer, "Texture has no default resource");
        ASSERT(not texture.m_isOnGPU, "Texture is already uploaded");

        if (barrierTransition and texture.desc.initialState != D3D12_RESOURCE_STATE_COPY_DEST)
        {
            CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                texture.defaultBuffer.Get(),
                texture.desc.initialState,
                D3D12_RESOURCE_STATE_COPY_DEST
            );
            cmdList.ResourceBarrier(1u, &barrier);
        }

        {
            D3D12_TEXTURE_COPY_LOCATION srcLoc{};
            srcLoc.pResource = texture.uploadBuffer.Get();
            srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            srcLoc.PlacedFootprint.Offset = 0;
            srcLoc.PlacedFootprint.Footprint.Format = texture.format;
            srcLoc.PlacedFootprint.Footprint.Width = texture.width;
            srcLoc.PlacedFootprint.Footprint.Height = texture.height;
            srcLoc.PlacedFootprint.Footprint.Depth = 1;
            srcLoc.PlacedFootprint.Footprint.RowPitch = texture.RowPitch;

            D3D12_TEXTURE_COPY_LOCATION dstLoc{};
            dstLoc.pResource = texture.defaultBuffer.Get();
            dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dstLoc.SubresourceIndex = 0;

            cmdList.CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

            if (createSRV)
            {
                ASSERT(texture.srvOffset.index != UINT32_MAX and texture.srvOffset.cpuAddr.ptr);

                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
                srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Texture2D.MipLevels = texture.desc.mipLevels;
                srvDesc.Format = texture.format;

                device->CreateShaderResourceView(texture.defaultBuffer.Get(), &srvDesc, texture.srvOffset.cpuAddr);
            }
        }

        if (barrierTransition and stateAfter != D3D12_RESOURCE_STATE_COPY_DEST)
        {
            CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                texture.defaultBuffer.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                stateAfter
            );
            cmdList.ResourceBarrier(1u, &barrier);
        }

        texture.m_isOnGPU = true;
    }
}
