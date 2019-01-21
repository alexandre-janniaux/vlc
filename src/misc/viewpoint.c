/*****************************************************************************
 * viewpoint.c: viewpoint helpers for conversions and transformations
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Alexandre Janniaux <ajanni@videolabs.io>
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

#include <vlc_viewpoint.h>

/* Quaternion to/from Euler conversion.
 * Original code from:
 * http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToEuler/ */
static void QuaternionToEuler(float *yaw, float *pitch, float *roll, const float *q)
{
    float sqx = q[0] * q[0];
    float sqy = q[1] * q[1];
    float sqz = q[2] * q[2];
    float sqw = q[3] * q[3];

    // if the quaternion is normalised, unit is one
    // otherwise it is correction factor
    float unit = sqx + sqy + sqz + sqw;
    float test = q[0] * q[1] + q[2] * q[3];

    if (test > 0.499 * unit)
    {
        // singularity at north pole
        *yaw = 2 * atan2(q[0], q[3]);
        *roll = M_PI / 2;
        *pitch = 0;
    }
    else if (test < -0.499 * unit)
    {
        // singularity at south pole
        *yaw = -2 * atan2(q[0], q[3]);
        *roll = -M_PI / 2;
        *pitch = 0;
    }
    else
    {
        *yaw   = atan2(2 * q[1] * q[3] - 2 * q[0] * q[2],
                       sqx - sqy - sqz + sqw);
        *roll  = asin(2 * test / unit);
        *pitch = atan2(2 * q[0] * q[3] - 2 * q[1] * q[2],
                       -sqx + sqy - sqz + sqw);
    }
}

static void EulerToQuaternion(float *q, float yaw, float pitch, float roll)
{
    float c_yaw     = cos(yaw / 2.f);
    float c_pitch   = cos(pitch / 2.f);
    float c_roll    = cos(roll / 2.f);
    float s_yaw     = sin(yaw / 2.f);
    float s_pitch   = sin(pitch / 2.f);
    float s_roll    = sin(roll / 2.f);

    q[0] = c_yaw * c_pitch * c_roll + s_yaw * s_pitch * s_roll;
    q[1] = c_yaw * c_pitch * s_roll - s_yaw * s_pitch * c_roll;
    q[2] = s_yaw * c_pitch * s_roll + c_yaw * s_pitch * c_roll;
    q[3] = s_yaw * c_pitch * c_roll - c_yaw * s_pitch * s_roll;
}

void vlc_viewpoint_to_4x4( const vlc_viewpoint_t *vp, float *m )
{
    /* The quaternion must be normalized */
    const float *q = vp->quat;

    /* The quaternion is the opposite rotation of the view.
     * We need to inverse the matrix at the same time. */
    m[0] = 1 - 2 * (q[1]*q[1] + q[2]*q[2]);
    m[4] = 2 * (q[0]*q[1] + q[2]*q[3]);
    m[8] = 2 * (q[0]*q[2] - q[1]*q[3]);
    m[3] = 0;

    m[1] = 2 * (q[0]*q[1] - q[2]*q[3]);
    m[5] = 1 - 2 * (q[0]*q[0] + q[2]*q[2]);
    m[9] = 2 * (q[1]*q[2] + q[0]*q[3]);
    m[7] = 0;

    m[2] = 2 * (q[0]*q[2] + q[1]*q[3]);
    m[6] = 2 * (q[1]*q[2] - q[0]*q[3]);
    m[10] = 1 - 2*(q[0]*q[0] + q[1]*q[1]);
    m[11] = 0;

    m[12] = m[13] = m[14] = 0;
    m[15] = 1;
}

void vlc_viewpoint_from_euler(vlc_viewpoint_t *vp,
                              float yaw, float pitch, float roll)
{
    /* convert angles from degrees into radians */
    yaw   *= -(float)M_PI / 180.f;
    pitch *= -(float)M_PI / 180.f;
    roll  *=  (float)M_PI / 180.f;

    EulerToQuaternion(vp->quat, yaw, pitch, roll);
}

void vlc_viewpoint_to_euler(const vlc_viewpoint_t *vp,
                            float *yaw, float *pitch, float *roll)
{
    QuaternionToEuler(yaw, pitch, roll, vp->quat);

    /* convert angles from radian into degrees */
    *yaw   *= -180.f / (float)M_PI;
    *pitch *= -180.f / (float)M_PI;
    *roll  *=  180.f / (float)M_PI;
}
