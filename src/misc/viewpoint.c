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

/* Quaternion to Euler conversion.
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

    /* Precompute square values as they are used multiple times. */
    float sqx = q[0] * q[0];
    float sqy = q[1] * q[1];
    float sqz = q[2] * q[2];
    float sqw = q[3] * q[3];

    float gw = q[3];
    float gx = q[0];
    float gy = q[1];
    float gz = q[2];

    /* Diagonal values. */
    float V_11 = 1 - 2 * (sqy + sqz);
    float V_22 = 1 - 2 * (sqx + sqz);
    float V_33 = 1 - 2 * (sqx + sqy);

    /* Values with a minus sign. */
    float V_31 = 2 * (gx * gz - gw * gy);
    float V_12 = 2 * (gx * gy - gw * gz);

    /* Values with only plus sign. */
    float V_32 = 2 * (gw * gx + gy * gz);
    float V_21 = 2 * (gx * gy + gw * gz);
    float V_13 = 2 * (gw * gy + gx * gz);

    *pitch = atan2( -V_32, sqrt( V_31*V_31 + V_33*V_33 ) );

    if (fabs(*pitch) + 0.01f > (float)M_PI_2)
    {
        *roll = atan2( V_11, -V_21 );
        *yaw  = atan2( V_11, -V_13 );
    }
    else
    {
        *roll = atan2( V_12, V_22 );
        *yaw  = atan2( V_31, V_33 );
    }

    //float s1 = 2.f * (q[0] * q[1] + q[2] * q[3]);
    //float c1 = 1.f - 2.f * (q[1] * q[1] - q[2] * q[2]);
    //*roll = atan2(s1, c1);

    //float s2 = 2.f * (q[0] * q[2] - q[3] * q[1]);
    //if (fabs(s2) >= 1)
    //    *pitch = s2 > 0 ? M_PI_2 : -M_PI_2;
    //else
    //    *pitch = asin(s2);

    //float s3 = 2.f * (q[0] * q[3] + q[1] * q[2]);
    //float c3 = 1.f - 2.f * (q[2] * q[2] + q[3] * q[3]);

    //*yaw = atan2(s3, c3);

    //
    //float sqx = q[0] * q[0];
    //float sqy = q[1] * q[1];
    //float sqz = q[2] * q[2];
    //float sqw = q[3] * q[3];

    //// if the quaternion is normalised, unit is one
    //// otherwise it is correction factor
    //float unit = sqx + sqy + sqz + sqw;
    //float test = q[0] * q[1] + q[2] * q[3];

    //if (test > 0.499 * unit)
    //{
    //    // singularity at north pole
    //    *yaw = 2 * atan2(q[0], q[3]);
    //    *roll = M_PI / 2;
    //    *pitch = 0;
    //}
    //else if (test < -0.499 * unit)
    //{
    //    // singularity at south pole
    //    *yaw = -2 * atan2(q[0], q[3]);
    //    *roll = -M_PI / 2;
    //    *pitch = 0;
    //}
    //else
    //{
    //    *yaw   = atan2(2 * q[1] * q[3] - 2 * q[0] * q[2],
    //                   sqx - sqy - sqz + sqw);
    //    *roll  = asin(2 * test / unit);
    //    *pitch = atan2(2 * q[0] * q[3] - 2 * q[1] * q[2],
    //                   -sqx + sqy - sqz + sqw);
    //}
}

static void EulerToQuaternion(float *q, float yaw, float pitch, float roll)
{
    const float c_yaw   = cos(yaw / 2.f);
    const float s_yaw   = sin(yaw / 2.f);
    const float c_pitch = cos(pitch / 2.f);
    const float s_pitch = sin(pitch / 2.f);
    const float c_roll  = cos(roll / 2.f);
    const float s_roll  = sin(roll / 2.f);

    q[0] = c_yaw * c_pitch * c_roll + s_yaw * s_pitch * s_roll;
    q[1] = c_yaw * c_pitch * s_roll - s_yaw * s_pitch * c_roll;
    q[2] = s_yaw * c_pitch * s_roll + c_yaw * s_pitch * c_roll;
    q[3] = s_yaw * c_pitch * c_roll - c_yaw * s_pitch * s_roll;
}

void vlc_viewpoint_to_4x4( const vlc_viewpoint_t *vp, float *m )
{
    float quat[] = { 0, 0, 0, 1.f };
    float y, p, r;
    QuaternionToEuler(&y, &p, &r, quat);
    EulerToQuaternion(quat, y, p, r);

    fprintf(stderr, "QUAT: %f %f %f %f | Euler %f %f %f \n", quat[0], quat[1], quat[2], quat[3], y, p, r);

    memcpy(quat, &(float[4]){ 0, 0, 0, -1.f }, sizeof(quat));
    QuaternionToEuler(&y, &p, &r, quat);
    fprintf(stderr, "QUAT: %f %f %f %f | Euler %f %f %f \n", quat[0], quat[1], quat[2], quat[3], y, p, r);

    y = 0; p = 0; r = 0;
    EulerToQuaternion(quat, y, p, r);
    fprintf(stderr, "QUAT: %f %f %f %f | Euler %f %f %f \n", quat[0], quat[1], quat[2], quat[3], y, p, r);

    y = M_PI_2; p = 0; r = 0;
    EulerToQuaternion(quat, y, p, r);
    fprintf(stderr, "QUAT: %f %f %f %f | Euler %f %f %f \n", quat[0], quat[1], quat[2], quat[3], y, p, r);
    QuaternionToEuler(&y, &p, &r, quat);
    fprintf(stderr, "QUAT: %f %f %f %f | Euler %f %f %f \n", quat[0], quat[1], quat[2], quat[3], y, p, r);

    y = 0; p = M_PI_2; r = 0;
    EulerToQuaternion(quat, y, p, r);
    fprintf(stderr, "QUAT: %f %f %f %f | Euler %f %f %f \n", quat[0], quat[1], quat[2], quat[3], y, p, r);
    QuaternionToEuler(&y, &p, &r, quat);
    fprintf(stderr, "QUAT: %f %f %f %f | Euler %f %f %f \n", quat[0], quat[1], quat[2], quat[3], y, p, r);

    y = 0; p = 0; r = M_PI_2;
    EulerToQuaternion(quat, y, p, r);
    fprintf(stderr, "QUAT: %f %f %f %f | Euler %f %f %f \n", quat[0], quat[1], quat[2], quat[3], y, p, r);
    QuaternionToEuler(&y, &p, &r, quat);
    fprintf(stderr, "QUAT: %f %f %f %f | Euler %f %f %f \n", quat[0], quat[1], quat[2], quat[3], y, p, r);

    //exit(1);


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
