/*****************************************************************************
 * glfb.c: run OpenGL in offscreen framebuffer
 *****************************************************************************
 * Copyright (C) 2020 Videolabs
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
#include "../video_output/opengl/gl_common.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>

#define BUFFER_COUNT 3

struct pbo_picture_context
{
    struct picture_context_t context;
    void *buffer_mapping;
    int rc;
    vlc_mutex_t *lock;
    vlc_cond_t *cond;
};

struct vlc_gl_pbo_filter
{
    vout_display_opengl_t   *vgl;
    vlc_gl_t                *gl;
    vout_window_t           *win;
    vlc_mutex_t             lock;
    vlc_cond_t              cond;

    size_t                  current_flip;
    GLuint                  pixelbuffers[BUFFER_COUNT];
    GLuint                  framebuffers[BUFFER_COUNT];
    GLuint                  textures[BUFFER_COUNT];
    struct pbo_picture_context     picture_contexts[BUFFER_COUNT];

    opengl_vtable_t         glapi;

    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;

    PFNEGLCREATEIMAGEKHRPROC    eglCreateImageKHR;
    PFNEGLDESTROYIMAGEKHRPROC   eglDestroyImageKHR;
};

static int MakeCurrent (vlc_gl_t *gl)
{
    struct vlc_gl_pbo_filter *sys = gl->sys;

    if (eglMakeCurrent (sys->display, sys->surface, sys->surface,
                        sys->context) != EGL_TRUE)
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

static void ReleaseCurrent (vlc_gl_t *gl)
{
    struct vlc_gl_pbo_filter *sys = gl->sys;

    eglMakeCurrent (sys->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                    EGL_NO_CONTEXT);
}

static void SwapBuffers (vlc_gl_t *gl)
{
    struct vlc_gl_pbo_filter *sys = gl->sys;

    eglSwapBuffers (sys->display, sys->surface);
}

static void *GetSymbol(vlc_gl_t *gl, const char *procname)
{
    (void) gl;
    return (void *)eglGetProcAddress (procname);
}

static const char *QueryString(vlc_gl_t *gl, int32_t name)
{
    struct vlc_gl_pbo_filter *sys = gl->sys;

    return eglQueryString(sys->display, name);
}

static void *CreateImageKHR(vlc_gl_t *gl, unsigned target, void *buffer,
                            const int32_t *attrib_list)
{
    struct vlc_gl_pbo_filter *sys = gl->sys;

    return sys->eglCreateImageKHR(sys->display, NULL, target, buffer,
                                  attrib_list);
}

static bool DestroyImageKHR(vlc_gl_t *gl, void *image)
{
    struct vlc_gl_pbo_filter *sys = gl->sys;

    return sys->eglDestroyImageKHR(sys->display, image);
}

static vlc_gl_t *CreateGL(filter_t *filter)
{
    struct vlc_gl_pbo_filter *sys = filter->p_sys;

    sys->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (sys->display == EGL_NO_DISPLAY)
        return NULL;

    vlc_gl_t *gl = vlc_object_create(filter, sizeof *gl);
    if (gl == NULL)
        return NULL;
    gl->sys = sys;

    unsigned width = filter->fmt_out.video.i_visible_width;
    unsigned height = filter->fmt_out.video.i_visible_height;

    /* Initialize EGL display */
    EGLint major, minor;
    if (eglInitialize(sys->display, &major, &minor) != EGL_TRUE)
        goto error;
    msg_Dbg(filter, "EGL version %s by %s",
            eglQueryString(sys->display, EGL_VERSION),
            eglQueryString(sys->display, EGL_VENDOR));

    const EGLint conf_attr[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE,
    };
    EGLConfig cfgv[1];
    EGLint cfgc;

    msg_Info(filter, "WIDTH=%u HEIGHT=%u", width, height);
    const EGLint surface_attr[] = {
        EGL_WIDTH, width,
        EGL_HEIGHT, height,
        EGL_NONE,
    };

    if (eglChooseConfig(sys->display, conf_attr, cfgv, 1, &cfgc) != EGL_TRUE
     || cfgc == 0)
    {
        msg_Err (filter, "cannot choose EGL configuration");
        goto error;
    }

    /* Create a drawing surface */
    sys->surface = eglCreatePbufferSurface(sys->display, cfgv[0], surface_attr);
    if (sys->surface == EGL_NO_SURFACE)
    {
        msg_Err (filter, "cannot create EGL window surface");
        assert(false);
        goto error;
    }

    if (eglBindAPI (EGL_OPENGL_ES_API) != EGL_TRUE)
    {
        msg_Err (filter, "cannot bind EGL API");
        goto error;
    }

    const GLint ctx_attr[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    EGLContext ctx
        = sys->context
        = eglCreateContext(sys->display, cfgv[0], EGL_NO_CONTEXT, ctx_attr);

    if (ctx == EGL_NO_CONTEXT)
    {
        msg_Err (filter, "cannot create EGL context");
        goto error;
    }
    /* Initialize OpenGL callbacks */
    gl->ext = VLC_GL_EXT_EGL;
    gl->makeCurrent = MakeCurrent;
    gl->releaseCurrent = ReleaseCurrent;
    gl->resize = NULL;
    gl->swap = SwapBuffers;
    gl->getProcAddress = GetSymbol;
    gl->destroy = NULL;
    gl->egl.queryString = QueryString;

    sys->eglCreateImageKHR = (void *)eglGetProcAddress("eglCreateImageKHR");
    sys->eglDestroyImageKHR = (void *)eglGetProcAddress("eglDestroyImageKHR");
    if (sys->eglCreateImageKHR != NULL && sys->eglDestroyImageKHR != NULL)
    {
        gl->egl.createImageKHR = CreateImageKHR;
        gl->egl.destroyImageKHR = DestroyImageKHR;
    }

    return gl;
error:
    free(gl);
    return NULL;
}

static picture_context_t *picture_context_copy(picture_context_t *input)
{
    struct pbo_picture_context *context =
        (struct pbo_picture_context *)input;

    vlc_mutex_lock(context->lock);
    context->rc++;
    vlc_mutex_unlock(context->lock);
    return input;
}

static void picture_context_destroy(picture_context_t *input)
{
    struct pbo_picture_context *context =
        (struct pbo_picture_context *)input;

    vlc_mutex_lock(context->lock);
    context->rc--;
    vlc_cond_signal(context->cond);
    vlc_mutex_unlock(context->lock);
}

static picture_t *Filter(filter_t *filter, picture_t *input)
{
    struct vlc_gl_pbo_filter *sys = filter->p_sys;

    vlc_mutex_lock(&sys->lock);
    size_t index;

    do {
        for (index=0; index<BUFFER_COUNT; ++index)
        {
            assert(sys->picture_contexts[index].rc >= 0);
            if (sys->picture_contexts[index].rc == 0)
                goto out_loop;
        }
        vlc_cond_wait(&sys->cond, &sys->lock);
    } while(index == BUFFER_COUNT);
out_loop:
    vlc_mutex_unlock(&sys->lock);

     struct pbo_picture_context *context = &sys->picture_contexts[index];
     sys->current_flip = index;

    if (vlc_gl_MakeCurrent(sys->gl) != VLC_SUCCESS)
        return NULL;

    vout_display_opengl_Prepare(sys->vgl, input, NULL);

    sys->glapi.BindBuffer(GL_PIXEL_PACK_BUFFER, sys->pixelbuffers[sys->current_flip]);
    sys->glapi.BindFramebuffer(GL_FRAMEBUFFER, sys->framebuffers[sys->current_flip]);
    if (context->buffer_mapping != NULL)
        sys->glapi.UnmapBuffer(GL_PIXEL_PACK_BUFFER);

    vout_display_opengl_Display(sys->vgl, &input->format);

    GLsizei width = filter->fmt_out.video.i_visible_width;
    GLsizei height = filter->fmt_out.video.i_visible_height;
    GLenum format = GL_RGBA;

    sys->glapi.ReadPixels(0, 0, width, height, format,
                          GL_UNSIGNED_BYTE, 0);

    void *pixels = sys->glapi.MapBufferRange(
            GL_PIXEL_PACK_BUFFER, 0, width*height*4, GL_MAP_READ_BIT);

    GLsizei stride;
    sys->glapi.GetIntegerv(GL_PACK_ROW_LENGTH, &stride);
    stride = width;

    sys->glapi.BindFramebuffer(GL_FRAMEBUFFER, 0);
    sys->glapi.BindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    vlc_gl_ReleaseCurrent(sys->gl);

    sys->current_flip = (sys->current_flip + 1) % BUFFER_COUNT;

    picture_resource_t pict_resource = {
        .p_sys = input->p_sys,
        .pf_destroy = NULL,
    };

    pict_resource.p[0].p_pixels = pixels;
    pict_resource.p[0].i_lines = height;
    pict_resource.p[0].i_pitch = stride * 4;

    picture_t *output = picture_NewFromResource(&filter->fmt_out.video, &pict_resource);
    if (output == NULL)
    {
        free(context);
        return NULL;
    }

    picture_CopyProperties(output, input);

    context->buffer_mapping = pixels;
    context->rc ++;
    output->context = (picture_context_t *)context;
    output->context->vctx = NULL;
    output->format.orientation = ORIENT_VFLIPPED;

    picture_Release(input);
    return output;
}
static int Open( vlc_object_t *obj )
{
    filter_t *filter = (filter_t *)obj;
    struct vlc_gl_pbo_filter *sys = NULL;

    if (filter->fmt_out.video.i_chroma != VLC_CODEC_BGRA)
        return VLC_EGENERIC;

    filter->p_sys
        = sys
        = malloc(sizeof *sys);
    if (sys == NULL)
        return VLC_ENOMEM;

    filter->fmt_out.video.i_chroma
        = filter->fmt_out.i_codec
        = VLC_CODEC_BGRA;

    unsigned width
        = filter->fmt_out.video.i_visible_width
        = filter->fmt_in.video.i_visible_width;

    unsigned height
        = filter->fmt_out.video.i_visible_height
        = filter->fmt_in.video.i_visible_height;

    vlc_mutex_init(&sys->lock);
    vlc_cond_init(&sys->cond);

    sys->gl = NULL;
    sys->vgl = NULL;
    sys->current_flip = 0;

    sys->gl = CreateGL(filter);
    if (sys->gl == NULL)
    {
        msg_Err(obj, "Failed to create opengl context\n");
        goto error;
    }
    const vlc_fourcc_t *spu_chromas;

    if (vlc_gl_MakeCurrent (sys->gl))
    {
        msg_Err(obj, "Failed to gl make current");
        goto error;
    }

    vlc_viewpoint_t viewpoint;
    vlc_viewpoint_init(&viewpoint);
    sys->vgl = vout_display_opengl_New(&filter->fmt_in.video, &spu_chromas, sys->gl,
                                        &viewpoint, NULL);

    if (sys->vgl == NULL)
    {
        vlc_gl_ReleaseCurrent (sys->gl);
        msg_Err(obj, "Failed to have a vout new opengl display");
        goto error;
    }


#if defined(USE_OPENGL_ES2) || defined(HAVE_GL_CORE_SYMBOLS)
#define GET_PROC_ADDR_CORE(name) sys->glapi.name = gl##name
#else
#define GET_PROC_ADDR_CORE(name) GET_PROC_ADDR_EXT(name, true)
#endif
#define GET_PROC_ADDR_EXT(name, critical) do { \
    sys->glapi.name = vlc_gl_GetProcAddress(sys->gl, "gl"#name); \
    if (sys->glapi.name == NULL && critical) { \
        msg_Err(filter, "gl"#name" symbol not found, bailing out"); \
        goto error; \
    } \
} while(0)
#if defined(USE_OPENGL_ES2)
#define GET_PROC_ADDR(name) GET_PROC_ADDR_CORE(name)
#define GET_PROC_ADDR_CORE_GL(name) GET_PROC_ADDR_EXT(name, false) /* optional for GLES */
#else
#define GET_PROC_ADDR(name) GET_PROC_ADDR_EXT(name, true)
#define GET_PROC_ADDR_CORE_GL(name) GET_PROC_ADDR_CORE(name)
#endif
#define GET_PROC_ADDR_OPTIONAL(name) GET_PROC_ADDR_EXT(name, false) /* GL 3 or more */

    GET_PROC_ADDR_CORE(BindTexture);
    GET_PROC_ADDR_CORE(BlendFunc);
    GET_PROC_ADDR_CORE(Clear);
    GET_PROC_ADDR_CORE(ClearColor);
    GET_PROC_ADDR_CORE(DeleteTextures);
    GET_PROC_ADDR_CORE(DepthMask);
    GET_PROC_ADDR_CORE(Disable);
    GET_PROC_ADDR_CORE(DrawArrays);
    GET_PROC_ADDR_CORE(DrawElements);
    GET_PROC_ADDR_CORE(Enable);
    GET_PROC_ADDR_CORE(Finish);
    GET_PROC_ADDR_CORE(Flush);
    GET_PROC_ADDR_CORE(GenTextures);
    GET_PROC_ADDR_CORE(GetError);
    GET_PROC_ADDR_CORE(GetIntegerv);
    GET_PROC_ADDR_CORE(GetString);
    GET_PROC_ADDR_CORE(PixelStorei);
    GET_PROC_ADDR_CORE(TexImage2D);
    GET_PROC_ADDR_CORE(TexParameterf);
    GET_PROC_ADDR_CORE(TexParameteri);
    GET_PROC_ADDR_CORE(TexSubImage2D);
    GET_PROC_ADDR_CORE(Viewport);

    GET_PROC_ADDR_CORE_GL(GetTexLevelParameteriv);
    GET_PROC_ADDR_CORE_GL(TexEnvf);

    GET_PROC_ADDR(GenBuffers);
    GET_PROC_ADDR(BindBuffer);
    GET_PROC_ADDR(BufferData);
    GET_PROC_ADDR(DeleteBuffers);
    GET_PROC_ADDR(ReadPixels);

    GET_PROC_ADDR_OPTIONAL(GenFramebuffers);
    GET_PROC_ADDR_OPTIONAL(DeleteFramebuffers);
    GET_PROC_ADDR_OPTIONAL(FramebufferTexture2D);
    GET_PROC_ADDR_OPTIONAL(BindFramebuffer);

    GET_PROC_ADDR_OPTIONAL(BufferSubData);
    GET_PROC_ADDR_OPTIONAL(BufferStorage);
    GET_PROC_ADDR_OPTIONAL(MapBuffer);
    GET_PROC_ADDR_OPTIONAL(MapBufferRange);
    GET_PROC_ADDR_OPTIONAL(FlushMappedBufferRange);
    GET_PROC_ADDR_OPTIONAL(UnmapBuffer);
    GET_PROC_ADDR_OPTIONAL(FenceSync);
    GET_PROC_ADDR_OPTIONAL(DeleteSync);
    GET_PROC_ADDR_OPTIONAL(ClientWaitSync);
#undef GET_PROC_ADDR

    sys->glapi.GenBuffers(BUFFER_COUNT, sys->pixelbuffers);
    sys->glapi.GenFramebuffers(BUFFER_COUNT, sys->framebuffers);
    sys->glapi.GenTextures(BUFFER_COUNT, sys->textures);

    for (size_t i=0; i<BUFFER_COUNT; ++i)
    {
        sys->glapi.BindBuffer(GL_PIXEL_PACK_BUFFER, sys->pixelbuffers[i]);
        sys->glapi.BufferData(GL_PIXEL_PACK_BUFFER, width*height*4, NULL, GL_STREAM_READ);
        sys->glapi.BindFramebuffer(GL_FRAMEBUFFER, sys->framebuffers[i]);
        sys->glapi.BindTexture(GL_TEXTURE_2D, sys->textures[i]);
        sys->glapi.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                              GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        sys->glapi.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                        GL_TEXTURE_2D, sys->textures[i], 0);

        struct pbo_picture_context *context = &sys->picture_contexts[i];
        context->buffer_mapping = NULL;
        context->lock = &sys->lock;
        context->cond = &sys->cond;
        context->context.destroy = picture_context_destroy;
        context->context.copy = picture_context_copy;
        context->rc = 0;
    }
    sys->glapi.BindFramebuffer(GL_FRAMEBUFFER, 0);
    sys->glapi.BindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    vlc_gl_ReleaseCurrent(sys->gl);

    filter->fmt_out.video.orientation = ORIENT_VFLIPPED;
    filter->pf_video_filter = Filter;
    return VLC_SUCCESS;

error:
    vlc_mutex_destroy(&sys->lock);
    vlc_cond_destroy(&sys->cond);
    return VLC_EGENERIC;
}

static void Close( vlc_object_t *obj )
{
    filter_t *filter = (filter_t *)obj;
    struct vlc_gl_pbo_filter *sys = filter->p_sys;

    if (sys != NULL)
    {
        vlc_gl_MakeCurrent(sys->gl);
        vout_display_opengl_Delete(sys->vgl);
        sys->glapi.DeleteBuffers(BUFFER_COUNT, sys->pixelbuffers);
        sys->glapi.DeleteFramebuffers(BUFFER_COUNT, sys->framebuffers);
        sys->glapi.DeleteTextures(BUFFER_COUNT, sys->textures);
        vlc_gl_ReleaseCurrent(sys->gl);

        vlc_object_delete(sys->gl);
        free(sys);
    }
}

vlc_module_begin()
    set_shortname( N_("gl_pbo") )
    set_description( N_("PBO opengl filter executor") )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )
    set_capability( "video filter", 100 )
    add_shortcut( "gl_pbo" )
    set_callbacks( Open, Close )
vlc_module_end()
