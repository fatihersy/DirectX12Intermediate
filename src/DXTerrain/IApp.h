#pragma once
class IApp
{
public:
    IApp(UINT width, UINT height, std::wstring_view title) : im_width(width), im_height(height), im_title(title) {}
    virtual ~IApp() {}

    static IApp* GetInstance()
    {
        assert(s_instance);
        return s_instance;
    }

    virtual void OnInit() = 0;
    virtual void OnUpdate() = 0;
    virtual void OnRender() = 0;
    virtual void OnDestroy() = 0;
    virtual void OnResize(UINT width, UINT height) = 0;
    virtual void ToggleFullScreen() = 0;

    float im_aspectRatio{};
    UINT im_width{};
    UINT im_height{};
    std::wstring_view im_title;

    std::wstring im_executablePath;
    std::wstring im_assetsPath;

    constexpr static UINT ic_framesInFlight = 2u;

protected:
    static IApp* s_instance;
};

