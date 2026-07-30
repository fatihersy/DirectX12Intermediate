#include "stdafx.h"
#include "WaylandWindow.h"

#include "Logger.h"

#include <wayland-client.h>
#include <xdg-shell-client-protocol.h>
#include <fractional-scale-v1-client-protocol.h>
#include <viewporter-client-protocol.h>

#include <cstring>

namespace NSPlatformWayland
{
    // Wayland's C API takes listener structs of plain function pointers
    // with a void* user-data. These trampolines just forward to the
    // WaylandWindow instance.
    struct WaylandCallbacks
    {
        static void RegistryGlobal(void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version)
        {
            static_cast<WaylandWindow*>(data)->OnRegistryGlobal(registry, name, interface, version);
        }
        static void RegistryGlobalRemove(void*, wl_registry*, uint32_t) {}

        // The compositor pings periodically to check we're still alive;
        // failing to pong makes it treat the app as hung.
        static void WmBasePing(void*, xdg_wm_base* wmBase, uint32_t serial)
        {
            xdg_wm_base_pong(wmBase, serial);
        }

        static void SurfaceConfigure(void* data, xdg_surface* surface, uint32_t serial)
        {
            static_cast<WaylandWindow*>(data)->OnSurfaceConfigure(surface, serial);
        }

        static void ToplevelConfigure(void* data, xdg_toplevel*, int32_t width, int32_t height, wl_array*)
        {
            static_cast<WaylandWindow*>(data)->OnToplevelConfigure(width, height);
        }
        static void ToplevelClose(void* data, xdg_toplevel*)
        {
            static_cast<WaylandWindow*>(data)->OnToplevelClose();
        }
        static void FractionalScale(void* data, wp_fractional_scale_v1*, uint32_t scale120)
        {
            static_cast<WaylandWindow*>(data)->OnFractionalScale(scale120);
        }

        // Integer fallback, used only when the compositor has no
        // fractional-scale support.
        static void SurfacePreferredBufferScale(void* data, wl_surface*, int32_t scale)
        {
            static_cast<WaylandWindow*>(data)->OnPreferredBufferScale(scale);
        }
        static void SurfaceEnter(void*, wl_surface*, wl_output*) {}
        static void SurfaceLeave(void*, wl_surface*, wl_output*) {}
        static void SurfacePreferredBufferTransform(void*, wl_surface*, uint32_t) {}

        static void ToplevelConfigureBounds(void*, xdg_toplevel*, int32_t, int32_t) {}
        static void ToplevelWmCapabilities(void*, xdg_toplevel*, wl_array*) {}
    };

    namespace
    {
        const wl_registry_listener kRegistryListener{
            WaylandCallbacks::RegistryGlobal,
            WaylandCallbacks::RegistryGlobalRemove,
        };
        const xdg_wm_base_listener kWmBaseListener{
            WaylandCallbacks::WmBasePing,
        };
        const xdg_surface_listener kXdgSurfaceListener{
            WaylandCallbacks::SurfaceConfigure,
        };
        const wp_fractional_scale_v1_listener kFractionalScaleListener{
            WaylandCallbacks::FractionalScale,
        };

        // Designated initialisers: enter/leave are v1, the preferred_*
        // events arrived in wl_compositor v6.
        const wl_surface_listener kSurfaceListener{
            .enter = WaylandCallbacks::SurfaceEnter,
            .leave = WaylandCallbacks::SurfaceLeave,
            .preferred_buffer_scale = WaylandCallbacks::SurfacePreferredBufferScale,
            .preferred_buffer_transform = WaylandCallbacks::SurfacePreferredBufferTransform,
        };

        const xdg_toplevel_listener kToplevelListener{
            WaylandCallbacks::ToplevelConfigure,
            WaylandCallbacks::ToplevelClose,
            WaylandCallbacks::ToplevelConfigureBounds,
            WaylandCallbacks::ToplevelWmCapabilities,
        };
    }

    void WaylandWindow::OnRegistryGlobal(wl_registry* registry, uint32_t name, const char* interface, uint32_t version)
    {
        // The compositor advertises what it supports; we bind the two
        // globals we need. Binding min(ours, theirs) keeps us working
        // against both older and newer compositors.
        if (std::strcmp(interface, wl_compositor_interface.name) == 0)
        {
            // Version 6 for wl_surface.preferred_buffer_scale, the integer
            // scaling fallback. Older compositors simply never send it.
            const uint32_t bindVersion = version < 6u ? version : 6u;
            m_compositor = static_cast<wl_compositor*>(wl_registry_bind(registry, name, &wl_compositor_interface, bindVersion));
        }
        else if (std::strcmp(interface, wp_fractional_scale_manager_v1_interface.name) == 0)
        {
            m_fractionalScaleManager = static_cast<wp_fractional_scale_manager_v1*>(
                wl_registry_bind(registry, name, &wp_fractional_scale_manager_v1_interface, 1));
        }
        else if (std::strcmp(interface, wp_viewporter_interface.name) == 0)
        {
            m_viewporter = static_cast<wp_viewporter*>(
                wl_registry_bind(registry, name, &wp_viewporter_interface, 1));
        }
        else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0)
        {
            const uint32_t bindVersion = version < 1u ? version : 1u;
            m_wmBase = static_cast<xdg_wm_base*>(wl_registry_bind(registry, name, &xdg_wm_base_interface, bindVersion));
            xdg_wm_base_add_listener(m_wmBase, &kWmBaseListener, this);
        }
    }

    void WaylandWindow::OnSurfaceConfigure(xdg_surface* surface, uint32_t serial)
    {
        // Acknowledging the configure is what makes the surface become
        // visible; skipping it means the window never maps.
        xdg_surface_ack_configure(surface, serial);
    }

    void WaylandWindow::OnToplevelConfigure(int32_t width, int32_t height)
    {
        // width/height are 0 when the compositor lets us pick our own size.
        if (width <= 0 or height <= 0) return;

        const uint32_t newWidth = static_cast<uint32_t>(width);
        const uint32_t newHeight = static_cast<uint32_t>(height);
        if (newWidth == m_width and newHeight == m_height) return;

        m_width = newWidth;
        m_height = newHeight;

        // ApplyScale re-declares the viewport destination for the new
        // logical size and fires the resize callback itself.
        ApplyScale();
    }

    void WaylandWindow::OnFractionalScale(uint32_t scale120)
    {
        if (scale120 == 0 or scale120 == m_scale120) return;

        m_scale120 = scale120;
        g_FDebug("Wayland: display scale %u%%, framebuffer %ux%u",
            (scale120 * 100u) / 120u, FramebufferWidth(), FramebufferHeight());
        ApplyScale();
    }

    void WaylandWindow::OnPreferredBufferScale(int32_t scale)
    {
        // Ignored when fractional scaling is active: that path is exact,
        // and this one would round 150% down to 100%.
        if (m_fractionalScale or scale <= 0) return;

        const uint32_t asScale120 = static_cast<uint32_t>(scale) * 120u;
        if (asScale120 == m_scale120) return;

        m_scale120 = asScale120;
        ApplyScale();
    }

    void WaylandWindow::ApplyScale()
    {
        if (m_viewport)
        {
            // Fractional path: the buffer is physical-sized and the
            // viewport declares the logical area it covers. buffer_scale
            // stays 1 - the two mechanisms are alternatives, not layers.
            wp_viewport_set_destination(m_viewport,
                static_cast<int32_t>(m_width), static_cast<int32_t>(m_height));
        }
        else
        {
            // Integer path. Only whole numbers survive the division, which
            // is precisely why the fractional protocol exists.
            wl_surface_set_buffer_scale(m_surface, static_cast<int32_t>(m_scale120 / 120u));
        }

        wl_surface_commit(m_surface);

        // The logical size did not change, but the pixel count did - so
        // the swapchain has to be rebuilt. The resize callback carries
        // logical units; the renderer reads FramebufferWidth/Height.
        if (m_resizeCallback) m_resizeCallback(m_width, m_height);
    }

    void WaylandWindow::OnToplevelClose()
    {
        if (m_closeCallback) m_closeCallback();
        m_shouldClose = true;
    }

    bool WaylandWindow::Create(const NSPlatform::WindowDesc& desc)
    {
        m_width = desc.width;
        m_height = desc.height;

        m_display = wl_display_connect(nullptr);
        if (not m_display)
        {
            g_FError("Wayland: no compositor (is WAYLAND_DISPLAY set?)");
            return false;
        }

        m_registry = wl_display_get_registry(m_display);
        wl_registry_add_listener(m_registry, &kRegistryListener, this);
        // First roundtrip collects the advertised globals, second lets any
        // follow-up events from binding them settle.
        wl_display_roundtrip(m_display);
        wl_display_roundtrip(m_display);

        if (not m_compositor or not m_wmBase)
        {
            g_FError("Wayland: compositor lacks wl_compositor or xdg_wm_base");
            return false;
        }

        m_surface = wl_compositor_create_surface(m_compositor);
        wl_surface_add_listener(m_surface, &kSurfaceListener, this);

        // Both are per-surface objects, so they can only be made once the
        // surface exists. Either may be absent - a compositor need not
        // support fractional scaling, and then we fall back to the integer
        // path or to no scaling at all.
        if (m_fractionalScaleManager and m_viewporter)
        {
            m_fractionalScale = wp_fractional_scale_manager_v1_get_fractional_scale(
                m_fractionalScaleManager, m_surface);
            wp_fractional_scale_v1_add_listener(m_fractionalScale, &kFractionalScaleListener, this);

            m_viewport = wp_viewporter_get_viewport(m_viewporter, m_surface);
        }

        m_xdgSurface = xdg_wm_base_get_xdg_surface(m_wmBase, m_surface);
        xdg_surface_add_listener(m_xdgSurface, &kXdgSurfaceListener, this);

        m_toplevel = xdg_surface_get_toplevel(m_xdgSurface);
        xdg_toplevel_add_listener(m_toplevel, &kToplevelListener, this);

        // desc.title is wide (the Win32 side needs that); Wayland wants
        // UTF-8. These titles are ASCII literals today, so a narrowing
        // copy is sufficient — revisit if titles ever become localised.
        std::string title;
        title.reserve(desc.title.size());
        for (wchar_t ch : desc.title) title.push_back(static_cast<char>(ch));
        xdg_toplevel_set_title(m_toplevel, title.c_str());
        xdg_toplevel_set_app_id(m_toplevel, "DXFoliage");

        // A surface only becomes real after a commit + the compositor's
        // configure round-trip.
        wl_surface_commit(m_surface);
        wl_display_roundtrip(m_display);

        return true;
    }

    void WaylandWindow::Show()
    {
        // Nothing to do: unlike Win32's ShowWindow, a Wayland surface
        // becomes visible once it has been committed with content. The
        // renderer's first present is what actually maps it.
        wl_surface_commit(m_surface);
        wl_display_roundtrip(m_display);
    }

    void WaylandWindow::Destroy()
    {
        if (m_viewport) { wp_viewport_destroy(m_viewport); m_viewport = nullptr; }
        if (m_fractionalScale) { wp_fractional_scale_v1_destroy(m_fractionalScale); m_fractionalScale = nullptr; }
        if (m_viewporter) { wp_viewporter_destroy(m_viewporter); m_viewporter = nullptr; }
        if (m_fractionalScaleManager) { wp_fractional_scale_manager_v1_destroy(m_fractionalScaleManager); m_fractionalScaleManager = nullptr; }
        if (m_toplevel) { xdg_toplevel_destroy(m_toplevel); m_toplevel = nullptr; }
        if (m_xdgSurface) { xdg_surface_destroy(m_xdgSurface); m_xdgSurface = nullptr; }
        if (m_surface) { wl_surface_destroy(m_surface); m_surface = nullptr; }
        if (m_wmBase) { xdg_wm_base_destroy(m_wmBase); m_wmBase = nullptr; }
        if (m_registry) { wl_registry_destroy(m_registry); m_registry = nullptr; }
        if (m_display) { wl_display_disconnect(m_display); m_display = nullptr; }
    }

    WaylandWindow::~WaylandWindow()
    {
        Destroy();
    }

    void WaylandWindow::RequestClose()
    {
        // No compositor round-trip needed — this is our own intent, and
        // it mirrors Win32Window posting itself a WM_CLOSE.
        OnToplevelClose();
    }

    bool WaylandWindow::PumpEvents()
    {
        if (m_shouldClose) return false;

        // Flush our outgoing requests, then dispatch whatever already
        // arrived. Deliberately non-blocking: the caller drives the frame
        // loop, so we must not wait for compositor events.
        wl_display_flush(m_display);
        if (wl_display_dispatch_pending(m_display) < 0)
        {
            g_FError("Wayland: connection lost");
            return false;
        }

        return not m_shouldClose;
    }

    NSPlatform::NativeWindowHandle WaylandWindow::GetNativeHandle() const
    {
        // Vulkan's VK_KHR_wayland_surface needs both, which is why
        // NativeWindowHandle carries two pointers.
        return NSPlatform::NativeWindowHandle{ .a = m_display, .b = m_surface };
    }

    void WaylandWindow::ToggleFullscreen()
    {
        m_isFullscreen = not m_isFullscreen;

        // The compositor decides the resulting size and tells us via a
        // configure event — unlike Win32, we don't compute monitor
        // geometry ourselves.
        if (m_isFullscreen) xdg_toplevel_set_fullscreen(m_toplevel, nullptr);
        else                xdg_toplevel_unset_fullscreen(m_toplevel);
    }
}