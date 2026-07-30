#pragma once

#include <cstdint>
#include <functional>
#include <string_view>

// The windowing contract every backend (Win32, Wayland, ...) implements.
// Application code (app.cpp, main.cpp, the RHI swapchain) should only ever
// talk to IWindow, never to HWND/wl_surface directly, so it doesn't need to
// know or care which OS it's running on.
namespace NSPlatform
{
    struct WindowDesc
    {
        uint32_t width{};
        uint32_t height{};
        std::wstring_view title;
    };

    // A window handle that's meaningful to the graphics backend (e.g. to
    // create a swapchain against). Two raw pointers cover every backend
    // this project targets: Win32 needs (HINSTANCE, HWND), Wayland needs
    // (wl_display*, wl_surface*). Which pointer means what is defined by
    // whichever IWindow implementation produced it — callers are expected
    // to already know which backend they're talking to when they use this.
    struct NativeWindowHandle
    {
        void* a = nullptr;
        void* b = nullptr;
    };

    using FnWindowResize = std::function<void(uint32_t width, uint32_t height)>;
    using FnWindowClose = std::function<void()>;

    class IWindow
    {
    public:
        virtual ~IWindow() = default;

        virtual bool Create(const WindowDesc& desc) = 0;
        virtual void Show() = 0;
        virtual void Destroy() = 0;

        // Asks the window to close, the same way clicking the OS close
        // button would (asynchronous — takes effect on a later
        // PumpEvents() call, not immediately). Replaces the original
        // code's PostMessage(hwnd, WM_CLOSE, ...) call from app.cpp.
        virtual void RequestClose() = 0;

        // Processes whatever OS/compositor events are waiting right now
        // and returns immediately (never blocks). Returns false once the
        // window has received a close request, so the caller's main loop
        // knows to stop. Deliberately does *not* render or tick game
        // logic itself — every backend hands control back to the caller
        // once per pump, and rendering happens uniformly from there
        // (see app::Run()), instead of Windows-only code driving the
        // frame from inside a WM_PAINT handler.
        virtual bool PumpEvents() = 0;

        virtual NativeWindowHandle GetNativeHandle() const = 0;

        // LOGICAL size, in the units the windowing system lays out in.
        // Mouse coordinates and UI metrics are in these units. On a
        // display scaled to 150% this stays 1280x720 while the window
        // physically covers 1920x1080 pixels.
        virtual uint32_t Width() const = 0;
        virtual uint32_t Height() const = 0;

        // PHYSICAL size, in real pixels. This is what a swapchain must be
        // sized to; using the logical size instead makes the compositor
        // upscale a too-small image, which looks like rendering at half
        // resolution.
        //
        // Separate accessors rather than one "size" because the two units
        // genuinely differ and conflating them is the classic HiDPI bug.
        // GLFW draws the same distinction (window size vs framebuffer
        // size) and says outright: do not pass the window size to
        // pixel-based calls.
        virtual uint32_t FramebufferWidth() const = 0;
        virtual uint32_t FramebufferHeight() const = 0;

        virtual void ToggleFullscreen() = 0;

        virtual void SetResizeCallback(FnWindowResize callback) = 0;
        virtual void SetCloseCallback(FnWindowClose callback) = 0;
    };
}
