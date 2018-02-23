/*****************************************************************************
 * assimp.cpp: Assimp 3d object loader
 *****************************************************************************
 * Copyright (C) 2002-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Adrien Maglo <magsoft@videolan.org>
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

#include <assert.h>
#include <vector>
#include <unordered_map>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_object_loader.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *p_this);
static void Close(vlc_object_t *);

#define PREFIX "assimp-"

vlc_module_begin()
    set_shortname(N_("Assimp"))
    set_category(CAT_3D)
    set_subcategory(SUBCAT_3D_OBJECT_LOADER)
    set_description(N_("Assimp 3d object loader"))
    set_capability("object loader", 10)

    add_shortcut("assimp")
    set_callbacks(Open, Close)
vlc_module_end()


scene_t *loadScene(object_loader_t *p_loader, const char *psz_path);


struct object_loader_sys_t
{
    Assimp::Importer importer;
};


static int Open(vlc_object_t *p_this)
{
    object_loader_t *p_loader = (object_loader_t *)p_this;

    p_loader->p_sys = new(std::nothrow) object_loader_sys_t();
    if (unlikely(p_loader->p_sys == NULL))
        return VLC_ENOMEM;

    p_loader->loadScene = loadScene;

    return VLC_SUCCESS;
}


static void Close(vlc_object_t *p_this)
{
    object_loader_t *p_loader = (object_loader_t *)p_this;

    delete p_loader->p_sys;
}


static void getAllNodes(std::vector<aiNode *> &nodes, aiNode *node)
{
    for (unsigned c = 0; c < node->mNumChildren; ++c)
        getAllNodes(nodes, node->mChildren[c]);
    nodes.push_back(node);
}


scene_t *loadScene(object_loader_t *p_loader, const char *psz_path)
{
    object_loader_sys_t *p_sys = p_loader->p_sys;
    scene_t *p_scene = NULL;

    const aiScene *myAiScene = p_sys->importer.ReadFile( psz_path,
        aiProcess_CalcTangentSpace       |
        aiProcess_Triangulate            |
        aiProcess_JoinIdenticalVertices  |
        aiProcess_SortByPType            |
        aiProcess_TransformUVCoords);

    if (!myAiScene)
    {
        msg_Err(p_loader, "%s", p_sys->importer.GetErrorString());
        return NULL;
    }

    // Meshes
    std::vector<scene_mesh_t *> meshes;
    std::unordered_map<unsigned, unsigned> aiMeshMap;
    for (unsigned i = 0; i < myAiScene->mNumMeshes; ++i)
    {
        aiMesh *myAiMesh = myAiScene->mMeshes[i];

        unsigned *facesIndices = new unsigned[3 * myAiMesh->mNumFaces];
        for (unsigned f = 0; f < myAiMesh->mNumFaces; ++f)
        {
            assert(myAiMesh->mFaces[f].mNumIndices == 3);
            memcpy(facesIndices + 3 * f, myAiMesh->mFaces[f].mIndices, 3 * sizeof(*facesIndices));
        }

        float *textureCoords = NULL;
        if (myAiMesh->HasTextureCoords(0) && myAiMesh->mNumUVComponents[0] == 2)
        {
            textureCoords = new float[2 * myAiMesh->mNumVertices];
            for (unsigned v = 0; v < myAiMesh->mNumVertices; ++v)
            {
                textureCoords[2 * v] = myAiMesh->mTextureCoords[0][v].x;
                textureCoords[2 * v + 1] = 1 - myAiMesh->mTextureCoords[0][v].y;
            }
        }

        scene_mesh_t *p_mesh = scene_mesh_New(myAiMesh->mNumVertices, myAiMesh->mNumFaces,
                                              (float *)myAiMesh->mVertices,
                                              textureCoords, facesIndices);
        delete[] facesIndices;
        delete[] textureCoords;

        if (p_mesh == NULL)
        {
            msg_Warn(p_loader, "Could not load the mesh number %d", i);
            continue;
        }

        meshes.push_back(p_mesh);
        aiMeshMap[i] = meshes.size() - 1;
    }

    // Materials
    std::vector<scene_material_t  *> materials;
    std::unordered_map<unsigned, unsigned> aiTextureMap;
    for (unsigned i = 0; i < myAiScene->mNumMaterials; ++i)
    {
        aiMaterial *myAiMaterial = myAiScene->mMaterials[i];

        aiColor3D diffuseColor;
        myAiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor);
        msg_Err(p_loader, "Diffuse color: %f %f %f", diffuseColor.r, diffuseColor.g, diffuseColor.b);

        unsigned i_nbProperties = myAiMaterial->mNumProperties;
        for (unsigned j = 0; j < i_nbProperties; ++j)
        {
            msg_Err(p_loader, "name %s, %s", myAiMaterial->mProperties[j]->mKey.C_Str(), myAiMaterial->mProperties[j]->mData + 4);
        }

        unsigned i_nbTextures = myAiMaterial->GetTextureCount(aiTextureType_DIFFUSE);
        if (i_nbTextures > 0)
        {
            for (unsigned j = 0; j < i_nbTextures; ++j)
            {
                aiString path;
                myAiMaterial->GetTexture(aiTextureType_DIFFUSE, j, &path, NULL, NULL, NULL, NULL, NULL);
                char psz_path[1024];
                #define TEXTURE_DIR "VirtualTheater" DIR_SEP "Textures" DIR_SEP
                strcpy(psz_path, TEXTURE_DIR);
                strcpy(psz_path + strlen(TEXTURE_DIR), path.C_Str() + strlen("..\\..\\sourceimages\\"));

                scene_material_t *p_material = scene_material_New();
                if (p_material == NULL)
                    continue;
                p_material->material_type = MATERIAL_TYPE_TEXTURE;

                int loaded = scene_material_LoadTexture(p_loader, p_material, psz_path);
                if (loaded != VLC_SUCCESS)
                {
                    msg_Warn(p_loader, "Could not load the texture at path %s", psz_path);
                    continue;
                }

                materials.push_back(p_material);
                aiTextureMap[i] = materials.size() - 1;
            }
        }
        else
        {
            scene_material_t *p_material = scene_material_New();
            if (p_material == NULL)
                continue;
            p_material->material_type = MATERIAL_TYPE_DIFFUSE_COLOR;

            p_material->diffuse_color[0] = diffuseColor.r;
            p_material->diffuse_color[1] = diffuseColor.g;
            p_material->diffuse_color[2] = diffuseColor.b;

            materials.push_back(p_material);
            aiTextureMap[i] = materials.size() - 1;
        }


    }

    // Objects
    std::vector<aiNode *> nodes;
    getAllNodes(nodes, myAiScene->mRootNode);

    std::vector<scene_object_t *> objects;
    for (auto it = nodes.begin(); it != nodes.end(); ++it)
    {
        aiNode *node = *it;
        for (unsigned j = 0; j < node->mNumMeshes; ++j)
        {
            unsigned i_mesh = node->mMeshes[j];
            unsigned i_texture = myAiScene->mMeshes[i_mesh]->mMaterialIndex;

            auto texMapIt = aiTextureMap.find(i_texture);
            if (texMapIt == aiTextureMap.end())
            {
                msg_Warn(p_loader, "Could not add the current object as its texture could not be loaded");
                continue;
            }

            auto meshMapIt = aiMeshMap.find(i_mesh);
            if (meshMapIt == aiMeshMap.end())
            {
                msg_Warn(p_loader, "Could not add the current object as its mesh could not be loaded");
                continue;
            }

            unsigned texMap = texMapIt->second;
            unsigned meshMap = meshMapIt->second;
            objects.push_back(scene_object_New((float *)&node->mTransformation,
                                               meshMap, texMap));
        }
    }

    p_scene = scene_New(objects.size(), meshes.size(), materials.size());
    if (unlikely(p_scene == NULL))
        return NULL;

    std::copy(objects.begin(), objects.end(), p_scene->objects);
    std::copy(meshes.begin(), meshes.end(), p_scene->meshes);
    std::copy(materials.begin(), materials.end(), p_scene->materials);

    scene_CalcTransformationMatrix(p_scene);

    msg_Dbg(p_loader, "3D scene loaded with %d object(s), %d mesh(es) and %d texture(s)",
            p_scene->nObjects, p_scene->nMeshes, p_scene->nMaterials);

    return p_scene;
}
