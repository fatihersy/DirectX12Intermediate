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

    UINT m_width;
    UINT m_height;
    std::wstring m_title;
};
