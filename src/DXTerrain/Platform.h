#pragma once

class IApp;

struct SWindow {
    HINSTANCE hInstance = nullptr;
    HWND hWnd = nullptr;
    IApp* pApp = nullptr;
    int nCmdShow{};
    UINT width{};
    UINT height{};
    std::wstring_view title;
};

class Platform
{
public:
    Platform(){}
    Platform(SWindow wnd);
    ~Platform();

    void ShowWindow();
    void Dispatch(MSG& msg);

    const HWND GetWindow() const
    {
        return m_wnd.hWnd;
    }


protected:

private:
    SWindow m_wnd;

};

