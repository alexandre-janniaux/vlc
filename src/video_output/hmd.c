
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc_vout.h>
#include <vlc_vout_hmd.h>
#include <vlc_input.h>

#include "../libvlc.h"
#include "vout_internal.h"
#include "event.h"
#include "../input/event.h"
#include "hmd.h"


int vout_openHMD(vout_thread_t *p_vout)
{
    vout_hmd_t* p_hmd = vlc_custom_create(p_vout, sizeof(*p_hmd), "HMD");
    if (unlikely(p_hmd == NULL))
    {
        p_vout->p->hmd.hmd = NULL;
        return VLC_ENOMEM;
    }

    p_vout->p->hmd.hmd = p_hmd;

    p_hmd->p_vout = p_vout;
    p_hmd->p_display = p_vout->p->display.vd;
    p_hmd->event = HMDEvent;

    // Ugly but this may take too much time...
    // This is to avoid filling the pools with late frames.
    if (p_vout->p->input != NULL)
        var_SetInteger(p_vout->p->input, "state", PAUSE_S);

    p_hmd->p_module = module_need(p_hmd, "vout hmd", NULL, false);
    if (unlikely(p_hmd->p_module == NULL))
    {
        vlc_object_release(p_hmd);
        p_vout->p->hmd.hmd = NULL;
        return VLC_EGENERIC;
    }

    var_SetBool(p_vout, "viewpoint-changeable", true);
    /* Ugly hack to update the hotkeys module. */
    if (p_vout->p->input != NULL)
        input_SendEventVout((input_thread_t *)p_vout->p->input);

    // Ugly!
    if (p_vout->p->input != NULL)
        var_SetInteger(p_vout->p->input, "state", PLAYING_S);

    return VLC_SUCCESS;
}


int vout_stopHMD(vout_thread_t *p_vout)
{
    vout_hmd_t* p_hmd = p_vout->p->hmd.hmd;

    // Ugly but this may take too much time...
    // This is to avoid filling the pools with late frames.
    if (p_vout->p->input != NULL)
        var_SetInteger(p_vout->p->input, "state", PAUSE_S);

    p_hmd->params.b_HMDEnabled = false;
    vout_ControlChangeHMDConfiguration(p_vout, &p_hmd->params);

    module_unneed(p_hmd, p_hmd->p_module);

    vlc_object_release(p_hmd);

    // Ugly!
    if (p_vout->p->input != NULL)
        var_SetInteger(p_vout->p->input, "state", PLAYING_S);

    p_vout->p->hmd.hmd = NULL;
    return VLC_SUCCESS;
}


void HMDEvent(vout_hmd_t *p_hmd, int query, va_list args)
{
    VLC_UNUSED(args);

    switch (query)
    {
        case VOUT_DISPLAY_HMD_UPDATE_CONFIGURATION:
            vout_ControlChangeHMDConfiguration(p_hmd->p_vout, &p_hmd->params);
            break;
        default:
            msg_Err(p_hmd, "Unknown event: %d", query);
    }
}
