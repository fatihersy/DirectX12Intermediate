#pragma once
#include "core/Defines.h"
#include "StepTimer.h"

class IApp
{
public:
    IApp(UINT width, UINT height, std::wstring title) : im_width(width), im_height(height), im_title(title) {}
    virtual ~IApp() {}

    static IApp* GetInstance()
    {
        ASSERT(s_instance);
        return s_instance;
    }

    virtual void OnInit() = 0;
    virtual void OnUpdate() = 0;
    virtual void OnRender() = 0;
    virtual void OnDestroy() = 0;
    virtual void OnResize(UINT width, UINT height) = 0;
    virtual void ToggleFullScreen() = 0;

    std::wstring im_title;
    UINT im_width{};
    UINT im_height{};
    RECT im_windowedRECT{};
    bool im_isFullScreen{};

    constexpr static UINT ic_framesInFlight = 2u;
    constexpr static UINT ic_maxObjects = 128u;

    float im_aspectRatio{};
    std::filesystem::path im_executablePath;
    std::filesystem::path im_assetsPath;
    StepTimer m_timer;

    ComPtr<ID3D12Device14> im_device;
    ComPtr<IWICImagingFactory2> im_wicFactory;

    bool isQuitting{};
protected:
    static IApp* s_instance;
};
