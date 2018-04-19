/*****************************************************************************
 * openhmd.c: HMD module using OpenHMD
 *****************************************************************************
 * Copyright (C) 2002-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Adrien Maglo <magsoft@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <math.h>
#include <locale.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout.h>
#include <vlc_vout_hmd.h>
#include <vlc_input.h>

#include <openhmd/openhmd.h>


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *p_this);
static void Close(vlc_object_t *);
static int headTrackingCallback(vlc_object_t *p_this, char const *psz_var,
                                vlc_value_t oldval, vlc_value_t newval,
                                void *p_data);

#define HEAD_TRACKING_TEXT N_("No head tracking")
#define HEAD_TRACKING_LONGTEXT N_("Disable the HMD head tracking")

#define PREFIX "openhmd-"

vlc_module_begin()
    set_shortname(N_("OpenHMD"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_description(N_("OpenHMD head mounted display handler"))
    set_capability("vout hmd", 10)

    add_bool(PREFIX "no-head-tracking", false,
             HEAD_TRACKING_TEXT, HEAD_TRACKING_LONGTEXT, false)

    add_shortcut("openhmd")
    set_callbacks(Open, Close)
vlc_module_end()


struct vout_hmd_sys_t
{
    vout_thread_t *p_vout;
    vout_display_t *p_display;

    vlc_thread_t thread;
    vlc_mutex_t lock;
    vlc_cond_t thread_cond;
    bool b_thread_running;
    bool b_init_completed;
    bool b_init_successfull;
    bool b_headTracking;

    ohmd_context* ctx;
    ohmd_device* hmd;

    vlc_mutex_t vp_lock;
    vlc_viewpoint_t vp;
};


static void Release(vout_hmd_t *p_hmd);
static void *HMDThread(void *p_data);


static int Open(vlc_object_t *p_this)
{
    vout_hmd_t *p_hmd = (vout_hmd_t *)p_this;

    p_hmd->p_sys = (vout_hmd_sys_t*)vlc_obj_calloc(p_this, 1, sizeof(vout_hmd_sys_t));
    if (unlikely(p_hmd->p_sys == NULL))
        return VLC_ENOMEM;

    vout_hmd_sys_t *p_sys = p_hmd->p_sys;

    p_sys->p_vout = p_hmd->p_vout;
    p_sys->p_display = p_hmd->p_display;

    vlc_mutex_init(&p_sys->lock);
    vlc_cond_init(&p_sys->thread_cond);

    vlc_mutex_init(&p_sys->vp_lock);

    p_sys->b_thread_running = true;
    p_sys->b_init_completed = false;

    if (vlc_clone(&p_sys->thread, HMDThread, p_hmd, 0) != VLC_SUCCESS)
    {
        Release(p_hmd);
        return VLC_EGENERIC;
    }

    p_sys->b_headTracking = !var_CreateGetBool(p_hmd, PREFIX "no-head-tracking");

    var_AddCallback(p_hmd, PREFIX "no-head-tracking",
                    headTrackingCallback, p_sys);

    // Wait for the OpenHMD initialization in its thread to complete.
    // Return an error and release resources if needed.
    int i_ret;

    while (true)
    {
        vlc_mutex_lock(&p_sys->lock);
        vlc_cond_wait(&p_sys->thread_cond, &p_sys->lock);

        if (p_sys->b_init_completed)
        {
            if (p_sys->b_init_successfull)
            {
                i_ret = VLC_SUCCESS;
                vlc_mutex_unlock(&p_sys->lock);
            }
            else
            {
                i_ret = VLC_EGENERIC;
                vlc_mutex_unlock(&p_sys->lock);
                vlc_join(p_sys->thread, NULL);
                Release(p_hmd);
            }
            break;
        }

        vlc_mutex_unlock(&p_sys->lock);
    }

    return i_ret;
}


static void Close(vlc_object_t *p_this)
{
    vout_hmd_t* p_hmd = (vout_hmd_t*)p_this;
    vout_hmd_sys_t* p_sys = p_hmd->p_sys;

    vlc_mutex_lock(&p_sys->lock);
    p_sys->b_thread_running = false;
    vlc_cond_signal(&p_sys->thread_cond);
    vlc_mutex_unlock(&p_sys->lock);
    vlc_join(p_sys->thread, NULL);

    Release(p_hmd);
}


static void Release(vout_hmd_t* p_hmd)
{
    vout_hmd_sys_t* p_sys = p_hmd->p_sys;

    var_DelCallback(p_hmd, PREFIX "no-head-tracking", headTrackingCallback, p_sys);
    var_Destroy(p_hmd, PREFIX "no-head-tracking");

    vlc_mutex_destroy(&p_sys->lock);
    vlc_cond_destroy(&p_sys->thread_cond);
}


/* Quaternion to Euler conversion.
 * Original code from:
 * http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToEuler/ */
static void quaternionToEuler(float *q, vlc_viewpoint_t *vp)
{
    float sqx = q[0] * q[0];
    float sqy = q[1] * q[1];
    float sqz = q[2] * q[2];
    float sqw = q[3] * q[3];

    float unit = sqx + sqy + sqz + sqw; // if normalised is one, otherwise is correction factor
    float test = q[0] * q[1] + q[2] * q[3];

    if (test > 0.499 * unit)
    {
        // singularity at north pole
        vp->yaw = 2 * atan2(q[0], q[3]);
        vp->roll = M_PI / 2;
        vp->pitch = 0;
    }
    else if (test < -0.499 * unit)
    {
        // singularity at south pole
        vp->yaw = -2 * atan2(q[0], q[3]);
        vp->roll = -M_PI / 2;
        vp->pitch = 0;
    }
    else
    {
        vp->yaw = atan2(2 * q[1] * q[3] - 2 * q[0] * q[2], sqx - sqy - sqz + sqw);
        vp->roll = asin(2 * test / unit);
        vp->pitch = atan2(2 * q[0] * q[3] - 2 * q[1] * q[2], -sqx + sqy - sqz + sqw);
    }

    vp->yaw = -vp->yaw * 180 / M_PI;
    vp->pitch = -vp->pitch * 180 / M_PI;
    vp->roll = vp->roll * 180 / M_PI;
    vp->fov = FIELD_OF_VIEW_DEGREES_DEFAULT;
}


vlc_viewpoint_t getViewpoint(vout_hmd_viewpoint_provider_t *p_vpProvider)
{
    vout_hmd_sys_t* p_sys = (vout_hmd_sys_t*)p_vpProvider;

    vlc_mutex_lock(&p_sys->vp_lock);
    vlc_viewpoint_t vp = p_sys->vp;
    vlc_mutex_unlock(&p_sys->vp_lock);

    return vp;
}


static void* HMDThread(void *p_data)
{
    vout_hmd_t* p_hmd = (vout_hmd_t*)p_data;
    vout_hmd_sys_t* p_sys = (vout_hmd_sys_t*)p_hmd->p_sys;
    bool b_init_successfull = true;

    p_sys->ctx = ohmd_ctx_create();

    // We should rather use the thread-safe uselocale but it is not avaible on Windows...
    setlocale(LC_ALL, "C");

    int num_devices = ohmd_ctx_probe(p_sys->ctx);
    if (num_devices < 0) {
        msg_Err(p_hmd, "Failed to probe devices: %s", ohmd_ctx_get_error(p_sys->ctx));
        ohmd_ctx_destroy(p_sys->ctx);
        b_init_successfull = false;
    }

    if (b_init_successfull)
    {
        ohmd_device_settings* settings = ohmd_device_settings_create(p_sys->ctx);

        int auto_update = 1;
        ohmd_device_settings_seti(settings, OHMD_IDS_AUTOMATIC_UPDATE, &auto_update);

        p_sys->hmd = ohmd_list_open_device_s(p_sys->ctx, 0, settings);

        ohmd_device_settings_destroy(settings);

        if (p_sys->hmd == NULL)
        {
            msg_Err(p_hmd, "Failed to open device: %s", ohmd_ctx_get_error(p_sys->ctx));
            ohmd_ctx_destroy(p_sys->ctx);
            b_init_successfull = false;
        }
    }

    if (b_init_successfull)
    {
        p_hmd->params.b_HMDEnabled = true;

        ohmd_device_geti(p_sys->hmd, OHMD_SCREEN_HORIZONTAL_RESOLUTION, &p_hmd->params.i_screen_width);
        ohmd_device_geti(p_sys->hmd, OHMD_SCREEN_VERTICAL_RESOLUTION, &p_hmd->params.i_screen_height);

        ohmd_device_getf(p_sys->hmd, OHMD_SCREEN_HORIZONTAL_SIZE, &p_hmd->params.viewportScale[0]);
        ohmd_device_getf(p_sys->hmd, OHMD_SCREEN_VERTICAL_SIZE, &p_hmd->params.viewportScale[1]);
        p_hmd->params.viewportScale[0] /= 2.0f;

        ohmd_device_getf(p_sys->hmd, OHMD_UNIVERSAL_DISTORTION_K, p_hmd->params.distorsionCoefs);
        ohmd_device_getf(p_sys->hmd, OHMD_UNIVERSAL_ABERRATION_K, p_hmd->params.aberrScale);

        float sep;
        ohmd_device_getf(p_sys->hmd, OHMD_LENS_HORIZONTAL_SEPARATION, &sep);

        p_hmd->params.leftLensCenter[0] = p_hmd->params.viewportScale[0] - sep / 2.0f;
        ohmd_device_getf(p_sys->hmd, OHMD_LENS_VERTICAL_POSITION, &p_hmd->params.leftLensCenter[1]);

        p_hmd->params.rightLensCenter[0] = sep / 2.0f;
        ohmd_device_getf(p_sys->hmd, OHMD_LENS_VERTICAL_POSITION, &p_hmd->params.rightLensCenter[1]);

        // Asume calibration was for lens view to which ever edge of screen is further away from lens center
        p_hmd->params.warpScale = (p_hmd->params.leftLensCenter[0] > p_hmd->params.rightLensCenter[0]) ?
            p_hmd->params.leftLensCenter[0] : p_hmd->params.rightLensCenter[0];
        p_hmd->params.warpAdj = 1.0f;

        ohmd_device_getf(p_sys->hmd, OHMD_LEFT_EYE_GL_PROJECTION_MATRIX, p_hmd->params.projection.left);
        ohmd_device_getf(p_sys->hmd, OHMD_RIGHT_EYE_GL_PROJECTION_MATRIX, p_hmd->params.projection.right);

        ohmd_device_getf(p_sys->hmd, OHMD_LEFT_EYE_GL_MODELVIEW_MATRIX, p_hmd->params.modelview.left);
        ohmd_device_getf(p_sys->hmd, OHMD_RIGHT_EYE_GL_MODELVIEW_MATRIX, p_hmd->params.modelview.right);

        p_hmd->params.p_vpProvider = (vout_hmd_viewpoint_provider_t *)p_sys;
        p_hmd->params.getViewpoint = getViewpoint;

        vout_hmd_SendEventConfigurationChanged(p_hmd);
    }

    vlc_mutex_lock(&p_sys->lock);
    p_sys->b_init_completed = true;
    p_sys->b_init_successfull = b_init_successfull;
    vlc_cond_signal(&p_sys->thread_cond);
    vlc_mutex_unlock(&p_sys->lock);

    // Quit the thread in the case of an initialization error.
    if (!b_init_successfull)
        return NULL;

    /* Main OpenHMD thread. */
    while (true)
    {
        ohmd_ctx_update(p_sys->ctx);

        float quat[] = {0, 0, 0, 1};
        vlc_viewpoint_t vp;
        if (p_sys->b_headTracking)
            ohmd_device_getf(p_sys->hmd, OHMD_ROTATION_QUAT, quat);

        quaternionToEuler(quat, &vp);

        vlc_mutex_lock(&p_sys->vp_lock);
        p_sys->vp = vp;
        vlc_mutex_unlock(&p_sys->vp_lock);

        vout_display_SendEventViewpointMoved(p_sys->p_display, &vp);

        vlc_mutex_lock(&p_sys->lock);
        mtime_t timeout = vlc_tick_now() + 3000;
        vlc_cond_timedwait(&p_sys->thread_cond, &p_sys->lock, timeout);

        if (!p_sys->b_thread_running)
        {
            vlc_mutex_unlock(&p_sys->lock);
            break;
        }

        vlc_mutex_unlock(&p_sys->lock);
    }

    ohmd_ctx_destroy(p_sys->ctx);

    /* Ugly hack: sleep to be sure the device is closed correctly.
     * This fix an issue with the Vive that does not switch on when it
     * has just been switched on by a previous instance of the module. */
    //vlc_msleep_i11e(VLC_TICK_FROM_MS(500000));

    return NULL;
}


static int headTrackingCallback(vlc_object_t *p_this, char const *psz_var,
                                vlc_value_t oldval, vlc_value_t newval,
                                void *p_data)
{
    VLC_UNUSED(p_this);
    VLC_UNUSED(psz_var);
    VLC_UNUSED(oldval);
    vout_hmd_sys_t *p_sys = (vout_hmd_sys_t *)p_data;

    //vlc_mutex_lock( &p_sys->lock );
    p_sys->b_headTracking = !newval.b_bool;
    //vlc_mutex_unlock( &p_sys->lock );

    return VLC_SUCCESS;
}
