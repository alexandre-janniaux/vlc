/*****************************************************************************
 * converter_gbm.c: OpenGL GBM opaque converter
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

#include <assert.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <gbm.h>

#include "converter.h"

struct opengl_tex_converter_sys_t
{
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
};

/* TODO: use glEGLImageTargetTexture2DOES ? */
static int AllocateTextures(const opengl_tex_converter_t *tc, GLuint *textures,
                            const GLsizei *tex_width, const GLsizei *tex_height)
{
    for (unsigned i = 0; i < tc->tex_count; i++)
    {
        tc->vt->BindTexture(tc->tex_target, textures[i]);
        tc->vt->TexImage2D(tc->tex_target, 0, tc->texs[i].internal,
                           tex_width[i], tex_height[i], 0, tc->texs[i].format,
                           tc->texs[i].type, NULL);
    }
    return VLC_SUCCESS;
}


static int Update(const opengl_tex_converter_t *tc, GLuint *textures,
                  const GLsizei *tex_width, const GLsizei *tex_height,
                  picture_t *pic, const size_t *plane_offset)
{
    struct opengl_tex_converter_sys_t *sys = tc->priv;

    /* We can't work with non GBM frames. */
    assert(pic->format.i_chroma == VLC_CODEC_GBM_BUFFER);

    /* TODO: extract buffer from picture data */
    struct gbm_bo *buffer = NULL;

    uint32_t format = gbm_bo_get_format(buffer);
    if (format != GBM_FORMAT_XRGB888)
        return VLC_EGENERIC;

    *tex_width = gbm_bo_get_width(buffer);
    *tex_height = gbm_bo_get_height(buffer);

    /* TODO: should we do size checks ? */

    /* TODO: should be written according to plane definition, currently only
     * support GBM_FORMAT_XRGB8888. */
    EGLint attribs[] = {
        EGL_WIDTH, *tex_width,
        EGL_HEIGHT, *tex_height,
        EGL_LINUX_DRM_FOURCC_EXT, format,
        EGL_DMA_BUF_PLANE0_FD_EXT, gbm_bo_get_fd(buffer),
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, gbm_bo_get_offset(buffer, 0),
        EGL_DMA_BUF_PLANE0_PICTH_EXT, gbm_bo_get_stride_for_plane(buffer, 0),
        EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, /* Don't destroy input buffer */
        EGL_NONE
    };

    /* Import the gbm buffer into the EGL API. */
    EGLImageKHR egl_image = tc->gl->egl.createImageKHR(
            tc, EGL_LINUX_DMA_BUF_EXT, NULL, attribs);

    if (egl_image == EGL_NO_IMAGE_KHR)
    {
        msg_Err(tc, "cannot import GBM buffer into EGL Image");
        return VLC_EGENERIC;
    }

    /* Import and reference the buffer into opengl. */
    tc->vt->BindTexture(tc->tex_target, textures[i]);
    sys->glEGLImageTargetTexture2DOES(tc->tex_target, egl_image);


    /* The image will subsist as long as it is referenced in any of the client
     * API or in the EGL API, so it can be removed as long as we produced
     * textures from it.
     * https://www.khronos.org/registry/EGL/extensions/KHR/EGL_KHR_image_base.txt */
    tc->gl->egl.destroyImageKHR(tc, egl_image);
}

static void Destroy(picture_t *pic)
{

}

static void Close(vlc_object_t *obj)
{

}

static int Open(vlc_object_t *obj)
{
    /* This module use EGL to import GBM buffer into EGL Images. */
    if (tc->gl->ext != VLC_GL_EXT_EGL
     || tc->gl->egl.createImageKHR == NULL
     || tc->gl->egl.destroyImageKHR == NULL)
        return VLC_EGENERIC;

    if (!vlc_gl_StrHasToken(tc->glexts, "GL_OES_EGL_image"))
        return VLC_EGENERIC;

    const char *eglexts = tc->gl->egl.queryString(tc->gl, EGL_EXTENSIONS);
    if (eglexts == NULL || !vlc_gl_StrHasToken(eglexts, "EGL_EXT_image_dma_buf_import"))
        return VLC_EGENERIC;

    struct opengl_tex_converter_sys_t *priv = tc->priv =
        calloc(1, sizeof(*priv));

    if (unlikely(tc->priv == NULL))
        goto error;

    priv->glEGLImageTargetTexture2DOES =
        vlc_gl_GetProcAddress(tc->gl, "glEGLImageTargetTexture2DOES");
    if (priv->glEGLImageTargetTexture2DOES == NULL)
        goto error;

    tc->fshader = opengl_fragment_shader_init(tc, GL_TEXTURE_2D, vlc_sw_chroma,
                                              tc->fmt.space);
    if (tc->fshader == 0)
        goto error;

    tc->pf_update  = tc_vaegl_update;
    tc->pf_get_pool = tc_vaegl_get_pool;

    return VLC_SUCCESS;

error:
    return VLC_EGENERIC;
}

vlc_module_begin()
    set_description("GBM buffer Opengl converter")
    set_capability("glconv", 1)
    set_callbacks(Open, Close)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    add_shortcut("gbm_bo")
vlc_module_end()
