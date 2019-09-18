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
#include <vlc_variables.h>

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
        struct wl_surface *parent;
        struct wl_surface *handle;
        struct wl_subsurface *role;
    } surface;

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

static void ReportSize(vout_window_t *wnd)
{
    vout_window_sys_t *sys = wnd->sys;

    unsigned width  = sys->surface.width;
    unsigned height = sys->surface.height;

    /* Enfore future display size. */
    var_SetInteger(wnd, "width", width);
    var_SetInteger(wnd, "height", height);

    /* TODO: should we report zero size values ? */
    vout_window_ReportSize(wnd, width, height);
}

static int Enable(vout_window_t *wnd, const vout_window_cfg_t *restrict cfg)
{
    struct wl_display *display = wnd->display.wl;

    return VLC_SUCCESS;
}

static void Disable(vout_window_t *wnd)
{
    struct wl_display *display = wnd->display.wl;

    //wl_surface_attach(wnd->handle.wl, NULL, 0, 0);
    //wl_surface_commit(wnd->handle.wl);
    //wl_display_flush(display);
}

static void Close(vout_window_t *);
static const struct vout_window_operations ops = {
    .enable = Enable,
    .disable = Disable,
    .destroy = Close,
};

static int OnVariableChanged(vlc_object_t *obj, const char *name,
                             vlc_value_t old_value, vlc_value_t new_value,
                             void *userdata)
{
    vout_window_t *wnd = userdata;
    vout_window_sys_t *sys = wnd->sys;

    if (!strcmp(name, "wl-embed-size"))
    {
        sys->surface.width = new_value.coords.x;
        sys->surface.height = new_value.coords.y;
        ReportSize(wnd);
    }
    else if (!strcmp(name, "wl-embed-surface"))
    {
        // TODO: cleanup
        vout_window_ReportClose(wnd);
    }
    else { vlc_assert_unreachable(); }
}

static int Open(vout_window_t *wnd)
{
    struct wl_display *display = var_InheritAddress(wnd, "wl-embed-display");
    struct wl_surface *surface = var_InheritAddress(wnd, "wl-embed-surface");

    if (!display || !surface)
        return VLC_EGENERIC;

    vout_window_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->surface.parent = surface;
    sys->compositor = NULL;
    sys->surface.handle = NULL;
    sys->surface.role = NULL;
    vlc_mutex_init(&sys->lock);

    /* Initialize sys here as it is used in callbacks afterwards. */
    wnd->sys = sys;

    //sys->eventq = wl_display_create_queue(display);
    //if (sys->eventq == NULL)
    //    goto error;

    struct wl_registry *registry =
        wl_display_get_registry(display);
    if (registry == NULL)
        goto error;
    //wl_proxy_set_event_queue((wl_proxy*) registry, sys->eventq);

    wl_registry_add_listener(registry, &registry_cbs, wnd);
    wl_display_roundtrip(display);
    wl_registry_destroy(registry);

    ///* Create video surface. */
    //sys->surface.handle = wl_compositor_create_surface(sys->compositor);
    //if (sys->surface.handle == NULL)
    //    goto error;

    //sys->surface.role = wl_subcompositor_get_subsurface(
    //                            sys->subcompositor,
    //                            sys->surface.handle,
    //                            sys->surface.parent);
    //if (sys->surface.role == NULL)
    //    goto error;

    //wl_subsurface_set_desync(sys->surface.role);
    //wl_subsurface_place_above(sys->surface.role, surface);

    ///* Disable events on the window surface. */
    //struct wl_region *region = wl_compositor_create_region(sys->compositor);
    //wl_region_add(region, 0, 0, 0, 0);
    //wl_surface_set_input_region(sys->surface.handle, region);
    //wl_region_destroy(region);

    vlc_value_t var_embed_size;
    var_Inherit(wnd, "wl-embed-size", VLC_VAR_COORDS, &var_embed_size);
    sys->surface.width  = var_embed_size.x;
    sys->surface.height = var_embed_size.y;
    ReportSize(wnd);

    /*
     * The display modules using this window will use the surface above. */
    wnd->type = VOUT_WINDOW_TYPE_WAYLAND;
    wnd->handle.wl = surface;//sys->surface.handle;
    wnd->display.wl = display;
    wnd->ops = &ops;

    vlc_object_t *parent = vlc_object_parent(wnd);
    var_AddCallback(parent, "wl-embed-surface", OnVariableChanged, wnd);
    var_AddCallback(parent, "wl-embed-size", OnVariableChanged, wnd);

    return VLC_SUCCESS;

error:
    vlc_mutex_destroy(&sys->lock);

    //if (sys->surface.role != NULL)
    //    wl_subsurface_destroy(sys->subsurface.role);
    //if (sys->surface.handle != NULL)
    //    wl_surface_destroy(sys->surface.handle);
    //if (sys->eventq != NULL)
    //    wl_event_queue_destroy(sys->eventq);

    free(sys);
    return VLC_EGENERIC;
}

static void Close(vout_window_t *wnd)
{
    vout_window_sys_t *sys = wnd->sys;

    //vlc_cancel(sys->thread);
    //vlc_join(sys->thread, NULL);

    vlc_mutex_destroy(&sys->lock);
    //wl_surface_destroy(wnd->handle.wl);
    //wl_compositor_destroy(sys->compositor);
    //wl_registry_destroy(sys->registry);
    free(sys);
}

vlc_module_begin()
    set_shortname(N_("Wayland embed surface"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_capability("vout window", 0)
    set_callbacks(Open, Close)
    add_shortcut("embed-wl")
vlc_module_end()
