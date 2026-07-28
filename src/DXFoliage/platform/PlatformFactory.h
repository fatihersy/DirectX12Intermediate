#pragma once

#include "IInputSource.h"
#include "IWindow.h"

#include <memory>

// The one function app.cpp is allowed to call to get a window+input pair.
// Everything platform-specific — which concrete class to build, how to
// wire the window's native handle into the input source, console/
// debugger setup (including reading launch flags like --console-pid=
// straight out of core/Defines.h's g_CmdArguments — that header is
// deliberately safe to include anywhere in this project) — happens
// inside whichever CreatePlatform() implementation actually gets
// compiled (platform/win32/PlatformFactory_Win32.cpp today; a Wayland
// equivalent later). app.cpp includes only this header, never
// Win32Window.h/Win32Input.h directly, so it has zero knowledge that
// Win32 exists — the same reason Kohi's application layer only calls
// into platform_system_startup() and never names platform_win32 directly.
namespace NSPlatform
{
    struct PlatformHandles
    {
        std::unique_ptr<IWindow> window;
        std::unique_ptr<NSInput::IInputSource> input;
    };

    PlatformHandles CreatePlatform(const WindowDesc& desc);
}
