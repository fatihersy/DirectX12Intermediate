#pragma once
#include "core/Defines.h"
#include "StepTimer.h"

class IApp
{
public:
    IApp(uint32_t width, uint32_t height) : im_width(width), im_height(height)
    {

    }
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

    uint32_t im_width{};
    uint32_t im_height{};
    RECT im_windowedRECT{};
    bool im_isFullscreen{};

    constexpr static uint32_t ic_framesInFlight = 2u;
    constexpr static uint32_t ic_maxObjects = 12u;

    float im_aspectRatio{};
    std::filesystem::path im_executablePath;
    std::filesystem::path im_assetsPath;
    StepTimer im_timer;

    ComPtr<ID3D12Device14> im_device;
    ComPtr<IWICImagingFactory2> im_wicFactory;

    bool im_isQuitting{};

protected:
    static IApp* s_instance;
};
