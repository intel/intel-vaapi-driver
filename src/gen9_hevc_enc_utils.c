/*
 * Copyright © 2017 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWAR OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Chen, Peng <chen.c.peng@intel.com>
 *
 */

#include "intel_driver.h"
#include "gen9_hevc_enc_utils.h"

static int
hevc_get_max_mbps(unsigned int level_idc)
{
    int max_bps = 0;

    switch (level_idc) {
    case 30:
        max_bps = 552960;
        break;
    case 60:
        max_bps = 686400;
        break;
    case 90:
        max_bps = 13762560;
        break;
    case 93:
        max_bps = 33177600;
        break;
    case 120:
    case 123:
        max_bps = 62668800;
        break;
    case 126:
    case 129:
        max_bps = 133693440;
        break;
    case 150:
    case 153:
        max_bps = 267386880;
        break;
    case 156:
        max_bps = 534773760;
        break;
    case 180:
        max_bps = 1002700800;
        break;
    case 183:
        max_bps = 2005401600;
        break;
    case 186:
        max_bps = 4010803200;
        break;
    default:
        max_bps = 13762560;
        break;
    }

    return max_bps;
}

unsigned int
gen9_hevc_get_profile_level_max_frame(VAEncSequenceParameterBufferHEVC *seq_param,
                                      unsigned int user_max_frame_size,
                                      unsigned int frame_rate)
{
    double bits_per_mb, tmp_f;
    int max_mbps, num_mb_per_frame;
    unsigned long long max_byte_per_pic, max_byte_per_pic_not0;
    int profile_level_max_frame;
    double frameRateD = 100;

    bits_per_mb = 192;

    if (seq_param->seq_fields.bits.chroma_format_idc == 0)
        max_mbps = hevc_get_max_mbps(seq_param->general_level_idc) / 16 / 16;
    else
        max_mbps = (int)(((double)hevc_get_max_mbps(seq_param->general_level_idc)) * 1.5 / 16 / 16);

    num_mb_per_frame = ALIGN(seq_param->pic_width_in_luma_samples, 16) *
                       ALIGN(seq_param->pic_height_in_luma_samples, 16) / 256;

    tmp_f = (double)num_mb_per_frame;
    if (tmp_f < max_mbps / 172)
        tmp_f = max_mbps / 172;

    max_byte_per_pic = (unsigned long long)(tmp_f * bits_per_mb);
    max_byte_per_pic_not0 =
        (unsigned long long)((((double)max_mbps * frameRateD) /
                              (double)frame_rate) * bits_per_mb);

    if (user_max_frame_size) {
        profile_level_max_frame = (unsigned int)MIN(user_max_frame_size, max_byte_per_pic);
        profile_level_max_frame = (unsigned int)MIN(max_byte_per_pic_not0, profile_level_max_frame);
    } else
        profile_level_max_frame = (unsigned int)MIN(max_byte_per_pic_not0, max_byte_per_pic);

    return MIN(profile_level_max_frame,
               seq_param->pic_width_in_luma_samples * seq_param->pic_height_in_luma_samples);
}
