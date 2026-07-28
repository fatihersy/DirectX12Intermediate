#include "stdafx.h"
#include "RendererBackendFactory.h"

#include "Logger.h"
#include "core/Defines.h"

// Until the build system defines these per-target (see the mox Linux
// plumbing task), default to "DX12 on Windows, nothing else" so the
// Windows build keeps working exactly as before. Once build.lua sets
// them explicitly, this fallback becomes dead and can go.
#if !defined(D12F_RHI_HAS_DX12) && !defined(D12F_RHI_HAS_VULKAN)
    #if defined(D12F_OS_WINDOWS)
        #define D12F_RHI_HAS_DX12
    #endif
#endif

// Both of these headers are deliberately backend-header-free — they only
// declare a CreateXBackend() factory function, so this file never pulls in
// d3d12.h / vulkan.h just to construct a backend.
#if defined(D12F_RHI_HAS_DX12)
    #include "dx12/DX12RendererBackend.h"
#endif
#if defined(D12F_RHI_HAS_VULKAN)
    #include "vulkan/VulkanRendererBackend.h"
#endif

namespace NSRHI
{
    namespace
    {
        const char* BackendName(EBackend backend)
        {
            return (backend == EBackend::Vulkan) ? "vulkan" : "dx12";
        }

        bool IsCompiledIn(EBackend backend)
        {
#if defined(D12F_RHI_HAS_DX12)
            if (backend == EBackend::DX12) return true;
#endif
#if defined(D12F_RHI_HAS_VULKAN)
            if (backend == EBackend::Vulkan) return true;
#endif
            (void)backend;
            return false;
        }

        // Preference order when the request can't be honored: DX12 first
        // on a build that has it (it's the proven path), else Vulkan.
        bool PickAvailable(EBackend& out)
        {
#if defined(D12F_RHI_HAS_DX12)
            out = EBackend::DX12;
            return true;
#elif defined(D12F_RHI_HAS_VULKAN)
            out = EBackend::Vulkan;
            return true;
#else
            (void)out;
            return false;
#endif
        }

        std::unique_ptr<IRendererBackend> Construct(EBackend backend)
        {
#if defined(D12F_RHI_HAS_DX12)
            if (backend == EBackend::DX12) return NSRHIDX12::CreateDX12Backend();
#endif
#if defined(D12F_RHI_HAS_VULKAN)
            if (backend == EBackend::Vulkan) return NSRHIVulkan::CreateVulkanBackend();
#endif
            (void)backend;
            return nullptr;
        }
    }

    EBackend ParseBackendName(std::string_view name)
    {
        if (name == "vulkan" or name == "vk") return EBackend::Vulkan;
        if (name == "dx12" or name == "d3d12") return EBackend::DX12;

        EBackend fallback{ EBackend::DX12 };
        PickAvailable(fallback);

        if (not name.empty())
        {
            g_FWarn("Unknown --rhi value '%s'; using '%s'", std::string(name), BackendName(fallback));
        }

        return fallback;
    }

    std::unique_ptr<IRendererBackend> CreateRendererBackend(EBackend requested)
    {
        EBackend chosen = requested;

        if (not IsCompiledIn(requested))
        {
            EBackend available{};
            if (not PickAvailable(available))
            {
                g_FError("No graphics backend was compiled into this build");
                return nullptr;
            }

            g_FWarn("Backend '%s' is not available in this build; falling back to '%s'",
                BackendName(requested), BackendName(available));

            chosen = available;
        }

        return Construct(chosen);
    }

    std::unique_ptr<IRendererBackend> CreateRendererBackendFromArgs()
    {
        const SCmdArg& arg = g_CmdArguments[ARG_RHI_BACKEND];
        return CreateRendererBackend(ParseBackendName(arg.present ? arg.value : std::string_view{}));
    }
}
