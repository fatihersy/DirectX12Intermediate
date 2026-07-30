#pragma once

// This header is only ever compiled as part of the Windows build — it
// pulls in windows.h itself so it doesn't depend on include order.
#include "PlatformHeaders_Win32.h"
#include "platform/IWindow.h"

#include <cstdint>

// Win32 implementation of IWindow. Ports the window-creation/message-pump
// logic from the project's original Platform.h/Platform.cpp — same
// WNDCLASSEX/CreateWindow/WindowProc approach. The console-attach-for-
// debug-output dance and libassert wiring moved to PlatformFactory_Win32.cpp
// instead: they're process-level setup, not really about the window
// itself, so this class stays scoped to exactly what its name says.
//
// One real behavior change from the original: the pieces that used to
// reach directly into IApp — WM_CLOSE calling iApp->OnDestroy(), WM_SIZE
// calling iApp->OnResize(), Alt+Enter calling iApp->ToggleFullScreen() —
// now go through the neutral resize/close callbacks (or, for fullscreen,
// are handled entirely inside this class) instead, so Win32Window has no
// knowledge of IApp at all. That's what makes it swappable for a Wayland
// equivalent later.
//
// A second behavior change: WM_PAINT no longer drives rendering (it used
// to call iApp->OnUpdate()/OnRender() directly, and — because it never
// validated the update region — was actually a disguised free-running
// loop). Rendering is now driven explicitly from app::Run(), once per
// PumpEvents() call, on both platforms.
namespace NSPlatformWin32
{
    class Win32Window final : public NSPlatform::IWindow
    {
    public:
        Win32Window() = default;
        ~Win32Window() override;

        bool Create(const NSPlatform::WindowDesc& desc) override;
        void Show() override;
        void Destroy() override;
        void RequestClose() override;

        bool PumpEvents() override;

        NSPlatform::NativeWindowHandle GetNativeHandle() const override;

        // Win32 client-area size is already in physical pixels, so these
        // match until this window opts into per-monitor DPI awareness.
        // When it does, Width()/Height() become GetDpiForWindow-scaled and
        // only these two keep reporting raw pixels.
        uint32_t FramebufferWidth() const override { return Width(); }
        uint32_t FramebufferHeight() const override { return Height(); }

        uint32_t Width() const override { return m_width; }
        uint32_t Height() const override { return m_height; }

        void ToggleFullscreen() override;

        void SetResizeCallback(NSPlatform::FnWindowResize callback) override { m_resizeCallback = std::move(callback); }
        void SetCloseCallback(NSPlatform::FnWindowClose callback) override { m_closeCallback = std::move(callback); }

    private:
        friend LRESULT CALLBACK Win32WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

        HINSTANCE m_hInstance{ nullptr };
        HWND m_hWnd{ nullptr };
        uint32_t m_width{};
        uint32_t m_height{};

        bool m_isFullscreen{};
        RECT m_windowedRect{};

        NSPlatform::FnWindowResize m_resizeCallback;
        NSPlatform::FnWindowClose m_closeCallback;
    };
}
