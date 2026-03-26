#include "stdafx.h"
#include "Platform.h"

#include "IApp.h"

static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

Platform::Platform(SWindow wnd)
{
    assert(wnd.pApp);

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
}

Platform::~Platform()
{

}

void Platform::ShowWindow()
{
    ::ShowWindow(m_wnd.hWnd, m_wnd.nCmdShow);
}

void Platform::Dispatch(MSG& msg)
{
    if (PeekMessage(&msg, m_wnd.hWnd, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

LRESULT WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
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
    case WM_MOUSEMOVE:
        break;

    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
        break;
    case WM_SYSKEYDOWN:
    {
        if (wParam == VK_RETURN and (lParam & 0x60000000) == 0x20000000)
        {
            if(iApp) iApp->ToggleFullScreen();
            return S_OK;
        }

        break;
    }

    case WM_MOUSEACTIVATE: return MA_ACTIVATEANDEAT;

    case WM_PAINT:
    {
        if (iApp)
        {
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
        if (iApp)
        {
            iApp->OnDestroy();
        }

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
