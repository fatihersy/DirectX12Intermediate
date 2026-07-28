#pragma once

#include "PlatformHeaders_DX12.h"
#include "rhi/ITexture.h"

// DX12 implementation of ITexture. Every DX12Texture owns a private,
// single-descriptor heap for whichever view it needs (RTV or DSV) —
// unlike the original code's one-shared-heap-with-N-slots pattern
// (Renderer.h's m_rtvHeap), this way a texture is fully self-contained
// and doesn't need an external heap/index to be usable. This also
// matches Vulkan cleanly: under dynamic rendering there's no
// descriptor-heap concept for render targets at all, just a VkImageView
// attached directly — so nothing outside this class needs to know DX12
// render targets are backed by a heap internally.
namespace NSRHIDX12
{
    class DX12Texture final : public NSRHI::ITexture
    {
    public:
        // Wraps a pre-existing resource (e.g. one handed back by
        // swapchain->GetBuffer()) and creates an RTV for it. Ownership of
        // backBuffer transfers in.
        DX12Texture(ID3D12Device14* device, ComPtr<ID3D12Resource> backBuffer, const NSRHI::TextureDesc& desc);

        // Creates a brand-new resource (e.g. a depth buffer) and its
        // matching DSV. Not exercised by anything yet — DXFoliage's
        // current depth-stencil creation (Renderer::Resize) is itself
        // dead code today (created but never bound/cleared), so this
        // constructor is here for when that becomes real rather than
        // being wired in now.
        DX12Texture(ID3D12Device14* device, const NSRHI::TextureDesc& desc);

        ~DX12Texture() override = default;

        uint32_t Width() const override { return m_desc.width; }
        uint32_t Height() const override { return m_desc.height; }
        NSRHI::EFormat Format() const override { return m_desc.format; }

        // Not part of ITexture — DX12-specific accessors for Renderer
        // code that still talks to D3D12 directly today (resource-barrier
        // transitions, OMSetRenderTargets) until drawing itself moves
        // behind ICommandList.
        ID3D12Resource* Raw() const { return m_resource.Get(); }
        D3D12_CPU_DESCRIPTOR_HANDLE View() const { return m_viewHandle; }

    private:
        ComPtr<ID3D12Resource> m_resource;
        NSRHI::TextureDesc m_desc;

        ComPtr<ID3D12DescriptorHeap> m_viewHeap;
        D3D12_CPU_DESCRIPTOR_HANDLE m_viewHandle{};
    };
}
