/*****************************************************************************
 * viewpoint.c: test for viewpoint
 *****************************************************************************
 * Copyright (C) 2019 VLC Authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <vlc_viewpoint.h>
#include "../../libvlc/test.h"

static bool reciprocal_euler(float epsilon, float yaw, float pitch, float roll)
{
    vlc_viewpoint_t vp;
    vlc_viewpoint_from_euler(&vp, yaw, pitch, roll);

    float yaw2, pitch2, roll2;
    vlc_viewpoint_to_euler(&vp, &yaw2, &pitch2, &roll2);

    fprintf(stderr, "==========================================\n");
    fprintf(stderr, "original:   yaw=%f, pitch=%f, roll=%f\n", yaw, pitch, roll);
    fprintf(stderr, "converted:  yaw=%f, pitch=%f, roll=%f\n", yaw2, pitch2, roll2);
    fprintf(stderr, "==========================================\n");

    float d1 = fabs(yaw   - yaw2);
    float d2 = fabs(pitch - pitch2);
    float d3 = fabs(roll  - roll2);

    return (d1 < epsilon || fabs(d1 - 90.f)  < epsilon) &&
           (d2 < epsilon || fabs(d2 - 180.f)  < epsilon) &&
           (d3 < epsilon || fabs(d3 - 180.f) < epsilon);
}

static void test_conversion()
{
    const float epsilon = 0.1f;
    assert(reciprocal_euler(epsilon, 0.f,  0.f,  0.f));
    assert(reciprocal_euler(epsilon, 45.f, 0.f,  0.f));
    assert(reciprocal_euler(epsilon, 0.f,  45.f, 0.f));
    assert(reciprocal_euler(epsilon, 0.f,  0.f,  45.f));
    assert(reciprocal_euler(epsilon, 45.f, 45.f, 0.f));
    assert(reciprocal_euler(epsilon, 0.f,  45.f, 45.f));
    assert(reciprocal_euler(epsilon, 45.f, 45.f, 45.f));
}

int main( void )
{
    test_init();

    test_conversion();

    return 0;
}
