
/*
 * Copyright @ 2017 Intel Corporation
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
 *     Pengfei Qu <Pengfei.Qu@intel.com>
 *
 */

#include "i965_avc_encoder_common.h"

// H.264 table A-1 - level limits.
static const struct avc_level_limits {
    int level_idc;
    int max_mbps;
    int max_fs;
    int max_dpb_mbs;
    int max_v_mv_r;
    int max_mvs_per_2mb;
} avc_level_limits[] = {
    { INTEL_AVC_LEVEL_1,      1485,     99,     396,   64,  0 },
    { INTEL_AVC_LEVEL_11,     3000,    396,     900,  128,  0 },
    { INTEL_AVC_LEVEL_12,     6000,    396,    2376,  128,  0 },
    { INTEL_AVC_LEVEL_13,    11880,    396,    2376,  128,  0 },
    { INTEL_AVC_LEVEL_2,     11880,    396,    2376,  128,  0 },
    { INTEL_AVC_LEVEL_21,    19800,    792,    4752,  256,  0 },
    { INTEL_AVC_LEVEL_22,    20250,   1620,    8100,  256,  0 },
    { INTEL_AVC_LEVEL_3,     40500,   1620,    8100,  256, 32 },
    { INTEL_AVC_LEVEL_31,   108000,   3600,   18000,  512, 16 },
    { INTEL_AVC_LEVEL_32,   216000,   5120,   20480,  512, 16 },
    { INTEL_AVC_LEVEL_4,    245760,   8192,   32768,  512, 16 },
    { INTEL_AVC_LEVEL_41,   245760,   8192,   32768,  512, 16 },
    { INTEL_AVC_LEVEL_42,   522240,   8704,   34816,  512, 16 },
    { INTEL_AVC_LEVEL_5,    589824,  22080,  110400,  512, 16 },
    { INTEL_AVC_LEVEL_51,   983040,  36864,  184320,  512, 16 },
    { INTEL_AVC_LEVEL_52,  2073600,  36864,  184320,  512, 16 },
    { INTEL_AVC_LEVEL_6,   4177920, 139264,  696320, 8192, 16 },
    { INTEL_AVC_LEVEL_61,  8355840, 139264,  696320, 8192, 16 },
    { INTEL_AVC_LEVEL_62, 16711680, 139264,  696320, 8192, 16 },
};

static const struct avc_level_limits*
get_level_limits(int level_idc)
{
    int i;
    for (i = 1; i < ARRAY_ELEMS(avc_level_limits); i++) {
        if (level_idc < avc_level_limits[i].level_idc)
            break;
    }
    return &avc_level_limits[i - 1];
}

int
i965_avc_level_is_valid(int level_idc)
{
    return get_level_limits(level_idc)->level_idc == level_idc;
}

int
i965_avc_get_max_mbps(int level_idc)
{
    return get_level_limits(level_idc)->max_mbps;
};

unsigned int
i965_avc_get_profile_level_max_frame(struct avc_param * param,
                                     int level_idc)
{
    double bits_per_mb, tmpf;
    int max_mbps, num_mb_per_frame;
    uint64_t max_byte_per_frame0, max_byte_per_frame1;
    unsigned int ret;
    unsigned int scale_factor = 4;


    if (level_idc >= INTEL_AVC_LEVEL_31 && level_idc <= INTEL_AVC_LEVEL_4)
        bits_per_mb = 96.0;
    else {
        bits_per_mb = 192.0;
        scale_factor = 2;

    }

    max_mbps = i965_avc_get_max_mbps(level_idc);
    num_mb_per_frame = param->frame_width_in_mbs * param->frame_height_in_mbs;

    tmpf = (double)num_mb_per_frame;

    if (tmpf < max_mbps / 172.0)
        tmpf = max_mbps / 172.0;

    max_byte_per_frame0 = (uint64_t)(tmpf * bits_per_mb);
    max_byte_per_frame1 = (uint64_t)(((double)max_mbps * 100) / param->frames_per_100s * bits_per_mb);

    /* TODO: check VAEncMiscParameterTypeMaxFrameSize */
    ret = (unsigned int)MIN(max_byte_per_frame0, max_byte_per_frame1);
    ret = (unsigned int)MIN(ret, param->frame_width_in_pixel * param->frame_height_in_pixel * 3 / (2 * scale_factor));

    return ret;
}

int
i965_avc_calculate_initial_qp(struct avc_param * param)
{
    float x0 = 0, y0 = 1.19f, x1 = 1.75f, y1 = 1.75f;
    unsigned frame_size;
    int qp, delat_qp;

    frame_size = (param->frame_width_in_pixel * param->frame_height_in_pixel * 3 / 2);
    qp = (int)(1.0 / 1.2 * pow(10.0,
                               (log10(frame_size * 2.0 / 3.0 * ((float)param->frames_per_100s) /
                                      ((float)(param->target_bit_rate * 1000) * 100)) - x0) *
                               (y1 - y0) / (x1 - x0) + y0) + 0.5);
    qp += 2;
    delat_qp = (int)(9 - (param->vbv_buffer_size_in_bit * ((float)param->frames_per_100s) /
                          ((float)(param->target_bit_rate * 1000) * 100)));
    if (delat_qp > 0)
        qp += delat_qp;

    qp = CLAMP(1, 51, qp);
    qp--;

    return qp;
}


int
i965_avc_get_max_mv_len(int level_idc)
{
    return get_level_limits(level_idc)->max_v_mv_r - 1;
}

int
i965_avc_get_max_mv_per_2mb(int level_idc)
{
    return get_level_limits(level_idc)->max_mvs_per_2mb;
}

unsigned short
i965_avc_calc_skip_value(unsigned int enc_block_based_sip_en, unsigned int transform_8x8_flag, unsigned short skip_value)
{
    if (!enc_block_based_sip_en) {
        skip_value *= 3;
    } else if (!transform_8x8_flag) {
        skip_value /= 2;
    }

    return skip_value;
}
