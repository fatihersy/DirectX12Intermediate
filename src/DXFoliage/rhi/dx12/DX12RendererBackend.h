#pragma once

#include "rhi/IRendererBackend.h"

#include <memory>

// DirectX-free by design: the concrete DX12RendererBackend class (with its
// ID3D12* / ComPtr members) lives entirely in DX12RendererBackend.cpp. All
// this header exposes is the factory function the RendererBackendFactory
// calls — so neither the factory nor anything else neutral ever pulls in
// windows.h / d3d12.h just to construct a backend. (Contrast the resource
// headers like DX12Device.h, which legitimately carry DirectX because only
// other DX12 .cpp files include them.)
namespace NSRHIDX12
{
    std::unique_ptr<NSRHI::IRendererBackend> CreateDX12Backend();
}
