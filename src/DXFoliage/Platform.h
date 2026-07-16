#pragma once

class IApp;

struct SWindow
{
    HINSTANCE hInstance = nullptr;
    HWND hWnd = nullptr;
    IApp* pApp;
    int nCmdShow{};
    UINT width{};
    UINT height{};
    std::wstring_view title;
};

class Platform
{
public:
    Platform(){};
    ~Platform();

    bool OnInit(SWindow wnd);

    void ShowWindow();
    void Dispatch(MSG& msg);

    const HWND GetWindow() const {
        return m_wnd.hWnd;
    }

protected:
    static void PlatformConsoleWrite(std::uint8_t level, const std::string_view message);

private:
    SWindow m_wnd;
};
