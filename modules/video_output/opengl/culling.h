/*****************************************************************************
 * culling.h: 3D Scene culling algorithm
 *****************************************************************************
 * Copyright (C) 2018 VLC authors and VideoLAN
 *
 * Authors: Alexandre Janniaux <alexandre.janniaux@gmail.com>
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

typedef struct {
    float top[3];
    float bottom[3];
    float left[3];
    float right[3];
} frustrum_t;

struct scene_object_t;
struct scene_mesh_t;

void
vec_Add(float dst[3], float a[3], float b[3]);

void
vec_Cross(float dst[3], float a[3], float b[3]);

void
vec_Scale(float dst[3], float vec[3], float scale);

void
vec_LinearXY(float dst[3], float x, float x_vec[3], float y, float y_vex[3]);

float
vec_Dot(float a[3], float b[3]);

float
vec_Normalize(float vec[3]);

/**
 * Initialize a frustrum object from camera's parameters
 *
 * /param frustrum the frustrum structure to build
 * /param up the camera up vector, which must be normalized
 * /param direction the camera direction vector which must be normalized
 * /param fovx the camera horizontal opening angle
 * /param fovy the camera vertical opening angle
 * /param znear the distance to the near plane along the direction vector
 * /param zfar the distance to the far plane along the direction vector
 */
void
culling_BuildFrustrum(frustrum_t *p_frustrum, float up[3], float direction[3], float fovx,
                      float fovy, float znear, float zfar);

/**
 * Determine whether the given object with given mesh is in front of or behind the
 * camera.
 *
 * \param p_object the object to cull
 * \param p_mesh the mesh corresponding to the p_object mesh id
 * \param p_origin any point belonging to the plane
 * \param p_normal the normal of the plane
 * \param tolerance the margin we accept for the plane
 *
 * \return true if the object is in front of the camera, false otherwise
 */
bool
culling_OrientedPlane(scene_object_t *p_object, scene_mesh_t *p_mesh,
                      float p_origin[3], float p_normal[3], float tolerance);

/**
 * Determine whether the given object with given mesh in inside of the given frustrum
 *
 * \param p_frustrum a correctly initialized frustrum object
 * \param p_object the object to cull
 * \param p_mesh the mesh corresponding to the p_object mesh id
 * \param p_origin the position of the camera
 * \param tolerance some wiggle room around the edge of the frustrum
 * \return true if the object is inside of the frustrum, false otherwise
 */
bool
culling_Frustrum(frustrum_t *p_frustrum, scene_object_t *p_object, scene_mesh_t *p_mesh, float p_origin[3],
                 float tolerance);
