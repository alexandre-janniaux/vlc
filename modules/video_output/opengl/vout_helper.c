/*****************************************************************************
 * vout_helper.c: OpenGL and OpenGL ES output common code
 *****************************************************************************
 * Copyright (C) 2004-2016 VLC authors and VideoLAN
 * Copyright (C) 2009, 2011 Laurent Aimar
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *          Ilkka Ollakka <ileoo@videolan.org>
 *          Rémi Denis-Courmont
 *          Adrien Maglo <magsoft at videolan dot org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          Pierre d'Herbemont <pdherbemont at videolan dot org>
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
# include "config.h"
#endif

#include <assert.h>
#include <math.h>

#include <vlc_common.h>
#include <vlc_picture_pool.h>
#include <vlc_subpicture.h>
#include <vlc_opengl.h>
#include <vlc_modules.h>
#include <vlc_vout.h>
#include <vlc_viewpoint.h>
#include <vlc_hmd_controller.h>
#include <vlc_vout_hmd.h>

#include "vout_helper.h"
#include "internal.h"
#include "objects.h"

#ifndef GL_CLAMP_TO_EDGE
# define GL_CLAMP_TO_EDGE 0x812F
#endif

#define SPHERE_RADIUS 1.f
#define SCENE_MAX_LIGHT 20

/* FIXME: GL_ASSERT_NOERROR disabled for now because:
 * Proper GL error handling need to be implemented
 * glClear(GL_COLOR_BUFFER_BIT) throws a GL_INVALID_FRAMEBUFFER_OPERATION on macOS
 * assert fails on vout_display_opengl_Delete on iOS
 */
#if 0
# define HAVE_GL_ASSERT_NOERROR
#endif

#ifdef HAVE_GL_ASSERT_NOERROR
# define GL_ASSERT_NOERROR() do { \
    GLenum glError = vgl->vt.GetError(); \
    switch (glError) \
    { \
        case GL_NO_ERROR: break; \
        case GL_INVALID_ENUM: assert(!"GL_INVALID_ENUM"); \
        case GL_INVALID_VALUE: assert(!"GL_INVALID_VALUE"); \
        case GL_INVALID_OPERATION: assert(!"GL_INVALID_OPERATION"); \
        case GL_INVALID_FRAMEBUFFER_OPERATION: assert(!"GL_INVALID_FRAMEBUFFER_OPERATION"); \
        case GL_OUT_OF_MEMORY: assert(!"GL_OUT_OF_MEMORY"); \
        default: assert(!"GL_UNKNOWN_ERROR"); \
    } \
} while(0)
#else
# define GL_ASSERT_NOERROR()
#endif

typedef struct {
    GLuint   texture;
    GLsizei  width;
    GLsizei  height;

    float    alpha;

    float    top;
    float    left;
    float    bottom;
    float    right;

    float    tex_width;
    float    tex_height;
} gl_region_t;

struct prgm
{
    GLuint id;
    opengl_tex_converter_t *tc;

    int light_count;
    bool b_has_light;

    struct {
        GLfloat OrientationMatrix[16];
        GLfloat ProjectionMatrix[16];
        GLfloat ModelViewMatrix[16];
        GLfloat ZRotMatrix[16];
        GLfloat YRotMatrix[16];
        GLfloat XRotMatrix[16];
        GLfloat ZoomMatrix[16];
        GLfloat ObjectTransformMatrix[16];
        GLfloat SceneTransformMatrix[16];
        GLfloat HeadPositionMatrix[16];
        GLfloat SbSCoefs[2];
        GLfloat SbSOffsets[2];
    } var;

    struct { /* UniformLocation */
        GLint OrientationMatrix;
        GLint ProjectionMatrix;
        GLint ModelViewMatrix;
        GLint ZRotMatrix;
        GLint YRotMatrix;
        GLint XRotMatrix;
        GLint ZoomMatrix;
        GLint ObjectTransformMatrix;
        GLint SceneTransformMatrix;
        GLint HeadPositionMatrix;
        GLint SbSCoefs;
        GLint SbSOffsets;

        GLint LightCount;
        GLint HasLight;
        struct {
            GLint Position;
            GLint Ambient, Diffuse, Specular;
            GLint Kc, Kl, Kq;
            GLint Direction;
            GLint Cutoff;
        } lights;

        GLint MatAmbientTex;
        GLint MatDiffuseTex;
        GLint MatNormalTex;
        GLint MatSpecularTex;

        GLint MatAmbient;
        GLint MatDiffuse;
        GLint MatSpecular;
        GLint SceneAmbient;

        GLint UseAmbiantTexture;
        GLint UseDiffuseTexture;
        GLint UseSpecularTexture;

        GLint IsUniformColor;
    } uloc;
    struct { /* AttribLocation */
        GLint MultiTexCoord[3];
        GLint VertexPosition;
        GLint VertexNormal;
        GLint VertexTangent;
        GLint ObjectTransformMatrix;
    } aloc;
};

struct vout_display_opengl_t {

    vlc_gl_t   *gl;
    opengl_vtable_t vt;

    video_format_t fmt;

    GLsizei    tex_width[PICTURE_PLANE_MAX];
    GLsizei    tex_height[PICTURE_PLANE_MAX];

    GLuint     texture[PICTURE_PLANE_MAX];

    int         region_count;
    gl_region_t *region;


    picture_pool_t *pool;

    /* One YUV program and one RGBA program (for subpics) */
    struct prgm prgms[5];
    struct prgm *prgm; /* Main program */
    struct prgm *sub_prgm; /* Subpicture program */
    struct prgm *ctl_prgm; /* HMD controller program */
    struct prgm *stereo_prgm; /* Stereo program */
    struct prgm *scene_prgm; /* 3D scene program */

    unsigned nb_indices;
    GLuint vertex_buffer_object;
    GLuint index_buffer_object;
    GLuint texture_buffer_object[PICTURE_PLANE_MAX];

    GLuint *subpicture_buffer_object;
    int    subpicture_buffer_object_count;

    GLuint hmd_controller_buffer_object[2];
    GLuint hmd_controller_texture;
    GLsizei hmd_controller_width, hmd_controller_height;
    bool b_show_hmd_controller;
    hmd_controller_pos_t hmdCtlPos;

    struct {
        unsigned int i_x_offset;
        unsigned int i_y_offset;
        unsigned int i_visible_width;
        unsigned int i_visible_height;
    } last_source;

    /* Non-power-of-2 texture size support */
    bool supports_npot;

    /* View point */
    float f_teta;
    float f_phi;
    float f_roll;
    float f_fovx; /* f_fovx and f_fovy are linked but we keep both */
    float f_fovy; /* to avoid recalculating them when needed.      */
    float f_z;    /* Position of the camera on the shpere radius vector */
    float f_sar;

    /* Side by side */
    bool b_sideBySide;
    bool b_lastSideBySide;

    unsigned i_displayX;
    unsigned i_displayY;
    unsigned i_displayWidth;
    unsigned i_displayHeight;
    vout_display_place_t displayPlace;

    /* FBO */
    GLuint leftColorTex, leftDepthTex, leftFBO;
    GLuint rightColorTex, rightDepthTex, rightFBO;

    GLuint vertex_buffer_object_stereo;
    GLuint index_buffer_object_stereo;
    GLuint texture_buffer_object_stereo;

    /* 3D objects */
    gl_scene_objects_display_t *p_objDisplay;

    vout_hmd_cfg_t hmd_cfg;
};

static const vlc_fourcc_t gl_subpicture_chromas[] = {
    VLC_CODEC_RGBA,
    0
};

typedef enum
{
    UNDEFINED_EYE,
    LEFT_EYE,
    RIGHT_EYE
} side_by_side_eye;

static const GLfloat identity[] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};

/* rotation around the Z axis */
static void getZRotMatrix(float theta, GLfloat matrix[static 16])
{
    float st, ct;

    sincosf(theta, &st, &ct);

    const GLfloat m[] = {
    /*  x    y    z    w */
        ct,  -st, 0.f, 0.f,
        st,  ct,  0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f
    };

    memcpy(matrix, m, sizeof(m));
}

/* rotation around the Y axis */
static void getYRotMatrix(float theta, GLfloat matrix[static 16])
{
    float st, ct;

    sincosf(theta, &st, &ct);

    const GLfloat m[] = {
    /*  x    y    z    w */
        ct,  0.f, -st, 0.f,
        0.f, 1.f, 0.f, 0.f,
        st,  0.f, ct,  0.f,
        0.f, 0.f, 0.f, 1.f
    };

    memcpy(matrix, m, sizeof(m));
}

/* rotation around the X axis */
static void getXRotMatrix(float phi, GLfloat matrix[static 16])
{
    float sp, cp;

    sincosf(phi, &sp, &cp);

    const GLfloat m[] = {
    /*  x    y    z    w */
        1.f, 0.f, 0.f, 0.f,
        0.f, cp,  sp,  0.f,
        0.f, -sp, cp,  0.f,
        0.f, 0.f, 0.f, 1.f
    };

    memcpy(matrix, m, sizeof(m));
}

static void getZoomMatrix(float zoom, GLfloat matrix[static 16]) {

    const GLfloat m[] = {
        /* x   y     z     w */
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, zoom, 1.0f
    };

    memcpy(matrix, m, sizeof(m));
}

/* perspective matrix see https://www.opengl.org/sdk/docs/man2/xhtml/gluPerspective.xml */
static void getProjectionMatrix(float sar, float fovy, GLfloat matrix[static 16]) {

    float zFar  = 1000;
    float zNear = 0.01;

    float f = 1.f / tanf(fovy / 2.f);

    const GLfloat m[] = {
        f / sar, 0.f,                   0.f,                0.f,
        0.f,     f,                     0.f,                0.f,
        0.f,     0.f,     (zNear + zFar) / (zNear - zFar), -1.f,
        0.f,     0.f, (2 * zNear * zFar) / (zNear - zFar),  0.f};

     memcpy(matrix, m, sizeof(m));
}


static void getViewpointMatrixes(vout_display_opengl_t *vgl,
                                 video_projection_mode_t projection_mode,
                                 struct prgm *prgm)
{
    if (projection_mode == PROJECTION_MODE_EQUIRECTANGULAR
        || projection_mode == PROJECTION_MODE_CUBEMAP_LAYOUT_STANDARD
        || vgl->b_sideBySide)
    {
        float sar = (float) vgl->f_sar;
        getProjectionMatrix(sar, vgl->f_fovy, prgm->var.ProjectionMatrix);
        memcpy(prgm->var.ModelViewMatrix, identity, sizeof(identity));
        getYRotMatrix(vgl->f_teta, prgm->var.YRotMatrix);
        getXRotMatrix(vgl->f_phi, prgm->var.XRotMatrix);
        getZRotMatrix(vgl->f_roll, prgm->var.ZRotMatrix);
        getZoomMatrix(vgl->f_z, prgm->var.ZoomMatrix);
        memcpy(prgm->var.ObjectTransformMatrix, identity, sizeof(identity));
        memcpy(prgm->var.SceneTransformMatrix, identity, sizeof(identity));
        memcpy(prgm->var.HeadPositionMatrix, identity, sizeof(identity));
    }
    else
    {
        memcpy(prgm->var.ProjectionMatrix, identity, sizeof(identity));
        memcpy(prgm->var.ModelViewMatrix, identity, sizeof(identity));
        memcpy(prgm->var.ZRotMatrix, identity, sizeof(identity));
        memcpy(prgm->var.YRotMatrix, identity, sizeof(identity));
        memcpy(prgm->var.XRotMatrix, identity, sizeof(identity));
        memcpy(prgm->var.ZoomMatrix, identity, sizeof(identity));
        memcpy(prgm->var.ObjectTransformMatrix, identity, sizeof(identity));
        memcpy(prgm->var.SceneTransformMatrix, identity, sizeof(identity));
        memcpy(prgm->var.HeadPositionMatrix, identity, sizeof(identity));
    }
}


static void getSbSParams(vout_display_opengl_t *vgl, struct prgm *prgm,
                         side_by_side_eye eye)
{
    float *SbSCoefs = prgm->var.SbSCoefs;
    float *SbSOffsets = prgm->var.SbSOffsets;

    if (vgl->b_sideBySide)
    {
        switch (vgl->fmt.multiview_mode)
        {
        case MULTIVIEW_STEREO_TB:
            switch (eye)
            {
            case LEFT_EYE:
                SbSCoefs[0] = 1; SbSCoefs[1] = 0.5;
                SbSOffsets[0] = 0; SbSOffsets[1] = 0;
                break;
            case RIGHT_EYE:
                SbSCoefs[0] = 1; SbSCoefs[1] = 0.5;
                SbSOffsets[0] = 0; SbSOffsets[1] = 0.5;
                break;
            default:
                vlc_assert_unreachable();
                break;
            }
            break;
        case MULTIVIEW_STEREO_SBS:
            switch (eye)
            {
            case LEFT_EYE:
                SbSCoefs[0] = 0.5; SbSCoefs[1] = 1;
                SbSOffsets[0] = 0; SbSOffsets[1] = 0;
                break;
            case RIGHT_EYE:
                SbSCoefs[0] = 0.5; SbSCoefs[1] = 1;
                SbSOffsets[0] = 0.5; SbSOffsets[1] = 0;
                break;
            default:
                vlc_assert_unreachable();
                break;
            }
            break;
        default:
            SbSCoefs[0] = 1; SbSCoefs[1] = 1;
            SbSOffsets[0] = 0; SbSOffsets[1] = 0;
            break;
        }
    }
    else
    {
        SbSCoefs[0] = 1; SbSCoefs[1] = 1;
        SbSOffsets[0] = 0; SbSOffsets[1] = 0;
    }
}


static void getOrientationTransformMatrix(video_orientation_t orientation,
                                          GLfloat matrix[static 16])
{
    memcpy(matrix, identity, sizeof(identity));

    const int k_cos_pi = -1;
    const int k_cos_pi_2 = 0;
    const int k_cos_n_pi_2 = 0;

    const int k_sin_pi = 0;
    const int k_sin_pi_2 = 1;
    const int k_sin_n_pi_2 = -1;

    switch (orientation) {

        case ORIENT_ROTATED_90:
            matrix[0 * 4 + 0] = k_cos_pi_2;
            matrix[0 * 4 + 1] = -k_sin_pi_2;
            matrix[1 * 4 + 0] = k_sin_pi_2;
            matrix[1 * 4 + 1] = k_cos_pi_2;
            matrix[3 * 4 + 1] = 1;
            break;
        case ORIENT_ROTATED_180:
            matrix[0 * 4 + 0] = k_cos_pi;
            matrix[0 * 4 + 1] = -k_sin_pi;
            matrix[1 * 4 + 0] = k_sin_pi;
            matrix[1 * 4 + 1] = k_cos_pi;
            matrix[3 * 4 + 0] = 1;
            matrix[3 * 4 + 1] = 1;
            break;
        case ORIENT_ROTATED_270:
            matrix[0 * 4 + 0] = k_cos_n_pi_2;
            matrix[0 * 4 + 1] = -k_sin_n_pi_2;
            matrix[1 * 4 + 0] = k_sin_n_pi_2;
            matrix[1 * 4 + 1] = k_cos_n_pi_2;
            matrix[3 * 4 + 0] = 1;
            break;
        case ORIENT_HFLIPPED:
            matrix[0 * 4 + 0] = -1;
            matrix[3 * 4 + 0] = 1;
            break;
        case ORIENT_VFLIPPED:
            matrix[1 * 4 + 1] = -1;
            matrix[3 * 4 + 1] = 1;
            break;
        case ORIENT_TRANSPOSED:
            matrix[0 * 4 + 0] = 0;
            matrix[1 * 4 + 1] = 0;
            matrix[2 * 4 + 2] = -1;
            matrix[0 * 4 + 1] = 1;
            matrix[1 * 4 + 0] = 1;
            break;
        case ORIENT_ANTI_TRANSPOSED:
            matrix[0 * 4 + 0] = 0;
            matrix[1 * 4 + 1] = 0;
            matrix[2 * 4 + 2] = -1;
            matrix[0 * 4 + 1] = -1;
            matrix[1 * 4 + 0] = -1;
            matrix[3 * 4 + 0] = 1;
            matrix[3 * 4 + 1] = 1;
            break;
        default:
            break;
    }
}

static inline GLsizei GetAlignedSize(unsigned size)
{
    /* Return the smallest larger or equal power of 2 */
    unsigned align = 1 << (8 * sizeof (unsigned) - clz(size));
    return ((align >> 1) == size) ? size : align;
}

static GLuint BuildVertexShader(const opengl_tex_converter_t *tc,
                                unsigned plane_count)
{
    /* Basic vertex shader */
    static const char *template =
        "#version %u\n"
        "varying vec2 TexCoord0;\n"
        "varying vec3 Position_world;\n"
        "varying mat4 ViewMatrix;\n"
        "varying mat4 ModelMatrix;\n"
        "varying mat4 NormalMatrix;\n"
        "varying mat3 TBNMatrix;\n"
        "attribute vec4 MultiTexCoord0;\n"
        "%s%s"
        "attribute vec3 VertexPosition;\n"
        "attribute vec3 VertexNormal;\n"
        "attribute vec3 VertexTangent;\n"
        "uniform mat4 OrientationMatrix;\n"
        "uniform mat4 ProjectionMatrix;\n"
        "uniform mat4 ModelViewMatrix;\n"
        "uniform mat4 XRotMatrix;\n"
        "uniform mat4 YRotMatrix;\n"
        "uniform mat4 ZRotMatrix;\n"
        "uniform mat4 ZoomMatrix;\n"
        "attribute mat4 ObjectTransformMatrix;\n"
        "uniform mat4 SceneTransformMatrix;\n"
        "uniform mat4 HeadPositionMatrix;\n"
        "void main() {\n"
        " TexCoord0 = vec4(OrientationMatrix * MultiTexCoord0).st;\n"
        "%s%s"
        " ViewMatrix  = ModelViewMatrix * ZoomMatrix * ZRotMatrix * XRotMatrix * YRotMatrix * HeadPositionMatrix * SceneTransformMatrix;\n"
        " ModelMatrix = ObjectTransformMatrix;\n"
        " NormalMatrix = ViewMatrix*ModelMatrix;\n"

        " Position_world = vec3(ModelMatrix*vec4(VertexPosition, 1));\n"

        // Compute TBN matrix so as to compute normals in world space
        // It should be inverse(transpose(ModelMatrix)) but as there no
        // not-uniform scale, ModelMatrix is orthonormal, thus its tranpose
        // equal its inverse
        " vec3 Normal_world = mat3(ModelMatrix) * VertexNormal;\n"
        " vec3 Tangent_world = mat3(ModelMatrix) * VertexTangent;\n"
        " vec3 Bitangent_world = cross(Normal_world, Tangent_world);\n"
        " TBNMatrix = mat3(Tangent_world, Bitangent_world, Normal_world);\n"

        " gl_Position = ProjectionMatrix * ViewMatrix * vec4(Position_world, 1);\n"
        " if (dot(ViewMatrix * vec4(Position_world, 1) * gl_Position, vec4(0, 0, -1, 0)) <= 0)\n"
        "   gl_Position = ProjectionMatrix * vec4(0, 0, 1, 1);\n"
        "}";

    const char *coord1_header = plane_count > 1 ?
        "varying vec2 TexCoord1;\nattribute vec4 MultiTexCoord1;\n" : "";
    const char *coord1_code = plane_count > 1 ?
        " TexCoord1 = vec4(OrientationMatrix * MultiTexCoord1).st;\n" : "";
    const char *coord2_header = plane_count > 2 ?
        "varying vec2 TexCoord2;\nattribute vec4 MultiTexCoord2;\n" : "";
    const char *coord2_code = plane_count > 2 ?
        " TexCoord2 = vec4(OrientationMatrix * MultiTexCoord2).st;\n" : "";

    char *code;
    if (asprintf(&code, template, tc->glsl_version, coord1_header, coord2_header,
                 coord1_code, coord2_code) < 0)
        return 0;

    GLuint shader = tc->vt->CreateShader(GL_VERTEX_SHADER);
    tc->vt->ShaderSource(shader, 1, (const char **) &code, NULL);
    if (tc->b_dump_shaders)
        msg_Dbg(tc->gl, "\n=== Vertex shader for fourcc: %4.4s ===\n%s\n",
                (const char *)&tc->fmt.i_chroma, code);
    tc->vt->CompileShader(shader);
    free(code);
    return shader;
}

static int
GenTextures(const opengl_tex_converter_t *tc,
            const GLsizei *tex_width, const GLsizei *tex_height,
            GLuint *textures)
{
    tc->vt->GenTextures(tc->tex_count, textures);

    for (unsigned i = 0; i < tc->tex_count; i++)
    {
        tc->vt->BindTexture(tc->tex_target, textures[i]);

#if !defined(USE_OPENGL_ES2)
        /* Set the texture parameters */
        tc->vt->TexParameterf(tc->tex_target, GL_TEXTURE_PRIORITY, 1.0);
        tc->vt->TexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
#endif

        tc->vt->TexParameteri(tc->tex_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        tc->vt->TexParameteri(tc->tex_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        tc->vt->TexParameteri(tc->tex_target, GL_TEXTURE_WRAP_S, GL_REPEAT);
        tc->vt->TexParameteri(tc->tex_target, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    if (tc->pf_allocate_textures != NULL)
    {
        int ret = tc->pf_allocate_textures(tc, textures, tex_width, tex_height);
        if (ret != VLC_SUCCESS)
        {
            tc->vt->DeleteTextures(tc->tex_count, textures);
            memset(textures, 0, tc->tex_count * sizeof(GLuint));
            return ret;
        }
    }
    return VLC_SUCCESS;
}

static void
DelTextures(const opengl_tex_converter_t *tc, GLuint *textures)
{
    tc->vt->DeleteTextures(tc->tex_count, textures);
    memset(textures, 0, tc->tex_count * sizeof(GLuint));
}

static int CheckShaderMessages(const vlc_gl_t *gl, const opengl_vtable_t *vt,
                               GLuint* shaders)
{
    for (unsigned i = 0; i < 2; i++) {
        int infoLength;
        vt->GetShaderiv(shaders[i], GL_INFO_LOG_LENGTH, &infoLength);
        if (infoLength <= 1)
            continue;

        char *infolog = malloc(infoLength);
        if (infolog != NULL)
        {
            int charsWritten;
            vt->GetShaderInfoLog(shaders[i], infoLength, &charsWritten,
                                      infolog);
            msg_Err(tc->gl, "shader %u: %s", i, infolog);
            free(infolog);
        }
    }

    return VLC_SUCCESS;
}

static int CheckProgramMessages(const vlc_gl_t *gl, const opengl_vtable_t *vt,
                                struct prgm *prgm)
{
    int infoLength = 0;
    vt->GetProgramiv(prgm->id, GL_INFO_LOG_LENGTH, &infoLength);
    if (infoLength > 1)
    {
        char *infolog = malloc(infoLength);
        if (infolog != NULL)
        {
            int charsWritten;
            vt->GetProgramInfoLog(prgm->id, infoLength, &charsWritten,
                                       infolog);
            msg_Err(gl, "shader program: %s", infolog);
            free(infolog);
        }

        /* If there is some message, better to check linking is ok */
        GLint link_status = GL_TRUE;
        vt->GetProgramiv(prgm->id, GL_LINK_STATUS, &link_status);
        if (link_status == GL_FALSE)
        {
            msg_Err(gl, "Unable to use program");
            return VLC_EGENERIC;
        }
    }

    return VLC_SUCCESS;
}


static int
opengl_link_program(struct prgm *prgm)
{
    opengl_tex_converter_t *tc = prgm->tc;

    GLuint vertex_shader = BuildVertexShader(tc, tc->tex_count);
    GLuint shaders[] = { tc->fshader, vertex_shader };

    if (CheckShaderMessages(tc->gl, tc->vt, shaders) != VLC_SUCCESS)
        goto error;

    prgm->id = tc->vt->CreateProgram();
    tc->vt->AttachShader(prgm->id, tc->fshader);
    tc->vt->AttachShader(prgm->id, vertex_shader);
    tc->vt->LinkProgram(prgm->id);

    tc->vt->DeleteShader(vertex_shader);
    tc->vt->DeleteShader(tc->fshader);

    if (CheckProgramMessages(tc->gl, tc->vt, prgm) != VLC_SUCCESS)
        goto error;

    /* Fetch UniformLocations and AttribLocations */
#define GET_LOC(type, x, str) do { \
    x = tc->vt->Get##type##Location(prgm->id, str); }while(0)
    //assert(x != -1); \
    if (x == -1) { \
        msg_Err(tc->gl, "Unable to Get"#type"Location(%s)", str); \
        goto error; \
    } \
} while (0)
#define GET_ULOC(x, str) GET_LOC(Uniform, prgm->uloc.x, str)
#define GET_ALOC(x, str) GET_LOC(Attrib, prgm->aloc.x, str)
    GET_ULOC(OrientationMatrix, "OrientationMatrix");
    GET_ULOC(ProjectionMatrix, "ProjectionMatrix");
    GET_ULOC(ModelViewMatrix, "ModelViewMatrix");
    GET_ULOC(ZRotMatrix, "ZRotMatrix");
    GET_ULOC(YRotMatrix, "YRotMatrix");
    GET_ULOC(XRotMatrix, "XRotMatrix");
    GET_ULOC(ZoomMatrix, "ZoomMatrix");
    //GET_ULOC(ObjectTransformMatrix, "ObjectTransformMatrix");
    GET_ULOC(SceneTransformMatrix, "SceneTransformMatrix");
    GET_ULOC(HeadPositionMatrix, "HeadPositionMatrix");
    GET_ULOC(SbSCoefs, "SbSCoefs");
    GET_ULOC(SbSOffsets, "SbSOffsets");

    GET_ALOC(VertexPosition, "VertexPosition");
    GET_ALOC(VertexNormal, "VertexNormal");
    GET_ALOC(VertexTangent, "VertexTangent");
    GET_ALOC(MultiTexCoord[0], "MultiTexCoord0");
    GET_ALOC(ObjectTransformMatrix, "ObjectTransformMatrix");
    /* MultiTexCoord 1 and 2 can be optimized out if not used */
    if (prgm->tc->tex_count > 1)
        GET_ALOC(MultiTexCoord[1], "MultiTexCoord1");
    else
        prgm->aloc.MultiTexCoord[1] = -1;
    if (prgm->tc->tex_count > 2)
        GET_ALOC(MultiTexCoord[2], "MultiTexCoord2");
    else
        prgm->aloc.MultiTexCoord[2] = -1;

    GET_ULOC(HasLight, "HasLight");
    GET_ULOC(LightCount, "LightCount");

    GET_ULOC(lights.Position, "Lights.Position");
    GET_ULOC(lights.Ambient, "Lights.Ambient");
    GET_ULOC(lights.Diffuse, "Lights.Diffuse");
    GET_ULOC(lights.Specular, "Lights.Specular");
    GET_ULOC(lights.Kc, "Lights.Kc");
    GET_ULOC(lights.Kl, "Lights.Kl");
    GET_ULOC(lights.Kq, "Lights.Kq");
    GET_ULOC(lights.Direction, "Lights.Direction");
    GET_ULOC(lights.Cutoff, "Lights.Cutoff");

    GET_ULOC(MatAmbientTex, "MatAmbientTex");
    GET_ULOC(MatDiffuseTex, "MatDiffuseTex");
    GET_ULOC(MatNormalTex, "MatNormalTex");
    GET_ULOC(MatSpecularTex, "MatSpecularTex");

    GET_ULOC(MatAmbient, "MatAmbient");
    GET_ULOC(MatDiffuse, "MatDiffuse");
    GET_ULOC(MatSpecular, "MatSpecular");
    GET_ULOC(SceneAmbient, "SceneAmbient");

    GET_ULOC(UseAmbiantTexture, "UseAmbiantTexture");
    GET_ULOC(UseDiffuseTexture, "UseDiffuseTexture");
    GET_ULOC(UseSpecularTexture, "UseSpecularTexture");

#undef GET_LOC
#undef GET_ULOC
#undef GET_ALOC
    int ret = prgm->tc->pf_fetch_locations(prgm->tc, prgm->id);
    assert(ret == VLC_SUCCESS);
    if (ret != VLC_SUCCESS)
    {
        msg_Err(tc->gl, "Unable to get locations from tex_conv");
        goto error;
    }

    return VLC_SUCCESS;

error:
    tc->vt->DeleteProgram(prgm->id);
    prgm->id = 0;
    return VLC_EGENERIC;
}

static void
opengl_deinit_program(vout_display_opengl_t *vgl, struct prgm *prgm)
{
    opengl_tex_converter_t *tc = prgm->tc;
    if (tc->p_module != NULL)
        module_unneed(tc, tc->p_module);
    else if (tc->priv != NULL)
        opengl_tex_converter_generic_deinit(tc);
    if (prgm->id != 0)
        vgl->vt.DeleteProgram(prgm->id);

#ifdef HAVE_LIBPLACEBO
    FREENULL(tc->uloc.pl_vars);
    if (tc->pl_ctx)
        pl_context_destroy(&tc->pl_ctx);
#endif

    vlc_object_release(tc);
}

#ifdef HAVE_LIBPLACEBO
static void
log_cb(void *priv, enum pl_log_level level, const char *msg)
{
    opengl_tex_converter_t *tc = priv;
    switch (level) {
    case PL_LOG_FATAL: // fall through
    case PL_LOG_ERR:  msg_Err(tc->gl, "%s", msg); break;
    case PL_LOG_WARN: msg_Warn(tc->gl,"%s", msg); break;
    case PL_LOG_INFO: msg_Info(tc->gl,"%s", msg); break;
    default: break;
    }
}
#endif

static int
opengl_init_program(vout_display_opengl_t *vgl, struct prgm *prgm,
                    const char *glexts, const video_format_t *fmt, bool subpics,
                    bool b_dump_shaders)
{
    opengl_tex_converter_t *tc =
        vlc_object_create(vgl->gl, sizeof(opengl_tex_converter_t));
    if (tc == NULL)
        return VLC_ENOMEM;

    tc->gl = vgl->gl;
    tc->vt = &vgl->vt;
    tc->b_dump_shaders = b_dump_shaders;
    tc->pf_fragment_shader_init = opengl_fragment_shader_init_impl;
    tc->glexts = glexts;
#if defined(USE_OPENGL_ES2)
    tc->is_gles = true;
    tc->glsl_version = 100;
    tc->glsl_precision_header = "precision highp float;\n";
#else
    tc->is_gles = false;
    tc->glsl_version = 120;
    tc->glsl_precision_header = "";
#endif
    tc->fmt = *fmt;

#ifdef HAVE_LIBPLACEBO
    // create the main libplacebo context
    if (!subpics)
    {
        tc->pl_ctx = pl_context_create(PL_API_VER, &(struct pl_context_params) {
            .log_cb    = log_cb,
            .log_priv  = tc,
            .log_level = PL_LOG_INFO,
        });
        if (tc->pl_ctx)
            tc->pl_sh = pl_shader_alloc(tc->pl_ctx, NULL, 0, 0);
    }
#endif

    int ret;
    if (subpics)
    {
        tc->fmt.i_chroma = VLC_CODEC_RGB32;
        /* Normal orientation and no projection for subtitles */
        tc->fmt.orientation = ORIENT_NORMAL;
        //tc->fmt.projection_mode = PROJECTION_MODE_RECTANGULAR;
        tc->fmt.primaries = COLOR_PRIMARIES_UNDEF;
        tc->fmt.transfer = TRANSFER_FUNC_UNDEF;
        tc->fmt.space = COLOR_SPACE_UNDEF;

        ret = opengl_tex_converter_generic_init(tc, false);
    }
    else
    {
        const vlc_chroma_description_t *desc =
            vlc_fourcc_GetChromaDescription(fmt->i_chroma);

        if (desc == NULL)
        {
            vlc_object_release(tc);
            return VLC_EGENERIC;
        }
        if (desc->plane_count == 0)
        {
            /* Opaque chroma: load a module to handle it */
            tc->p_module = module_need_var(tc, "glconv", "glconv");
        }

        if (tc->p_module != NULL)
            ret = VLC_SUCCESS;
        else
        {
            /* Software chroma or gl hw converter failed: use a generic
             * converter */
            ret = opengl_tex_converter_generic_init(tc, true);
        }
    }

    if (ret != VLC_SUCCESS)
    {
        vlc_object_release(tc);
        return VLC_EGENERIC;
    }

    assert(tc->fshader != 0 && tc->tex_target != 0 && tc->tex_count > 0 &&
           tc->pf_update != NULL && tc->pf_fetch_locations != NULL &&
           tc->pf_prepare_shader != NULL);

    prgm->tc = tc;

    ret = opengl_link_program(prgm);
    if (ret != VLC_SUCCESS)
    {
        opengl_deinit_program(vgl, prgm);
        return VLC_EGENERIC;
    }

    getOrientationTransformMatrix(tc->fmt.orientation,
                                  prgm->var.OrientationMatrix);
    getViewpointMatrixes(vgl, tc->fmt.projection_mode, prgm);

    return VLC_SUCCESS;
}

static void
ResizeFormatToGLMaxTexSize(video_format_t *fmt, unsigned int max_tex_size)
{
    if (fmt->i_width > fmt->i_height)
    {
        unsigned int const  vis_w = fmt->i_visible_width;
        unsigned int const  vis_h = fmt->i_visible_height;
        unsigned int const  nw_w = max_tex_size;
        unsigned int const  nw_vis_w = nw_w * vis_w / fmt->i_width;

        fmt->i_height = nw_w * fmt->i_height / fmt->i_width;
        fmt->i_width = nw_w;
        fmt->i_visible_height = nw_vis_w * vis_h / vis_w;
        fmt->i_visible_width = nw_vis_w;
    }
    else
    {
        unsigned int const  vis_w = fmt->i_visible_width;
        unsigned int const  vis_h = fmt->i_visible_height;
        unsigned int const  nw_h = max_tex_size;
        unsigned int const  nw_vis_h = nw_h * vis_h / fmt->i_height;

        fmt->i_width = nw_h * fmt->i_width / fmt->i_height;
        fmt->i_height = nw_h;
        fmt->i_visible_width = nw_vis_h * vis_w / vis_h;
        fmt->i_visible_height = nw_vis_h;
    }
}

// Temporary hack.
#if defined(USE_OPENGL_ES2)
#define GLSL_VERSION "100"
#define GLSL_PRECISION "precision highp float;\n"
#else
#define GLSL_VERSION "120"
#define GLSL_PRECISION ""
#endif

static void BuildStereoVertexShader(vout_display_opengl_t *vgl,
                                    GLuint *shader)
{
    const char *fragmentShader =
        "#version " GLSL_VERSION "\n"
        GLSL_PRECISION
        "attribute vec4 MultiTexCoord0;"
        "attribute vec3 VertexPosition;"
        "varying vec2 TexCoord0;"
        ""

        "void main(void)"
        "{"
        "   TexCoord0 = MultiTexCoord0.st;"
        "   gl_Position = vec4(VertexPosition, 1.0);"
        "}";

    *shader = vgl->vt.CreateShader(GL_VERTEX_SHADER);
    vgl->vt.ShaderSource(*shader, 1, &fragmentShader, NULL);
    vgl->vt.CompileShader(*shader);
}


static void BuildStereoFragmentShader(vout_display_opengl_t *vgl,
                                      GLuint *shader)
{
    // Fragment shader from OpenHMD.
    const char *fragmentShader =
        "#version " GLSL_VERSION "\n"
        GLSL_PRECISION
        "//per eye texture to warp for lens distortion\n"
        "uniform sampler2D warpTexture;\n"
        "\n"
        "//Position of lens center in m (usually eye_w/2, eye_h/2)\n"
        "uniform vec2 LensCenter;\n"
        "//Scale from texture co-ords to m (usually eye_w, eye_h)\n"
        "uniform vec2 ViewportScale;\n"
        "//Distortion overall scale in m (usually ~eye_w/2)\n"
        "uniform float WarpScale;\n"
        "//Distoriton coefficients (PanoTools model) [a,b,c,d]\n"
        "uniform vec4 HmdWarpParam;\n"
        "\n"
        "//chromatic distortion post scaling\n"
        "uniform vec3 aberr;\n"
        "\n"
        "varying vec2 TexCoord0;\n"
        "\n"
        "void main()\n"
        "{\n"
            "//output_loc is the fragment location on screen from [0,1]x[0,1]\n"
            "vec2 output_loc = vec2(TexCoord0.s, TexCoord0.t);\n"
            "//Compute fragment location in lens-centered co-ordinates at world scale\n"
            "vec2 r = output_loc * ViewportScale - LensCenter;\n"
            "//scale for distortion model\n"
            "//distortion model has r=1 being the largest circle inscribed (e.g. eye_w/2)\n"
            "r /= WarpScale;\n"
        "\n"
            "//|r|**2\n"
            "float r_mag = length(r);\n"
            "//offset for which fragment is sourced\n"
            "vec2 r_displaced = r * (HmdWarpParam.w + HmdWarpParam.z * r_mag +\n"
                "HmdWarpParam.y * r_mag * r_mag +\n"
                "HmdWarpParam.x * r_mag * r_mag * r_mag);\n"
            "//back to world scale\n"
            "r_displaced *= WarpScale;\n"
            "//back to viewport co-ord\n"
            "vec2 tc_r = (LensCenter + aberr.r * r_displaced) / ViewportScale;\n"
            "vec2 tc_g = (LensCenter + aberr.g * r_displaced) / ViewportScale;\n"
            "vec2 tc_b = (LensCenter + aberr.b * r_displaced) / ViewportScale;\n"
        "\n"
            "float red = texture2D(warpTexture, tc_r).r;\n"
            "float green = texture2D(warpTexture, tc_g).g;\n"
            "float blue = texture2D(warpTexture, tc_b).b;\n"
            "//Black edges off the texture\n"
            "gl_FragColor = ((tc_g.x < 0.0) || (tc_g.x > 1.0) || (tc_g.y < 0.0) || (tc_g.y > 1.0)) ? vec4(0.0, 0.0, 0.0, 1.0) : vec4(red, green, blue, 1.0);\n"
        "}";

    *shader = vgl->vt.CreateShader(GL_FRAGMENT_SHADER);
    vgl->vt.ShaderSource(*shader, 1, &fragmentShader, NULL);
    vgl->vt.CompileShader(*shader);
}


static int opengl_init_stereo_program(vout_display_opengl_t *vgl)
{
    GLuint stereo_vertex_shader;
    GLuint stereo_fragment_shader;
    BuildStereoVertexShader(vgl, &stereo_vertex_shader);
    BuildStereoFragmentShader(vgl, &stereo_fragment_shader);

    GLuint shaders[] = {stereo_vertex_shader, stereo_fragment_shader};

    if (CheckShaderMessages(vgl->gl, &vgl->vt, shaders) != VLC_SUCCESS)
        return VLC_EGENERIC;

    /* Stereo fragment shaders */
    vgl->stereo_prgm->id = vgl->vt.CreateProgram();
    vgl->vt.AttachShader(vgl->stereo_prgm->id, stereo_fragment_shader);
    vgl->vt.AttachShader(vgl->stereo_prgm->id, stereo_vertex_shader);
    vgl->vt.LinkProgram(vgl->stereo_prgm->id);

    if (CheckProgramMessages(vgl->gl, &vgl->vt, vgl->stereo_prgm) != VLC_SUCCESS)
        return VLC_EGENERIC;

    vgl->vt.UseProgram(vgl->stereo_prgm->id);
    vgl->vt.Uniform1i(vgl->vt.GetUniformLocation(vgl->stereo_prgm->id, "warpTexture"), 0);

    return VLC_SUCCESS;
}


static void CreateFBO(vout_display_opengl_t *vgl,
                      GLuint* fbo, GLuint* color_tex, GLuint* depth_tex)
{
    vgl->vt.GenTextures(1, color_tex);
#ifdef USE_OPENGL_ES2
    vgl->vt.GenRenderbuffers(1, depth_tex);
#else
    vgl->vt.GenTextures(1, depth_tex);
#endif
    vgl->vt.GenFramebuffers(1, fbo);
}


static void DeleteFBO(vout_display_opengl_t *vgl,
                      GLuint fbo, GLuint color_tex, GLuint depth_tex)
{
    vgl->vt.DeleteTextures(1, &color_tex);
#ifdef USE_OPENGL_ES2
    vgl->vt.DeleteRenderbuffers(1, &depth_tex);
#else
    vgl->vt.DeleteTextures(1, &depth_tex);
#endif
    vgl->vt.DeleteFramebuffers(1, &fbo);
}


static void UpdateFBOSize(vout_display_opengl_t *vgl,
                          GLuint fbo, GLuint color_tex, GLuint depth_tex)
{
#ifdef USE_OPENGL_ES2
    vgl->vt.BindTexture(GL_TEXTURE_2D, color_tex);
    vgl->vt.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    vgl->vt.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, vgl->i_displayWidth, vgl->i_displayHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    vgl->vt.BindRenderbuffer(GL_RENDERBUFFER, depth_tex);
    vgl->vt.RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, vgl->i_displayWidth, vgl->i_displayHeight);
#else
    vgl->vt.BindTexture(GL_TEXTURE_2D, color_tex);
    vgl->vt.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, vgl->i_displayWidth / 2.f, vgl->i_displayHeight, 0, GL_RGBA, GL_UNSIGNED_INT, NULL);
    vgl->vt.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    vgl->vt.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    vgl->vt.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    vgl->vt.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    vgl->vt.BindTexture(GL_TEXTURE_2D, depth_tex);
    vgl->vt.TexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, vgl->i_displayWidth / 2.f, vgl->i_displayHeight, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
    vgl->vt.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    vgl->vt.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    vgl->vt.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    vgl->vt.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
#endif

    vgl->vt.BindTexture(GL_TEXTURE_2D, 0);

    vgl->vt.BindFramebuffer(GL_FRAMEBUFFER, fbo);
    vgl->vt.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex, 0);
#ifdef USE_OPENGL_ES2
    vgl->vt.FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_tex);
#else
    vgl->vt.FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_tex, 0);
#endif

    GLenum status = vgl->vt.CheckFramebufferStatus(GL_FRAMEBUFFER);

    if(status != GL_FRAMEBUFFER_COMPLETE){
        msg_Err(vgl->gl, "Failed to create fbo %x %d %d\n", status, vgl->i_displayWidth, vgl->i_displayHeight);
    }
    vgl->vt.BindFramebuffer(GL_FRAMEBUFFER, 0);
}


vout_display_opengl_t *vout_display_opengl_New(video_format_t *fmt,
                                               const vlc_fourcc_t **subpicture_chromas,
                                               vlc_gl_t *gl,
                                               const vlc_viewpoint_t *viewpoint, bool b_hmd)
{
    if (gl->getProcAddress == NULL) {
        msg_Err(gl, "getProcAddress not implemented, bailing out");
        return NULL;
    }

    vout_display_opengl_t *vgl = calloc(1, sizeof(*vgl));
    if (!vgl)
        return NULL;

    vgl->gl = gl;

#if defined(USE_OPENGL_ES2) || defined(HAVE_GL_CORE_SYMBOLS)
#define GET_PROC_ADDR_CORE(name) vgl->vt.name = gl##name
#else
#define GET_PROC_ADDR_CORE(name) GET_PROC_ADDR_EXT(name, true)
#endif
#define GET_PROC_ADDR_EXT(name, critical) do { \
    vgl->vt.name = vlc_gl_GetProcAddress(gl, "gl"#name); \
    if (vgl->vt.name == NULL && critical) { \
        msg_Err(gl, "gl"#name" symbol not found, bailing out"); \
        free(vgl); \
        return NULL; \
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
    GET_PROC_ADDR_CORE(CullFace);
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

    GET_PROC_ADDR(CreateShader);
    GET_PROC_ADDR(ShaderSource);
    GET_PROC_ADDR(CompileShader);
    GET_PROC_ADDR(AttachShader);
    GET_PROC_ADDR(DeleteShader);

    GET_PROC_ADDR(DrawElementsInstanced);

    GET_PROC_ADDR(GetProgramiv);
    GET_PROC_ADDR(GetShaderiv);
    GET_PROC_ADDR(GetProgramInfoLog);
    GET_PROC_ADDR(GetShaderInfoLog);

    GET_PROC_ADDR(GetUniformLocation);
    GET_PROC_ADDR(GetAttribLocation);
    GET_PROC_ADDR(VertexAttribPointer);
    GET_PROC_ADDR(VertexAttribDivisor);
    GET_PROC_ADDR(EnableVertexAttribArray);
    GET_PROC_ADDR(UniformMatrix4fv);
    GET_PROC_ADDR(UniformMatrix3fv);
    GET_PROC_ADDR(UniformMatrix2fv);
    GET_PROC_ADDR(Uniform4fv);
    GET_PROC_ADDR(Uniform3fv);
    GET_PROC_ADDR(Uniform2fv);
    GET_PROC_ADDR(Uniform1fv);
    GET_PROC_ADDR(Uniform4f);
    GET_PROC_ADDR(Uniform3f);
    GET_PROC_ADDR(Uniform2f);
    GET_PROC_ADDR(Uniform1f);
    GET_PROC_ADDR(Uniform1i);
    GET_PROC_ADDR(Uniform1ui);

    GET_PROC_ADDR(CreateProgram);
    GET_PROC_ADDR(LinkProgram);
    GET_PROC_ADDR(UseProgram);
    GET_PROC_ADDR(DeleteProgram);

    GET_PROC_ADDR(ActiveTexture);
    GET_PROC_ADDR(GenerateMipmap);

    GET_PROC_ADDR(GenBuffers);
    GET_PROC_ADDR(BindBuffer);
    GET_PROC_ADDR(BufferData);
    GET_PROC_ADDR(DeleteBuffers);

    GET_PROC_ADDR(BindFramebuffer);
    GET_PROC_ADDR(GenFramebuffers);
    GET_PROC_ADDR(DeleteFramebuffers);
    GET_PROC_ADDR(CheckFramebufferStatus);
    GET_PROC_ADDR(FramebufferTexture2D);
    GET_PROC_ADDR(GenRenderbuffers);
    GET_PROC_ADDR(DeleteRenderbuffers);
    GET_PROC_ADDR(BindRenderbuffer);
    GET_PROC_ADDR(RenderbufferStorage);
    GET_PROC_ADDR(FramebufferRenderbuffer);

    GET_PROC_ADDR_OPTIONAL(GetFramebufferAttachmentParameteriv);

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

    GL_ASSERT_NOERROR();

    const char *extensions = (const char *)vgl->vt.GetString(GL_EXTENSIONS);
    assert(extensions);
    if (!extensions)
    {
        msg_Err(gl, "glGetString returned NULL");
        free(vgl);
        return NULL;
    }
#if !defined(USE_OPENGL_ES2)
    const unsigned char *ogl_version = vgl->vt.GetString(GL_VERSION);
    bool supports_shaders = strverscmp((const char *)ogl_version, "2.0") >= 0;
    if (!supports_shaders)
    {
        msg_Err(gl, "shaders not supported, bailing out");
        free(vgl);
        return NULL;
    }
#endif

    /* Resize the format if it is greater than the maximum texture size
     * supported by the hardware */
    GLint       max_tex_size;
    vgl->vt.GetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex_size);

    if ((GLint)fmt->i_width > max_tex_size ||
        (GLint)fmt->i_height > max_tex_size)
        ResizeFormatToGLMaxTexSize(fmt, max_tex_size);

#if defined(USE_OPENGL_ES2)
    /* OpenGL ES 2 includes support for non-power of 2 textures by specification
     * so checks for extensions are bound to fail. Check for OpenGL ES version instead. */
    vgl->supports_npot = true;
#else
    vgl->supports_npot = HasExtension(extensions, "GL_ARB_texture_non_power_of_two") ||
                         HasExtension(extensions, "GL_APPLE_texture_2D_limited_npot");
#endif

    bool b_dump_shaders = var_InheritInteger(gl, "verbose") >= 4;

    vgl->prgm = &vgl->prgms[0];
    vgl->sub_prgm = &vgl->prgms[1];
    vgl->stereo_prgm = &vgl->prgms[2];
    vgl->ctl_prgm = &vgl->prgms[3];
    vgl->scene_prgm = &vgl->prgms[4];

    GL_ASSERT_NOERROR();
    int ret;
    ret = opengl_init_program(vgl, vgl->prgm, extensions, fmt, false,
                              b_dump_shaders);
    if (ret != VLC_SUCCESS)
    {
        msg_Warn(gl, "could not init tex converter for %4.4s",
                 (const char *) &fmt->i_chroma);
        free(vgl);
        return NULL;
    }

    GL_ASSERT_NOERROR();
    ret = opengl_init_program(vgl, vgl->sub_prgm, extensions, fmt, true,
                              b_dump_shaders);
    if (ret != VLC_SUCCESS)
    {
        msg_Warn(gl, "could not init subpictures tex converter for %4.4s",
                 (const char *) &fmt->i_chroma);
        opengl_deinit_program(vgl, vgl->prgm);
        free(vgl);
        return NULL;
    }

    GL_ASSERT_NOERROR();
    ret = opengl_init_program(vgl, vgl->ctl_prgm, extensions, fmt, true,
                              b_dump_shaders);
    if (ret != VLC_SUCCESS)
    {
        msg_Warn(gl, "could not init hmd controller tex converter for %4.4s",
                 (const char *) &fmt->i_chroma);
        opengl_deinit_program(vgl, vgl->prgm);
        opengl_deinit_program(vgl, vgl->sub_prgm);
        free(vgl);
        return NULL;
    }

    GL_ASSERT_NOERROR();
    video_format_t sceneTextureFmt;
    sceneTextureFmt.i_chroma = VLC_CODEC_RGB32;
    sceneTextureFmt.orientation = ORIENT_NORMAL;
    sceneTextureFmt.primaries = COLOR_PRIMARIES_UNDEF;
    sceneTextureFmt.transfer = TRANSFER_FUNC_UNDEF;
    sceneTextureFmt.space = COLOR_SPACE_UNDEF;
    ret = opengl_init_program(vgl, vgl->scene_prgm, extensions, &sceneTextureFmt, false,
                              b_dump_shaders);
    if (ret != VLC_SUCCESS)
    {
        msg_Warn(gl, "could not init 3d scene converter for %4.4s",
                 (const char *) &fmt->i_chroma);
        opengl_deinit_program(vgl, vgl->prgm);
        opengl_deinit_program(vgl, vgl->sub_prgm);
        opengl_deinit_program(vgl, vgl->ctl_prgm);
        free(vgl);
        return NULL;
    }

    GL_ASSERT_NOERROR();

    /* Update the fmt to main program one */
    vgl->fmt = vgl->prgm->tc->fmt;
    /* The orientation is handled by the orientation matrix */
    vgl->fmt.orientation = fmt->orientation;

    /* Texture size */
    const opengl_tex_converter_t *tc = vgl->prgm->tc;
    for (unsigned j = 0; j < tc->tex_count; j++) {
        const GLsizei w = vgl->fmt.i_visible_width  * tc->texs[j].w.num
                        / tc->texs[j].w.den;
        const GLsizei h = vgl->fmt.i_visible_height * tc->texs[j].h.num
                        / tc->texs[j].h.den;
        if (vgl->supports_npot) {
            vgl->tex_width[j]  = w;
            vgl->tex_height[j] = h;
        } else {
            vgl->tex_width[j]  = GetAlignedSize(w);
            vgl->tex_height[j] = GetAlignedSize(h);
        }
    }

    /* Allocates our textures */
    assert(!vgl->sub_prgm->tc->handle_texs_gen);

    if (!vgl->prgm->tc->handle_texs_gen)
    {
        ret = GenTextures(vgl->prgm->tc, vgl->tex_width, vgl->tex_height,
                          vgl->texture);
        if (ret != VLC_SUCCESS)
        {
            vout_display_opengl_Delete(vgl);
            return NULL;
        }
    }

    ret = opengl_init_stereo_program(vgl);
    if (ret != VLC_SUCCESS)
    {
        msg_Warn(gl, "could not init the stereo shader");
        vout_display_opengl_Delete(vgl);
        return NULL;
    }

    /* */
    vgl->vt.Disable(GL_BLEND);
    //vgl->vt.Disable(GL_DEPTH_TEST);
    //vgl->vt.DepthMask(GL_FALSE);
    vgl->vt.Enable(GL_CULL_FACE);
    vgl->vt.ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    vgl->vt.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    vgl->vt.GenBuffers(1, &vgl->vertex_buffer_object);
    vgl->vt.GenBuffers(1, &vgl->index_buffer_object);
    vgl->vt.GenBuffers(vgl->prgm->tc->tex_count, vgl->texture_buffer_object);

    vgl->vt.GenBuffers(1, &vgl->vertex_buffer_object_stereo);
    vgl->vt.GenBuffers(1, &vgl->index_buffer_object_stereo);
    vgl->vt.GenBuffers(1, &vgl->texture_buffer_object_stereo);


    /* Initial number of allocated buffer objects for subpictures, will grow dynamically. */
    int subpicture_buffer_object_count = 8;
    vgl->subpicture_buffer_object = vlc_alloc(subpicture_buffer_object_count, sizeof(GLuint));
    if (!vgl->subpicture_buffer_object) {
        vout_display_opengl_Delete(vgl);
        return NULL;
    }
    vgl->subpicture_buffer_object_count = subpicture_buffer_object_count;
    vgl->vt.GenBuffers(vgl->subpicture_buffer_object_count, vgl->subpicture_buffer_object);

    /* */
    vgl->region_count = 0;
    vgl->region = NULL;
    vgl->pool = NULL;

    if ((vgl->fmt.projection_mode != PROJECTION_MODE_RECTANGULAR || b_hmd)
     && vout_display_opengl_SetViewpoint(vgl, viewpoint) != VLC_SUCCESS)
    {
        vout_display_opengl_Delete(vgl);
        return NULL;
    }

    *fmt = vgl->fmt;
    if (subpicture_chromas) {
        *subpicture_chromas = gl_subpicture_chromas;
    }

    CreateFBO(vgl, &vgl->leftFBO, &vgl->leftColorTex, &vgl->leftDepthTex);
    CreateFBO(vgl, &vgl->rightFBO, &vgl->rightColorTex, &vgl->rightDepthTex);

    vgl->p_objDisplay = loadSceneObjects("VirtualTheater" DIR_SEP "virtualCinemaTargo.json",
                                         vgl->gl, vgl->scene_prgm->tc);
    if (vgl->p_objDisplay == NULL)
        msg_Warn(vgl->gl, "Could not load the virtual theater");

    GL_ASSERT_NOERROR();
    return vgl;
}

void vout_display_opengl_Delete(vout_display_opengl_t *vgl)
{
    GL_ASSERT_NOERROR();

    /* */
    vgl->vt.Finish();
    vgl->vt.Flush();

    releaseSceneObjects(vgl->p_objDisplay);

    const size_t main_tex_count = vgl->prgm->tc->tex_count;
    const bool main_del_texs = !vgl->prgm->tc->handle_texs_gen;

    DeleteFBO(vgl, vgl->leftFBO, vgl->leftColorTex, vgl->leftDepthTex);
    DeleteFBO(vgl, vgl->rightFBO, vgl->rightColorTex, vgl->rightDepthTex);
    vgl->vt.DeleteProgram(vgl->stereo_prgm->id);

    if (vgl->pool)
        picture_pool_Release(vgl->pool);
    opengl_deinit_program(vgl, vgl->prgm);
    opengl_deinit_program(vgl, vgl->sub_prgm);
    opengl_deinit_program(vgl, vgl->ctl_prgm);
    opengl_deinit_program(vgl, vgl->scene_prgm);

    vgl->vt.DeleteBuffers(1, &vgl->vertex_buffer_object);
    vgl->vt.DeleteBuffers(1, &vgl->index_buffer_object);
    vgl->vt.DeleteBuffers(main_tex_count, vgl->texture_buffer_object);
    vgl->vt.DeleteBuffers(2, vgl->hmd_controller_buffer_object);

    if (vgl->subpicture_buffer_object_count > 0)
        vgl->vt.DeleteBuffers(vgl->subpicture_buffer_object_count,
                              vgl->subpicture_buffer_object);
    free(vgl->subpicture_buffer_object);

    if (main_del_texs)
        vgl->vt.DeleteTextures(main_tex_count, vgl->texture);

    vgl->vt.DeleteTextures(1, &vgl->hmd_controller_texture);

    for (int i = 0; i < vgl->region_count; i++)
    {
        if (vgl->region[i].texture)
            vgl->vt.DeleteTextures(1, &vgl->region[i].texture);
    }
    free(vgl->region);
    GL_ASSERT_NOERROR();

    free(vgl);
}

static void UpdateZ(vout_display_opengl_t *vgl)
{
    /* Do trigonometry to calculate the minimal z value
     * that will allow us to zoom out without seeing the outside of the
     * sphere (black borders). */
    float tan_fovx_2 = tanf(vgl->f_fovx / 2);
    float tan_fovy_2 = tanf(vgl->f_fovy / 2);
    float z_min = - SPHERE_RADIUS / sinf(atanf(sqrtf(
                    tan_fovx_2 * tan_fovx_2 + tan_fovy_2 * tan_fovy_2)));

    /* The FOV value above which z is dynamically calculated. */
    const float z_thresh = 90.f;

    if (vgl->f_fovx <= z_thresh * M_PI / 180)
        vgl->f_z = 0;
    else
    {
        float f = z_min / ((FIELD_OF_VIEW_DEGREES_MAX - z_thresh) * M_PI / 180);
        vgl->f_z = f * vgl->f_fovx - f * z_thresh * M_PI / 180;
        if (vgl->f_z < z_min)
            vgl->f_z = z_min;
    }
}

static void UpdateFOVy(vout_display_opengl_t *vgl)
{
    vgl->f_fovy = 2 * atanf(tanf(vgl->f_fovx / 2) / vgl->f_sar);
}

int vout_display_opengl_SetViewpoint(vout_display_opengl_t *vgl,
                                     const vlc_viewpoint_t *p_vp)
{
    if (p_vp->fov > FIELD_OF_VIEW_DEGREES_MAX
            || p_vp->fov < FIELD_OF_VIEW_DEGREES_MIN)
        return VLC_EBADVAR;

#define RAD(d) ((float) ((d) * M_PI / 180.f))
    float f_fovx = RAD(p_vp->fov);

    vgl->f_teta = RAD(p_vp->yaw) - (float) M_PI_2;
    vgl->f_phi  = RAD(p_vp->pitch);
    vgl->f_roll = RAD(p_vp->roll);

    if (fabsf(f_fovx - vgl->f_fovx) >= 0.001f)
    {
        /* FOVx has changed. */
        vgl->f_fovx = f_fovx;
        UpdateFOVy(vgl);
        UpdateZ(vgl);
    }
    getViewpointMatrixes(vgl, vgl->fmt.projection_mode, vgl->prgm);
    getViewpointMatrixes(vgl, vgl->fmt.projection_mode, vgl->scene_prgm);

    return VLC_SUCCESS;
#undef RAD
}


void vout_display_opengl_SetWindowAspectRatio(vout_display_opengl_t *vgl,
                                              float f_sar)
{
    /* Each time the window size changes, we must recompute the minimum zoom
     * since the aspect ration changes.
     * We must also set the new current zoom value. */
    vgl->f_sar = f_sar;
    UpdateFOVy(vgl);
    UpdateZ(vgl);
    getViewpointMatrixes(vgl, vgl->fmt.projection_mode, vgl->prgm);
    getViewpointMatrixes(vgl, vgl->fmt.projection_mode, vgl->scene_prgm);
}

void vout_display_opengl_Viewport(vout_display_opengl_t *vgl, int x, int y,
                                  unsigned width, unsigned height)
{
    vgl->i_displayX = x;
    vgl->i_displayY = y;
    vgl->i_displayWidth = width;
    vgl->i_displayHeight = height;

    if (vgl->b_sideBySide)
    {
        vgl->vt.Viewport(0, 0, width, height);

        UpdateFBOSize(vgl, vgl->leftFBO, vgl->leftColorTex, vgl->leftDepthTex);
        UpdateFBOSize(vgl, vgl->rightFBO, vgl->rightColorTex, vgl->rightDepthTex);
    }
    else
        vgl->vt.Viewport(x, y, width, height);
}

picture_pool_t *vout_display_opengl_GetPool(vout_display_opengl_t *vgl, unsigned requested_count)
{
    GL_ASSERT_NOERROR();

    if (vgl->pool)
        return vgl->pool;

    opengl_tex_converter_t *tc = vgl->prgm->tc;
    requested_count = __MIN(VLCGL_PICTURE_MAX, requested_count);
    /* Allocate with tex converter pool callback if it exists */
    if (tc->pf_get_pool != NULL)
    {
        vgl->pool = tc->pf_get_pool(tc, requested_count);
        if (!vgl->pool)
            goto error;
        return vgl->pool;
    }

    /* Allocate our pictures */
    picture_t *picture[VLCGL_PICTURE_MAX] = {NULL, };
    unsigned count;
    for (count = 0; count < requested_count; count++)
    {
        picture[count] = picture_NewFromFormat(&vgl->fmt);
        if (!picture[count])
            break;
    }
    if (count <= 0)
        goto error;

    /* Wrap the pictures into a pool */
    vgl->pool = picture_pool_New(count, picture);
    if (!vgl->pool)
    {
        for (unsigned i = 0; i < count; i++)
            picture_Release(picture[i]);
        goto error;
    }

    GL_ASSERT_NOERROR();
    return vgl->pool;

error:
    DelTextures(tc, vgl->texture);
    return NULL;
}

int vout_display_opengl_Prepare(vout_display_opengl_t *vgl,
                                picture_t *picture, subpicture_t *subpicture)
{
    GL_ASSERT_NOERROR();

    opengl_tex_converter_t *tc = vgl->prgm->tc;

    /* Update the texture */
    int ret = tc->pf_update(tc, vgl->texture, vgl->tex_width, vgl->tex_height,
                            picture, NULL);
    if (ret != VLC_SUCCESS)
        return ret;

    int         last_count = vgl->region_count;
    gl_region_t *last = vgl->region;

    vgl->region_count = 0;
    vgl->region       = NULL;

    tc = vgl->sub_prgm->tc;
    if (subpicture) {

        int count = 0;
        for (subpicture_region_t *r = subpicture->p_region; r; r = r->p_next)
            count++;

        vgl->region_count = count;
        vgl->region       = calloc(count, sizeof(*vgl->region));

        int i = 0;
        for (subpicture_region_t *r = subpicture->p_region;
             r && ret == VLC_SUCCESS; r = r->p_next, i++) {
            gl_region_t *glr = &vgl->region[i];

            glr->width  = r->fmt.i_visible_width;
            glr->height = r->fmt.i_visible_height;
            if (!vgl->supports_npot) {
                glr->width  = GetAlignedSize(glr->width);
                glr->height = GetAlignedSize(glr->height);
                glr->tex_width  = (float) r->fmt.i_visible_width  / glr->width;
                glr->tex_height = (float) r->fmt.i_visible_height / glr->height;
            } else {
                glr->tex_width  = 1.0;
                glr->tex_height = 1.0;
            }
            glr->alpha  = (float)subpicture->i_alpha * r->i_alpha / 255 / 255;
            glr->left   =  2.0 * (r->i_x                          ) / subpicture->i_original_picture_width  - 1.0;
            glr->top    = -2.0 * (r->i_y                          ) / subpicture->i_original_picture_height + 1.0;
            glr->right  =  2.0 * (r->i_x + r->fmt.i_visible_width ) / subpicture->i_original_picture_width  - 1.0;
            glr->bottom = -2.0 * (r->i_y + r->fmt.i_visible_height) / subpicture->i_original_picture_height + 1.0;

            glr->texture = 0;
            /* Try to recycle the textures allocated by the previous
               call to this function. */
            for (int j = 0; j < last_count; j++) {
                if (last[j].texture &&
                    last[j].width  == glr->width &&
                    last[j].height == glr->height) {
                    glr->texture = last[j].texture;
                    memset(&last[j], 0, sizeof(last[j]));
                    break;
                }
            }

            const size_t pixels_offset =
                r->fmt.i_y_offset * r->p_picture->p->i_pitch +
                r->fmt.i_x_offset * r->p_picture->p->i_pixel_pitch;
            if (!glr->texture)
            {
                /* Could not recycle a previous texture, generate a new one. */
                ret = GenTextures(tc, &glr->width, &glr->height, &glr->texture);
                if (ret != VLC_SUCCESS)
                    continue;
            }
            ret = tc->pf_update(tc, &glr->texture, &glr->width, &glr->height,
                                r->p_picture, &pixels_offset);
        }
    }
    for (int i = 0; i < last_count; i++) {
        if (last[i].texture)
            DelTextures(tc, &last[i].texture);
    }
    free(last);

    VLC_UNUSED(subpicture);

    GL_ASSERT_NOERROR();
    return ret;
}

static int BuildSphere(unsigned nbPlanes,
                        GLfloat **vertexCoord, GLfloat **textureCoord, unsigned *nbVertices,
                        GLushort **indices, unsigned *nbIndices,
                        const float *left, const float *top,
                        const float *right, const float *bottom)
{
    unsigned nbLatBands = 128;
    unsigned nbLonBands = 128;

    *nbVertices = (nbLatBands + 1) * (nbLonBands + 1);
    *nbIndices = nbLatBands * nbLonBands * 3 * 2;

    *vertexCoord = vlc_alloc(*nbVertices * 3, sizeof(GLfloat));
    if (*vertexCoord == NULL)
        return VLC_ENOMEM;
    *textureCoord = vlc_alloc(nbPlanes * *nbVertices * 2, sizeof(GLfloat));
    if (*textureCoord == NULL)
    {
        free(*vertexCoord);
        return VLC_ENOMEM;
    }
    *indices = vlc_alloc(*nbIndices, sizeof(GLushort));
    if (*indices == NULL)
    {
        free(*textureCoord);
        free(*vertexCoord);
        return VLC_ENOMEM;
    }

    for (unsigned lat = 0; lat <= nbLatBands; lat++) {
        float theta = lat * (float) M_PI / nbLatBands;
        float sinTheta, cosTheta;

        sincosf(theta, &sinTheta, &cosTheta);

        for (unsigned lon = 0; lon <= nbLonBands; lon++) {
            float phi = lon * 2 * (float) M_PI / nbLonBands;
            float sinPhi, cosPhi;

            sincosf(phi, &sinPhi, &cosPhi);

            float x = cosPhi * sinTheta;
            float y = cosTheta;
            float z = sinPhi * sinTheta;

            unsigned off1 = (lat * (nbLonBands + 1) + lon) * 3;
            (*vertexCoord)[off1] = SPHERE_RADIUS * x;
            (*vertexCoord)[off1 + 1] = SPHERE_RADIUS * y;
            (*vertexCoord)[off1 + 2] = SPHERE_RADIUS * z;

            for (unsigned p = 0; p < nbPlanes; ++p)
            {
                unsigned off2 = (p * (nbLatBands + 1) * (nbLonBands + 1)
                                + lat * (nbLonBands + 1) + lon) * 2;
                float width = right[p] - left[p];
                float height = bottom[p] - top[p];
                float u = (float)lon / nbLonBands * width;
                float v = (float)lat / nbLatBands * height;
                (*textureCoord)[off2] = u;
                (*textureCoord)[off2 + 1] = v;
            }
        }
    }

    for (unsigned lat = 0; lat < nbLatBands; lat++) {
        for (unsigned lon = 0; lon < nbLonBands; lon++) {
            unsigned first = (lat * (nbLonBands + 1)) + lon;
            unsigned second = first + nbLonBands + 1;

            unsigned off = (lat * nbLatBands + lon) * 3 * 2;

            (*indices)[off] = first;
            (*indices)[off + 1] = second;
            (*indices)[off + 2] = first + 1;

            (*indices)[off + 3] = second;
            (*indices)[off + 4] = second + 1;
            (*indices)[off + 5] = first + 1;
        }
    }

    return VLC_SUCCESS;
}


static int BuildCube(unsigned nbPlanes,
                     float padW, float padH,
                     GLfloat **vertexCoord, GLfloat **textureCoord, unsigned *nbVertices,
                     GLushort **indices, unsigned *nbIndices,
                     const float *left, const float *top,
                     const float *right, const float *bottom)
{
    *nbVertices = 4 * 6;
    *nbIndices = 6 * 6;

    *vertexCoord = vlc_alloc(*nbVertices * 3, sizeof(GLfloat));
    if (*vertexCoord == NULL)
        return VLC_ENOMEM;
    *textureCoord = vlc_alloc(nbPlanes * *nbVertices * 2, sizeof(GLfloat));
    if (*textureCoord == NULL)
    {
        free(*vertexCoord);
        return VLC_ENOMEM;
    }
    *indices = vlc_alloc(*nbIndices, sizeof(GLushort));
    if (*indices == NULL)
    {
        free(*textureCoord);
        free(*vertexCoord);
        return VLC_ENOMEM;
    }

    static const GLfloat coord[] = {
        -1.0,    1.0,    -1.0f, // front
        -1.0,    -1.0,   -1.0f,
        1.0,     1.0,    -1.0f,
        1.0,     -1.0,   -1.0f,

        -1.0,    1.0,    1.0f, // back
        -1.0,    -1.0,   1.0f,
        1.0,     1.0,    1.0f,
        1.0,     -1.0,   1.0f,

        -1.0,    1.0,    -1.0f, // left
        -1.0,    -1.0,   -1.0f,
        -1.0,     1.0,    1.0f,
        -1.0,     -1.0,   1.0f,

        1.0f,    1.0,    -1.0f, // right
        1.0f,   -1.0,    -1.0f,
        1.0f,   1.0,     1.0f,
        1.0f,   -1.0,    1.0f,

        -1.0,    -1.0,    1.0f, // bottom
        -1.0,    -1.0,   -1.0f,
        1.0,     -1.0,    1.0f,
        1.0,     -1.0,   -1.0f,

        -1.0,    1.0,    1.0f, // top
        -1.0,    1.0,   -1.0f,
        1.0,     1.0,    1.0f,
        1.0,     1.0,   -1.0f,
    };

    memcpy(*vertexCoord, coord, *nbVertices * 3 * sizeof(GLfloat));

    for (unsigned p = 0; p < nbPlanes; ++p)
    {
        float width = right[p] - left[p];
        float height = bottom[p] - top[p];

        float col[] = {left[p],
                       left[p] + width * 1.f/3,
                       left[p] + width * 2.f/3,
                       left[p] + width};

        float row[] = {top[p],
                       top[p] + height * 1.f/2,
                       top[p] + height};

        const GLfloat tex[] = {
            col[1] + padW, row[1] + padH, // front
            col[1] + padW, row[2] - padH,
            col[2] - padW, row[1] + padH,
            col[2] - padW, row[2] - padH,

            col[3] - padW, row[1] + padH, // back
            col[3] - padW, row[2] - padH,
            col[2] + padW, row[1] + padH,
            col[2] + padW, row[2] - padH,

            col[2] - padW, row[0] + padH, // left
            col[2] - padW, row[1] - padH,
            col[1] + padW, row[0] + padH,
            col[1] + padW, row[1] - padH,

            col[0] + padW, row[0] + padH, // right
            col[0] + padW, row[1] - padH,
            col[1] - padW, row[0] + padH,
            col[1] - padW, row[1] - padH,

            col[0] + padW, row[2] - padH, // bottom
            col[0] + padW, row[1] + padH,
            col[1] - padW, row[2] - padH,
            col[1] - padW, row[1] + padH,

            col[2] + padW, row[0] + padH, // top
            col[2] + padW, row[1] - padH,
            col[3] - padW, row[0] + padH,
            col[3] - padW, row[1] - padH,
        };

        memcpy(*textureCoord + p * *nbVertices * 2, tex,
               *nbVertices * 2 * sizeof(GLfloat));
    }

    const GLushort ind[] = {
        0, 1, 2,       2, 1, 3, // front
        6, 7, 4,       4, 7, 5, // back
        10, 11, 8,     8, 11, 9, // left
        12, 13, 14,    14, 13, 15, // right
        18, 19, 16,    16, 19, 17, // bottom
        20, 21, 22,    22, 21, 23, // top
    };

    memcpy(*indices, ind, *nbIndices * sizeof(GLushort));

    return VLC_SUCCESS;
}


static int BuildVirtualScreen(unsigned nbPlanes,
                              GLfloat **vertexCoord, GLfloat **textureCoord, unsigned *nbVertices,
                              GLushort **indices, unsigned *nbIndices,
                              const float *left, const float *top,
                              const float *right, const float *bottom,
                              float f_ar, float screenSize,
                              float *screenPosition, float *screenNormalDir, float *screenFitDir)
{
    *nbVertices = 4;
    *nbIndices = 6;

    *vertexCoord = malloc(*nbVertices * 3 * sizeof(GLfloat));
    if (*vertexCoord == NULL)
        return VLC_ENOMEM;
    *textureCoord = malloc(nbPlanes * *nbVertices * 2 * sizeof(GLfloat));
    if (*textureCoord == NULL)
    {
        free(*vertexCoord);
        return VLC_ENOMEM;
    }
    *indices = malloc(*nbIndices * sizeof(GLushort));
    if (*indices == NULL)
    {
        free(*textureCoord);
        free(*vertexCoord);
        return VLC_ENOMEM;
    }

    /*
     * +----------------------------------------+
     * |                         virtual screen |
     * |                 /\                     |
     * |                 |                      |
     * |                 | screenFitDir         |
     * |                 |                      |
     * |   <-------------x screenNormalDir      |
     * |         dir                            |
     * |                                        |
     * +----------------------------------------+
     *
     */

    float hss = screenSize / 2.f; // Half screen size
    // Calculate the cross-product to get the third direction.
    float dir[3] = {(screenNormalDir[1] * screenFitDir[2]) - (screenNormalDir[2] * screenFitDir[1]),
                    -((screenNormalDir[0] * screenFitDir[2]) - (screenNormalDir[2] * screenFitDir[0])),
                    (screenNormalDir[0] * screenFitDir[1]) - (screenNormalDir[1] * screenFitDir[0])};

    const GLfloat coord[] = {
        screenPosition[0] + dir[0] * hss * f_ar + screenFitDir[0] * hss,
        screenPosition[1] + dir[1] * hss * f_ar + screenFitDir[1] * hss,
        screenPosition[2] + dir[2] * hss * f_ar + screenFitDir[2] * hss,

        screenPosition[0] + dir[0] * hss * f_ar - screenFitDir[0] * hss,
        screenPosition[1] + dir[1] * hss * f_ar - screenFitDir[1] * hss,
        screenPosition[2] + dir[2] * hss * f_ar - screenFitDir[2] * hss,

        screenPosition[0] - dir[0] * hss * f_ar + screenFitDir[0] * hss,
        screenPosition[1] - dir[1] * hss * f_ar + screenFitDir[1] * hss,
        screenPosition[2] - dir[2] * hss * f_ar + screenFitDir[2] * hss,

        screenPosition[0] - dir[0] * hss * f_ar - screenFitDir[0] * hss,
        screenPosition[1] - dir[1] * hss * f_ar - screenFitDir[1] * hss,
        screenPosition[2] - dir[2] * hss * f_ar - screenFitDir[2] * hss,
    };

    memcpy(*vertexCoord, coord, *nbVertices * 3 * sizeof(GLfloat));

    for (unsigned p = 0; p < nbPlanes; ++p)
    {
        const GLfloat tex[] = {
            left[p],  top[p],
            left[p],  bottom[p],
            right[p], top[p],
            right[p], bottom[p]
        };

        memcpy(*textureCoord + p * *nbVertices * 2, tex,
               *nbVertices * 2 * sizeof(GLfloat));
    }

    const GLushort ind[] = {
        0, 1, 2,
        2, 1, 3,
    };

    memcpy(*indices, ind, *nbIndices * sizeof(GLushort));

    return VLC_SUCCESS;
}


static int BuildRectangle(unsigned nbPlanes,
                          GLfloat **vertexCoord, GLfloat **textureCoord, unsigned *nbVertices,
                          GLushort **indices, unsigned *nbIndices,
                          const float *left, const float *top,
                          const float *right, const float *bottom)
{
    *nbVertices = 4;
    *nbIndices = 6;

    *vertexCoord = vlc_alloc(*nbVertices * 3, sizeof(GLfloat));
    if (*vertexCoord == NULL)
        return VLC_ENOMEM;
    *textureCoord = vlc_alloc(nbPlanes * *nbVertices * 2, sizeof(GLfloat));
    if (*textureCoord == NULL)
    {
        free(*vertexCoord);
        return VLC_ENOMEM;
    }
    *indices = vlc_alloc(*nbIndices, sizeof(GLushort));
    if (*indices == NULL)
    {
        free(*textureCoord);
        free(*vertexCoord);
        return VLC_ENOMEM;
    }

    static const GLfloat coord[] = {
       -1.0,    1.0,    -1.0f,
       -1.0,    -1.0,   -1.0f,
       1.0,     1.0,    -1.0f,
       1.0,     -1.0,   -1.0f
    };

    memcpy(*vertexCoord, coord, *nbVertices * 3 * sizeof(GLfloat));

    for (unsigned p = 0; p < nbPlanes; ++p)
    {
        const GLfloat tex[] = {
            left[p],  top[p],
            left[p],  bottom[p],
            right[p], top[p],
            right[p], bottom[p]
        };

        memcpy(*textureCoord + p * *nbVertices * 2, tex,
               *nbVertices * 2 * sizeof(GLfloat));
    }

    const GLushort ind[] = {
        0, 1, 2,
        2, 1, 3
    };

    memcpy(*indices, ind, *nbIndices * sizeof(GLushort));

    return VLC_SUCCESS;
}

static int SetupCoords(vout_display_opengl_t *vgl,
                       const float *left, const float *top,
                       const float *right, const float *bottom,
                       float f_ar)
{
    GLfloat *vertexCoord, *textureCoord;
    GLushort *indices;
    unsigned nbVertices, nbIndices;

    int i_ret;
    switch (vgl->fmt.projection_mode)
    {
    case PROJECTION_MODE_RECTANGULAR:
        if (vgl->b_sideBySide)
        {
            float screenSize = 2.f;
            float screenPosition[3] = {-2.f, 0.f, 0.f};
            float screenNormalDir[3] = {1.f, 0.f, 0.f};
            float screenFitDir[3] = {0.f, 1.f, 0.f};
            if (vgl->p_objDisplay)
            {
                memcpy(screenPosition, vgl->p_objDisplay->p_scene->screenPosition, sizeof(screenPosition));
                memcpy(screenNormalDir, vgl->p_objDisplay->p_scene->screenNormalDir, sizeof(screenNormalDir));
                memcpy(screenFitDir, vgl->p_objDisplay->p_scene->screenFitDir, sizeof(screenFitDir));
                screenSize = vgl->p_objDisplay->p_scene->screenSize;
            }
            i_ret = BuildVirtualScreen(vgl->prgm->tc->tex_count,
                                       &vertexCoord, &textureCoord, &nbVertices,
                                       &indices, &nbIndices,
                                       left, top, right, bottom, f_ar,
                                       screenSize, screenPosition, screenNormalDir, screenFitDir);
        }
        else
            i_ret = BuildRectangle(vgl->prgm->tc->tex_count,
                                   &vertexCoord, &textureCoord, &nbVertices,
                                   &indices, &nbIndices,
                                   left, top, right, bottom);
        break;
    case PROJECTION_MODE_EQUIRECTANGULAR:
        i_ret = BuildSphere(vgl->prgm->tc->tex_count,
                            &vertexCoord, &textureCoord, &nbVertices,
                            &indices, &nbIndices,
                            left, top, right, bottom);
        break;
    case PROJECTION_MODE_CUBEMAP_LAYOUT_STANDARD:
        i_ret = BuildCube(vgl->prgm->tc->tex_count,
                          (float)vgl->fmt.i_cubemap_padding / vgl->fmt.i_width,
                          (float)vgl->fmt.i_cubemap_padding / vgl->fmt.i_height,
                          &vertexCoord, &textureCoord, &nbVertices,
                          &indices, &nbIndices,
                          left, top, right, bottom);
        break;
    default:
        i_ret = VLC_EGENERIC;
        break;
    }

    if (i_ret != VLC_SUCCESS)
        return i_ret;

    for (unsigned j = 0; j < vgl->prgm->tc->tex_count; j++)
    {
        vgl->vt.BindBuffer(GL_ARRAY_BUFFER, vgl->texture_buffer_object[j]);
        vgl->vt.BufferData(GL_ARRAY_BUFFER, nbVertices * 2 * sizeof(GLfloat),
                           textureCoord + j * nbVertices * 2, GL_STATIC_DRAW);
    }

    vgl->vt.BindBuffer(GL_ARRAY_BUFFER, vgl->vertex_buffer_object);
    vgl->vt.BufferData(GL_ARRAY_BUFFER, nbVertices * 3 * sizeof(GLfloat),
                       vertexCoord, GL_STATIC_DRAW);

    vgl->vt.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, vgl->index_buffer_object);
    vgl->vt.BufferData(GL_ELEMENT_ARRAY_BUFFER, nbIndices * sizeof(GLushort),
                       indices, GL_STATIC_DRAW);

    free(textureCoord);
    free(vertexCoord);
    free(indices);

    vgl->nb_indices = nbIndices;

    return VLC_SUCCESS;
}

static void DrawWithShaders(vout_display_opengl_t *vgl, struct prgm *prgm,
                            side_by_side_eye eye)
{
    opengl_tex_converter_t *tc = prgm->tc;
    tc->pf_prepare_shader(tc, vgl->tex_width, vgl->tex_height, 1.0f);

    for (unsigned j = 0; j < vgl->prgm->tc->tex_count; j++) {
        assert(vgl->texture[j] != 0);
        vgl->vt.ActiveTexture(GL_TEXTURE0+j);
        vgl->vt.BindTexture(tc->tex_target, vgl->texture[j]);

        vgl->vt.BindBuffer(GL_ARRAY_BUFFER, vgl->texture_buffer_object[j]);

        assert(prgm->aloc.MultiTexCoord[j] != -1);
        vgl->vt.EnableVertexAttribArray(prgm->aloc.MultiTexCoord[j]);
        vgl->vt.VertexAttribPointer(prgm->aloc.MultiTexCoord[j], 2, GL_FLOAT,
                                     0, 0, 0);
    }

    vgl->vt.BindBuffer(GL_ARRAY_BUFFER, vgl->vertex_buffer_object);
    vgl->vt.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, vgl->index_buffer_object);
    vgl->vt.EnableVertexAttribArray(prgm->aloc.VertexPosition);
    vgl->vt.VertexAttribPointer(prgm->aloc.VertexPosition, 3, GL_FLOAT, 0, 0, 0);

    if (vgl->b_sideBySide)
    {
        if (eye == LEFT_EYE)
        {
            memcpy(vgl->prgm->var.ModelViewMatrix, vgl->hmd_cfg.modelview.left, 16 * sizeof(float));
            memcpy(vgl->prgm->var.ProjectionMatrix, vgl->hmd_cfg.projection.left, 16 * sizeof(float));
        }
        else if (eye == RIGHT_EYE)
        {
            memcpy(vgl->prgm->var.ModelViewMatrix, vgl->hmd_cfg.modelview.right, 16 * sizeof(float));
            memcpy(vgl->prgm->var.ProjectionMatrix, vgl->hmd_cfg.projection.right, 16 * sizeof(float));
        }
    }

    vgl->vt.UniformMatrix4fv(prgm->uloc.OrientationMatrix, 1, GL_FALSE,
                              prgm->var.OrientationMatrix);
    vgl->vt.UniformMatrix4fv(prgm->uloc.ProjectionMatrix, 1, GL_FALSE,
                              prgm->var.ProjectionMatrix);
    vgl->vt.UniformMatrix4fv(prgm->uloc.ModelViewMatrix, 1, GL_FALSE,
                              prgm->var.ModelViewMatrix);
    vgl->vt.UniformMatrix4fv(prgm->uloc.ZRotMatrix, 1, GL_FALSE,
                              prgm->var.ZRotMatrix);
    vgl->vt.UniformMatrix4fv(prgm->uloc.YRotMatrix, 1, GL_FALSE,
                              prgm->var.YRotMatrix);
    vgl->vt.UniformMatrix4fv(prgm->uloc.XRotMatrix, 1, GL_FALSE,
                              prgm->var.XRotMatrix);
    vgl->vt.UniformMatrix4fv(prgm->uloc.ZoomMatrix, 1, GL_FALSE,
                              prgm->var.ZoomMatrix);
    //vgl->vt.UniformMatrix4fv(prgm->uloc.ObjectTransformMatrix, 1, GL_FALSE,
    //                          prgm->var.ObjectTransformMatrix);
    vgl->vt.UniformMatrix4fv(prgm->uloc.SceneTransformMatrix, 1, GL_FALSE,
                              prgm->var.SceneTransformMatrix);
    vgl->vt.UniformMatrix4fv(prgm->uloc.HeadPositionMatrix, 1, GL_FALSE,
                              prgm->var.HeadPositionMatrix);

    getSbSParams(vgl, prgm, eye);
    vgl->vt.Uniform2fv(prgm->uloc.SbSCoefs, 1, prgm->var.SbSCoefs);
    vgl->vt.Uniform2fv(prgm->uloc.SbSOffsets, 1, prgm->var.SbSOffsets);

    vgl->vt.DrawElements(GL_TRIANGLES, vgl->nb_indices, GL_UNSIGNED_SHORT, 0);
}

static void GetTextureCropParamsForStereo(unsigned i_nbTextures,
                                          const float *stereoCoefs,
                                          const float *stereoOffsets,
                                          float *left, float *top,
                                          float *right, float *bottom)
{
    for (unsigned i = 0; i < i_nbTextures; ++i)
    {
        float f_2eyesWidth = right[i] - left[i];
        left[i] = left[i] + f_2eyesWidth * stereoOffsets[0];
        right[i] = left[i] + f_2eyesWidth * stereoCoefs[0];

        float f_2eyesHeight = bottom[i] - top[i];
        top[i] = top[i] + f_2eyesHeight * stereoOffsets[1];
        bottom[i] = top[i] + f_2eyesHeight * stereoCoefs[1];
    }
}

static void TextureCropForStereo(vout_display_opengl_t *vgl,
                                 float *left, float *top,
                                 float *right, float *bottom)
{
    float stereoCoefs[2];
    float stereoOffsets[2];

    switch (vgl->fmt.multiview_mode)
    {
    case MULTIVIEW_STEREO_TB:
        // Display only the left eye.
        stereoCoefs[0] = 1; stereoCoefs[1] = 0.5;
        stereoOffsets[0] = 0; stereoOffsets[1] = 0;
        GetTextureCropParamsForStereo(vgl->prgm->tc->tex_count,
                                      stereoCoefs, stereoOffsets,
                                      left, top, right, bottom);
        break;
    case MULTIVIEW_STEREO_SBS:
        // Display only the left eye.
        stereoCoefs[0] = 0.5; stereoCoefs[1] = 1;
        stereoOffsets[0] = 0; stereoOffsets[1] = 0;
        GetTextureCropParamsForStereo(vgl->prgm->tc->tex_count,
                                      stereoCoefs, stereoOffsets,
                                      left, top, right, bottom);
        break;
    default:
        break;
    }
}

static void DrawHMDController(vout_display_opengl_t *vgl, side_by_side_eye eye)
{
    // Change the program for overlays
    struct prgm *prgm = vgl->ctl_prgm;
    GLuint program = prgm->id;
    opengl_tex_converter_t *tc = prgm->tc;
    vgl->vt.UseProgram(program);

    vgl->vt.Enable(GL_BLEND);
    vgl->vt.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    vgl->vt.ActiveTexture(GL_TEXTURE0 + 0);
    {
        const GLfloat vertexCoord[] = {
            vgl->hmdCtlPos.f_depth,      vgl->hmdCtlPos.f_top,   vgl->hmdCtlPos.f_right,
            vgl->hmdCtlPos.f_depth,   vgl->hmdCtlPos.f_bottom,   vgl->hmdCtlPos.f_right,
            vgl->hmdCtlPos.f_depth,      vgl->hmdCtlPos.f_top,    vgl->hmdCtlPos.f_left,
            vgl->hmdCtlPos.f_depth,   vgl->hmdCtlPos.f_bottom,    vgl->hmdCtlPos.f_left,
        };

        const GLfloat textureCoord[] = {
            0.0,   0.0,
            0.0,   1.0,
            1.0,   0.0,
            1.0,   1.0,
        };

        assert(vgl->hmd_controller_texture != 0);
        vgl->vt.BindTexture(tc->tex_target, vgl->hmd_controller_texture);

        tc->pf_prepare_shader(tc, &vgl->hmd_controller_width, &vgl->hmd_controller_height, 1.0);

        vgl->vt.BindBuffer(GL_ARRAY_BUFFER, vgl->hmd_controller_buffer_object[0]);
        vgl->vt.BufferData(GL_ARRAY_BUFFER, sizeof(textureCoord), textureCoord, GL_STATIC_DRAW);
        vgl->vt.EnableVertexAttribArray(prgm->aloc.MultiTexCoord[0]);
        vgl->vt.VertexAttribPointer(prgm->aloc.MultiTexCoord[0], 2, GL_FLOAT,
                                     0, 0, 0);

        vgl->vt.BindBuffer(GL_ARRAY_BUFFER, vgl->subpicture_buffer_object[1]);
        vgl->vt.BufferData(GL_ARRAY_BUFFER, sizeof(vertexCoord), vertexCoord, GL_STATIC_DRAW);
        vgl->vt.EnableVertexAttribArray(prgm->aloc.VertexPosition);
        vgl->vt.VertexAttribPointer(prgm->aloc.VertexPosition, 3, GL_FLOAT,
                                     0, 0, 0);

        if (eye == LEFT_EYE)
        {
            memcpy(prgm->var.ModelViewMatrix, vgl->hmd_cfg.modelview.left, 16 * sizeof(float));
            memcpy(prgm->var.ProjectionMatrix, vgl->hmd_cfg.projection.left, 16 * sizeof(float));
        }
        else if (eye == RIGHT_EYE)
        {
            memcpy(prgm->var.ModelViewMatrix, vgl->hmd_cfg.modelview.right, 16 * sizeof(float));
            memcpy(prgm->var.ProjectionMatrix, vgl->hmd_cfg.projection.right, 16 * sizeof(float));
        }

        memcpy(prgm->var.OrientationMatrix, vgl->prgm->var.OrientationMatrix, 16 * sizeof(float));
        memcpy(prgm->var.ZRotMatrix, vgl->prgm->var.ZRotMatrix, 16 * sizeof(float));
        memcpy(prgm->var.YRotMatrix, vgl->prgm->var.YRotMatrix, 16 * sizeof(float));
        memcpy(prgm->var.XRotMatrix, vgl->prgm->var.XRotMatrix, 16 * sizeof(float));
        memcpy(prgm->var.ZoomMatrix, vgl->prgm->var.ZoomMatrix, 16 * sizeof(float));
        memcpy(prgm->var.SceneTransformMatrix, vgl->prgm->var.SceneTransformMatrix, 16 * sizeof(float));
        memcpy(prgm->var.HeadPositionMatrix, vgl->prgm->var.HeadPositionMatrix, 16 * sizeof(float));

        vgl->vt.UniformMatrix4fv(prgm->uloc.OrientationMatrix, 1, GL_FALSE,
                                 prgm->var.OrientationMatrix);
        vgl->vt.UniformMatrix4fv(prgm->uloc.ProjectionMatrix, 1, GL_FALSE,
                                 prgm->var.ProjectionMatrix);
        vgl->vt.UniformMatrix4fv(prgm->uloc.ModelViewMatrix, 1, GL_FALSE,
                                 prgm->var.ModelViewMatrix);
        vgl->vt.UniformMatrix4fv(prgm->uloc.ZRotMatrix, 1, GL_FALSE,
                                 prgm->var.ZRotMatrix);
        vgl->vt.UniformMatrix4fv(prgm->uloc.YRotMatrix, 1, GL_FALSE,
                                 prgm->var.YRotMatrix);
        vgl->vt.UniformMatrix4fv(prgm->uloc.XRotMatrix, 1, GL_FALSE,
                                 prgm->var.XRotMatrix);
        vgl->vt.UniformMatrix4fv(prgm->uloc.ZoomMatrix, 1, GL_FALSE,
                                 prgm->var.ZoomMatrix);
        vgl->vt.UniformMatrix4fv(prgm->uloc.SceneTransformMatrix, 1, GL_FALSE,
                                 prgm->var.SceneTransformMatrix);
        vgl->vt.UniformMatrix4fv(prgm->uloc.HeadPositionMatrix, 1, GL_FALSE,
                                 prgm->var.HeadPositionMatrix);

        float *SbSCoefs = prgm->var.SbSCoefs;
        float *SbSOffsets = prgm->var.SbSOffsets;
        SbSCoefs[0] = 1.f; SbSCoefs[1] = 1.f;
        SbSOffsets[0] = 0.f; SbSOffsets[1] = 0.f;

        vgl->vt.Uniform2fv(prgm->uloc.SbSCoefs, 1, SbSCoefs);
        vgl->vt.Uniform2fv(prgm->uloc.SbSOffsets, 1, SbSOffsets);

        vgl->vt.DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
    vgl->vt.BlendFunc(GL_ONE, GL_ZERO);
    vgl->vt.Disable(GL_BLEND);
}

static bool is_object_visible(scene_object_t *p_object, scene_mesh_t *p_mesh, float *eye_position, float *eye_direction)
{
    float mesh_to_eye[3] =  {
        p_object->transformMatrix[12] - eye_position[0],
        p_object->transformMatrix[13] - eye_position[1],
        p_object->transformMatrix[14] - eye_position[2]
    };

    float distance = mesh_to_eye[0]*mesh_to_eye[0]
        + mesh_to_eye[1]*mesh_to_eye[1]
        + mesh_to_eye[2]*mesh_to_eye[2];

    if (distance < p_mesh->boundingSquareRadius)
        return true;

    float mesh_dot_eye =
        mesh_to_eye[0]*eye_direction[0] +
        mesh_to_eye[1]*eye_direction[1] +
        mesh_to_eye[2]*eye_direction[2];

    return mesh_dot_eye < 0;
}

static void DrawSceneObjects(vout_display_opengl_t *vgl, struct prgm *prgm,
                             side_by_side_eye eye)
{
    GLuint program = prgm->id;
    opengl_tex_converter_t *tc = prgm->tc;
    vgl->vt.UseProgram(program);

    vgl->vt.Enable(GL_DEPTH_TEST);

    scene_t *p_scene = vgl->p_objDisplay->p_scene;
    if (p_scene == NULL)
        return;

    memcpy(prgm->var.SceneTransformMatrix, p_scene->transformMatrix,
           sizeof(p_scene->transformMatrix));
    memcpy(prgm->var.HeadPositionMatrix, p_scene->headPositionMatrix,
           sizeof(p_scene->headPositionMatrix));

    if (vgl->b_sideBySide)
    {
        if (eye == LEFT_EYE)
        {
            memcpy(prgm->var.ModelViewMatrix, vgl->hmd_cfg.modelview.left, 16 * sizeof(float));
            memcpy(prgm->var.ProjectionMatrix, vgl->hmd_cfg.projection.left, 16 * sizeof(float));
        }
        else if (eye == RIGHT_EYE)
        {
            memcpy(prgm->var.ModelViewMatrix, vgl->hmd_cfg.modelview.right, 16 * sizeof(float));
            memcpy(prgm->var.ProjectionMatrix, vgl->hmd_cfg.projection.right, 16 * sizeof(float));
        }
    }

    vgl->vt.UniformMatrix4fv(prgm->uloc.OrientationMatrix, 1, GL_FALSE,
                              prgm->var.OrientationMatrix);
    vgl->vt.UniformMatrix4fv(prgm->uloc.ProjectionMatrix, 1, GL_FALSE,
                              prgm->var.ProjectionMatrix);
    vgl->vt.UniformMatrix4fv(prgm->uloc.ModelViewMatrix, 1, GL_FALSE,
                              prgm->var.ModelViewMatrix);
    vgl->vt.UniformMatrix4fv(prgm->uloc.ZRotMatrix, 1, GL_FALSE,
                              prgm->var.ZRotMatrix);
    vgl->vt.UniformMatrix4fv(prgm->uloc.YRotMatrix, 1, GL_FALSE,
                              prgm->var.YRotMatrix);
    vgl->vt.UniformMatrix4fv(prgm->uloc.XRotMatrix, 1, GL_FALSE,
                              prgm->var.XRotMatrix);
    vgl->vt.UniformMatrix4fv(prgm->uloc.ZoomMatrix, 1, GL_FALSE,
                              prgm->var.ZoomMatrix);
    vgl->vt.UniformMatrix4fv(prgm->uloc.SceneTransformMatrix, 1, GL_FALSE,
                              prgm->var.SceneTransformMatrix);
    vgl->vt.UniformMatrix4fv(prgm->uloc.HeadPositionMatrix, 1, GL_FALSE,
                              prgm->var.HeadPositionMatrix);

    // lights settings
    float scene_ambient[] = { 0.1f, 0.1f, 0.1f };
    vgl->vt.Uniform1i(prgm->uloc.HasLight, GL_TRUE);
    vgl->vt.Uniform1i(prgm->uloc.LightCount, vgl->p_objDisplay->light_count);
    vgl->vt.Uniform3fv(prgm->uloc.SceneAmbient, 1, scene_ambient);
    vgl->vt.Uniform1i(tc->uloc.IsUniformColor, GL_TRUE);

    vgl->vt.Uniform3fv(prgm->uloc.lights.Position, vgl->p_objDisplay->light_count,
                        *vgl->p_objDisplay->lights.position);
    vgl->vt.Uniform3fv(prgm->uloc.lights.Ambient, vgl->p_objDisplay->light_count,
                        *vgl->p_objDisplay->lights.ambient);
    vgl->vt.Uniform3fv(prgm->uloc.lights.Diffuse, vgl->p_objDisplay->light_count,
                        *vgl->p_objDisplay->lights.diffuse);
    vgl->vt.Uniform3fv(prgm->uloc.lights.Specular, vgl->p_objDisplay->light_count,
                        *vgl->p_objDisplay->lights.specular);
    vgl->vt.Uniform1fv(prgm->uloc.lights.Kc, vgl->p_objDisplay->light_count,
                        vgl->p_objDisplay->lights.k_c);
    vgl->vt.Uniform1fv(prgm->uloc.lights.Kl, vgl->p_objDisplay->light_count,
                        vgl->p_objDisplay->lights.k_l);
    vgl->vt.Uniform1fv(prgm->uloc.lights.Kq, vgl->p_objDisplay->light_count,
                        vgl->p_objDisplay->lights.k_q);
    vgl->vt.Uniform3fv(prgm->uloc.lights.Direction, vgl->p_objDisplay->light_count,
                        *vgl->p_objDisplay->lights.direction);
    vgl->vt.Uniform1fv(prgm->uloc.lights.Cutoff, vgl->p_objDisplay->light_count,
                        vgl->p_objDisplay->lights.cutoff);

    getSbSParams(vgl, prgm, eye);
    vgl->vt.Uniform2fv(prgm->uloc.SbSCoefs, 1, prgm->var.SbSCoefs);
    vgl->vt.Uniform2fv(prgm->uloc.SbSOffsets, 1, prgm->var.SbSOffsets);

    // Set the active texture id for the different sampler2D
    vgl->vt.Uniform1i(prgm->uloc.MatDiffuseTex, 0);
    vgl->vt.Uniform1i(prgm->uloc.MatAmbientTex, 1);
    vgl->vt.Uniform1i(prgm->uloc.MatSpecularTex, 2);
    vgl->vt.Uniform1i(prgm->uloc.MatNormalTex, 3);

    vgl->vt.EnableVertexAttribArray(prgm->aloc.ObjectTransformMatrix);
    vgl->vt.EnableVertexAttribArray(prgm->aloc.ObjectTransformMatrix+1);
    vgl->vt.EnableVertexAttribArray(prgm->aloc.ObjectTransformMatrix+2);
    vgl->vt.EnableVertexAttribArray(prgm->aloc.ObjectTransformMatrix+3);

    vgl->vt.VertexAttribDivisor(prgm->aloc.ObjectTransformMatrix, 1);
    vgl->vt.VertexAttribDivisor(prgm->aloc.ObjectTransformMatrix+1, 1);
    vgl->vt.VertexAttribDivisor(prgm->aloc.ObjectTransformMatrix+2, 1);
    vgl->vt.VertexAttribDivisor(prgm->aloc.ObjectTransformMatrix+3, 1);

    float p_eye_pos[3] = {
        vgl->p_objDisplay->p_scene->headPositionMatrix[12],
        vgl->p_objDisplay->p_scene->headPositionMatrix[13],
        vgl->p_objDisplay->p_scene->headPositionMatrix[14]
    };

    float p_eye_dir[3] = {
        -cos(vgl->f_teta),
        -sin(vgl->f_teta),
        0
    };

    unsigned instance_count = 1;
    for (unsigned o = 0; o < p_scene->nObjects; o += instance_count)
    {
        scene_object_t *p_object = p_scene->objects[o];
        scene_mesh_t *p_mesh = p_scene->meshes[p_object->meshId];
        scene_material_t *p_material = p_scene->materials[p_object->textureId];

        /*if (!is_object_visible(p_object, p_mesh, p_eye_pos, p_eye_dir))
        {
            // Skip this instance
            instance_count = 1;
            continue;
        }*/


        unsigned next_object_idx = o;
        scene_object_t* next_object = p_object;

        while(next_object->meshId == p_object->meshId)
        {
            next_object_idx++;
            if (next_object_idx >= p_scene->nObjects)
                break;

            next_object = vgl->p_objDisplay->p_scene->objects[next_object_idx];
            // suboptimal, the next instance can be skipped, but it's more complex
            /*if (!is_object_visible(next_object, p_scene->meshes[next_object->meshId], p_eye_pos, p_eye_dir))
                break;*/
        }

        // count how many instances of this mesh will be rendered at once by openGL
        instance_count = next_object_idx - o;

        vgl->vt.BindBuffer(GL_ARRAY_BUFFER, vgl->p_objDisplay->transform_buffer_object);
        // OpenGL only allows to bind a vertex attrib but we want to bind a vec4.
        // Fortunately we can bind the attrib 4 times with the correct offset and stride
        // and the location will just be the next one for the next column
        vgl->vt.VertexAttribPointer(prgm->aloc.ObjectTransformMatrix, 4, GL_FLOAT,
                GL_FALSE, 16*sizeof(float), (void*) ((16*o + 0) * sizeof(GLfloat)));
        vgl->vt.VertexAttribPointer(prgm->aloc.ObjectTransformMatrix+1, 4, GL_FLOAT,
                GL_FALSE, 16*sizeof(float), (void*) ((16*o + 4) * sizeof(GLfloat)));
        vgl->vt.VertexAttribPointer(prgm->aloc.ObjectTransformMatrix+2, 4, GL_FLOAT,
                GL_FALSE, 16*sizeof(float), (void*) ((16*o + 8) * sizeof(GLfloat)));
        vgl->vt.VertexAttribPointer(prgm->aloc.ObjectTransformMatrix+3, 4, GL_FLOAT,
                GL_FALSE, 16*sizeof(float), (void*) ((16*o + 12) * sizeof(GLfloat)));

        if (p_material->p_baseColorTex != NULL)
        {
            GLsizei i_width = p_material->p_baseColorTex->format.i_width;
            GLsizei i_height = p_material->p_baseColorTex->format.i_height;
            tc->pf_prepare_shader(tc, &i_width, &i_height, 1.0f);
        }

        vgl->vt.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, vgl->p_objDisplay->index_buffer_object[p_object->meshId]);

        vgl->vt.BindTexture(tc->tex_target, vgl->p_objDisplay->texturesBaseColor[p_object->textureId]);
        tc->vt->Uniform1i(tc->uloc.IsUniformColor, GL_FALSE);

        vgl->vt.BindBuffer(GL_ARRAY_BUFFER, vgl->p_objDisplay->texture_buffer_object[p_object->meshId]);
        vgl->vt.EnableVertexAttribArray(prgm->aloc.MultiTexCoord[0]);
        vgl->vt.VertexAttribPointer(prgm->aloc.MultiTexCoord[0], 2, GL_FLOAT, 0, 0, 0);

        vgl->vt.BindBuffer(GL_ARRAY_BUFFER, vgl->p_objDisplay->vertex_buffer_object[p_object->meshId]);
        vgl->vt.EnableVertexAttribArray(prgm->aloc.VertexPosition);
        vgl->vt.VertexAttribPointer(prgm->aloc.VertexPosition, 3, GL_FLOAT, 0, 0, 0);

        vgl->vt.BindBuffer(GL_ARRAY_BUFFER, vgl->p_objDisplay->normal_buffer_object[p_object->meshId]);
        vgl->vt.EnableVertexAttribArray(prgm->aloc.VertexNormal);
        vgl->vt.VertexAttribPointer(prgm->aloc.VertexNormal, 3, GL_FLOAT, 0, 0, 0);

        vgl->vt.BindBuffer(GL_ARRAY_BUFFER, vgl->p_objDisplay->tangent_buffer_object[p_object->meshId]);
        vgl->vt.EnableVertexAttribArray(prgm->aloc.VertexTangent);
        vgl->vt.VertexAttribPointer(prgm->aloc.VertexTangent, 3, GL_FLOAT, 0, 0, 0);

        tc->vt->Uniform1i(tc->uloc.IsUniformColor, GL_TRUE);
        tc->vt->Uniform4f(tc->uloc.UniformColor,
            p_material->diffuse_color[0], p_material->diffuse_color[1], p_material->diffuse_color[2],
            1.f);

        vgl->vt.Uniform3fv(prgm->uloc.MatDiffuse, 1, p_material->diffuse_color);
        vgl->vt.Uniform3fv(prgm->uloc.MatAmbient, 1, p_material->ambient_color);
        //vgl->vt.Uniform1fv(prgm->uloc.MatSpecular, 1, p_material->specular_color);



        if(p_material->p_normalTex != NULL)
        {
            vgl->vt.ActiveTexture(GL_TEXTURE3);
            vgl->vt.BindTexture(GL_TEXTURE_2D, vgl->p_objDisplay->texturesNormal[p_object->textureId]);
        }

        if(p_material->p_baseColorTex != NULL)
        {
            vgl->vt.ActiveTexture(GL_TEXTURE0);
            vgl->vt.BindTexture(GL_TEXTURE_2D, vgl->p_objDisplay->texturesBaseColor[p_object->textureId]);
            vgl->vt.Uniform1i(prgm->uloc.UseDiffuseTexture, GL_TRUE);
        }
        else
        {
            vgl->vt.Uniform1i(prgm->uloc.UseDiffuseTexture, GL_FALSE);
        }

        vgl->vt.Uniform1i(prgm->uloc.UseAmbiantTexture, GL_FALSE);

        //vgl->vt.UniformMatrix4fv(prgm->uloc.ObjectTransformMatrix, 1, GL_FALSE,
        //                          p_object->transformMatrix);

        vgl->vt.DrawElementsInstanced(GL_TRIANGLES, p_mesh->nFaces * 3, GL_UNSIGNED_INT, 0, instance_count);

        GL_ASSERT_NOERROR();
    }
    //vgl->vt.DisableVertexAttribArray(prgm->aloc.ObjectTransformMatrix);
    //vgl->vt.DisableVertexAttribArray(prgm->aloc.ObjectTransformMatrix+1);
    //vgl->vt.DisableVertexAttribArray(prgm->aloc.ObjectTransformMatrix+2);
    //vgl->vt.DisableVertexAttribArray(prgm->aloc.ObjectTransformMatrix+3);

    vgl->vt.VertexAttribDivisor(prgm->aloc.ObjectTransformMatrix, 0);
    vgl->vt.VertexAttribDivisor(prgm->aloc.ObjectTransformMatrix+1, 0);
    vgl->vt.VertexAttribDivisor(prgm->aloc.ObjectTransformMatrix+2, 0);
    vgl->vt.VertexAttribDivisor(prgm->aloc.ObjectTransformMatrix+3, 0);

    //vgl->vt.Disable(GL_DEPTH_TEST);
}

static int drawScene(vout_display_opengl_t *vgl, const video_format_t *source, side_by_side_eye eye)
{
    vgl->vt.UseProgram(vgl->prgm->id);

    if (source->i_x_offset != vgl->last_source.i_x_offset
     || source->i_y_offset != vgl->last_source.i_y_offset
     || source->i_visible_width != vgl->last_source.i_visible_width
     || source->i_visible_height != vgl->last_source.i_visible_height
     || vgl->b_lastSideBySide != vgl->b_sideBySide)
    {
        float left[PICTURE_PLANE_MAX];
        float top[PICTURE_PLANE_MAX];
        float right[PICTURE_PLANE_MAX];
        float bottom[PICTURE_PLANE_MAX];
        const opengl_tex_converter_t *tc = vgl->prgm->tc;
        for (unsigned j = 0; j < tc->tex_count; j++)
        {
            float scale_w = (float)tc->texs[j].w.num / tc->texs[j].w.den
                          / vgl->tex_width[j];
            float scale_h = (float)tc->texs[j].h.num / tc->texs[j].h.den
                          / vgl->tex_height[j];

            /* Warning: if NPOT is not supported a larger texture is
               allocated. This will cause right and bottom coordinates to
               land on the edge of two texels with the texels to the
               right/bottom uninitialized by the call to
               glTexSubImage2D. This might cause a green line to appear on
               the right/bottom of the display.
               There are two possible solutions:
               - Manually mirror the edges of the texture.
               - Add a "-1" when computing right and bottom, however the
               last row/column might not be displayed at all.
            */
            left[j]   = (source->i_x_offset +                       0 ) * scale_w;
            top[j]    = (source->i_y_offset +                       0 ) * scale_h;
            right[j]  = (source->i_x_offset + source->i_visible_width ) * scale_w;
            bottom[j] = (source->i_y_offset + source->i_visible_height) * scale_h;
        }

        if (!vgl->b_sideBySide)
            TextureCropForStereo(vgl, left, top, right, bottom);
        int ret = SetupCoords(vgl, left, top, right, bottom,
                              (float)source->i_visible_width / source->i_visible_height);
        if (ret != VLC_SUCCESS)
            return ret;

        vgl->last_source.i_x_offset = source->i_x_offset;
        vgl->last_source.i_y_offset = source->i_y_offset;
        vgl->last_source.i_visible_width = source->i_visible_width;
        vgl->last_source.i_visible_height = source->i_visible_height;
        vgl->b_lastSideBySide = vgl->b_sideBySide;
    }

    if (vgl->b_sideBySide
        && vgl->fmt.projection_mode == PROJECTION_MODE_RECTANGULAR
        && vgl->p_objDisplay)
        memcpy(vgl->prgm->var.HeadPositionMatrix, vgl->p_objDisplay->p_scene->headPositionMatrix,
               sizeof(vgl->p_objDisplay->p_scene->headPositionMatrix));


    int64_t d_before_shader = mdate();
    DrawWithShaders(vgl, vgl->prgm, eye);
    int64_t delta_draw_with_shader = mdate() - d_before_shader;

    int64_t d_before_hmd_controller = 0;
    int64_t delta_draw_hmd_controller = 0;

    if (vgl->b_show_hmd_controller)
    {
        d_before_hmd_controller = mdate();
        DrawHMDController(vgl, eye);
        delta_draw_hmd_controller = mdate() - d_before_hmd_controller;
    }

    int64_t d_before_scene_objects = 0;
    int64_t delta_draw_scene_objects = 0;
    if (vgl->b_sideBySide
        && vgl->fmt.projection_mode == PROJECTION_MODE_RECTANGULAR
        && vgl->p_objDisplay)
    {
        d_before_scene_objects = mdate();
        DrawSceneObjects(vgl, vgl->scene_prgm, eye);
        delta_draw_scene_objects = mdate() - d_before_scene_objects;
    }
    /* Draw the subpictures */
    // Change the program for overlays
    struct prgm *prgm = vgl->sub_prgm;
    GLuint program = prgm->id;
    opengl_tex_converter_t *tc = prgm->tc;
    vgl->vt.UseProgram(program);

    vgl->vt.Enable(GL_BLEND);
    vgl->vt.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* We need two buffer objects for each region: for vertex and texture coordinates. */
    if (2 * vgl->region_count > vgl->subpicture_buffer_object_count) {
        if (vgl->subpicture_buffer_object_count > 0)
            vgl->vt.DeleteBuffers(vgl->subpicture_buffer_object_count,
                                  vgl->subpicture_buffer_object);
        vgl->subpicture_buffer_object_count = 0;

        int new_count = 2 * vgl->region_count;
        vgl->subpicture_buffer_object = realloc_or_free(vgl->subpicture_buffer_object, new_count * sizeof(GLuint));
        if (!vgl->subpicture_buffer_object)
            return VLC_ENOMEM;

        vgl->subpicture_buffer_object_count = new_count;
        vgl->vt.GenBuffers(vgl->subpicture_buffer_object_count,
                           vgl->subpicture_buffer_object);
    }

    vgl->vt.ActiveTexture(GL_TEXTURE0 + 0);

    int64_t d_before_eye = mdate();

    for (int i = 0; i < vgl->region_count; i++) {
        gl_region_t *glr = &vgl->region[i];
        const GLfloat vertexCoord[] = {
            glr->left,  glr->top,
            glr->left,  glr->bottom,
            glr->right, glr->top,
            glr->right, glr->bottom,
        };
        const GLfloat textureCoord[] = {
            0.0, 0.0,
            0.0, glr->tex_height,
            glr->tex_width, 0.0,
            glr->tex_width, glr->tex_height,
        };

        assert(glr->texture != 0);
        vgl->vt.BindTexture(tc->tex_target, glr->texture);

        tc->pf_prepare_shader(tc, &glr->width, &glr->height, glr->alpha);

        vgl->vt.BindBuffer(GL_ARRAY_BUFFER, vgl->subpicture_buffer_object[2 * i]);
        vgl->vt.BufferData(GL_ARRAY_BUFFER, sizeof(textureCoord), textureCoord, GL_STATIC_DRAW);
        vgl->vt.EnableVertexAttribArray(prgm->aloc.MultiTexCoord[0]);
        vgl->vt.VertexAttribPointer(prgm->aloc.MultiTexCoord[0], 2, GL_FLOAT,
                                     0, 0, 0);

        vgl->vt.BindBuffer(GL_ARRAY_BUFFER, vgl->subpicture_buffer_object[2 * i + 1]);
        vgl->vt.BufferData(GL_ARRAY_BUFFER, sizeof(vertexCoord), vertexCoord, GL_STATIC_DRAW);
        vgl->vt.EnableVertexAttribArray(prgm->aloc.VertexPosition);
        vgl->vt.VertexAttribPointer(prgm->aloc.VertexPosition, 2, GL_FLOAT,
                                     0, 0, 0);

        vgl->vt.UniformMatrix4fv(prgm->uloc.OrientationMatrix, 1, GL_FALSE,
                                  prgm->var.OrientationMatrix);
        vgl->vt.UniformMatrix4fv(prgm->uloc.ProjectionMatrix, 1, GL_FALSE,
                                  prgm->var.ProjectionMatrix);
        vgl->vt.UniformMatrix4fv(prgm->uloc.ModelViewMatrix, 1, GL_FALSE,
                                  prgm->var.ModelViewMatrix);
        vgl->vt.UniformMatrix4fv(prgm->uloc.ZRotMatrix, 1, GL_FALSE,
                                  prgm->var.ZRotMatrix);
        vgl->vt.UniformMatrix4fv(prgm->uloc.YRotMatrix, 1, GL_FALSE,
                                  prgm->var.YRotMatrix);
        vgl->vt.UniformMatrix4fv(prgm->uloc.XRotMatrix, 1, GL_FALSE,
                                  prgm->var.XRotMatrix);
        vgl->vt.UniformMatrix4fv(prgm->uloc.ZoomMatrix, 1, GL_FALSE,
                                  prgm->var.ZoomMatrix);
        //vgl->vt.UniformMatrix4fv(prgm->uloc.ObjectTransformMatrix, 1, GL_FALSE,
        //                          prgm->var.ObjectTransformMatrix);
        vgl->vt.UniformMatrix4fv(prgm->uloc.SceneTransformMatrix, 1, GL_FALSE,
                                  prgm->var.SceneTransformMatrix);
        vgl->vt.UniformMatrix4fv(prgm->uloc.HeadPositionMatrix, 1, GL_FALSE,
                                  prgm->var.HeadPositionMatrix);

        getSbSParams(vgl, prgm, eye);
        vgl->vt.Uniform2fv(prgm->uloc.SbSCoefs, 1, prgm->var.SbSCoefs);
        vgl->vt.Uniform2fv(prgm->uloc.SbSOffsets, 1, prgm->var.SbSOffsets);


        vgl->vt.DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    int64_t delta_draw_eye = mdate() - d_before_eye;

    printf("== time ==\n"
           " DrawWithShader:     %"PRId64"\n"
           " DrawHMDcontroller:  %"PRId64"\n"
           " DrawSceneObject:    %"PRId64"\n"
           " DrawEyes:           %"PRId64"\n",
           delta_draw_with_shader,
           delta_draw_hmd_controller,
           delta_draw_scene_objects,
           delta_draw_eye);




    vgl->vt.Disable(GL_BLEND);

    return VLC_SUCCESS;
}


int vout_display_opengl_Display(vout_display_opengl_t *vgl,
                                const video_format_t *source)
{
    GL_ASSERT_NOERROR();

    static int64_t i_last_time = 0;

    int64_t i_current_time = mdate();
    int64_t delta = (i_current_time - i_last_time);
    float fps = 1000.f * 1000.f / delta;
    i_last_time = i_current_time;

    fprintf(stderr, "FPS: %f, Delta (µs): %"PRId64" \n", fps, delta);

    /* Why drawing here and not in Render()? Because this way, the
       OpenGL providers can call vout_display_opengl_Display to force redraw.
       Currently, the OS X provider uses it to get a smooth window resizing */
    vgl->vt.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (vgl->b_sideBySide) {
        // Draw scene into framebuffers.
        vgl->vt.UseProgram(vgl->prgm->id);

        // Left eye
        int64_t d_before_scene = mdate();
        vgl->vt.BindFramebuffer(GL_FRAMEBUFFER, vgl->leftFBO);
        vgl->vt.Viewport(0, 0, vgl->i_displayWidth / 2.f, vgl->i_displayHeight);
        vgl->vt.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        drawScene(vgl, source, LEFT_EYE);
        d_before_scene = mdate() - d_before_scene;

        printf(" DrawScene1:     %"PRId64"\n", d_before_scene);

        // Right eye
        d_before_scene = mdate();
        vgl->vt.BindFramebuffer(GL_FRAMEBUFFER, vgl->rightFBO);
        vgl->vt.Viewport(0, 0, vgl->i_displayWidth / 2.f, vgl->i_displayHeight);
        vgl->vt.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        drawScene(vgl, source, RIGHT_EYE);
        d_before_scene = mdate() - d_before_scene;

        printf(" DrawScene2:     %"PRId64"\n", d_before_scene);

        // Exit framebuffer.
        vgl->vt.BindFramebuffer(GL_FRAMEBUFFER, 0);
        vgl->vt.Viewport(0, 0, vgl->i_displayWidth, vgl->i_displayHeight);

        vgl->vt.ActiveTexture(GL_TEXTURE0);

        GLuint program = vgl->stereo_prgm->id;
        vgl->vt.UseProgram(program);

        // Draw eyes.
        GLfloat vertexCoordLeft[] = {
            -1, -1, 0,
            0, -1, 0,
            0, 1, 0,
            -1, 1, 0,
        };

        GLfloat vertexCoordRight[] = {
            0, -1, 0,
            1, -1, 0,
            1,  1, 0,
            0,  1, 0,
        };

        GLushort indices[] = {
            0, 1, 2,
            0, 2, 3,
        };

        GLfloat textureCoord[] = {
            0, 0,
            1, 0,
            1, 1,
            0, 1,
        };
        int64_t d_last_draw = mdate();

        vgl->vt.Uniform1f(vgl->vt.GetUniformLocation(program, "WarpScale"), vgl->hmd_cfg.warpScale * vgl->hmd_cfg.warpAdj);
        vgl->vt.Uniform4fv(vgl->vt.GetUniformLocation(program, "HmdWarpParam"), 1, vgl->hmd_cfg.distorsionCoefs);

        vgl->vt.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, vgl->index_buffer_object_stereo);
        vgl->vt.BufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        vgl->vt.BindBuffer(GL_ARRAY_BUFFER, vgl->texture_buffer_object_stereo);
        vgl->vt.BufferData(GL_ARRAY_BUFFER, sizeof(textureCoord), textureCoord, GL_STATIC_DRAW);
        vgl->vt.EnableVertexAttribArray(vgl->vt.GetAttribLocation(program, "MultiTexCoord0"));
        vgl->vt.VertexAttribPointer(vgl->vt.GetAttribLocation(program, "MultiTexCoord0"), 2, GL_FLOAT, 0, 0, 0);

        // Left eye
        vgl->vt.Uniform2fv(vgl->vt.GetUniformLocation(program, "LensCenter"), 1, vgl->hmd_cfg.leftLensCenter);

        vgl->vt.BindTexture(GL_TEXTURE_2D, vgl->leftColorTex);
        vgl->vt.BindBuffer(GL_ARRAY_BUFFER, vgl->vertex_buffer_object_stereo);
        vgl->vt.BufferData(GL_ARRAY_BUFFER, sizeof(vertexCoordLeft), vertexCoordLeft, GL_STATIC_DRAW);
        vgl->vt.EnableVertexAttribArray(vgl->vt.GetAttribLocation(program, "VertexPosition"));
        vgl->vt.VertexAttribPointer(vgl->vt.GetAttribLocation(program, "VertexPosition"), 3, GL_FLOAT, 0, 0, 0);

        vgl->vt.DrawElements(GL_TRIANGLES, sizeof(indices) / sizeof(GLushort), GL_UNSIGNED_SHORT, 0);

        // Right eye
        vgl->vt.Uniform2fv(vgl->vt.GetUniformLocation(program, "LensCenter"), 1, vgl->hmd_cfg.rightLensCenter);

        vgl->vt.BindTexture(GL_TEXTURE_2D, vgl->rightColorTex);
        vgl->vt.BindBuffer(GL_ARRAY_BUFFER, vgl->vertex_buffer_object_stereo);
        vgl->vt.BufferData(GL_ARRAY_BUFFER, sizeof(vertexCoordRight), vertexCoordRight, GL_STATIC_DRAW);
        vgl->vt.EnableVertexAttribArray(vgl->vt.GetAttribLocation(program, "VertexPosition"));
        vgl->vt.VertexAttribPointer(vgl->vt.GetAttribLocation(program, "VertexPosition"), 3, GL_FLOAT, 0, 0, 0);

        vgl->vt.DrawElements(GL_TRIANGLES, sizeof(indices) / sizeof(GLushort), GL_UNSIGNED_SHORT, 0);

        d_last_draw = mdate() - d_last_draw;
        printf(" LastDraw:     %"PRId64"\n", d_last_draw);

    }
    else
        drawScene(vgl, source, UNDEFINED_EYE);

    /* Display */
    int64_t d_swap_buffer = mdate();
    vlc_gl_Swap(vgl->gl);
    d_swap_buffer = mdate() - d_swap_buffer;

    printf(" SwapBuffer:     %"PRId64"\n", d_swap_buffer);

    GL_ASSERT_NOERROR();

    return VLC_SUCCESS;
}


int vout_display_opengl_ChangeHMDConfiguration(vout_display_opengl_t *vgl, const vout_hmd_cfg_t *p_hmd_cfg)
{
    if (p_hmd_cfg->b_HMDEnabled)
    {
        vgl->hmd_cfg = *p_hmd_cfg;
        vgl->b_sideBySide = true;

        vgl->vt.UseProgram(vgl->stereo_prgm->id);

        vgl->vt.Uniform2fv(vgl->vt.GetUniformLocation(vgl->stereo_prgm->id, "ViewportScale"), 1, vgl->hmd_cfg.viewportScale);
        vgl->vt.Uniform3fv(vgl->vt.GetUniformLocation(vgl->stereo_prgm->id, "aberr"), 1, vgl->hmd_cfg.aberrScale);
    }
    else
        vgl->b_sideBySide = false;

    vout_display_opengl_Viewport(vgl, vgl->i_displayX, vgl->i_displayY,
                                 vgl->i_displayWidth, vgl->i_displayHeight);
    getViewpointMatrixes(vgl, vgl->fmt.projection_mode, vgl->prgm);
    getViewpointMatrixes(vgl, vgl->fmt.projection_mode, vgl->scene_prgm);

    return VLC_SUCCESS;
}

int vout_display_opengl_UpdateHMDControllerPicture(vout_display_opengl_t *vgl,
                                                   vlc_hmd_controller_t *p_ctl)
{
    // Change the HMD controller picture.

    opengl_tex_converter_t *tc = vgl->ctl_prgm->tc;

    int ret = VLC_SUCCESS;
    vgl->hmdCtlPos = p_ctl->pos;

    if (p_ctl->b_visible)
    {
        picture_t *p_pic = p_ctl->p_pic;

        vgl->hmd_controller_width = p_pic->format.i_width;
        vgl->hmd_controller_height = p_pic->format.i_height;

        if (vgl->hmd_controller_texture == 0)
        {
            vgl->vt.GenBuffers(2, vgl->hmd_controller_buffer_object);
            GenTextures(vgl->ctl_prgm->tc, &vgl->hmd_controller_width,
                        &vgl->hmd_controller_height, &vgl->hmd_controller_texture);
        }

        ret = tc->pf_update(tc, &vgl->hmd_controller_texture,
                            &vgl->hmd_controller_width, &vgl->hmd_controller_height,
                            p_pic, NULL);
        vgl->b_show_hmd_controller = true;
    }
    else
    {
        vgl->b_show_hmd_controller = false;
    }

    return ret;
}
