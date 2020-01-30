#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include <vlc_picture.h>
#include <vlc_filter.h>
#include <vlc_opengl.h>
#include <vlc_vout_window.h>
#include <vlc_vout_display.h>
#include <vlc_atomic.h>
#include "../video_output/opengl/vout_helper.h"
#include <gbm.h>


/**
 *  offscreen filter
 *
 *  Create a gbm_egl context, and filter picture's through it
 */

static int create( vlc_object_t * );
static void destroy( vlc_object_t * );

static picture_t *filter_input( filter_t *, picture_t * );

struct gbm_filter_t
{
    vout_display_opengl_t   *vgl;
    vlc_gl_t                *gl;
    vout_window_t           *win;
    vlc_mutex_t             lock;
    vlc_cond_t              cond;
};

static int dummy(void)
{
    return 0;
}

static struct vout_window_callbacks win_cbs = {
    (void *)dummy, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
};

static vout_window_owner_t owner = {
    .cbs = &win_cbs,
    .sys = NULL,
};

static int create( vlc_object_t *obj )
{
    filter_t                    *filter = (filter_t *)obj;

    fprintf(stderr, "Real filter %4.4s -> %4.4s\n", (char *)&filter->fmt_in.i_codec,
                    (char *)&filter->fmt_out.i_codec);

    if (filter->fmt_in.video.i_chroma != VLC_CODEC_BGRA)
        return VLC_EGENERIC;

    struct gbm_filter_t         *sys;
    struct vout_display_cfg     displ_cfg = {
        .display = {
            .width = filter->fmt_in.video.i_visible_width ,
            .height = filter->fmt_in.video.i_visible_height,
        },
    };
    vout_window_cfg_t           wind_cfg = {
        .width = filter->fmt_in.video.i_visible_width ,
        .height = filter->fmt_in.video.i_visible_height,
    };

    filter->fmt_out.i_codec = VLC_CODEC_GBM;
    filter->fmt_out.video.i_chroma = VLC_CODEC_GBM;
    filter->fmt_out.video.i_visible_width = filter->fmt_in.video.i_visible_width;
    filter->fmt_out.video.i_visible_height = filter->fmt_in.video.i_visible_height;

    filter->p_sys = sys = malloc(sizeof(*sys));
    if (sys == NULL)
        return VLC_ENOMEM;

    vlc_mutex_init(&sys->lock);
    vlc_cond_init(&sys->cond);

    sys->gl = NULL;
    sys->vgl = NULL;
    displ_cfg.window = sys->win = vout_window_New(obj, "gbm", &owner);
    if (displ_cfg.window == NULL)
    {
        msg_Err(obj, "Failed to create gbm pseudo window\n");
        goto error;
    }

    vout_window_Enable(displ_cfg.window, &wind_cfg);

    sys->gl = vlc_gl_Create(&displ_cfg, VLC_OPENGL, "egl_gbm");
    if (sys->gl == NULL)
    {
        msg_Err(obj, "Failed to create gbm's opengl context\n");
        goto error;
    }
    const vlc_fourcc_t *spu_chromas;

    if (vlc_gl_MakeCurrent (sys->gl))
    {
        msg_Err(obj, "Failed to gl make current");
        goto error;
    }

    sys->vgl = vout_display_opengl_New(&filter->fmt_in.video, &spu_chromas, sys->gl,
                                        &displ_cfg.viewpoint, NULL);

    if (sys->vgl == NULL)
    {
        vlc_gl_ReleaseCurrent (sys->gl);
        msg_Err(obj, "Failed to have a vout new opengl display");
        goto error;
    }

    {
        /* Load the different opengl filter into the opengl renderer */
        char *filter_config = var_InheritString(obj, "offscreen-filters");
        if (filter_config != NULL && strcmp(filter_config, "none") != 0)
        {
            char *name = NULL;
            config_chain_t *chain = NULL;
            char *next_module = filter_config;

            while (next_module != NULL)
            {
                next_module = config_ChainCreate(&name, &chain, next_module);
                // TODO: chain == null ?
                //if (name != NULL)
                //    vout_display_opengl_AppendFilter(sys->vgl, name, chain);
                config_ChainDestroy(chain);
            }
        }
        free(filter_config);
    }

    vlc_gl_ReleaseCurrent(sys->gl);

    filter->pf_video_filter = filter_input;
    return VLC_SUCCESS;

error:
    vlc_mutex_destroy(&sys->lock);
    vlc_cond_destroy(&sys->cond);
    destroy(obj);
    return VLC_EGENERIC;
}

static void destroy( vlc_object_t *obj )
{
    filter_t        *filter = (filter_t *)obj;
    struct gbm_filter_t    *sys = filter->p_sys;

    if (sys != NULL)
    {
        if (sys->gl != NULL)
        {
            vlc_gl_MakeCurrent(sys->gl);
            vout_display_opengl_Delete(sys->vgl);
            vlc_gl_ReleaseCurrent(sys->gl);

            vout_window_Disable(sys->gl->surface);
            vlc_gl_Release(sys->gl);
            vout_window_Delete(sys->win);
        }
        free(sys);
    }
}

/**
 * The filter part
 */

struct gbm_context
{
    struct picture_context_t cbs;
    struct gbm_bo *bo;
    struct gbm_surface *surface;
    vlc_atomic_rc_t rc;
    vlc_mutex_t *lock;
    vlc_cond_t *cond;
};

static picture_context_t    *picture_context_copy(picture_context_t *input)
{
    struct gbm_context *context = (struct gbm_context *)input;

    vlc_atomic_rc_inc(&context->rc);
    return input;
}

static void picture_context_destroy(picture_context_t *input)
{
    struct gbm_context *context = (struct gbm_context *)input;

    if (vlc_atomic_rc_dec(&context->rc))
    {
        vlc_mutex_lock(context->lock);
        if (context->bo != NULL)
            gbm_surface_release_buffer(context->surface, context->bo);
        vlc_cond_signal(context->cond);
        vlc_mutex_unlock(context->lock);
        free(context);
    }
}

static picture_t *filter_input( filter_t *filter, picture_t *input )
{
    struct gbm_filter_t *sys = filter->p_sys;
    struct gbm_context *context = malloc(sizeof(*context));
    picture_resource_t pict_resource = {
        .p_sys = input->p_sys,
    };

    for (int i = 0; i < PICTURE_PLANE_MAX; i++)
    {
        pict_resource.p[i].p_pixels = input->p[i].p_pixels;
        pict_resource.p[i].i_lines = input->p[i].i_lines;
        pict_resource.p[i].i_pitch = input->p[i].i_pitch;
    }

    if (context == NULL)
        return NULL;

    picture_t *output = picture_NewFromResource(&filter->fmt_out.video, &pict_resource);

    if (output == NULL)
    {
        free(context);
        return NULL;
    }

    picture_CopyProperties( output, input );

    context->lock = &sys->lock;
    context->cond = &sys->cond;
    context->surface = sys->gl->surface->handle.gbm;
    context->cbs.destroy = picture_context_destroy;
    context->cbs.copy = picture_context_copy;
    vlc_atomic_rc_init(&context->rc);
    output->context = (picture_context_t *)context;
    output->context->vctx = NULL;

    vlc_mutex_lock(&sys->lock);
    while(gbm_surface_has_free_buffers(context->surface) == 0)
        vlc_cond_wait(&sys->cond, &sys->lock);
    vlc_mutex_unlock(&sys->lock);

    if (vlc_gl_MakeCurrent(sys->gl) == VLC_SUCCESS)
    {
        vout_display_opengl_Prepare(sys->vgl, input, NULL);
        vout_display_opengl_Display(sys->vgl, &input->format);
        vlc_gl_ReleaseCurrent(sys->gl);
    }

    context->bo = gbm_surface_lock_front_buffer(context->surface);
    if (context->bo == NULL)
    {
        picture_Release(output);
        output = NULL;
    }
    // free(input);
    picture_Release(input);
    return output;
}

/**
 *  cpu_offscreen filter
 *
 *  Takes a GBM picture and converts it to a CPU picture
 */
static int cpu_create( vlc_object_t * );

static picture_t *filter_output( filter_t *, picture_t * );

static int cpu_create( vlc_object_t *obj )
{
    filter_t *filter = (filter_t *)obj;

    fprintf(stderr, "CPU filter %4.4s -> %4.4s\n", (char *)&filter->fmt_in.i_codec,
                    (char *)&filter->fmt_out.i_codec);

    if (filter->fmt_in.i_codec != VLC_CODEC_GBM)
        return VLC_EGENERIC;

    if (filter->fmt_out.i_codec != VLC_CODEC_RGB32)
        return VLC_EGENERIC;

    filter->fmt_out.i_codec
        = filter->fmt_out.video.i_chroma
        = VLC_CODEC_RGBA;

    filter->fmt_out.video.i_visible_width
        = filter->fmt_out.video.i_width
        = filter->fmt_in.video.i_visible_width;

    filter->fmt_out.video.i_visible_height
        = filter->fmt_out.video.i_height
        = filter->fmt_in.video.i_visible_height;

    filter->pf_video_filter = filter_output;

    return VLC_SUCCESS;
}

static picture_t *filter_output( filter_t *filter, picture_t *input )
{
    struct gbm_context *context = (struct gbm_context *)input->context;
    picture_t   *output = NULL;

    if (context->bo != NULL)
    {
        uint32_t width = gbm_bo_get_width(context->bo);
        uint32_t height = gbm_bo_get_height(context->bo);
        uint32_t stride = gbm_bo_get_stride(context->bo);
        uint32_t offset;

        void *buffer = NULL;
        void *base = gbm_bo_map(context->bo, 0, 0, width, height,
                                GBM_BO_TRANSFER_READ, &offset, &buffer);
        if (base != NULL)
        {
            output = filter_NewPicture(filter);
            if (output != NULL)
            {
                assert(gbm_bo_get_format(context->bo) == GBM_FORMAT_XRGB8888);
                msg_Info(filter, "stride = %u", stride);
                msg_Info(filter, "offset = %u", offset);
                msg_Info(filter, "ASSERT: %u = %u",
                         output->format.i_visible_width, width);
                msg_Info(filter, "ASSERT: %u = %u",
                         output->format.i_visible_height, height);
                msg_Info(filter, "plane count = %u", gbm_bo_get_plane_count(context->bo));
                msg_Info(filter, "bit pp = %u", gbm_bo_get_bpp(context->bo));
                assert(output->i_planes == 1);
                assert(output->format.i_visible_width == width);
                assert(output->format.i_visible_height == height);

                for(size_t line=0; line < height; ++line)
                {
                    memcpy((char*)output->p[0].p_pixels + line*width, (const char*)base + offset * line, width);
                }

                picture_CopyProperties( output, input );
            }
            gbm_bo_unmap(context->bo, base);

        }
        else
            msg_Err(filter, "gbm_bo_map failed");
    }
    picture_Release(input);
    return output;
}

vlc_module_begin()

    set_shortname( N_("offscreen") )
    set_description( N_("Offscreen GBM filter") )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )
    set_capability( "video filter", 100 )

    add_shortcut( "offscreen", "fgbm" )
    add_module_list("offscreen-filters", "offscreen Opengl filters", "none",
                    "Offscreen GL filters", "Offscreen GL filters")

    set_callbacks( create, destroy )


    add_submodule()

    set_shortname( N_("cpu_offscreen") )
    set_description( N_("Put an offscreen surface in CPU") )
    set_capability( "video converter", 100 )
    add_shortcut( "coffscreen", "cpu-offscreen", "conv-offscreen" )
    set_callback( cpu_create )

vlc_module_end()
