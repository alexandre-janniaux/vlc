/*****************************************************************************
 * hmd.c: head mounted headset API implementation
 *****************************************************************************
 * Copyright (C) 2019 VLC authors, VideoLabs and VideoLAN
 *
 * Authors: Alexandre Janniaux <ajanni@videolabs.io>
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
# include "config.h"
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_hmd.h>
#include <vlc_threads.h>
#include <vlc_viewpoint.h>
#include <vlc_modules.h>

#include "../libvlc.h"

struct vlc_hmd_device_owner
{
    vlc_hmd_device_t device;

    vlc_hmd_driver_t *driver;

    vlc_mutex_t lock;
    bool dead;
};

struct vlc_hmd_interface_t
{
    /* callbacks */
    const vlc_hmd_interface_cbs_t* cbs;

    /* userdata stored for callbacks */
    void *userdata;

    /* device on which the interface is mapped */
    vlc_hmd_device_t *device;

    struct
    {
        enum vlc_hmd_state_e state;
        vlc_viewpoint_t viewpoint;
        int screen_num;
        vlc_hmd_cfg_t cfg;
        bool dead;
    } current_state;
};


int vlc_hmd_ReadEvents(struct vlc_hmd_interface_t *hmd)
{
    /* If we already notified that the device was disconnected, we won't do
     * anything more */
    assert (!hmd->current_state.dead);
    if (hmd->current_state.dead)
        return 0;

    struct vlc_hmd_device_owner *device_owner =
        container_of(hmd->device, struct vlc_hmd_device_owner, device);

    /* Lock so that we can copy the current state
     * TODO: move to a sampled push + copy model? */
    vlc_mutex_lock(&device_owner->lock);

    /* If the device has been disconnected, we notify one time that it's dead
     * and ignore any other change */
    if (device_owner->dead)
    {
        vlc_mutex_unlock(&device_owner->lock);
        if (!hmd->current_state.dead && hmd->cbs->state_changed)
            hmd->cbs->state_changed(hmd, VLC_HMD_STATE_DISCONNECTED,
                                    hmd->userdata);
        hmd->current_state.dead = true;
        return 0;
    }

    /* Otherwise, we can get the different values */

    assert(device_owner->driver);

    // TODO: should be in a static inline function
    assert(device_owner->driver->get_viewpoint);
    vlc_viewpoint_t viewpoint =
        device_owner->driver->get_viewpoint(device_owner->driver);

    assert(device_owner->driver->get_state);
    vlc_hmd_state_e state =
        device_owner->driver->get_state(device_owner->driver);

    assert(device_owner->driver->get_config);
    vlc_hmd_cfg_t cfg =
        device_owner->driver->get_config(device_owner->driver);

    vlc_mutex_unlock(&device_owner->lock);

    /* Viewpoint is handled separately and is not considered as an event, store
     * it first so that its value is available in the other callbacks */
    hmd->current_state.viewpoint = viewpoint;

    /* Compare with current state and trigger the different events */
    if (hmd->current_state.state != state)
    {
        hmd->current_state.state = state;

        // TODO: use static inline function to call state_changed
        // (hmd_signal_state_changed(hmd, state)?)
        if (hmd->cbs->state_changed)
            hmd->cbs->state_changed(hmd, state, hmd->userdata);
    }

    if (memcmp(&hmd->current_state.cfg, &cfg, sizeof(cfg)) != 0)
    {
        hmd->current_state.cfg = cfg;

        // TODO: use static inline function to call config_changed
        // (hmd_signal_config_changed(hmd, cfg)?)
        if (hmd->cbs->config_changed)
            hmd->cbs->config_changed(hmd, cfg, hmd->userdata);
    }

    return 0;
}

vlc_viewpoint_t
vlc_hmd_ReadViewpoint(struct vlc_hmd_interface_t *hmd)
{
    // we already polled the current state
    return hmd->current_state.viewpoint;
}

vlc_hmd_interface_t *
vlc_hmd_MapDevice(vlc_hmd_device_t *device,
                  const vlc_hmd_interface_cbs_t *cbs,
                  void *userdata)
{
    struct vlc_hmd_interface_t *hmd = calloc(1, sizeof(*hmd));

    vlc_object_hold(VLC_OBJECT(device));
    hmd->device = device;
    hmd->userdata = userdata;
    hmd->cbs = cbs;

    return hmd;
}

void vlc_hmd_UnmapDevice(vlc_hmd_interface_t *hmd)
{
    hmd->current_state.dead = true;
    vlc_object_release(hmd->device);

    free(hmd);
}

static int ActivateHmdDriver(void *func, va_list args)
{
    int (*activate)(vlc_hmd_driver_t *) = func;
    vlc_hmd_driver_t *driver = va_arg(args, vlc_hmd_driver_t*);

    driver->get_viewpoint = NULL;
    driver->get_state = NULL;
    driver->get_config = NULL;

    return activate(driver);
}

vlc_hmd_device_t *
vlc_hmd_FindDevice(vlc_object_t *parent,
                   const char *modules,
                   const char *name)
{
    assert(parent);
    assert(modules);

    vlc_hmd_driver_t *driver =
        vlc_custom_create(parent, sizeof(*driver), "HMD driver");

    driver->module = vlc_module_load(driver, "hmd driver", modules,
                                     false, ActivateHmdDriver, driver);

    assert(driver->get_viewpoint);
    assert(driver->get_state);
    assert(driver->get_config);

    if (!driver->module)
    {
        vlc_object_release(driver);
        return NULL;
    }

    struct vlc_hmd_device_owner *device_owner =
        vlc_custom_create(driver, sizeof(*device_owner), "HMD device");

    device_owner->driver = driver;

    return &device_owner->device;
}
