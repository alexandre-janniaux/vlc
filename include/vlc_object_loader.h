#ifndef VLC_OBJECT_LOADER_H
#define VLC_OBJECT_LOADER_H

#include <float.h>

#include <vlc_common.h>
#include <vlc_image.h>
#include <vlc_url.h>


typedef struct object_loader_t object_loader_t;
typedef struct object_loader_sys_t object_loader_sys_t;


typedef struct
{
    float transformMatrix[16];
    unsigned meshId;
    unsigned textureId;
} scene_object_t;


typedef struct
{
    float *vCoords;
    float *tCoords;
    unsigned *faces;

    unsigned nVertices;
    unsigned nFaces;
} scene_mesh_t;


typedef enum material_type_t
{
    MATERIAL_TYPE_TEXTURE,
    MATERIAL_TYPE_DIFFUSE_COLOR
} material_type_t;


typedef struct
{
    char *psz_path;
    picture_t *p_pic;
    material_type_t material_type;

    float diffuse_color[3]; // RGB
} scene_material_t;


typedef struct
{
    scene_object_t **objects;
    scene_mesh_t **meshes;
    scene_material_t **materials;

    float transformMatrix[16];
    float headPositionMatrix[16];

    unsigned nObjects;
    unsigned nMeshes;
    unsigned nMaterials;

    float screenSize;
    float screenPosition[3];
    float screenNormalDir[3];
    float screenFitDir[3];
} scene_t;


struct object_loader_t
{
    struct vlc_common_members obj;

    /* Module */
    module_t* p_module;

    /* Private structure */
    object_loader_sys_t* p_sys;

    scene_t * (*loadScene)(object_loader_t *p_loader, const char *psz_path);
};


VLC_API object_loader_t *objLoader_get(vlc_object_t *p_parent);
VLC_API void objLoader_release(object_loader_t* p_objLoader);
VLC_API scene_t *objLoader_loadScene(object_loader_t *p_objLoader, const char *psz_path);
VLC_API scene_object_t *scene_object_New(float *transformMatrix, unsigned meshId,
                                         unsigned textureId);
VLC_API void scene_object_Release(scene_object_t *p_object);
VLC_API scene_mesh_t *scene_mesh_New(unsigned nVertices, unsigned nFaces,
                                     float *vCoords, float *tCoords,
                                     unsigned int *faces);
VLC_API void scene_mesh_Release(scene_mesh_t *p_mesh);
VLC_API scene_material_t *scene_material_New(void);
VLC_API int scene_material_LoadTexture(object_loader_t *p_loader, scene_material_t *p_material,
                                       const char *psz_path);
VLC_API void scene_material_Release(scene_material_t *p_material);
VLC_API void scene_CalcTransformationMatrix(scene_t *p_scene, float sf, float *rotationAngles);
VLC_API void scene_CalcHeadPositionMatrix(scene_t *p_scene, float *p);
VLC_API scene_t *scene_New(unsigned nObjects, unsigned nMeshes, unsigned nTextures);
VLC_API void scene_Release(scene_t *p_scene);


#endif // VLC_OBJECT_LOADER_H
