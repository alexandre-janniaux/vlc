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
#include <stdio.h>

static void multiplyQuat(float *dst, const float *left, const float *right)
{
    dst[3] = left[3] * right[3] /* w * w terms */
           - left[0] * right[0] /* other paired terms */
           - left[1] * right[1]
           - left[2] * right[2];

    dst[0] = left[3] * right[0] /* w * i terms */
           + left[0] * right[3]
           + left[1] * right[2] /* j * k terms */
           - left[2] * right[1];

    dst[1] = left[3] * right[1] /* w * j terms */
           + left[1] * right[3]
           + left[2] * right[0] /* i * k terms */
           - left[0] * right[2];

    dst[2] = left[3] * right[2] /* w * k terms */
           + left[2] * right[3]
           + left[0] * right[1] /* i * j terms */
           - left[1] * right[0];
}

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
    float test = q[1] * q[3] - q[0] * q[2];

    if (test > 0.499 * unit)
    {
        // singularity at north pole
        *yaw = -2 * atan2(q[0], q[3]);
        *pitch = M_PI / 2;
        *roll = 0;
    }
    else if (test < -0.499 * unit)
    {
        // singularity at south pole
        *yaw = 2 * atan2(q[0], q[3]);
        *pitch = -M_PI / 2;
        *roll = 0;
    }
    else
    {
        *yaw   = atan2(2 * (q[2] * q[3] + q[0] * q[1]),
                       unit - 2 * (q[1]*q[1] + q[2]*q[2]));
        *pitch = asin(2 * test / unit);
        *roll  = atan2(2 * (q[0] * q[3] + q[1] * q[2]),
                       unit - 2 * (q[0]*q[0] + q[1]*q[1]));
    }
}

static void EulerToQuaternion(float *q, float yaw, float pitch, float roll)
{
    //yaw   *= -1;
    //pitch *= -1;

    const float c_yaw   = cos(yaw / 2.f);
    const float s_yaw   = sin(yaw / 2.f);
    const float c_pitch = cos(pitch / 2.f);
    const float s_pitch = sin(pitch / 2.f);
    const float c_roll  = cos(roll / 2.f);
    const float s_roll  = sin(roll / 2.f);

    q[3] = c_pitch*c_roll*c_yaw - s_pitch*s_roll*s_yaw;
    q[0] = c_pitch*s_roll*s_yaw - c_roll*c_yaw*s_pitch;
    q[1] = -c_pitch*c_roll*s_yaw - c_yaw*s_pitch*s_roll;
    q[2] = c_pitch*c_yaw*s_roll + c_roll*s_pitch*s_yaw;

    //q[3] = c_yaw * c_pitch * c_roll + s_yaw * s_pitch * s_roll;
    //q[0] = c_yaw * s_pitch * c_roll + s_yaw * c_pitch * s_roll;
    //q[1] = s_yaw * c_pitch * c_roll - c_yaw * s_pitch * s_roll;
    //q[2] = s_yaw * s_pitch * c_roll - c_yaw * c_pitch * s_roll;
}

void vlc_viewpoint_to_4x4( const vlc_viewpoint_t *vp, float *m )
{
    /* The quaternion must be normalized */
    const float *q = vp->quat;

    const float xx = q[0] * q[0];
    const float yy = q[1] * q[1];
    const float zz = q[2] * q[2];
    const float ww = q[3] * q[3];

    const float xy = q[0] * q[1];
    const float zw = q[2] * q[3];
    const float yz = q[1] * q[2];
    const float xw = q[0] * q[3];

    const float xz = q[0] * q[2];
    const float yw = q[1] * q[3];

    /* The quaternion is the opposite rotation of the view.
     * We need to inverse the matrix at the same time. */
    m[0]  = xx + ww - yy - zz;
    m[4]  = 2 * (xy + zw);
    m[8]  = 2 * (xz - yw);
    m[3]  = 0;

    m[1]  = 2 * (xy - zw);
    m[5]  = 1 - 2 * (xx + zz);
    m[9]  = 2 * (yz + xw);
    m[7]  = 0;

    m[2]  = 2 * (xz + yw);
    m[6]  = 2 * (yz - xw);
    m[10] = 1 - 2 * (xx + yy);
    m[11] = 0;

    m[12] = m[13] = m[14] = 0;
    m[15] = 1;

#define SWP(x, y) \
    do { float tmp = x; x = y; y = tmp;} while(0)

    //SWP(m[1], m[8]);
    //SWP(m[2], m[4]);
    //SWP(m[5], m[10]);

    float transpose[16];
    for (int j=0; j<4; ++j)
        for (int k=0; k<4; ++k)
            transpose[4*k+j] = m[4*j+k];
    memcpy(m, transpose, sizeof(transpose));

}

void vlc_viewpoint_from_euler(vlc_viewpoint_t *vp,
                              float yaw, float pitch, float roll)
{
    /* convert angles from degrees into radians */
    yaw   = yaw   * (float)M_PI / 180.f + (float)M_PI_2;
    pitch = pitch * (float)M_PI / 180.f;
    roll  = roll  * (float)M_PI / 180.f;

    EulerToQuaternion(vp->quat, yaw, pitch, roll);
}

void vlc_viewpoint_to_euler(const vlc_viewpoint_t *vp,
                            float *yaw, float *pitch, float *roll)
{
    QuaternionToEuler(yaw, pitch, roll, vp->quat);

    /* convert angles from radian into degrees */
    *yaw   = 180.f / (float)M_PI * (*yaw - (float)M_PI_2);
    *pitch = 180.f / (float)M_PI * (*pitch);
    *roll  = 180.f / (float)M_PI * (*roll);
}
