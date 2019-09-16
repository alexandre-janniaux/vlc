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

static void Resized(vout_window_t *wnd, unsigned int width, unsigned int height)
{
    vout_thread_t *vout = (vout_thread_t *)vlc_object_parent(wnd);
    msg_Err(wnd, "[window provider] resized to %ux%u", width, height);
}

const vout_window_callbacks voutWindowCbs =
{
    Resized, //Resized,
    nullptr, //Closed,
    nullptr, //StateChanged,
    nullptr, //Windowed,
    nullptr, //Fullscreened,
    nullptr, //MouseEvent,
    nullptr, //KeyboardEvent,
    nullptr //OutputEvent,
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

vout_window_t *
WaylandWindowProvider::GetWindow(vlc_window_provider_t *provider,
                                 vlc_object_t *parent)
{
    printf("WINDOW PROVIDER\n");
    WaylandWindowProvider *qtprovider =
        static_cast<WaylandWindowProvider*>(provider->sys);

    qtprovider->window = static_cast<vout_window_t*>(vlc_object_create(parent, sizeof(*window)));

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

    /* We might not be able to create a subwindow for the given window type.
     * In this case, fallback to the vlc core window provider mechanism or
     * stop vout window creation. */
    if (!qtprovider->module)
    {
        return nullptr;
    }

    //vout_window_SetSize(qtprovider->window,
    //                    qtprovider->width, qtprovider->height);
end:
    return qtprovider->window;
    //emit provider->WindowRequested();
}

vlc_window_provider_t *
WaylandWindowProvider::GetProvider()
{
    return &provider;
}

void
WaylandWindowProvider::Resize(unsigned width, unsigned height)
{
    printf("PROVIDER RESIZE: %ux%u\n", width, height);
    if (window && module)
        vout_window_SetSize(window, width, height);

    this->width = width;
    this->height = height;
}
