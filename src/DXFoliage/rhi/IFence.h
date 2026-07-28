#pragma once

#include <cstdint>

// A "the GPU has reached this point" counter — mirrors the existing
// single-fence scheme in Renderer.cpp (m_fence/MoveToNextFrame/
// WaitForGPU): a monotonically increasing value the GPU signals once it
// finishes the work submitted up to that point, which the CPU can wait on
// before reusing per-frame resources.
//
// On Vulkan this is backed by VkFence/VkSemaphore + vkWaitForFences, which
// needs no OS event handle — unlike today's DX12 path, which calls Win32's
// CreateEvent/WaitForSingleObjectEx directly inside Renderer.cpp. Moving
// that wait behind IFence is what lets Renderer-level code stop touching
// Win32 sync primitives at all.
namespace NSRHI
{
    class IFence
    {
    public:
        virtual ~IFence() = default;

        virtual void Signal(uint64_t value) = 0;
        virtual void Wait(uint64_t value) = 0;
        virtual uint64_t CompletedValue() const = 0;
    };
}
