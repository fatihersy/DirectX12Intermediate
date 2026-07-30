#pragma once

#include "ICommandList.h"
#include "IDevice.h"

#include "platform/IWindow.h"

#include <cstdint>

// The seam between the renderer front-end and a concrete graphics API.
// ONE implementation per API (RendererBackend_DX12, RendererBackend_Vulkan).
// The front-end (Renderer) owns a unique_ptr<IRendererBackend> and never
// names a concrete backend or a native GPU type.
//
// It bundles the three exposed objects for its API and drives the frame:
//   1. IDevice (backend->GetDevice()) — the first-class resource factory
//      the front-end creates buffers/textures/pipelines through
//      (Diligent-style; see IDevice.h).
//   2. ICommandList (returned by BeginFrame()) — the exposed command
//      context the front-end records draws into (Diligent-style; #2).
//   3. The swapchain + frame fence + queue + native factory/instance +
//      adapter selection stay PRIVATE to the backend — bootstrapping and
//      frame plumbing the front-end never touches. Acquire/submit/present
//      "progress in the background."
//
// BACKEND-AFFINITY INVARIANT (important):
//   The backend hands out I* handles (via GetDevice()/BeginFrame()) and
//   later consumes them, downcasting (e.g. static_cast<DX12Texture*>) to
//   reach the native resource. This is sound ONLY because exactly one
//   backend is live per run (chosen at startup via --rhi=). A resource is
//   backend-affine: never create it with one backend and use it with
//   another. Enforced by construction, not by the type system.
namespace NSRHI
{
    class IRendererBackend
    {
    public:
        virtual ~IRendererBackend() = default;

        // --- Lifecycle ---
        // Creates the device/queue/swapchain against the given window and
        // gets ready to render at width x height. The backend pulls the
        // native window handle out of IWindow itself — the front-end
        // never touches an HWND / wl_surface.
        virtual bool Initialize(NSPlatform::IWindow& window, uint32_t width, uint32_t height) = 0;
        virtual void Shutdown() = 0;
        virtual void Resize(uint32_t width, uint32_t height) = 0;

        // --- Exposed resource factory ---
        virtual IDevice& GetDevice() = 0;

        // The pixel format of the swapchain's backbuffers.
        //
        // The front-end needs this when it builds a pipeline, because
        // Vulkan bakes attachment formats into VkPipeline and D3D12 puts
        // them in the PSO desc - and that happens before any frame has
        // begun, so it cannot come from CurrentBackBuffer(). The format is
        // whatever the surface actually supports, which is NOT something
        // the front-end can assume: it varies by driver and compositor,
        // and hardcoding a guess produces a mismatch that only shows up as
        // a validation error or swapped colour channels.
        virtual EFormat BackBufferFormat() const = 0;

        // --- Frame boundary ---
        // BeginFrame acquires this frame's backbuffer, transitions it to a
        // render target, begins command recording, and returns the command
        // list for the front-end to record draws into. EndFrame finishes
        // recording, submits, and presents. Acquire/submit/present + the
        // frame fence stay backend-private; command *recording* is exposed.
        virtual ICommandList& BeginFrame() = 0;
        virtual void EndFrame() = 0;

        // Blocks until the GPU has finished everything submitted so far.
        //
        // Needed because destroying a resource the GPU may still be reading
        // is undefined behaviour on both APIs, and the front-end owns
        // resources whose lifetime it controls - so only the front-end
        // knows when it is about to release them. Both backends already had
        // this internally (vkDeviceWaitIdle / a fence wait); it is exposed
        // so Renderer::Shutdown can drain before releasing.
        //
        // Expensive by design: a full pipeline stall. Shutdown and resize
        // only, never per frame.
        virtual void WaitForGPU() = 0;

        // The current frame's backbuffer, as a render-target ITexture the
        // front-end passes to cmd.BeginRendering(). Valid between
        // BeginFrame() and EndFrame(); must be re-fetched each frame
        // (the swapchain recreates its backbuffers on resize).
        virtual ITexture& CurrentBackBuffer() = 0;
    };
}
