#pragma once
#include "Logger.h"

class IApp;

struct SWindow {
    HINSTANCE hInstance = nullptr;
    HWND hWnd = nullptr;
    IApp* pApp = nullptr;
    int nCmdShow{};
    UINT width{};
    UINT height{};
    std::wstring title;
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

    template<typename... Args>
    static void FDebug(const char* fmt, Args&&... args) {
        PlatformConsoleWrite(FlogLevel::FLOG_DEBUG, NSTool::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void FError(const char* fmt, Args&&... args) {
        PlatformConsoleWrite(FlogLevel::FLOG_ERROR, NSTool::format(fmt, std::forward<Args>(args)...));
    }
protected:
    static void PlatformConsoleWrite(FlogLevel level, const std::string_view& message);
private:
    SWindow m_wnd;
};
