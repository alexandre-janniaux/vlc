/**
 * @file splitter.c
 * @brief Video splitter video output module for VLC media player
 */
/*****************************************************************************
 * Copyright © 2009 Laurent Aimar
 * Copyright © 2009-2018 Rémi Denis-Courmont
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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>


#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_video_splitter.h>
#include <vlc_vector.h>

struct vlc_vidsplit_part {
    vout_window_t *window;
    vout_display_t *display;
    vlc_sem_t lock;
    unsigned width;
    unsigned height;
};

struct vout_display_sys_t {
    video_splitter_t splitter;
    vlc_mutex_t lock;

    picture_t **pictures;
    struct vlc_vidsplit_part *parts;

    int sphere_fd;
};

static void vlc_vidsplit_Prepare(vout_display_t *vd, picture_t *pic,
                                 subpicture_t *subpic, vlc_tick_t date)
{
    vout_display_sys_t *sys = vd->sys;

    picture_Hold(pic);
    (void) subpic;

    vlc_mutex_lock(&sys->lock);
    if (video_splitter_Filter(&sys->splitter, sys->pictures, pic)) {
        vlc_mutex_unlock(&sys->lock);

        for (int i = 0; i < sys->splitter.i_output; i++)
            sys->pictures[i] = NULL;
        return;
    }
    vlc_mutex_unlock(&sys->lock);

    for (int i = 0; i < sys->splitter.i_output; i++) {
        struct vlc_vidsplit_part *part = &sys->parts[i];

        vlc_sem_wait(&part->lock);
        sys->pictures[i] = vout_display_Prepare(part->display,
                                                sys->pictures[i], NULL, date);
    }
}

static void vlc_vidsplit_Display(vout_display_t *vd, picture_t *picture)
{
    vout_display_sys_t *sys = vd->sys;

    char buffer[512];
    char *cursor = buffer;

    lseek(sys->sphere_fd, 0, SEEK_SET);
    ssize_t size = read(sys->sphere_fd, buffer, sizeof(buffer));

    msg_Info(vd, "read from spheres: %d \n%.*s", (int)size, (int)size, buffer);

    int num_sphere;
    if (sscanf(cursor, "%d", &num_sphere) == 1 && num_sphere > 0)
    {
        cursor = strchr(cursor, '\n') + 1; // TODO: boundary check
        vlc_mutex_lock(&sys->lock);

        struct VLC_VECTOR(struct viewpoint_view) yaw_offsets;
        vlc_vector_init(&yaw_offsets);

        struct viewpoint_view { float fov, yaw, pitch; };

        float g_yaw = 0.f;
        float g_pitch = 0.f;
        for (int i_sphere=0; i_sphere<num_sphere; i_sphere++)
        {
            int fov, yaw, pitch;
            msg_Info(vd, "cursor: %s | start with %c", cursor, *cursor);
            if (sscanf(cursor, "%d %d %d", &fov, &yaw, &pitch) != 3)
                break;
            cursor = strchr(cursor, '\n')+1; // TODO: boundary check
            g_yaw += yaw;
            g_pitch += pitch;
            struct viewpoint_view view = { .fov=fov, .yaw=yaw, .pitch=pitch };
            vlc_vector_push(&yaw_offsets, view);
        }

        g_yaw   /= num_sphere;
        g_pitch /= num_sphere;

        for(int i_sphere=0; i_sphere<num_sphere; i_sphere++)
        {
            // TODO: set viewpoint
            if( i_sphere < sys->splitter.i_output )
            {
                struct viewpoint_view* view = &yaw_offsets.data[i_sphere];
                vout_display_cfg_t display_cfg = {
                    .viewpoint = {
                        .yaw_offset = (view->yaw - g_yaw)/ 1000.f,
                        .yaw = g_yaw / 1000.f,
                        .pitch = g_pitch / 1000.f,
                        .roll = 0.f,
                        .fov = view->fov / 1000.f
                    }
                };
                vout_display_Control(sys->parts[i_sphere].display,
                                     VOUT_DISPLAY_CHANGE_VIEWPOINT,
                                     &display_cfg);
            }
        }
        vlc_mutex_unlock(&sys->lock);
    }
    else
    {
        msg_Warn(vd, "not updating viewpoint, spheres: %d", num_sphere);
    }

    for (int i = 0; i < sys->splitter.i_output; i++) {
        struct vlc_vidsplit_part *part = &sys->parts[i];

        if (sys->pictures[i] != NULL)
            vout_display_Display(part->display, sys->pictures[i]);
        vlc_sem_post(&part->lock);
    }

    (void) picture;
}

static int vlc_vidsplit_Control(vout_display_t *vd, int query, va_list args)
{
    vout_display_sys_t *sys = vd->sys;
    const vout_display_cfg_t *cfg = va_arg(args, const vout_display_cfg_t *);

    vout_display_cfg_t display_cfg = *cfg;

    for (int i = 0; i < sys->splitter.i_output; i++) {
        struct vlc_vidsplit_part *part = &sys->parts[i];

        if (query == VOUT_DISPLAY_CHANGE_VIEWPOINT)
        {
            if (i == 0)
                display_cfg.viewpoint.yaw_offset = -cfg->viewpoint.fov / 2;
            else
                display_cfg.viewpoint.yaw_offset = cfg->viewpoint.fov / 2;
        }

        vout_display_Control(part->display, query, &display_cfg);
    }

    return VLC_SUCCESS;
}

static void vlc_vidsplit_display_Event(vout_display_t *p, int id, va_list ap)
{
    (void) p; (void) id; (void) ap;
}

static void vlc_vidsplit_Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    int n = sys->splitter.i_output;

    for (int i = 0; i < n; i++) {
        struct vlc_vidsplit_part *part = &sys->parts[i];
        vout_display_t *display;

        vlc_sem_wait(&part->lock);
        display = part->display;
        part->display = NULL;
        vlc_sem_post(&part->lock);

        if (display != NULL)
            vout_display_Delete(display);

        vout_window_Disable(part->window);
        vout_window_Delete(part->window);
        vlc_sem_destroy(&part->lock);
    }

    module_unneed(&sys->splitter, sys->splitter.p_module);
    video_format_Clean(&sys->splitter.fmt);
    vlc_mutex_destroy(&sys->lock);
    vlc_object_delete(&sys->splitter);

    close( sys->sphere_fd );
}

static void vlc_vidsplit_window_Resized(vout_window_t *wnd,
                                        unsigned width, unsigned height)
{
    struct vlc_vidsplit_part *part = wnd->owner.sys;

    vlc_sem_wait(&part->lock);
    part->width = width;
    part->height = height;

    if (part->display != NULL)
        vout_display_SetSize(part->display, width, height);
    vlc_sem_post(&part->lock);
}

static void vlc_vidsplit_window_Closed(vout_window_t *wnd)
{
    struct vlc_vidsplit_part *part = wnd->owner.sys;
    vout_display_t *display;

    vlc_sem_wait(&part->lock);
    display = part->display;
    part->display = NULL;
    vlc_sem_post(&part->lock);

    if (display != NULL)
        vout_display_Delete(display);
}

static void vlc_vidsplit_window_MouseEvent(vout_window_t *wnd,
                                           const vout_window_mouse_event_t *e)
{
    struct vlc_vidsplit_part *part = wnd->owner.sys;
    vout_display_t *vd = (vout_display_t *)vlc_object_parent(wnd);
    vout_display_sys_t *sys = vd->sys;
    vout_window_mouse_event_t ev = *e;

    vlc_mutex_lock(&sys->lock);
    if (video_splitter_Mouse(&sys->splitter, part - sys->parts,
                             &ev) == VLC_SUCCESS)
        vout_window_SendMouseEvent(vd->cfg->window, &ev);
    vlc_mutex_unlock(&sys->lock);
}

static void vlc_vidsplit_window_KeyboardEvent(vout_window_t *wnd, unsigned key)
{
    vout_display_t *vd = (vout_display_t *)vlc_object_parent(wnd);
    vout_display_sys_t *sys = vd->sys;

    vlc_mutex_lock(&sys->lock);
    vout_window_ReportKeyPress(vd->cfg->window, key);
    vlc_mutex_unlock(&sys->lock);
}

static const struct vout_window_callbacks vlc_vidsplit_window_cbs = {
    .resized = vlc_vidsplit_window_Resized,
    .closed = vlc_vidsplit_window_Closed,
    .mouse_event = vlc_vidsplit_window_MouseEvent,
    .keyboard_event = vlc_vidsplit_window_KeyboardEvent,
};

static vout_window_t *video_splitter_CreateWindow(vlc_object_t *obj,
    const vout_display_cfg_t *restrict vdcfg,
    const video_format_t *restrict source, void *sys)
{
    vout_window_cfg_t cfg = {
        .is_decorated = true,
    };
    vout_window_owner_t owner = {
        .cbs = &vlc_vidsplit_window_cbs,
        .sys = sys,
    };

    vout_display_GetDefaultDisplaySize(&cfg.width, &cfg.height, source,
                                       vdcfg);

    vout_window_t *window = vout_window_New(obj, NULL, &owner);
    if (window != NULL) {
        if (vout_window_Enable(window, &cfg)) {
            vout_window_Delete(window);
            window = NULL;
        }
    }
    return window;
}

static vout_display_t *vlc_vidsplit_CreateDisplay(vlc_object_t *obj,
    const video_format_t *restrict source,
    const vout_display_cfg_t *restrict cfg,
    const char *name)
{
    vout_display_owner_t owner = {
        .event = vlc_vidsplit_display_Event,
    };
    return vout_display_New(obj, source, cfg, name, &owner);
}

static int vlc_vidsplit_Open(vout_display_t *vd,
                             const vout_display_cfg_t *cfg,
                             video_format_t *fmtp, vlc_video_context *ctx)
{
    vlc_object_t *obj = VLC_OBJECT(vd);

    if (vout_display_cfg_IsWindowed(cfg))
        return VLC_EGENERIC;

    char *name = var_InheritString(obj, "video-splitter");
    if (name == NULL)
        return VLC_EGENERIC;

    vout_display_sys_t *sys = vlc_object_create(obj, sizeof (*sys));
    if (unlikely(sys == NULL)) {
        free(name);
        return VLC_ENOMEM;
    }
    vd->sys = sys;

    char *psz_sphere_device = var_InheritString(obj, "idviu-sphere-device");
    if (!psz_sphere_device)
    {
        msg_Err(obj, "cannot find any idviu device, see --idviu-sphere-device");
        free(sys);
        free(name);
        return VLC_EGENERIC;
    }
    sys->sphere_fd = open(psz_sphere_device, O_RDONLY);
    if (sys->sphere_fd < 0)
    {
        msg_Err(obj, "cannot open device %s", psz_sphere_device);
        free(psz_sphere_device);
        free(sys);
        return VLC_EGENERIC;
    }
    msg_Info(obj, "using device %s for sphere viewpoint", psz_sphere_device);
    free(psz_sphere_device);

    video_splitter_t *splitter = &sys->splitter;

    vlc_mutex_init(&sys->lock);
    video_format_Copy(&splitter->fmt, &vd->source);

    splitter->p_module = module_need(splitter, "video splitter", name, true);
    free(name);
    if (splitter->p_module == NULL) {
        video_format_Clean(&splitter->fmt);
        vlc_mutex_destroy(&sys->lock);
        vlc_object_delete(splitter);
        return VLC_EGENERIC;
    }

    sys->pictures = vlc_obj_malloc(obj, splitter->i_output
                                        * sizeof (*sys->pictures));
    sys->parts = vlc_obj_malloc(obj,
                                splitter->i_output * sizeof (*sys->parts));
    if (unlikely(sys->pictures == NULL || sys->parts == NULL)) {
        splitter->i_output = 0;
        vlc_vidsplit_Close(vd);
        return VLC_ENOMEM;
    }

    for (int i = 0; i < splitter->i_output; i++) {
        const video_splitter_output_t *output = &splitter->p_output[i];
        vout_display_cfg_t vdcfg = {
            .display = { 0, 0, { 1, 1 } },
            .align = { 0, 0 } /* TODO */,
            .is_display_filled = true,
            .zoom = { 1, 1 },
            .viewpoint = {0, 0, 0, FIELD_OF_VIEW_DEGREES_DEFAULT,
                          i == 0 ?
                            -FIELD_OF_VIEW_DEGREES_DEFAULT / 2
                            : FIELD_OF_VIEW_DEGREES_DEFAULT / 2}
        };
        const char *modname = output->psz_module;
        struct vlc_vidsplit_part *part = &sys->parts[i];
        vout_display_t *display;

        vlc_sem_init(&part->lock, 1);
        part->display = NULL;
        part->width = 1;
        part->height = 1;

        part->window = video_splitter_CreateWindow(obj, &vdcfg, &output->fmt,
                                                   part);
        if (part->window == NULL) {
            splitter->i_output = i;
            vlc_vidsplit_Close(vd);
            return VLC_EGENERIC;
        }

        vdcfg.window = part->window;
        display = vlc_vidsplit_CreateDisplay(obj, &output->fmt, &vdcfg,
                                             modname);
        if (display == NULL) {
            vout_window_Disable(vdcfg.window);
            vout_window_Delete(vdcfg.window);
            vlc_sem_destroy(&part->lock);
            splitter->i_output = i;
            vlc_vidsplit_Close(vd);
            return VLC_EGENERIC;
        }

        vlc_sem_wait(&part->lock);
        part->display = display;
        vout_display_SetSize(display, part->width, part->height);
        vlc_sem_post(&part->lock);
    }

    vd->prepare = vlc_vidsplit_Prepare;
    vd->display = vlc_vidsplit_Display;
    vd->control = vlc_vidsplit_Control;
    (void) cfg; (void) fmtp; (void) ctx;
    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname(N_("Splitter"))
    set_description(N_("Video splitter display plugin"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_capability("vout display", 0)
    set_callbacks(vlc_vidsplit_Open, vlc_vidsplit_Close)
    add_module("video-splitter", "video splitter", NULL,
               N_("Video splitter module"), N_("Video splitter module"))
    add_loadfile( "idviu-sphere-device", "/proc/idviu/spheres",
                  "Proc device for sphere viewpoints",
                  "Proc device for sphere viewpoints" );
vlc_module_end()
