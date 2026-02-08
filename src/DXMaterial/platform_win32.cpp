#include "stdafx.h"

#include "platform_win32.h"
#include "windowsx.h"
#include "io.h"
#include "fcntl.h"

#include "directxtk12/Keyboard.h"
#include "directxtk12/Mouse.h"

int platform::m_nCmdShow = 0;
HWND platform::m_hwnd = nullptr;
HINSTANCE platform::m_hInstance = nullptr;

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

platform::platform(UINT width, UINT height, std::wstring title, HINSTANCE hInstance, int nCmdShow, IApp* iApp)
{
    if (not iApp)
    {
        throw std::runtime_error("lpParam is invalid");
    }

    if (AllocConsole())
    {
        FILE* dummyFile;
        freopen_s(&dummyFile, "CONOUT$", "w", stdout);
        freopen_s(&dummyFile, "CONOUT$", "w", stderr);
        freopen_s(&dummyFile, "CONIN$", "r", stdin);

        SetConsoleTitle(L"Debug Console");

        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;

        GetConsoleMode(hOut, &dwMode);
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
    }
    g_PlatformConsoleWrite = &platform::PlatformConsoleWrite;

    m_hInstance = hInstance;
    m_nCmdShow = nCmdShow;

    WNDCLASSEX windowClass{};
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = m_hInstance;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpszClassName = L"WinMain";
    RegisterClassEx(&windowClass);

    AdjustWindowRect(&iApp->m_defaultWindowedRECT, WS_OVERLAPPEDWINDOW, FALSE);

    m_hwnd = CreateWindow(
        windowClass.lpszClassName,
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        iApp->m_defaultWindowedRECT.right - iApp->m_defaultWindowedRECT.left,
        iApp->m_defaultWindowedRECT.bottom - iApp->m_defaultWindowedRECT.top,
        nullptr,
        nullptr,
        m_hInstance,
        iApp
    );
}

LRESULT CALLBACK platform::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
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

        case WM_ACTIVATE:
        case WM_INPUT:
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MOUSEWHEEL:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        case WM_MOUSEHOVER: DirectX::Mouse::ProcessMessage(message, wParam, lParam);
            break;
        
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
            DirectX::Keyboard::ProcessMessage(message, wParam, lParam);
            break;
        case WM_SYSKEYDOWN:
            if (wParam == VK_RETURN and (lParam & 0x60000000) == 0x20000000) // Alt + Enter
            {
                if (iApp)
                {
                    iApp->ToggleFullScreen();
                }
                return 0;
            }
            DirectX::Keyboard::ProcessMessage(message, wParam, lParam);
            break;
        
        case WM_MOUSEACTIVATE:
            return MA_ACTIVATEANDEAT;
        
        case WM_PAINT:
        {
            if (iApp)
            {
                iApp->OnUpdate();
                iApp->OnRender();
            }
            return 0;
        }
        case WM_CREATE:
        {
            LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
        
            return 0;
        }

        case WM_CLOSE:
        {
            if (iApp)
            {
                iApp->OnDestroy();
            }
            DestroyWindow(hWnd);
            return 0;
        }
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }
        case WM_SIZE:
        {
            if (iApp and wParam != SIZE_MINIMIZED)
            {
                const UINT width = LOWORD(lParam);
                const UINT height = HIWORD(lParam);
                iApp->OnResize(width, height);
            }
            return 0;
        }
        
        default: { break; }
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

void platform::PlatShowWindow()
{
    ShowWindow(m_hwnd, m_nCmdShow);
}
void platform::PlatMessageDispatch(MSG& msg)
{
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}


void platform::PlatformConsoleWrite(FlogLevel level, const std::string_view& message)
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE)
    {
        OutputDebugStringA(message.data());
        return;
    }

    bool isError = level <= FlogLevel::FLOG_ERROR;

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    WORD originalColor = csbi.wAttributes;

    SetConsoleTextAttribute(hConsole, static_cast<WORD>(isError ? FConsoleColor::Red : FConsoleColor::Green));

    DWORD charsWritten;
    WriteConsoleA(hConsole, message.data(), static_cast<DWORD>(message.length()), &charsWritten, NULL);

    SetConsoleTextAttribute(hConsole, originalColor);
}


