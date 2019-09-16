/**
 * @file subsurface_wnd.c
 * @brief
 */
/*****************************************************************************
 * Copyright © 2019 VideoLabs
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>

#include <wayland-client.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_window.h>

#include "input.h"
#include "output.h"

typedef struct
{
    struct wl_event_queue *eventq;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_subcompositor *subcompositor;

    struct
    {
        unsigned width;
        unsigned height;
        struct wl_surface *handle;
        struct wl_subsurface *role;
    } surface;

    struct
    {
    } wl;

    vlc_mutex_t lock;
    vlc_thread_t thread;
} vout_window_sys_t;


static void registry_global_cb( void *data, struct wl_registry *registry,
                                uint32_t name, const char *intf,
                                uint32_t version )
{
    vout_window_t *wnd = data;
    vout_window_sys_t *sys = wnd->sys;

    if (!strcmp(intf, "wl_compositor"))
        sys->compositor = wl_registry_bind(registry, name,
                                           &wl_compositor_interface,
                                           __MIN(2, version));

    else if (!strcmp(intf, "wl_subcompositor"))
        sys->subcompositor = wl_registry_bind(registry, name,
                                              &wl_subcompositor_interface, 1);
}

static void registry_global_remove_cb( void *data, struct wl_registry *registry, uint32_t name)
{
    VLC_UNUSED(data);
    VLC_UNUSED(registry);
    VLC_UNUSED(name);
}

const struct wl_registry_listener registry_cbs =
{
    registry_global_cb,
    registry_global_remove_cb,
};


/** Background thread for Wayland surface events handling */
static void *Thread(void *data)
{
    vout_window_t *wnd = data;
    vout_window_sys_t *sys = wnd->sys;

    VLC_UNUSED(sys);

    return NULL;
}

static void ReportSize(vout_window_t *wnd)
{
    vout_window_sys_t *sys = wnd->sys;

    unsigned width  = sys->surface.width;
    unsigned height = sys->surface.height;

    var_SetInteger(wnd, "width", width);
    var_SetInteger(wnd, "height", height);

    /* TODO: should we report zero size values ? */
    msg_Err(wnd, "WAYLAND report size %ux%u", width, height);
    vout_window_ReportSize(wnd, width, height);
}

static void Resize(vout_window_t *wnd, unsigned width, unsigned height)
{
    vout_window_sys_t *sys = wnd->sys;

    msg_Err(wnd, "Resizing vout window to %ux%u", width, height);

    vlc_mutex_lock(&sys->lock);
    sys->surface.width  = width;
    sys->surface.height = height;
    ReportSize(wnd);
    vlc_mutex_unlock(&sys->lock);
    wl_display_flush(wnd->display.wl);
}

static int Enable(vout_window_t *wnd, const vout_window_cfg_t *restrict cfg)
{
    vout_window_sys_t *sys = wnd->sys;
    struct wl_display *display = wnd->display.wl;



    wl_surface_commit(wnd->handle.wl);
    wl_display_flush(display);

    VLC_UNUSED(sys);

    return VLC_SUCCESS;
}

static void Disable(vout_window_t *wnd)
{
    struct wl_display *display = wnd->display.wl;

    wl_surface_attach(wnd->handle.wl, NULL, 0, 0);
    wl_surface_commit(wnd->handle.wl);
    wl_display_flush(display);
}

static void Close(vout_window_t *);
static const struct vout_window_operations ops = {
    .enable = Enable,
    .disable = Disable,
    .resize = Resize,
    .destroy = Close,
};

static int Open(vout_window_t *wnd, vout_window_t *parent)
{
    printf("MODULE FOUND WINDOW\n");
    if (parent == NULL)
    {
        msg_Err(wnd, "wayland subsurface windows must be embedded into "
                     "another window");
        return VLC_EGENERIC;
    }

    if (parent->type != VOUT_WINDOW_TYPE_WAYLAND)
    {
        msg_Err(wnd, "cannot embed a wayland subsurface into "
                     "a non-wayland window");
        return VLC_EGENERIC;
    }

    vout_window_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    struct wl_display *display = parent->display.wl;

    sys->compositor = NULL;
    sys->surface.handle = NULL;
    sys->surface.role = NULL;
    vlc_mutex_init(&sys->lock);
    wnd->sys = sys;
    wnd->handle.wl = NULL;

    sys->eventq = wl_display_create_queue(display);
    if (sys->eventq == NULL)
        goto error;

    struct wl_registry *registry =
        wl_display_get_registry(display);
    if (registry == NULL)
        goto error;

    wl_registry_add_listener(registry, &registry_cbs, wnd);
    wl_display_roundtrip(display);
    wl_registry_destroy(registry);

    sys->surface.handle = wl_compositor_create_surface(sys->compositor);
    if (sys->surface.handle == NULL)
        goto error;

    sys->surface.role = wl_subcompositor_get_subsurface(
                                sys->subcompositor,
                                sys->surface.handle,
                                parent->handle.wl);
    if (sys->surface.role == NULL)
        goto error;

    wl_subsurface_set_desync(sys->surface.role);
    wl_subsurface_place_below(sys->surface.role, parent->handle.wl);

    struct wl_region *region = wl_compositor_create_region(sys->compositor);
    wl_region_add(region, 0, 0, 0, 0);
    wl_surface_set_input_region(sys->surface.handle, region);
    wl_region_destroy(region);

    wnd->type = VOUT_WINDOW_TYPE_WAYLAND;
    wnd->handle.wl = sys->surface.handle;
    wnd->display.wl = display;
    wnd->ops = &ops;

    if (vlc_clone(&sys->thread, Thread, wnd, VLC_THREAD_PRIORITY_LOW))
        goto error;

    return VLC_SUCCESS;

error:
    if (wnd->handle.wl != NULL)
        wl_surface_destroy(wnd->handle.wl);
    if (sys->eventq != NULL)
        wl_event_queue_destroy(sys->eventq);
    free(sys);
    return VLC_EGENERIC;
}

static void Close(vout_window_t *wnd)
{
    vout_window_sys_t *sys = wnd->sys;

    vlc_cancel(sys->thread);
    vlc_join(sys->thread, NULL);

    vlc_mutex_destroy(&sys->lock);
    wl_surface_destroy(wnd->handle.wl);
    wl_compositor_destroy(sys->compositor);
    wl_registry_destroy(sys->registry);
    free(sys);
}

vlc_module_begin()
    set_shortname(N_("Wayland subsurface"))
    set_description(N_("XDG subsurface window"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_capability("vout subwindow", 10)
    set_callbacks(Open, Close)
    add_shortcut("wl_subsurface")
vlc_module_end()
