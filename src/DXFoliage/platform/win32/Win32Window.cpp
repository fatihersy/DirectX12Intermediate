#include "stdafx.h"
#include "Win32Window.h"

#include "imgui_impl_win32.h"

#include "directxtk12/Keyboard.h"
#include "directxtk12/Mouse.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace NSPlatformWin32
{
    LRESULT CALLBACK Win32WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam)) return true;

        Win32Window* window = reinterpret_cast<Win32Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

        switch (message)
        {
            case WM_ACTIVATEAPP:
            {
                if (wParam == WA_INACTIVE) break;

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
            case WM_MOUSEMOVE:
                DirectX::Mouse::ProcessMessage(message, wParam, lParam);
                break;

            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYUP:
                DirectX::Keyboard::ProcessMessage(message, wParam, lParam);
                break;
            case WM_SYSKEYDOWN:
            {
                if (wParam == VK_RETURN and (lParam & 0x60000000) == 0x20000000) // Alt + Enter
                {
                    ASSERT_VAL(window != nullptr, "Invalid window handle");

                    window->ToggleFullscreen();
                    return 0;
                }
                DirectX::Keyboard::ProcessMessage(message, wParam, lParam);
                break;
            }

            case WM_MOUSEACTIVATE: return MA_ACTIVATEANDEAT;

            case WM_NCCREATE:
            {
                CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
                window = reinterpret_cast<Win32Window*>(pCreate->lpCreateParams);

                SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));

                return DefWindowProc(hWnd, message, wParam, lParam);
            }
            case WM_CLOSE:
            {
                ASSERT_VAL(window != nullptr, "Invalid window handle");

                // Let the app tear down its renderer/scene *before* we
                // shut down ImGui and destroy the window — same ordering
                // the original Platform.cpp used (iApp->OnDestroy() first).
                if (window->m_closeCallback) window->m_closeCallback();

                window->Destroy();

                return 0;
            }
            case WM_DESTROY:
            {
                PostQuitMessage(EXIT_SUCCESS);
                return 0;
            }
            case WM_SIZE:
            {
                ASSERT_VAL(window != nullptr, "Invalid window handle");

                if (wParam != SIZE_MINIMIZED)
                {
                    const uint32_t width = LOWORD(lParam);
                    const uint32_t height = HIWORD(lParam);

                    window->m_width = width;
                    window->m_height = height;

                    if (window->m_resizeCallback) window->m_resizeCallback(width, height);
                }

                return 0;
            }

            default: break;
        }

        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    bool Win32Window::Create(const NSPlatform::WindowDesc& desc)
    {
        m_width = desc.width;
        m_height = desc.height;

        // GetModuleHandle(nullptr) returns the same HINSTANCE a normal
        // EXE would have received as WinMain's first parameter — this
        // way Win32Window doesn't need WinMain to hand it anything.
        m_hInstance = GetModuleHandle(nullptr);

        WNDCLASSEX wndClass{};
        wndClass.cbSize = sizeof(decltype(wndClass));
        wndClass.style = CS_HREDRAW | CS_VREDRAW;
        wndClass.lpfnWndProc = Win32WindowProc;
        wndClass.hInstance = m_hInstance;
        wndClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wndClass.lpszClassName = L"WinMain";
        RegisterClassEx(&wndClass);

        RECT rect{ 0L, 0L, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

        // desc.title is a non-owning wstring_view; this assumes (as the
        // original code did) that it points at a null-terminated buffer,
        // true for the string-literal titles main.cpp passes today.
        m_hWnd = CreateWindow
        (
            wndClass.lpszClassName,
            desc.title.data(),
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            rect.right - rect.left,
            rect.bottom - rect.top,
            nullptr,
            nullptr,
            m_hInstance,
            this
        );

        m_windowedRect = { 0L, 0L, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        return m_hWnd != nullptr;
    }

    void Win32Window::Show()
    {
        ImGui_ImplWin32_Init(m_hWnd);

        ::ShowWindow(m_hWnd, SW_SHOW);
    }

    void Win32Window::Destroy()
    {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        FreeConsole();

        if (m_hWnd)
        {
            DestroyWindow(m_hWnd);
            m_hWnd = nullptr;
        }
    }

    Win32Window::~Win32Window() = default;

    void Win32Window::RequestClose()
    {
        PostMessage(m_hWnd, WM_CLOSE, 0, 0);
    }

    bool Win32Window::PumpEvents()
    {
        MSG msg{};
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) return false;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // The original code called this from inside the WM_PAINT handler,
        // once per (accidentally very frequent) paint message. Now that
        // rendering is driven explicitly by the caller once per
        // PumpEvents() call, this is the equivalent "once per frame" spot
        // for it — still entirely inside the Win32-specific window code,
        // so app.cpp never has to know ImGui's Win32 backend exists.
        ImGui_ImplWin32_NewFrame();

        return true;
    }

    NSPlatform::NativeWindowHandle Win32Window::GetNativeHandle() const
    {
        return NSPlatform::NativeWindowHandle{ .a = m_hInstance, .b = m_hWnd };
    }

    void Win32Window::ToggleFullscreen()
    {
        m_isFullscreen = not m_isFullscreen;

        MONITORINFO monitorInfo{ sizeof(MONITORINFO) };
        GetMonitorInfo(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTOPRIMARY), &monitorInfo);
        const uint32_t monitorWidth = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
        const uint32_t monitorHeight = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
        const uint32_t monitorLeft = monitorInfo.rcMonitor.left;
        const uint32_t monitorTop = monitorInfo.rcMonitor.top;

        if (m_isFullscreen)
        {
            SetWindowLong(m_hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
            SetWindowPos(m_hWnd, HWND_TOP, monitorLeft, monitorTop, monitorWidth, monitorHeight, SWP_FRAMECHANGED | SWP_SHOWWINDOW);

            m_width = monitorWidth;
            m_height = monitorHeight;
            if (m_resizeCallback) m_resizeCallback(monitorWidth, monitorHeight);
            return;
        }

        const uint32_t windowWidth = m_windowedRect.right - m_windowedRect.left;
        const uint32_t windowHeight = m_windowedRect.bottom - m_windowedRect.top;

        const uint32_t windowLeft = monitorLeft + monitorWidth / 2 - windowWidth / 2;
        const uint32_t windowTop = monitorTop + monitorHeight / 2 - windowHeight / 2;

        SetWindowLong(m_hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
        SetWindowPos(m_hWnd, HWND_TOP, windowLeft, windowTop, windowWidth, windowHeight, SWP_FRAMECHANGED | SWP_SHOWWINDOW);

        m_width = windowWidth;
        m_height = windowHeight;
        if (m_resizeCallback) m_resizeCallback(windowWidth, windowHeight);
    }
}
