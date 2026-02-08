#pragma once

class IApp
{
public:
    IApp(unsigned int width, unsigned int height, std::wstring title);
    virtual ~IApp();

    virtual void OnUpdate() = 0;
    virtual void OnRender() = 0;
    virtual void OnInit() = 0;
    virtual void OnDestroy() = 0;
    virtual void OnResize(UINT width, UINT height) = 0;
    virtual void ToggleFullScreen() = 0;

    std::wstring m_title;
    UINT m_width;
    UINT m_height;
    RECT m_defaultWindowedRECT;
    bool m_isFullscreen;
};
