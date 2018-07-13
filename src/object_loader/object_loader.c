
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc_object_loader.h>
#include <vlc_vout.h>
#include <vlc_block.h>

#include "../libvlc.h"


object_loader_t *objLoader_get(vlc_object_t *p_parent)
{
    object_loader_t *p_objLoader = vlc_custom_create(p_parent, sizeof(*p_objLoader), "Object loader");
    if (unlikely(p_objLoader == NULL))
        return NULL;

    p_objLoader->p_module = module_need(p_objLoader, "object loader", NULL, false);
    if (unlikely(p_objLoader->p_module == NULL))
    {
        vlc_object_release(p_objLoader);
        return NULL;
    }

    p_objLoader->p_imgHandler = image_HandlerCreate(p_objLoader);
    if (unlikely(p_objLoader->p_imgHandler == NULL))
    {
        module_unneed(p_objLoader, p_objLoader->p_module);
        vlc_object_release(p_objLoader);
        return NULL;
    }

    video_format_Init(&p_objLoader->texPic_fmt_in, 0);
    video_format_Init(&p_objLoader->texPic_fmt_out, VLC_CODEC_RGBA);

    return p_objLoader;
}


void objLoader_release(object_loader_t* p_objLoader)
{
    video_format_Clean(&p_objLoader->texPic_fmt_in);
    video_format_Clean(&p_objLoader->texPic_fmt_out);

    module_unneed(p_objLoader, p_objLoader->p_module);
    vlc_object_release(p_objLoader);
}


scene_t *objLoader_loadScene(object_loader_t *p_objLoader, const char *psz_path)
{
    if (p_objLoader->loadScene == NULL)
        return NULL;

    return p_objLoader->loadScene(p_objLoader, psz_path);
}


scene_object_t *scene_object_New(float *transformMatrix, unsigned meshId,
                                 unsigned textureId)
{
    scene_object_t *p_object = (scene_object_t *)malloc(sizeof(scene_object_t));
    if (unlikely(p_object == NULL))
        return NULL;

    memcpy(p_object->transformMatrix, transformMatrix, sizeof(p_object->transformMatrix));
    p_object->meshId = meshId;
    p_object->textureId = textureId;

    float vec_length(float* vec)
    {
        return sqrt(vec[0]*vec[0] + vec[1]*vec[1] + vec[2]*vec[2]);
    }

    float scale[] =
    {
        vec_length(&transformMatrix[0]),
        vec_length(&transformMatrix[4]),
        vec_length(&transformMatrix[8])
    };
    memcpy(p_object->scale, scale, sizeof(scale));

    return p_object;
}


void scene_object_Release(scene_object_t *p_object)
{
    free(p_object);
}

static void ComputeBoundingBox(scene_mesh_t *p_mesh, float* bb)
{
    if (p_mesh->nVertices <= 0) return;

    // initialize with first coordinate so we always have valid values
    bb[0] = bb[1] = p_mesh->vCoords[0];
    bb[2] = bb[3] = p_mesh->vCoords[1];
    bb[4] = bb[5] = p_mesh->vCoords[2];

    for (unsigned i = 1; i< p_mesh->nVertices; ++i)
    {
        bb[0] = fmin(bb[0], p_mesh->vCoords[3*i]);
        bb[1] = fmax(bb[1], p_mesh->vCoords[3*i]);
        bb[2] = fmin(bb[2], p_mesh->vCoords[3*i+1]);
        bb[3] = fmax(bb[3], p_mesh->vCoords[3*i+1]);
        bb[4] = fmin(bb[4], p_mesh->vCoords[3*i+2]);
        bb[5] = fmax(bb[5], p_mesh->vCoords[3*i+2]);
    }
}

float scene_mesh_computeBoundingSquareRadius(scene_mesh_t *p_mesh)
{
    float boundings[6];
    ComputeBoundingBox(p_mesh, boundings);

    float xrad = abs(boundings[1] - boundings[0]);
    float yrad = abs(boundings[2] - boundings[1]);
    float zrad = abs(boundings[4] - boundings[3]);

    float radius = sqrt(2) / 2 * fmax(xrad, fmax(yrad, zrad));
    float center[] =
    {
        (boundings[0] + boundings[1]) / 2,
        (boundings[2] + boundings[3]) / 2,
        (boundings[4] + boundings[5]) / 2
    };

    memcpy(p_mesh->center, center, sizeof(center));
    p_mesh->boundingSquareRadius = radius*radius;

    return radius*radius;
}


scene_mesh_t *scene_mesh_New(unsigned nVertices, unsigned nFaces,
                             float *vCoords, float *nCoords, float *tanCoords,
                             float *tCoords, unsigned int *faces)
{
    scene_mesh_t *p_mesh = (scene_mesh_t *)calloc(1, sizeof(scene_mesh_t));
    if (unlikely(p_mesh == NULL))
        return NULL;

    p_mesh->nVertices = nVertices;
    p_mesh->nFaces = nFaces;

    // Allocations.
    p_mesh->vCoords = (float *)malloc(3 * nVertices * sizeof(float));
    if (unlikely(p_mesh->vCoords == NULL))
        goto error;

    p_mesh->nCoords = (float *)malloc(3 * nVertices * sizeof(float));
    if (unlikely(p_mesh->nCoords == NULL))
        goto error;

    p_mesh->tanCoords = (float *)malloc(3 * nVertices * sizeof(float));
    if (unlikely(p_mesh->tanCoords == NULL))
        goto error;

    p_mesh->tCoords = (float *)malloc(2 * nVertices * sizeof(float));
    if (unlikely(p_mesh->tCoords == NULL))
        goto error;

    p_mesh->faces = (unsigned *)malloc(3 * nFaces * sizeof(unsigned));
    if (unlikely(p_mesh->faces == NULL))
        goto error;

    // Copy the mesh data.
    memcpy(p_mesh->vCoords, vCoords, 3 * nVertices * sizeof(float));
    memcpy(p_mesh->nCoords, nCoords, 3 * nVertices * sizeof(float));
    memcpy(p_mesh->tanCoords, tanCoords, 3 * nVertices * sizeof(float));
    if (tCoords != NULL) // A mesh can have no texture coordinates
        memcpy(p_mesh->tCoords, tCoords, 2 * nVertices * sizeof(float));
    memcpy(p_mesh->faces, faces, 3 * nFaces * sizeof(unsigned));

    scene_mesh_computeBoundingSquareRadius(p_mesh);

    return p_mesh;
error:
    free(p_mesh->faces);
    free(p_mesh->tCoords);
    free(p_mesh->tanCoords);
    free(p_mesh->nCoords);
    free(p_mesh->vCoords);
    free(p_mesh);
    return NULL;
}


void scene_mesh_Release(scene_mesh_t *p_mesh)
{
    free(p_mesh->faces);
    free(p_mesh->tCoords);
    free(p_mesh->tanCoords);
    free(p_mesh->nCoords);
    free(p_mesh->vCoords);
    free(p_mesh);
}


scene_material_t *scene_material_New(void)
{
    scene_material_t *p_material = (scene_material_t *)calloc(1, sizeof(scene_material_t));
    if (unlikely(p_material == NULL))
        return NULL;

    return p_material;
}


picture_t *scene_material_LoadTexture(object_loader_t *p_loader, const char *psz_path)
{
    char *psz_url = vlc_path2uri(psz_path, NULL);
    picture_t *p_pic = image_ReadUrl(p_loader->p_imgHandler, psz_url,
                                     &p_loader->texPic_fmt_in,
                                     &p_loader->texPic_fmt_out);
    free(psz_url);

    return p_pic;
}


static void embededTextureDataRelease(block_t *block)
{
    // Nothing to do as the object loaded library owns the data.
    VLC_UNUSED(block);
}


picture_t *scene_material_LoadTextureFromData(object_loader_t *p_loader, const char *p_data, size_t size)
{
    block_t dataBlock;

    block_Init(&dataBlock, (void *)p_data, size);
    dataBlock.pf_release = embededTextureDataRelease;

    picture_t *p_pic = image_Read(p_loader->p_imgHandler, &dataBlock,
                                  &p_loader->texPic_fmt_in,
                                  &p_loader->texPic_fmt_out);

    return p_pic;
}


void scene_material_Release(scene_material_t *p_material)
{
    if (p_material->p_baseColorTex)
        picture_Release(p_material->p_baseColorTex);

    if (p_material->p_metalnessTex)
        picture_Release(p_material->p_metalnessTex);

    if (p_material->p_normalTex)
        picture_Release(p_material->p_normalTex);

    if (p_material->p_roughnessTex)
        picture_Release(p_material->p_roughnessTex);

    free(p_material);
}


scene_light_t *scene_light_New(void)
{
    scene_light_t *p_light = (scene_light_t *)calloc(1, sizeof(scene_light_t));
    if (unlikely(p_light == NULL))
        return NULL;

    return p_light;
}


void scene_light_Release(scene_light_t *p_light)
{
    free(p_light);
}


static void matrixMul(float ret[], const float m1[], const float m2[])
{
    for (unsigned i = 0; i < 4; i++)
    {
        for (unsigned j = 0; j < 4; j++)
        {
            float sum = 0;
            for (unsigned k = 0; k < 4; k++) {
                sum = sum + m1[4 * i + k] * m2[4 * k + j];
            }
            ret[4 * i + j] = sum;
        }
    }
}


void scene_CalcTransformationMatrix(scene_t *p_scene, float s, float *rotationAngles)
{
    memset(p_scene->transformMatrix, 0, sizeof(p_scene->transformMatrix));

    // Compute the axis aligned mesh bounding box.
    float min[] = {FLT_MAX, FLT_MAX, FLT_MAX};
    float max[] = {FLT_MIN, FLT_MIN, FLT_MIN};

    for (unsigned m = 0; m < p_scene->nMeshes; ++m)
    {
        scene_mesh_t *p_mesh = p_scene->meshes[m];
        for (unsigned v = 0; v < p_mesh->nVertices; ++v)
        {
            for (unsigned a = 0; a < 3; ++a)
            {
                float val = p_mesh->vCoords[3 * v + a];
                if (val < min[a])
                    min[a] = val;
                if (val > max[a])
                    max[a] = val;
            }
        }
    }

    // Compute the mesh center.
    float c[] = {min[0] + (max[0] - min[0]) / 2.f,
                 min[1] + (max[1] - min[1]) / 2.f,
                 min[2] + (max[2] - min[2]) / 2.f};

    // Set the scene transformation matrix.
    const float t[] = {
    /*         x          y          z         w */
               s,       0.f,       0.f,      0.f,
             0.f,         s,       0.f,      0.f,
             0.f,       0.f,         s,      0.f,
       -s * c[0], -s * c[1], -s * c[2],        1
    };

    float st, ct;

    // Rotation on X
    sincosf(rotationAngles[0] * M_PI / 180.f, &st, &ct);
    const float rotX[] = {
        /*  x    y    z    w */
          1.f, 0.f, 0.f, 0.f,
          0.f,  ct,  st, 0.f,
          0.f, -st,  ct, 0.f,
          0.f, 0.f, 0.f, 1.f
    };

    // Rotation on Y
    sincosf(rotationAngles[1] * M_PI / 180.f, &st, &ct);
    const float rotY[] = {
        /*  x    y    z    w */
           ct, 0.f, -st, 0.f,
          0.f, 1.f, 0.f, 0.f,
           st, 0.f,  ct, 0.f,
          0.f, 0.f, 0.f, 1.f
    };

    // Rotation on Z
    sincosf(rotationAngles[2] * M_PI / 180.f, &st, &ct);
    const float rotZ[] = {
        /*  x    y    z    w */
           ct,  st, 0.f, 0.f,
          -st,  ct, 0.f, 0.f,
          0.f, 0.f, 1.f, 0.f,
          0.f, 0.f, 0.f, 1.f
    };

    float res1[16];
    matrixMul(res1, t, rotX);
    float res2[16];
    matrixMul(res2, res1, rotY);
    float res3[16];
    matrixMul(res3, res2, rotZ);

    memcpy(p_scene->transformMatrix, res3, sizeof(res3));
}


void scene_CalcHeadPositionMatrix(scene_t *p_scene, float *p)
{
    const float m[] = {
        /*   x    y    z    w */
            1.f,   0.f,   0.f, 0.f,
            0.f,   1.f,   0.f, 0.f,
            0.f,   0.f,   1.f, 0.f,
          -p[0], -p[1], -p[2], 1.f
    };

    memcpy(p_scene->headPositionMatrix, m, sizeof(m));
}


scene_t *scene_New(unsigned nObjects, unsigned nMeshes, unsigned nMaterials,
                   unsigned nLights)
{
    scene_t *p_scene = (scene_t *)malloc(sizeof(scene_t));
    if (unlikely(p_scene == NULL))
        return NULL;

    // Allocations.
    p_scene->objects = (scene_object_t **)malloc(nObjects * sizeof(scene_object_t *));
    if (unlikely(p_scene->objects == NULL))
    {
        free(p_scene);
        return NULL;
    }

    p_scene->meshes = (scene_mesh_t **)malloc(nMeshes * sizeof(scene_mesh_t *));
    if (unlikely(p_scene->meshes == NULL))
    {
        free(p_scene->objects);
        free(p_scene);
        return NULL;
    }

    p_scene->materials = (scene_material_t **)malloc(nMaterials * sizeof(scene_material_t *));
    if (unlikely(p_scene->materials == NULL))
    {
        free(p_scene->meshes);
        free(p_scene->objects);
        free(p_scene);
        return NULL;
    }

    p_scene->lights = (scene_light_t **)malloc(nLights * sizeof(scene_light_t *));
    if (unlikely(p_scene->lights == NULL))
    {
        free(p_scene->materials);
        free(p_scene->meshes);
        free(p_scene->objects);
        free(p_scene);
        return NULL;
    }

    p_scene->nObjects = nObjects;
    p_scene->nMeshes = nMeshes;
    p_scene->nMaterials = nMaterials;
    p_scene->nLights = nLights;

    return p_scene;
}


void scene_Release(scene_t *p_scene)
{
    if (p_scene == NULL)
        return;

    for (unsigned i = 0; i < p_scene->nObjects; ++i)
        scene_object_Release(p_scene->objects[i]);
    free(p_scene->objects);

    for (unsigned i = 0; i < p_scene->nMeshes; ++i)
        scene_mesh_Release(p_scene->meshes[i]);
    free(p_scene->meshes);

    for (unsigned i = 0; i < p_scene->nMaterials; ++i)
        scene_material_Release(p_scene->materials[i]);
    free(p_scene->materials);

    for (unsigned i = 0; i < p_scene->nLights; ++i)
        scene_light_Release(p_scene->lights[i]);
    free(p_scene->lights);

    free(p_scene);
}
