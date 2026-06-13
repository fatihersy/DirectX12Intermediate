#include "stdafx.h"

#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfFrameBuffer.h>
#include <OpenEXR/ImfChannelList.h>
#include <Imath/half.h>

#include "IApp.h"
#include "DXSampleHelper.h"

template<typename T>
T ToUNORM(float v)
{
    return static_cast<T>(std::clamp(v, 0.f, 1.f) * std::numeric_limits<T>::max() + 0.5f);
};

namespace NSTexture
{
    EChannel ChannelDeduction(unsigned char name)
    {
        for (uint32_t idx = ChFront; idx < static_cast<uint32_t>(ChEnd); idx++)
        {
            if (std::toupper(name) == channelNames[idx][0])
            {
               return static_cast<EChannel>(idx);
            }
        }
        return ChUnknown;
    };

    TextureMetadata ProbeTexture(std::filesystem::path path)
    {
        if (not path.is_absolute())
        {
            path = IApp::GetInstance()->im_assetsPath / path;
        }
        if (not std::filesystem::is_regular_file(path)) return {};

        std::string pathStr = path.generic_string();

        size_t dotPos = pathStr.find_last_of('.');
        ASSERT(dotPos != pathStr.npos, "Extension extraction failed from the file");

        TextureMetadata metadata{};

        if (strcmp(pathStr.c_str() + dotPos, ".exr") == 0)
        {
            Imf::InputFile file(pathStr.c_str());

            const Imath::Box2i dataWindow = file.header().dataWindow();

            metadata.width = dataWindow.max.x - dataWindow.min.x + 1;
            metadata.height = dataWindow.max.y - dataWindow.min.y + 1;

            Imf::ChannelList::ConstIterator itr = file.header().channels().begin();
            const Imf::ChannelList::ConstIterator chEnd = file.header().channels().end();
            for (; itr != chEnd ;)
            {
                metadata.channels.push_back(ChannelDeduction( (*itr.name()) ));

                ASSERT(metadata.channels.back() != ChUnknown);
                ASSERT(itr.channel().xSampling == 1 and itr.channel().ySampling == 1);

                itr++;
            }
        }
        else
        {
            ASSERT(false, "Feature probing textures via WIC didn't implemented yet");
        }

        metadata.success = true;
        return metadata;
    }

    UINT CalculateRowPitch(UINT width, UINT bytesPerPixel, NSTexture::ERowPitchMode mode)
    {
        const UINT64 tight = static_cast<UINT64>(width) * bytesPerPixel;

        switch (mode)
        {
            case NSTexture::ERowPitchMode::DX_ALIGN:
            {
                const UINT64 alignment = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
                const UINT64 dxAligned = (tight + alignment - 1u) & ~(alignment - 1u);
                ASSERT(dxAligned < std::numeric_limits<UINT>::max());
                return static_cast<UINT>(dxAligned);
            }
            case NSTexture::ERowPitchMode::TIGHT:
            {
                ASSERT(tight < std::numeric_limits<UINT>::max());
                return static_cast<UINT>(tight);
            }
            default: {
                ASSERT(false, "Undefined mode");
                return std::numeric_limits<UINT>::max();
            }
        }
    }
    Texture LoadTexture(std::wstring_view name, std::filesystem::path path, EType type)
    {
        return LoadTextureWIC(name, path, GetWICTextureDesc(type));
    }
    Texture LoadTexture(std::wstring_view name, std::filesystem::path path, EXRLoadDesc desc)
    {
        return LoadTextureEXR(name, path, desc);
    }

    Texture LoadTextureWIC(std::wstring_view name, IWICBitmapDecoder* decoder, EType type)
    {
        return LoadTextureWIC(name, decoder, GetWICTextureDesc(type));
    }
    Texture LoadTextureWIC(std::wstring_view name, std::filesystem::path path, WICLoadDesc desc)
    {
        IWICImagingFactory2* wicFactory = IApp::GetInstance()->im_wicFactory.Get();
        ASSERT(wicFactory);

        ComPtr<IWICBitmapDecoder> decoder;

        if (not path.is_absolute())
        {
            path = IApp::GetInstance()->im_assetsPath / path;
        }

        ThrowIfFailed(wicFactory->CreateDecoderFromFilename(path.generic_wstring().c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder));

        if (desc.texDesc.format == DXGI_FORMAT_UNKNOWN)
        {
            desc.texDesc.format = TypeToFormat(desc.texDesc.textureType);
        }
        desc.texDesc.bytesPerPixel = BytesPerPixel(desc.texDesc.format);

        return LoadTextureWIC(name, decoder.Get(), desc);
    }
    Texture LoadTextureWIC(std::wstring_view name, IWICBitmapDecoder* decoder, WICLoadDesc desc)
    {
        ASSERT(static_cast<int>(desc.texDesc.textureType) < static_cast<int>(EType::EType_MAX));
        ASSERT(desc.texDesc.format != DXGI_FORMAT_UNKNOWN);
        ASSERT(desc.texDesc.mipLevels == 1u, "LoadTexture currently uploads only the first mip");
        ASSERT(desc.texDesc.bytesPerPixel == BytesPerPixel(desc.texDesc.format));
        ASSERT(decoder);
        ASSERT(desc.texDesc.localRowPitchMode != ERowPitchMode::UNDEFINED);

        IWICImagingFactory2* wicFactory = IApp::GetInstance()->im_wicFactory.Get();
        ID3D12Device14* device = IApp::GetInstance()->im_device.Get();

        ASSERT(wicFactory and device);

        Texture tex{};
        tex.name = name;

        ComPtr<IWICBitmapFrameDecode> frame;
        ThrowIfFailed(decoder->GetFrame(0, &frame));
        ThrowIfFailed(frame->GetSize(&tex.width, &tex.height));

        tex.localRowPitch = CalculateRowPitch(tex.width, desc.texDesc.bytesPerPixel, desc.texDesc.localRowPitchMode);
        tex.uploadRowPitch = CalculateRowPitch(tex.width, desc.texDesc.bytesPerPixel, desc.texDesc.uploadRowPitchMode);

        ComPtr<IWICFormatConverter> converter;
        ThrowIfFailed(wicFactory->CreateFormatConverter(&converter));

        ThrowIfFailed(converter->Initialize(
            frame.Get(),
            desc.wicPixelFormat,
            desc.dither,
            desc.palette,
            desc.texDesc.alphaThresholdPercent,
            desc.paletteTranslate
        ));

        const UINT totalChunkCount = (tex.height + ROWS_AT_A_TIME - 1u) / ROWS_AT_A_TIME;
        tex.chunks.assign(totalChunkCount, {});
        for (UINT idx{}; idx < totalChunkCount; idx++)
        {
            TextureChunk& chunk = tex.chunks[idx];

            chunk.firstRow = idx * ROWS_AT_A_TIME;
            chunk.rowCount = std::min(ROWS_AT_A_TIME, tex.height - chunk.firstRow);
            chunk.bytes.assign(static_cast<size_t>(chunk.rowCount) * tex.localRowPitch, {});

            WICRect rect{
                .X = 0,
                .Y = static_cast<INT>(chunk.firstRow),
                .Width = static_cast<INT>(tex.width),
                .Height = static_cast<INT>(chunk.rowCount)
            };

            ThrowIfFailed(converter->CopyPixels(&rect, tex.localRowPitch, static_cast<UINT>(chunk.bytes.size()), reinterpret_cast<BYTE*>(chunk.bytes.data())));
        }

        tex.desc = desc.texDesc;
        return tex;
    }

    Texture LoadTextureEXR(std::wstring_view name, std::filesystem::path path, EXRLoadDesc desc)
    {
        ASSERT(static_cast<int>(desc.texDesc.textureType) < static_cast<int>(EType::EType_MAX));
        ASSERT(desc.texDesc.localRowPitchMode != ERowPitchMode::UNDEFINED);

        if (desc.texDesc.format == DXGI_FORMAT_UNKNOWN)
        {
            desc.texDesc.format = TypeToFormat(desc.texDesc.textureType);
        }

        ASSERT(desc.texDesc.mipLevels == 1u, "LoadTextureEXR currently uploads only the first mip");

        ID3D12Device14* device = IApp::GetInstance()->im_device.Get();
        ASSERT(device);

        desc.texDesc.bytesPerPixel = BytesPerPixel(desc.texDesc.format);
        const UINT componentCount = ComponentCount(desc.texDesc.format);

        if (not path.is_absolute())
        {
            path = IApp::GetInstance()->im_assetsPath / path;
        }

        Imf::InputFile file(path.generic_string().c_str());

        const Imath::Box2i dataWindow = file.header().dataWindow();

        ASSERT(
            (dataWindow.max.x - dataWindow.min.x + 1) > 0
            and
            (dataWindow.max.y - dataWindow.min.y + 1) > 0
        );

        const UINT width = static_cast<UINT>(dataWindow.max.x - dataWindow.min.x + 1);
        const UINT height = static_cast<UINT>(dataWindow.max.y - dataWindow.min.y + 1);

        const UINT totalChunkCount = (height + ROWS_AT_A_TIME - 1u) / ROWS_AT_A_TIME;

        Texture tex{};
        tex.name = name;
        tex.desc = desc.texDesc;
        tex.width = width;
        tex.height = height;
        tex.chunks.assign(totalChunkCount, {});

        tex.localRowPitch = CalculateRowPitch(tex.width, desc.texDesc.bytesPerPixel, desc.texDesc.localRowPitchMode);
        tex.uploadRowPitch = CalculateRowPitch(tex.width, desc.texDesc.bytesPerPixel, desc.texDesc.uploadRowPitchMode);

        std::unordered_map<std::string_view, std::pair<float, std::vector<float>>> channels{};

        for (UINT component{}; component < componentCount; ++component)
        {
            std::string_view channelName = desc.channels[component];

            if (channels.contains(channelName.data()) or channelName.empty()) continue;

            const Imf::Channel* channel = file.header().channels().findChannel(channelName.data());

            ASSERT(channel, "Channel does not exist");
            ASSERT(channel->xSampling == 1 and channel->ySampling == 1, "Subsampled EXR channels are not supported");

            auto& [defaultVal, vec] = channels[channelName.data()];

            defaultVal = desc.defaultValues[component];
        }

        ASSERT(not channels.empty(), "No channels to load");

        auto ReadComponent = [&channels, &desc](UINT component, size_t pixelIndex)
        {
            if (channels.contains(desc.channels[component])) return channels[desc.channels[component]].second[pixelIndex];

            return desc.defaultValues[component];
        };

        auto WritePixelData = [&desc, &tex, &width, &ReadComponent](UINT x, size_t absoluteY, size_t relativeY, UINT absoluteRow, UINT relativeRow, TextureChunk& chunk)
        {
            std::byte* dstRow = chunk.bytes.data() + static_cast<size_t>(relativeRow) * tex.localRowPitch;

            switch (desc.texDesc.format)
            {
                case DXGI_FORMAT::DXGI_FORMAT_R8_UNORM: {
                    reinterpret_cast<uint8_t*>(dstRow)[x] = ToUNORM<uint8_t>(ReadComponent(0u, relativeY));
                    break;
                }
                case DXGI_FORMAT::DXGI_FORMAT_R16_UNORM: {
                    reinterpret_cast<uint16_t*>(dstRow)[x] = ToUNORM<uint16_t>(ReadComponent(0u, relativeY));
                    break;
                }
                case DXGI_FORMAT::DXGI_FORMAT_R16_FLOAT: {
                    reinterpret_cast<Imath::half*>(dstRow)[x] = Imath::half(ReadComponent(0u, relativeY));
                    break;
                }
                case DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT: {
                    reinterpret_cast<float*>(dstRow)[x] = ReadComponent(0u, relativeY);
                    break;
                }
                case DXGI_FORMAT::DXGI_FORMAT_R16G16_FLOAT: {
                    Imath::half* dst = reinterpret_cast<Imath::half*>(dstRow) + static_cast<size_t>(x) * 2u;
                    dst[0] = Imath::half(ReadComponent(0u, relativeY));
                    dst[1] = Imath::half(ReadComponent(1u, relativeY));
                    break;
                }
                case DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT: {
                    float* dst = reinterpret_cast<float*>(dstRow) + static_cast<size_t>(x) * 2u;
                    dst[0] = ReadComponent(0u, relativeY);
                    dst[1] = ReadComponent(1u, relativeY);
                    break;
                }
                case DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM:
                case DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: {
                    uint8_t* dst = reinterpret_cast<uint8_t*>(dstRow) + static_cast<size_t>(x) * 4u;
                    dst[0] = ToUNORM<uint8_t>(ReadComponent(0u, relativeY));
                    dst[1] = ToUNORM<uint8_t>(ReadComponent(1u, relativeY));
                    dst[2] = ToUNORM<uint8_t>(ReadComponent(2u, relativeY));
                    dst[3] = ToUNORM<uint8_t>(ReadComponent(3u, relativeY));
                    break;
                }
                case DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_FLOAT: {
                    Imath::half* dst = reinterpret_cast<Imath::half*>(dstRow) + static_cast<size_t>(x) * 4u;
                    dst[0] = Imath::half(ReadComponent(0u, relativeY));
                    dst[1] = Imath::half(ReadComponent(1u, relativeY));
                    dst[2] = Imath::half(ReadComponent(2u, relativeY));
                    dst[3] = Imath::half(ReadComponent(3u, relativeY));
                    break;
                }
                case DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT: {
                    float* dst = reinterpret_cast<float*>(dstRow) + static_cast<size_t>(x) * 4u;
                    dst[0] = ReadComponent(0u, relativeY);
                    dst[1] = ReadComponent(1u, relativeY);
                    dst[2] = ReadComponent(2u, relativeY);
                    dst[3] = ReadComponent(3u, relativeY);
                    break;
                }
                default: ASSERT(false, "Unsupported format");
            }
        };

        for (UINT chunkIdx{}; chunkIdx < totalChunkCount; chunkIdx++)
        {
            const UINT rowStart = chunkIdx * ROWS_AT_A_TIME;
            const UINT rowsToRead = std::min(ROWS_AT_A_TIME, height - rowStart);
            const size_t pixelCountToRead = static_cast<size_t>(width) * rowsToRead;

            TextureChunk& chunk = tex.chunks[chunkIdx];
            chunk.firstRow = rowStart;
            chunk.rowCount = rowsToRead;
            chunk.bytes.assign(rowsToRead * tex.localRowPitch, {});

            for (auto& component : channels)
            {
                auto& [channelName, channel] = component;
                auto& [defaultValue, pixels] = channel;
                pixels.assign(pixelCountToRead, {});
            }

            Imf::FrameBuffer frameBuffer;
            const int fileYBegin = dataWindow.min.y + static_cast<int>(rowStart);
            const int fileYEnd = fileYBegin + static_cast<int>(rowsToRead) - 1;

            for (auto& [channelName, channel] : channels)
            {
                auto& [defaultVal, pixels] = channel;
                float* base = pixels.data();
                char* baseDiff =
                    reinterpret_cast<char*>(base) -
                    static_cast<ptrdiff_t>(dataWindow.min.x) * sizeof(float) -
                    static_cast<ptrdiff_t>(fileYBegin) * sizeof(float) * width;

                frameBuffer.insert(
                    channelName.data(),
                    Imf::Slice(Imf::FLOAT, baseDiff, sizeof(float), sizeof(float) * width)
                );
            }

            file.setFrameBuffer(frameBuffer);
            file.readPixels(fileYBegin, fileYEnd);

            for (UINT row{}; row < rowsToRead; ++row)
            {
                const UINT relativeRow = row;
                const UINT absoluteRow = rowStart + relativeRow;

                for (UINT col{}; col < width; ++col)
                {
                    const size_t relativeY = static_cast<size_t>(relativeRow) * width + col;
                    const size_t absoluteY = static_cast<size_t>(absoluteRow) * width + col;

                    WritePixelData(col, absoluteY, relativeY, absoluteRow, relativeRow, chunk);
                }
            }
        }

        return tex;
    }
    Texture&& Texture::PopulateCPU(bool consumeLocalData)
    {
        ID3D12Device14* device = IApp::GetInstance()->im_device.Get();
        ASSERT(device, "Device is not valid");
        ASSERT(not chunks.empty(), "No data to populate upload buffers");
        ASSERT(not m_isOnCPU, "Texture Upload buffer is already populated");
        ASSERT(desc.bytesPerPixel > 0u);
        ASSERT(localRowPitch > 0u);
        ASSERT(uploadRowPitch > 0u);

        const size_t uploadSize = uploadRowPitch * height;

        CD3DX12_RESOURCE_DESC resDesc(CD3DX12_RESOURCE_DESC::Buffer(uploadSize));
        CD3DX12_HEAP_PROPERTIES props(D3D12_HEAP_TYPE_UPLOAD);

        if (not uploadBuffer) {
            ThrowIfFailed(device->CreateCommittedResource(
                &props,
                D3D12_HEAP_FLAG_NONE,
                &resDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&uploadBuffer)
            ));
            if (not name.empty()) {
                std::wstring_view texTypeStr(TextureTypeToWString(desc.textureType));

                std::wstring uploadName(NSTool::wformat(L"%s::%s::uploadBuffer", name, texTypeStr));
                uploadBuffer->SetName(uploadName.c_str());
            }
        }

        std::byte* mappedData = nullptr;
        ThrowIfFailed(uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData)));

        CopyPixels(
            mappedData,
            uploadRowPitch,
            localRowPitch
        );

        if (consumeLocalData) {
            chunks.clear();
            chunks.shrink_to_fit();
        }

        uploadBuffer->Unmap(0, nullptr);
        m_isOnCPU = true;

        return std::move(*this);
    }
    Texture&& Texture::PopulateGPU(NSRenderer::GraphicsCommandList cmdList, bool barrierTransition, D3D12_RESOURCE_STATES stateAfter, SHADER_RESOURCE_VIEW_DESC _srvDesc)
    {
        ID3D12Device14* device = IApp::GetInstance()->im_device.Get();
        ASSERT(device, "Device is not valid");
        ASSERT(m_isOnCPU and uploadBuffer, "Trying to upload texture without loading first");
        ASSERT(not m_isOnGPU, "Texture is already uploaded");
        ASSERT(uploadRowPitch != 0 and uploadRowPitch % D3D12_TEXTURE_DATA_PITCH_ALIGNMENT == 0);

        if(not defaultBuffer) {
            CD3DX12_HEAP_PROPERTIES props(D3D12_HEAP_TYPE_DEFAULT);

            D3D12_RESOURCE_DESC resDesc{};
            resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            resDesc.Width = width;
            resDesc.Height = height;
            resDesc.DepthOrArraySize = 1;
            resDesc.MipLevels = desc.mipLevels;
            resDesc.Format = desc.format;
            resDesc.SampleDesc.Count = 1;
            resDesc.SampleDesc.Quality = 0;
            resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            resDesc.Flags = desc.resourceFlags;

            ThrowIfFailed(device->CreateCommittedResource(
                &props,
                D3D12_HEAP_FLAG_NONE,
                &resDesc,
                desc.initialState,
                nullptr,
                IID_PPV_ARGS(&defaultBuffer)
            ));
            if (not name.empty()) {
                std::wstring_view texTypeStr(TextureTypeToWString(desc.textureType));

                std::wstring defaultName(NSTool::wformat(L"%s::%s::defaultBuffer", name, texTypeStr));
                defaultBuffer->SetName(defaultName.c_str());
            }
        }

        if (barrierTransition and desc.initialState != D3D12_RESOURCE_STATE_COPY_DEST)
        {
            CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                defaultBuffer.Get(),
                desc.initialState,
                D3D12_RESOURCE_STATE_COPY_DEST
            );
            cmdList.ResourceBarrier(1u, &barrier);
        }

        {
            D3D12_TEXTURE_COPY_LOCATION srcLoc{};
            srcLoc.pResource = uploadBuffer.Get();
            srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            srcLoc.PlacedFootprint.Offset = 0;
            srcLoc.PlacedFootprint.Footprint.Format = desc.format;
            srcLoc.PlacedFootprint.Footprint.Width = width;
            srcLoc.PlacedFootprint.Footprint.Height = height;
            srcLoc.PlacedFootprint.Footprint.Depth = 1;
            srcLoc.PlacedFootprint.Footprint.RowPitch = uploadRowPitch;

            D3D12_TEXTURE_COPY_LOCATION dstLoc{};
            dstLoc.pResource = defaultBuffer.Get();
            dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dstLoc.SubresourceIndex = 0;

            cmdList.CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

            ASSERT(srvOffset.index != UINT32_MAX and srvOffset.cpuAddr.ptr);

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = D3D12_SHADER_RESOURCE_VIEW_DESC
            {
                .ViewDimension = _srvDesc.ViewDimension,
                .Shader4ComponentMapping = _srvDesc.Shader4ComponentMapping,
                .Texture2D = _srvDesc.Texture2D
            };
            srvDesc.Format = desc.format;

            device->CreateShaderResourceView(defaultBuffer.Get(), &srvDesc, srvOffset.cpuAddr);
        }

        if (barrierTransition and stateAfter != D3D12_RESOURCE_STATE_COPY_DEST)
        {
            CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                defaultBuffer.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                stateAfter
            );
            cmdList.ResourceBarrier(1u, &barrier);
        }

        m_isOnGPU = true;
        return std::move(*this);
    }
    void Texture::UnloadCPU()
    {
        uploadBuffer->Release();
        m_isOnCPU = false;
    }
    void Texture::UnloadGPU()
    {
        defaultBuffer->Release();
        m_isOnGPU = false;
    }

    void Texture::CopyPixels(std::byte* dst, size_t dstRowPitch, size_t bytesPerRow, bool consumeLocalData)
    {
        ASSERT(dst);
        ASSERT(bytesPerRow <= localRowPitch);
        ASSERT(bytesPerRow <= dstRowPitch);

        for (const TextureChunk& chunk : chunks)
        {
            ASSERT(chunk.firstRow + chunk.rowCount <= height);
            ASSERT(chunk.bytes.size() >= static_cast<size_t>(chunk.rowCount) * localRowPitch);

            if (dstRowPitch == localRowPitch and bytesPerRow == localRowPitch)
            {
                memcpy(
                    dst + static_cast<size_t>(chunk.firstRow) * dstRowPitch,
                    chunk.bytes.data(),
                    static_cast<size_t>(chunk.rowCount) * localRowPitch
                );
            }
            else
            {
                for (UINT row{}; row < chunk.rowCount; ++row)
                {
                    const std::byte* srcRow = chunk.bytes.data() + static_cast<size_t>(row) * localRowPitch;
                    std::byte* dstRow = dst + static_cast<size_t>(chunk.firstRow + row) * dstRowPitch;

                    memcpy(dstRow, srcRow, bytesPerRow);
                }
            }
        }

        if (consumeLocalData)
        {
            chunks.clear();
            chunks.shrink_to_fit();
        }
    }

    void Texture::CopyPixels(std::byte* dst, size_t dstRowPitch, NSMath::SRectU32 cpyRect)
    {
        ASSERT(dst);
        ASSERT(cpyRect.x + cpyRect.width <= width);
        ASSERT(cpyRect.y + cpyRect.height <= height);

        const size_t copyRowBytes = static_cast<size_t>(cpyRect.width) * desc.bytesPerPixel;
        ASSERT(dstRowPitch >= copyRowBytes);
        ASSERT(copyRowBytes <= localRowPitch);

        const uint32_t srcEndY = cpyRect.y + cpyRect.height;

        for (const TextureChunk& chunk : chunks)
        {
            ASSERT(chunk.firstRow + chunk.rowCount <= height);
            ASSERT(chunk.bytes.size() >= static_cast<size_t>(chunk.rowCount) * localRowPitch);

            const uint32_t chunkStartY = chunk.firstRow;
            const uint32_t chunkEndY = chunk.firstRow + chunk.rowCount;

            const uint32_t copyStartY = std::max(cpyRect.y, chunkStartY);
            const uint32_t copyEndY = std::min(srcEndY, chunkEndY);

            if (copyStartY >= copyEndY) continue;

            for (uint32_t y = copyStartY; y < copyEndY; y++)
            {
                const uint32_t srcChunkRow = y - chunk.firstRow;
                const uint32_t dstRow = y - cpyRect.y;

                const std::byte * src = chunk.bytes.data()
                    + static_cast<size_t>(srcChunkRow) * localRowPitch
                    + static_cast<size_t>(cpyRect.x) * desc.bytesPerPixel;

                std::byte * out = dst
                    + static_cast<size_t>(dstRow) * dstRowPitch;

                memcpy(out, src, copyRowBytes);
            }
        }
    }

    void Texture::CopyPixels(std::byte* dst, size_t dstRowPitch, NSMath::SRectU32 cpyRect, ECopyPixelEdgeMode edgeMode)
    {
        ASSERT(dst);
        ASSERT(width > 1u);
        ASSERT(height > 1u);
        ASSERT(cpyRect.width > 0u);
        ASSERT(cpyRect.height > 0u);
        ASSERT(desc.bytesPerPixel > 0u);
        ASSERT(cpyRect.x < width);
        ASSERT(cpyRect.y < height);

        const size_t bpp = desc.bytesPerPixel;
        ASSERT(static_cast<size_t>(cpyRect.width) <= std::numeric_limits<size_t>::max() / bpp);
        ASSERT(static_cast<size_t>(cpyRect.x) <= std::numeric_limits<size_t>::max() / bpp);

        const size_t dstTightRowPitch = static_cast<size_t>(cpyRect.width) * bpp;
        const uint32_t copyWidth = std::min(cpyRect.width, width - cpyRect.x);
        const uint32_t edgePixelCount = cpyRect.width - copyWidth;
        const size_t copyOffsetBytes = static_cast<size_t>(cpyRect.x) * bpp;
        const size_t copyRowBytes = static_cast<size_t>(copyWidth) * bpp;
        const size_t edgeBytes = static_cast<size_t>(edgePixelCount) * bpp;
        const uint32_t sourceRowCount = std::min(cpyRect.height, height - cpyRect.y);
        const uint32_t cpyEndY = cpyRect.y + sourceRowCount;

        ASSERT(dstRowPitch >= dstTightRowPitch);
        ASSERT(static_cast<size_t>(cpyRect.height - 1u) <= (std::numeric_limits<size_t>::max() - dstTightRowPitch) / dstRowPitch);
        ASSERT(copyOffsetBytes <= localRowPitch);
        ASSERT(copyRowBytes <= static_cast<size_t>(localRowPitch) - copyOffsetBytes);
        ASSERT(copyRowBytes + edgeBytes == dstTightRowPitch);

        for (const TextureChunk& chunk : chunks)
        {
            ASSERT(chunk.firstRow < height);
            ASSERT(chunk.rowCount <= height - chunk.firstRow);
            ASSERT(chunk.bytes.size() >= static_cast<size_t>(chunk.rowCount) * localRowPitch);
        }

        auto ResolveSourceY = [this, edgeMode](size_t srcY) -> std::optional<uint32_t>
        {
            if (srcY < height) return static_cast<uint32_t>(srcY);

            switch (edgeMode)
            {
                case ECopyPixelEdgeMode::ZERO: return std::nullopt;

                case ECopyPixelEdgeMode::CLAMP: return height - 1u;

                case ECopyPixelEdgeMode::MIRROR:
                {
                    size_t period = static_cast<size_t>(height) * 2u - 2u;
                    size_t m = srcY % period;
                    return static_cast<uint32_t>(m < height ? m : period - m);
                }
                case ECopyPixelEdgeMode::REPEAT: return static_cast<uint32_t>(srcY % height);

                default: ASSERT(false, "Unsupported copy pixel edge mode");
            }

            return std::nullopt;
        };

        auto FindChunk = [this](uint32_t srcY) -> OptRef<const TextureChunk>
        {
            for (const TextureChunk& chunk : chunks)
            {
                if (srcY >= chunk.firstRow and srcY - chunk.firstRow < chunk.rowCount)
                {
                    return chunk;
                }
            }
            return std::nullopt;
        };

        for (uint32_t dstRow{}; dstRow < cpyRect.height; ++dstRow)
        {
            std::byte * out = dst + static_cast<size_t>(dstRow) * dstRowPitch;
            std::optional<uint32_t> absoluteY = ResolveSourceY(static_cast<size_t>(cpyRect.y) + dstRow);

            if (not absoluteY.has_value())
            {
                memset(out, 0, dstTightRowPitch);
                continue;
            }

            const OptRef<const TextureChunk> chunk = FindChunk(absoluteY.value());
            ASSERT(chunk.has_value());

            const size_t relativeY = absoluteY.value() - chunk.value().get().firstRow;

            const std::byte * chunkStart = chunk.value().get().bytes.data() + relativeY * localRowPitch;
            const std::byte * copyStart = chunkStart + copyOffsetBytes;

            memcpy(out, copyStart, copyRowBytes);

            if (edgePixelCount == 0u) continue;

            std::byte * offEdge = out + copyRowBytes;

            switch (edgeMode)
            {
                case ECopyPixelEdgeMode::ZERO: memset(offEdge, 0, edgeBytes);
                break;

                case ECopyPixelEdgeMode::CLAMP:
                {
                    const std::byte * lastPixel = copyStart + static_cast<size_t>(copyWidth - 1u) * bpp;

                    for (size_t off{}; off < edgePixelCount; off++)
                    {
                        memcpy(offEdge + off * bpp, lastPixel, bpp);
                    }
                    break;
                }
                case ECopyPixelEdgeMode::MIRROR:
                {
                    const size_t period = static_cast<size_t>(width) * 2u - 2u;

                    for (size_t off{}; off < edgePixelCount; off++)
                    {
                        const size_t m = (cpyRect.x + copyWidth + off) % period;
                        const size_t col = m < width ? m : period - m;

                        memcpy(offEdge + off * bpp, chunkStart + col * bpp, bpp);
                    }
                    break;
                }
                case ECopyPixelEdgeMode::REPEAT:
                {
                    for (size_t off{}; off < edgePixelCount; off++)
                    {
                        const size_t srcCol = static_cast<size_t>(cpyRect.x) + copyWidth + off;
                        const size_t col = srcCol % width;

                        memcpy(offEdge + off * bpp, chunkStart + col * bpp, bpp);
                    }
                    break;
                }
            }
        }
    }

    Texture LoadTextureMemory(std::wstring_view name, const std::byte* data, UINT srcRowPitch, size_t dataSize, MemoryLoadDesc desc, OptRef<const std::filesystem::path> path)
    {
        ASSERT(data, "data is invalid");
        ASSERT(desc.width > 0u);
        ASSERT(desc.height > 0u);
        ASSERT(srcRowPitch > 0u);
        ASSERT(desc.texDesc.textureType != EType::EType_UNKNOWN);
        ASSERT(desc.texDesc.format != DXGI_FORMAT_UNKNOWN);
        ASSERT(desc.texDesc.localRowPitchMode != ERowPitchMode::UNDEFINED);
        ASSERT(desc.texDesc.uploadRowPitchMode != ERowPitchMode::UNDEFINED);
        ASSERT(desc.texDesc.mipLevels == 1u, "LoadTextureMemory currently supports only the first mip");
        ASSERT(desc.texDesc.bytesPerPixel == BytesPerPixel(desc.texDesc.format));

        const UINT tightRowPitch = CalculateRowPitch(desc.width, desc.texDesc.bytesPerPixel, NSTexture::ERowPitchMode::TIGHT);
        ASSERT(srcRowPitch >= tightRowPitch);

        const size_t totalSize = static_cast<size_t>(desc.height - 1u) * srcRowPitch + tightRowPitch;

        ASSERT(dataSize >= totalSize, "Texture memory buffer is too small for the supplied dimensions and row pitch");

        Texture tex{};
        tex.name = name;
        tex.desc = desc.texDesc;
        tex.width = desc.width;
        tex.height = desc.height;
        tex.localRowPitch = CalculateRowPitch(tex.width, tex.desc.bytesPerPixel, tex.desc.localRowPitchMode);
        tex.uploadRowPitch = CalculateRowPitch(tex.width, tex.desc.bytesPerPixel, tex.desc.uploadRowPitchMode);

        const UINT totalChunkCount = (tex.height + ROWS_AT_A_TIME - 1u) / ROWS_AT_A_TIME;
        tex.chunks.assign(totalChunkCount, {});

        for (UINT idx{}; idx < totalChunkCount; idx++)
        {
            TextureChunk& chunk = tex.chunks[idx];

            chunk.firstRow = idx * ROWS_AT_A_TIME;
            ASSERT(tex.height > chunk.firstRow);

            chunk.rowCount = std::min( ROWS_AT_A_TIME, tex.height - chunk.firstRow);

            chunk.bytes.assign(static_cast<size_t>(chunk.rowCount) * tex.localRowPitch, std::byte{});

            for (UINT row{}; row < chunk.rowCount; ++row)
            {
                const UINT absoluteRow = chunk.firstRow + row;
                const std::byte* _Src = data + static_cast<size_t>(absoluteRow) * srcRowPitch;

                std::byte* _Dst = chunk.bytes.data() + static_cast<size_t>(row) * tex.localRowPitch;

                memcpy(_Dst, _Src, tightRowPitch);
            }
        }

        if(path.has_value()) tex.path = path.value().get();
        return tex;
    }
}
