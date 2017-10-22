
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
int
i965_avc_get_max_mbps(int level_idc)
{
    int max_mbps = 11880;

    switch (level_idc) {
    case INTEL_AVC_LEVEL_2:
        max_mbps = 11880;
        break;

    case INTEL_AVC_LEVEL_21:
        max_mbps = 19800;
        break;

    case INTEL_AVC_LEVEL_22:
        max_mbps = 20250;
        break;

    case INTEL_AVC_LEVEL_3:
        max_mbps = 40500;
        break;

    case INTEL_AVC_LEVEL_31:
        max_mbps = 108000;
        break;

    case INTEL_AVC_LEVEL_32:
        max_mbps = 216000;
        break;

    case INTEL_AVC_LEVEL_4:
    case INTEL_AVC_LEVEL_41:
        max_mbps = 245760;
        break;

    case INTEL_AVC_LEVEL_42:
        max_mbps = 522240;
        break;

    case INTEL_AVC_LEVEL_5:
        max_mbps = 589824;
        break;

    case INTEL_AVC_LEVEL_51:
        max_mbps = 983040;
        break;

    case INTEL_AVC_LEVEL_52:
        max_mbps = 2073600;
        break;

    default:
        break;
    }

    return max_mbps;
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

    if (qp < 0)
        qp = 1;

    return qp;
}

int
i965_avc_get_max_v_mv_r(int level_idc)
{
    int max_v_mv_r = 128 * 4;

    // See JVT Spec Annex A Table A-1 Level limits for below mapping
    // MaxVmvR is in luma quarter pel unit
    switch (level_idc) {
    case INTEL_AVC_LEVEL_1:
        max_v_mv_r = 64 * 4;
        break;
    case INTEL_AVC_LEVEL_11:
    case INTEL_AVC_LEVEL_12:
    case INTEL_AVC_LEVEL_13:
    case INTEL_AVC_LEVEL_2:
        max_v_mv_r = 128 * 4;
        break;
    case INTEL_AVC_LEVEL_21:
    case INTEL_AVC_LEVEL_22:
    case INTEL_AVC_LEVEL_3:
        max_v_mv_r = 256 * 4;
        break;
    case INTEL_AVC_LEVEL_31:
    case INTEL_AVC_LEVEL_32:
    case INTEL_AVC_LEVEL_4:
    case INTEL_AVC_LEVEL_41:
    case INTEL_AVC_LEVEL_42:
    case INTEL_AVC_LEVEL_5:
    case INTEL_AVC_LEVEL_51:
    case INTEL_AVC_LEVEL_52:
        max_v_mv_r = 512 * 4;
        break;
    default:
        assert(0);
        break;
    }

    return max_v_mv_r;
}

int
i965_avc_get_max_mv_len(int level_idc)
{
    int max_mv_len = 127;

    // See JVT Spec Annex A Table A-1 Level limits for below mapping
    // MaxVmvR is in luma quarter pel unit
    switch (level_idc) {
    case INTEL_AVC_LEVEL_1:
        max_mv_len = 63;
        break;
    case INTEL_AVC_LEVEL_11:
    case INTEL_AVC_LEVEL_12:
    case INTEL_AVC_LEVEL_13:
    case INTEL_AVC_LEVEL_2:
        max_mv_len = 127;
        break;
    case INTEL_AVC_LEVEL_21:
    case INTEL_AVC_LEVEL_22:
    case INTEL_AVC_LEVEL_3:
        max_mv_len = 255;
        break;
    case INTEL_AVC_LEVEL_31:
    case INTEL_AVC_LEVEL_32:
    case INTEL_AVC_LEVEL_4:
    case INTEL_AVC_LEVEL_41:
    case INTEL_AVC_LEVEL_42:
    case INTEL_AVC_LEVEL_5:
    case INTEL_AVC_LEVEL_51:
    case INTEL_AVC_LEVEL_52:
        max_mv_len = 511;
        break;
    default:
        assert(0);
        break;
    }

    return max_mv_len;
}

int
i965_avc_get_max_mv_per_2mb(int level_idc)
{
    unsigned int max_mv_per_2mb = 32;

    // See JVT Spec Annex A Table A-1 Level limits for below mapping
    switch (level_idc) {
    case INTEL_AVC_LEVEL_3:
        max_mv_per_2mb = 32;
        break;
    case INTEL_AVC_LEVEL_31:
    case INTEL_AVC_LEVEL_32:
    case INTEL_AVC_LEVEL_4:
    case INTEL_AVC_LEVEL_41:
    case INTEL_AVC_LEVEL_42:
    case INTEL_AVC_LEVEL_5:
    case INTEL_AVC_LEVEL_51:
    case INTEL_AVC_LEVEL_52:
        max_mv_per_2mb = 16;
        break;
    default:
        break;
    }

    return max_mv_per_2mb;
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

unsigned short i965_avc_get_maxnum_slices_num(int profile_idc, int level_idc, unsigned int frames_per_100s)
{
    unsigned int  slice_num = 0;

    if ((profile_idc == VAProfileH264Main) ||
        (profile_idc == VAProfileH264High)) {
        switch (level_idc) {
        case INTEL_AVC_LEVEL_3:
            slice_num = (unsigned int)(40500.0 * 100 / 22.0 / frames_per_100s);
            break;
        case INTEL_AVC_LEVEL_31:
            slice_num = (unsigned int)(108000.0 * 100 / 60.0 / frames_per_100s);
            break;
        case INTEL_AVC_LEVEL_32:
            slice_num = (unsigned int)(216000.0 * 100 / 60.0 / frames_per_100s);
            break;
        case INTEL_AVC_LEVEL_4:
        case INTEL_AVC_LEVEL_41:
            slice_num = (unsigned int)(245760.0 * 100 / 24.0 / frames_per_100s);
            break;
        case INTEL_AVC_LEVEL_42:
            slice_num = (unsigned int)(522240.0 * 100 / 24.0 / frames_per_100s);
            break;
        case INTEL_AVC_LEVEL_5:
            slice_num = (unsigned int)(589824.0 * 100 / 24.0 / frames_per_100s);
            break;
        case INTEL_AVC_LEVEL_51:
            slice_num = (unsigned int)(983040.0 * 100 / 24.0 / frames_per_100s);
            break;
        case INTEL_AVC_LEVEL_52:
            slice_num = (unsigned int)(2073600.0 * 100 / 24.0 / frames_per_100s);
            break;
        default:
            slice_num = 0;
        }
    }

    return slice_num;
}
