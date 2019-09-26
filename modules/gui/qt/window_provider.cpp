/*****************************************************************************
 * window_provider.hpp : Link between the video window (vout_window) and Qt
 ****************************************************************************
 * Copyright (C) 2019 VideoLabs
 *
 * Authors: Alexandre Janniaux <ajanni@videolabs.io>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "window_provider.hpp"

#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc_vout_window.h>

/* The header is a private header from QPA but will become public when
 * it will stabilize. */
#include <QGuiApplication>
#include <QWindow>
#include QPNI_HEADER

#include "main_interface.hpp"

namespace
{

const vlc_window_provider_ops windowProviderOps =
{
    WaylandWindowProvider::OpenWindow
};

}

WaylandWindowProvider::WaylandWindowProvider(vlc_object_t *obj, MainInterface *intf) :
    provider_wrapper { this, { &windowProviderOps } },
    parent_window {},
    window {},
    intf(intf),
    obj(obj)
{
    parent_window = static_cast<vout_window_t*>(vlc_object_create(obj, sizeof(*parent_window)));
    assert(parent_window);

    parent_window->sys = this;

    QString platformName = QGuiApplication::platformName();

    if (platformName.startsWith(QLatin1String("wayland"), Qt::CaseInsensitive))
    {
        msg_Info(obj, "Using wayland window provider for platform %s", platformName.toUtf8().constData());
        parent_window->type = VOUT_WINDOW_TYPE_WAYLAND;
    }
    else
    {
        msg_Info(obj, "Using dummy window provider");
        parent_window->type = VOUT_WINDOW_TYPE_DUMMY;
        // TODO: We don't handle this yet
    }
}

WaylandWindowProvider::~WaylandWindowProvider()
{
    // TODO:
    //if (module)
    //    vlc_module_unload(module, SubwindowDeactivate);
}

static int SubwindowActivate(void *func, bool forced, va_list args)
{
    using Activate = int (*)(vout_window_t *window, vout_window_t *parent);
    Activate activate = reinterpret_cast<Activate>(func);
    vout_window_t *window = va_arg(args, vout_window_t*);
    vout_window_t *parent_window = va_arg(args, vout_window_t*);
    return activate(window, parent_window);
}

int WaylandWindowProvider::OpenWindow(vlc_window_provider_t *provider,
                                      vout_window_t *window)
{
    printf("WINDOW PROVIDER\n");
    WaylandWindowProvider *qtprovider = container_of(
            provider, WindowProviderWrapper, provider)->handle;

    QPlatformNativeInterface *platform =
        QGuiApplication::platformNativeInterface();

    void *display = platform->nativeResourceForWindow("display", NULL);
    qtprovider->parent_window->display.wl = static_cast<wl_display *>(display);

    void *surface =
        platform->nativeResourceForWindow("surface", qtprovider->intf->windowHandle());
    qtprovider->parent_window->handle.wl = static_cast<wl_surface*>(surface);

    MainInterface *intf = qtprovider->intf;
    QRect inner = intf->geometry();
    int x = inner.x() - intf->x();
    int y = inner.y() - intf->y();

    var_Create(window, "window-x", VLC_VAR_INTEGER);
    var_SetInteger(window, "window-x", x);
    var_Create(window, "window-y", VLC_VAR_INTEGER);
    var_SetInteger(window, "window-y", y);
    var_Create(window, "egl-prevent-terminate", VLC_VAR_BOOL);
    var_SetBool(window, "egl-prevent-terminate", true);

    /* TODO: write helper */
    qtprovider->module = vlc_module_load(vlc_object_logger(window), "vout subwindow",
                                         nullptr, false, SubwindowActivate,
                                         window, qtprovider->parent_window);

    /* We might not be able to create a subwindow for the given window type.
     * In this case, fallback to the vlc core window provider mechanism or
     * stop vout window creation. */
    if (!qtprovider->module)
    {
        return VLC_EGENERIC;
    }

    qtprovider->window = window;

    return VLC_SUCCESS;
    //emit provider->WindowRequested();
}

vlc_window_provider_t *
WaylandWindowProvider::GetProvider()
{
    return &provider_wrapper.provider;
}

void
WaylandWindowProvider::Resize(unsigned width, unsigned height)
{
    printf("PROVIDER RESIZE: %ux%u\n", width, height);
    if (window && module)
    {
        var_SetInteger(vlc_object_parent(window), "width", width);
        var_SetInteger(vlc_object_parent(window), "height", height);
        vout_window_SetSize(window, width, height);
    }

    this->width = width;
    this->height = height;
}
