#include "stdafx.h"
#include "Platform.h"

#include "IApp.h"

#include "directxtk12/Keyboard.h"
#include "directxtk12/Mouse.h"

#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

enum class FConsoleColor : WORD {
    White = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
    Red = FOREGROUND_RED | FOREGROUND_INTENSITY,
    Green = FOREGROUND_GREEN | FOREGROUND_INTENSITY,
    Blue = FOREGROUND_BLUE | FOREGROUND_INTENSITY,
    Yellow = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
    Magenta = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
    Cyan = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
    Gray = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE
};

Platform::Platform(SWindow wnd)
{
    if (not wnd.pApp)
    {
        std::runtime_error("Invalid app handle");
    }

    int consolePid = std::stoul(g_CmdArguments[ARG_CONSOLE_PID].value);
    FreeConsole();
    if (AttachConsole(consolePid) or AllocConsole())
    {
        SetConsoleTitle(L"Debug Console");

        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;

        GetConsoleMode(hOut, &dwMode);
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
        WriteConsoleA(hOut, "\n", 1, nullptr, nullptr);
    }
    g_PlatformConsoleWrite = &Platform::PlatformConsoleWrite;

    this->m_wnd = wnd;

    WNDCLASSEX wndClass{};
    wndClass.cbSize = sizeof(decltype(wndClass));
    wndClass.style = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc = WindowProc;
    wndClass.hInstance = m_wnd.hInstance;
    wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndClass.lpszClassName = L"WinMain";
    RegisterClassEx(&wndClass);

    RECT rect = {0l, 0l, static_cast<LONG>(m_wnd.width), static_cast<LONG>(m_wnd.height)};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    m_wnd.hWnd = CreateWindow(
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
        std::string msg = inf.to_string(inf.message->size());

        g_PlatformConsoleWrite(FlogLevel::FLOG_ERROR, msg);

        std::abort();
    };

    libassert::set_failure_handler(libAssertOutput);
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
LRESULT WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    IApp* iApp = reinterpret_cast<IApp*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam)) return true;

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
    case WM_MOUSEWHEEL:
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
            if(iApp) iApp->ToggleFullScreen();
            return S_OK;
        }
        DirectX::Keyboard::ProcessMessage(message, wParam, lParam);
        break;
    }

    case WM_MOUSEACTIVATE: return MA_ACTIVATEANDEAT;

    case WM_PAINT:
    {
        if (iApp and not iApp->isQuitting)
        {
            ImGui_ImplWin32_NewFrame();
            iApp->OnUpdate();
            iApp->OnRender();
        }

        return S_OK;
    }

    case WM_CREATE:
    {
        LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));

        return S_OK;
    }

    case WM_CLOSE:
    {
        ImGui_ImplWin32_Shutdown();
        if (iApp)
        {
            iApp->OnDestroy();
            iApp->isQuitting = true;
        }
        FreeConsole();
        DestroyWindow(hWnd);
        return S_OK;
    }

    case WM_DESTROY:
    {
        PostQuitMessage(S_OK);
        return S_OK;
    }

    case WM_SIZE:
    {
        if (iApp and wParam != SIZE_MINIMIZED)
        {
            const UINT width = LOWORD(lParam);
            const UINT height = HIWORD(lParam);
            iApp->OnResize(width, height);
        }
        return S_OK;
    }

    default: break;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

void Platform::PlatformConsoleWrite(FlogLevel level, const std::string_view& message)
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE)
    {
        OutputDebugStringA(message.data());
        OutputDebugStringA("\n");
        return;
    }

    bool isError = level <= FlogLevel::FLOG_ERROR;

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    WORD originalColor = csbi.wAttributes;

    WORD newColor = static_cast<WORD>(FConsoleColor::Red);

    switch (level)
    {
    case FlogLevel::FLOG_FATAL:  break;
    case FlogLevel::FLOG_ERROR: break;
    case FlogLevel::FLOG_WARN: newColor = static_cast<WORD>(FConsoleColor::Yellow);
        break;
    case FlogLevel::FLOG_INFO: newColor = static_cast<WORD>(FConsoleColor::Blue);
        break;
    case FlogLevel::FLOG_DEBUG: newColor = static_cast<WORD>(FConsoleColor::White);
        break;
    case FlogLevel::FLOG_TRACE: newColor = static_cast<WORD>(FConsoleColor::White);
        break;
    default: break;
    }

    SetConsoleTextAttribute(hConsole, newColor);

    DWORD charsWritten;
    WriteConsoleA(hConsole, message.data(), static_cast<DWORD>(message.length()), &charsWritten, NULL);
    WriteConsoleA(hConsole, "\n", 1, nullptr, nullptr);

    SetConsoleTextAttribute(hConsole, originalColor);
}
