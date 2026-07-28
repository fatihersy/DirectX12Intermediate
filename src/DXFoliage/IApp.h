#pragma once
#include "StepTimer.h"

#include <cstdint>
#include <filesystem>

// The shared app base class — deliberately holds nothing DirectX- or
// Windows-specific. im_windowedRECT/im_isFullscreen used to live here too;
// removed as dead weight once fullscreen toggling moved into Win32Window
// (nothing reads them anymore — verified). im_device/im_wicFactory moved
// down into the concrete app class (app.h) — they're DX12/WIC-specific,
// which is fine for the concrete Windows app today, but doesn't belong on
// the interface every future platform's app class will also inherit from.
class IApp
{
public:
    IApp(uint32_t width, uint32_t height) : im_width(width), im_height(height)
    {

    }
    virtual ~IApp() {}

    static IApp* GetInstance()
    {
        ASSERT(s_instance != nullptr);
        return s_instance;
    }

    virtual void OnInit() = 0;
    virtual void OnUpdate() = 0;
    virtual void OnRender() = 0;
    virtual void OnDestroy() = 0;
    virtual void OnResize(uint32_t width, uint32_t height) = 0;
    virtual void ToggleFullScreen() = 0;

    uint32_t im_width{};
    uint32_t im_height{};

    constexpr static uint32_t ic_framesInFlight = 2u;
    constexpr static uint32_t ic_maxObjects = 12u;

    float im_aspectRatio{};
    std::filesystem::path im_executablePath;
    std::filesystem::path im_assetsPath;
    StepTimer im_timer;

    bool im_isQuitting{};

protected:
    static IApp* s_instance;
};
