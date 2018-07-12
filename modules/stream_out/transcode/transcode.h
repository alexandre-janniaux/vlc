#include <vlc_picture_fifo.h>
#include <vlc_filter.h>
#include <vlc_codec.h>

/*100ms is around the limit where people are noticing lipsync issues*/
#define MASTER_SYNC_MAX_DRIFT VLC_TICK_FROM_MS(100)
#define FIRSTVALID(a,b,c) ( a ? a : ( b ? b : c ) )

typedef struct sout_stream_id_sys_t sout_stream_id_sys_t;

typedef struct
{
    char *psz_filters;
    union
    {
        struct
        {
            char            *psz_deinterlace;
            config_chain_t  *p_deinterlace_cfg;
            char            *psz_spu_sources;
        } video;
    };
} sout_filters_config_t;

static inline
void sout_filters_config_init( sout_filters_config_t *p_cfg )
{
    memset( p_cfg, 0, sizeof(*p_cfg) );
}

static inline
void sout_filters_config_clean( sout_filters_config_t *p_cfg )
{
    free( p_cfg->psz_filters );
    if( p_cfg->video.psz_deinterlace )
    {
        free( p_cfg->video.psz_deinterlace );
        config_ChainDestroy( p_cfg->video.p_deinterlace_cfg );
    }
    free( p_cfg->video.psz_spu_sources );
}

typedef struct
{
    vlc_fourcc_t i_codec; /* (0 if not transcode) */
    char         *psz_name;
    char         *psz_lang;
    config_chain_t *p_config_chain;
    union
    {
        struct
        {
            unsigned int    i_bitrate;
            float           f_scale;
            unsigned int    i_width, i_maxwidth;
            unsigned int    i_height, i_maxheight;
            bool            b_hurry_up;
            vlc_rational_t  fps;
            struct
            {
                unsigned int i_count;
                int          i_priority;
                uint32_t     pool_size;
            } threads;
        } video;
        struct
        {
            unsigned int    i_bitrate;
            uint32_t        i_sample_rate;
            uint32_t        i_channels;
        } audio;
        struct
        {
            unsigned int    i_width; /* render width */
            unsigned int    i_height;
        } spu;
    };
} sout_encoder_config_t;

static inline
void sout_encoder_config_init( sout_encoder_config_t *p_cfg )
{
    memset( p_cfg, 0, sizeof(*p_cfg) );
}

static inline
void sout_encoder_config_clean( sout_encoder_config_t *p_cfg )
{
    free( p_cfg->psz_name );
    free( p_cfg->psz_lang );
    config_ChainDestroy( p_cfg->p_config_chain );
}

typedef struct
{
    sout_stream_id_sys_t *id_video;

    bool                  b_soverlay;

    /* Audio */
    sout_encoder_config_t aenc_cfg;
    sout_filters_config_t afilters_cfg;

    /* Video */
    sout_encoder_config_t venc_cfg;
    sout_filters_config_t vfilters_cfg;

    /* SPU */
    sout_encoder_config_t senc_cfg;

    /* Sync */
    bool            b_master_sync;
    sout_stream_id_sys_t *id_master_sync;

} sout_stream_sys_t;

struct aout_filters;

struct sout_stream_id_sys_t
{
    bool            b_transcode;
    bool            b_error;

    /* id of the out stream */
    void *downstream_id;

    /* Decoder */
    decoder_t       *p_decoder;

    struct
    {
        vlc_mutex_t lock;
        union
        {
            struct {
                picture_t *first;
                picture_t **last;
            } pic;
            struct {
                subpicture_t *first;
                subpicture_t **last;
            } spu;
            struct {
                block_t *first;
                block_t **last;
            } audio;
        };
    } fifo;

    union
    {
        struct
        {
            int (*pf_drift_validate)(void *cbdata, vlc_tick_t);
        };
        struct
        {
            void (*pf_send_subpicture)(void *cbdata, subpicture_t *);
            int (*pf_get_output_dimensions)(void *cbdata, unsigned *, unsigned *);
            vlc_tick_t (*pf_get_master_drift)(void *cbdata);
        };
    };
    void *callback_data;

    union
    {
         struct
         {
             filter_chain_t  *p_f_chain; /**< Video filters */
             filter_chain_t  *p_uf_chain; /**< User-specified video filters */
             filter_t        *p_spu_blender;
             spu_t           *p_spu;
             video_format_t  fmt_input_video;
             video_format_t  video_dec_out; /* only rw from pf_vout_format_update() */
         };
         struct
         {
             struct aout_filters    *p_af_chain; /**< Audio filters */
             audio_format_t  fmt_input_audio;
             audio_format_t  audio_dec_out; /* only rw from pf_aout_format_update() */
         };
    };
    const sout_filters_config_t *p_filterscfg;

    /* Encoder */
    const sout_encoder_config_t *p_enccfg;
    encoder_t       *p_encoder;
    vlc_thread_t    thread;
    vlc_mutex_t     lock_out;
    bool            b_abort;
    picture_fifo_t *pp_pics;
    vlc_sem_t       picture_pool_has_room;
    vlc_cond_t      cond;
    es_format_t     encoder_tested_fmt_in;

    /* output buffers */
    block_t         *p_buffers;

    /* Packetizer */
    decoder_t       *p_packetizer;

    /* Last registered format */
    es_format_t     last_fmt;

    /* Sync */
    date_t          next_input_pts; /**< Incoming calculated PTS */
    vlc_tick_t      i_drift; /** how much buffer is ahead of calculated PTS */
};

struct decoder_owner
{
    decoder_t dec;
    vlc_object_t *p_obj;
    sout_stream_id_sys_t *id;
};

static inline struct decoder_owner *dec_get_owner( decoder_t *p_dec )
{
    return container_of( p_dec, struct decoder_owner, dec );
}

/* SPU */

void transcode_spu_close  ( sout_stream_t *, sout_stream_id_sys_t * );
int  transcode_spu_process( sout_stream_t *, sout_stream_id_sys_t *,
                                   block_t *, block_t ** );
bool transcode_spu_add    ( sout_stream_t *, const es_format_t *, sout_stream_id_sys_t *);

/* AUDIO */

void transcode_audio_close  ( sout_stream_id_sys_t * );
int  transcode_audio_process( sout_stream_t *, sout_stream_id_sys_t *,
                                     block_t *, block_t ** );
bool transcode_audio_add    ( sout_stream_t *, const es_format_t *,
                                sout_stream_id_sys_t *);

/* VIDEO */

void transcode_video_close  ( sout_stream_t *, sout_stream_id_sys_t * );
int  transcode_video_process( sout_stream_t *, sout_stream_id_sys_t *,
                                     block_t *, block_t ** );
int transcode_video_get_output_dimensions( sout_stream_t *, sout_stream_id_sys_t *,
                                           unsigned *w, unsigned *h );
void transcode_video_push_spu( sout_stream_t *, sout_stream_id_sys_t *, subpicture_t * );
bool transcode_video_add    ( sout_stream_t *, const es_format_t *,
                                sout_stream_id_sys_t *);
