#pragma once

#include "IApp.h"

#include "core/Defines.h"
#include "platform/PlatformFactory.h"

#include "Renderer.h"

class app : public IApp
{
public:
    // No HINSTANCE, no mention anywhere in this class of which OS it's
    // running on: NSPlatform::CreatePlatform() (see
    // platform/PlatformFactory.h) hides all of that, including reading
    // whatever launch flags (--console-pid=, --wait-for-debugger) it
    // needs straight out of core/Defines.h's g_CmdArguments.
    app(uint32_t width, uint32_t height, std::wstring_view title);
    ~app();

    int Run();

    void OnInit() override;
    void OnUpdate() override;
    void OnRender() override;
    void OnDestroy() override;
    void OnResize(uint32_t width, uint32_t height) override;
    void ToggleFullScreen() override;

private:
    // No GPU types here at all: the device now lives inside whichever
    // renderer backend the factory picked (see rhi/RendererBackendFactory).
    void LoadPipeline();
    void LoadAssets();
    void UpdateBindings();

    // Renderer and Scene are siblings — Scene is "what exists", the
    // Renderer draws it; neither owns the other. (Scene itself lands with
    // the model/content work.)
    Renderer m_renderer;

    std::unique_ptr<NSPlatform::IWindow> m_window;
    std::unique_ptr<NSInput::IInputSource> m_input;
    NSInput::KeyStateTracker m_keyboardTracker;
};
