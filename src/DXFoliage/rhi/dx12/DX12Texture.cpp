#include "stdafx.h"
#include "DX12Texture.h"

#include "DXSampleHelper.h"

#include "DX12Format.h"

namespace NSRHIDX12
{
    namespace
    {
        // The mask's D3D12 half. Note what is NOT here: CopySource and
        // CopyDestination have no D3D12 resource flag — any resource is a
        // legal copy endpoint — so they are Vulkan-only in effect.
        //
        // DENY_SHADER_RESOURCE is the payoff for declaring the whole set.
        // It is a promise that nothing will ever sample this depth buffer,
        // which lets the driver keep compression the SRV path would force
        // it to drop. The old two-bool desc could not express it, because
        // "is not a render target" said nothing about being sampled.
        D3D12_RESOURCE_FLAGS ResourceFlagsFor(Flag<NSRHI::ETextureUsage> usage)
        {
            D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
            if (usage.HasLeastOne(NSRHI::ETextureUsage::RenderTarget))    flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            if (usage.HasLeastOne(NSRHI::ETextureUsage::UnorderedAccess)) flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            if (usage.HasLeastOne(NSRHI::ETextureUsage::DepthStencil))
            {
                flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
                if (not usage.HasLeastOne(NSRHI::ETextureUsage::Sampled))
                {
                    flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
                }
            }
            return flags;
        }
    }

    DX12Texture::DX12Texture(ID3D12Device14* device, ComPtr<ID3D12Resource> backBuffer, const NSRHI::TextureDesc& desc)
        : m_resource(std::move(backBuffer))
        , m_desc(desc)
    {
        ASSERT(desc.usage.HasLeastOne(NSRHI::ETextureUsage::RenderTarget),
            "This constructor is for render-target back buffers");

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.NumDescriptors = 1;
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_viewHeap)));

        m_viewHandle = m_viewHeap->GetCPUDescriptorHandleForHeapStart();
        device->CreateRenderTargetView(m_resource.Get(), nullptr, m_viewHandle);
    }

    DX12Texture::DX12Texture(ID3D12Device14* device, const NSRHI::TextureDesc& desc)
        : m_desc(desc)
    {
        ASSERT(desc.usage.HasLeastOne(NSRHI::ETextureUsage::DepthStencil),
            "This constructor is for newly-created depth/stencil resources");

        const DXGI_FORMAT format = ToDXGIFormat(desc.format);

        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, desc.width, desc.height);
        resourceDesc.Flags |= ResourceFlagsFor(desc.usage);
        CD3DX12_CLEAR_VALUE clearValue(format, 1.0f, 0);

        ThrowIfFailed(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clearValue,
            IID_PPV_ARGS(&m_resource)
        ));

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.NumDescriptors = 1;
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_viewHeap)));

        m_viewHandle = m_viewHeap->GetCPUDescriptorHandleForHeapStart();

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
        dsvDesc.Format = format;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        device->CreateDepthStencilView(m_resource.Get(), &dsvDesc, m_viewHandle);
    }
}
