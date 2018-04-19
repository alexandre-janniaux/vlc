#ifndef VLC_VOUT_HMD_H
#define VLC_VOUT_HMD_H

#include <vlc_common.h>
#include <vlc_vout_display.h>


/* The matrices are stored in the OpenGL format:
 * vectors are stores in lines.
 * You may have to transpose them. */

struct vout_hmd_projection_t
{
    float left[16];
    float right[16];
};

struct vout_hmd_modelview_t
{
    float left[16];
    float right[16];
};


struct vout_hmd_cfg_t
{
    bool b_HMDEnabled;

    int i_screen_width;
    int i_screen_height;

    float distorsionCoefs[4];
    float viewportScale[2];
    float aberrScale[3];

    float leftLensCenter[2];
    float rightLensCenter[2];

    float warpScale;
    float warpAdj;

    vout_hmd_projection_t projection;
    vout_hmd_modelview_t modelview;

    vout_hmd_viewpoint_provider_t *p_vpProvider;
    vlc_viewpoint_t (*getViewpoint)(vout_hmd_viewpoint_provider_t *);
};


struct vout_hmd_t {
    struct vlc_common_members obj;

    /* Module */
    module_t* p_module;

    /* Input thread */
    vout_thread_t* p_vout;
    vout_display_t* p_display;

    /* Private structure */
    vout_hmd_sys_t* p_sys;

    /* HMD parameters */
    vout_hmd_cfg_t params;

    void (*event)(vout_hmd_t *, int, va_list);
};


enum {
    VOUT_DISPLAY_HMD_UPDATE_CONFIGURATION,
};


static inline void vout_hmd_SendEvent(vout_hmd_t *hmd, int query, ...)
{
    va_list args;
    va_start(args, query);
    hmd->event(hmd, query, args);
    va_end(args);
}

static inline void vout_hmd_SendEventConfigurationChanged(vout_hmd_t *hmd)
{
    vout_hmd_SendEvent(hmd, VOUT_DISPLAY_HMD_UPDATE_CONFIGURATION);
}


#endif // VLC_VOUT_HMD_H
