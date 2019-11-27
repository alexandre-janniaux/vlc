/*****************************************************************************
 * audiomix.c: audiomix stream output module
 *****************************************************************************
 * Copyright (C) 2019 Videolabs
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

/**
 * @file audiomix.c
 * @brief mix multiple audio input ES into the same output ES
 *
 * This plugin provides a stream output module allowing mixing to be done in
 * the stream output (sout) pipeline.
 */


/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_block.h>
#include <vlc_list.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int      Open    ( vlc_object_t * );
static void     Close   ( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("Audiomix stream output") )
    set_capability( "sout stream", 50 )
    add_shortcut( "audiomix" )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static void *Add( sout_stream_t *, const es_format_t * );
static void  Del( sout_stream_t *, void * );
static int   Send( sout_stream_t *, void *, block_t * );

typedef struct
{
    es_format_t fmt;
    void          *id;
    struct vlc_list node;
} sout_stream_id_sys_t;

typedef struct
{
    struct vlc_list es_list;
} sout_stream_sys_t;

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open(vlc_object_t *p_this)
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *sys;

    p_stream->p_sys = sys = malloc(sizeof(*sys));
    if (sys == NULL)
        return VLC_EGENERIC;

    vlc_list_init(&sys->es_list);

    if (!p_stream->p_next)
    {
        free(sys);
        return VLC_EGENERIC;
    }
    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close (vlc_object_t * p_this)
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *sys = p_stream->p_sys;

    sout_stream_id_sys_t *es_entry = NULL;
    vlc_list_foreach(es_entry, &sys->es_list, node)
    {
        if (es_entry->fmt.i_cat != AUDIO_ES)
            sout_StreamIdDel(p_stream->p_next, es_entry->id);
        es_format_Clean(&es_entry->fmt);
        vlc_list_remove(&es_entry->node);
        free(es_entry);
    }

    free(sys);
}

/*****************************************************************************
 * Add:
 *****************************************************************************/
static void *Add( sout_stream_t *p_stream, const es_format_t *p_fmt )
{
    sout_stream_sys_t    *sys = p_stream->p_sys;
    sout_stream_id_sys_t *id  = NULL;

    /* TODO: different state for audio ES and video ES */
    /* TODO: use PCM only, warn if not PCM */

    id = malloc(sizeof(sout_stream_id_sys_t));
    if (id == NULL)
        return NULL;
    es_format_Copy(&id->fmt, p_fmt);

    /* Add this stream_id only if it's not audio (bypass ES). */
    if (p_fmt->i_cat != AUDIO_ES)
    {
        id->id = sout_StreamIdAdd(p_stream->p_next, &id->fmt);
        if (id->id == NULL)
        {
            es_format_Clean(&id->fmt);
            free(id);
            return NULL;
        }
    }
    vlc_list_append(&id->node, &sys->es_list);

    return id;
}

/*****************************************************************************
 * Del:
 *****************************************************************************/
static void Del(sout_stream_t *p_stream, void *opaque_id)
{
    sout_stream_id_sys_t *id = opaque_id;

    /* We don't forward streams for input except if they are non-audio streams
     * so there is no need to delete them otherwise. */
    if (id->fmt.i_cat != AUDIO_ES)
        sout_StreamIdDel(p_stream->p_next, id->id);

    vlc_list_remove(&id->node);

    es_format_Clean(&id->fmt);
    free(id);
}

/*****************************************************************************
 * Send:
 *****************************************************************************/
static int Send(sout_stream_t *p_stream, void *opaque_id, block_t *p_buffer)
{
    sout_stream_id_sys_t *id = opaque_id;
    if (id->fmt.i_cat == AUDIO_ES)
    {
        block_Release(p_buffer);
        return VLC_SUCCESS;
    }

    return sout_StreamIdSend(p_stream->p_next, id->id, p_buffer);
}
