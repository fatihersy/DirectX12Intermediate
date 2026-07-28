#pragma once

#include "PlatformHeaders_DX12.h"
#include "rhi/IBuffer.h"

// DX12 implementation of IBuffer. Today only the cpu-visible (upload
// heap) path is exercised — ports the vertex buffer creation from
// Renderer.cpp (a D3D12_HEAP_TYPE_UPLOAD committed resource, mapped once,
// memcpy'd, unmapped) near-verbatim. The GPU-only (default heap + copy)
// path is wired in but not yet exercised by anything — it's the same
// default/upload/UpdateSubresources/barrier pattern DXTerrain's
// NSTexture::Texture already proves out, and is where this goes once
// real models/textures replace today's hardcoded triangle.
namespace NSRHIDX12
{
    class DX12Buffer final : public NSRHI::IBuffer
    {
    public:
        DX12Buffer(ID3D12Device14* device, const NSRHI::BufferDesc& desc);
        ~DX12Buffer() override = default;

        size_t Size() const override { return m_size; }

        void* Map() override;
        void Unmap() override;

        // Not part of IBuffer — the DX12 backend needs the raw resource/
        // GPU virtual address to build a D3D12_VERTEX_BUFFER_VIEW or bind
        // a root CBV/SRV. Callers on the Renderer side that already know
        // they're talking to the DX12 backend use these directly (see
        // Renderer::DrawScene's IASetVertexBuffers call) until drawing
        // itself moves behind ICommandList.
        ID3D12Resource* Raw() const { return m_resource.Get(); }
        D3D12_GPU_VIRTUAL_ADDRESS GPUAddress() const { return m_resource->GetGPUVirtualAddress(); }

    private:
        ComPtr<ID3D12Resource> m_resource;
        size_t m_size{};
        bool m_cpuVisible{ false };
    };
}
