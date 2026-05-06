#include "stdafx.h"
#include "app.h"

struct FindArgResult
{
    bool found{};
    size_t At = std::string_view::npos;
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

            if (cmdArg.expectsValue) value = cmdLine.substr(valBegin, valEnd == std::string_view::npos ? std::string_view::npos : valEnd - valBegin);

            return {
                .found = true,
                .At = argAt,
                .value = value
            };
        }
    };

    return FindArgResult();
}

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR _cmdLine, int nCmdShow)
{
    std::string_view cmdLine = _cmdLine ? _cmdLine : "";

    g_CmdArguments[static_cast<size_t>(ARG_CONSOLE_PID)].value = std::to_string(ATTACH_PARENT_PROCESS);

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

    if (g_CmdArguments[ARG_WAIT_FOR_DEBUGGER].present)
    {
        AttachConsole(ATTACH_PARENT_PROCESS);
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

        const std::string message = std::format("DXTerrain waiting for debugger. PID: {}\n", GetCurrentProcessId());

        WriteConsoleA(hConsole, message.c_str(), message.size(), nullptr, nullptr);

        while (not IsDebuggerPresent())
        {
            Sleep(5000);

            WriteConsoleA(hConsole, "*\n", 2, nullptr, nullptr);
        }

        __debugbreak();
    }

    app app(1280, 720, L"DXTerrain", hInstance, nCmdShow);
    app.OnInit();

    return app.Run();
}
