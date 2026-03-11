#pragma once

class IApp
{
public:
    IApp(unsigned int width, unsigned int height, std::wstring title);
    virtual ~IApp();

    static IApp* GetInstance() {
        assert(s_instance != nullptr);
        return s_instance;
    };

    virtual void OnUpdate() = 0;
    virtual void OnRender() = 0;
    virtual void OnInit() = 0;
    virtual void OnDestroy() = 0;
    virtual void OnResize(UINT width, UINT height) = 0;
    virtual void ToggleFullScreen() = 0;

    std::wstring im_title;
    UINT im_width{};
    UINT im_height{};
    RECT im_defaultWindowedRECT{};
    bool im_isFullscreen{};
   
    static constexpr UINT ic_frameCount = 2;
    static constexpr UINT ic_maxObjects = 110;

    float im_aspectRatio{};
    std::wstring im_assetsPath;
    std::wstring im_executablePath;
    StepTimer im_timer;

    inline std::wstring GetAssetFullPath(const LPCWSTR assetName) const {
        return im_assetsPath + assetName;
    }

    protected:
        static IApp* s_instance;
};
