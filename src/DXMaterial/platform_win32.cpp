#include "stdafx.h"
#include "platform_win32.h"

int platform::m_nCmdShow = 0;
HWND platform::m_hwnd = nullptr;
HINSTANCE platform::m_hInstance = nullptr;

platform::platform(UINT width, UINT height, std::wstring title, HINSTANCE hInstance, int nCmdShow, IApp* iApp)
{
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

    RECT windowRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    m_hwnd = CreateWindow(
        windowClass.lpszClassName,
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
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
        case WM_CREATE:
        {
            LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));

            return 0;
        }
        case WM_KEYDOWN:
        {
            if (iApp)
            {
                iApp->OnKeyDown(static_cast<UINT8>(wParam));
            }
            return 0;
        }
        case WM_KEYUP:
        {
            if (iApp)
            {
                iApp->OnKeyUp(static_cast<UINT8>(wParam));
            }
            return 0;
        }
        case WM_PAINT:
        {
            if (iApp)
            {
                iApp->OnUpdate();
                iApp->OnRender();
            }
            return 0;
        }
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }

        default: return DefWindowProc(hWnd, message, wParam, lParam);
    }
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
