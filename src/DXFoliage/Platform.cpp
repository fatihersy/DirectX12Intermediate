#include "stdafx.h"
#include "Platform.h"

#include "IApp.h"
#include "Logger.h"

#include "directxtk12/Keyboard.h"
#include "directxtk12/Mouse.h"

#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

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

bool Platform::OnInit(SWindow wnd)
{
    ASSERT_VAL(wnd.pApp != nullptr, "Invalid app handle");

    int consolePid = std::stoul(g_CmdArguments[ARG_CONSOLE_PID].value);
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

    this->m_wnd = wnd;

    WNDCLASSEX wndClass{};
    wndClass.cbSize = sizeof(decltype(wndClass));
    wndClass.style = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc = WindowProc;
    wndClass.hInstance = m_wnd.hInstance;
    wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndClass.lpszClassName = L"WinMain";
    RegisterClassEx(&wndClass);

    RECT rect = {0L, 0L, static_cast<LONG>(m_wnd.width), static_cast<LONG>(m_wnd.height)};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    m_wnd.hWnd = CreateWindow
    (
        wndClass.lpszClassName,
        m_wnd.title.data(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        m_wnd.hInstance,
        m_wnd.pApp
    );

    auto libAssertOutput = [](const libassert::assertion_info& inf)
    {
        if (inf.message->size() < std::numeric_limits<uint16_t>::max())
        {
            std::string msg(inf.to_string(inf.message->size()));

            g_PlatformConsoleWrite(ELogLevel::EERROR, msg);
        }
        else {
            g_PlatformConsoleWrite(ELogLevel::EERROR, "Failed to build message");
        }


        std::abort();
    };

    libassert::set_failure_handler(libAssertOutput);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    return true;
}

Platform::~Platform()
{}

void Platform::ShowWindow()
{
    ImGui_ImplWin32_Init(m_wnd.hWnd);

    ::ShowWindow(m_wnd.hWnd, m_wnd.nCmdShow);
}
void Platform::Dispatch(MSG& msg)
{
    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam)) return true;

    IApp* iApp = reinterpret_cast<IApp*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (message)
    {
        case WM_ACTIVATEAPP:
        {
            if (wParam == WA_INACTIVE)
            {
                break;
            }
            DirectX::Keyboard::ProcessMessage(message, wParam, lParam);
            DirectX::Mouse::ProcessMessage(message, wParam, lParam);
            break;
        }

        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        case WM_ACTIVATE:
        case WM_INPUT:
        case WM_MOUSEHWHEEL:
        case WM_MOUSEHOVER:
        case WM_MOUSEMOVE: DirectX::Mouse::ProcessMessage(message, wParam, lParam);
            break;

        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP: DirectX::Keyboard::ProcessMessage(message, wParam, lParam);
            break;
        case WM_SYSKEYDOWN:
        {
            if (wParam == VK_RETURN and (lParam & 0x60000000) == 0x20000000) // Alt + Enter
            {
                ASSERT_VAL(iApp != nullptr, "Invalid app handle");

                iApp->ToggleFullScreen();
                return 0;
            }
            DirectX::Keyboard::ProcessMessage(message, wParam, lParam);
            break;
        }

        case WM_MOUSEACTIVATE: return MA_ACTIVATEANDEAT;

        case WM_PAINT:
        {
            ASSERT_VAL(iApp != nullptr, "Invalid app handle");

            if (not iApp->im_isQuitting)
            {
                ImGui_ImplWin32_NewFrame();
                iApp->OnUpdate();
                iApp->OnRender();
            }

            return 0;
        }
        case WM_NCCREATE:
        {
            CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            iApp = reinterpret_cast<IApp*>(pCreate->lpCreateParams);

            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(iApp));

            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        case WM_CLOSE:
        {
            ASSERT_VAL(iApp != nullptr, "Invalid app handle");

            iApp->OnDestroy();

            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();

            iApp->im_isQuitting = true;

            FreeConsole();
            DestroyWindow(hWnd);

            return 0;
        }
        case WM_DESTROY:
        {
            PostQuitMessage(EXIT_SUCCESS);
            return 0;
        }
        case WM_SIZE:
        {
            ASSERT_VAL(iApp != nullptr, "Invalid app handle");

            if (wParam != SIZE_MINIMIZED)
            {
                const UINT width = LOWORD(lParam);
                const UINT height = HIWORD(lParam);
                iApp->OnResize(width, height);
            }

            return 0;
        }

        default: break;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

void Platform::PlatformConsoleWrite(std::uint8_t level, const std::string_view message)
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
        case static_cast<uint8_t>(ELogLevel::EFATAL):
            newColor = static_cast<WORD>(EConsoleColor::Red);
            break;
        case static_cast<uint8_t>(ELogLevel::EERROR):
            newColor = static_cast<WORD>(EConsoleColor::Red);
            break;
        case static_cast<uint8_t>(ELogLevel::EWARN):
            newColor = static_cast<WORD>(EConsoleColor::Yellow);
            break;
        case static_cast<uint8_t>(ELogLevel::EINFO):
            newColor = static_cast<WORD>(EConsoleColor::Blue);
            break;
        case static_cast<uint8_t>(ELogLevel::EDEBUG):
            newColor = static_cast<WORD>(EConsoleColor::White);
            break;
        case static_cast<uint8_t>(ELogLevel::ETRACE):
            newColor = static_cast<WORD>(EConsoleColor::White);
            break;
        default:
            newColor = static_cast<WORD>(EConsoleColor::Red);
            break;
    }

    SetConsoleTextAttribute(hConsole, newColor);

    DWORD charsWritten{};
    WriteConsoleA(hConsole, message.data(), static_cast<DWORD>(message.length()), &charsWritten, NULL);
    WriteConsoleA(hConsole, "\n", 1, nullptr, nullptr);

    SetConsoleTextAttribute(hConsole, originalColor);
}
