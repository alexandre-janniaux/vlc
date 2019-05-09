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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "window_provider.hpp"

#include <vlc_modules.h>

/* The header is a private header from QPA but will become public when
 * it will stabilize. */
#include <QGuiApplication>
#include <QWindow>
#include QPNI_HEADER

#include "main_interface.hpp"


WaylandWindowProvider::WaylandWindowProvider(MainInterface *intf) :
    parent_window {}
{
    QPlatformNativeInterface *platform =
        QGuiApplication::platformNativeInterface();

    if (intf->platform == "wayland")
    {
        parent_window.type = VOUT_WINDOW_TYPE_WAYLAND;
        parent_window.sys = this;

        void *display = platform->nativeResourceForWindow("display", NULL);
        parent_window.display.wl = static_cast<wl_display *>(display);

        void *surface =
            platform->nativeResourceForWindow("surface", intf->windowHandle());
        parent_window.handle.wl = static_cast<wl_surface*>(surface);

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
WaylandWindowProvider::GetWindow(vlc_window_provider_t *opaque_provider,
                                 vlc_object_t *parent)
{
    WaylandWindowProvider *provider =
        static_cast<WaylandWindowProvider*>(opaque_provider->sys);

    if (provider->module)
        goto end;

    /* TODO: write helper */
    provider->module = vlc_module_load(vlc_object_logger(parent),
                                       "vout subwindow", NULL, false,
                                       SubwindowActivate,
                                       &provider->window,
                                       provider->parent_window);

end:
    return &provider->window;
    //emit provider->WindowRequested();
}
