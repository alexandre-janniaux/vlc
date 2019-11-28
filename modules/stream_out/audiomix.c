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
#include <assert.h>

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

    block_t *block_list;
    block_t **block_list_head;

    struct vlc_list node;
} sout_stream_id_sys_t;

typedef struct
{
    struct vlc_list es_list;
    struct vlc_list audio_list;

    void *out_stream;
    es_format_t out_fmt;
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
    vlc_list_init(&sys->audio_list);
    sys->out_stream = NULL;
    es_format_Init(&sys->out_fmt, AUDIO_ES, VLC_CODEC_S16L);

    sys->out_fmt.audio.i_rate = 44100;

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
        sout_StreamIdDel(p_stream->p_next, es_entry->id);
        es_format_Clean(&es_entry->fmt);
        vlc_list_remove(&es_entry->node);
        free(es_entry);
    }

    vlc_list_foreach(es_entry, &sys->audio_list, node)
    {
        /* TODO: should never happen */
        if (es_entry->block_list)
            block_ChainRelease(es_entry->block_list);
        es_format_Clean(&es_entry->fmt);
        vlc_list_remove(&es_entry->node);
        free(es_entry);
    }

    es_format_Clean(&sys->out_fmt);

    if (sys->out_stream)
        sout_StreamIdDel(p_stream->p_next, sys->out_stream);

    free(sys);
}

static void ComputeOutputBlock(block_t *output, block_t *input,
                               size_t out_offset, size_t length,
                               unsigned pitch_data)
{
    for (size_t i=0; i<length; i+=pitch_data)
    {
        unsigned int current = 0;
        unsigned int value = 0;

        for (unsigned j=0; j<pitch_data; ++j)
        {
            value   = value   << 4 | input->p_buffer[i+j];
            current = current << 4 | output->p_buffer[i+j+out_offset];
        }
        current += value / 2; // HACK

        for (unsigned j=pitch_data; j>0; --j)
        {
            output->p_buffer[i+j+out_offset] = (value >> (j-1)) & 0xFF;
        }
    }
}

static size_t ProcessData(sout_stream_t *stream)
{
    sout_stream_sys_t *sys = stream->p_sys;

    // TODO: handle PTS, rate
    const unsigned samplerate = 44100;
    const unsigned pitch_data = 2;

    /* Compute the maximum amount of data that will be processed. */
    size_t max_data = SIZE_MAX;
    sout_stream_id_sys_t *es_stream = NULL;
    vlc_list_foreach(es_stream, &sys->audio_list, node)
    {
        /* If one stream is not ready, we cannot process data and send it
         * to next stream, wait for more data. */
        if (es_stream->block_list == NULL)
        {
            msg_Info(stream, "Not enough data to process, waiting for more");
            return 0;
        }

        size_t available_data = es_stream->block_list->i_buffer;

        /* We need to compress data with next block if available, so that we
         * don't need to copy buffer even when available_data < pitch_data. */
        if (es_stream->block_list->p_next != NULL)
            available_data += es_stream->block_list->p_next->i_buffer;

        msg_Dbg(stream, "ES %p available data: %zu", es_stream, available_data);

        max_data = __MIN(max_data, es_stream->block_list->i_buffer) & ~((pitch_data<<1)-1);
    }

    if (max_data < pitch_data)
        return 0;

    /* Allocate the work memory for the output. */
    block_t *buffer = block_Alloc(max_data);
    if (!buffer)
    {
        msg_Err(stream, "cannot allocate data");
        return 0;
    }
    memset(buffer->p_buffer, 0, buffer->i_buffer);

    /* Do the actual processing */
    vlc_list_foreach(es_stream, &sys->audio_list, node)
    {
        block_t *first_input = es_stream->block_list;
        size_t available_data = __MIN(max_data, first_input->i_buffer);
        ComputeOutputBlock(buffer, first_input, 0, available_data, pitch_data);

        if (first_input->p_next != NULL && first_input->i_buffer < max_data)
        {
            ComputeOutputBlock(buffer, first_input->p_next,
                               first_input->i_buffer,
                               max_data - first_input->i_buffer,
                               pitch_data);
            first_input->p_next->p_buffer += max_data - available_data;
            first_input->p_next->i_buffer -= max_data - available_data;
        }

        first_input->i_buffer -= available_data;
        first_input->p_buffer += available_data;

        /* If we exhausted a block, release it. */
        while (es_stream->block_list)
        {
            if (es_stream->block_list->i_buffer == 0)
            {
                msg_Info(stream, "Release block %p", es_stream->block_list);
                block_t *last_block = es_stream->block_list;
                /* We reached the last block, reset the block list state. */
                if (last_block->p_next == NULL)
                    es_stream->block_list_head = &es_stream->block_list;

                es_stream->block_list = es_stream->block_list->p_next;

                block_Release(last_block);
            }
            else break;
        }
    }

    /* Setup metadata on the block and compute next ones. */
    //block->i_pts = block->i_dts = date_Get(&sys->date);
    //date_Increment(&sys->date, max_data / pitch_data);

    /* The block is ready to be sent to next stream. */
    if (sys->out_stream) // TODO: should always be available
        sout_StreamIdSend(stream, sys->out_stream, buffer);
    else
        block_Release(buffer);

    return max_data;
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
        vlc_list_append(&id->node, &sys->es_list);
    }
    else
    {
        id->block_list = NULL;
        id->block_list_head = &id->block_list;
        vlc_list_append(&id->node, &sys->audio_list);
    }

    if (sys->out_stream == NULL)
    {
        sys->out_stream = sout_StreamIdAdd(p_stream->p_next, &sys->out_fmt);
        if (sys->out_stream == NULL)
        {
            /* what to do ? */
            abort();
        }
    }


    return id;
}

/*****************************************************************************
 * Del:
 *****************************************************************************/
static void Del(sout_stream_t *p_stream, void *opaque_id)
{
    sout_stream_sys_t *sys = p_stream->p_sys;
    sout_stream_id_sys_t *id = opaque_id;

    /* We don't forward streams for input except if they are non-audio streams
     * so there is no need to delete them otherwise. */
    if (id->fmt.i_cat != AUDIO_ES)
        sout_StreamIdDel(p_stream->p_next, id->id);

    vlc_list_remove(&id->node);
    es_format_Clean(&id->fmt);

    if (vlc_list_is_empty(&sys->audio_list)
        || vlc_list_is_empty(&sys->es_list))
    {
        if (sys->out_stream)
            sout_StreamIdDel(p_stream->p_next, sys->out_stream);
        sys->out_stream = NULL;
    }

    free(id);
}

/*****************************************************************************
 * Send:
 *****************************************************************************/
static int Send(sout_stream_t *p_stream, void *opaque_id, block_t *p_buffer)
{
    sout_stream_sys_t *sys = p_stream->p_sys;
    sout_stream_id_sys_t *id = opaque_id;
    if (id->fmt.i_cat == AUDIO_ES)
    {
        msg_Info(p_stream, "Received audio pts from stream %p: %" PRId64,
                 opaque_id, p_buffer->i_pts);

        block_ChainLastAppend(&id->block_list_head, p_buffer);
        ProcessData(p_stream);

        return VLC_SUCCESS;
    }

    return sout_StreamIdSend(p_stream->p_next, id->id, p_buffer);
}
