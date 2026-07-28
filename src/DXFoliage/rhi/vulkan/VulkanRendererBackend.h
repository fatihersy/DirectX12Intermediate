#pragma once

#include "rhi/IRendererBackend.h"

#include <memory>

// Vulkan-free by design, mirroring dx12/DX12RendererBackend.h: the concrete
// VulkanRendererBackend class (with its Vk* members) lives entirely in
// VulkanRendererBackend.cpp. All this header exposes is the factory
// function RendererBackendFactory calls, so nothing neutral ever pulls in
// vulkan.h just to construct a backend.
namespace NSRHIVulkan
{
    std::unique_ptr<NSRHI::IRendererBackend> CreateVulkanBackend();
}
