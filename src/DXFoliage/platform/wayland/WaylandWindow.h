#pragma once

#include "platform/IWindow.h"

#include <cstdint>
#include <string>

struct wl_display;
struct wl_registry;
struct wl_compositor;
struct wl_surface;
struct xdg_wm_base;
struct xdg_surface;
struct xdg_toplevel;
struct wp_fractional_scale_manager_v1;
struct wp_fractional_scale_v1;
struct wp_viewport;
struct wp_viewporter;

// Wayland implementation of IWindow.
//
// Wayland splits what Win32 calls "a window" across three objects:
//   wl_surface   — a rectangle of pixels, with no window semantics at all
//   xdg_surface  — gives that surface a window role
//   xdg_toplevel — makes it an application window (title, close, resize)
// Those last two come from the xdg-shell protocol, whose C bindings are
// generated from XML at init time (see init.py GenerateWaylandProtocols).
//
// HiDPI works through two protocols rather than one. wl_surface's
// set_buffer_scale takes an INTEGER, so it cannot express 150% - the
// common case on a 4K laptop panel. Fractional scaling instead uses
// wp_fractional_scale_v1 (the compositor reports the scale in 120ths) plus
// wp_viewport (the buffer is physical-sized and the viewport declares the
// logical size it should occupy), with buffer_scale left at 1.
//
// Three tiers, best first:
//   1. fractional-scale + viewporter -> exact, any scale
//   2. wl_surface.preferred_buffer_scale -> integer only (compositor v6)
//   3. neither advertised -> scale 1, same as before this existed
//
// Unlike Win32 there is no WM_PAINT and no blocking message loop:
// PumpEvents() dispatches whatever the compositor has sent and returns
// immediately, so the caller drives rendering — which is exactly the
// shape app::Run() already uses.
namespace NSPlatformWayland
{
    class WaylandWindow final : public NSPlatform::IWindow
    {
    public:
        WaylandWindow() = default;
        ~WaylandWindow() override;

        bool Create(const NSPlatform::WindowDesc& desc) override;
        void Show() override;
        void Destroy() override;
        void RequestClose() override;

        bool PumpEvents() override;

        NSPlatform::NativeWindowHandle GetNativeHandle() const override;

        // WaylandInput opens its own wl_registry on this same connection;
        // see its header for why the seat is not bound here.
        wl_display* GetDisplay() const { return m_display; }

        uint32_t Width() const override { return m_width; }
        uint32_t Height() const override { return m_height; }

        uint32_t FramebufferWidth() const override { return Scaled(m_width); }
        uint32_t FramebufferHeight() const override { return Scaled(m_height); }

        void ToggleFullscreen() override;

        void SetResizeCallback(NSPlatform::FnWindowResize callback) override { m_resizeCallback = std::move(callback); }
        void SetCloseCallback(NSPlatform::FnWindowClose callback) override { m_closeCallback = std::move(callback); }

    private:
        // Compositor event handlers. Declared here so the C listener
        // structs in the .cpp can reach the instance state.
        void OnRegistryGlobal(wl_registry* registry, uint32_t name, const char* interface, uint32_t version);
        void OnToplevelConfigure(int32_t width, int32_t height);
        void OnToplevelClose();
        void OnSurfaceConfigure(xdg_surface* surface, uint32_t serial);

        // scale120 is the scale multiplied by 120, which is how
        // fractional-scale-v1 avoids floating point on the wire: 150% is
        // 180, 125% is 150.
        void OnFractionalScale(uint32_t scale120);
        void OnPreferredBufferScale(int32_t scale);

        friend struct WaylandCallbacks;

        wl_display* m_display{ nullptr };
        wl_registry* m_registry{ nullptr };
        wl_compositor* m_compositor{ nullptr };
        wl_surface* m_surface{ nullptr };
        xdg_wm_base* m_wmBase{ nullptr };
        xdg_surface* m_xdgSurface{ nullptr };
        xdg_toplevel* m_toplevel{ nullptr };

        // Rounds UP: a buffer one pixel short of what the compositor
        // expects gets stretched, which is the artefact this whole
        // mechanism exists to avoid.
        uint32_t Scaled(uint32_t logical) const { return (logical * m_scale120 + 119u) / 120u; }
        void ApplyScale();

        wp_fractional_scale_manager_v1* m_fractionalScaleManager{ nullptr };
        wp_fractional_scale_v1* m_fractionalScale{ nullptr };
        wp_viewporter* m_viewporter{ nullptr };
        wp_viewport* m_viewport{ nullptr };

        // 120 == 1.0. Held in 120ths so the integer and fractional paths
        // share one representation.
        uint32_t m_scale120{ 120 };

        uint32_t m_width{};
        uint32_t m_height{};
        bool m_isFullscreen{};
        bool m_shouldClose{};

        NSPlatform::FnWindowResize m_resizeCallback;
        NSPlatform::FnWindowClose m_closeCallback;
    };
}
