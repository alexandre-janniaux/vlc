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
    WaylandWindowProvider::GetWindow
};

}

WaylandWindowProvider::WaylandWindowProvider(vlc_object_t *obj, MainInterface *intf) :
    provider {},
    parent_window {},
    window {},
    intf(intf),
    obj(obj)
{
    provider.sys = this;
    provider.ops = &windowProviderOps;

    parent_window = static_cast<vout_window_t*>(vlc_object_create(obj, sizeof(*parent_window)));
    window = static_cast<vout_window_t*>(vlc_object_create(obj, sizeof(*window)));

    assert(parent_window);
    assert(window);

    parent_window->type = VOUT_WINDOW_TYPE_WAYLAND;
    parent_window->sys = this;

    if (true)//intf->platform == "wayland")
    {

    }
    else
    {
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

vout_window_t *
WaylandWindowProvider::GetWindow(vlc_window_provider_t *provider,
                                 vlc_object_t *parent)
{
    printf("WINDOW PROVIDER\n");
    WaylandWindowProvider *qtprovider =
        static_cast<WaylandWindowProvider*>(provider->sys);


    QPlatformNativeInterface *platform =
        QGuiApplication::platformNativeInterface();

    void *display = platform->nativeResourceForWindow("display", NULL);
    qtprovider->parent_window->display.wl = static_cast<wl_display *>(display);

    void *surface =
        platform->nativeResourceForWindow("surface", qtprovider->intf->windowHandle());
    qtprovider->parent_window->handle.wl = static_cast<wl_surface*>(surface);

    //if (qtprovider->module)
    //    goto end;

    /* TODO: write helper */
    qtprovider->module = vlc_module_load(vlc_object_logger(parent),
                                         "vout subwindow", nullptr, false,
                                         SubwindowActivate,
                                         qtprovider->window,
                                         qtprovider->parent_window);

    if (!qtprovider->module)
        return nullptr;

end:
    return qtprovider->window;
    //emit provider->WindowRequested();
}

const vlc_window_provider_t *
WaylandWindowProvider::GetProvider()
{
    return &provider;
}
