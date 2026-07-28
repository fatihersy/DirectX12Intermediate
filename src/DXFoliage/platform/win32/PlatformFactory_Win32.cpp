#include "stdafx.h"
#include "platform/PlatformFactory.h"

#include "Logger.h"
#include "core/Defines.h"

#include "Win32Input.h"
#include "Win32Window.h"

// Everything in this file is process-level setup that used to live inside
// Win32Window::Create() even though it has nothing to do with the window
// itself (console attach, libassert wiring, the debugger-wait loop) —
// moved here so Win32Window stays scoped to exactly what its name says,
// and so all of it lives in the one file that's allowed to know Windows
// exists. It reads --console-pid=/--wait-for-debugger straight out of
// core/Defines.h's g_CmdArguments (already parsed by main.cpp, itself
// fully OS-neutral) — that header is deliberately safe to include from
// anywhere in this project, so there's no need to thread those values
// through app's constructor just to avoid reaching for it here.
namespace NSPlatform
{
    namespace
    {
        enum class EConsoleColor : WORD
        {
            White = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
            Red = FOREGROUND_RED | FOREGROUND_INTENSITY,
            Green = FOREGROUND_GREEN | FOREGROUND_INTENSITY,
            Blue = FOREGROUND_BLUE | FOREGROUND_INTENSITY,
            Yellow = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
            Magenta = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
            Cyan = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
            Gray = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE
        };

        void PlatformConsoleWrite(uint8_t level, std::string_view message)
        {
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

            if (hConsole == INVALID_HANDLE_VALUE)
            {
                OutputDebugStringA(message.data());
                OutputDebugStringA("\n");
                return;
            }

            CONSOLE_SCREEN_BUFFER_INFO csbi{};
            GetConsoleScreenBufferInfo(hConsole, &csbi);
            WORD originalColor = csbi.wAttributes;
            WORD newColor = static_cast<WORD>(EConsoleColor::Red);

            switch (level)
            {
                case static_cast<uint8_t>(ELogLevel::EFATAL): newColor = static_cast<WORD>(EConsoleColor::Red); break;
                case static_cast<uint8_t>(ELogLevel::EERROR): newColor = static_cast<WORD>(EConsoleColor::Red); break;
                case static_cast<uint8_t>(ELogLevel::EWARN):  newColor = static_cast<WORD>(EConsoleColor::Yellow); break;
                case static_cast<uint8_t>(ELogLevel::EINFO):  newColor = static_cast<WORD>(EConsoleColor::Blue); break;
                case static_cast<uint8_t>(ELogLevel::EDEBUG): newColor = static_cast<WORD>(EConsoleColor::White); break;
                case static_cast<uint8_t>(ELogLevel::ETRACE): newColor = static_cast<WORD>(EConsoleColor::White); break;
                default: newColor = static_cast<WORD>(EConsoleColor::Red); break;
            }

            SetConsoleTextAttribute(hConsole, newColor);

            DWORD charsWritten{};
            WriteConsoleA(hConsole, message.data(), static_cast<DWORD>(message.length()), &charsWritten, nullptr);
            WriteConsoleA(hConsole, "\n", 1, nullptr, nullptr);

            SetConsoleTextAttribute(hConsole, originalColor);
        }

        // Attaches to whatever process launched us (--console-pid=, or
        // the parent process by default) or allocates a fresh console —
        // then points the shared g_PlatformConsoleWrite hook (see
        // Logger.h) at this file's colored-output implementation.
        void SetupConsole()
        {
            const DWORD consolePid = g_CmdArguments[ARG_CONSOLE_PID].present
                ? static_cast<DWORD>(std::stoul(g_CmdArguments[ARG_CONSOLE_PID].value))
                : ATTACH_PARENT_PROCESS;

            FreeConsole();
            if (AttachConsole(consolePid) or AllocConsole())
            {
                SetConsoleTitle(L"Debug Console");

                HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);

                DWORD dwMode{};
                GetConsoleMode(hOut, &dwMode);
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hOut, dwMode);

                WriteConsoleA(hOut, "\n", 1, nullptr, nullptr);
            }

            g_PlatformConsoleWrite = [](ELogLevel level, const std::string_view message)
            {
                PlatformConsoleWrite(static_cast<uint8_t>(level), message);
            };
        }

        void SetupAssertHandler()
        {
            auto libAssertOutput = [](const libassert::assertion_info& inf)
            {
                if (inf.message->size() < std::numeric_limits<uint16_t>::max())
                {
                    std::string msg(inf.to_string(inf.message->size()));

                    g_PlatformConsoleWrite(ELogLevel::EERROR, msg);
                }
                else
                {
                    g_PlatformConsoleWrite(ELogLevel::EERROR, "Failed to build message");
                }

                std::abort();
            };

            libassert::set_failure_handler(libAssertOutput);
        }

        void WaitForDebugger()
        {
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

            const std::string message = std::format("Waiting for debugger. PID: {}\n", GetCurrentProcessId());
            WriteConsoleA(hConsole, message.c_str(), static_cast<DWORD>(message.size()), nullptr, nullptr);

            while (not IsDebuggerPresent())
            {
                Sleep(5000);

                WriteConsoleA(hConsole, "*\n", 2, nullptr, nullptr);
            }

            __debugbreak();
        }
    }

    PlatformHandles CreatePlatform(const WindowDesc& desc)
    {
        SetupConsole();
        SetupAssertHandler();

        if (g_CmdArguments[ARG_WAIT_FOR_DEBUGGER].present)
        {
            WaitForDebugger();
        }

        auto window = std::make_unique<NSPlatformWin32::Win32Window>();
        window->Create(desc);

        auto input = std::make_unique<NSPlatformWin32::Win32Input>();
        input->SetWindow(reinterpret_cast<HWND>(window->GetNativeHandle().b));

        return PlatformHandles{ .window = std::move(window), .input = std::move(input) };
    }
}
