/*****************************************************************************
 * test/src/video_output/spu.c
 *****************************************************************************
 * Copyright (C) 2019 VideoLabs
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

#include "../../libvlc/test.h"
#include "../../../lib/libvlc_internal.h"
#include <assert.h>
#include <vlc_common.h>
#include <vlc_spu.h>
#include <vlc_codec.h>
#include "../../../modules/codec/substext.h"


/*
 * Main tested issues:
 *
 * + Check that calling spu_Render multiple time for the same PTS won't move
 *   the subtitle (with or without external scaling).
 *
 * + Check that calling spu_Render with or without external scaling gives the
 *   same values.
 */

const char vlc_module_name[] = "spu_test";

static const char *libvlc_argv[] = {
    "--verbose=2",
    "--ignore-config",
    "-Idummy",
    "--no-media-library",
    "--text-renderer=freetype"
};

static subpicture_t *subpicture_new(decoder_t *dec,
                                    const subpicture_updater_t *p_dyn)
{
    (void)dec;
    return subpicture_New(p_dyn);
}

static const struct decoder_owner_callbacks dec_subpicture_cbs =
{
    .spu.buffer_new = subpicture_new,
};

static decoder_t dec_subpicture =
{
    .fmt_in.i_cat = SPU_ES,
    .cbs = &dec_subpicture_cbs,
};

static spu_t *create_spu(libvlc_instance_t *vlc)
{
    spu_t *spu = spu_Create(vlc->p_libvlc_int, NULL);

    return spu;
}

static subpicture_t *create_subtitle(const char *subtitle)
{
    video_format_t fmt;
    video_format_Init(&fmt, VLC_CODEC_TEXT);
    subpicture_t *sub = decoder_NewSubpictureText(&dec_subpicture);
    assert(sub);

    subtext_updater_sys_t *sub_sys = sub->updater.p_sys;
    SubpictureUpdaterSysRegionInit(&sub_sys->region);
    sub_sys->region.p_segments = text_segment_New(subtitle);
    sub_sys->region.p_segments->style = text_style_Create(STYLE_NO_DEFAULTS);
    assert(sub_sys->region.p_segments);

    /* Set first region defaults */
    /* The "leavetext" alignment is a special mode where the subpicture
       region itself gets aligned, but the text inside it does not */
    sub_sys->region.align = SUBPICTURE_ALIGN_TOP|SUBPICTURE_ALIGN_LEFT;
    sub_sys->region.inner_align = SUBPICTURE_ALIGN_BOTTOM|SUBPICTURE_ALIGN_LEFT;
    sub_sys->region.flags = UPDT_REGION_IGNORE_BACKGROUND | UPDT_REGION_USES_GRID_COORDINATES;

    /* Set style defaults (will be added to segments if none set) */
    sub_sys->p_default_style->i_style_flags |= STYLE_MONOSPACED;
    {
        sub_sys->p_default_style->i_background_alpha = STYLE_ALPHA_OPAQUE;
        sub_sys->p_default_style->i_features |= STYLE_HAS_BACKGROUND_ALPHA;
        sub_sys->p_default_style->i_style_flags |= STYLE_BACKGROUND;
    }


    return sub;
}

const vlc_fourcc_t chroma_list[] =
{
    VLC_CODEC_RGBA, 0
};

static void test_multiple_spu_render(const video_format_t *fmt_src,
                                     const video_format_t* fmt_dst,
                                     bool can_scale_spu)
{
    subpicture_t *subtitle, *output;
    spu_t *spu;

    libvlc_instance_t *vlc = libvlc_new(ARRAY_SIZE(libvlc_argv), libvlc_argv);

    spu = create_spu(vlc);
    subtitle =  create_subtitle("test");
    subtitle->b_subtitle = true;
    subtitle->b_absolute = false;
    subtitle->i_start = 1;
    subtitle->i_stop = 100000;
    spu_PutSubpicture(spu, subtitle);
    output     = spu_Render(spu, chroma_list, fmt_dst, fmt_src,
                            1, 1, 1.f, false, can_scale_spu);
    assert(output);
    assert(output->p_region); //< the text has been rasterized

    video_format_t out1_fmt;
    video_format_Init(&out1_fmt, VLC_CODEC_RGBA);
    video_format_Copy(&out1_fmt, &output->p_region->fmt);
    vlc_rational_t out1_zoomh = output->p_region->zoom_h;
    vlc_rational_t out1_zoomv = output->p_region->zoom_v;
    int i_x = output->p_region->i_x;
    int i_y = output->p_region->i_y;
    output     = spu_Render(spu, chroma_list, fmt_dst, fmt_src,
                            1, 1, 1.f, false, can_scale_spu);
    assert(output);
    subpicture_Delete(output);
    output     = spu_Render(spu, chroma_list, fmt_dst, fmt_src,
                            1, 1, 1.f, false, can_scale_spu);
    assert(output);
    subpicture_Delete(output);
    output     = spu_Render(spu, chroma_list, fmt_dst, fmt_src,
                            1, 1, 1.f, false, can_scale_spu);
    assert(output);
    assert(out1_zoomh.num == output->p_region->zoom_h.num);
    assert(out1_zoomh.den == output->p_region->zoom_h.den);
    assert(out1_zoomv.num == output->p_region->zoom_v.num);
    assert(out1_zoomv.den == output->p_region->zoom_v.den);
    assert(out1_fmt.i_visible_width == output->p_region->fmt.i_visible_width);
    assert(out1_fmt.i_visible_height == output->p_region->fmt.i_visible_height);
    assert(i_x == output->p_region->i_x && i_y == output->p_region->i_y);
    subpicture_Delete(output);
    spu_Destroy(spu);

    libvlc_release(vlc);

}


static void test_spu_size(const video_format_t *fmt_src,
                          const video_format_t* fmt_dst)
{
    subpicture_t *subtitle1, *subtitle2, *output1, *output2;
    spu_t *spu1, *spu2;

    libvlc_instance_t *vlc = libvlc_new(ARRAY_SIZE(libvlc_argv), libvlc_argv);

    spu1 = create_spu(vlc);
    spu2 = create_spu(vlc);

    subtitle1 =  create_subtitle("test");
    subtitle1->b_subtitle = true;
    subtitle1->b_absolute = false;
    subtitle1->i_start = 1;
    subtitle1->i_stop = 100000;

    subtitle2 =  create_subtitle("test");
    subtitle2->b_subtitle = true;
    subtitle2->b_absolute = false;
    subtitle2->i_start = 1;
    subtitle2->i_stop = 100000;

    spu_PutSubpicture(spu1, subtitle1);
    spu_PutSubpicture(spu2, subtitle2);

    output1     = spu_Render(spu1, chroma_list, fmt_dst, fmt_src,
                            1, 1, 1.f, false, false);
    output2     = spu_Render(spu2, chroma_list, fmt_dst, fmt_src,
                            1, 1, 1.f, false, true);
    assert(output1); assert(output2);
    assert(output1->p_region); //< the text has been rasterized
    assert(output2->p_region); //< the text has been rasterized

    int width1 = output1->p_region->fmt.i_visible_width;
    int height1 = output1->p_region->fmt.i_visible_height;

    /* There is no external scaling with subtitles, as they are rasterized
     * at the size of the output. */
    assert(output1->p_region->zoom_h.num == output1->p_region->zoom_h.den);
    assert(output1->p_region->zoom_v.num == output1->p_region->zoom_v.den);
    assert(output2->p_region->zoom_h.num == output2->p_region->zoom_h.den);
    assert(output2->p_region->zoom_v.num == output2->p_region->zoom_v.den);

    int width2 = output2->p_region->fmt.i_visible_width;
    int height2 = output2->p_region->fmt.i_visible_height;

    int i_x1 = output1->p_region->i_x;
    int i_y1 = output1->p_region->i_y;
    int i_x2 = output2->p_region->i_x;
    int i_y2 = output2->p_region->i_y;

    msg_Err(spu1, "x1=%d, y1=%d, width1=%d, height1=%d",
            i_x1, i_y1, width1, height1);
    msg_Err(spu2, "x2=%d, y2=%d, width2=%d, height2=%d",
            i_x2, i_y2, width2, height2);

    /* There should be no differences for subpicture rendering whether external
     * scaling is enabled or not. */
    assert(width1 == width2 && height1 == height2);
    assert(i_x1 == i_x2 && i_y1 == i_y2);

    subpicture_Delete(output1);
    subpicture_Delete(output2);
    spu_Destroy(spu1);
    spu_Destroy(spu2);

    libvlc_release(vlc);

}

int main(int argc, char **argv)
{
    test_init();

    (void)argc; (void)argv;

    video_format_t fmt_in, fmt_out;
    video_format_Init(&fmt_in, VLC_CODEC_RGBA);
    fmt_in.i_width = fmt_in.i_visible_width = 1920;
    fmt_in.i_height = fmt_in.i_visible_height = 1080;
    fmt_in.i_sar_num = fmt_in.i_sar_den = 1;

    video_format_Init(&fmt_out, VLC_CODEC_RGBA);
    fmt_out.i_width = fmt_out.i_visible_width = 3860;
    fmt_out.i_height = fmt_out.i_visible_height = 2160;
    fmt_out.i_sar_num = fmt_out.i_sar_den = 1;

    test_multiple_spu_render(&fmt_in, &fmt_out, false);
    test_multiple_spu_render(&fmt_in, &fmt_out, true);

    test_spu_size(&fmt_in, &fmt_out);

    return 0;
}
