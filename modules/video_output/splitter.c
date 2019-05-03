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

#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_video_splitter.h>

struct vlc_vidsplit_part {
    vout_window_t *window;
    vout_display_t *display;
    picture_t *picture;
    vlc_tick_t date;
    vlc_sem_t lock;
    unsigned width;
    unsigned height;
};

struct vlc_vidsplit_thread
{
    vout_display_t *vd;
    struct vlc_vidsplit_part *part;
    vlc_thread_t thread;
};

struct vout_display_sys_t {
    video_splitter_t splitter;
    vlc_mutex_t lock;

    picture_t **pictures;
    struct vlc_vidsplit_part *parts;
    struct vlc_vidsplit_thread *threads;

    vlc_sem_t barrier_wait;
    vlc_sem_t barrier_done;
};

static void* vlc_vidsplit_ThreadDisplay(void *data)
{
    struct vlc_vidsplit_thread *vd_thread = data;
    vout_display_t *vd = vd_thread->vd;
    vout_display_sys_t *sys = vd->sys;
    struct vlc_vidsplit_part *part = vd_thread->part;

    for (;;)
    {
        sem_wait(&sys->barrier_wait);

        /* We might need to stop the prepare just after the barrier
         * and we were not supposed to take the semaphore token. */
        if (part->display == NULL)
        {
            sem_post(&sys->barrier_wait);
            break;
        }

        part->picture = vout_display_Prepare(part->display,
                                             part->picture, NULL,
                                             part->date);

        /* notify readiness */
        sem_post(&sys->barrier_done);

        sem_wait(&sys->barrier_wait);
        if (part->picture)
            vout_display_Display(part->display, part->picture);

        /* notify that image has been displayed */
        sem_post(&sys->barrier_done);
    }

    return NULL;
}

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
        part->picture = sys->pictures[i];
        part->date = date;

        vlc_sem_wait(&part->lock);


        /* From here, part is locked until display's end so we can read it */
        if (part->display == NULL)
        {
            part->picture = NULL;
            if (sys->pictures[i] != NULL)
            {
                picture_Release(sys->pictures[i]);
                sys->pictures[i] = NULL;
            }
        }
    }

    /* Start prepare on each vout  */
    for (int i = 0; i < sys->splitter.i_output; ++i)
        if (sys->parts[i].display != NULL)
            vlc_sem_post(&sys->barrier_wait);

    /* Wait for each vout to have finished */
    for (int i = 0; i < sys->splitter.i_output; ++i)
        if (sys->parts[i].display != NULL)
            vlc_sem_wait(&sys->barrier_done);
}

static void vlc_vidsplit_Display(vout_display_t *vd, picture_t *picture)
{
    vout_display_sys_t *sys = vd->sys;

    /* Request each output to display the picture. */
    for (int i = 0; i < sys->splitter.i_output; i++)
        if (sys->parts[i].display != NULL)
            vlc_sem_post(&sys->barrier_wait);

    /* Wait until every output has displayed a picture. */
    for (int i = 0; i < sys->splitter.i_output; i++)
        if (sys->parts[i].display != NULL)
            vlc_sem_wait(&sys->barrier_done);

    /* Release parts lock, we can't read sys->parts[i] after that */
    for (int i = 0; i < sys->splitter.i_output; i++)
        vlc_sem_post(&sys->parts[i].lock);

    (void) picture;
}

static int vlc_vidsplit_Control(vout_display_t *vd, int query, va_list args)
{
    (void) vd; (void) args;

    switch (query) {
        case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
        case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
        case VOUT_DISPLAY_CHANGE_ZOOM:
        case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
        case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
            return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
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
        struct vlc_vidsplit_thread *thread = &sys->threads[i];
        vout_display_t *display;

        vlc_sem_wait(&part->lock);
        display = part->display;
        part->display = NULL;
        vlc_sem_post(&part->lock);

        vlc_cancel(thread->thread);
        vlc_join(thread->thread, NULL);

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
    sys->threads = vlc_obj_malloc(obj,
                                  splitter->i_output * sizeof (*sys->threads));
    if (unlikely(sys->pictures == NULL || sys->parts == NULL)) {
        splitter->i_output = 0;
        vlc_vidsplit_Close(vd);
        return VLC_ENOMEM;
    }

    vlc_sem_init(&sys->barrier_wait, 0);
    vlc_sem_init(&sys->barrier_done, 0);

    for (int i = 0; i < splitter->i_output; i++) {
        const video_splitter_output_t *output = &splitter->p_output[i];
        vout_display_cfg_t vdcfg = {
            .display = { 0, 0, { 1, 1 } },
            .align = { 0, 0 } /* TODO */,
            .is_display_filled = true,
            .zoom = { 1, 1 },
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

        sys->threads[i].vd = vd;
        sys->threads[i].part = part;

        int ret_clone = vlc_clone(&sys->threads[i].thread,
                                  vlc_vidsplit_ThreadDisplay,
                                  &sys->threads[i],
                                  VLC_THREAD_PRIORITY_VIDEO);

        if (ret_clone != VLC_SUCCESS) {
            /* TODO: Abort and cleanup */
        }
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
vlc_module_end()
