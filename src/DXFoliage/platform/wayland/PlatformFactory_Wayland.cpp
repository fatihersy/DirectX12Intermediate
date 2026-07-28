#include "stdafx.h"
#include "platform/PlatformFactory.h"

#include "Logger.h"

#include "WaylandInput.h"
#include "WaylandWindow.h"

#include <cstdio>

// The Linux counterpart of PlatformFactory_Win32.cpp — the one file on
// this platform allowed to name concrete window/input types.
namespace NSPlatform
{
    namespace
    {
        void SetupConsole()
        {
            // Linux needs no console attaching: a process launched from a
            // shell already has stdout/stderr. Just point the shared log
            // hook at them.
            g_PlatformConsoleWrite = [](ELogLevel level, const std::string_view message)
            {
                std::FILE* out = (level == ELogLevel::EERROR or level == ELogLevel::EFATAL) ? stderr : stdout;
                std::fprintf(out, "%.*s\n", static_cast<int>(message.size()), message.data());
            };
        }
    }

    PlatformHandles CreatePlatform(const WindowDesc& desc)
    {
        SetupConsole();

        auto window = std::make_unique<NSPlatformWayland::WaylandWindow>();
        if (not window->Create(desc))
        {
            return PlatformHandles{};
        }

        return PlatformHandles{ .window = std::move(window), .input = std::make_unique<NSPlatformWayland::WaylandInput>() };
    }
}
