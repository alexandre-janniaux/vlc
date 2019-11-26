/*****************************************************************************
 * audiomix.c: audiomix stream output module
 *****************************************************************************
 * Copyright (C) 2003-2004 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
} sout_stream_id_sys_t;

typedef struct
{
    int              i_id;
    sout_stream_id_sys_t **id;
} sout_stream_sys_t;

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys;

    p_stream->p_sys = p_sys = malloc( sizeof( sout_stream_sys_t ) );
    if( p_sys == NULL )
        return VLC_EGENERIC;

    if( !p_stream->p_next )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }
    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;

    TAB_INIT( p_sys->i_id, p_sys->id );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    int i;

    for( i = 0; i < p_sys->i_id; i++ )
    {
        sout_stream_id_sys_t *id = p_sys->id[i];

        sout_StreamIdDel( p_stream->p_next, id->id );
        es_format_Clean( &id->fmt );
        free( id );
    }
    TAB_CLEAN( p_sys->i_id, p_sys->id );

    free( p_sys );
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

    id = malloc( sizeof( sout_stream_id_sys_t ) );
    if(id == NULL)
        return NULL;
    es_format_Copy( &id->fmt, p_fmt );
    id->id               = sout_StreamIdAdd( p_stream->p_next, &id->fmt );
    if( id->id == NULL )
    {
        free( id );
        return NULL;
    }
    TAB_APPEND( p_sys->i_id, p_sys->id, id );

    return id;
}

/*****************************************************************************
 * Del:
 *****************************************************************************/
static void Del( sout_stream_t *p_stream, void *_id )
{
    VLC_UNUSED(p_stream);
    sout_stream_id_sys_t *id = (sout_stream_id_sys_t *)_id;
    id->b_used = false;
}

/*****************************************************************************
 * Send:
 *****************************************************************************/
static int Send( sout_stream_t *p_stream, void *_id, block_t *p_buffer )
{
    sout_stream_id_sys_t *id = (sout_stream_id_sys_t *)_id;
    if ( id->b_streamswap )
    {
        id->b_streamswap = false;
        p_buffer->i_flags |= BLOCK_FLAG_DISCONTINUITY;
    }
    return sout_StreamIdSend( p_stream->p_next, id->id, p_buffer );
}
