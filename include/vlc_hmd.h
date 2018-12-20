/*****************************************************************************
 * vlc_hmd.h: head mounted headset related definitions
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

#ifndef VLC_HMD_H
#define VLC_HMD_H

#include <vlc_common.h>
#include <vlc_viewpoint.h>

# ifdef __cplusplus
extern "C" {
# endif

/**
 * \defgroup hmd hmd
 * \ingroup misc
 * @{
 */

/**
 * TODO: HMD description and examples
 */

enum vlc_hmd_state_e
{
    VLC_HMD_STATE_DISCONNECTED,
    VLC_HMD_STATE_DISABLED,
    VLC_HMD_STATE_ENABLED,
};

// TODO: move to vlc_common.h
typedef enum vlc_hmd_state_e vlc_hmd_state_e;

/**
 *
 */
struct vlc_hmd_cfg_t
{
    int i_screen_width;
    int i_screen_height;

    float distorsion_coefs[4];
    float viewport_scale[2];
    float aberr_scale[3];

    struct {
        float projection[16];
        float modelview[16];
        float lens_center[2];
    } left, right;

    float warp_scale;
    float warp_adj;
};

/**
 * Holds the headset module itself
 */
struct vlc_hmd_driver_t
{
    struct vlc_common_members obj;

    /* Module */
    module_t* module;

    /* Private structure */
    void* sys;

    /* HMD parameters */
    vlc_hmd_cfg_t cfg;

    int listeners_count;
    struct vlc_hmd_interface **listeners;

    vlc_viewpoint_t (*get_viewpoint)(vlc_hmd_driver_t *driver);
    vlc_hmd_state_e (*get_state)(vlc_hmd_driver_t *driver);
    vlc_hmd_cfg_t   (*get_config)(vlc_hmd_driver_t *driver);
};

/**
 * Holds the headset device representation
 */
struct vlc_hmd_device_t
{
    struct vlc_common_members obj;
};

/**
 * These callbacks are called when the HMD has been sampled and the listener is
 * ready to process these events.
 */
struct vlc_hmd_interface_cbs_t
{
    /**
     * Called when the state of the headset is changed, for example when it is
     * disconnected or disabled by the user.
     */
    void (*state_changed)(struct vlc_hmd_interface_t *hmd,
                          enum vlc_hmd_state_e state,
                          void *userdata);

    /**
     * Called when the device's virtual screen has changed.
     *
     * TODO: screen shouldn't be represented by a screen num only, but should
     * use an opaque structure.
     */
    void (*screen_changed)(struct vlc_hmd_interface_t *hmd,
                           int screen_num,
                           void *userdata);

    /**
     * Called when an HMD parameter has changed, for example transformation
     * matrix or interpupillary distance.
     */
    void (*config_changed)(struct vlc_hmd_interface_t *hmd,
                           vlc_hmd_cfg_t cfg,
                           void *userdata);
};

/**
 * Listen to the headset state and triggers events
 */
struct vlc_hmd_interface_t;

/**
 * Triggers events from the HMD
 *
 * \param hmd the mapped hmd device interface
 * \return TODO
 */
VLC_API int
vlc_hmd_ReadEvents(vlc_hmd_interface_t *hmd);

/**
 *
 * \param hmd the mapped hmd device interface
 * \return the viewpoint coming from the last sampling step
 */
VLC_API vlc_viewpoint_t
vlc_hmd_ReadViewpoint(vlc_hmd_interface_t *hmd);

/**
 * Map an HMD device to a local interface
 *
 * Map the device to an interface so that it can be used
 * locally and receive events. The device will continue to
 * exists as long as there are interfaces connected to it.
 *
 * \param hmd the hmd device to map to an interface
 * \param cbs callbacks defining the interface
 * \param userdata userdata to be used by the callbacks
 * \return a HMD interface linked to the HMD device
 */
VLC_API vlc_hmd_interface_t *
vlc_hmd_MapDevice(vlc_hmd_device_t *hmd,
                  const vlc_hmd_interface_cbs_t *cbs,
                  void *userdata) VLC_USED;

/**
 * Unmap and invalidate an HMD interface
 *
 * \param hmd a valid mapped HMD interface
 */
VLC_API void
vlc_hmd_UnmapDevice(vlc_hmd_interface_t *hmd);

/**
 * TODO
 */
VLC_API vlc_hmd_device_t *
vlc_hmd_FindDevice(vlc_object_t *parent,
                   const char *modules,
                   const char *name) VLC_USED;

/** @} */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
