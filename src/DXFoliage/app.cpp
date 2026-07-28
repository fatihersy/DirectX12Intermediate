#include "stdafx.h"
#include "app.h"

#include "Logger.h"
#include "platform/PlatformUtils.h"

IApp* IApp::s_instance = nullptr;

app::app(uint32_t width, uint32_t height, std::wstring_view title) : IApp(width, height)
{
    s_instance = this;

    im_aspectRatio = static_cast<float>(width) / static_cast<float>(height);

    // Whichever OS this actually compiles for decides what a "window"
    // and an "input source" concretely are — this file never finds out.
    NSPlatform::PlatformHandles platform = NSPlatform::CreatePlatform(NSPlatform::WindowDesc{ .width = width, .height = height, .title = title });
    m_window = std::move(platform.window);
    m_input = std::move(platform.input);

    if (not m_window or not m_input)
    {
        // Platform creation can legitimately fail (no compositor, no
        // display, backend not implemented for this OS yet). Bail out
        // clearly instead of dereferencing null below.
        g_FError("Platform creation failed — no window/input available");
        im_isQuitting = true;
        return;
    }

    m_window->SetResizeCallback([this](uint32_t w, uint32_t h) { OnResize(w, h); });
    m_window->SetCloseCallback([this]() { OnDestroy(); });

    im_assetsPath = std::filesystem::current_path();
    im_executablePath = NSPlatform::GetExecutableDirectory();
}
void app::OnInit()
{
    if (im_isQuitting) return; // platform creation failed in the ctor

    LoadPipeline();
    LoadAssets();

    m_window->Show();

    m_keyboardTracker.Reset();
}
app::~app()
{
    s_instance = nullptr;
}
void app::OnDestroy()
{
    // The DXGI live-object leak report that used to be here moved into
    // DX12RendererBackend::Shutdown — it reports *DX12* object leaks, so
    // it belongs with the backend that created them, and it has to run
    // after those objects are released anyway.
    m_renderer.Shutdown();
};
int app::Run()
{
    if (im_isQuitting) return EXIT_FAILURE; // platform creation failed

    // Rendering is now driven explicitly here, once per PumpEvents() call,
    // instead of implicitly from inside a WM_PAINT handler (see
    // Win32Window's header comment for why that was actually a disguised
    // free-running loop already).
    while (m_window->PumpEvents())
    {
        OnUpdate();
        OnRender();
    }

    return 0;
}
void app::LoadPipeline()
{
    // Everything GPU-side — which backend (--rhi=), the device, the
    // swapchain — is decided and owned below this call. app just says
    // "render into this window at this size".
    if (not m_renderer.Initialize(*m_window, im_width, im_height))
    {
        // Nothing can be drawn without a renderer, so don't enter the
        // frame loop — Run() bails out on this flag.
        g_FError("Renderer initialization failed");
        im_isQuitting = true;
    }
}
void app::LoadAssets()
{

}
void app::OnUpdate()
{
    im_timer.Tick(NULL);

    app::UpdateBindings();
};
void app::OnRender()
{
    m_renderer.BeginFrame();

    m_renderer.DrawScene(nullptr);

    m_renderer.EndFrame();
};

void app::OnResize(uint32_t width, uint32_t height)
{
    if (width == 0 or height == 0 or (width == im_width and height == im_height))
    {
        return;
    }

    im_width = width;
    im_height = height;
    im_aspectRatio = static_cast<float>(im_width) / static_cast<float>(im_height);

    // Previously the renderer was never told about resizes at all, so the
    // swapchain kept its original size. Now that Renderer::Resize goes
    // through the backend cleanly, wire it up.
    m_renderer.Resize(width, height);
};
void app::ToggleFullScreen()
{
    // The actual Win32 fullscreen-toggle logic (SetWindowLong/
    // SetWindowPos/monitor geometry) now lives in Win32Window, since
    // that's genuinely a windowing concern, not an app one. Nothing else
    // in this codebase calls IApp::ToggleFullScreen() directly anymore
    // (Win32WindowProc's Alt+Enter handler calls window->ToggleFullscreen()
    // directly) — this override exists so the method stays meaningful for
    // any future caller that goes through the IApp interface.
    m_window->ToggleFullscreen();
};

void app::UpdateBindings()
{
    m_input->Update();

    const NSInput::KeyboardState kbState = m_input->GetKeyboardState();
    m_keyboardTracker.Update(kbState);

    if (m_keyboardTracker.IsKeyReleased(NSInput::EKey::End))
    {
        m_window->RequestClose();
    }
    if (m_keyboardTracker.IsKeyReleased(NSInput::EKey::Insert))
    {
        const NSInput::MouseState mouseState = m_input->GetMouseState();

        if (mouseState.mode == NSInput::EMouseMode::Relative)
        {
            m_input->SetMouseMode(NSInput::EMouseMode::Absolute);
        }
        else
        {
            m_input->SetMouseMode(NSInput::EMouseMode::Relative);
        }
    }
}
