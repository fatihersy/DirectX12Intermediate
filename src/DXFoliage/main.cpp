#include "stdafx.h"
#include "app.h"

struct FindArgResult
{
    bool found{};
    size_t at = std::string_view::npos;
    std::string_view value;
};

FindArgResult FindArg(std::string_view cmdLine, SCmdArg& cmdArg)
{
    if (cmdLine.empty()) return FindArgResult();

    for (std::string& argVariant : cmdArg.aliases)
    {
        size_t argAt = std::string_view(cmdLine).find(argVariant);

        if (argAt != std::string_view::npos)
        {
            size_t valBegin = argAt + argVariant.size();
            size_t valEnd = cmdLine.find(' ', valBegin);
            std::string_view value;

            if (cmdArg.expectsValue) value = cmdLine.substr(
                valBegin,
                valEnd == std::string_view::npos ? std::string_view::npos : valEnd - valBegin
            );

            return {
                .found = true,
                .at = argAt,
                .value = value
            };
        }
    }

    return FindArgResult();
}


// One real entry point on every platform. On Windows this only works
// because build.lua tells the linker to start at mainCRTStartup instead
// of WinMainCRTStartup (see the /ENTRY: linkoption there) — the OS still
// launches a windowed, console-less app, but the CRT hands control to
// main() instead of WinMain(), with argc/argv already parsed from the
// Windows command line exactly like a console app would get. That
// linker-level trick is the only thing that changes between platforms;
// this function itself doesn't need to.
int main(int argc, char** argv)
{
    // FindArg()/g_CmdArguments below expect one joined string to search
    // substrings in — that's what WinMain's LPSTR used to hand over
    // directly. argv comes pre-split on every platform, so we rejoin it
    // here rather than rewriting the (otherwise OS-neutral) parsing logic
    // to work off argv directly.
    std::string joinedArgs;
    for (int i = 1; i < argc; ++i)
    {
        if (i > 1) joinedArgs += ' ';
        joinedArgs += argv[i];
    }
    std::string_view cmdLine = joinedArgs;

    if (not cmdLine.empty())
    {
        for (SCmdArg& arg : g_CmdArguments)
        {
            FindArgResult result = FindArg(cmdLine, arg);

            if (result.found)
            {
                arg.present = true;
                arg.value = result.value;
            }
        }
    }

    // g_CmdArguments is now fully populated; NSPlatform::CreatePlatform()
    // (called from app's constructor) reads whatever launch flags it
    // needs straight out of it — see PlatformFactory_Win32.cpp.
    app app(1280, 720, PROJECT_NAME);
    app.OnInit();

    return app.Run();
}
