#pragma once

#include "IRendererBackend.h"

#include <memory>
#include <string_view>

// The one place that knows which backends exist. Everything else — the
// front-end Renderer, app — only ever sees IRendererBackend.
//
// Which backends are actually compiled in is decided by the build system
// via D12F_RHI_HAS_DX12 / D12F_RHI_HAS_VULKAN (Windows gets both, so the
// Vulkan path can be validated on Win32 before Wayland exists; Linux gets
// Vulkan only, since the D3D12 headers don't exist there).
namespace NSRHI
{
    enum class EBackend
    {
        DX12,
        Vulkan,
    };

    // Parses the --rhi= value ("dx12" / "vulkan"). Falls back to the
    // default for this build if the string is empty or unrecognized.
    EBackend ParseBackendName(std::string_view name);

    // Creates the requested backend if it's compiled into this binary;
    // otherwise warns and falls back to one that is. Returns nullptr only
    // if no backend at all was compiled in.
    std::unique_ptr<IRendererBackend> CreateRendererBackend(EBackend requested);

    // Same, reading the request from --rhi= in g_CmdArguments.
    std::unique_ptr<IRendererBackend> CreateRendererBackendFromArgs();
}
