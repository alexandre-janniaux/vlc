/*****************************************************************************
 * idviu.c: HMD module using idviu /proc interface
 *****************************************************************************
 * Copyright (C) 2019 VideoLabs
 *
 * Authors: Alexandre Janniaux
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

#include <vlc_common.h>
#include <vlc_hmd.h>
#include <vlc_viewpoint.h>
#include <vlc_tick.h>

#define HMD_DEVICE_TEXT "HMD device providing rotation information"
#define HMD_DEVICE_LONGTEXT "The HMD device file which expose yaw pitch roll information"

static int Open(vlc_object_t *hmd);
static void Close(vlc_object_t *hmd);

static vlc_viewpoint_t GetViewpoint(vlc_hmd_driver_t *driver);
static vlc_hmd_state_e GetState(vlc_hmd_driver_t *driver);
static vlc_hmd_cfg_t GetConfig(vlc_hmd_driver_t *driver);

vlc_module_begin()
    set_shortname("IDVIU HMD")
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_description("IDVIU head mounted display driver")
    set_capability("hmd driver", 20)

    /* driver device which expose %d%d%d as a rotation */
    add_loadfile("idviu-device", "/proc/VRRotation", HMD_DEVICE_TEXT, HMD_DEVICE_LONGTEXT)

    add_shortcut("idviu")
    set_callbacks(Open, Close)
vlc_module_end()

struct vlc_hmd_driver_sys_t
{
    int device_fd;
};

static int Open(vlc_hmd_driver_t *hmd)
{
    int ret = VLC_SUCCESS;
    char *device_path = NULL;
    vlc_hmd_driver_sys_t *sys = vlc_object_malloc(hmd, sizeof(*sys));

    if (!sys)
    {
        ret = VLC_ENOMEM;
        goto error;
    }

    hmd->sys = sys;

    char *device_path = var_InheritString(hmd, "idviu-dev");
    if (device == NULL)
    {
        ret = VLC_ENOMEM;
        goto error;
    }

    sys->device_fd = vlc_open(device_path, "r");
    if (sys->device_fd < 0)
    {
        msg_Err(hmd, "cannot open device file %s", device_path);
        ret = VLC_EGENERIC;
        goto error;
    }
    msg_Dbg(device_path, "successfully opened device %s", device_path);
    free(device_path);

    hmd->get_config    = GetConfig;
    hmd->get_state     = GetState;
    hmd->get_viewpoint = GetViewpoint;

    return VLC_SUCCESS;

error:
    assert(ret != VLC_SUCCESS);

    if (device_path)
        free(device_path);

    if (sys)
        vlc_obj_free(hmd, sys);

    return ret;
}

static void Close(vlc_hmd_driver_t *hmd)
{
    vlc_hmd_driver_sys_t *sys = hmd->sys;
    vlc_close(sys->device_fd);
}

static vlc_viewpoint_t GetViewpoint(vlc_hmd_driver_t *driver)
{
    vlc_viewpoint_t vp = {0};
    vlc_viewpoint_from_euler(&vp, 0,0,0);
    return vp;
}

static vlc_hmd_state_e GetState(vlc_hmd_driver_t *driver)
{
    return VLC_HMD_STATE_ENABLED;
}

static vlc_hmd_cfg_t GetConfig(vlc_hmd_driver_t *driver)
{
    return (vlc_hmd_cfg_t) {
        .i_screen_width = 1920,
        .i_screen_height = 1080,
        .viewport_scale[0] = 0.1f,
        .viewport_scale[1] = 0.1f,
        .distorsion_coefs = {{1.f, 1.f, 1.f}},
        .aberr_scale = {{1.f, 1.f, 1.f}},
        .left.lens_center = {{ 960, 540 }},
        .right.lens_center = {{ 960, 540 }},
        .warp_adj = 1.f,
    };
}
