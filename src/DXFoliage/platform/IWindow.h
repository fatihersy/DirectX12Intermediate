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

        virtual uint32_t Width() const = 0;
        virtual uint32_t Height() const = 0;

        virtual void ToggleFullscreen() = 0;

        virtual void SetResizeCallback(FnWindowResize callback) = 0;
        virtual void SetCloseCallback(FnWindowClose callback) = 0;
    };
}
