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

    std::wstring m_title;
    UINT m_width{};
    UINT m_height{};
    RECT m_defaultWindowedRECT{};
    bool m_isFullscreen{};
   
    static constexpr UINT c_frameCount = 2;
    static constexpr UINT c_maxObjects = 110;

    float m_aspectRatio{};
    std::wstring m_assetsPath;
    std::wstring m_executablePath;
    StepTimer m_timer;

    inline std::wstring GetAssetFullPath(const LPCWSTR assetName) const {
        return m_assetsPath + assetName;
    }

    protected:
        static IApp* s_instance;
};
