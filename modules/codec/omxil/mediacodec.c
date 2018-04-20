/*****************************************************************************
 * mediacodec.c: Video decoder module using the Android MediaCodec API
 *****************************************************************************
 * Copyright (C) 2012 Martin Storsjo
 *
 * Authors: Martin Storsjo <martin@martin.st>
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

#include <stdint.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_aout.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_block_helper.h>
#include <vlc_memory.h>
#include <vlc_timestamp_helper.h>
#include <vlc_threads.h>
#include <vlc_bits.h>

#include "mediacodec.h"
#include "../codec/hxxx_helper.h"
#include <OMX_Core.h>
#include <OMX_Component.h>
#include "omxil_utils.h"
#include "../../video_output/android/display.h"

#define BLOCK_FLAG_CSD (0x01 << BLOCK_FLAG_PRIVATE_SHIFT)

#define DECODE_FLAG_RESTART (0x01)
#define DECODE_FLAG_DRAIN (0x02)
/**
 * Callback called when a new block is processed from DecodeBlock.
 * It returns -1 in case of error, 0 if block should be dropped, 1 otherwise.
 */
typedef int (*dec_on_new_block_cb)(decoder_t *, block_t **);

/**
 * Callback called when decoder is flushing.
 */
typedef void (*dec_on_flush_cb)(decoder_t *);

/**
 * Callback called when DecodeBlock try to get an output buffer (pic or block).
 * It returns -1 in case of error, or the number of output buffer returned.
 */
typedef int (*dec_process_output_cb)(decoder_t *, mc_api_out *, picture_t **,
                                     block_t **);

struct decoder_sys_t
{
    mc_api api;

    /* Codec Specific Data buffer: sent in DecodeBlock after a start or a flush
     * with the BUFFER_FLAG_CODEC_CONFIG flag.*/
    #define MAX_CSD_COUNT 3
    block_t *pp_csd[MAX_CSD_COUNT];
    size_t i_csd_count;
    size_t i_csd_send;

    bool b_has_format;

    int64_t i_preroll_end;
    int     i_quirks;

    /* Specific Audio/Video callbacks */
    dec_on_new_block_cb     pf_on_new_block;
    dec_on_flush_cb         pf_on_flush;
    dec_process_output_cb   pf_process_output;

    vlc_mutex_t     lock;
    vlc_thread_t    out_thread;
    /* Cond used to signal the output thread */
    vlc_cond_t      cond;
    /* Cond used to signal the decoder thread */
    vlc_cond_t      dec_cond;
    /* Set to true by pf_flush to signal the output thread to flush */
    bool            b_flush_out;
    /* If true, the output thread will start to dequeue output pictures */
    bool            b_output_ready;
    /* If true, the first input block was successfully dequeued */
    bool            b_input_dequeued;
    bool            b_aborted;
    bool            b_drained;
    bool            b_adaptive;
    int             i_decode_flags;

    union
    {
        struct
        {
            void *p_surface, *p_jsurface;
            unsigned i_angle;
            unsigned i_input_width, i_input_height;
            unsigned int i_stride, i_slice_height;
            int i_pixel_format;
            struct hxxx_helper hh;
            /* stores the inflight picture for each output buffer or NULL */
            picture_sys_t** pp_inflight_pictures;
            unsigned int i_inflight_pictures;
            timestamp_fifo_t *timestamp_fifo;
            int i_mpeg_dar_num, i_mpeg_dar_den;
        } video;
        struct {
            date_t i_end_date;
            int i_channels;
            bool b_extract;
            /* Some audio decoders need a valid channel count */
            bool b_need_channels;
            int pi_extraction[AOUT_CHAN_MAX];
        } audio;
    };
};

struct encoder_sys_t
{
    mc_api api;
    vlc_mutex_t     lock;
    vlc_thread_t    out_thread;
    /* Cond used to signal the output thread */
    vlc_cond_t      cond;
    vlc_cond_t      enc_cond;

    /* Fifo storing the available encoded blocks, which are
     * returned from EncodeVideo */
    vlc_fifo_t     *fifo_out;

    bool b_started;
    bool b_aborted;
    bool b_flush_out;
    /* if true, start to pop frames from the encoder and push them in the fifo */
    bool b_output_ready;
    bool b_input_dequeued;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoderJni(vlc_object_t *);
static int  OpenDecoderNdk(vlc_object_t *);
static void CleanDecoder(decoder_t *);
static void CloseDecoder(vlc_object_t *);

static int OpenEncoderNdk(vlc_object_t *);
static void CleanEncoder(encoder_t *);
static void CloseEncoder(vlc_object_t *);

static int Video_OnNewBlock(decoder_t *, block_t **);
static int VideoHXXX_OnNewBlock(decoder_t *, block_t **);
static int VideoMPEG2_OnNewBlock(decoder_t *, block_t **);
static int VideoVC1_OnNewBlock(decoder_t *, block_t **);
static void Video_OnFlush(decoder_t *);
static int Video_ProcessOutput(decoder_t *, mc_api_out *, picture_t **,
                               block_t **);
static int DecodeBlock(decoder_t *, block_t *);

static block_t* EncodeVideo(encoder_t *, picture_t *);
static block_t* EncodeAudio(encoder_t *, block_t *);
static block_t* EncodeSub(encoder_t *, subpicture_t *);

static int Audio_OnNewBlock(decoder_t *, block_t **);
static void Audio_OnFlush(decoder_t *);
static int Audio_ProcessOutput(decoder_t *, mc_api_out *, picture_t **,
                               block_t **);

static void DecodeFlushLocked(decoder_t *);
static void DecodeFlush(decoder_t *);
static void EncodeFlushLocked(encoder_t *);
static void EncodeFlush(encoder_t *);
static void StopMediaCodec(decoder_t *);
static void StopMediaCodec_encoder(encoder_t *);
static void *OutThread(void *);
static void *EncoderOutputThread(void *);

static void InvalidateAllPictures(decoder_t *);
static void RemoveInflightPictures(decoder_t *);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define DIRECTRENDERING_TEXT "Android direct rendering"
#define DIRECTRENDERING_LONGTEXT \
    "Enable Android direct rendering using opaque buffers."

#define MEDIACODEC_AUDIO_TEXT "Use MediaCodec for audio decoding"
#define MEDIACODEC_AUDIO_LONGTEXT "Still experimental."

#define MEDIACODEC_TUNNELEDPLAYBACK_TEXT "Use a tunneled surface for playback"

#define CFG_PREFIX "mediacodec-"

vlc_module_begin ()
    set_description("Video decoder using Android MediaCodec via NDK")
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_VCODEC)
    set_section(N_("Decoding"), NULL)
    set_capability("video decoder", 0) /* Only enabled via commandline arguments */
    add_bool(CFG_PREFIX "dr", true,
             DIRECTRENDERING_TEXT, DIRECTRENDERING_LONGTEXT, true)
    add_bool(CFG_PREFIX "audio", false,
             MEDIACODEC_AUDIO_TEXT, MEDIACODEC_AUDIO_LONGTEXT, true)
    add_bool(CFG_PREFIX "tunneled-playback", false,
             MEDIACODEC_TUNNELEDPLAYBACK_TEXT, NULL, true)
    set_callbacks(OpenDecoderNdk, CloseDecoder)
    add_shortcut("mediacodec_ndk")
    add_submodule ()
        set_capability("audio decoder", 0)
        set_callbacks(OpenDecoderNdk, CloseDecoder)
        add_shortcut("mediacodec_ndk")
    add_submodule ()
        set_description("Video decoder using Android MediaCodec via JNI")
        set_capability("video decoder", 0)
        set_callbacks(OpenDecoderJni, CloseDecoder)
        add_shortcut("mediacodec_jni")
    add_submodule ()
        set_capability("audio decoder", 0)
        set_callbacks(OpenDecoderJni, CloseDecoder)
        add_shortcut("mediacodec_jni")
    add_submodule ()
        set_description("Video encoder using Android MediaCodec via NDK")
        set_capability("encoder", 0)
        set_callbacks(OpenEncoderNdk, CloseEncoder)
        add_shortcut("mediacodec")
vlc_module_end ()

static void CSDFree(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    for (unsigned int i = 0; i < p_sys->i_csd_count; ++i)
        block_Release(p_sys->pp_csd[i]);
    p_sys->i_csd_count = 0;
}

/* Init the p_sys->p_csd that will be sent from DecodeBlock */
static void CSDInit(decoder_t *p_dec, block_t *p_blocks, size_t i_count)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    assert(i_count >= 0 && i_count <= 3);

    CSDFree(p_dec);

    for (size_t i = 0; i < i_count; ++i)
    {
        assert(p_blocks != NULL);
        p_sys->pp_csd[i] = p_blocks;
        p_sys->pp_csd[i]->i_flags = BLOCK_FLAG_CSD;
        p_blocks = p_blocks->p_next;
        p_sys->pp_csd[i]->p_next = NULL;
    }

    p_sys->i_csd_count = i_count;
    p_sys->i_csd_send = 0;
}

static int CSDDup(decoder_t *p_dec, const void *p_buf, size_t i_buf)
{
    block_t *p_block = block_Alloc(i_buf);
    if (!p_block)
        return VLC_ENOMEM;
    memcpy(p_block->p_buffer, p_buf, i_buf);

    CSDInit(p_dec, p_block, 1);
    return VLC_SUCCESS;
}

static void HXXXInitSize(decoder_t *p_dec, bool *p_size_changed)
{
    if (p_size_changed)
    {
        decoder_sys_t *p_sys = p_dec->p_sys;
        struct hxxx_helper *hh = &p_sys->video.hh;
        unsigned i_w, i_h, i_vw, i_vh;
        hxxx_helper_get_current_picture_size(hh, &i_w, &i_h, &i_vw, &i_vh);

        *p_size_changed = (i_w != p_sys->video.i_input_width
                        || i_h != p_sys->video.i_input_height);
        p_sys->video.i_input_width = i_w;
        p_sys->video.i_input_height = i_h;
        /* fmt_out video size will be updated by mediacodec output callback */
    }
}

/* Fill the p_sys->p_csd struct with H264 Parameter Sets */
static int H264SetCSD(decoder_t *p_dec, bool *p_size_changed)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    struct hxxx_helper *hh = &p_sys->video.hh;
    assert(hh->h264.i_sps_count > 0 || hh->h264.i_pps_count > 0);

    block_t *p_spspps_blocks = h264_helper_get_annexb_config(hh);

    if (p_spspps_blocks != NULL)
        CSDInit(p_dec, p_spspps_blocks, 2);

    HXXXInitSize(p_dec, p_size_changed);

    return VLC_SUCCESS;
}

/* Fill the p_sys->p_csd struct with HEVC Parameter Sets */
static int HEVCSetCSD(decoder_t *p_dec, bool *p_size_changed)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    struct hxxx_helper *hh = &p_sys->video.hh;

    assert(hh->hevc.i_vps_count > 0 || hh->hevc.i_sps_count > 0 ||
           hh->hevc.i_pps_count > 0 );

    block_t *p_xps_blocks = hevc_helper_get_annexb_config(hh);
    if (p_xps_blocks != NULL)
    {
        block_t *p_monolith = block_ChainGather(p_xps_blocks);
        if (p_monolith == NULL)
        {
            block_ChainRelease(p_xps_blocks);
            return VLC_ENOMEM;
        }
        CSDInit(p_dec, p_monolith, 1);
    }

    HXXXInitSize(p_dec, p_size_changed);
    return VLC_SUCCESS;
}

static int ParseVideoExtraH264(decoder_t *p_dec, uint8_t *p_extra, int i_extra)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    struct hxxx_helper *hh = &p_sys->video.hh;

    int i_ret = hxxx_helper_set_extra(hh, p_extra, i_extra);
    if (i_ret != VLC_SUCCESS)
        return i_ret;
    assert(hh->pf_process_block != NULL);

    if (p_sys->api.i_quirks & MC_API_VIDEO_QUIRKS_ADAPTIVE)
        p_sys->b_adaptive = true;

    p_sys->pf_on_new_block = VideoHXXX_OnNewBlock;

    if (hh->h264.i_sps_count > 0 || hh->h264.i_pps_count > 0)
        return H264SetCSD(p_dec, NULL);
    return VLC_SUCCESS;
}

static int ParseVideoExtraHEVC(decoder_t *p_dec, uint8_t *p_extra, int i_extra)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    struct hxxx_helper *hh = &p_sys->video.hh;

    int i_ret = hxxx_helper_set_extra(hh, p_extra, i_extra);
    if (i_ret != VLC_SUCCESS)
        return i_ret;
    assert(hh->pf_process_block != NULL);

    if (p_sys->api.i_quirks & MC_API_VIDEO_QUIRKS_ADAPTIVE)
        p_sys->b_adaptive = true;

    p_sys->pf_on_new_block = VideoHXXX_OnNewBlock;

    if (hh->hevc.i_vps_count > 0 || hh->hevc.i_sps_count > 0 ||
        hh->hevc.i_pps_count > 0 )
        return HEVCSetCSD(p_dec, NULL);
    return VLC_SUCCESS;
}

static int ParseVideoExtraVc1(decoder_t *p_dec, uint8_t *p_extra, int i_extra)
{
    int offset = 0;

    if (i_extra < 4)
        return VLC_EGENERIC;

    /* Initialisation data starts with : 0x00 0x00 0x01 0x0f */
    /* Skipping unecessary data */
    static const uint8_t vc1_start_code[4] = {0x00, 0x00, 0x01, 0x0f};
    for (; offset < i_extra - 4 ; ++offset)
    {
        if (!memcmp(&p_extra[offset], vc1_start_code, 4))
            break;
    }

    /* Could not find the sequence header start code */
    if (offset >= i_extra - 4)
        return VLC_EGENERIC;

    p_dec->p_sys->pf_on_new_block = VideoVC1_OnNewBlock;
    return CSDDup(p_dec, p_extra + offset, i_extra - offset);
}

static int ParseVideoExtraWmv3(decoder_t *p_dec, uint8_t *p_extra, int i_extra)
{
    /* WMV3 initialisation data :
     * 8 fixed bytes
     * 4 extradata bytes
     * 4 height bytes (little endian)
     * 4 width bytes (little endian)
     * 16 fixed bytes */

    if (i_extra < 4)
        return VLC_EGENERIC;

    uint8_t p_data[36] = {
        0x8e, 0x01, 0x00, 0xc5, /* Fixed bytes values */
        0x04, 0x00, 0x00, 0x00, /* Same */
        0x00, 0x00, 0x00, 0x00, /* extradata emplacement */
        0x00, 0x00, 0x00, 0x00, /* height emplacement (little endian) */
        0x00, 0x00, 0x00, 0x00, /* width emplacement (little endian) */
        0x0c, 0x00, 0x00, 0x00, /* Fixed byte pattern */
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };

    /* Adding extradata */
    memcpy(&p_data[8], p_extra, 4);
    /* Adding height and width, little endian */
    SetDWLE(&(p_data[12]), p_dec->fmt_in.video.i_height);
    SetDWLE(&(p_data[16]), p_dec->fmt_in.video.i_width);

    return CSDDup(p_dec, p_data, sizeof(p_data));
}

static int ParseExtra(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t *p_extra = p_dec->fmt_in.p_extra;
    int i_extra = p_dec->fmt_in.i_extra;

    switch (p_dec->fmt_in.i_codec)
    {
    case VLC_CODEC_H264:
        return ParseVideoExtraH264(p_dec, p_extra, i_extra);
    case VLC_CODEC_HEVC:
        return ParseVideoExtraHEVC(p_dec, p_extra, i_extra);
    case VLC_CODEC_WMV3:
        return ParseVideoExtraWmv3(p_dec, p_extra, i_extra);
    case VLC_CODEC_VC1:
        return ParseVideoExtraVc1(p_dec, p_extra, i_extra);
    case VLC_CODEC_MP4V:
        if (!i_extra && p_sys->api.i_quirks & MC_API_VIDEO_QUIRKS_ADAPTIVE)
            p_sys->b_adaptive = true;
        break;
    case VLC_CODEC_MPGV:
    case VLC_CODEC_MP2V:
        p_dec->p_sys->pf_on_new_block = VideoMPEG2_OnNewBlock;
        break;
    }
    /* Set default CSD */
    if (p_dec->fmt_in.i_extra)
        return CSDDup(p_dec, p_dec->fmt_in.p_extra, p_dec->fmt_in.i_extra);
    else
        return VLC_SUCCESS;
}

static int UpdateVout(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if ((p_dec->fmt_in.i_codec == VLC_CODEC_MPGV ||
         p_dec->fmt_in.i_codec == VLC_CODEC_MP2V) &&
        (p_sys->video.i_mpeg_dar_num * p_sys->video.i_mpeg_dar_den != 0))
    {
        p_dec->fmt_out.video.i_sar_num =
            p_sys->video.i_mpeg_dar_num * p_dec->fmt_out.video.i_height;
        p_dec->fmt_out.video.i_sar_den =
            p_sys->video.i_mpeg_dar_den * p_dec->fmt_out.video.i_width;
    }

    /* If MediaCodec can handle the rotation, reset the orientation to
     * Normal in order to ask the vout not to rotate. */
    if (p_sys->video.i_angle != 0)
    {
        assert(p_dec->fmt_out.i_codec == VLC_CODEC_ANDROID_OPAQUE);
        p_dec->fmt_out.video.orientation = p_dec->fmt_in.video.orientation;
        video_format_TransformTo(&p_dec->fmt_out.video, ORIENT_NORMAL);
    }

    if (decoder_UpdateVideoFormat(p_dec) != 0)
        return VLC_EGENERIC;

    if (p_dec->fmt_out.i_codec != VLC_CODEC_ANDROID_OPAQUE)
        return VLC_SUCCESS;

    /* Direct rendering: get the surface attached to the VOUT */
    picture_t *p_dummy_hwpic = decoder_NewPicture(p_dec);
    if (p_dummy_hwpic == NULL)
        return VLC_EGENERIC;

    assert(p_dummy_hwpic->p_sys);
    assert(p_dummy_hwpic->p_sys->hw.p_surface);
    assert(p_dummy_hwpic->p_sys->hw.p_jsurface);

    p_sys->video.p_surface = p_dummy_hwpic->p_sys->hw.p_surface;
    p_sys->video.p_jsurface = p_dummy_hwpic->p_sys->hw.p_jsurface;
    picture_Release(p_dummy_hwpic);
    return VLC_SUCCESS;
}

/*****************************************************************************
 * StartMediaCodec: Create the mediacodec instance
 *****************************************************************************/
static int StartMediaCodec(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    union mc_api_args args;

    if (p_dec->fmt_in.i_cat == VIDEO_ES)
    {
        args.video.i_width = p_dec->fmt_out.video.i_width;
        args.video.i_height = p_dec->fmt_out.video.i_height;
        args.video.i_angle = p_sys->video.i_angle;

        args.video.p_surface = p_sys->video.p_surface;
        args.video.p_jsurface = p_sys->video.p_jsurface;
        args.video.b_tunneled_playback = args.video.p_surface ?
                var_InheritBool(p_dec, CFG_PREFIX "tunneled-playback") : false;
        if (p_sys->b_adaptive)
            msg_Dbg(p_dec, "mediacodec configured for adaptative playback");
        args.video.b_adaptive_playback = p_sys->b_adaptive;
    }
    else
    {
        date_Set(&p_sys->audio.i_end_date, VLC_TS_INVALID);

        args.audio.i_sample_rate    = p_dec->fmt_in.audio.i_rate;
        args.audio.i_channel_count  = p_dec->p_sys->audio.i_channels;
    }

    return p_sys->api.start(&p_sys->api, &args);
}

static int StartMediaCodec_Encoder(encoder_t *p_enc, const picture_t* p_picture)
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    return p_sys->api.start_encoder(&p_sys->api, p_picture);
}

/*****************************************************************************
 * StopMediaCodec: Close the mediacodec instance
 *****************************************************************************/
static void StopMediaCodec(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* Remove all pictures that are currently in flight in order
     * to prevent the vout from using destroyed output buffers. */
    if (p_sys->api.b_direct_rendering)
        RemoveInflightPictures(p_dec);

    p_sys->api.stop(&p_sys->api);
}

static void StopMediaCodec_Encoder(encoder_t *p_enc)
{
    msg_Dbg(p_enc, "Stopping mediacodec encoder");
    encoder_sys_t *p_sys = p_enc->p_sys;
    p_sys->api.stop(&p_sys->api);
}

/*****************************************************************************
 * OpenDecoder: Create the decoder instance
 *****************************************************************************/
static int OpenDecoder(vlc_object_t *p_this, pf_MediaCodecApi_init pf_init)
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys;
    int i_ret;
    int i_profile = p_dec->fmt_in.i_profile;
    const char *mime = NULL;

    /* Video or Audio if "mediacodec-audio" bool is true */
    if (p_dec->fmt_in.i_cat != VIDEO_ES && (p_dec->fmt_in.i_cat != AUDIO_ES
     || !var_InheritBool(p_dec, CFG_PREFIX "audio")))
        return VLC_EGENERIC;

    /* Fail if this module already failed to decode this ES */
    if (var_Type(p_dec, "mediacodec-failed") != 0)
        return VLC_EGENERIC;

    if (p_dec->fmt_in.i_cat == VIDEO_ES)
    {
        /* Not all mediacodec versions can handle a size of 0. Hopefully, the
         * packetizer will trigger a decoder restart when a new video size is
         * found. */
        if (!p_dec->fmt_in.video.i_width || !p_dec->fmt_in.video.i_height)
            return VLC_EGENERIC;

        switch (p_dec->fmt_in.i_codec) {
        case VLC_CODEC_HEVC:
            if (i_profile == -1)
            {
                uint8_t i_hevc_profile;
                if (hevc_get_profile_level(&p_dec->fmt_in, &i_hevc_profile, NULL, NULL))
                    i_profile = i_hevc_profile;
            }
            mime = "video/hevc";
            break;
        case VLC_CODEC_H264:
            if (i_profile == -1)
            {
                uint8_t i_h264_profile;
                if (h264_get_profile_level(&p_dec->fmt_in, &i_h264_profile, NULL, NULL))
                    i_profile = i_h264_profile;
            }
            mime = "video/avc";
            break;
        case VLC_CODEC_H263: mime = "video/3gpp"; break;
        case VLC_CODEC_MP4V: mime = "video/mp4v-es"; break;
        case VLC_CODEC_MPGV:
        case VLC_CODEC_MP2V:
            mime = "video/mpeg2";
            break;
        case VLC_CODEC_WMV3: mime = "video/x-ms-wmv"; break;
        case VLC_CODEC_VC1:  mime = "video/wvc1"; break;
        case VLC_CODEC_VP8:  mime = "video/x-vnd.on2.vp8"; break;
        case VLC_CODEC_VP9:  mime = "video/x-vnd.on2.vp9"; break;
        }
    }
    else
    {
        switch (p_dec->fmt_in.i_codec) {
        case VLC_CODEC_AMR_NB: mime = "audio/3gpp"; break;
        case VLC_CODEC_AMR_WB: mime = "audio/amr-wb"; break;
        case VLC_CODEC_MPGA:
        case VLC_CODEC_MP3:    mime = "audio/mpeg"; break;
        case VLC_CODEC_MP2:    mime = "audio/mpeg-L2"; break;
        case VLC_CODEC_MP4A:   mime = "audio/mp4a-latm"; break;
        case VLC_CODEC_QCELP:  mime = "audio/qcelp"; break;
        case VLC_CODEC_VORBIS: mime = "audio/vorbis"; break;
        case VLC_CODEC_OPUS:   mime = "audio/opus"; break;
        case VLC_CODEC_ALAW:   mime = "audio/g711-alaw"; break;
        case VLC_CODEC_MULAW:  mime = "audio/g711-mlaw"; break;
        case VLC_CODEC_FLAC:   mime = "audio/flac"; break;
        case VLC_CODEC_GSM:    mime = "audio/gsm"; break;
        case VLC_CODEC_A52:    mime = "audio/ac3"; break;
        case VLC_CODEC_EAC3:   mime = "audio/eac3"; break;
        case VLC_CODEC_ALAC:   mime = "audio/alac"; break;
        case VLC_CODEC_DTS:    mime = "audio/vnd.dts"; break;
        /* case VLC_CODEC_: mime = "audio/mpeg-L1"; break; */
        /* case VLC_CODEC_: mime = "audio/aac-adts"; break; */
        }
    }
    if (!mime)
    {
        msg_Dbg(p_dec, "codec %4.4s not supported",
                (char *)&p_dec->fmt_in.i_codec);
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if ((p_sys = calloc(1, sizeof(*p_sys))) == NULL)
        return VLC_ENOMEM;

    p_sys->api.p_obj = p_this;
    p_sys->api.i_codec = p_dec->fmt_in.i_codec;
    p_sys->api.i_cat = p_dec->fmt_in.i_cat;
    p_sys->api.psz_mime = mime;
    p_sys->video.i_mpeg_dar_num = 0;
    p_sys->video.i_mpeg_dar_den = 0;

    if (pf_init(&p_sys->api) != 0)
    {
        free(p_sys);
        return VLC_EGENERIC;
    }
    if (p_sys->api.configure(&p_sys->api, i_profile, MC_API_FLAG_DECODER) != 0)
    {
        /* If the device can't handle video/wvc1,
         * it can probably handle video/x-ms-wmv */
        if (!strcmp(mime, "video/wvc1") && p_dec->fmt_in.i_codec == VLC_CODEC_VC1)
        {
            p_sys->api.psz_mime = "video/x-ms-wmv";
            if (p_sys->api.configure(&p_sys->api, i_profile, MC_API_FLAG_DECODER) != 0)
            {
                p_sys->api.clean(&p_sys->api);
                free(p_sys);
                return (VLC_EGENERIC);
            }
        }
        else
        {
            p_sys->api.clean(&p_sys->api);
            free(p_sys);
            return VLC_EGENERIC;
        }
    }

    p_dec->p_sys = p_sys;

    vlc_mutex_init(&p_sys->lock);
    vlc_cond_init(&p_sys->cond);
    vlc_cond_init(&p_sys->dec_cond);

    if (p_dec->fmt_in.i_cat == VIDEO_ES)
    {
        switch (p_dec->fmt_in.i_codec)
        {
        case VLC_CODEC_H264:
        case VLC_CODEC_HEVC:
            hxxx_helper_init(&p_sys->video.hh, VLC_OBJECT(p_dec),
                             p_dec->fmt_in.i_codec, false);
            break;
        }
        p_sys->pf_on_new_block = Video_OnNewBlock;
        p_sys->pf_on_flush = Video_OnFlush;
        p_sys->pf_process_output = Video_ProcessOutput;

        p_sys->video.timestamp_fifo = timestamp_FifoNew(32);
        if (!p_sys->video.timestamp_fifo)
            goto bailout;

        TAB_INIT(p_sys->video.i_inflight_pictures,
                 p_sys->video.pp_inflight_pictures);

        if (var_InheritBool(p_dec, CFG_PREFIX "dr"))
        {
            /* Direct rendering: Request a valid OPAQUE Vout in order to get
             * the surface attached to it */
            p_dec->fmt_out.i_codec = VLC_CODEC_ANDROID_OPAQUE;

            if (p_sys->api.b_support_rotation)
            {
                switch (p_dec->fmt_out.video.orientation)
                {
                    case ORIENT_ROTATED_90:
                        p_sys->video.i_angle = 90;
                        break;
                    case ORIENT_ROTATED_180:
                        p_sys->video.i_angle = 180;
                        break;
                    case ORIENT_ROTATED_270:
                        p_sys->video.i_angle = 270;
                        break;
                    default:
                        p_sys->video.i_angle = 0;
                        break;
                }
            }
            else
                p_sys->video.i_angle = 0;

            p_dec->fmt_out.video = p_dec->fmt_in.video;
            if (p_dec->fmt_out.video.i_sar_num * p_dec->fmt_out.video.i_sar_den == 0)
            {
                p_dec->fmt_out.video.i_sar_num = 1;
                p_dec->fmt_out.video.i_sar_den = 1;
            }

            p_sys->video.i_input_width =
            p_dec->fmt_out.video.i_visible_width = p_dec->fmt_out.video.i_width;
            p_sys->video.i_input_height =
            p_dec->fmt_out.video.i_visible_height = p_dec->fmt_out.video.i_height;

            if (UpdateVout(p_dec) != VLC_SUCCESS)
            {
                msg_Err(p_dec, "Opaque Vout request failed");
                goto bailout;
            }
        }
    }
    else
    {
        p_sys->pf_on_new_block = Audio_OnNewBlock;
        p_sys->pf_on_flush = Audio_OnFlush;
        p_sys->pf_process_output = Audio_ProcessOutput;
        p_sys->audio.i_channels = p_dec->fmt_in.audio.i_channels;

        if ((p_sys->api.i_quirks & MC_API_AUDIO_QUIRKS_NEED_CHANNELS)
         && !p_sys->audio.i_channels)
        {
            msg_Warn(p_dec, "codec need a valid channel count");
            goto bailout;
        }

        p_dec->fmt_out.audio = p_dec->fmt_in.audio;
    }

    /* Try first to configure CSD */
    if (ParseExtra(p_dec) != VLC_SUCCESS)
        goto bailout;

    if ((p_sys->api.i_quirks & MC_API_QUIRKS_NEED_CSD) && !p_sys->i_csd_count
     && !p_sys->b_adaptive)
    {
        switch (p_dec->fmt_in.i_codec)
        {
        case VLC_CODEC_H264:
        case VLC_CODEC_HEVC:
            break; /* CSDs will come from hxxx_helper */
        default:
            msg_Warn(p_dec, "Not CSD found for %4.4s",
                     (const char *) &p_dec->fmt_in.i_codec);
            goto bailout;
        }
    }

    i_ret = StartMediaCodec(p_dec);
    if (i_ret != VLC_SUCCESS)
    {
        msg_Err(p_dec, "StartMediaCodec failed");
        goto bailout;
    }

    if (vlc_clone(&p_sys->out_thread, OutThread, p_dec,
                  VLC_THREAD_PRIORITY_LOW))
    {
        msg_Err(p_dec, "vlc_clone failed");
        vlc_mutex_unlock(&p_sys->lock);
        goto bailout;
    }

    p_dec->pf_decode = DecodeBlock;
    p_dec->pf_flush  = DecodeFlush;

    return VLC_SUCCESS;

bailout:
    CleanDecoder(p_dec);
    return VLC_EGENERIC;
}

static int OpenDecoderNdk(vlc_object_t *p_this)
{
    return OpenDecoder(p_this, MediaCodecNdk_Init);
}

static int OpenDecoderJni(vlc_object_t *p_this)
{
    return OpenDecoder(p_this, MediaCodecJni_Init);
}

static int OpenEncoder(vlc_object_t *p_this, pf_MediaCodecApi_init pf_init)
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys;

    const char *mime = NULL;

    if (p_enc->fmt_out.i_cat != VIDEO_ES)
    {
        msg_Err(p_enc, "MediaCodec encoder only support video encoding");
        return VLC_EGENERIC;
    }

    if (!p_enc->fmt_in.video.i_width || !p_enc->fmt_in.video.i_height)
    {
        msg_Err(p_enc, "MediaCodec might not work with video of size 0");
        return VLC_EGENERIC;
    }

    switch (p_enc->fmt_out.i_codec) {
    case VLC_CODEC_H264:
        mime = "video/avc";
        break;
    default:
        break;
    }
    /* Fail if this module already failed to encode this ES */
    //if (var_Type(p_enc, "mediacodec-encoder-failed") != 0)
    //    return VLC_EGENERIC;

    if (mime == NULL)
    {
        msg_Err(p_enc, "Codec %4.4s not supported", (char *)&p_enc->fmt_out.i_codec);
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if ((p_sys = calloc(1, sizeof(*p_sys))) == NULL)
    {
        msg_Err(p_this, "Can't allocate encoder_sys_t");
        return VLC_ENOMEM;
    }

    p_sys->api.p_obj = p_this;
    p_sys->api.i_codec = p_enc->fmt_in.i_codec;
    p_sys->api.i_cat = p_enc->fmt_in.i_cat;
    p_sys->api.psz_mime = mime;

    vlc_mutex_init(&p_sys->lock);
    vlc_cond_init(&p_sys->cond);

    p_enc->p_sys = p_sys;
    p_sys->b_flush_out = false;
    p_sys->b_output_ready = false;

    p_enc->fmt_in.i_codec = VLC_CODEC_I420;

    if (pf_init(&p_sys->api) != 0)
    {
        msg_Err(p_enc, "Can't initialize mediacodec API for mediacodec encoder");
        goto bailout;
    }

    if (p_sys->api.configure(&p_sys->api, p_enc->fmt_out.i_profile, MC_API_FLAG_ENCODER) != 0)
    {
        msg_Err(p_enc, "Can't configure MediaCodec encoder for the given mime type");
        return VLC_EGENERIC;
    }

    //StartMediaCodec_Encoder(p_enc, NULL);

    if (vlc_clone(&p_sys->out_thread, EncoderOutputThread, p_enc,
                VLC_THREAD_PRIORITY_LOW))
    {
        msg_Err(p_enc, "vlc_clone failed");
        vlc_mutex_unlock(&p_sys->lock);
        goto bailout;
    }


    p_enc->pf_encode_video = EncodeVideo;
    p_enc->pf_encode_audio = NULL;
    p_enc->pf_encode_sub   = NULL;

    p_sys->fifo_out = block_FifoNew();

    if (p_sys->fifo_out == NULL)
    {
        msg_Err(p_enc, "Can't allocation fifo block for encoder");
        goto bailout;
    }

    msg_Dbg(p_enc, "Mediacodec encoder successfully created");

    return VLC_SUCCESS;

bailout:
    CleanEncoder(p_enc);
    return VLC_EGENERIC;
}

static int OpenEncoderNdk(vlc_object_t *p_this)
{
    msg_Dbg(p_this, "Opening MediaCodec NDK encoder");
    return OpenEncoder(p_this, MediaCodecNdk_Init);
}

static void AbortDecoderLocked(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (!p_sys->b_aborted)
    {
        p_sys->b_aborted = true;
        vlc_cancel(p_sys->out_thread);
    }
}

static void CleanDecoder(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    vlc_mutex_destroy(&p_sys->lock);
    vlc_cond_destroy(&p_sys->cond);
    vlc_cond_destroy(&p_sys->dec_cond);

    StopMediaCodec(p_dec);

    CSDFree(p_dec);
    p_sys->api.clean(&p_sys->api);

    if (p_dec->fmt_in.i_cat == VIDEO_ES)
    {
        if (p_dec->fmt_in.i_codec == VLC_CODEC_H264
         || p_dec->fmt_in.i_codec == VLC_CODEC_HEVC)
            hxxx_helper_clean(&p_sys->video.hh);

        if (p_sys->video.timestamp_fifo)
            timestamp_FifoRelease(p_sys->video.timestamp_fifo);
    }
    free(p_sys);
}

static void CleanEncoder(encoder_t *p_enc)
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    vlc_mutex_destroy(&p_sys->lock);
    vlc_cond_destroy(&p_sys->cond);

    StopMediaCodec_Encoder(p_enc);

    p_sys->api.clean(&p_sys->api);

    block_FifoRelease(p_sys->fifo_out);

    free(p_sys);
}

/*****************************************************************************
 * CloseDecoder: Close the decoder instance
 *****************************************************************************/
static void CloseDecoder(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    vlc_mutex_lock(&p_sys->lock);
    /* Unblock output thread waiting in dequeue_out */
    DecodeFlushLocked(p_dec);
    /* Cancel the output thread */
    AbortDecoderLocked(p_dec);
    vlc_mutex_unlock(&p_sys->lock);

    vlc_join(p_sys->out_thread, NULL);

    CleanDecoder(p_dec);
}

static void CloseEncoder(vlc_object_t *p_this)
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;

    vlc_mutex_lock(&p_sys->lock);
    EncodeFlushLocked(p_enc);
    vlc_mutex_unlock(&p_sys->lock);

    CleanEncoder(p_enc);
}

/*****************************************************************************
 * vout callbacks
 *****************************************************************************/
static void ReleasePicture(decoder_t *p_dec, unsigned i_index, bool b_render)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    p_sys->api.release_out(&p_sys->api, i_index, b_render);
}

static void ReleasePictureTs(decoder_t *p_dec, unsigned i_index, mtime_t i_ts)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    assert(p_sys->api.release_out_ts);

    p_sys->api.release_out_ts(&p_sys->api, i_index, i_ts * INT64_C(1000));
}

static void InvalidateAllPictures(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    for (unsigned int i = 0; i < p_sys->video.i_inflight_pictures; ++i)
        AndroidOpaquePicture_Release(p_sys->video.pp_inflight_pictures[i],
                                     false);
}

static int InsertInflightPicture(decoder_t *p_dec, picture_sys_t *p_picsys)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (!p_picsys->hw.p_dec)
    {
        p_picsys->hw.p_dec = p_dec;
        p_picsys->hw.pf_release = ReleasePicture;
        if (p_sys->api.release_out_ts)
            p_picsys->hw.pf_release_ts = ReleasePictureTs;
        TAB_APPEND_CAST((picture_sys_t **),
                        p_sys->video.i_inflight_pictures,
                        p_sys->video.pp_inflight_pictures,
                        p_picsys);
    } /* else already attached */
    return 0;
}

static void RemoveInflightPictures(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    for (unsigned int i = 0; i < p_sys->video.i_inflight_pictures; ++i)
        AndroidOpaquePicture_DetachDecoder(p_sys->video.pp_inflight_pictures[i]);
    TAB_CLEAN(p_sys->video.i_inflight_pictures,
              p_sys->video.pp_inflight_pictures);
}

static int Video_ProcessOutput(decoder_t *p_dec, mc_api_out *p_out,
                               picture_t **pp_out_pic, block_t **pp_out_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    (void) pp_out_block;
    assert(pp_out_pic);

    if (p_out->type == MC_OUT_TYPE_BUF)
    {
        picture_t *p_pic = NULL;

        /* If the oldest input block had no PTS, the timestamp of
         * the frame returned by MediaCodec might be wrong so we
         * overwrite it with the corresponding dts. Call FifoGet
         * first in order to avoid a gap if buffers are released
         * due to an invalid format or a preroll */
        int64_t forced_ts = timestamp_FifoGet(p_sys->video.timestamp_fifo);

        if (!p_sys->b_has_format) {
            msg_Warn(p_dec, "Buffers returned before output format is set, dropping frame");
            return p_sys->api.release_out(&p_sys->api, p_out->buf.i_index, false);
        }

        if (p_out->buf.i_ts <= p_sys->i_preroll_end)
            return p_sys->api.release_out(&p_sys->api, p_out->buf.i_index, false);

        if (!p_sys->api.b_direct_rendering && p_out->buf.p_ptr == NULL)
        {
            /* This can happen when receiving an EOS buffer */
            msg_Warn(p_dec, "Invalid buffer, dropping frame");
            return p_sys->api.release_out(&p_sys->api, p_out->buf.i_index, false);
        }

        p_pic = decoder_NewPicture(p_dec);
        if (!p_pic) {
            msg_Warn(p_dec, "NewPicture failed");
            return p_sys->api.release_out(&p_sys->api, p_out->buf.i_index, false);
        }

        if (forced_ts == VLC_TS_INVALID)
            p_pic->date = p_out->buf.i_ts;
        else
            p_pic->date = forced_ts;
        p_pic->b_progressive = true;

        if (p_sys->api.b_direct_rendering)
        {
            p_pic->p_sys->hw.i_index = p_out->buf.i_index;
            InsertInflightPicture(p_dec, p_pic->p_sys);
        } else {
            unsigned int chroma_div;
            GetVlcChromaSizes(p_dec->fmt_out.i_codec,
                              p_dec->fmt_out.video.i_width,
                              p_dec->fmt_out.video.i_height,
                              NULL, NULL, &chroma_div);
            CopyOmxPicture(p_sys->video.i_pixel_format, p_pic,
                           p_sys->video.i_slice_height, p_sys->video.i_stride,
                           (uint8_t *)p_out->buf.p_ptr, chroma_div, NULL);

            if (p_sys->api.release_out(&p_sys->api, p_out->buf.i_index, false))
            {
                picture_Release(p_pic);
                return -1;
            }
        }
        assert(!(*pp_out_pic));
        *pp_out_pic = p_pic;
        return 1;
    } else {
        assert(p_out->type == MC_OUT_TYPE_CONF);
        p_sys->video.i_pixel_format = p_out->conf.video.pixel_format;

        const char *name = "unknown";
        if (!p_sys->api.b_direct_rendering
         && !GetVlcChromaFormat(p_sys->video.i_pixel_format,
                                &p_dec->fmt_out.i_codec, &name))
        {
            msg_Err(p_dec, "color-format not recognized");
            return -1;
        }

        msg_Err(p_dec, "output: %d %s, %dx%d stride %d %d, crop %d %d %d %d",
                p_sys->video.i_pixel_format, name,
                p_out->conf.video.width, p_out->conf.video.height,
                p_out->conf.video.stride, p_out->conf.video.slice_height,
                p_out->conf.video.crop_left, p_out->conf.video.crop_top,
                p_out->conf.video.crop_right, p_out->conf.video.crop_bottom);

        int i_width  = p_out->conf.video.crop_right + 1
                     - p_out->conf.video.crop_left;
        int i_height = p_out->conf.video.crop_bottom + 1
                     - p_out->conf.video.crop_top;
        if (i_width <= 1 || i_height <= 1)
        {
            i_width = p_out->conf.video.width;
            i_height = p_out->conf.video.height;
        }

        p_dec->fmt_out.video.i_visible_width =
        p_dec->fmt_out.video.i_width = i_width;
        p_dec->fmt_out.video.i_visible_height =
        p_dec->fmt_out.video.i_height = i_height;

        p_sys->video.i_stride = p_out->conf.video.stride;
        p_sys->video.i_slice_height = p_out->conf.video.slice_height;
        if (p_sys->video.i_stride <= 0)
            p_sys->video.i_stride = p_out->conf.video.width;
        if (p_sys->video.i_slice_height <= 0)
            p_sys->video.i_slice_height = p_out->conf.video.height;

        if (p_sys->video.i_pixel_format == OMX_TI_COLOR_FormatYUV420PackedSemiPlanar)
            p_sys->video.i_slice_height -= p_out->conf.video.crop_top/2;
        if ((p_sys->api.i_quirks & MC_API_VIDEO_QUIRKS_IGNORE_PADDING))
        {
            p_sys->video.i_slice_height = 0;
            p_sys->video.i_stride = p_dec->fmt_out.video.i_width;
        }

        if (UpdateVout(p_dec) != VLC_SUCCESS)
        {
            msg_Err(p_dec, "UpdateVout failed");
            return -1;
        }

        p_sys->b_has_format = true;
        return 0;
    }
}

/* samples will be in the following order: FL FR FC LFE BL BR BC SL SR */
static uint32_t pi_audio_order_src[] =
{
    AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT, AOUT_CHAN_CENTER, AOUT_CHAN_LFE,
    AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT, AOUT_CHAN_REARCENTER,
    AOUT_CHAN_MIDDLELEFT, AOUT_CHAN_MIDDLERIGHT,
};

static int Audio_ProcessOutput(decoder_t *p_dec, mc_api_out *p_out,
                               picture_t **pp_out_pic, block_t **pp_out_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    (void) pp_out_pic;
    assert(pp_out_block);

    if (p_out->type == MC_OUT_TYPE_BUF)
    {
        block_t *p_block = NULL;
        if (p_out->buf.p_ptr == NULL)
        {
            /* This can happen when receiving an EOS buffer */
            msg_Warn(p_dec, "Invalid buffer, dropping frame");
            return p_sys->api.release_out(&p_sys->api, p_out->buf.i_index, false);
        }

        if (!p_sys->b_has_format) {
            msg_Warn(p_dec, "Buffers returned before output format is set, dropping frame");
            return p_sys->api.release_out(&p_sys->api, p_out->buf.i_index, false);
        }

        p_block = block_Alloc(p_out->buf.i_size);
        if (!p_block)
            return -1;
        p_block->i_nb_samples = p_out->buf.i_size
                              / p_dec->fmt_out.audio.i_bytes_per_frame;

        if (p_sys->audio.b_extract)
        {
            aout_ChannelExtract(p_block->p_buffer,
                                p_dec->fmt_out.audio.i_channels,
                                p_out->buf.p_ptr, p_sys->audio.i_channels,
                                p_block->i_nb_samples, p_sys->audio.pi_extraction,
                                p_dec->fmt_out.audio.i_bitspersample);
        }
        else
            memcpy(p_block->p_buffer, p_out->buf.p_ptr, p_out->buf.i_size);

        if (p_out->buf.i_ts != 0
         && p_out->buf.i_ts != date_Get(&p_sys->audio.i_end_date))
            date_Set(&p_sys->audio.i_end_date, p_out->buf.i_ts);

        p_block->i_pts = date_Get(&p_sys->audio.i_end_date);
        p_block->i_length = date_Increment(&p_sys->audio.i_end_date,
                                           p_block->i_nb_samples)
                          - p_block->i_pts;

        if (p_sys->api.release_out(&p_sys->api, p_out->buf.i_index, false))
        {
            block_Release(p_block);
            return -1;
        }
        *pp_out_block = p_block;
        return 1;
    } else {
        uint32_t i_layout_dst;
        int      i_channels_dst;

        assert(p_out->type == MC_OUT_TYPE_CONF);

        if (p_out->conf.audio.channel_count <= 0
         || p_out->conf.audio.channel_count > 8
         || p_out->conf.audio.sample_rate <= 0)
        {
            msg_Warn(p_dec, "invalid audio properties channels count %d, sample rate %d",
                     p_out->conf.audio.channel_count,
                     p_out->conf.audio.sample_rate);
            return -1;
        }

        msg_Err(p_dec, "output: channel_count: %d, channel_mask: 0x%X, rate: %d",
                p_out->conf.audio.channel_count, p_out->conf.audio.channel_mask,
                p_out->conf.audio.sample_rate);

        p_dec->fmt_out.i_codec = VLC_CODEC_S16N;
        p_dec->fmt_out.audio.i_format = p_dec->fmt_out.i_codec;

        p_dec->fmt_out.audio.i_rate = p_out->conf.audio.sample_rate;
        date_Init(&p_sys->audio.i_end_date, p_out->conf.audio.sample_rate, 1);

        p_sys->audio.i_channels = p_out->conf.audio.channel_count;
        p_sys->audio.b_extract =
            aout_CheckChannelExtraction(p_sys->audio.pi_extraction,
                                        &i_layout_dst, &i_channels_dst,
                                        NULL, pi_audio_order_src,
                                        p_sys->audio.i_channels);

        if (p_sys->audio.b_extract)
            msg_Warn(p_dec, "need channel extraction: %d -> %d",
                     p_sys->audio.i_channels, i_channels_dst);

        p_dec->fmt_out.audio.i_physical_channels = i_layout_dst;
        aout_FormatPrepare(&p_dec->fmt_out.audio);

        if (decoder_UpdateAudioFormat(p_dec))
            return -1;

        p_sys->b_has_format = true;
        return 0;
    }
}

static void DecodeFlushLocked(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    bool b_had_input = p_sys->b_input_dequeued;

    p_sys->b_input_dequeued = false;
    p_sys->b_flush_out = true;
    p_sys->i_preroll_end = 0;
    p_sys->b_output_ready = false;
    /* Resend CODEC_CONFIG buffer after a flush */
    p_sys->i_csd_send = 0;

    p_sys->pf_on_flush(p_dec);

    if (b_had_input && p_sys->api.flush(&p_sys->api) != VLC_SUCCESS)
    {
        AbortDecoderLocked(p_dec);
        return;
    }

    vlc_cond_broadcast(&p_sys->cond);

    while (!p_sys->b_aborted && p_sys->b_flush_out)
        vlc_cond_wait(&p_sys->dec_cond, &p_sys->lock);
}

static void DecodeFlush(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    vlc_mutex_lock(&p_sys->lock);
    DecodeFlushLocked(p_dec);
    vlc_mutex_unlock(&p_sys->lock);
}

static void EncodeFlushLocked(encoder_t *p_enc)
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    bool b_had_input = p_sys->b_input_dequeued;

    if (b_had_input && p_sys->api.flush(&p_sys->api) != VLC_SUCCESS)
    {
        // TODO: error
        return;
    }

    vlc_cond_broadcast(&p_sys->cond);

    while (!p_sys->b_aborted && p_sys->b_flush_out)
        vlc_cond_wait(&p_sys->enc_cond, &p_sys->lock);
}

static void EncodeFlush(encoder_t *p_enc)
{
    encoder_sys_t *p_sys = p_enc->p_sys;

    vlc_mutex_lock(&p_sys->lock);
    EncodeFlushLocked(p_enc);
    vlc_mutex_unlock(&p_sys->lock);
}

static void *OutThread(void *data)
{
    decoder_t *p_dec = data;
    decoder_sys_t *p_sys = p_dec->p_sys;

    vlc_mutex_lock(&p_sys->lock);
    mutex_cleanup_push(&p_sys->lock);
    for (;;)
    {
        int i_index;

        /* Wait for output ready */
        while (!p_sys->b_flush_out && !p_sys->b_output_ready)
            vlc_cond_wait(&p_sys->cond, &p_sys->lock);

        if (p_sys->b_flush_out)
        {
            /* Acknowledge flushed state */
            p_sys->b_flush_out = false;
            vlc_cond_broadcast(&p_sys->dec_cond);
            continue;
        }

        int canc = vlc_savecancel();

        vlc_mutex_unlock(&p_sys->lock);

        /* Wait for an output buffer. This function returns when a new output
         * is available or if output is flushed. */
        i_index = p_sys->api.dequeue_out(&p_sys->api, -1);

        vlc_mutex_lock(&p_sys->lock);

        /* Ignore dequeue_out errors caused by flush */
        if (p_sys->b_flush_out)
        {
            /* If i_index >= 0, Release it. There is no way to know if i_index
             * is owned by us, so don't check the error. */
            if (i_index >= 0)
                p_sys->api.release_out(&p_sys->api, i_index, false);

            /* Parse output format/buffers even when we are flushing */
            if (i_index != MC_API_INFO_OUTPUT_FORMAT_CHANGED
             && i_index != MC_API_INFO_OUTPUT_BUFFERS_CHANGED)
            {
                vlc_restorecancel(canc);
                continue;
            }
        }

        /* Process output returned by dequeue_out */
        if (i_index >= 0 || i_index == MC_API_INFO_OUTPUT_FORMAT_CHANGED
         || i_index == MC_API_INFO_OUTPUT_BUFFERS_CHANGED)
        {
            struct mc_api_out out;
            int i_ret = p_sys->api.get_out(&p_sys->api, i_index, &out);

            if (i_ret == 1)
            {
                picture_t *p_pic = NULL;
                block_t *p_block = NULL;

                if (p_sys->pf_process_output(p_dec, &out, &p_pic,
                                             &p_block) == -1 && !out.b_eos)
                {
                    msg_Err(p_dec, "pf_process_output failed");
                    vlc_restorecancel(canc);
                    break;
                }
                if (p_pic)
                    decoder_QueueVideo(p_dec, p_pic);
                else if (p_block)
                    decoder_QueueAudio(p_dec, p_block);

                if (out.b_eos)
                {
                    msg_Warn(p_dec, "EOS received");
                    p_sys->b_drained = true;
                    vlc_cond_signal(&p_sys->dec_cond);
                }
            } else if (i_ret != 0)
            {
                msg_Err(p_dec, "get_out failed");
                vlc_restorecancel(canc);
                break;
            }
        }
        else
        {
            vlc_restorecancel(canc);
            break;
        }
        vlc_restorecancel(canc);
    }
    msg_Warn(p_dec, "OutThread stopped");

    /* Signal DecoderFlush that the output thread aborted */
    p_sys->b_aborted = true;
    vlc_cond_signal(&p_sys->dec_cond);

    vlc_cleanup_pop();
    vlc_mutex_unlock(&p_sys->lock);

    return NULL;
}

static block_t *GetNextBlock(decoder_sys_t *p_sys, block_t *p_block)
{
    if (p_sys->i_csd_send < p_sys->i_csd_count)
        return p_sys->pp_csd[p_sys->i_csd_send++];
    else
        return p_block;
}

static int QueueBlockLocked(decoder_t *p_dec, block_t *p_in_block,
                            bool b_drain)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block = NULL;
    bool b_dequeue_timeout = false;

    assert(p_sys->api.b_started);

    if ((p_sys->api.i_quirks & MC_API_QUIRKS_NEED_CSD) && !p_sys->i_csd_count
     && !p_sys->b_adaptive)
        return VLC_EGENERIC; /* Wait for CSDs */

    /* Queue CSD blocks and input blocks */
    while (b_drain || (p_block = GetNextBlock(p_sys, p_in_block)))
    {
        int i_index;

        vlc_mutex_unlock(&p_sys->lock);
        /* Wait for an input buffer. This function returns when a new input
         * buffer is available or after 2secs of timeout. */
        i_index = p_sys->api.dequeue_in(&p_sys->api,
                                        p_sys->api.b_direct_rendering ?
                                        INT64_C(2000000) : -1);
        vlc_mutex_lock(&p_sys->lock);

        if (p_sys->b_aborted)
            return VLC_EGENERIC;

        bool b_config = false;
        mtime_t i_ts = 0;
        p_sys->b_input_dequeued = true;
        const void *p_buf = NULL;
        size_t i_size = 0;

        if (i_index >= 0)
        {
            assert(b_drain || p_block != NULL);
            if (p_block != NULL)
            {
                b_config = (p_block->i_flags & BLOCK_FLAG_CSD);
                if (!b_config)
                {
                    i_ts = p_block->i_pts;
                    if (!i_ts && p_block->i_dts)
                        i_ts = p_block->i_dts;
                }
                p_buf = p_block->p_buffer;
                i_size = p_block->i_buffer;
            }

            if (p_sys->api.queue_in(&p_sys->api, i_index, p_buf, i_size,
                                    i_ts, b_config) == 0)
            {
                if (!b_config && p_block != NULL)
                {
                    if (p_block->i_flags & BLOCK_FLAG_PREROLL)
                        p_sys->i_preroll_end = i_ts;

                    /* One input buffer is queued, signal OutThread that will
                     * fetch output buffers */
                    p_sys->b_output_ready = true;
                    vlc_cond_broadcast(&p_sys->cond);

                    assert(p_block == p_in_block),
                    p_in_block = NULL;
                }
                b_dequeue_timeout = false;
                if (b_drain)
                    break;
            } else
            {
                msg_Err(p_dec, "queue_in failed");
                goto error;
            }
        }
        else if (i_index == MC_API_INFO_TRYAGAIN)
        {
            /* HACK: When direct rendering is enabled, there is a possible
             * deadlock between the Decoder and the Vout. It happens when the
             * Vout is paused and when the Decoder is flushing. In that case,
             * the Vout won't release any output buffers, therefore MediaCodec
             * won't dequeue any input buffers. To work around this issue,
             * release all output buffers if DecodeBlock is waiting more than
             * 2secs for a new input buffer. */
            if (!b_dequeue_timeout)
            {
                msg_Warn(p_dec, "Decoder stuck: invalidate all buffers");
                InvalidateAllPictures(p_dec);
                b_dequeue_timeout = true;
                continue;
            }
            else
            {
                msg_Err(p_dec, "dequeue_in timeout: no input available for 2secs");
                goto error;
            }
        }
        else
        {
            msg_Err(p_dec, "dequeue_in failed");
            goto error;
        }
    }

    if (b_drain)
    {
        msg_Warn(p_dec, "EOS sent, waiting for OutThread");

        /* Wait for the OutThread to stop (and process all remaining output
         * frames. Use a timeout here since we can't know if all decoders will
         * behave correctly. */
        mtime_t deadline = mdate() + INT64_C(3000000);
        while (!p_sys->b_aborted && !p_sys->b_drained
            && vlc_cond_timedwait(&p_sys->dec_cond, &p_sys->lock, deadline) == 0);

        if (!p_sys->b_drained)
        {
            msg_Err(p_dec, "OutThread timed out");
            AbortDecoderLocked(p_dec);
        }
        p_sys->b_drained = false;
    }

    return VLC_SUCCESS;

error:
    AbortDecoderLocked(p_dec);
    return VLC_EGENERIC;
}

static int DecodeBlock(decoder_t *p_dec, block_t *p_in_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i_ret;

    vlc_mutex_lock(&p_sys->lock);

    if (p_sys->b_aborted)
    {
        if (p_sys->b_has_format)
            goto end;
        else
            goto reload;
    }

    if (p_in_block == NULL)
    {
        /* No input block, decoder is draining */
        msg_Err(p_dec, "Decoder is draining");

        if (p_sys->b_output_ready)
            QueueBlockLocked(p_dec, NULL, true);
        goto end;
    }

    if (p_in_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED))
    {
        if (p_sys->b_output_ready)
            QueueBlockLocked(p_dec, NULL, true);
        DecodeFlushLocked(p_dec);
        if (p_sys->b_aborted)
            goto end;
        if (p_in_block->i_flags & BLOCK_FLAG_CORRUPTED)
            goto end;
    }

    if (p_in_block->i_flags & BLOCK_FLAG_INTERLACED_MASK
     && !(p_sys->api.i_quirks & MC_API_VIDEO_QUIRKS_SUPPORT_INTERLACED))
    {
        /* Before Android 21 and depending on the vendor, MediaCodec can
         * crash or be in an inconsistent state when decoding interlaced
         * videos. See OMXCodec_GetQuirks() for a white list of decoders
         * that supported interlaced videos before Android 21. */
        msg_Warn(p_dec, "codec doesn't support interlaced videos");
        goto reload;
    }

    /* Parse input block */
    if ((i_ret = p_sys->pf_on_new_block(p_dec, &p_in_block)) != 1)
    {
        if (i_ret != 0)
        {
            AbortDecoderLocked(p_dec);
            msg_Err(p_dec, "pf_on_new_block failed");
        }
        goto end;
    }
    if (p_sys->i_decode_flags & (DECODE_FLAG_DRAIN|DECODE_FLAG_RESTART))
    {
        msg_Warn(p_dec, "Draining from DecodeBlock");
        const bool b_restart = p_sys->i_decode_flags & DECODE_FLAG_RESTART;
        p_sys->i_decode_flags = 0;

        /* Drain and flush before restart to unblock OutThread */
        if (p_sys->b_output_ready)
            QueueBlockLocked(p_dec, NULL, true);
        DecodeFlushLocked(p_dec);
        if (p_sys->b_aborted)
            goto end;

        if (b_restart)
        {
            StopMediaCodec(p_dec);

            int i_ret = StartMediaCodec(p_dec);
            switch (i_ret)
            {
            case VLC_SUCCESS:
                msg_Warn(p_dec, "Restarted from DecodeBlock");
                break;
            case VLC_ENOOBJ:
                break;
            default:
                msg_Err(p_dec, "StartMediaCodec failed");
                AbortDecoderLocked(p_dec);
                goto end;
            }
        }
    }

    /* Abort if MediaCodec is not yet started */
    if (p_sys->api.b_started)
        QueueBlockLocked(p_dec, p_in_block, false);

end:
    if (p_in_block)
        block_Release(p_in_block);
    /* Too late to reload here, we already modified/released the input block,
     * do it next time. */
    int ret = p_sys->b_aborted && p_sys->b_has_format ? VLCDEC_ECRITICAL
                                                      : VLCDEC_SUCCESS;
    vlc_mutex_unlock(&p_sys->lock);
    return ret;

reload:
    vlc_mutex_unlock(&p_sys->lock);
    /* Add an empty variable so that mediacodec won't be loaded again
     * for this ES */
    var_Create(p_dec, "mediacodec-failed", VLC_VAR_VOID);
    return VLCDEC_RELOAD;
}

static block_t* EncodeVideo(encoder_t *p_enc, picture_t *picture)
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    struct mc_api_out out;

    if (!p_sys->api.b_started)
    {
        if (!picture && !p_sys->b_started)
            return NULL;

        msg_Dbg(p_enc, "Encoding video");

        int i_ret = StartMediaCodec_Encoder(p_enc, picture);
        if (i_ret != VLC_SUCCESS)
        {
            msg_Err(p_enc, "StartMediaCodec failed");
            return NULL;
        }
    }

    // XXX: should we check an output before ?

    // Request input buffer to MC
    int i_index = p_sys->api.dequeue_in(&p_sys->api, -1);

    if (i_index >= 0)
    {
        /*
         * Send data with timestamp, telling MC it's not a
         * configure buffer. If p_buff is null, it will send
         * an end_of_stream signal to MC
         */
        p_sys->api.queue_picture_in(&p_sys->api, i_index, picture,
                                     picture->date, false);
        p_sys->b_output_ready = true;
    }

    /*
     * Return the first encoded block available
     */
    if (vlc_fifo_GetCount(p_sys->fifo_out) > 0)
    {
        msg_Dbg(p_enc, "Popping one frame from the encoder");
        vlc_fifo_Lock(p_sys->fifo_out);
        block_t *out_block = block_FifoGet(p_sys->fifo_out);
        vlc_fifo_Unlock(p_sys->fifo_out);

        return out_block;
    }

    /*
     * There can be no available output buffer, because the encoder
     * might need multiple frame so as to output some data.
     * In this case we return NULL */
    return NULL;
}

/*
 *  Asynchronous task waiting for a new block to become available from
 *  the encoder and pushing it to a FIFO so it can be returned in the
 *  EncodeBlock function
 */
static void* EncoderOutputThread(void *p_obj)
{
    encoder_t *p_enc = (encoder_t *)p_obj;
    encoder_sys_t *p_sys = p_enc->p_sys;

    struct mc_api_out out;

    vlc_mutex_lock(&p_sys->lock);
    mutex_cleanup_push(&p_sys->lock);
    for(;;)
    {
        // TODO: wait encoder in correct state

        /* Wait for output ready */
        while (!p_sys->b_flush_out && !p_sys->b_output_ready)
            vlc_cond_wait(&p_sys->cond, &p_sys->lock);

        int canc = vlc_savecancel();

        // Check if an output buffer is available
        // TODO: move into its own thread
        int i_index = p_sys->api.dequeue_out(&p_sys->api, -1);

        // TODO: check TRYAGAIN, errors and code
        if (i_index >= 0)
        {
            /*
             * Extract the buffer we were allowed to read from the
             * dequeue_out call
             */
            int i_ret = p_sys->api.get_out(&p_sys->api, i_index, &out);

            // TODO: check TRYAGAIN, errors and code

            if (out.b_eos)
            {

            }

            // TODO: can it be a config buffer?
            /*
             * Allocate a new block with the data of the buffer
             * we just got from get_out call.
             * The MediaCodec API advises that we should release
             * this buffer as soon as possible because it can
             * stall the encoder.
             */
            block_t *p_block = block_Alloc(out.buf.i_size);
            p_block->i_pts = out.buf.i_ts;
            p_block->i_dts = out.buf.i_ts;
            memcpy(p_block->p_buffer, out.buf.p_ptr, out.buf.i_size);

            p_sys->api.release_out(&p_sys->api, i_index, false);

            /* push block into the queue so that it is returned at the next call */
            vlc_fifo_Lock(p_sys->fifo_out);
            vlc_fifo_QueueUnlocked(p_sys->fifo_out, p_block);
            vlc_fifo_Unlock(p_sys->fifo_out);
        }
        else
        {
            vlc_restorecancel(canc);
            break;
        }
    }
    msg_Warn(p_enc, "Encoder output thread stopped");

    vlc_cleanup_pop();
    vlc_mutex_unlock(&p_sys->lock);

    return NULL;
}

static int Video_OnNewBlock(decoder_t *p_dec, block_t **pp_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block = *pp_block;

    timestamp_FifoPut(p_sys->video.timestamp_fifo,
                      p_block->i_pts ? VLC_TS_INVALID : p_block->i_dts);

    return 1;
}

static int VideoHXXX_OnNewBlock(decoder_t *p_dec, block_t **pp_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    struct hxxx_helper *hh = &p_sys->video.hh;
    bool b_config_changed = false;
    bool *p_config_changed = p_sys->b_adaptive ? NULL : &b_config_changed;

    *pp_block = hh->pf_process_block(hh, *pp_block, p_config_changed);
    if (!*pp_block)
        return 0;
    if (b_config_changed)
    {
        bool b_size_changed;
        int i_ret;
        switch (p_dec->fmt_in.i_codec)
        {
        case VLC_CODEC_H264:
            if (hh->h264.i_sps_count > 0 || hh->h264.i_pps_count > 0)
                i_ret = H264SetCSD(p_dec, &b_size_changed);
            else
                i_ret = VLC_EGENERIC;
            break;
        case VLC_CODEC_HEVC:
            if (hh->hevc.i_vps_count > 0 || hh->hevc.i_sps_count > 0 ||
                hh->hevc.i_pps_count > 0 )
                i_ret = HEVCSetCSD(p_dec, &b_size_changed);
            else
                i_ret = VLC_EGENERIC;
            break;
        }
        if (i_ret != VLC_SUCCESS)
            return i_ret;
        if (b_size_changed || !p_sys->api.b_started)
        {
            if (p_sys->api.b_started)
                msg_Err(p_dec, "SPS/PPS changed during playback and "
                        "video size are different. Restart it !");
            p_sys->i_decode_flags |= DECODE_FLAG_RESTART;
        } else
        {
            msg_Err(p_dec, "SPS/PPS changed during playback. Drain it");
            p_sys->i_decode_flags |= DECODE_FLAG_DRAIN;
        }
    }

    return Video_OnNewBlock(p_dec, pp_block);
}

static int VideoMPEG2_OnNewBlock(decoder_t *p_dec, block_t **pp_block)
{
    if (pp_block == NULL || (*pp_block)->i_buffer <= 7)
        return 1;

    decoder_sys_t *p_sys = p_dec->p_sys;
    const int startcode = (*pp_block)->p_buffer[3];

    /* DAR aspect ratio from the DVD MPEG2 standard */
    static const int mpeg2_aspect[16][2] =
    {
        {0,0}, /* reserved */
        {0,0}, /* DAR = 0:0 will result in SAR = 1:1 */
        {4,3}, {16,9}, {221,100},
        /* reserved */
        {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
        {0,0}, {0,0}
    };

    if (startcode == 0xB3 /* SEQUENCE_HEADER_STARTCODE */)
    {
        int mpeg_dar_code = (*pp_block)->p_buffer[7] >> 4;

        if (mpeg_dar_code >= 16)
            return 0;

        p_sys->video.i_mpeg_dar_num = mpeg2_aspect[mpeg_dar_code][0];
        p_sys->video.i_mpeg_dar_den = mpeg2_aspect[mpeg_dar_code][1];
    }

    return 1;
}

static int VideoVC1_OnNewBlock(decoder_t *p_dec, block_t **pp_block)
{
    block_t *p_block = *pp_block;

    /* Adding frame start code */
    p_block = *pp_block = block_Realloc(p_block, 4, p_block->i_buffer);
    if (p_block == NULL)
        return VLC_ENOMEM;
    p_block->p_buffer[0] = 0x00;
    p_block->p_buffer[1] = 0x00;
    p_block->p_buffer[2] = 0x01;
    p_block->p_buffer[3] = 0x0d;

    return Video_OnNewBlock(p_dec, pp_block);
}

static void Video_OnFlush(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    timestamp_FifoEmpty(p_sys->video.timestamp_fifo);
    /* Invalidate all pictures that are currently in flight
     * since flushing make all previous indices returned by
     * MediaCodec invalid. */
    if (p_sys->api.b_direct_rendering)
        InvalidateAllPictures(p_dec);
}

static int Audio_OnNewBlock(decoder_t *p_dec, block_t **pp_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block = *pp_block;

    /* We've just started the stream, wait for the first PTS. */
    if (!date_Get(&p_sys->audio.i_end_date))
    {
        if (p_block->i_pts <= VLC_TS_INVALID)
            return 0;
        date_Set(&p_sys->audio.i_end_date, p_block->i_pts);
    }

    return 1;
}

static void Audio_OnFlush(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    date_Set(&p_sys->audio.i_end_date, VLC_TS_INVALID);
}
