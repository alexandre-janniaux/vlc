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
#include <assert.h>

/* Quaternion to/from Euler conversion.
 * Original code from:
 * http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToEuler/ */
static void QuaternionToEuler(float *yaw, float *pitch, float *roll, const float *q)
{
    /* The matrix built from the angles is made from the multiplication of the
     * following matrices:
     *                    ⎡cos(yaw)  0  -sin(yaw)⎤
     *  m_yaw (y_rot)   = ⎢   0      1      0    ⎥
     *                    ⎣sin(yaw)  0  cos(yaw) ⎦
     *                    ⎡1       0           0     ⎤
     *  m_pitch (x_rot) = ⎢0  cos(pitch)   sin(pitch)⎥
     *                    ⎣0  -sin(pitch)  cos(pitch)⎦
     *                    ⎡cos(roll)   sin(roll)  0⎤
     *  m_roll (z_rot)  = ⎢-sin(roll)  cos(roll)  0⎥
     *                    ⎣    0           0      1⎦
     *
     * Which, multiplied in the correct order will bring, with the symbols
     * rewritten: sin = s , cos = c, yaw = y, pitch = p, roll = r
     *
     *     ⎡s(p)⋅s(r)⋅s(y) + c(r)⋅c(y)  s(r)⋅c(p)  s(p)⋅s(r)⋅c(y) - s(y)⋅c(r)⎤
     * V = ⎢s(p)⋅s(y)⋅c(r) - s(r)⋅c(y)  c(p)⋅c(r)  s(p)⋅c(r)⋅c(y) + s(r)⋅s(y)⎥
     *     ⎣           s(y)⋅c(p)          -s(p)           c(p)⋅c(y)          ⎦
     *
     * We can first extract pitch = atan2( -V_32, sqrt(V_31^2 + V_33^2) )
     *
     * By taking the case |pitch| = 90 degree, it simplify c(y) and s(y) and:
     *      roll = atan2( V_11, -V_21 )
     *      yaw  = atan2( V_11, -V_13 )
     *
     * Otherwise, |pitch| != 90 degree and we can get:
     *      roll = atan2( V_12, V_22 )
     *      yaw  = atan2( V_31, V_33 )
     *
     * By identifying the coefficient in this matrix and the matrix obtained
     * from converting the quaternion to 3x3 matrix, we get the following
     * results.
     */

    /* Rename variables and precompute square values to improve readability. as
     * they are used multiple times. */
    float gw = q[3];
    float gx = q[0];
    float gy = q[1];
    float gz = q[2];

    float sqx = gx * gx;
    float sqy = gy * gy;
    float sqz = gz * gz;
    float sqw = gw * gw;

    // if the quaternion is normalised, unit is one
    // otherwise it is correction factor
    float unit = sqx + sqy + sqz + sqw;

    /*
     *     ⎡      2       2                                              ⎤
     *     ⎢- 2⋅gy  - 2⋅gz  + 1   2⋅gw⋅gz + 2⋅gx⋅gy   -2⋅gw⋅gy + 2⋅gx⋅gz ⎥
     *     ⎢                                                             ⎥
     * M = ⎢                           2       2                         ⎥
     *     ⎢-2⋅gw⋅gz + 2⋅gx⋅gy   - 2⋅gx  - 2⋅gz  + 1   2⋅gw⋅gx + 2⋅gy⋅gz ⎥
     *     ⎢                                                             ⎥
     *     ⎢                                                2       2    ⎥
     *     ⎣ 2⋅gw⋅gy + 2⋅gx⋅gz   -2⋅gw⋅gx + 2⋅gy⋅gz   - 2⋅gx  - 2⋅gy  + 1⎦
     */


    /*
     *     ⎡sp⋅sr⋅sy + cr⋅cy    sp⋅sy⋅cr - sr⋅cy    sy⋅cp⎤
     *     ⎢                                             ⎥
     * M = ⎢    sr⋅cp              cp⋅cr             -sp ⎥
     *     ⎢                                             |
     *     ⎣sp⋅sr⋅cy - sy⋅cr    sp⋅cr⋅cy + sr⋅sy    cp⋅cy⎦
     */

    /* The test value is extracted from M_23 = -sin(pitch).
     * When abs(M_23 / 2) > = 0.4999, the cos(pitch) will be 0 and we need a
     * fallback method to get the other angles in the singularity. */
    float test = -q[3]*q[0] - q[1]*q[2];

    const float M_11 = 1.f - 2.f * (sqy + sqz) / unit;

    /* TODO not used currently */
    /* Diagonal values. */
    float V_11 = 1 - 2 * (sqy + sqz);
    float V_22 = 1 - 2 * (sqx + sqz);
    float V_33 = 1 - 2 * (sqx + sqy);

    /* Values with a minus sign. */
    float V_13 = 2 * (gx * gz - gw * gy);
    float V_21 = 2 * (gx * gy - gw * gz);

    /* Values with only plus sign. */
    float V_23 = 2 * (gw * gx + gy * gz);
    float V_12 = 2 * (gx * gy + gw * gz);
    float V_31 = 2 * (gw * gy + gx * gz);

    if (test > 0.499 * unit)
    {
        // singularity at north pole
        *yaw = -asin(V_11);
        *pitch = M_PI / 2;
        *roll = 0;
    }
    else if (test < -0.499 * unit)
    {
        // singularity at south pole
        *yaw = asin(V_11);
        *pitch = -M_PI / 2;
        *roll = 0;
    }
    else
    {
        *yaw   = atan2( V_13, V_33 );
        *pitch = atan2( -V_23, sqrt( V_21*V_21 + V_22*V_22 ) );
        *roll  = -atan2( V_21, V_22 );
    }
}

static void EulerToQuaternion(float *q, float yaw, float pitch, float roll)
{
    const float c_yaw   = cos(yaw / 2.f);
    const float s_yaw   = sin(yaw / 2.f);
    const float c_pitch = cos(pitch / 2.f);
    const float s_pitch = sin(pitch / 2.f);
    const float c_roll  = cos(roll / 2.f);
    const float s_roll  = sin(roll / 2.f);

    q[3] =  c_yaw * c_pitch * c_roll - s_yaw * s_pitch * s_roll;
    q[0] =  s_yaw * c_pitch * s_roll - c_yaw * s_pitch * c_roll;
    q[1] = -s_yaw * c_pitch * c_roll - c_yaw * s_pitch * s_roll;
    q[2] =  c_yaw * c_pitch * s_roll + s_yaw * s_pitch * c_roll;
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
    m[1]  = 2 * (xy + zw);
    m[2]  = 2 * (xz - yw);
    m[3]  = 0;

    m[4]  = 2 * (xy - zw);
    m[5]  = 1 - 2 * (xx + zz);
    m[6]  = 2 * (yz + xw);
    m[7]  = 0;

    m[8]  = 2 * (xz + yw);
    m[9]  = 2 * (yz - xw);
    m[10] = 1 - 2 * (xx + yy);
    m[11] = 0;

    m[12] = m[13] = m[14] = 0;
    m[15] = 1;
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
