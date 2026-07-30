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

                // stdout is block-buffered whenever it is not a terminal, so
                // without this every log line is lost if the process is
                // killed or crashes - exactly when the log matters most.
                // stderr is unbuffered already; flushing it costs nothing.
                std::fflush(out);
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

        // Input shares the window's display connection but binds its own
        // wl_seat - see WaylandInput.h for why it cannot reuse one bound
        // by the window.
        auto input = std::make_unique<NSPlatformWayland::WaylandInput>(window->GetDisplay());

        return PlatformHandles{ .window = std::move(window), .input = std::move(input) };
    }
}
