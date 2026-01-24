#pragma once

#include <IApp.h>

class platform
{
public:
    platform() = default;
    platform(UINT width, UINT height, std::wstring title, HINSTANCE hInstance, int nCmdShow, IApp *iApp);

    static void PlatShowWindow();
    static void PlatMessageDispatch(MSG& msg);

    static int GetCmdShow() { return m_nCmdShow; }
    static HWND GetHWND() { return m_hwnd; }
    static HINSTANCE GetHINSTANCE() { return m_hInstance; }

    static void GetCmdShow(int nCmdShow)          { m_nCmdShow = nCmdShow; }
    static void GetHWND(HWND hwnd)                { m_hwnd = hwnd; }
    static void GetHINSTANCE(HINSTANCE hInstance) { m_hInstance = hInstance; }

protected:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    static int m_nCmdShow;
    static HWND m_hwnd;
    static HINSTANCE m_hInstance;
};

