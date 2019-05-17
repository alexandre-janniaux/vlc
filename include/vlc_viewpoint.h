/*****************************************************************************
 * vlc_viewpoint.h: viewpoint struct and helpers
 *****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
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

#ifndef VLC_VIEWPOINT_H_
#define VLC_VIEWPOINT_H_ 1

#include <vlc_common.h>

#include <math.h>

/**
 * \file
 * Video and audio viewpoint struct and helpers
 */

#define FIELD_OF_VIEW_DEGREES_DEFAULT  80.f
#define FIELD_OF_VIEW_DEGREES_MAX 150.f
#define FIELD_OF_VIEW_DEGREES_MIN 20.f

/**
 * Viewpoints
 */

struct vlc_viewpoint_t {
    /**
     * orientation quaternion with the following properties:
     *  1/ use ijk = -1 for the operations
     *  2/ memory layout is [x, y, z, w] (like GLSL)
     *  3/ system is right-handed
     */
    float quat[4];
    float fov;   /* field of view in degrees */
};

static inline void vlc_viewpoint_init( vlc_viewpoint_t *p_vp )
{
    p_vp->quat[3] = 1;
    p_vp->quat[0] = p_vp->quat[1] = p_vp->quat[2] = 0;
    p_vp->fov = FIELD_OF_VIEW_DEGREES_DEFAULT;
}

static inline void vlc_viewpoint_clip( vlc_viewpoint_t *p_vp )
{
    // TODO: normalize quaternion
    p_vp->fov = VLC_CLIP( p_vp->fov, FIELD_OF_VIEW_DEGREES_MIN,
                          FIELD_OF_VIEW_DEGREES_MAX );
}

/**
 * Generate the 4x4 transform matrix corresponding to a viewpoint
 *
 * Convert a vlc_viewpoint_t into a 4x4 transform matrix with a column-major
 * layout.
 *
 * \param vp a valid viewpoint object
 * \param matrix a 4x4-sized array which will contain the matrix data
 */
VLC_API
void vlc_viewpoint_to_4x4( const vlc_viewpoint_t *vp, float *matrix );

VLC_API
void vlc_viewpoint_from_euler( vlc_viewpoint_t *vp,
                               float yaw, float pitch, float roll );

VLC_API
void vlc_viewpoint_to_euler( const vlc_viewpoint_t *vp,
                             float *yaw, float *pitch, float *roll );

#endif /* VLC_VIEWPOINT_H_ */
