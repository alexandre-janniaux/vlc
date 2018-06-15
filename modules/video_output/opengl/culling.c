/*****************************************************************************
 * culling.c: 3D Scene culling algorithm
 *****************************************************************************
 * Copyright (C) 2018 VLC authors and VideoLAN
 *
 * author: Alexandre Janniaux <alexandre.janniaux@gmail.com>
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

#include "objects.h"
#include "culling.h"


void vec_Scale(float dst[3], float vec[3], float scale)
{
    dst[0] = vec[0] * scale;
    dst[1] = vec[1] * scale;
    dst[2] = vec[2] * scale;
}

void vec_Add(float dst[3], float a[3], float b[3])
{
    dst[0] = a[0] + b[0];
    dst[1] = a[1] + b[1];
    dst[2] = a[2] + b[2];
}

void vec_Sub(float dst[3], float a[3], float b[3])
{
    dst[0] = a[0] - b[0];
    dst[1] = a[1] - b[1];
    dst[2] = a[2] - b[2];
}

void vec_LinearXY(float dst[3], float x, float x_vec[3], float y, float y_vec[3])
{
    float x_ret[3];
    vec_Scale(x_ret, x_vec, x);

    float y_ret[3];
    vec_Scale(y_ret, y_vec, y);

    vec_Add(dst, x_ret, y_ret);
}

void vec_Cross(float dst[3], float a[3], float b[3])
{
    dst[2] = a[0] * b[1] - a[1] * b[0];
    dst[0] = a[1] * b[2] - a[2] * b[1];
    dst[1] = a[2] * b[0] - a[0] * b[2];
}

float vec_Dot(float a[3], float b[3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

float vec_Normalize(float vec[3])
{
    float length = sqrt(vec[0] * vec[0] + 
                        vec[1] * vec[1] +
                        vec[2] * vec[2]);
    vec[0] /= length;
    vec[1] /= length;
    vec[2] /= length;

    return length;
}

void
culling_BuildFrustrum(frustrum_t *frustrum, float up[3], float direction[3],
                      float fovx, float fovy, float znear, float zfar)
{
    float left[3];
    vec_Cross(left, up, direction);
    vec_Normalize(left);
    vec_Normalize(direction);
    vec_Normalize(up);


    void compute_edge_vector(float dst[3], float hh_near, float hh_far, float hw_near,
                             float hw_far)
    {
        float vec_near[3];
        vec_LinearXY(vec_near, hh_near, up, hw_near, left);
        float vec_far[3];
        vec_LinearXY(vec_far, hh_far, up, hw_far, left);

        dst[0] = vec_far[0] - vec_near[0];
        dst[1] = vec_far[1] - vec_near[1];
        dst[2] = vec_far[2] - vec_near[2];
    }

    float hh_near = znear * tan(fovy);
    float hw_near = znear * tan(fovx);

    float hh_far = zfar * tan(fovy);
    float hw_far = zfar * tan(fovx);

    float lt[3];
    compute_edge_vector(lt, hh_near, hh_far, hw_near, hw_far);
    float lb[3];
    compute_edge_vector(lb, -hh_near, -hh_far, hw_near, hw_far);
    float rt[3];
    compute_edge_vector(rt, hh_near, hh_far, -hw_near, -hw_far);
    float rb[3];
    compute_edge_vector(rb, -hh_near, -hh_far, -hw_near, -hw_far);

    //fprintf(stderr, "lt: %f %f %f\n", lt[0], lt[1], lt[2]);
    //fprintf(stderr, "lb: %f %f %f\n", lb[0], lb[1], lb[2]);
    //fprintf(stderr, "rt: %f %f %f\n", rt[0], rt[1], rt[2]);
    //fprintf(stderr, "rb: %f %f %f\n", rb[0], rb[1], rb[2]);

    vec_Cross(frustrum->left, lb, lt);
    vec_Cross(frustrum->bottom, rb, lb);
    vec_Cross(frustrum->right, rt, rb);
    vec_Cross(frustrum->top, lt, rt);

    vec_Normalize(frustrum->left);
    vec_Normalize(frustrum->bottom);
    vec_Normalize(frustrum->right);
    vec_Normalize(frustrum->top);

    //fprintf(stderr, "top: %f %f %f\n", frustrum->top[0], frustrum->top[1], frustrum->top[2]);
    //fprintf(stderr, "bottom: %f %f %f\n", frustrum->bottom[0], frustrum->bottom[1], frustrum->bottom[2]);
    //fprintf(stderr, "left: %f %f %f\n", frustrum->left[0], frustrum->left[1], frustrum->left[2]);
    //fprintf(stderr, "right: %f %f %f\n", frustrum->right[0], frustrum->right[1], frustrum->right[2]);
}

bool 
culling_OrientedPlane(scene_object_t *p_object, scene_mesh_t *p_mesh, float p_origin[3],
                      float p_normal[3], float tolerance)
{
    float to_obj[3] = {0};
    vec_Add(to_obj, to_obj, &p_object->transformMatrix[12]);
    vec_Add(to_obj, to_obj, p_mesh->center);
    vec_Sub(to_obj, to_obj, p_origin);

    float projection = vec_Dot(p_normal, to_obj);
    //fprintf(stderr, "Projection: %f  / radius: %f\n", projection, p_mesh->boundingSquareRadius / *p_object->scale);

    return projection > -tolerance; //- p_mesh->boundingSquareRadius / *p_object->scale;
}

bool
culling_Frustrum(frustrum_t *p_frustrum, scene_object_t *p_object, scene_mesh_t *p_mesh,
                 float p_origin[3], float tolerance)
{
    assert( p_frustrum->top && p_frustrum->bottom && p_frustrum->left && p_frustrum->right );
    float *normals[] =
    {
        p_frustrum->top,
        p_frustrum->bottom,
        p_frustrum->left,
        p_frustrum->right
    };

    int i_normals = sizeof(normals) / sizeof(float*);

//    fprintf(stderr, "frustrum_normals\n%f %f %f\n%f %f %f\n", 
//            p_frustrum->top[0], p_frustrum->top[1], p_frustrum[2],
//            p_frustrum->bottom[0], p_frustrum->bottom[1], p_frustrum[2]);
    for(int i=0; i<i_normals; ++i)
    {
        if (!culling_OrientedPlane(p_object, p_mesh, p_origin, normals[i], tolerance))
        {
            return false;
        }
    }
    return true;
}
