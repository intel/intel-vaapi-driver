/*
 * Copyright © 2015 Intel Corporation
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
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Xiang Haihao <haihao.xiang@intel.com>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "intel_batchbuffer.h"
#include "i965_defines.h"
#include "i965_structs.h"
#include "i965_drv_video.h"
#include "i965_encoder.h"
#include "i965_encoder_utils.h"
#include "intel_media.h"
#include "gen9_vdenc.h"

extern int
intel_avc_enc_slice_type_fixup(int slice_type);

static const uint8_t buf_rate_adj_tab_i_lowdelay[72] = {
    0,   0, -8, -12, -16, -20, -28, -36,
    0,   0, -4,  -8, -12, -16, -24, -32,
    4,   2,  0,  -1,  -3,  -8, -16, -24,
    8,   4,  2,   0,  -1,  -4,  -8, -16,
    20, 16,  4,   0,  -1,  -4,  -8, -16,
    24, 20, 16,   8,   4,   0,  -4,  -8,
    28, 24, 20,  16,   8,   4,   0,  -8,
    32, 24, 20,  16,   8,   4,   0,  -4,
    64, 48, 28,  20,   16, 12,   8,   4,
};

static const uint8_t buf_rate_adj_tab_p_lowdelay[72] = {
    -8, -24, -32, -40, -44, -48, -52, -80,
    -8, -16, -32, -40, -40, -44, -44, -56,
    0,    0, -12, -20, -24, -28, -32, -36,
    8,    4,   0,   0,  -8, -16, -24, -32,
    32,  16,   8,   4,  -4,  -8, -16, -20,
    36,  24,  16,   8,   4,  -2,  -4,  -8,
    40,  36,  24,  20,  16,   8,   0,  -8,
    48,  40,  28,  24,  20,  12,   0,  -4,
    64,  48,  28,  20,  16,  12,   8,   4,
};

static const uint8_t buf_rate_adj_tab_b_lowdelay[72] = {
    0,  -4, -8, -16, -24, -32, -40, -48,
    1,   0, -4,  -8, -16, -24, -32, -40,
    4,   2,  0,  -1,  -3,  -8, -16, -24,
    8,   4,  2,   0,  -1,  -4,  -8, -16,
    20, 16,  4,   0,  -1,  -4,  -8, -16,
    24, 20, 16,   8,   4,   0,  -4,  -8,
    28, 24, 20,  16,   8,   4,   0,  -8,
    32, 24, 20,  16,   8,   4,   0,  -4,
    64, 48, 28,  20,  16,  12,   8,   4,
};

static const int8_t dist_qp_adj_tab_i_vbr[81] = {
    +0,  0,  0,  0, 0, 3, 4, 6, 8,
    +0,  0,  0,  0, 0, 2, 3, 5, 7,
    -1,  0,  0,  0, 0, 2, 2, 4, 5,
    -1, -1,  0,  0, 0, 1, 2, 2, 4,
    -2, -2, -1,  0, 0, 0, 1, 2, 4,
    -2, -2, -1,  0, 0, 0, 1, 2, 4,
    -3, -2, -1, -1, 0, 0, 1, 2, 5,
    -3, -2, -1, -1, 0, 0, 2, 4, 7,
    -4, -3, -2, -1, 0, 1, 3, 5, 8,
};

static const int8_t dist_qp_adj_tab_p_vbr[81] = {
    -1,  0,  0,  0, 0, 1, 1, 2, 3,
    -1, -1,  0,  0, 0, 1, 1, 2, 3,
    -2, -1, -1,  0, 0, 1, 1, 2, 3,
    -3, -2, -2, -1, 0, 0, 1, 2, 3,
    -3, -2, -1, -1, 0, 0, 1, 2, 3,
    -3, -2, -1, -1, 0, 0, 1, 2, 3,
    -3, -2, -1, -1, 0, 0, 1, 2, 3,
    -3, -2, -1, -1, 0, 0, 1, 2, 3,
    -3, -2, -1, -1, 0, 0, 1, 2, 3,
};

static const int8_t dist_qp_adj_tab_b_vbr[81] = {
    +0,  0,  0,  0, 0, 2, 3, 3, 4,
    +0,  0,  0,  0, 0, 2, 3, 3, 4,
    -1,  0,  0,  0, 0, 2, 2, 3, 3,
    -1, -1,  0,  0, 0, 1, 2, 2, 2,
    -1, -1, -1,  0, 0, 0, 1, 2, 2,
    -2, -1, -1,  0, 0, 0, 0, 1, 2,
    -2, -1, -1, -1, 0, 0, 0, 1, 3,
    -2, -2, -1, -1, 0, 0, 1, 1, 3,
    -2, -2, -1, -1, 0, 1, 1, 2, 4,
};

static const int8_t buf_rate_adj_tab_i_vbr[72] = {
    -4, -20, -28, -36, -40, -44, -48, -80,
    +0,  -8, -12, -20, -24, -28, -32, -36,
    +0,   0,  -8, -16, -20, -24, -28, -32,
    +8,   4,   0,   0,  -8, -16, -24, -28,
    32,  24,  16,   2,  -4,  -8, -16, -20,
    36,  32,  28,  16,   8,   0,  -4,  -8,
    40,  36,  24,  20,  16,   8,   0,  -8,
    48,  40,  28,  24,  20,  12,   0,  -4,
    64,  48,  28,  20,  16,  12,   8,   4,
};

static const int8_t buf_rate_adj_tab_p_vbr[72] = {
    -8, -24, -32, -44, -48, -56, -64, -80,
    -8, -16, -32, -40, -44, -52, -56, -64,
    +0,   0, -16, -28, -36, -40, -44, -48,
    +8,   4,   0,   0,  -8, -16, -24, -36,
    20,  12,   4,   0,  -8,  -8,  -8, -16,
    24,  16,   8,   8,   8,   0,  -4,  -8,
    40,  36,  24,  20,  16,   8,   0,  -8,
    48,  40,  28,  24,  20,  12,   0,  -4,
    64,  48,  28,  20,  16,  12,   8,   4,
};

static const int8_t buf_rate_adj_tab_b_vbr[72] = {
    0,  -4, -8, -16, -24, -32, -40, -48,
    1,   0, -4,  -8, -16, -24, -32, -40,
    4,   2,  0,  -1,  -3,  -8, -16, -24,
    8,   4,  2,   0,  -1,  -4,  -8, -16,
    20, 16,  4,   0,  -1,  -4,  -8, -16,
    24, 20, 16,   8,   4,   0,  -4,  -8,
    28, 24, 20,  16,   8,   4,   0,  -8,
    32, 24, 20,  16,   8,   4,   0,  -4,
    64, 48, 28,  20,  16,  12,   8,   4,
};

static const struct huc_brc_update_constant_data
        gen9_brc_update_constant_data = {
    .global_rate_qp_adj_tab_i = {
        48, 40, 32,  24,  16,   8,   0,  -8,
        40, 32, 24,  16,   8,   0,  -8, -16,
        32, 24, 16,   8,   0,  -8, -16, -24,
        24, 16,  8,   0,  -8, -16, -24, -32,
        16, 8,   0,  -8, -16, -24, -32, -40,
        8,  0,  -8, -16, -24, -32, -40, -48,
        0, -8, -16, -24, -32, -40, -48, -56,
        48, 40, 32,  24,  16,   8,   0,  -8,
    },

    .global_rate_qp_adj_tab_p = {
        48,  40,  32,  24,  16,  8,    0,  -8,
        40,  32,  24,  16,   8,  0,   -8, -16,
        16,   8,   8,   4,  -8, -16, -16, -24,
        8,    0,   0,  -8, -16, -16, -16, -24,
        8,    0,   0, -24, -32, -32, -32, -48,
        0,  -16, -16, -24, -32, -48, -56, -64,
        -8, -16, -32, -32, -48, -48, -56, -64,
        -16, -32, -48, -48, -48, -56, -64, -80,
    },

    .global_rate_qp_adj_tab_b = {
        48, 40, 32, 24,  16,   8,   0,  -8,
        40, 32, 24, 16,  8,    0,  -8, -16,
        32, 24, 16,  8,  0,   -8, -16, -24,
        24, 16, 8,   0, -8,   -8, -16, -24,
        16, 8,  0,   0, -8,  -16, -24, -32,
        16, 8,  0,   0, -8,  -16, -24, -32,
        0, -8, -8, -16, -32, -48, -56, -64,
        0, -8, -8, -16, -32, -48, -56, -64
    },

    .dist_threshld_i = { 2, 4, 8, 12, 19, 32, 64, 128, 0, 0 },
    .dist_threshld_p = { 2, 4, 8, 12, 19, 32, 64, 128, 0, 0 },
    .dist_threshld_b = { 2, 4, 8, 12, 19, 32, 64, 128, 0, 0 },

    .dist_qp_adj_tab_i = {
        0,   0,  0,  0,  0,  3,  4,  6,  8,
        0,   0,  0,  0,  0,  2,  3,  5,  7,
        -1,  0,  0,  0,  0,  2,  2,  4,  5,
        -1, -1,  0,  0,  0,  1,  2,  2,  4,
        -2, -2, -1,  0,  0,  0,  1,  2,  4,
        -2, -2, -1,  0,  0,  0,  1,  2,  4,
        -3, -2, -1, -1,  0,  0,  1,  2,  5,
        -3, -2, -1, -1,  0,  0,  2,  4,  7,
        -4, -3, -2, -1,  0,  1,  3,  5,  8,
    },

    .dist_qp_adj_tab_p = {
        -1,   0,  0,  0,  0,  1,  1,  2,  3,
        -1,  -1,  0,  0,  0,  1,  1,  2,  3,
        -2,  -1, -1,  0,  0,  1,  1,  2,  3,
        -3,  -2, -2, -1,  0,  0,  1,  2,  3,
        -3,  -2, -1, -1,  0,  0,  1,  2,  3,
        -3,  -2, -1, -1,  0,  0,  1,  2,  3,
        -3,  -2, -1, -1,  0,  0,  1,  2,  3,
        -3,  -2, -1, -1,  0,  0,  1,  2,  3,
        -3,  -2, -1, -1,  0,  0,  1,  2,  3,
    },

    .dist_qp_adj_tab_b = {
        0,   0,  0,  0, 0, 2, 3, 3, 4,
        0,   0,  0,  0, 0, 2, 3, 3, 4,
        -1,  0,  0,  0, 0, 2, 2, 3, 3,
        -1, -1,  0,  0, 0, 1, 2, 2, 2,
        -1, -1, -1,  0, 0, 0, 1, 2, 2,
        -2, -1, -1,  0, 0, 0, 0, 1, 2,
        -2, -1, -1, -1, 0, 0, 0, 1, 3,
        -2, -2, -1, -1, 0, 0, 1, 1, 3,
        -2, -2, -1, -1, 0, 1, 1, 2, 4,
    },

    /* default table for non lowdelay */
    .buf_rate_adj_tab_i = {
        -4, -20, -28, -36, -40, -44, -48, -80,
        0,   -8, -12, -20, -24, -28, -32, -36,
        0,    0,  -8, -16, -20, -24, -28, -32,
        8,    4,   0,   0,  -8, -16, -24, -28,
        32,  24,  16,   2,  -4,  -8, -16, -20,
        36,  32,  28,  16,   8,   0,  -4,  -8,
        40,  36,  24,  20,  16,   8,   0,  -8,
        48,  40,  28,  24,  20,  12,   0,  -4,
        64,  48,  28,  20,  16,  12,   8,   4,
    },

    /* default table for non lowdelay */
    .buf_rate_adj_tab_p = {
        -8, -24, -32, -44, -48, -56, -64, -80,
        -8, -16, -32, -40, -44, -52, -56, -64,
        0,    0, -16, -28, -36, -40, -44, -48,
        8,    4,   0,   0,  -8, -16, -24, -36,
        20,  12,   4,   0,  -8,  -8,  -8, -16,
        24,  16,   8,   8,   8,   0,  -4,  -8,
        40,  36,  24,  20,  16,   8,   0,  -8,
        48,  40,  28,  24,  20,  12,   0,  -4,
        64,  48,  28,  20,  16,  12,   8,   4,
    },

    /* default table for non lowdelay */
    .buf_rate_adj_tab_b = {
        0,  -4, -8, -16, -24, -32, -40, -48,
        1,   0, -4,  -8, -16, -24, -32, -40,
        4,   2,  0,  -1,  -3,  -8, -16, -24,
        8,   4,  2,   0,  -1,  -4,  -8, -16,
        20, 16,  4,   0,  -1,  -4,  -8, -16,
        24, 20, 16,   8,   4,   0,  -4,  -8,
        28, 24, 20,  16,   8,   4,   0,  -8,
        32, 24, 20,  16,   8,   4,   0,  -4,
        64, 48, 28,  20,  16,  12,   8,   4,
    },

    .frame_size_min_tab_p = { 1, 2, 4, 6, 8, 10, 16, 16, 16 },
    .frame_size_min_tab_i = { 1, 2, 4, 8, 16, 20, 24, 32, 36 },

    .frame_size_max_tab_p = { 48, 64, 80, 96, 112, 128, 144, 160, 160 },
    .frame_size_max_tab_i = { 48, 64, 80, 96, 112, 128, 144, 160, 160 },

    .frame_size_scg_tab_p = { 4, 8, 12, 16, 20, 24, 24, 0, 0 },
    .frame_size_scg_tab_i = { 4, 8, 12, 16, 20, 24, 24, 0, 0 },

    .i_intra_non_pred = {
        0x0e, 0x0e, 0x0e, 0x18, 0x19, 0x1b, 0x1c, 0x0d, 0x0f, 0x18, 0x19, 0x0d, 0x0f, 0x0f,
        0x0c, 0x0e, 0x0c, 0x0c, 0x0a, 0x0a, 0x0b, 0x0a, 0x0a, 0x0a, 0x09, 0x09, 0x08, 0x08,
        0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x07, 0x07, 0x07, 0x07, 0x07,
    },

    .i_intra_16x16 = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    },

    .i_intra_8x8 = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x04, 0x04, 0x04, 0x04, 0x06, 0x06, 0x06,
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x07, 0x07, 0x07, 0x07, 0x07,
    },

    .i_intra_4x4 = {
        0x2e, 0x2e, 0x2e, 0x38, 0x39, 0x3a, 0x3b, 0x2c, 0x2e, 0x38, 0x39, 0x2d, 0x2f, 0x38,
        0x2e, 0x38, 0x2e, 0x38, 0x2f, 0x2e, 0x38, 0x38, 0x38, 0x38, 0x2f, 0x2f, 0x2f, 0x2e,
        0x2d, 0x2c, 0x2b, 0x2a, 0x29, 0x28, 0x1e, 0x1c, 0x1b, 0x1a, 0x19, 0x18, 0x0e, 0x0d,
    },

    .i_intra_chroma = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    },

    .p_intra_non_pred = {
        0x06, 0x06, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x05, 0x06, 0x07, 0x08, 0x06, 0x07, 0x07,
        0x07, 0x07, 0x06, 0x07, 0x07, 0x06, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    },

    .p_intra_16x16 = {
        0x1b, 0x1b, 0x1b, 0x1c, 0x1e, 0x28, 0x29, 0x1a, 0x1b, 0x1c, 0x1e, 0x1a, 0x1c, 0x1d,
        0x1b, 0x1c, 0x1c, 0x1c, 0x1c, 0x1b, 0x1c, 0x1c, 0x1d, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c,
        0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c
    },

    .p_intra_8x8 = {
        0x1d, 0x1d, 0x1d, 0x1e, 0x28, 0x29, 0x2a, 0x1b, 0x1d, 0x1e, 0x28, 0x1c, 0x1d, 0x1f,
        0x1d, 0x1e, 0x1d, 0x1e, 0x1d, 0x1d, 0x1f, 0x1e, 0x1e, 0x1e, 0x1d, 0x1e, 0x1e, 0x1d,
        0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
    },

    .p_intra_4x4 = {
        0x38, 0x38, 0x38, 0x39, 0x3a, 0x3b, 0x3d, 0x2e, 0x38, 0x39, 0x3a, 0x2f, 0x39, 0x3a,
        0x38, 0x39, 0x38, 0x39, 0x39, 0x38, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
    },

    .p_intra_chroma = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    },

    .p_inter_16x8 = {
        0x07, 0x07, 0x07, 0x08, 0x09, 0x0b, 0x0c, 0x06, 0x07, 0x09, 0x0a, 0x07, 0x08, 0x09,
        0x08, 0x09, 0x08, 0x09, 0x08, 0x08, 0x09, 0x09, 0x09, 0x09, 0x08, 0x08, 0x08, 0x08,
        0x08, 0x08, 0x08, 0x08, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
    },

    .p_inter_8x8 = {
        0x02, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x02, 0x02, 0x02, 0x03, 0x02, 0x02, 0x02,
        0x02, 0x03, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    },

    .p_inter_16x16 = {
        0x05, 0x05, 0x05, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    },

    .p_ref_id = {
        0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
        0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
        0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04
    },

    .hme_mv_cost = {
        /* mv = 0 */
        {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        },

        /* mv <= 16 */
        {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        },

        /* mv <= 32 */
        {
            0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
            0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
            0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        },

        /* mv <= 64 */
        {
            0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
            0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
            0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
        },

        /* mv <= 128 */
        {
            0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
            0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
            0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
        },

        /* mv <= 256 */
        {
            0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
            0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
            0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
        },

        /* mv <= 512 */
        {
            0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
            0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
            0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f,
        },

        /* mv <= 1024 */
        {
            0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
            0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
            0x1a, 0x1a, 0x1a, 0x1a, 0x1f, 0x2a, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d
        },
    },
};

/* 11 DWs */
static const uint8_t vdenc_const_qp_lambda[44] = {
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02,
    0x02, 0x03, 0x03, 0x03, 0x04, 0x04, 0x05, 0x05, 0x06, 0x07,
    0x07, 0x08, 0x09, 0x0a, 0x0c, 0x0d, 0x0f, 0x11, 0x13, 0x15,
    0x17, 0x1a, 0x1e, 0x21, 0x25, 0x2a, 0x2f, 0x35, 0x3b, 0x42,
    0x4a, 0x53, 0x00, 0x00
};

/* 14 DWs */
static const uint16_t vdenc_const_skip_threshold[28] = {

};

/* 14 DWs */
static const uint16_t vdenc_const_sic_forward_transform_coeff_threshold_0[28] = {

};

/* 7 DWs */
static const uint8_t vdenc_const_sic_forward_transform_coeff_threshold_1[28] = {

};

/* 7 DWs */
static const uint8_t vdenc_const_sic_forward_transform_coeff_threshold_2[28] = {

};

/* 7 DWs */
static const uint8_t vdenc_const_sic_forward_transform_coeff_threshold_3[28] = {

};

/* P frame */
/* 11 DWs */
static const uint8_t vdenc_const_qp_lambda_p[44] = {
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02,
    0x02, 0x03, 0x03, 0x03, 0x04, 0x04, 0x05, 0x05, 0x06, 0x07,
    0x07, 0x08, 0x09, 0x0a, 0x0c, 0x0d, 0x0f, 0x11, 0x13, 0x15,
    0x17, 0x1a, 0x1e, 0x21, 0x25, 0x2a, 0x2f, 0x35, 0x3b, 0x42,
    0x4a, 0x53, 0x00, 0x00
};

/* 14 DWs */
static const uint16_t vdenc_const_skip_threshold_p[28] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0002, 0x0004, 0x0007, 0x000b,
    0x0011, 0x0019, 0x0023, 0x0032, 0x0044, 0x005b, 0x0077, 0x0099,
    0x00c2, 0x00f1, 0x0128, 0x0168, 0x01b0, 0x0201, 0x025c, 0x02c2,
    0x0333, 0x03b0, 0x0000, 0x0000
};

/* 14 DWs */
static const uint16_t vdenc_const_sic_forward_transform_coeff_threshold_0_p[28] = {
    0x02, 0x02, 0x03, 0x04, 0x04, 0x05, 0x07, 0x09, 0x0b, 0x0e,
    0x12, 0x14, 0x18, 0x1d, 0x20, 0x25, 0x2a, 0x34, 0x39, 0x3f,
    0x4e, 0x51, 0x5b, 0x63, 0x6f, 0x7f, 0x00, 0x00
};

/* 7 DWs */
static const uint8_t vdenc_const_sic_forward_transform_coeff_threshold_1_p[28] = {
    0x03, 0x04, 0x05, 0x05, 0x07, 0x09, 0x0b, 0x0e, 0x12, 0x17,
    0x1c, 0x21, 0x27, 0x2c, 0x33, 0x3b, 0x41, 0x51, 0x5c, 0x1a,
    0x1e, 0x21, 0x22, 0x26, 0x2c, 0x30, 0x00, 0x00
};

/* 7 DWs */
static const uint8_t vdenc_const_sic_forward_transform_coeff_threshold_2_p[28] = {
    0x02, 0x02, 0x03, 0x04, 0x04, 0x05, 0x07, 0x09, 0x0b, 0x0e,
    0x12, 0x14, 0x18, 0x1d, 0x20, 0x25, 0x2a, 0x34, 0x39, 0x0f,
    0x13, 0x14, 0x16, 0x18, 0x1b, 0x1f, 0x00, 0x00
};

/* 7 DWs */
static const uint8_t vdenc_const_sic_forward_transform_coeff_threshold_3_p[28] = {
    0x04, 0x05, 0x06, 0x09, 0x0b, 0x0d, 0x12, 0x16, 0x1b, 0x23,
    0x2c, 0x33, 0x3d, 0x45, 0x4f, 0x5b, 0x66, 0x7f, 0x8e, 0x2a,
    0x2f, 0x32, 0x37, 0x3c, 0x45, 0x4c, 0x00, 0x00
};

static const double
vdenc_brc_dev_threshi0_fp_neg[4] = { 0.80, 0.60, 0.34, 0.2 };

static const double
vdenc_brc_dev_threshi0_fp_pos[4] = { 0.2, 0.4, 0.66, 0.9 };

static const double
vdenc_brc_dev_threshpb0_fp_neg[4] = { 0.90, 0.66, 0.46, 0.3 };

static const double
vdenc_brc_dev_threshpb0_fp_pos[4] = { 0.3, 0.46, 0.70, 0.90 };

static const double
vdenc_brc_dev_threshvbr0_neg[4] = { 0.90, 0.70, 0.50, 0.3 };

static const double
vdenc_brc_dev_threshvbr0_pos[4] = { 0.4, 0.5, 0.75, 0.90 };

static const unsigned char
vdenc_brc_estrate_thresh_p0[7] = { 4, 8, 12, 16, 20, 24, 28 };

static const unsigned char
vdenc_brc_estrate_thresh_i0[7] = { 4, 8, 12, 16, 20, 24, 28 };

static const uint16_t
vdenc_brc_start_global_adjust_frame[4] = { 10, 50, 100, 150 };

static const uint8_t
vdenc_brc_global_rate_ratio_threshold[7] = { 80, 90, 95, 101, 105, 115, 130};

static const uint8_t
vdenc_brc_start_global_adjust_mult[5] = { 1, 1, 3, 2, 1 };

static const uint8_t
vdenc_brc_start_global_adjust_div[5] = { 40, 5, 5, 3, 1 };

static const int8_t
vdenc_brc_global_rate_ratio_threshold_qp[8] = { -3, -2, -1, 0, 1, 1, 2, 3 };

static const int vdenc_mode_const[2][12][52] = {
    //INTRASLICE
    {
        //LUTMODE_INTRA_NONPRED
        {
            14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,         //QP=[0 ~12]
            16, 18, 22, 24, 13, 15, 16, 18, 13, 15, 15, 12, 14,         //QP=[13~25]
            12, 12, 10, 10, 11, 10, 10, 10, 9, 9, 8, 8, 8,              //QP=[26~38]
            8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 7,                      //QP=[39~51]
        },

        //LUTMODE_INTRA_16x16, LUTMODE_INTRA
        {
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    //QP=[0 ~12]
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    //QP=[13~25]
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    //QP=[26~38]
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    //QP=[39~51]
        },

        //LUTMODE_INTRA_8x8
        {
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //QP=[0 ~12]
            0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1,  //QP=[13~25]
            1, 1, 1, 1, 1, 4, 4, 4, 4, 6, 6, 6, 6,  //QP=[26~38]
            6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7,  //QP=[39~51]
        },

        //LUTMODE_INTRA_4x4
        {
            56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56,   //QP=[0 ~12]
            64, 72, 80, 88, 48, 56, 64, 72, 53, 59, 64, 56, 64,   //QP=[13~25]
            57, 64, 58, 55, 64, 64, 64, 64, 59, 59, 60, 57, 50,   //QP=[26~38]
            46, 42, 38, 34, 31, 27, 23, 22, 19, 18, 16, 14, 13,   //QP=[39~51]
        },

        //LUTMODE_INTER_16x8, LUTMODE_INTER_8x16
        { 0, },

        //LUTMODE_INTER_8X8Q
        { 0, },

        //LUTMODE_INTER_8X4Q, LUTMODE_INTER_4X8Q, LUTMODE_INTER_16x8_FIELD
        { 0, },

        //LUTMODE_INTER_4X4Q, LUTMODE_INTER_8X8_FIELD
        { 0, },

        //LUTMODE_INTER_16x16, LUTMODE_INTER
        { 0, },

        //LUTMODE_INTER_BWD
        { 0, },

        //LUTMODE_REF_ID
        { 0, },

        //LUTMODE_INTRA_CHROMA
        { 0, },
    },

    //PREDSLICE
    {
        //LUTMODE_INTRA_NONPRED
        {
            6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,     //QP=[0 ~12]
            7, 8, 9, 10, 5, 6, 7, 8, 6, 7, 7, 7, 7,    //QP=[13~25]
            6, 7, 7, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7,     //QP=[26~38]
            7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,     //QP=[39~51]
        },

        //LUTMODE_INTRA_16x16, LUTMODE_INTRA
        {
            21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
            24, 28, 31, 35, 19, 21, 24, 28, 20, 24, 25, 21, 24,
            24, 24, 24, 21, 24, 24, 26, 24, 24, 24, 24, 24, 24,
            24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,

        },

        //LUTMODE_INTRA_8x8
        {
            26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26,   //QP=[0 ~12]
            28, 32, 36, 40, 22, 26, 28, 32, 24, 26, 30, 26, 28,   //QP=[13~25]
            26, 28, 26, 26, 30, 28, 28, 28, 26, 28, 28, 26, 28,   //QP=[26~38]
            28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,   //QP=[39~51]
        },

        //LUTMODE_INTRA_4x4
        {
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,   //QP=[0 ~12]
            72, 80, 88, 104, 56, 64, 72, 80, 58, 68, 76, 64, 68,  //QP=[13~25]
            64, 68, 68, 64, 70, 70, 70, 70, 68, 68, 68, 68, 68,   //QP=[26~38]
            68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68,   //QP=[39~51]
        },

        //LUTMODE_INTER_16x8, LUTMODE_INTER_8x16
        {
            7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,      //QP=[0 ~12]
            8, 9, 11, 12, 6, 7, 9, 10, 7, 8, 9, 8, 9,   //QP=[13~25]
            8, 9, 8, 8, 9, 9, 9, 9, 8, 8, 8, 8, 8,      //QP=[26~38]
            8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,      //QP=[39~51]
        },

        //LUTMODE_INTER_8X8Q
        {
            2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,   //QP=[0 ~12]
            2, 3, 3, 3, 2, 2, 2, 3, 2, 2, 2, 2, 3,   //QP=[13~25]
            2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,   //QP=[26~38]
            3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,   //QP=[39~51]
        },

        //LUTMODE_INTER_8X4Q, LUTMODE_INTER_4X8Q, LUTMODE_INTER_16X8_FIELD
        {
            5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,   //QP=[0 ~12]
            5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,   //QP=[13~25]
            5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,   //QP=[26~38]
            5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,   //QP=[39~51]
        },

        //LUTMODE_INTER_4X4Q, LUTMODE_INTER_8x8_FIELD
        {
            7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,   //QP=[0 ~12]
            7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,   //QP=[13~25]
            7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,   //QP=[26~38]
            7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,   //QP=[39~51]
        },

        //LUTMODE_INTER_16x16, LUTMODE_INTER
        {
            5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,   //QP=[0 ~12]
            6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,   //QP=[13~25]
            6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,   //QP=[26~38]
            6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,   //QP=[39~51]
        },

        //LUTMODE_INTER_BWD
        {
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    //QP=[0 ~12]
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    //QP=[13~25]
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    //QP=[26~38]
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    //QP=[39~51]
        },

        //LUTMODE_REF_ID
        {
            4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,    //QP=[0 ~12]
            4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,    //QP=[13~25]
            4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,    //QP=[26~38]
            4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,    //QP=[39~51]
        },

        //LUTMODE_INTRA_CHROMA
        {
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    //QP=[0 ~12]
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    //QP=[13~25]
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    //QP=[26~38]
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    //QP=[39~51]
        },
    },
};

static const int vdenc_mv_cost_skipbias_qpel[8] = {
    //PREDSLICE
    0, 6, 6, 9, 10, 13, 14, 16
};

static const int vdenc_hme_cost[8][52] = {
    //mv=0
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     //QP=[0 ~12]
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     //QP=[13 ~25]
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     //QP=[26 ~38]
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     //QP=[39 ~51]
    },
    //mv<=16
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     //QP=[0 ~12]
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     //QP=[13 ~25]
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     //QP=[26 ~38]
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     //QP=[39 ~51]
    },
    //mv<=32
    {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,     //QP=[0 ~12]
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,     //QP=[13 ~25]
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,     //QP=[26 ~38]
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,     //QP=[39 ~51]
    },
    //mv<=64
    {
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,     //QP=[0 ~12]
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,     //QP=[13 ~25]
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,     //QP=[26 ~38]
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,     //QP=[39 ~51]
    },
    //mv<=128
    {
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,     //QP=[0 ~12]
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,     //QP=[13 ~25]
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,     //QP=[26 ~38]
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,     //QP=[39 ~51]
    },
    //mv<=256
    {
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,     //QP=[0 ~12]
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,     //QP=[13 ~25]
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,     //QP=[26 ~38]
        10, 10, 10, 10, 20, 30, 40, 50, 50, 50, 50, 50, 50,     //QP=[39 ~51]
    },
    //mv<=512
    {
        20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,     //QP=[0 ~12]
        20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,     //QP=[13 ~25]
        20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,     //QP=[26 ~38]
        20, 20, 20, 40, 60, 80, 100, 100, 100, 100, 100, 100, 100,     //QP=[39 ~51]
    },

    //mv<=1024
    {
        20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,     //QP=[0 ~12]
        20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,     //QP=[13 ~25]
        20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,     //QP=[26 ~38]
        20, 20, 30, 50, 100, 200, 200, 200, 200, 200, 200, 200, 200,     //QP=[39 ~51]
    },
};

#define OUT_BUFFER_2DW(batch, bo, is_target, delta)  do {               \
        if (bo) {                                                       \
            OUT_BCS_RELOC64(batch,                                      \
                            bo,                                         \
                            I915_GEM_DOMAIN_RENDER,                     \
                            is_target ? I915_GEM_DOMAIN_RENDER : 0,     \
                            delta);                                     \
        } else {                                                        \
            OUT_BCS_BATCH(batch, 0);                                    \
            OUT_BCS_BATCH(batch, 0);                                    \
        }                                                               \
    } while (0)

#define OUT_BUFFER_3DW(batch, bo, is_target, delta, attr)  do { \
        OUT_BUFFER_2DW(batch, bo, is_target, delta);            \
        OUT_BCS_BATCH(batch, i965->intel.mocs_state);                             \
    } while (0)

#define ALLOC_VDENC_BUFFER_RESOURCE(buffer, bfsize, des) do {   \
        buffer.type = I965_GPE_RESOURCE_BUFFER;                 \
        buffer.width = bfsize;                                  \
        buffer.height = 1;                                      \
        buffer.pitch = buffer.width;                            \
        buffer.size = buffer.pitch;                             \
        buffer.tiling = I915_TILING_NONE;                       \
        i965_allocate_gpe_resource(i965->intel.bufmgr,          \
                                   &buffer,                     \
                                   bfsize,                      \
                                   (des));                      \
    } while (0)

static int
gen9_vdenc_get_max_vmv_range(int level)
{
    int max_vmv_range = 512;

    if (level == 10)
        max_vmv_range = 256;
    else if (level <= 20)
        max_vmv_range = 512;
    else if (level <= 30)
        max_vmv_range = 1024;
    else
        max_vmv_range = 2048;

    return max_vmv_range;
}

static unsigned char
map_44_lut_value(unsigned int v, unsigned char max)
{
    unsigned int maxcost;
    int d;
    unsigned char ret;

    if (v == 0) {
        return 0;
    }

    maxcost = ((max & 15) << (max >> 4));

    if (v >= maxcost) {
        return max;
    }

    d = (int)(log((double)v) / log(2.0)) - 3;

    if (d < 0) {
        d = 0;
    }

    ret = (unsigned char)((d << 4) + (int)((v + (d == 0 ? 0 : (1 << (d - 1)))) >> d));
    ret = (ret & 0xf) == 0 ? (ret | 8) : ret;

    return ret;
}

static void
gen9_vdenc_update_misc_parameters(VADriverContextP ctx,
                                  struct encode_state *encode_state,
                                  struct intel_encoder_context *encoder_context)
{
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    int i;

    vdenc_context->gop_size = encoder_context->brc.gop_size;
    vdenc_context->ref_dist = encoder_context->brc.num_bframes_in_gop + 1;

    if (vdenc_context->internal_rate_mode != I965_BRC_CQP &&
        encoder_context->brc.need_reset) {
        /* So far, vdenc doesn't support temporal layer */
        vdenc_context->framerate = encoder_context->brc.framerate[0];

        vdenc_context->vbv_buffer_size_in_bit = encoder_context->brc.hrd_buffer_size;
        vdenc_context->init_vbv_buffer_fullness_in_bit = encoder_context->brc.hrd_initial_buffer_fullness;

        vdenc_context->max_bit_rate = encoder_context->brc.bits_per_second[0];
        vdenc_context->mb_brc_enabled = encoder_context->brc.mb_rate_control[0] == 1;
        vdenc_context->brc_need_reset = (vdenc_context->brc_initted && encoder_context->brc.need_reset);

        if (vdenc_context->internal_rate_mode == I965_BRC_CBR) {
            vdenc_context->min_bit_rate = vdenc_context->max_bit_rate;
            vdenc_context->target_bit_rate = vdenc_context->max_bit_rate;
        } else {
            assert(vdenc_context->internal_rate_mode == I965_BRC_VBR);
            vdenc_context->min_bit_rate = vdenc_context->max_bit_rate * (2 * encoder_context->brc.target_percentage[0] - 100) / 100;
            vdenc_context->target_bit_rate = vdenc_context->max_bit_rate * encoder_context->brc.target_percentage[0] / 100;
        }
    }

    vdenc_context->mb_brc_enabled = 1;
    vdenc_context->num_roi = MIN(encoder_context->brc.num_roi, 3);
    vdenc_context->max_delta_qp = encoder_context->brc.roi_max_delta_qp;
    vdenc_context->min_delta_qp = encoder_context->brc.roi_min_delta_qp;
    vdenc_context->vdenc_streamin_enable = !!vdenc_context->num_roi;

    for (i = 0; i < vdenc_context->num_roi; i++) {
        vdenc_context->roi[i].left = encoder_context->brc.roi[i].left >> 4;
        vdenc_context->roi[i].right = encoder_context->brc.roi[i].right >> 4;
        vdenc_context->roi[i].top = encoder_context->brc.roi[i].top >> 4;
        vdenc_context->roi[i].bottom = encoder_context->brc.roi[i].bottom >> 4;
        vdenc_context->roi[i].value = encoder_context->brc.roi[i].value;
    }
}

static void
gen9_vdenc_update_parameters(VADriverContextP ctx,
                             VAProfile profile,
                             struct encode_state *encode_state,
                             struct intel_encoder_context *encoder_context)
{
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    VAEncSequenceParameterBufferH264 *seq_param = (VAEncSequenceParameterBufferH264 *)encode_state->seq_param_ext->buffer;
    VAEncPictureParameterBufferH264 *pic_param = (VAEncPictureParameterBufferH264 *)encode_state->pic_param_ext->buffer;

    if (profile == VAProfileH264High)
        vdenc_context->transform_8x8_mode_enable = !!pic_param->pic_fields.bits.transform_8x8_mode_flag;
    else
        vdenc_context->transform_8x8_mode_enable = 0;

    vdenc_context->frame_width_in_mbs = seq_param->picture_width_in_mbs;
    vdenc_context->frame_height_in_mbs = seq_param->picture_height_in_mbs;

    vdenc_context->frame_width = vdenc_context->frame_width_in_mbs * 16;
    vdenc_context->frame_height = vdenc_context->frame_height_in_mbs * 16;

    vdenc_context->down_scaled_width_in_mb4x = WIDTH_IN_MACROBLOCKS(vdenc_context->frame_width / SCALE_FACTOR_4X);
    vdenc_context->down_scaled_height_in_mb4x = HEIGHT_IN_MACROBLOCKS(vdenc_context->frame_height / SCALE_FACTOR_4X);
    vdenc_context->down_scaled_width_4x = vdenc_context->down_scaled_width_in_mb4x * 16;
    vdenc_context->down_scaled_height_4x = ((vdenc_context->down_scaled_height_in_mb4x + 1) >> 1) * 16;
    vdenc_context->down_scaled_height_4x = ALIGN(vdenc_context->down_scaled_height_4x, 32) << 1;

    gen9_vdenc_update_misc_parameters(ctx, encode_state, encoder_context);

    vdenc_context->current_pass = 0;
    vdenc_context->num_passes = 1;

    if (vdenc_context->internal_rate_mode == I965_BRC_CBR ||
        vdenc_context->internal_rate_mode == I965_BRC_VBR)
        vdenc_context->brc_enabled = 1;
    else
        vdenc_context->brc_enabled = 0;

    if (vdenc_context->brc_enabled &&
        (!vdenc_context->init_vbv_buffer_fullness_in_bit ||
         !vdenc_context->vbv_buffer_size_in_bit ||
         !vdenc_context->max_bit_rate ||
         !vdenc_context->target_bit_rate ||
         !vdenc_context->framerate.num ||
         !vdenc_context->framerate.den))
        vdenc_context->brc_enabled = 0;

    if (!vdenc_context->brc_enabled) {
        vdenc_context->target_bit_rate = 0;
        vdenc_context->max_bit_rate = 0;
        vdenc_context->min_bit_rate = 0;
        vdenc_context->init_vbv_buffer_fullness_in_bit = 0;
        vdenc_context->vbv_buffer_size_in_bit = 0;
    } else {
        vdenc_context->num_passes = NUM_OF_BRC_PAK_PASSES;
    }
}

static void
gen9_vdenc_avc_calculate_mode_cost(VADriverContextP ctx,
                                   struct encode_state *encode_state,
                                   struct intel_encoder_context *encoder_context,
                                   int qp)
{
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    unsigned int frame_type = vdenc_context->frame_type;

    memset(vdenc_context->mode_cost, 0, sizeof(vdenc_context->mode_cost));
    memset(vdenc_context->mv_cost, 0, sizeof(vdenc_context->mv_cost));
    memset(vdenc_context->hme_mv_cost, 0, sizeof(vdenc_context->hme_mv_cost));

    vdenc_context->mode_cost[VDENC_LUTMODE_INTRA_NONPRED] = map_44_lut_value((uint32_t)(vdenc_mode_const[frame_type][VDENC_LUTMODE_INTRA_NONPRED][qp]), 0x6f);
    vdenc_context->mode_cost[VDENC_LUTMODE_INTRA_16x16] = map_44_lut_value((uint32_t)(vdenc_mode_const[frame_type][VDENC_LUTMODE_INTRA_16x16][qp]), 0x8f);
    vdenc_context->mode_cost[VDENC_LUTMODE_INTRA_8x8] = map_44_lut_value((uint32_t)(vdenc_mode_const[frame_type][VDENC_LUTMODE_INTRA_8x8][qp]), 0x8f);
    vdenc_context->mode_cost[VDENC_LUTMODE_INTRA_4x4] = map_44_lut_value((uint32_t)(vdenc_mode_const[frame_type][VDENC_LUTMODE_INTRA_4x4][qp]), 0x8f);

    if (frame_type == VDENC_FRAME_P) {
        vdenc_context->mode_cost[VDENC_LUTMODE_INTER_16x16] = map_44_lut_value((uint32_t)(vdenc_mode_const[frame_type][VDENC_LUTMODE_INTER_16x16][qp]), 0x8f);
        vdenc_context->mode_cost[VDENC_LUTMODE_INTER_16x8] = map_44_lut_value((uint32_t)(vdenc_mode_const[frame_type][VDENC_LUTMODE_INTER_16x8][qp]), 0x8f);
        vdenc_context->mode_cost[VDENC_LUTMODE_INTER_8X8Q] = map_44_lut_value((uint32_t)(vdenc_mode_const[frame_type][VDENC_LUTMODE_INTER_8X8Q][qp]), 0x6f);
        vdenc_context->mode_cost[VDENC_LUTMODE_INTER_8X4Q] = map_44_lut_value((uint32_t)(vdenc_mode_const[frame_type][VDENC_LUTMODE_INTER_8X4Q][qp]), 0x6f);
        vdenc_context->mode_cost[VDENC_LUTMODE_INTER_4X4Q] = map_44_lut_value((uint32_t)(vdenc_mode_const[frame_type][VDENC_LUTMODE_INTER_4X4Q][qp]), 0x6f);
        vdenc_context->mode_cost[VDENC_LUTMODE_REF_ID] = map_44_lut_value((uint32_t)(vdenc_mode_const[frame_type][VDENC_LUTMODE_REF_ID][qp]), 0x6f);

        vdenc_context->mv_cost[0] = map_44_lut_value((uint32_t)(vdenc_mv_cost_skipbias_qpel[0]), 0x6f);
        vdenc_context->mv_cost[1] = map_44_lut_value((uint32_t)(vdenc_mv_cost_skipbias_qpel[1]), 0x6f);
        vdenc_context->mv_cost[2] = map_44_lut_value((uint32_t)(vdenc_mv_cost_skipbias_qpel[2]), 0x6f);
        vdenc_context->mv_cost[3] = map_44_lut_value((uint32_t)(vdenc_mv_cost_skipbias_qpel[3]), 0x6f);
        vdenc_context->mv_cost[4] = map_44_lut_value((uint32_t)(vdenc_mv_cost_skipbias_qpel[4]), 0x6f);
        vdenc_context->mv_cost[5] = map_44_lut_value((uint32_t)(vdenc_mv_cost_skipbias_qpel[5]), 0x6f);
        vdenc_context->mv_cost[6] = map_44_lut_value((uint32_t)(vdenc_mv_cost_skipbias_qpel[6]), 0x6f);
        vdenc_context->mv_cost[7] = map_44_lut_value((uint32_t)(vdenc_mv_cost_skipbias_qpel[7]), 0x6f);

        vdenc_context->hme_mv_cost[0] = map_44_lut_value((uint32_t)(vdenc_hme_cost[0][qp]), 0x6f);
        vdenc_context->hme_mv_cost[1] = map_44_lut_value((uint32_t)(vdenc_hme_cost[1][qp]), 0x6f);
        vdenc_context->hme_mv_cost[2] = map_44_lut_value((uint32_t)(vdenc_hme_cost[2][qp]), 0x6f);
        vdenc_context->hme_mv_cost[3] = map_44_lut_value((uint32_t)(vdenc_hme_cost[3][qp]), 0x6f);
        vdenc_context->hme_mv_cost[4] = map_44_lut_value((uint32_t)(vdenc_hme_cost[4][qp]), 0x6f);
        vdenc_context->hme_mv_cost[5] = map_44_lut_value((uint32_t)(vdenc_hme_cost[5][qp]), 0x6f);
        vdenc_context->hme_mv_cost[6] = map_44_lut_value((uint32_t)(vdenc_hme_cost[6][qp]), 0x6f);
        vdenc_context->hme_mv_cost[7] = map_44_lut_value((uint32_t)(vdenc_hme_cost[7][qp]), 0x6f);
    }
}

static void
gen9_vdenc_update_roi_in_streamin_state(VADriverContextP ctx,
                                        struct intel_encoder_context *encoder_context)
{
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    struct gen9_vdenc_streamin_state *streamin_state;
    int row, col, i;

    if (!vdenc_context->num_roi)
        return;

    streamin_state = (struct gen9_vdenc_streamin_state *)i965_map_gpe_resource(&vdenc_context->vdenc_streamin_res);

    if (!streamin_state)
        return;

    for (col = 0;  col < vdenc_context->frame_width_in_mbs; col++) {
        for (row = 0; row < vdenc_context->frame_height_in_mbs; row++) {
            streamin_state[row * vdenc_context->frame_width_in_mbs + col].dw0.roi_selection = 0; /* non-ROI region */

            /* The last one has higher priority */
            for (i = vdenc_context->num_roi - 1; i >= 0; i--) {
                if ((col >= vdenc_context->roi[i].left && col <= vdenc_context->roi[i].right) &&
                    (row >= vdenc_context->roi[i].top && row <= vdenc_context->roi[i].bottom)) {
                    streamin_state[row * vdenc_context->frame_width_in_mbs + col].dw0.roi_selection = i + 1;

                    break;
                }
            }
        }
    }

    i965_unmap_gpe_resource(&vdenc_context->vdenc_streamin_res);
}

static VAStatus
gen9_vdenc_avc_prepare(VADriverContextP ctx,
                       VAProfile profile,
                       struct encode_state *encode_state,
                       struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    struct i965_coded_buffer_segment *coded_buffer_segment;
    struct object_surface *obj_surface;
    struct object_buffer *obj_buffer;
    VAEncPictureParameterBufferH264 *pic_param;
    VAEncSliceParameterBufferH264 *slice_param;
    VDEncAvcSurface *vdenc_avc_surface;
    dri_bo *bo;
    int i, j, enable_avc_ildb = 0;
    int qp;
    char *pbuffer;

    gen9_vdenc_update_parameters(ctx, profile, encode_state, encoder_context);

    for (j = 0; j < encode_state->num_slice_params_ext && enable_avc_ildb == 0; j++) {
        assert(encode_state->slice_params_ext && encode_state->slice_params_ext[j]->buffer);
        slice_param = (VAEncSliceParameterBufferH264 *)encode_state->slice_params_ext[j]->buffer;

        for (i = 0; i < encode_state->slice_params_ext[j]->num_elements; i++) {
            assert((slice_param->slice_type == SLICE_TYPE_I) ||
                   (slice_param->slice_type == SLICE_TYPE_SI) ||
                   (slice_param->slice_type == SLICE_TYPE_P) ||
                   (slice_param->slice_type == SLICE_TYPE_SP) ||
                   (slice_param->slice_type == SLICE_TYPE_B));

            if (slice_param->disable_deblocking_filter_idc != 1) {
                enable_avc_ildb = 1;
                break;
            }

            slice_param++;
        }
    }

    /* Setup current frame */
    obj_surface = encode_state->reconstructed_object;
    i965_check_alloc_surface_bo(ctx, obj_surface, 1, VA_FOURCC_NV12, SUBSAMPLE_YUV420);

    if (obj_surface->private_data == NULL) {
        vdenc_avc_surface = calloc(sizeof(VDEncAvcSurface), 1);
        assert(vdenc_avc_surface);

        vdenc_avc_surface->ctx = ctx;
        i965_CreateSurfaces(ctx,
                            vdenc_context->down_scaled_width_4x,
                            vdenc_context->down_scaled_height_4x,
                            VA_RT_FORMAT_YUV420,
                            1,
                            &vdenc_avc_surface->scaled_4x_surface_id);
        vdenc_avc_surface->scaled_4x_surface_obj = SURFACE(vdenc_avc_surface->scaled_4x_surface_id);
        assert(vdenc_avc_surface->scaled_4x_surface_obj);
        i965_check_alloc_surface_bo(ctx,
                                    vdenc_avc_surface->scaled_4x_surface_obj,
                                    1,
                                    VA_FOURCC_NV12,
                                    SUBSAMPLE_YUV420);

        obj_surface->private_data = (void *)vdenc_avc_surface;
        obj_surface->free_private_data = (void *)vdenc_free_avc_surface;
    }

    vdenc_avc_surface = (VDEncAvcSurface *)obj_surface->private_data;
    assert(vdenc_avc_surface->scaled_4x_surface_obj);

    /* Reconstructed surfaces */
    i965_free_gpe_resource(&vdenc_context->recon_surface_res);
    i965_free_gpe_resource(&vdenc_context->scaled_4x_recon_surface_res);
    i965_free_gpe_resource(&vdenc_context->post_deblocking_output_res);
    i965_free_gpe_resource(&vdenc_context->pre_deblocking_output_res);

    i965_object_surface_to_2d_gpe_resource(&vdenc_context->recon_surface_res, obj_surface);
    i965_object_surface_to_2d_gpe_resource(&vdenc_context->scaled_4x_recon_surface_res, vdenc_avc_surface->scaled_4x_surface_obj);

    if (enable_avc_ildb) {
        i965_object_surface_to_2d_gpe_resource(&vdenc_context->post_deblocking_output_res, obj_surface);
    } else {
        i965_object_surface_to_2d_gpe_resource(&vdenc_context->pre_deblocking_output_res, obj_surface);
    }


    /* Reference surfaces */
    for (i = 0; i < ARRAY_ELEMS(vdenc_context->list_reference_res); i++) {
        assert(ARRAY_ELEMS(vdenc_context->list_reference_res) ==
               ARRAY_ELEMS(vdenc_context->list_scaled_4x_reference_res));
        i965_free_gpe_resource(&vdenc_context->list_reference_res[i]);
        i965_free_gpe_resource(&vdenc_context->list_scaled_4x_reference_res[i]);
        obj_surface = encode_state->reference_objects[i];

        if (obj_surface && obj_surface->bo) {
            i965_object_surface_to_2d_gpe_resource(&vdenc_context->list_reference_res[i], obj_surface);

            if (obj_surface->private_data == NULL) {
                vdenc_avc_surface = calloc(sizeof(VDEncAvcSurface), 1);
                assert(vdenc_avc_surface);

                vdenc_avc_surface->ctx = ctx;
                i965_CreateSurfaces(ctx,
                                    vdenc_context->down_scaled_width_4x,
                                    vdenc_context->down_scaled_height_4x,
                                    VA_RT_FORMAT_YUV420,
                                    1,
                                    &vdenc_avc_surface->scaled_4x_surface_id);
                vdenc_avc_surface->scaled_4x_surface_obj = SURFACE(vdenc_avc_surface->scaled_4x_surface_id);
                assert(vdenc_avc_surface->scaled_4x_surface_obj);
                i965_check_alloc_surface_bo(ctx,
                                            vdenc_avc_surface->scaled_4x_surface_obj,
                                            1,
                                            VA_FOURCC_NV12,
                                            SUBSAMPLE_YUV420);

                obj_surface->private_data = vdenc_avc_surface;
                obj_surface->free_private_data = gen_free_avc_surface;
            }

            vdenc_avc_surface = obj_surface->private_data;
            i965_object_surface_to_2d_gpe_resource(&vdenc_context->list_scaled_4x_reference_res[i], vdenc_avc_surface->scaled_4x_surface_obj);
        }
    }

    /* Input YUV surface */
    i965_free_gpe_resource(&vdenc_context->uncompressed_input_surface_res);
    i965_object_surface_to_2d_gpe_resource(&vdenc_context->uncompressed_input_surface_res, encode_state->input_yuv_object);

    /* Encoded bitstream */
    obj_buffer = encode_state->coded_buf_object;
    bo = obj_buffer->buffer_store->bo;
    i965_free_gpe_resource(&vdenc_context->compressed_bitstream.res);
    i965_dri_object_to_buffer_gpe_resource(&vdenc_context->compressed_bitstream.res, bo);
    vdenc_context->compressed_bitstream.start_offset = I965_CODEDBUFFER_HEADER_SIZE;
    vdenc_context->compressed_bitstream.end_offset = ALIGN(obj_buffer->size_element - 0x1000, 0x1000);

    /* Status buffer */
    i965_free_gpe_resource(&vdenc_context->status_bffuer.res);
    i965_dri_object_to_buffer_gpe_resource(&vdenc_context->status_bffuer.res, bo);
    vdenc_context->status_bffuer.base_offset = offsetof(struct i965_coded_buffer_segment, codec_private_data);
    vdenc_context->status_bffuer.size = ALIGN(sizeof(struct gen9_vdenc_status), 64);
    vdenc_context->status_bffuer.bytes_per_frame_offset = offsetof(struct gen9_vdenc_status, bytes_per_frame);
    assert(vdenc_context->status_bffuer.base_offset + vdenc_context->status_bffuer.size <
           vdenc_context->compressed_bitstream.start_offset);

    dri_bo_map(bo, 1);

    coded_buffer_segment = (struct i965_coded_buffer_segment *)bo->virtual;
    coded_buffer_segment->mapped = 0;
    coded_buffer_segment->codec = encoder_context->codec;
    coded_buffer_segment->status_support = 1;

    pbuffer = bo->virtual;
    pbuffer += vdenc_context->status_bffuer.base_offset;
    memset(pbuffer, 0, vdenc_context->status_bffuer.size);

    dri_bo_unmap(bo);

    i965_free_gpe_resource(&vdenc_context->mfx_intra_row_store_scratch_res);
    ALLOC_VDENC_BUFFER_RESOURCE(vdenc_context->mfx_intra_row_store_scratch_res,
                                vdenc_context->frame_width_in_mbs * 64,
                                "Intra row store scratch buffer");

    i965_free_gpe_resource(&vdenc_context->mfx_deblocking_filter_row_store_scratch_res);
    ALLOC_VDENC_BUFFER_RESOURCE(vdenc_context->mfx_deblocking_filter_row_store_scratch_res,
                                vdenc_context->frame_width_in_mbs * 256,
                                "Deblocking filter row store scratch buffer");

    i965_free_gpe_resource(&vdenc_context->mfx_bsd_mpc_row_store_scratch_res);
    ALLOC_VDENC_BUFFER_RESOURCE(vdenc_context->mfx_bsd_mpc_row_store_scratch_res,
                                vdenc_context->frame_width_in_mbs * 128,
                                "BSD/MPC row store scratch buffer");

    i965_free_gpe_resource(&vdenc_context->vdenc_row_store_scratch_res);
    ALLOC_VDENC_BUFFER_RESOURCE(vdenc_context->vdenc_row_store_scratch_res,
                                vdenc_context->frame_width_in_mbs * 64,
                                "VDENC row store scratch buffer");

    assert(sizeof(struct gen9_vdenc_streamin_state) == 64);
    i965_free_gpe_resource(&vdenc_context->vdenc_streamin_res);
    ALLOC_VDENC_BUFFER_RESOURCE(vdenc_context->vdenc_streamin_res,
                                vdenc_context->frame_width_in_mbs *
                                vdenc_context->frame_height_in_mbs *
                                sizeof(struct gen9_vdenc_streamin_state),
                                "VDENC StreamIn buffer");

    /*
     * Calculate the index for each reference surface in list0 for the first slice
     * TODO: other slices
     */
    pic_param = (VAEncPictureParameterBufferH264 *)encode_state->pic_param_ext->buffer;
    slice_param = (VAEncSliceParameterBufferH264 *)encode_state->slice_params_ext[0]->buffer;

    vdenc_context->num_refs[0] = pic_param->num_ref_idx_l0_active_minus1 + 1;

    if (slice_param->num_ref_idx_active_override_flag)
        vdenc_context->num_refs[0] = slice_param->num_ref_idx_l0_active_minus1 + 1;

    for (i = 0; i < ARRAY_ELEMS(vdenc_context->list_ref_idx[0]); i++) {
        vdenc_context->list_ref_idx[0][i] = 0xFF;
    }

    if (vdenc_context->num_refs[0] > ARRAY_ELEMS(vdenc_context->list_ref_idx[0]))
        return VA_STATUS_ERROR_INVALID_VALUE;

    for (i = 0; i < ARRAY_ELEMS(vdenc_context->list_ref_idx[0]); i++) {
        VAPictureH264 *va_pic;

        assert(ARRAY_ELEMS(slice_param->RefPicList0) == ARRAY_ELEMS(vdenc_context->list_ref_idx[0]));

        if (i >= vdenc_context->num_refs[0])
            continue;

        va_pic = &slice_param->RefPicList0[i];

        for (j = 0; j < ARRAY_ELEMS(encode_state->reference_objects); j++) {
            obj_surface = encode_state->reference_objects[j];

            if (obj_surface &&
                obj_surface->bo &&
                obj_surface->base.id == va_pic->picture_id) {

                assert(obj_surface->base.id != VA_INVALID_SURFACE);
                vdenc_context->list_ref_idx[0][i] = j;

                break;
            }
        }
    }

    if (slice_param->slice_type == SLICE_TYPE_I ||
        slice_param->slice_type == SLICE_TYPE_SI)
        vdenc_context->frame_type = VDENC_FRAME_I;
    else
        vdenc_context->frame_type = VDENC_FRAME_P;

    qp = pic_param->pic_init_qp + slice_param->slice_qp_delta;

    gen9_vdenc_avc_calculate_mode_cost(ctx, encode_state, encoder_context, qp);
    gen9_vdenc_update_roi_in_streamin_state(ctx, encoder_context);

    return VA_STATUS_SUCCESS;
}

static void
gen9_vdenc_huc_pipe_mode_select(VADriverContextP ctx,
                                struct intel_encoder_context *encoder_context,
                                struct huc_pipe_mode_select_parameter *params)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;

    BEGIN_BCS_BATCH(batch, 3);

    OUT_BCS_BATCH(batch, HUC_PIPE_MODE_SELECT | (3 - 2));
    OUT_BCS_BATCH(batch,
                  (params->huc_stream_object_enable << 10) |
                  (params->indirect_stream_out_enable << 4));
    OUT_BCS_BATCH(batch,
                  params->media_soft_reset_counter);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_vdenc_huc_imem_state(VADriverContextP ctx,
                          struct intel_encoder_context *encoder_context,
                          struct huc_imem_state_parameter *params)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;

    BEGIN_BCS_BATCH(batch, 5);

    OUT_BCS_BATCH(batch, HUC_IMEM_STATE | (5 - 2));
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, params->huc_firmware_descriptor);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_vdenc_huc_dmem_state(VADriverContextP ctx,
                          struct intel_encoder_context *encoder_context,
                          struct huc_dmem_state_parameter *params)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct intel_batchbuffer *batch = encoder_context->base.batch;

    BEGIN_BCS_BATCH(batch, 6);

    OUT_BCS_BATCH(batch, HUC_DMEM_STATE | (6 - 2));
    OUT_BUFFER_3DW(batch, params->huc_data_source_res->bo, 0, 0, 0);
    OUT_BCS_BATCH(batch, params->huc_data_destination_base_address);
    OUT_BCS_BATCH(batch, params->huc_data_length);

    ADVANCE_BCS_BATCH(batch);
}

/*
static void
gen9_vdenc_huc_cfg_state(VADriverContextP ctx,
                         struct intel_encoder_context *encoder_context,
                         struct huc_cfg_state_parameter *params)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;

    BEGIN_BCS_BATCH(batch, 2);

    OUT_BCS_BATCH(batch, HUC_CFG_STATE | (2 - 2));
    OUT_BCS_BATCH(batch, !!params->force_reset);

    ADVANCE_BCS_BATCH(batch);
}
*/
static void
gen9_vdenc_huc_virtual_addr_state(VADriverContextP ctx,
                                  struct intel_encoder_context *encoder_context,
                                  struct huc_virtual_addr_parameter *params)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    int i;

    BEGIN_BCS_BATCH(batch, 49);

    OUT_BCS_BATCH(batch, HUC_VIRTUAL_ADDR_STATE | (49 - 2));

    for (i = 0; i < 16; i++) {
        if (params->regions[i].huc_surface_res && params->regions[i].huc_surface_res->bo)
            OUT_BUFFER_3DW(batch,
                           params->regions[i].huc_surface_res->bo,
                           !!params->regions[i].is_target, 0, 0);
        else
            OUT_BUFFER_3DW(batch, NULL, 0, 0, 0);
    }

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_vdenc_huc_ind_obj_base_addr_state(VADriverContextP ctx,
                                       struct intel_encoder_context *encoder_context,
                                       struct huc_ind_obj_base_addr_parameter *params)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct intel_batchbuffer *batch = encoder_context->base.batch;

    BEGIN_BCS_BATCH(batch, 11);

    OUT_BCS_BATCH(batch, HUC_IND_OBJ_BASE_ADDR_STATE | (11 - 2));

    if (params->huc_indirect_stream_in_object_res)
        OUT_BUFFER_3DW(batch,
                       params->huc_indirect_stream_in_object_res->bo,
                       0, 0, 0);
    else
        OUT_BUFFER_3DW(batch, NULL, 0, 0, 0);

    OUT_BUFFER_2DW(batch, NULL, 0, 0); /* ignore access upper bound */

    if (params->huc_indirect_stream_out_object_res)
        OUT_BUFFER_3DW(batch,
                       params->huc_indirect_stream_out_object_res->bo,
                       1, 0, 0);
    else
        OUT_BUFFER_3DW(batch, NULL, 0, 0, 0);

    OUT_BUFFER_2DW(batch, NULL, 0, 0); /* ignore access upper bound */

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_vdenc_huc_store_huc_status2(VADriverContextP ctx,
                                 struct intel_encoder_context *encoder_context)
{
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct gpe_mi_store_register_mem_parameter mi_store_register_mem_params;
    struct gpe_mi_store_data_imm_parameter mi_store_data_imm_params;

    /* Write HUC_STATUS2 mask (1 << 6) */
    memset(&mi_store_data_imm_params, 0, sizeof(mi_store_data_imm_params));
    mi_store_data_imm_params.bo = vdenc_context->huc_status2_res.bo;
    mi_store_data_imm_params.offset = 0;
    mi_store_data_imm_params.dw0 = (1 << 6);
    gen8_gpe_mi_store_data_imm(ctx, batch, &mi_store_data_imm_params);

    /* Store HUC_STATUS2 */
    memset(&mi_store_register_mem_params, 0, sizeof(mi_store_register_mem_params));
    mi_store_register_mem_params.mmio_offset = VCS0_HUC_STATUS2;
    mi_store_register_mem_params.bo = vdenc_context->huc_status2_res.bo;
    mi_store_register_mem_params.offset = 4;
    gen8_gpe_mi_store_register_mem(ctx, batch, &mi_store_register_mem_params);
}

static void
gen9_vdenc_huc_stream_object(VADriverContextP ctx,
                             struct intel_encoder_context *encoder_context,
                             struct huc_stream_object_parameter *params)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;

    BEGIN_BCS_BATCH(batch, 5);

    OUT_BCS_BATCH(batch, HUC_STREAM_OBJECT | (5 - 2));
    OUT_BCS_BATCH(batch, params->indirect_stream_in_data_length);
    OUT_BCS_BATCH(batch,
                  (1 << 31) |   /* Must be 1 */
                  params->indirect_stream_in_start_address);
    OUT_BCS_BATCH(batch, params->indirect_stream_out_start_address);
    OUT_BCS_BATCH(batch,
                  (!!params->huc_bitstream_enable << 29) |
                  (params->length_mode << 27) |
                  (!!params->stream_out << 26) |
                  (!!params->emulation_prevention_byte_removal << 25) |
                  (!!params->start_code_search_engine << 24) |
                  (params->start_code_byte2 << 16) |
                  (params->start_code_byte1 << 8) |
                  params->start_code_byte0);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_vdenc_huc_start(VADriverContextP ctx,
                     struct intel_encoder_context *encoder_context,
                     struct huc_start_parameter *params)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;

    BEGIN_BCS_BATCH(batch, 2);

    OUT_BCS_BATCH(batch, HUC_START | (2 - 2));
    OUT_BCS_BATCH(batch, !!params->last_stream_object);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_vdenc_vd_pipeline_flush(VADriverContextP ctx,
                             struct intel_encoder_context *encoder_context,
                             struct vd_pipeline_flush_parameter *params)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;

    BEGIN_BCS_BATCH(batch, 2);

    OUT_BCS_BATCH(batch, VD_PIPELINE_FLUSH | (2 - 2));
    OUT_BCS_BATCH(batch,
                  params->mfx_pipeline_command_flush << 19 |
                  params->mfl_pipeline_command_flush << 18 |
                  params->vdenc_pipeline_command_flush << 17 |
                  params->hevc_pipeline_command_flush << 16 |
                  params->vd_command_message_parser_done << 4 |
                  params->mfx_pipeline_done << 3 |
                  params->mfl_pipeline_done << 2 |
                  params->vdenc_pipeline_done << 1 |
                  params->hevc_pipeline_done);

    ADVANCE_BCS_BATCH(batch);
}

static int
gen9_vdenc_get_max_mbps(int level_idc)
{
    int max_mbps = 11880;

    switch (level_idc) {
    case 20:
        max_mbps = 11880;
        break;

    case 21:
        max_mbps = 19800;
        break;

    case 22:
        max_mbps = 20250;
        break;

    case 30:
        max_mbps = 40500;
        break;

    case 31:
        max_mbps = 108000;
        break;

    case 32:
        max_mbps = 216000;
        break;

    case 40:
    case 41:
        max_mbps = 245760;
        break;

    case 42:
        max_mbps = 522240;
        break;

    case 50:
        max_mbps = 589824;
        break;

    case 51:
        max_mbps = 983040;
        break;

    case 52:
        max_mbps = 2073600;
        break;

    default:
        break;
    }

    return max_mbps;
};

static unsigned int
gen9_vdenc_get_profile_level_max_frame(VADriverContextP ctx,
                                       struct intel_encoder_context *encoder_context,
                                       int level_idc)
{
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    double bits_per_mb, tmpf;
    int max_mbps, num_mb_per_frame;
    uint64_t max_byte_per_frame0, max_byte_per_frame1;
    unsigned int ret;

    if (level_idc >= 31 && level_idc <= 40)
        bits_per_mb = 96.0;
    else
        bits_per_mb = 192.0;

    max_mbps = gen9_vdenc_get_max_mbps(level_idc);
    num_mb_per_frame = vdenc_context->frame_width_in_mbs * vdenc_context->frame_height_in_mbs;

    tmpf = (double)num_mb_per_frame;

    if (tmpf < max_mbps / 172.0)
        tmpf = max_mbps / 172.0;

    max_byte_per_frame0 = (uint64_t)(tmpf * bits_per_mb);
    max_byte_per_frame1 = (uint64_t)(((double)max_mbps * vdenc_context->framerate.den) /
                                     (double)vdenc_context->framerate.num * bits_per_mb);

    /* TODO: check VAEncMiscParameterTypeMaxFrameSize */
    ret = (unsigned int)MIN(max_byte_per_frame0, max_byte_per_frame1);
    ret = (unsigned int)MIN(ret, vdenc_context->frame_height * vdenc_context->frame_height);

    return ret;
}

static int
gen9_vdenc_calculate_initial_qp(VADriverContextP ctx,
                                struct encode_state *encode_state,
                                struct intel_encoder_context *encoder_context)
{
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    float x0 = 0, y0 = 1.19f, x1 = 1.75f, y1 = 1.75f;
    unsigned frame_size;
    int qp, delat_qp;

    frame_size = (vdenc_context->frame_width * vdenc_context->frame_height * 3 / 2);
    qp = (int)(1.0 / 1.2 * pow(10.0,
                               (log10(frame_size * 2.0 / 3.0 * vdenc_context->framerate.num /
                                      ((double)vdenc_context->target_bit_rate * vdenc_context->framerate.den)) - x0) *
                               (y1 - y0) / (x1 - x0) + y0) + 0.5);
    qp += 2;
    delat_qp = (int)(9 - (vdenc_context->vbv_buffer_size_in_bit * ((double)vdenc_context->framerate.num) /
                          ((double)vdenc_context->target_bit_rate * vdenc_context->framerate.den)));
    if (delat_qp > 0)
        qp += delat_qp;

    qp = CLAMP(1, 51, qp);
    qp--;

    if (qp < 0)
        qp = 1;

    return qp;
}

static void
gen9_vdenc_update_huc_brc_init_dmem(VADriverContextP ctx,
                                    struct encode_state *encode_state,
                                    struct intel_encoder_context *encoder_context)
{
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    struct huc_brc_init_dmem *dmem;
    VAEncSequenceParameterBufferH264 *seq_param = (VAEncSequenceParameterBufferH264 *)encode_state->seq_param_ext->buffer;
    double input_bits_per_frame, bps_ratio;
    int i;

    vdenc_context->brc_init_reset_input_bits_per_frame =
        ((double)vdenc_context->max_bit_rate * vdenc_context->framerate.den) / vdenc_context->framerate.num;
    vdenc_context->brc_init_current_target_buf_full_in_bits = vdenc_context->brc_init_reset_input_bits_per_frame;
    vdenc_context->brc_target_size = vdenc_context->init_vbv_buffer_fullness_in_bit;

    dmem = (struct huc_brc_init_dmem *)i965_map_gpe_resource(&vdenc_context->brc_init_reset_dmem_res);

    if (!dmem)
        return;

    memset(dmem, 0, sizeof(*dmem));

    dmem->brc_func = vdenc_context->brc_initted ? 2 : 0;

    dmem->frame_width = vdenc_context->frame_width;
    dmem->frame_height = vdenc_context->frame_height;

    dmem->target_bitrate = vdenc_context->target_bit_rate;
    dmem->min_rate = vdenc_context->min_bit_rate;
    dmem->max_rate = vdenc_context->max_bit_rate;
    dmem->buffer_size = vdenc_context->vbv_buffer_size_in_bit;
    dmem->init_buffer_fullness = vdenc_context->init_vbv_buffer_fullness_in_bit;

    if (dmem->init_buffer_fullness > vdenc_context->init_vbv_buffer_fullness_in_bit)
        dmem->init_buffer_fullness = vdenc_context->vbv_buffer_size_in_bit;

    if (vdenc_context->internal_rate_mode == I965_BRC_CBR)
        dmem->brc_flag |= 0x10;
    else if (vdenc_context->internal_rate_mode == I965_BRC_VBR)
        dmem->brc_flag |= 0x20;

    dmem->frame_rate_m = vdenc_context->framerate.num;
    dmem->frame_rate_d = vdenc_context->framerate.den;

    dmem->profile_level_max_frame = gen9_vdenc_get_profile_level_max_frame(ctx, encoder_context, seq_param->level_idc);

    if (vdenc_context->ref_dist && vdenc_context->gop_size > 0)
        dmem->num_p_in_gop = (vdenc_context->gop_size - 1) / vdenc_context->ref_dist;

    dmem->min_qp = 10;
    dmem->max_qp = 51;

    input_bits_per_frame = ((double)vdenc_context->max_bit_rate * vdenc_context->framerate.den) / vdenc_context->framerate.num;
    bps_ratio = input_bits_per_frame /
                ((double)vdenc_context->vbv_buffer_size_in_bit * vdenc_context->framerate.den / vdenc_context->framerate.num);

    if (bps_ratio < 0.1)
        bps_ratio = 0.1;

    if (bps_ratio > 3.5)
        bps_ratio = 3.5;

    for (i = 0; i < 4; i++) {
        dmem->dev_thresh_pb0[i] = (char)(-50 * pow(vdenc_brc_dev_threshpb0_fp_neg[i], bps_ratio));
        dmem->dev_thresh_pb0[i + 4] = (char)(50 * pow(vdenc_brc_dev_threshpb0_fp_pos[i], bps_ratio));

        dmem->dev_thresh_i0[i] = (char)(-50 * pow(vdenc_brc_dev_threshi0_fp_neg[i], bps_ratio));
        dmem->dev_thresh_i0[i + 4] = (char)(50 * pow(vdenc_brc_dev_threshi0_fp_pos[i], bps_ratio));

        dmem->dev_thresh_vbr0[i] = (char)(-50 * pow(vdenc_brc_dev_threshvbr0_neg[i], bps_ratio));
        dmem->dev_thresh_vbr0[i + 4] = (char)(100 * pow(vdenc_brc_dev_threshvbr0_pos[i], bps_ratio));
    }

    dmem->init_qp_ip = gen9_vdenc_calculate_initial_qp(ctx, encode_state, encoder_context);

    if (vdenc_context->mb_brc_enabled) {
        dmem->mb_qp_ctrl = 1;
        dmem->dist_qp_delta[0] = -5;
        dmem->dist_qp_delta[1] = -2;
        dmem->dist_qp_delta[2] = 2;
        dmem->dist_qp_delta[3] = 5;
    }

    dmem->slice_size_ctrl_en = 0;       /* TODO: add support for slice size control */

    dmem->oscillation_qp_delta = 0;     /* TODO: add support */
    dmem->first_iframe_no_hrd_check = 0;/* TODO: add support */

    // 2nd re-encode pass if possible
    if (vdenc_context->frame_width_in_mbs * vdenc_context->frame_height_in_mbs >= (3840 * 2160 / 256)) {
        dmem->top_qp_delta_thr_for_2nd_pass = 5;
        dmem->bottom_qp_delta_thr_for_2nd_pass = 5;
        dmem->top_frame_size_threshold_for_2nd_pass = 80;
        dmem->bottom_frame_size_threshold_for_2nd_pass = 80;
    } else {
        dmem->top_qp_delta_thr_for_2nd_pass = 2;
        dmem->bottom_qp_delta_thr_for_2nd_pass = 1;
        dmem->top_frame_size_threshold_for_2nd_pass = 32;
        dmem->bottom_frame_size_threshold_for_2nd_pass = 24;
    }

    dmem->qp_select_for_first_pass = 1;
    dmem->mb_header_compensation = 1;
    dmem->delta_qp_adaptation = 1;
    dmem->max_crf_quality_factor = 52;

    dmem->crf_quality_factor = 0;               /* TODO: add support for CRF */
    dmem->scenario_info = 0;

    memcpy(&dmem->estrate_thresh_i0, vdenc_brc_estrate_thresh_i0, sizeof(dmem->estrate_thresh_i0));
    memcpy(&dmem->estrate_thresh_p0, vdenc_brc_estrate_thresh_p0, sizeof(dmem->estrate_thresh_p0));

    i965_unmap_gpe_resource(&vdenc_context->brc_init_reset_dmem_res);
}

static void
gen9_vdenc_huc_brc_init_reset(VADriverContextP ctx,
                              struct encode_state *encode_state,
                              struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    struct huc_pipe_mode_select_parameter pipe_mode_select_params;
    struct huc_imem_state_parameter imem_state_params;
    struct huc_dmem_state_parameter dmem_state_params;
    struct huc_virtual_addr_parameter virtual_addr_params;
    struct huc_ind_obj_base_addr_parameter ind_obj_base_addr_params;
    struct huc_stream_object_parameter stream_object_params;
    struct huc_start_parameter start_params;
    struct vd_pipeline_flush_parameter pipeline_flush_params;
    struct gpe_mi_flush_dw_parameter mi_flush_dw_params;

    vdenc_context->brc_target_size = vdenc_context->init_vbv_buffer_fullness_in_bit;

    memset(&imem_state_params, 0, sizeof(imem_state_params));
    imem_state_params.huc_firmware_descriptor = HUC_BRC_INIT_RESET;
    gen9_vdenc_huc_imem_state(ctx, encoder_context, &imem_state_params);

    memset(&pipe_mode_select_params, 0, sizeof(pipe_mode_select_params));
    gen9_vdenc_huc_pipe_mode_select(ctx, encoder_context, &pipe_mode_select_params);

    gen9_vdenc_update_huc_brc_init_dmem(ctx, encode_state, encoder_context);
    memset(&dmem_state_params, 0, sizeof(dmem_state_params));
    dmem_state_params.huc_data_source_res = &vdenc_context->brc_init_reset_dmem_res;
    dmem_state_params.huc_data_destination_base_address = HUC_DMEM_DATA_OFFSET;
    dmem_state_params.huc_data_length = ALIGN(sizeof(struct huc_brc_init_dmem), 64);
    gen9_vdenc_huc_dmem_state(ctx, encoder_context, &dmem_state_params);

    memset(&virtual_addr_params, 0, sizeof(virtual_addr_params));
    virtual_addr_params.regions[0].huc_surface_res = &vdenc_context->brc_history_buffer_res;
    virtual_addr_params.regions[0].is_target = 1;
    gen9_vdenc_huc_virtual_addr_state(ctx, encoder_context, &virtual_addr_params);

    memset(&ind_obj_base_addr_params, 0, sizeof(ind_obj_base_addr_params));
    ind_obj_base_addr_params.huc_indirect_stream_in_object_res = &vdenc_context->huc_dummy_res;
    ind_obj_base_addr_params.huc_indirect_stream_out_object_res = NULL;
    gen9_vdenc_huc_ind_obj_base_addr_state(ctx, encoder_context, &ind_obj_base_addr_params);

    memset(&stream_object_params, 0, sizeof(stream_object_params));
    stream_object_params.indirect_stream_in_data_length = 1;
    stream_object_params.indirect_stream_in_start_address = 0;
    gen9_vdenc_huc_stream_object(ctx, encoder_context, &stream_object_params);

    gen9_vdenc_huc_store_huc_status2(ctx, encoder_context);

    memset(&start_params, 0, sizeof(start_params));
    start_params.last_stream_object = 1;
    gen9_vdenc_huc_start(ctx, encoder_context, &start_params);

    memset(&pipeline_flush_params, 0, sizeof(pipeline_flush_params));
    pipeline_flush_params.hevc_pipeline_done = 1;
    pipeline_flush_params.hevc_pipeline_command_flush = 1;
    gen9_vdenc_vd_pipeline_flush(ctx, encoder_context, &pipeline_flush_params);

    memset(&mi_flush_dw_params, 0, sizeof(mi_flush_dw_params));
    mi_flush_dw_params.video_pipeline_cache_invalidate = 1;
    gen8_gpe_mi_flush_dw(ctx, batch, &mi_flush_dw_params);
}

static void
gen9_vdenc_update_huc_update_dmem(VADriverContextP ctx, struct intel_encoder_context *encoder_context)
{
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    struct huc_brc_update_dmem *dmem;
    int i, num_p_in_gop = 0;

    dmem = (struct huc_brc_update_dmem *)i965_map_gpe_resource(&vdenc_context->brc_update_dmem_res[vdenc_context->current_pass]);

    if (!dmem)
        return;

    dmem->brc_func = 1;

    if (vdenc_context->brc_initted && (vdenc_context->current_pass == 0)) {
        vdenc_context->brc_init_previous_target_buf_full_in_bits =
            (uint32_t)(vdenc_context->brc_init_current_target_buf_full_in_bits);
        vdenc_context->brc_init_current_target_buf_full_in_bits += vdenc_context->brc_init_reset_input_bits_per_frame;
        vdenc_context->brc_target_size += vdenc_context->brc_init_reset_input_bits_per_frame;
    }

    if (vdenc_context->brc_target_size > vdenc_context->vbv_buffer_size_in_bit)
        vdenc_context->brc_target_size -= vdenc_context->vbv_buffer_size_in_bit;

    dmem->target_size = vdenc_context->brc_target_size;

    dmem->peak_tx_bits_per_frame = (uint32_t)(vdenc_context->brc_init_current_target_buf_full_in_bits - vdenc_context->brc_init_previous_target_buf_full_in_bits);

    dmem->target_slice_size = 0;        // TODO: add support for slice size control

    memcpy(dmem->start_global_adjust_frame, vdenc_brc_start_global_adjust_frame, sizeof(dmem->start_global_adjust_frame));
    memcpy(dmem->global_rate_ratio_threshold, vdenc_brc_global_rate_ratio_threshold, sizeof(dmem->global_rate_ratio_threshold));

    dmem->current_frame_type = (vdenc_context->frame_type + 2) % 3;      // I frame:2, P frame:0, B frame:1

    memcpy(dmem->start_global_adjust_mult, vdenc_brc_start_global_adjust_mult, sizeof(dmem->start_global_adjust_mult));
    memcpy(dmem->start_global_adjust_div, vdenc_brc_start_global_adjust_div, sizeof(dmem->start_global_adjust_div));
    memcpy(dmem->global_rate_ratio_threshold_qp, vdenc_brc_global_rate_ratio_threshold_qp, sizeof(dmem->global_rate_ratio_threshold_qp));

    dmem->current_pak_pass = vdenc_context->current_pass;
    dmem->max_num_passes = 2;

    dmem->scene_change_detect_enable = 1;
    dmem->scene_change_prev_intra_percent_threshold = 96;
    dmem->scene_change_cur_intra_perent_threshold = 192;

    if (vdenc_context->ref_dist && vdenc_context->gop_size > 0)
        num_p_in_gop = (vdenc_context->gop_size - 1) / vdenc_context->ref_dist;

    for (i = 0; i < 2; i++)
        dmem->scene_change_width[i] = MIN((num_p_in_gop + 1) / 5, 6);

    if (vdenc_context->is_low_delay)
        dmem->ip_average_coeff = 0;
    else
        dmem->ip_average_coeff = 128;

    dmem->skip_frame_size = 0;
    dmem->num_of_frames_skipped = 0;

    dmem->roi_source = 0;               // TODO: add support for dirty ROI
    dmem->hme_detection_enable = 0;     // TODO: support HME kernel
    dmem->hme_cost_enable = 1;

    dmem->second_level_batchbuffer_size = 228;

    i965_unmap_gpe_resource(&vdenc_context->brc_update_dmem_res[vdenc_context->current_pass]);
}

static void
gen9_vdenc_init_mfx_avc_img_state(VADriverContextP ctx,
                                  struct encode_state *encode_state,
                                  struct intel_encoder_context *encoder_context,
                                  struct gen9_mfx_avc_img_state *pstate,
                                  int use_huc)
{
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    VAEncSequenceParameterBufferH264 *seq_param = (VAEncSequenceParameterBufferH264 *)encode_state->seq_param_ext->buffer;
    VAEncPictureParameterBufferH264 *pic_param = (VAEncPictureParameterBufferH264 *)encode_state->pic_param_ext->buffer;

    memset(pstate, 0, sizeof(*pstate));

    pstate->dw0.value = (MFX_AVC_IMG_STATE | (sizeof(*pstate) / 4 - 2));

    pstate->dw1.frame_size_in_mbs_minus1 = vdenc_context->frame_width_in_mbs * vdenc_context->frame_height_in_mbs - 1;

    pstate->dw2.frame_width_in_mbs_minus1 = vdenc_context->frame_width_in_mbs - 1;
    pstate->dw2.frame_height_in_mbs_minus1 = vdenc_context->frame_height_in_mbs - 1;

    pstate->dw3.image_structure = 0;
    pstate->dw3.weighted_bipred_idc = pic_param->pic_fields.bits.weighted_bipred_idc;
    pstate->dw3.weighted_pred_flag = pic_param->pic_fields.bits.weighted_pred_flag;
    pstate->dw3.brc_domain_rate_control_enable = !!use_huc;
    pstate->dw3.chroma_qp_offset = pic_param->chroma_qp_index_offset;
    pstate->dw3.second_chroma_qp_offset = pic_param->second_chroma_qp_index_offset;

    pstate->dw4.field_picture_flag = 0;
    pstate->dw4.mbaff_mode_active = seq_param->seq_fields.bits.mb_adaptive_frame_field_flag;
    pstate->dw4.frame_mb_only_flag = seq_param->seq_fields.bits.frame_mbs_only_flag;
    pstate->dw4.transform_8x8_idct_mode_flag = vdenc_context->transform_8x8_mode_enable;
    pstate->dw4.direct_8x8_interface_flag = seq_param->seq_fields.bits.direct_8x8_inference_flag;
    pstate->dw4.constrained_intra_prediction_flag = pic_param->pic_fields.bits.constrained_intra_pred_flag;
    pstate->dw4.entropy_coding_flag = pic_param->pic_fields.bits.entropy_coding_mode_flag;
    pstate->dw4.mb_mv_format_flag = 1;
    pstate->dw4.chroma_format_idc = seq_param->seq_fields.bits.chroma_format_idc;
    pstate->dw4.mv_unpacked_flag = 1;
    pstate->dw4.insert_test_flag = 0;
    pstate->dw4.load_slice_pointer_flag = 0;
    pstate->dw4.macroblock_stat_enable = 0;        /* Always 0 in VDEnc mode */
    pstate->dw4.minimum_frame_size = 0;

    pstate->dw5.intra_mb_max_bit_flag = 1;
    pstate->dw5.inter_mb_max_bit_flag = 1;
    pstate->dw5.frame_size_over_flag = 1;
    pstate->dw5.frame_size_under_flag = 1;
    pstate->dw5.intra_mb_ipcm_flag = 1;
    pstate->dw5.mb_rate_ctrl_flag = 0;             /* Always 0 in VDEnc mode */
    pstate->dw5.non_first_pass_flag = 0;
    pstate->dw5.aq_enable = pstate->dw5.aq_rounding = 0;
    pstate->dw5.aq_chroma_disable = 1;

    pstate->dw6.intra_mb_max_size = 2700;
    pstate->dw6.inter_mb_max_size = 4095;

    pstate->dw8.slice_delta_qp_max0 = 0;
    pstate->dw8.slice_delta_qp_max1 = 0;
    pstate->dw8.slice_delta_qp_max2 = 0;
    pstate->dw8.slice_delta_qp_max3 = 0;

    pstate->dw9.slice_delta_qp_min0 = 0;
    pstate->dw9.slice_delta_qp_min1 = 0;
    pstate->dw9.slice_delta_qp_min2 = 0;
    pstate->dw9.slice_delta_qp_min3 = 0;

    pstate->dw10.frame_bitrate_min = 0;
    pstate->dw10.frame_bitrate_min_unit = 1;
    pstate->dw10.frame_bitrate_min_unit_mode = 1;
    pstate->dw10.frame_bitrate_max = (1 << 14) - 1;
    pstate->dw10.frame_bitrate_max_unit = 1;
    pstate->dw10.frame_bitrate_max_unit_mode = 1;

    pstate->dw11.frame_bitrate_min_delta = 0;
    pstate->dw11.frame_bitrate_max_delta = 0;

    pstate->dw12.vad_error_logic = 1;
    /* TODO: set paramters DW19/DW20 for slices */
}

static void
gen9_vdenc_init_vdenc_img_state(VADriverContextP ctx,
                                struct encode_state *encode_state,
                                struct intel_encoder_context *encoder_context,
                                struct gen9_vdenc_img_state *pstate,
                                int update_cost)
{
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    VAEncSequenceParameterBufferH264 *seq_param = (VAEncSequenceParameterBufferH264 *)encode_state->seq_param_ext->buffer;
    VAEncPictureParameterBufferH264 *pic_param = (VAEncPictureParameterBufferH264 *)encode_state->pic_param_ext->buffer;
    VAEncSliceParameterBufferH264 *slice_param = (VAEncSliceParameterBufferH264 *)encode_state->slice_params_ext[0]->buffer;

    memset(pstate, 0, sizeof(*pstate));

    pstate->dw0.value = (VDENC_IMG_STATE | (sizeof(*pstate) / 4 - 2));

    if (vdenc_context->frame_type == VDENC_FRAME_I) {
        pstate->dw4.intra_sad_measure_adjustment = 2;
        pstate->dw4.sub_macroblock_sub_partition_mask = 0x70;

        pstate->dw5.cre_prefetch_enable = 1;

        pstate->dw9.mode0_cost = 10;
        pstate->dw9.mode1_cost = 0;
        pstate->dw9.mode2_cost = 3;
        pstate->dw9.mode3_cost = 30;

        pstate->dw20.penalty_for_intra_16x16_non_dc_prediction = 36;
        pstate->dw20.penalty_for_intra_8x8_non_dc_prediction = 12;
        pstate->dw20.penalty_for_intra_4x4_non_dc_prediction = 4;

        pstate->dw22.small_mb_size_in_word = 0xff;
        pstate->dw22.large_mb_size_in_word = 0xff;

        pstate->dw27.max_hmv_r = 0x2000;
        pstate->dw27.max_vmv_r = 0x200;

        pstate->dw33.qp_range_check_upper_bound = 0x33;
        pstate->dw33.qp_range_check_lower_bound = 0x0a;
        pstate->dw33.qp_range_check_value = 0x0f;
    } else {
        pstate->dw2.bidirectional_weight = 0x20;

        pstate->dw4.subpel_mode = 3;
        pstate->dw4.bme_disable_for_fbr_message = 1;
        pstate->dw4.inter_sad_measure_adjustment = 2;
        pstate->dw4.intra_sad_measure_adjustment = 2;
        pstate->dw4.sub_macroblock_sub_partition_mask = 0x70;

        pstate->dw5.cre_prefetch_enable = 1;

        pstate->dw8.non_skip_zero_mv_const_added = 1;
        pstate->dw8.non_skip_mb_mode_const_added = 1;
        pstate->dw8.ref_id_cost_mode_select = 1;

        pstate->dw9.mode0_cost = 7;
        pstate->dw9.mode1_cost = 26;
        pstate->dw9.mode2_cost = 30;
        pstate->dw9.mode3_cost = 57;

        pstate->dw10.mode4_cost = 8;
        pstate->dw10.mode5_cost = 2;
        pstate->dw10.mode6_cost = 4;
        pstate->dw10.mode7_cost = 6;

        pstate->dw11.mode8_cost = 5;
        pstate->dw11.mode9_cost = 0;
        pstate->dw11.ref_id_cost = 4;
        pstate->dw11.chroma_intra_mode_cost = 0;

        pstate->dw12_13.mv_cost.dw0.mv0_cost = 0;
        pstate->dw12_13.mv_cost.dw0.mv1_cost = 6;
        pstate->dw12_13.mv_cost.dw0.mv2_cost = 6;
        pstate->dw12_13.mv_cost.dw0.mv3_cost = 9;
        pstate->dw12_13.mv_cost.dw1.mv4_cost = 10;
        pstate->dw12_13.mv_cost.dw1.mv5_cost = 13;
        pstate->dw12_13.mv_cost.dw1.mv6_cost = 14;
        pstate->dw12_13.mv_cost.dw1.mv7_cost = 24;

        pstate->dw20.penalty_for_intra_16x16_non_dc_prediction = 36;
        pstate->dw20.penalty_for_intra_8x8_non_dc_prediction = 12;
        pstate->dw20.penalty_for_intra_4x4_non_dc_prediction = 4;

        pstate->dw22.small_mb_size_in_word = 0xff;
        pstate->dw22.large_mb_size_in_word = 0xff;

        pstate->dw27.max_hmv_r = 0x2000;
        pstate->dw27.max_vmv_r = 0x200;

        pstate->dw31.offset0_for_zone0_neg_zone1_boundary = 800;

        pstate->dw32.offset1_for_zone1_neg_zone2_boundary = 1600;
        pstate->dw32.offset2_for_zone2_neg_zone3_boundary = 2400;

        pstate->dw33.qp_range_check_upper_bound = 0x33;
        pstate->dw33.qp_range_check_lower_bound = 0x0a;
        pstate->dw33.qp_range_check_value = 0x0f;

        pstate->dw34.midpoint_distortion = 0x640;
    }

    /* ROI will be updated in HuC kernel for CBR/VBR */
    if (!vdenc_context->brc_enabled && vdenc_context->num_roi) {
        pstate->dw34.roi_enable = 1;

        pstate->dw30.roi_qp_adjustment_for_zone1 = CLAMP(-8, 7, vdenc_context->roi[0].value);

        if (vdenc_context->num_roi > 1)
            pstate->dw30.roi_qp_adjustment_for_zone2 = CLAMP(-8, 7, vdenc_context->roi[1].value);

        if (vdenc_context->num_roi > 2)
            pstate->dw30.roi_qp_adjustment_for_zone3 = CLAMP(-8, 7, vdenc_context->roi[2].value);
    }

    pstate->dw1.transform_8x8_flag = vdenc_context->transform_8x8_mode_enable;
    pstate->dw1.extended_pak_obj_cmd_enable = !!vdenc_context->use_extended_pak_obj_cmd;

    pstate->dw3.picture_width = vdenc_context->frame_width_in_mbs;

    pstate->dw4.forward_transform_skip_check_enable = 1; /* TODO: double-check it */

    pstate->dw5.picture_height_minus1 = vdenc_context->frame_height_in_mbs - 1;
    pstate->dw5.picture_type = vdenc_context->frame_type;
    pstate->dw5.constrained_intra_prediction_flag  = pic_param->pic_fields.bits.constrained_intra_pred_flag;

    if (vdenc_context->frame_type == VDENC_FRAME_P) {
        pstate->dw5.hme_ref1_disable = vdenc_context->num_refs[0] == 1 ? 1 : 0;
    }

    pstate->dw5.mb_slice_threshold_value = 0;

    pstate->dw6.slice_macroblock_height_minus1 = vdenc_context->frame_height_in_mbs - 1; /* single slice onlye */

    if (pstate->dw1.transform_8x8_flag)
        pstate->dw8.luma_intra_partition_mask = 0;
    else
        pstate->dw8.luma_intra_partition_mask = (1 << 1); /* disable transform_8x8 */

    pstate->dw14.qp_prime_y = pic_param->pic_init_qp + slice_param->slice_qp_delta;      /* TODO: check whether it is OK to use the first slice only */

    if (update_cost) {
        pstate->dw9.mode0_cost = vdenc_context->mode_cost[0];
        pstate->dw9.mode1_cost = vdenc_context->mode_cost[1];
        pstate->dw9.mode2_cost = vdenc_context->mode_cost[2];
        pstate->dw9.mode3_cost = vdenc_context->mode_cost[3];

        pstate->dw10.mode4_cost = vdenc_context->mode_cost[4];
        pstate->dw10.mode5_cost = vdenc_context->mode_cost[5];
        pstate->dw10.mode6_cost = vdenc_context->mode_cost[6];
        pstate->dw10.mode7_cost = vdenc_context->mode_cost[7];

        pstate->dw11.mode8_cost = vdenc_context->mode_cost[8];
        pstate->dw11.mode9_cost = vdenc_context->mode_cost[9];
        pstate->dw11.ref_id_cost = vdenc_context->mode_cost[10];
        pstate->dw11.chroma_intra_mode_cost = vdenc_context->mode_cost[11];

        pstate->dw12_13.mv_cost.dw0.mv0_cost = vdenc_context->mv_cost[0];
        pstate->dw12_13.mv_cost.dw0.mv1_cost = vdenc_context->mv_cost[1];
        pstate->dw12_13.mv_cost.dw0.mv2_cost = vdenc_context->mv_cost[2];
        pstate->dw12_13.mv_cost.dw0.mv3_cost = vdenc_context->mv_cost[3];
        pstate->dw12_13.mv_cost.dw1.mv4_cost = vdenc_context->mv_cost[4];
        pstate->dw12_13.mv_cost.dw1.mv5_cost = vdenc_context->mv_cost[5];
        pstate->dw12_13.mv_cost.dw1.mv6_cost = vdenc_context->mv_cost[6];
        pstate->dw12_13.mv_cost.dw1.mv7_cost = vdenc_context->mv_cost[7];

        pstate->dw28_29.hme_mv_cost.dw0.mv0_cost = vdenc_context->hme_mv_cost[0];
        pstate->dw28_29.hme_mv_cost.dw0.mv1_cost = vdenc_context->hme_mv_cost[1];
        pstate->dw28_29.hme_mv_cost.dw0.mv2_cost = vdenc_context->hme_mv_cost[2];
        pstate->dw28_29.hme_mv_cost.dw0.mv3_cost = vdenc_context->hme_mv_cost[3];
        pstate->dw28_29.hme_mv_cost.dw1.mv4_cost = vdenc_context->hme_mv_cost[4];
        pstate->dw28_29.hme_mv_cost.dw1.mv5_cost = vdenc_context->hme_mv_cost[5];
        pstate->dw28_29.hme_mv_cost.dw1.mv6_cost = vdenc_context->hme_mv_cost[6];
        pstate->dw28_29.hme_mv_cost.dw1.mv7_cost = vdenc_context->hme_mv_cost[7];
    }

    pstate->dw27.max_vmv_r = gen9_vdenc_get_max_vmv_range(seq_param->level_idc);

    pstate->dw34.image_state_qp_override = (vdenc_context->internal_rate_mode == I965_BRC_CQP) ? 1 : 0;

    /* TODO: check rolling I */

    /* TODO: handle ROI */

    /* TODO: check stream in support */
}

static void
gen9_vdenc_init_img_states(VADriverContextP ctx,
                           struct encode_state *encode_state,
                           struct intel_encoder_context *encoder_context)
{
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    struct gen9_mfx_avc_img_state *mfx_img_cmd;
    struct gen9_vdenc_img_state *vdenc_img_cmd;
    char *pbuffer;

    pbuffer = i965_map_gpe_resource(&vdenc_context->vdenc_avc_image_state_res);

    if (!pbuffer)
        return;

    mfx_img_cmd = (struct gen9_mfx_avc_img_state *)pbuffer;
    gen9_vdenc_init_mfx_avc_img_state(ctx, encode_state, encoder_context, mfx_img_cmd, 1);
    pbuffer += sizeof(*mfx_img_cmd);

    vdenc_img_cmd = (struct gen9_vdenc_img_state *)pbuffer;
    gen9_vdenc_init_vdenc_img_state(ctx, encode_state, encoder_context, vdenc_img_cmd, 0);
    pbuffer += sizeof(*vdenc_img_cmd);

    /* Add batch buffer end command */
    *((unsigned int *)pbuffer) = MI_BATCH_BUFFER_END;

    i965_unmap_gpe_resource(&vdenc_context->vdenc_avc_image_state_res);
}

static void
gen9_vdenc_huc_brc_update_constant_data(VADriverContextP ctx,
                                        struct encode_state *encode_state,
                                        struct intel_encoder_context *encoder_context)
{
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    struct huc_brc_update_constant_data *brc_buffer;
    int i, j;

    brc_buffer = (struct huc_brc_update_constant_data *)
                 i965_map_gpe_resource(&vdenc_context->brc_constant_data_res);

    if (!brc_buffer)
        return;

    memcpy(brc_buffer, &gen9_brc_update_constant_data, sizeof(gen9_brc_update_constant_data));

    for (i = 0; i < 8; i++) {
        for (j = 0; j < 42; j++) {
            brc_buffer->hme_mv_cost[i][j] = map_44_lut_value((vdenc_hme_cost[i][j + 10]), 0x6f);
        }
    }

    if (vdenc_context->internal_rate_mode == I965_BRC_VBR) {
        memcpy(brc_buffer->dist_qp_adj_tab_i, dist_qp_adj_tab_i_vbr, sizeof(dist_qp_adj_tab_i_vbr));
        memcpy(brc_buffer->dist_qp_adj_tab_p, dist_qp_adj_tab_p_vbr, sizeof(dist_qp_adj_tab_p_vbr));
        memcpy(brc_buffer->dist_qp_adj_tab_b, dist_qp_adj_tab_b_vbr, sizeof(dist_qp_adj_tab_b_vbr));
        memcpy(brc_buffer->buf_rate_adj_tab_i, buf_rate_adj_tab_i_vbr, sizeof(buf_rate_adj_tab_i_vbr));
        memcpy(brc_buffer->buf_rate_adj_tab_p, buf_rate_adj_tab_p_vbr, sizeof(buf_rate_adj_tab_p_vbr));
        memcpy(brc_buffer->buf_rate_adj_tab_b, buf_rate_adj_tab_b_vbr, sizeof(buf_rate_adj_tab_b_vbr));
    }


    i965_unmap_gpe_resource(&vdenc_context->brc_constant_data_res);
}

static void
gen9_vdenc_huc_brc_update(VADriverContextP ctx,
                          struct encode_state *encode_state,
                          struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    struct huc_pipe_mode_select_parameter pipe_mode_select_params;
    struct huc_imem_state_parameter imem_state_params;
    struct huc_dmem_state_parameter dmem_state_params;
    struct huc_virtual_addr_parameter virtual_addr_params;
    struct huc_ind_obj_base_addr_parameter ind_obj_base_addr_params;
    struct huc_stream_object_parameter stream_object_params;
    struct huc_start_parameter start_params;
    struct vd_pipeline_flush_parameter pipeline_flush_params;
    struct gpe_mi_store_register_mem_parameter mi_store_register_mem_params;
    struct gpe_mi_store_data_imm_parameter mi_store_data_imm_params;
    struct gpe_mi_flush_dw_parameter mi_flush_dw_params;

    memset(&mi_flush_dw_params, 0, sizeof(mi_flush_dw_params));
    mi_flush_dw_params.video_pipeline_cache_invalidate = 1;
    gen8_gpe_mi_flush_dw(ctx, batch, &mi_flush_dw_params);

    if (!vdenc_context->brc_initted || vdenc_context->brc_need_reset) {
        struct gpe_mi_conditional_batch_buffer_end_parameter mi_conditional_batch_buffer_end_params;

        memset(&mi_conditional_batch_buffer_end_params, 0, sizeof(mi_conditional_batch_buffer_end_params));
        mi_conditional_batch_buffer_end_params.bo = vdenc_context->huc_status2_res.bo;
        gen9_gpe_mi_conditional_batch_buffer_end(ctx, batch, &mi_conditional_batch_buffer_end_params);
    }

    gen9_vdenc_init_img_states(ctx, encode_state, encoder_context);

    memset(&imem_state_params, 0, sizeof(imem_state_params));
    imem_state_params.huc_firmware_descriptor = HUC_BRC_UPDATE;
    gen9_vdenc_huc_imem_state(ctx, encoder_context, &imem_state_params);

    memset(&pipe_mode_select_params, 0, sizeof(pipe_mode_select_params));
    gen9_vdenc_huc_pipe_mode_select(ctx, encoder_context, &pipe_mode_select_params);

    gen9_vdenc_update_huc_update_dmem(ctx, encoder_context);
    memset(&dmem_state_params, 0, sizeof(dmem_state_params));
    dmem_state_params.huc_data_source_res = &vdenc_context->brc_update_dmem_res[vdenc_context->current_pass];
    dmem_state_params.huc_data_destination_base_address = HUC_DMEM_DATA_OFFSET;
    dmem_state_params.huc_data_length = ALIGN(sizeof(struct huc_brc_update_dmem), 64);
    gen9_vdenc_huc_dmem_state(ctx, encoder_context, &dmem_state_params);

    gen9_vdenc_huc_brc_update_constant_data(ctx, encode_state, encoder_context);
    memset(&virtual_addr_params, 0, sizeof(virtual_addr_params));
    virtual_addr_params.regions[0].huc_surface_res = &vdenc_context->brc_history_buffer_res;
    virtual_addr_params.regions[0].is_target = 1;
    virtual_addr_params.regions[1].huc_surface_res = &vdenc_context->vdenc_statistics_res;
    virtual_addr_params.regions[2].huc_surface_res = &vdenc_context->pak_statistics_res;
    virtual_addr_params.regions[3].huc_surface_res = &vdenc_context->vdenc_avc_image_state_res;
    virtual_addr_params.regions[4].huc_surface_res = &vdenc_context->hme_detection_summary_buffer_res;
    virtual_addr_params.regions[4].is_target = 1;
    virtual_addr_params.regions[5].huc_surface_res = &vdenc_context->brc_constant_data_res;
    virtual_addr_params.regions[6].huc_surface_res = &vdenc_context->second_level_batch_res;
    virtual_addr_params.regions[6].is_target = 1;
    gen9_vdenc_huc_virtual_addr_state(ctx, encoder_context, &virtual_addr_params);

    memset(&ind_obj_base_addr_params, 0, sizeof(ind_obj_base_addr_params));
    ind_obj_base_addr_params.huc_indirect_stream_in_object_res = &vdenc_context->huc_dummy_res;
    ind_obj_base_addr_params.huc_indirect_stream_out_object_res = NULL;
    gen9_vdenc_huc_ind_obj_base_addr_state(ctx, encoder_context, &ind_obj_base_addr_params);

    memset(&stream_object_params, 0, sizeof(stream_object_params));
    stream_object_params.indirect_stream_in_data_length = 1;
    stream_object_params.indirect_stream_in_start_address = 0;
    gen9_vdenc_huc_stream_object(ctx, encoder_context, &stream_object_params);

    gen9_vdenc_huc_store_huc_status2(ctx, encoder_context);

    memset(&start_params, 0, sizeof(start_params));
    start_params.last_stream_object = 1;
    gen9_vdenc_huc_start(ctx, encoder_context, &start_params);

    memset(&pipeline_flush_params, 0, sizeof(pipeline_flush_params));
    pipeline_flush_params.hevc_pipeline_done = 1;
    pipeline_flush_params.hevc_pipeline_command_flush = 1;
    gen9_vdenc_vd_pipeline_flush(ctx, encoder_context, &pipeline_flush_params);

    memset(&mi_flush_dw_params, 0, sizeof(mi_flush_dw_params));
    mi_flush_dw_params.video_pipeline_cache_invalidate = 1;
    gen8_gpe_mi_flush_dw(ctx, batch, &mi_flush_dw_params);

    /* Store HUC_STATUS */
    memset(&mi_store_register_mem_params, 0, sizeof(mi_store_register_mem_params));
    mi_store_register_mem_params.mmio_offset = VCS0_HUC_STATUS;
    mi_store_register_mem_params.bo = vdenc_context->huc_status_res.bo;
    gen8_gpe_mi_store_register_mem(ctx, batch, &mi_store_register_mem_params);

    /* Write HUC_STATUS mask (1 << 31) */
    memset(&mi_store_data_imm_params, 0, sizeof(mi_store_data_imm_params));
    mi_store_data_imm_params.bo = vdenc_context->huc_status_res.bo;
    mi_store_data_imm_params.offset = 4;
    mi_store_data_imm_params.dw0 = (1 << 31);
    gen8_gpe_mi_store_data_imm(ctx, batch, &mi_store_data_imm_params);
}

static void
gen9_vdenc_mfx_pipe_mode_select(VADriverContextP ctx,
                                struct encode_state *encode_state,
                                struct intel_encoder_context *encoder_context)
{
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    struct intel_batchbuffer *batch = encoder_context->base.batch;

    BEGIN_BCS_BATCH(batch, 5);

    OUT_BCS_BATCH(batch, MFX_PIPE_MODE_SELECT | (5 - 2));
    OUT_BCS_BATCH(batch,
                  (1 << 29) |
                  (MFX_LONG_MODE << 17) |       /* Must be long format for encoder */
                  (MFD_MODE_VLD << 15) |
                  (1 << 13) |                   /* VDEnc mode */
                  ((!!vdenc_context->post_deblocking_output_res.bo) << 9)  |    /* Post Deblocking Output */
                  ((!!vdenc_context->pre_deblocking_output_res.bo) << 8)  |     /* Pre Deblocking Output */
                  (1 << 7)  |                   /* Scaled surface enable */
                  (1 << 6)  |                   /* Frame statistics stream out enable, always '1' in VDEnc mode */
                  (1 << 4)  |                   /* encoding mode */
                  (MFX_FORMAT_AVC << 0));
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_vdenc_mfx_surface_state(VADriverContextP ctx,
                             struct intel_encoder_context *encoder_context,
                             struct i965_gpe_resource *gpe_resource,
                             int id)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;

    BEGIN_BCS_BATCH(batch, 6);

    OUT_BCS_BATCH(batch, MFX_SURFACE_STATE | (6 - 2));
    OUT_BCS_BATCH(batch, id);
    OUT_BCS_BATCH(batch,
                  ((gpe_resource->height - 1) << 18) |
                  ((gpe_resource->width - 1) << 4));
    OUT_BCS_BATCH(batch,
                  (MFX_SURFACE_PLANAR_420_8 << 28) |    /* 420 planar YUV surface */
                  (1 << 27) |                           /* must be 1 for interleave U/V, hardware requirement */
                  ((gpe_resource->pitch - 1) << 3) |    /* pitch */
                  (0 << 2)  |                           /* must be 0 for interleave U/V */
                  (1 << 1)  |                           /* must be tiled */
                  (I965_TILEWALK_YMAJOR << 0));         /* tile walk, TILEWALK_YMAJOR */
    OUT_BCS_BATCH(batch,
                  (0 << 16) |                   /* must be 0 for interleave U/V */
                  (gpe_resource->y_cb_offset));         /* y offset for U(cb) */
    OUT_BCS_BATCH(batch,
                  (0 << 16) |                   /* must be 0 for interleave U/V */
                  (gpe_resource->y_cb_offset));         /* y offset for U(cb) */

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_vdenc_mfx_pipe_buf_addr_state(VADriverContextP ctx, struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    int i;

    BEGIN_BCS_BATCH(batch, 65);

    OUT_BCS_BATCH(batch, MFX_PIPE_BUF_ADDR_STATE | (65 - 2));

    /* the DW1-3 is for pre_deblocking */
    OUT_BUFFER_3DW(batch, vdenc_context->pre_deblocking_output_res.bo, 1, 0, 0);

    /* the DW4-6 is for the post_deblocking */
    OUT_BUFFER_3DW(batch, vdenc_context->post_deblocking_output_res.bo, 1, 0, 0);

    /* the DW7-9 is for the uncompressed_picture */
    OUT_BUFFER_3DW(batch, vdenc_context->uncompressed_input_surface_res.bo, 0, 0, 0);

    /* the DW10-12 is for PAK information (write) */
    OUT_BUFFER_3DW(batch, vdenc_context->pak_statistics_res.bo, 1, 0, 0);

    /* the DW13-15 is for the intra_row_store_scratch */
    OUT_BUFFER_3DW(batch, vdenc_context->mfx_intra_row_store_scratch_res.bo, 1, 0, 0);

    /* the DW16-18 is for the deblocking filter */
    OUT_BUFFER_3DW(batch, vdenc_context->mfx_deblocking_filter_row_store_scratch_res.bo, 1, 0, 0);

    /* the DW 19-50 is for Reference pictures*/
    for (i = 0; i < ARRAY_ELEMS(vdenc_context->list_reference_res); i++) {
        OUT_BUFFER_2DW(batch, vdenc_context->list_reference_res[i].bo, 0, 0);
    }

    /* DW 51, reference picture attributes */
    OUT_BCS_BATCH(batch, i965->intel.mocs_state);

    /* The DW 52-54 is for PAK information (read) */
    OUT_BUFFER_3DW(batch, vdenc_context->pak_statistics_res.bo, 0, 0, 0);

    /* the DW 55-57 is the ILDB buffer */
    OUT_BUFFER_3DW(batch, NULL, 0, 0, 0);

    /* the DW 58-60 is the second ILDB buffer */
    OUT_BUFFER_3DW(batch, NULL, 0, 0, 0);

    /* DW 61, memory compress enable & mode */
    OUT_BCS_BATCH(batch, 0);

    /* the DW 62-64 is the 4x Down Scaling surface */
    OUT_BUFFER_3DW(batch, vdenc_context->scaled_4x_recon_surface_res.bo, 1, 0, 0);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_vdenc_mfx_ind_obj_base_addr_state(VADriverContextP ctx, struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    struct intel_batchbuffer *batch = encoder_context->base.batch;

    BEGIN_BCS_BATCH(batch, 26);

    OUT_BCS_BATCH(batch, MFX_IND_OBJ_BASE_ADDR_STATE | (26 - 2));
    /* The DW1-5 is for the MFX indirect bistream offset, ignore for VDEnc mode */
    OUT_BUFFER_3DW(batch, NULL, 0, 0, 0);
    OUT_BUFFER_2DW(batch, NULL, 0, 0);

    /* the DW6-10 is for MFX Indirect MV Object Base Address, ignore for VDEnc mode */
    OUT_BUFFER_3DW(batch, NULL, 0, 0, 0);
    OUT_BUFFER_2DW(batch, NULL, 0, 0);

    /* The DW11-15 is for MFX IT-COFF. Not used on encoder */
    OUT_BUFFER_3DW(batch, NULL, 0, 0, 0);
    OUT_BUFFER_2DW(batch, NULL, 0, 0);

    /* The DW16-20 is for MFX indirect DBLK. Not used on encoder */
    OUT_BUFFER_3DW(batch, NULL, 0, 0, 0);
    OUT_BUFFER_2DW(batch, NULL, 0, 0);

    /* The DW21-25 is for MFC Indirect PAK-BSE Object Base Address for Encoder
     * Note: an offset is specified in MFX_AVC_SLICE_STATE
     */
    OUT_BUFFER_3DW(batch,
                   vdenc_context->compressed_bitstream.res.bo,
                   1,
                   0,
                   0);
    OUT_BUFFER_2DW(batch,
                   vdenc_context->compressed_bitstream.res.bo,
                   1,
                   vdenc_context->compressed_bitstream.end_offset);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_vdenc_mfx_bsp_buf_base_addr_state(VADriverContextP ctx, struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    struct intel_batchbuffer *batch = encoder_context->base.batch;

    BEGIN_BCS_BATCH(batch, 10);

    OUT_BCS_BATCH(batch, MFX_BSP_BUF_BASE_ADDR_STATE | (10 - 2));

    /* The DW1-3 is for bsd/mpc row store scratch buffer */
    OUT_BUFFER_3DW(batch, vdenc_context->mfx_bsd_mpc_row_store_scratch_res.bo, 1, 0, 0);

    /* The DW4-6 is for MPR Row Store Scratch Buffer Base Address, ignore for encoder */
    OUT_BUFFER_3DW(batch, NULL, 0, 0, 0);

    /* The DW7-9 is for Bitplane Read Buffer Base Address, ignore for encoder */
    OUT_BUFFER_3DW(batch, NULL, 0, 0, 0);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_vdenc_mfx_qm_state(VADriverContextP ctx,
                        int qm_type,
                        unsigned int *qm,
                        int qm_length,
                        struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    unsigned int qm_buffer[16];

    assert(qm_length <= 16);
    assert(sizeof(*qm) == 4);
    memcpy(qm_buffer, qm, qm_length * 4);

    BEGIN_BCS_BATCH(batch, 18);
    OUT_BCS_BATCH(batch, MFX_QM_STATE | (18 - 2));
    OUT_BCS_BATCH(batch, qm_type << 0);
    intel_batchbuffer_data(batch, qm_buffer, 16 * 4);
    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_vdenc_mfx_avc_qm_state(VADriverContextP ctx, struct intel_encoder_context *encoder_context)
{
    /* TODO: add support for non flat matrix */
    unsigned int qm[16] = {
        0x10101010, 0x10101010, 0x10101010, 0x10101010,
        0x10101010, 0x10101010, 0x10101010, 0x10101010,
        0x10101010, 0x10101010, 0x10101010, 0x10101010,
        0x10101010, 0x10101010, 0x10101010, 0x10101010
    };

    gen9_vdenc_mfx_qm_state(ctx, MFX_QM_AVC_4X4_INTRA_MATRIX, qm, 12, encoder_context);
    gen9_vdenc_mfx_qm_state(ctx, MFX_QM_AVC_4X4_INTER_MATRIX, qm, 12, encoder_context);
    gen9_vdenc_mfx_qm_state(ctx, MFX_QM_AVC_8x8_INTRA_MATRIX, qm, 16, encoder_context);
    gen9_vdenc_mfx_qm_state(ctx, MFX_QM_AVC_8x8_INTER_MATRIX, qm, 16, encoder_context);
}

static void
gen9_vdenc_mfx_fqm_state(VADriverContextP ctx,
                         int fqm_type,
                         unsigned int *fqm,
                         int fqm_length,
                         struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    unsigned int fqm_buffer[32];

    assert(fqm_length <= 32);
    assert(sizeof(*fqm) == 4);
    memcpy(fqm_buffer, fqm, fqm_length * 4);

    BEGIN_BCS_BATCH(batch, 34);
    OUT_BCS_BATCH(batch, MFX_FQM_STATE | (34 - 2));
    OUT_BCS_BATCH(batch, fqm_type << 0);
    intel_batchbuffer_data(batch, fqm_buffer, 32 * 4);
    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_vdenc_mfx_avc_fqm_state(VADriverContextP ctx, struct intel_encoder_context *encoder_context)
{
    /* TODO: add support for non flat matrix */
    unsigned int qm[32] = {
        0x10001000, 0x10001000, 0x10001000, 0x10001000,
        0x10001000, 0x10001000, 0x10001000, 0x10001000,
        0x10001000, 0x10001000, 0x10001000, 0x10001000,
        0x10001000, 0x10001000, 0x10001000, 0x10001000,
        0x10001000, 0x10001000, 0x10001000, 0x10001000,
        0x10001000, 0x10001000, 0x10001000, 0x10001000,
        0x10001000, 0x10001000, 0x10001000, 0x10001000,
        0x10001000, 0x10001000, 0x10001000, 0x10001000
    };

    gen9_vdenc_mfx_fqm_state(ctx, MFX_QM_AVC_4X4_INTRA_MATRIX, qm, 24, encoder_context);
    gen9_vdenc_mfx_fqm_state(ctx, MFX_QM_AVC_4X4_INTER_MATRIX, qm, 24, encoder_context);
    gen9_vdenc_mfx_fqm_state(ctx, MFX_QM_AVC_8x8_INTRA_MATRIX, qm, 32, encoder_context);
    gen9_vdenc_mfx_fqm_state(ctx, MFX_QM_AVC_8x8_INTER_MATRIX, qm, 32, encoder_context);
}

static void
gen9_vdenc_mfx_avc_img_state(VADriverContextP ctx,
                             struct encode_state *encode_state,
                             struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct gen9_mfx_avc_img_state mfx_img_cmd;

    gen9_vdenc_init_mfx_avc_img_state(ctx, encode_state, encoder_context, &mfx_img_cmd, 0);

    BEGIN_BCS_BATCH(batch, (sizeof(mfx_img_cmd) >> 2));
    intel_batchbuffer_data(batch, &mfx_img_cmd, sizeof(mfx_img_cmd));
    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_vdenc_vdenc_pipe_mode_select(VADriverContextP ctx,
                                  struct encode_state *encode_state,
                                  struct intel_encoder_context *encoder_context)
{
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    struct intel_batchbuffer *batch = encoder_context->base.batch;

    BEGIN_BCS_BATCH(batch, 2);

    OUT_BCS_BATCH(batch, VDENC_PIPE_MODE_SELECT | (2 - 2));
    OUT_BCS_BATCH(batch,
                  (vdenc_context->vdenc_streamin_enable << 9) |
                  (vdenc_context->vdenc_pak_threshold_check_enable << 8) |
                  (1 << 7)  |                   /* Tlb prefetch enable */
                  (1 << 5)  |                   /* Frame Statistics Stream-Out Enable */
                  (VDENC_CODEC_AVC << 0));

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_vdenc_vdenc_surface_state(VADriverContextP ctx,
                               struct intel_encoder_context *encoder_context,
                               struct i965_gpe_resource *gpe_resource,
                               int vdenc_surface_cmd)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;

    BEGIN_BCS_BATCH(batch, 6);

    OUT_BCS_BATCH(batch, vdenc_surface_cmd | (6 - 2));
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch,
                  ((gpe_resource->height - 1) << 18) |
                  ((gpe_resource->width - 1) << 4));
    OUT_BCS_BATCH(batch,
                  (VDENC_SURFACE_PLANAR_420_8 << 28) |  /* 420 planar YUV surface only on SKL */
                  (1 << 27) |                           /* must be 1 for interleave U/V, hardware requirement */
                  ((gpe_resource->pitch - 1) << 3) |    /* pitch */
                  (0 << 2)  |                           /* must be 0 for interleave U/V */
                  (1 << 1)  |                           /* must be tiled */
                  (I965_TILEWALK_YMAJOR << 0));         /* tile walk, TILEWALK_YMAJOR */
    OUT_BCS_BATCH(batch,
                  (0 << 16) |                   /* must be 0 for interleave U/V */
                  (gpe_resource->y_cb_offset));         /* y offset for U(cb) */
    OUT_BCS_BATCH(batch,
                  (0 << 16) |                   /* must be 0 for interleave U/V */
                  (gpe_resource->y_cb_offset));         /* y offset for v(cr) */

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_vdenc_vdenc_src_surface_state(VADriverContextP ctx,
                                   struct intel_encoder_context *encoder_context,
                                   struct i965_gpe_resource *gpe_resource)
{
    gen9_vdenc_vdenc_surface_state(ctx, encoder_context, gpe_resource, VDENC_SRC_SURFACE_STATE);
}

static void
gen9_vdenc_vdenc_ref_surface_state(VADriverContextP ctx,
                                   struct intel_encoder_context *encoder_context,
                                   struct i965_gpe_resource *gpe_resource)
{
    gen9_vdenc_vdenc_surface_state(ctx, encoder_context, gpe_resource, VDENC_REF_SURFACE_STATE);
}

static void
gen9_vdenc_vdenc_ds_ref_surface_state(VADriverContextP ctx,
                                      struct intel_encoder_context *encoder_context,
                                      struct i965_gpe_resource *gpe_resource)
{
    gen9_vdenc_vdenc_surface_state(ctx, encoder_context, gpe_resource, VDENC_DS_REF_SURFACE_STATE);
}

static void
gen9_vdenc_vdenc_pipe_buf_addr_state(VADriverContextP ctx,
                                     struct encode_state *encode_state,
                                     struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    struct intel_batchbuffer *batch = encoder_context->base.batch;

    BEGIN_BCS_BATCH(batch, 37);

    OUT_BCS_BATCH(batch, VDENC_PIPE_BUF_ADDR_STATE | (37 - 2));

    /* DW1-6 for DS FWD REF0/REF1 */

    if (vdenc_context->list_ref_idx[0][0] != 0xFF)
        OUT_BUFFER_3DW(batch, vdenc_context->list_scaled_4x_reference_res[vdenc_context->list_ref_idx[0][0]].bo, 0, 0, 0);
    else
        OUT_BUFFER_3DW(batch, NULL, 0, 0, 0);

    if (vdenc_context->list_ref_idx[0][1] != 0xFF)
        OUT_BUFFER_3DW(batch, vdenc_context->list_scaled_4x_reference_res[vdenc_context->list_ref_idx[0][1]].bo, 0, 0, 0);
    else
        OUT_BUFFER_3DW(batch, NULL, 0, 0, 0);

    /* DW7-9 for DS BWD REF0, ignored on SKL */
    OUT_BUFFER_3DW(batch, NULL, 0, 0, 0);

    /* DW10-12 for uncompressed input data */
    OUT_BUFFER_3DW(batch, vdenc_context->uncompressed_input_surface_res.bo, 0, 0, 0);

    /* DW13-DW15 for streamin data */
    if (vdenc_context->vdenc_streamin_enable)
        OUT_BUFFER_3DW(batch, vdenc_context->vdenc_streamin_res.bo, 0, 0, 0);
    else
        OUT_BUFFER_3DW(batch, NULL, 0, 0, 0);

    /* DW16-DW18 for row scratch buffer */
    OUT_BUFFER_3DW(batch, vdenc_context->vdenc_row_store_scratch_res.bo, 1, 0, 0);

    /* DW19-DW21, ignored on SKL */
    OUT_BUFFER_3DW(batch, NULL, 0, 0, 0);

    /* DW22-DW27 for FWD REF0/REF1 */

    if (vdenc_context->list_ref_idx[0][0] != 0xFF)
        OUT_BUFFER_3DW(batch, vdenc_context->list_reference_res[vdenc_context->list_ref_idx[0][0]].bo, 0, 0, 0);
    else
        OUT_BUFFER_3DW(batch, NULL, 0, 0, 0);

    if (vdenc_context->list_ref_idx[0][1] != 0xFF)
        OUT_BUFFER_3DW(batch, vdenc_context->list_reference_res[vdenc_context->list_ref_idx[0][1]].bo, 0, 0, 0);
    else
        OUT_BUFFER_3DW(batch, NULL, 0, 0, 0);

    /* DW28-DW30 for FWD REF2, ignored on SKL */
    OUT_BUFFER_3DW(batch, NULL, 0, 0, 0);

    /* DW31-DW33 for BDW REF0, ignored on SKL */
    OUT_BUFFER_3DW(batch, NULL, 0, 0, 0);

    /* DW34-DW36 for VDEnc statistics streamout */
    OUT_BUFFER_3DW(batch, vdenc_context->vdenc_statistics_res.bo, 1, 0, 0);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_vdenc_vdenc_const_qpt_state(VADriverContextP ctx,
                                 struct encode_state *encode_state,
                                 struct intel_encoder_context *encoder_context)
{
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    struct intel_batchbuffer *batch = encoder_context->base.batch;

    BEGIN_BCS_BATCH(batch, 61);

    OUT_BCS_BATCH(batch, VDENC_CONST_QPT_STATE | (61 - 2));

    if (vdenc_context->frame_type == VDENC_FRAME_I) {
        /* DW1-DW11 */
        intel_batchbuffer_data(batch, (void *)vdenc_const_qp_lambda, sizeof(vdenc_const_qp_lambda));

        /* DW12-DW25 */
        intel_batchbuffer_data(batch, (void *)vdenc_const_skip_threshold, sizeof(vdenc_const_skip_threshold));

        /* DW26-DW39 */
        intel_batchbuffer_data(batch, (void *)vdenc_const_sic_forward_transform_coeff_threshold_0, sizeof(vdenc_const_sic_forward_transform_coeff_threshold_0));

        /* DW40-DW46 */
        intel_batchbuffer_data(batch, (void *)vdenc_const_sic_forward_transform_coeff_threshold_1, sizeof(vdenc_const_sic_forward_transform_coeff_threshold_1));

        /* DW47-DW53 */
        intel_batchbuffer_data(batch, (void *)vdenc_const_sic_forward_transform_coeff_threshold_2, sizeof(vdenc_const_sic_forward_transform_coeff_threshold_2));

        /* DW54-DW60 */
        intel_batchbuffer_data(batch, (void *)vdenc_const_sic_forward_transform_coeff_threshold_3, sizeof(vdenc_const_sic_forward_transform_coeff_threshold_3));
    } else {
        int i;
        uint16_t tmp_vdenc_skip_threshold_p[28];

        memcpy(&tmp_vdenc_skip_threshold_p, vdenc_const_skip_threshold_p, sizeof(vdenc_const_skip_threshold_p));

        for (i = 0; i < 28; i++) {
            tmp_vdenc_skip_threshold_p[i] *= 3;
        }

        /* DW1-DW11 */
        intel_batchbuffer_data(batch, (void *)vdenc_const_qp_lambda_p, sizeof(vdenc_const_qp_lambda_p));

        /* DW12-DW25 */
        intel_batchbuffer_data(batch, (void *)tmp_vdenc_skip_threshold_p, sizeof(vdenc_const_skip_threshold_p));

        /* DW26-DW39 */
        intel_batchbuffer_data(batch, (void *)vdenc_const_sic_forward_transform_coeff_threshold_0_p, sizeof(vdenc_const_sic_forward_transform_coeff_threshold_0_p));

        /* DW40-DW46 */
        intel_batchbuffer_data(batch, (void *)vdenc_const_sic_forward_transform_coeff_threshold_1_p, sizeof(vdenc_const_sic_forward_transform_coeff_threshold_1_p));

        /* DW47-DW53 */
        intel_batchbuffer_data(batch, (void *)vdenc_const_sic_forward_transform_coeff_threshold_2_p, sizeof(vdenc_const_sic_forward_transform_coeff_threshold_2_p));

        /* DW54-DW60 */
        intel_batchbuffer_data(batch, (void *)vdenc_const_sic_forward_transform_coeff_threshold_3_p, sizeof(vdenc_const_sic_forward_transform_coeff_threshold_3_p));
    }

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_vdenc_vdenc_walker_state(VADriverContextP ctx,
                              struct encode_state *encode_state,
                              struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;

    BEGIN_BCS_BATCH(batch, 2);

    OUT_BCS_BATCH(batch, VDENC_WALKER_STATE | (2 - 2));
    OUT_BCS_BATCH(batch, 0); /* All fields are set to 0 */

    ADVANCE_BCS_BATCH(batch);
}

static void
gen95_vdenc_vdecn_weihgtsoffsets_state(VADriverContextP ctx,
                                       struct encode_state *encode_state,
                                       struct intel_encoder_context *encoder_context,
                                       VAEncSliceParameterBufferH264 *slice_param)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    VAEncPictureParameterBufferH264 *pic_param = (VAEncPictureParameterBufferH264 *)encode_state->pic_param_ext->buffer;

    BEGIN_BCS_BATCH(batch, 3);

    OUT_BCS_BATCH(batch, VDENC_WEIGHTSOFFSETS_STATE | (3 - 2));

    if (pic_param->pic_fields.bits.weighted_pred_flag == 1) {
        OUT_BCS_BATCH(batch, (slice_param->luma_offset_l0[1] << 24 |
                              slice_param->luma_weight_l0[1] << 16 |
                              slice_param->luma_offset_l0[0] << 8 |
                              slice_param->luma_weight_l0[0] << 0));
        OUT_BCS_BATCH(batch, (slice_param->luma_offset_l0[2] << 8 |
                              slice_param->luma_weight_l0[2] << 0));
    } else {
        OUT_BCS_BATCH(batch, (0 << 24 |
                              1 << 16 |
                              0 << 8 |
                              1 << 0));
        OUT_BCS_BATCH(batch, (0 << 8 |
                              1 << 0));
    }


    ADVANCE_BCS_BATCH(batch);
}

static void
gen95_vdenc_vdenc_walker_state(VADriverContextP ctx,
                               struct encode_state *encode_state,
                               struct intel_encoder_context *encoder_context,
                               VAEncSliceParameterBufferH264 *slice_param,
                               VAEncSliceParameterBufferH264 *next_slice_param)
{
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    VAEncPictureParameterBufferH264 *pic_param = (VAEncPictureParameterBufferH264 *)encode_state->pic_param_ext->buffer;
    int slice_type = intel_avc_enc_slice_type_fixup(slice_param->slice_type);
    int slice_hor_pos, slice_ver_pos, next_slice_hor_pos, next_slice_ver_pos;
    int luma_log2_weight_denom, weighted_pred_idc;

    slice_hor_pos = slice_param->macroblock_address % vdenc_context->frame_width_in_mbs;
    slice_ver_pos = slice_param->macroblock_address / vdenc_context->frame_width_in_mbs;

    if (next_slice_param) {
        next_slice_hor_pos = next_slice_param->macroblock_address % vdenc_context->frame_width_in_mbs;
        next_slice_ver_pos = next_slice_param->macroblock_address / vdenc_context->frame_width_in_mbs;
    } else {
        next_slice_hor_pos = 0;
        next_slice_ver_pos = vdenc_context->frame_height_in_mbs;
    }

    if (slice_type == SLICE_TYPE_P)
        weighted_pred_idc = pic_param->pic_fields.bits.weighted_pred_flag;
    else
        weighted_pred_idc = 0;

    if (weighted_pred_idc == 1)
        luma_log2_weight_denom = slice_param->luma_log2_weight_denom;
    else
        luma_log2_weight_denom = 0;

    BEGIN_BCS_BATCH(batch, 4);

    OUT_BCS_BATCH(batch, VDENC_WALKER_STATE | (4 - 2));
    OUT_BCS_BATCH(batch, (slice_hor_pos << 16 |
                          slice_ver_pos));
    OUT_BCS_BATCH(batch, (next_slice_hor_pos << 16 |
                          next_slice_ver_pos));
    OUT_BCS_BATCH(batch, luma_log2_weight_denom);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_vdenc_vdenc_img_state(VADriverContextP ctx,
                           struct encode_state *encode_state,
                           struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct gen9_vdenc_img_state vdenc_img_cmd;

    gen9_vdenc_init_vdenc_img_state(ctx, encode_state, encoder_context, &vdenc_img_cmd, 1);

    BEGIN_BCS_BATCH(batch, (sizeof(vdenc_img_cmd) >> 2));
    intel_batchbuffer_data(batch, &vdenc_img_cmd, sizeof(vdenc_img_cmd));
    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_vdenc_mfx_avc_insert_object(VADriverContextP ctx,
                                 struct intel_encoder_context *encoder_context,
                                 unsigned int *insert_data, int lenght_in_dws, int data_bits_in_last_dw,
                                 int skip_emul_byte_count, int is_last_header, int is_end_of_slice, int emulation_flag,
                                 int slice_header_indicator)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;

    if (data_bits_in_last_dw == 0)
        data_bits_in_last_dw = 32;

    BEGIN_BCS_BATCH(batch, lenght_in_dws + 2);

    OUT_BCS_BATCH(batch, MFX_INSERT_OBJECT | (lenght_in_dws));
    OUT_BCS_BATCH(batch,
                  (0 << 16) |   /* always start at offset 0 */
                  (slice_header_indicator << 14) |
                  (data_bits_in_last_dw << 8) |
                  (skip_emul_byte_count << 4) |
                  (!!emulation_flag << 3) |
                  ((!!is_last_header) << 2) |
                  ((!!is_end_of_slice) << 1) |
                  (0 << 0));    /* TODO: check this flag */
    intel_batchbuffer_data(batch, insert_data, lenght_in_dws * 4);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_vdenc_mfx_avc_insert_slice_packed_data(VADriverContextP ctx,
                                            struct encode_state *encode_state,
                                            struct intel_encoder_context *encoder_context,
                                            int slice_index)
{
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    VAEncPackedHeaderParameterBuffer *param = NULL;
    unsigned int length_in_bits;
    unsigned int *header_data = NULL;
    int count, i, start_index;
    int slice_header_index;
    unsigned int insert_one_zero_byte = 0;

    if (encode_state->slice_header_index[slice_index] == 0)
        slice_header_index = -1;
    else
        slice_header_index = (encode_state->slice_header_index[slice_index] & SLICE_PACKED_DATA_INDEX_MASK);

    count = encode_state->slice_rawdata_count[slice_index];
    start_index = (encode_state->slice_rawdata_index[slice_index] & SLICE_PACKED_DATA_INDEX_MASK);

    for (i = 0; i < count; i++) {
        unsigned int skip_emul_byte_cnt;

        header_data = (unsigned int *)encode_state->packed_header_data_ext[start_index + i]->buffer;

        param = (VAEncPackedHeaderParameterBuffer *)(encode_state->packed_header_params_ext[start_index + i]->buffer);

        /* skip the slice header packed data type as it is lastly inserted */
        if (param->type == VAEncPackedHeaderSlice)
            continue;

        length_in_bits = param->bit_length;

        skip_emul_byte_cnt = intel_avc_find_skipemulcnt((unsigned char *)header_data, length_in_bits);

        /* as the slice header is still required, the last header flag is set to
         * zero.
         */
        gen9_vdenc_mfx_avc_insert_object(ctx,
                                         encoder_context,
                                         header_data,
                                         ALIGN(length_in_bits, 32) >> 5,
                                         length_in_bits & 0x1f,
                                         skip_emul_byte_cnt,
                                         0,
                                         0,
                                         !param->has_emulation_bytes,
                                         0);

    }

    if (!vdenc_context->is_frame_level_vdenc) {
        insert_one_zero_byte = 1;
    }

    /* Insert one zero byte before the slice header if no any other NAL unit is inserted, required on KBL */
    if (insert_one_zero_byte) {
        unsigned int insert_data[] = { 0, };

        gen9_vdenc_mfx_avc_insert_object(ctx,
                                         encoder_context,
                                         insert_data,
                                         1,
                                         8,
                                         1,
                                         0, 0, 0, 0);
    }

    if (slice_header_index == -1) {
        VAEncSequenceParameterBufferH264 *seq_param = (VAEncSequenceParameterBufferH264 *)encode_state->seq_param_ext->buffer;
        VAEncPictureParameterBufferH264 *pic_param = (VAEncPictureParameterBufferH264 *)encode_state->pic_param_ext->buffer;
        VAEncSliceParameterBufferH264 *slice_params = (VAEncSliceParameterBufferH264 *)encode_state->slice_params_ext[slice_index]->buffer;
        unsigned char *slice_header = NULL, *slice_header1 = NULL;
        int slice_header_length_in_bits = 0;
        uint32_t saved_macroblock_address = 0;

        /* No slice header data is passed. And the driver needs to generate it */
        /* For the Normal H264 */

        if (slice_index &&
            (IS_KBL(i965->intel.device_info) ||
             IS_GLK(i965->intel.device_info))) {
            saved_macroblock_address = slice_params->macroblock_address;
            slice_params->macroblock_address = 0;
        }

        slice_header_length_in_bits = build_avc_slice_header(seq_param,
                                                             pic_param,
                                                             slice_params,
                                                             &slice_header);

        slice_header1 = slice_header;

        if (slice_index &&
            (IS_KBL(i965->intel.device_info) ||
             IS_GLK(i965->intel.device_info))) {
            slice_params->macroblock_address = saved_macroblock_address;
        }

        if (insert_one_zero_byte) {
            slice_header1 += 1;
            slice_header_length_in_bits -= 8;
        }

        gen9_vdenc_mfx_avc_insert_object(ctx,
                                         encoder_context,
                                         (unsigned int *)slice_header1,
                                         ALIGN(slice_header_length_in_bits, 32) >> 5,
                                         slice_header_length_in_bits & 0x1f,
                                         5,  /* first 5 bytes are start code + nal unit type */
                                         1, 0, 1,
                                         1);

        free(slice_header);
    } else {
        unsigned int skip_emul_byte_cnt;
        unsigned char *slice_header1 = NULL;

        if (slice_index &&
            (IS_KBL(i965->intel.device_info) ||
             IS_GLK(i965->intel.device_info))) {
            slice_header_index = (encode_state->slice_header_index[0] & SLICE_PACKED_DATA_INDEX_MASK);
        }

        header_data = (unsigned int *)encode_state->packed_header_data_ext[slice_header_index]->buffer;

        param = (VAEncPackedHeaderParameterBuffer *)(encode_state->packed_header_params_ext[slice_header_index]->buffer);
        length_in_bits = param->bit_length;

        slice_header1 = (unsigned char *)header_data;

        if (insert_one_zero_byte) {
            slice_header1 += 1;
            length_in_bits -= 8;
        }

        /* as the slice header is the last header data for one slice,
         * the last header flag is set to one.
         */
        skip_emul_byte_cnt = intel_avc_find_skipemulcnt((unsigned char *)header_data, length_in_bits);

        if (insert_one_zero_byte)
            skip_emul_byte_cnt -= 1;

        gen9_vdenc_mfx_avc_insert_object(ctx,
                                         encoder_context,
                                         (unsigned int *)slice_header1,
                                         ALIGN(length_in_bits, 32) >> 5,
                                         length_in_bits & 0x1f,
                                         skip_emul_byte_cnt,
                                         1,
                                         0,
                                         !param->has_emulation_bytes,
                                         1);
    }

    return;
}

static void
gen9_vdenc_mfx_avc_inset_headers(VADriverContextP ctx,
                                 struct encode_state *encode_state,
                                 struct intel_encoder_context *encoder_context,
                                 VAEncSliceParameterBufferH264 *slice_param,
                                 int slice_index)
{
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    int idx = va_enc_packed_type_to_idx(VAEncPackedHeaderH264_SPS);
    unsigned int internal_rate_mode = vdenc_context->internal_rate_mode;
    unsigned int skip_emul_byte_cnt;

    if (slice_index == 0) {

        if (encode_state->packed_header_data[idx]) {
            VAEncPackedHeaderParameterBuffer *param = NULL;
            unsigned int *header_data = (unsigned int *)encode_state->packed_header_data[idx]->buffer;
            unsigned int length_in_bits;

            assert(encode_state->packed_header_param[idx]);
            param = (VAEncPackedHeaderParameterBuffer *)encode_state->packed_header_param[idx]->buffer;
            length_in_bits = param->bit_length;

            skip_emul_byte_cnt = intel_avc_find_skipemulcnt((unsigned char *)header_data, length_in_bits);
            gen9_vdenc_mfx_avc_insert_object(ctx,
                                             encoder_context,
                                             header_data,
                                             ALIGN(length_in_bits, 32) >> 5,
                                             length_in_bits & 0x1f,
                                             skip_emul_byte_cnt,
                                             0,
                                             0,
                                             !param->has_emulation_bytes,
                                             0);

        }

        idx = va_enc_packed_type_to_idx(VAEncPackedHeaderH264_PPS);

        if (encode_state->packed_header_data[idx]) {
            VAEncPackedHeaderParameterBuffer *param = NULL;
            unsigned int *header_data = (unsigned int *)encode_state->packed_header_data[idx]->buffer;
            unsigned int length_in_bits;

            assert(encode_state->packed_header_param[idx]);
            param = (VAEncPackedHeaderParameterBuffer *)encode_state->packed_header_param[idx]->buffer;
            length_in_bits = param->bit_length;

            skip_emul_byte_cnt = intel_avc_find_skipemulcnt((unsigned char *)header_data, length_in_bits);

            gen9_vdenc_mfx_avc_insert_object(ctx,
                                             encoder_context,
                                             header_data,
                                             ALIGN(length_in_bits, 32) >> 5,
                                             length_in_bits & 0x1f,
                                             skip_emul_byte_cnt,
                                             0,
                                             0,
                                             !param->has_emulation_bytes,
                                             0);

        }

        idx = va_enc_packed_type_to_idx(VAEncPackedHeaderH264_SEI);

        if (encode_state->packed_header_data[idx]) {
            VAEncPackedHeaderParameterBuffer *param = NULL;
            unsigned int *header_data = (unsigned int *)encode_state->packed_header_data[idx]->buffer;
            unsigned int length_in_bits;

            assert(encode_state->packed_header_param[idx]);
            param = (VAEncPackedHeaderParameterBuffer *)encode_state->packed_header_param[idx]->buffer;
            length_in_bits = param->bit_length;

            skip_emul_byte_cnt = intel_avc_find_skipemulcnt((unsigned char *)header_data, length_in_bits);
            gen9_vdenc_mfx_avc_insert_object(ctx,
                                             encoder_context,
                                             header_data,
                                             ALIGN(length_in_bits, 32) >> 5,
                                             length_in_bits & 0x1f,
                                             skip_emul_byte_cnt,
                                             0,
                                             0,
                                             !param->has_emulation_bytes,
                                             0);

        } else if (internal_rate_mode == I965_BRC_CBR) {
            /* TODO: insert others */
        }
    }

    gen9_vdenc_mfx_avc_insert_slice_packed_data(ctx,
                                                encode_state,
                                                encoder_context,
                                                slice_index);
}

static void
gen9_vdenc_mfx_avc_slice_state(VADriverContextP ctx,
                               struct encode_state *encode_state,
                               struct intel_encoder_context *encoder_context,
                               VAEncPictureParameterBufferH264 *pic_param,
                               VAEncSliceParameterBufferH264 *slice_param,
                               VAEncSliceParameterBufferH264 *next_slice_param,
                               int slice_index)
{
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    unsigned int luma_log2_weight_denom = slice_param->luma_log2_weight_denom;
    unsigned int chroma_log2_weight_denom = slice_param->chroma_log2_weight_denom;
    unsigned char correct[6], grow, shrink;
    int slice_hor_pos, slice_ver_pos, next_slice_hor_pos, next_slice_ver_pos;
    int max_qp_n, max_qp_p;
    int i;
    int weighted_pred_idc = 0;
    int num_ref_l0 = 0, num_ref_l1 = 0;
    int slice_type = intel_avc_enc_slice_type_fixup(slice_param->slice_type);
    int slice_qp = pic_param->pic_init_qp + slice_param->slice_qp_delta; // TODO: fix for CBR&VBR */
    int inter_rounding = 0;

    if (vdenc_context->internal_rate_mode != I965_BRC_CQP)
        inter_rounding = 3;

    slice_hor_pos = slice_param->macroblock_address % vdenc_context->frame_width_in_mbs;
    slice_ver_pos = slice_param->macroblock_address / vdenc_context->frame_width_in_mbs;

    if (next_slice_param) {
        next_slice_hor_pos = next_slice_param->macroblock_address % vdenc_context->frame_width_in_mbs;
        next_slice_ver_pos = next_slice_param->macroblock_address / vdenc_context->frame_width_in_mbs;
    } else {
        next_slice_hor_pos = 0;
        next_slice_ver_pos = vdenc_context->frame_height_in_mbs;
    }

    if (slice_type == SLICE_TYPE_I) {
        luma_log2_weight_denom = 0;
        chroma_log2_weight_denom = 0;
    } else if (slice_type == SLICE_TYPE_P) {
        weighted_pred_idc = pic_param->pic_fields.bits.weighted_pred_flag;
        num_ref_l0 = pic_param->num_ref_idx_l0_active_minus1 + 1;

        if (slice_param->num_ref_idx_active_override_flag)
            num_ref_l0 = slice_param->num_ref_idx_l0_active_minus1 + 1;
    } else if (slice_type == SLICE_TYPE_B) {
        weighted_pred_idc = pic_param->pic_fields.bits.weighted_bipred_idc;
        num_ref_l0 = pic_param->num_ref_idx_l0_active_minus1 + 1;
        num_ref_l1 = pic_param->num_ref_idx_l1_active_minus1 + 1;

        if (slice_param->num_ref_idx_active_override_flag) {
            num_ref_l0 = slice_param->num_ref_idx_l0_active_minus1 + 1;
            num_ref_l1 = slice_param->num_ref_idx_l1_active_minus1 + 1;
        }

        if (weighted_pred_idc == 2) {
            /* 8.4.3 - Derivation process for prediction weights (8-279) */
            luma_log2_weight_denom = 5;
            chroma_log2_weight_denom = 5;
        }
    }

    max_qp_n = 0;       /* TODO: update it */
    max_qp_p = 0;       /* TODO: update it */
    grow = 0;           /* TODO: update it */
    shrink = 0;         /* TODO: update it */

    for (i = 0; i < 6; i++)
        correct[i] = 0; /* TODO: update it */

    BEGIN_BCS_BATCH(batch, 11);

    OUT_BCS_BATCH(batch, MFX_AVC_SLICE_STATE | (11 - 2));
    OUT_BCS_BATCH(batch, slice_type);
    OUT_BCS_BATCH(batch,
                  (num_ref_l0 << 16) |
                  (num_ref_l1 << 24) |
                  (chroma_log2_weight_denom << 8) |
                  (luma_log2_weight_denom << 0));
    OUT_BCS_BATCH(batch,
                  (weighted_pred_idc << 30) |
                  (slice_param->direct_spatial_mv_pred_flag << 29) |
                  (slice_param->disable_deblocking_filter_idc << 27) |
                  (slice_param->cabac_init_idc << 24) |
                  (slice_qp << 16) |
                  ((slice_param->slice_beta_offset_div2 & 0xf) << 8) |
                  ((slice_param->slice_alpha_c0_offset_div2 & 0xf) << 0));

    OUT_BCS_BATCH(batch,
                  slice_ver_pos << 24 |
                  slice_hor_pos << 16 |
                  slice_param->macroblock_address);
    OUT_BCS_BATCH(batch,
                  next_slice_ver_pos << 16 |
                  next_slice_hor_pos);

    OUT_BCS_BATCH(batch,
                  (0 << 31) |           /* TODO: ignore it for VDENC ??? */
                  (!slice_param->macroblock_address << 30) |    /* ResetRateControlCounter */
                  (2 << 28) |       /* Loose Rate Control */
                  (0 << 24) |           /* RC Stable Tolerance */
                  (0 << 23) |           /* RC Panic Enable */
                  (1 << 22) |           /* CBP mode */
                  (0 << 21) |           /* MB Type Direct Conversion, 0: Enable, 1: Disable */
                  (0 << 20) |           /* MB Type Skip Conversion, 0: Enable, 1: Disable */
                  (!next_slice_param << 19) |                   /* Is Last Slice */
                  (0 << 18) |           /* BitstreamOutputFlag Compressed BitStream Output Disable Flag 0:enable 1:disable */
                  (1 << 17) |           /* HeaderPresentFlag */
                  (1 << 16) |           /* SliceData PresentFlag */
                  (0 << 15) |           /* TailPresentFlag, TODO: check it on VDEnc  */
                  (1 << 13) |           /* RBSP NAL TYPE */
                  (slice_index << 4) |
                  (1 << 12));           /* CabacZeroWordInsertionEnable */

    OUT_BCS_BATCH(batch, vdenc_context->compressed_bitstream.start_offset);

    OUT_BCS_BATCH(batch,
                  (max_qp_n << 24) |     /*Target QP - 24 is lowest QP*/
                  (max_qp_p << 16) |     /*Target QP + 20 is highest QP*/
                  (shrink << 8) |
                  (grow << 0));
    OUT_BCS_BATCH(batch,
                  (1 << 31) |
                  (inter_rounding << 28) |
                  (1 << 27) |
                  (5 << 24) |
                  (correct[5] << 20) |
                  (correct[4] << 16) |
                  (correct[3] << 12) |
                  (correct[2] << 8) |
                  (correct[1] << 4) |
                  (correct[0] << 0));
    OUT_BCS_BATCH(batch, 0);

    ADVANCE_BCS_BATCH(batch);
}

static uint8_t
gen9_vdenc_mfx_get_ref_idx_state(VAPictureH264 *va_pic, unsigned int frame_store_id)
{
    unsigned int is_long_term =
        !!(va_pic->flags & VA_PICTURE_H264_LONG_TERM_REFERENCE);
    unsigned int is_top_field =
        !!(va_pic->flags & VA_PICTURE_H264_TOP_FIELD);
    unsigned int is_bottom_field =
        !!(va_pic->flags & VA_PICTURE_H264_BOTTOM_FIELD);

    return ((is_long_term                         << 6) |
            ((is_top_field ^ is_bottom_field ^ 1) << 5) |
            (frame_store_id                       << 1) |
            ((is_top_field ^ 1) & is_bottom_field));
}

static void
gen9_vdenc_mfx_avc_ref_idx_state(VADriverContextP ctx,
                                 struct encode_state *encode_state,
                                 struct intel_encoder_context *encoder_context,
                                 VAEncSliceParameterBufferH264 *slice_param)
{
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    VAPictureH264 *ref_pic;
    int i, slice_type, ref_idx_shift;
    unsigned int fwd_ref_entry;

    fwd_ref_entry = 0x80808080;
    slice_type = intel_avc_enc_slice_type_fixup(slice_param->slice_type);

    for (i = 0; i < MIN(vdenc_context->num_refs[0], 3); i++) {
        ref_pic = &slice_param->RefPicList0[i];
        ref_idx_shift = i * 8;

        if (vdenc_context->list_ref_idx[0][i] == 0xFF)
            continue;

        fwd_ref_entry &= ~(0xFF << ref_idx_shift);
        fwd_ref_entry += (gen9_vdenc_mfx_get_ref_idx_state(ref_pic, vdenc_context->list_ref_idx[0][i]) << ref_idx_shift);
    }

    if (slice_type == SLICE_TYPE_P) {
        BEGIN_BCS_BATCH(batch, 10);
        OUT_BCS_BATCH(batch, MFX_AVC_REF_IDX_STATE | 8);
        OUT_BCS_BATCH(batch, 0);                        // L0
        OUT_BCS_BATCH(batch, fwd_ref_entry);

        for (i = 0; i < 7; i++) {
            OUT_BCS_BATCH(batch, 0x80808080);
        }

        ADVANCE_BCS_BATCH(batch);
    }

    if (slice_type == SLICE_TYPE_B) {
        /* VDEnc on SKL doesn't support BDW */
        assert(0);
    }
}

static void
gen9_vdenc_mfx_avc_weightoffset_state(VADriverContextP ctx,
                                      struct encode_state *encode_state,
                                      struct intel_encoder_context *encoder_context,
                                      VAEncPictureParameterBufferH264 *pic_param,
                                      VAEncSliceParameterBufferH264 *slice_param)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    int i, slice_type;
    short weightoffsets[32 * 6];

    slice_type = intel_avc_enc_slice_type_fixup(slice_param->slice_type);

    if (slice_type == SLICE_TYPE_P &&
        pic_param->pic_fields.bits.weighted_pred_flag == 1) {

        for (i = 0; i < 32; i++) {
            weightoffsets[i * 6 + 0] = slice_param->luma_weight_l0[i];
            weightoffsets[i * 6 + 1] = slice_param->luma_offset_l0[i];
            weightoffsets[i * 6 + 2] = slice_param->chroma_weight_l0[i][0];
            weightoffsets[i * 6 + 3] = slice_param->chroma_offset_l0[i][0];
            weightoffsets[i * 6 + 4] = slice_param->chroma_weight_l0[i][1];
            weightoffsets[i * 6 + 5] = slice_param->chroma_offset_l0[i][1];
        }

        BEGIN_BCS_BATCH(batch, 98);
        OUT_BCS_BATCH(batch, MFX_AVC_WEIGHTOFFSET_STATE | (98 - 2));
        OUT_BCS_BATCH(batch, 0);
        intel_batchbuffer_data(batch, weightoffsets, sizeof(weightoffsets));

        ADVANCE_BCS_BATCH(batch);
    }

    if (slice_type == SLICE_TYPE_B) {
        /* VDEnc on SKL doesn't support BWD */
        assert(0);
    }
}

static void
gen9_vdenc_mfx_avc_single_slice(VADriverContextP ctx,
                                struct encode_state *encode_state,
                                struct intel_encoder_context *encoder_context,
                                VAEncSliceParameterBufferH264 *slice_param,
                                VAEncSliceParameterBufferH264 *next_slice_param,
                                int slice_index)
{
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    VAEncPictureParameterBufferH264 *pic_param = (VAEncPictureParameterBufferH264 *)encode_state->pic_param_ext->buffer;

    gen9_vdenc_mfx_avc_ref_idx_state(ctx, encode_state, encoder_context, slice_param);
    gen9_vdenc_mfx_avc_weightoffset_state(ctx,
                                          encode_state,
                                          encoder_context,
                                          pic_param,
                                          slice_param);
    gen9_vdenc_mfx_avc_slice_state(ctx,
                                   encode_state,
                                   encoder_context,
                                   pic_param,
                                   slice_param,
                                   next_slice_param,
                                   slice_index);
    gen9_vdenc_mfx_avc_inset_headers(ctx,
                                     encode_state,
                                     encoder_context,
                                     slice_param,
                                     slice_index);

    if (!vdenc_context->is_frame_level_vdenc) {
        gen95_vdenc_vdecn_weihgtsoffsets_state(ctx,
                                               encode_state,
                                               encoder_context,
                                               slice_param);
        gen95_vdenc_vdenc_walker_state(ctx,
                                       encode_state,
                                       encoder_context,
                                       slice_param,
                                       next_slice_param);
    }
}

static void
gen9_vdenc_mfx_vdenc_avc_slices(VADriverContextP ctx,
                                struct encode_state *encode_state,
                                struct intel_encoder_context *encoder_context)
{
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct gpe_mi_flush_dw_parameter mi_flush_dw_params;
    VAEncSliceParameterBufferH264 *slice_param, *next_slice_param, *next_slice_group_param;
    int i, j;
    int slice_index = 0;
    int has_tail = 0;                   /* TODO: check it later */

    for (j = 0; j < encode_state->num_slice_params_ext; j++) {
        slice_param = (VAEncSliceParameterBufferH264 *)encode_state->slice_params_ext[j]->buffer;

        if (j == encode_state->num_slice_params_ext - 1)
            next_slice_group_param = NULL;
        else
            next_slice_group_param = (VAEncSliceParameterBufferH264 *)encode_state->slice_params_ext[j + 1]->buffer;

        for (i = 0; i < encode_state->slice_params_ext[j]->num_elements; i++) {
            if (i < encode_state->slice_params_ext[j]->num_elements - 1)
                next_slice_param = slice_param + 1;
            else
                next_slice_param = next_slice_group_param;

            gen9_vdenc_mfx_avc_single_slice(ctx,
                                            encode_state,
                                            encoder_context,
                                            slice_param,
                                            next_slice_param,
                                            slice_index);

            if (vdenc_context->is_frame_level_vdenc)
                break;
            else {
                struct vd_pipeline_flush_parameter pipeline_flush_params;
                int insert_mi_flush;

                memset(&pipeline_flush_params, 0, sizeof(pipeline_flush_params));

                if (next_slice_group_param) {
                    pipeline_flush_params.mfx_pipeline_done = 1;
                    insert_mi_flush = 1;
                } else if (i < encode_state->slice_params_ext[j]->num_elements - 1) {
                    pipeline_flush_params.mfx_pipeline_done = 1;
                    insert_mi_flush = 1;
                } else {
                    pipeline_flush_params.mfx_pipeline_done = !has_tail;
                    insert_mi_flush = 0;
                }

                pipeline_flush_params.vdenc_pipeline_done = 1;
                pipeline_flush_params.vdenc_pipeline_command_flush = 1;
                pipeline_flush_params.vd_command_message_parser_done = 1;
                gen9_vdenc_vd_pipeline_flush(ctx, encoder_context, &pipeline_flush_params);

                if (insert_mi_flush) {
                    memset(&mi_flush_dw_params, 0, sizeof(mi_flush_dw_params));
                    mi_flush_dw_params.video_pipeline_cache_invalidate = 0;
                    gen8_gpe_mi_flush_dw(ctx, batch, &mi_flush_dw_params);
                }
            }

            slice_param++;
            slice_index++;
        }

        if (vdenc_context->is_frame_level_vdenc)
            break;
    }

    if (vdenc_context->is_frame_level_vdenc) {
        struct vd_pipeline_flush_parameter pipeline_flush_params;

        gen9_vdenc_vdenc_walker_state(ctx, encode_state, encoder_context);

        memset(&pipeline_flush_params, 0, sizeof(pipeline_flush_params));
        pipeline_flush_params.mfx_pipeline_done = !has_tail;
        pipeline_flush_params.vdenc_pipeline_done = 1;
        pipeline_flush_params.vdenc_pipeline_command_flush = 1;
        pipeline_flush_params.vd_command_message_parser_done = 1;
        gen9_vdenc_vd_pipeline_flush(ctx, encoder_context, &pipeline_flush_params);
    }

    if (has_tail) {
        /* TODO: insert a tail if required */
    }

    memset(&mi_flush_dw_params, 0, sizeof(mi_flush_dw_params));
    mi_flush_dw_params.video_pipeline_cache_invalidate = 1;
    gen8_gpe_mi_flush_dw(ctx, batch, &mi_flush_dw_params);
}

static void
gen9_vdenc_mfx_vdenc_pipeline(VADriverContextP ctx,
                              struct encode_state *encode_state,
                              struct intel_encoder_context *encoder_context)
{
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct gpe_mi_batch_buffer_start_parameter mi_batch_buffer_start_params;

    if (vdenc_context->brc_enabled) {
        struct gpe_mi_conditional_batch_buffer_end_parameter mi_conditional_batch_buffer_end_params;

        memset(&mi_conditional_batch_buffer_end_params, 0, sizeof(mi_conditional_batch_buffer_end_params));
        mi_conditional_batch_buffer_end_params.bo = vdenc_context->huc_status2_res.bo;
        gen9_gpe_mi_conditional_batch_buffer_end(ctx, batch, &mi_conditional_batch_buffer_end_params);
    }

    if (vdenc_context->current_pass) {
        struct gpe_mi_conditional_batch_buffer_end_parameter mi_conditional_batch_buffer_end_params;

        memset(&mi_conditional_batch_buffer_end_params, 0, sizeof(mi_conditional_batch_buffer_end_params));
        mi_conditional_batch_buffer_end_params.bo = vdenc_context->huc_status_res.bo;
        gen9_gpe_mi_conditional_batch_buffer_end(ctx, batch, &mi_conditional_batch_buffer_end_params);
    }

    gen9_vdenc_mfx_pipe_mode_select(ctx, encode_state, encoder_context);

    gen9_vdenc_mfx_surface_state(ctx, encoder_context, &vdenc_context->recon_surface_res, 0);
    gen9_vdenc_mfx_surface_state(ctx, encoder_context, &vdenc_context->uncompressed_input_surface_res, 4);
    gen9_vdenc_mfx_surface_state(ctx, encoder_context, &vdenc_context->scaled_4x_recon_surface_res, 5);

    gen9_vdenc_mfx_pipe_buf_addr_state(ctx, encoder_context);
    gen9_vdenc_mfx_ind_obj_base_addr_state(ctx, encoder_context);
    gen9_vdenc_mfx_bsp_buf_base_addr_state(ctx, encoder_context);

    gen9_vdenc_vdenc_pipe_mode_select(ctx, encode_state, encoder_context);
    gen9_vdenc_vdenc_src_surface_state(ctx, encoder_context, &vdenc_context->uncompressed_input_surface_res);
    gen9_vdenc_vdenc_ref_surface_state(ctx, encoder_context, &vdenc_context->recon_surface_res);
    gen9_vdenc_vdenc_ds_ref_surface_state(ctx, encoder_context, &vdenc_context->scaled_4x_recon_surface_res);
    gen9_vdenc_vdenc_pipe_buf_addr_state(ctx, encode_state, encoder_context);
    gen9_vdenc_vdenc_const_qpt_state(ctx, encode_state, encoder_context);

    if (!vdenc_context->brc_enabled) {
        gen9_vdenc_mfx_avc_img_state(ctx, encode_state, encoder_context);
        gen9_vdenc_vdenc_img_state(ctx, encode_state, encoder_context);
    } else {
        memset(&mi_batch_buffer_start_params, 0, sizeof(mi_batch_buffer_start_params));
        mi_batch_buffer_start_params.is_second_level = 1; /* Must be the second level batch buffer */
        mi_batch_buffer_start_params.bo = vdenc_context->second_level_batch_res.bo;
        gen8_gpe_mi_batch_buffer_start(ctx, batch, &mi_batch_buffer_start_params);
    }

    gen9_vdenc_mfx_avc_qm_state(ctx, encoder_context);
    gen9_vdenc_mfx_avc_fqm_state(ctx, encoder_context);

    gen9_vdenc_mfx_vdenc_avc_slices(ctx, encode_state, encoder_context);
}

static void
gen9_vdenc_context_brc_prepare(struct encode_state *encode_state,
                               struct intel_encoder_context *encoder_context)
{
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    unsigned int rate_control_mode = encoder_context->rate_control_mode;

    switch (rate_control_mode & 0x7f) {
    case VA_RC_CBR:
        vdenc_context->internal_rate_mode = I965_BRC_CBR;
        break;

    case VA_RC_VBR:
        vdenc_context->internal_rate_mode = I965_BRC_VBR;
        break;

    case VA_RC_CQP:
    default:
        vdenc_context->internal_rate_mode = I965_BRC_CQP;
        break;
    }
}

static void
gen9_vdenc_read_status(VADriverContextP ctx, struct intel_encoder_context *encoder_context)
{
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct gpe_mi_store_register_mem_parameter mi_store_register_mem_params;
    struct gpe_mi_flush_dw_parameter mi_flush_dw_params;
    unsigned int base_offset = vdenc_context->status_bffuer.base_offset;
    int i;

    memset(&mi_flush_dw_params, 0, sizeof(mi_flush_dw_params));
    gen8_gpe_mi_flush_dw(ctx, batch, &mi_flush_dw_params);

    memset(&mi_store_register_mem_params, 0, sizeof(mi_store_register_mem_params));
    mi_store_register_mem_params.mmio_offset = MFC_BITSTREAM_BYTECOUNT_FRAME_REG; /* TODO: fix it if VDBOX2 is used */
    mi_store_register_mem_params.bo = vdenc_context->status_bffuer.res.bo;
    mi_store_register_mem_params.offset = base_offset + vdenc_context->status_bffuer.bytes_per_frame_offset;
    gen8_gpe_mi_store_register_mem(ctx, batch, &mi_store_register_mem_params);

    /* Update DMEM buffer for BRC Update */
    for (i = 0; i < NUM_OF_BRC_PAK_PASSES; i++) {
        mi_store_register_mem_params.mmio_offset = MFC_BITSTREAM_BYTECOUNT_FRAME_REG; /* TODO: fix it if VDBOX2 is used */
        mi_store_register_mem_params.bo = vdenc_context->brc_update_dmem_res[i].bo;
        mi_store_register_mem_params.offset = 5 * sizeof(uint32_t);
        gen8_gpe_mi_store_register_mem(ctx, batch, &mi_store_register_mem_params);

        mi_store_register_mem_params.mmio_offset = MFC_IMAGE_STATUS_CTRL_REG; /* TODO: fix it if VDBOX2 is used */
        mi_store_register_mem_params.bo = vdenc_context->brc_update_dmem_res[i].bo;
        mi_store_register_mem_params.offset = 7 * sizeof(uint32_t);
        gen8_gpe_mi_store_register_mem(ctx, batch, &mi_store_register_mem_params);
    }
}

static VAStatus
gen9_vdenc_avc_check_capability(VADriverContextP ctx,
                                struct encode_state *encode_state,
                                struct intel_encoder_context *encoder_context)
{
    VAEncSliceParameterBufferH264 *slice_param;
    int i, j;

    for (j = 0; j < encode_state->num_slice_params_ext; j++) {
        slice_param = (VAEncSliceParameterBufferH264 *)encode_state->slice_params_ext[j]->buffer;

        for (i = 0; i < encode_state->slice_params_ext[j]->num_elements; i++) {
            if (slice_param->slice_type == SLICE_TYPE_B)
                return VA_STATUS_ERROR_UNKNOWN;

            slice_param++;
        }
    }

    return VA_STATUS_SUCCESS;
}

static VAStatus
gen9_vdenc_avc_encode_picture(VADriverContextP ctx,
                              VAProfile profile,
                              struct encode_state *encode_state,
                              struct intel_encoder_context *encoder_context)
{
    VAStatus va_status;
    struct gen9_vdenc_context *vdenc_context = encoder_context->mfc_context;
    struct intel_batchbuffer *batch = encoder_context->base.batch;

    va_status = gen9_vdenc_avc_check_capability(ctx, encode_state, encoder_context);

    if (va_status != VA_STATUS_SUCCESS)
        return va_status;

    gen9_vdenc_avc_prepare(ctx, profile, encode_state, encoder_context);

    for (vdenc_context->current_pass = 0; vdenc_context->current_pass < vdenc_context->num_passes; vdenc_context->current_pass++) {
        vdenc_context->is_first_pass = (vdenc_context->current_pass == 0);
        vdenc_context->is_last_pass = (vdenc_context->current_pass == (vdenc_context->num_passes - 1));

        intel_batchbuffer_start_atomic_bcs_override(batch, 0x1000, BSD_RING0);

        intel_batchbuffer_emit_mi_flush(batch);

        if (vdenc_context->brc_enabled) {
            if (!vdenc_context->brc_initted || vdenc_context->brc_need_reset)
                gen9_vdenc_huc_brc_init_reset(ctx, encode_state, encoder_context);

            gen9_vdenc_huc_brc_update(ctx, encode_state, encoder_context);
            intel_batchbuffer_emit_mi_flush(batch);
        }

        gen9_vdenc_mfx_vdenc_pipeline(ctx, encode_state, encoder_context);
        gen9_vdenc_read_status(ctx, encoder_context);

        intel_batchbuffer_end_atomic(batch);
        intel_batchbuffer_flush(batch);

        vdenc_context->brc_initted = 1;
        vdenc_context->brc_need_reset = 0;
    }

    return VA_STATUS_SUCCESS;
}

static VAStatus
gen9_vdenc_pipeline(VADriverContextP ctx,
                    VAProfile profile,
                    struct encode_state *encode_state,
                    struct intel_encoder_context *encoder_context)
{
    VAStatus vaStatus;

    switch (profile) {
    case VAProfileH264ConstrainedBaseline:
    case VAProfileH264Main:
    case VAProfileH264High:
        vaStatus = gen9_vdenc_avc_encode_picture(ctx, profile, encode_state, encoder_context);
        break;

    default:
        vaStatus = VA_STATUS_ERROR_UNSUPPORTED_PROFILE;
        break;
    }

    return vaStatus;
}

static void
gen9_vdenc_free_resources(struct gen9_vdenc_context *vdenc_context)
{
    int i;

    i965_free_gpe_resource(&vdenc_context->brc_init_reset_dmem_res);
    i965_free_gpe_resource(&vdenc_context->brc_history_buffer_res);
    i965_free_gpe_resource(&vdenc_context->brc_stream_in_res);
    i965_free_gpe_resource(&vdenc_context->brc_stream_out_res);
    i965_free_gpe_resource(&vdenc_context->huc_dummy_res);

    for (i = 0; i < NUM_OF_BRC_PAK_PASSES; i++)
        i965_free_gpe_resource(&vdenc_context->brc_update_dmem_res[i]);

    i965_free_gpe_resource(&vdenc_context->vdenc_statistics_res);
    i965_free_gpe_resource(&vdenc_context->pak_statistics_res);
    i965_free_gpe_resource(&vdenc_context->vdenc_avc_image_state_res);
    i965_free_gpe_resource(&vdenc_context->hme_detection_summary_buffer_res);
    i965_free_gpe_resource(&vdenc_context->brc_constant_data_res);
    i965_free_gpe_resource(&vdenc_context->second_level_batch_res);

    i965_free_gpe_resource(&vdenc_context->huc_status_res);
    i965_free_gpe_resource(&vdenc_context->huc_status2_res);

    i965_free_gpe_resource(&vdenc_context->recon_surface_res);
    i965_free_gpe_resource(&vdenc_context->scaled_4x_recon_surface_res);
    i965_free_gpe_resource(&vdenc_context->post_deblocking_output_res);
    i965_free_gpe_resource(&vdenc_context->pre_deblocking_output_res);

    for (i = 0; i < ARRAY_ELEMS(vdenc_context->list_reference_res); i++) {
        i965_free_gpe_resource(&vdenc_context->list_reference_res[i]);
        i965_free_gpe_resource(&vdenc_context->list_scaled_4x_reference_res[i]);
    }

    i965_free_gpe_resource(&vdenc_context->uncompressed_input_surface_res);
    i965_free_gpe_resource(&vdenc_context->compressed_bitstream.res);
    i965_free_gpe_resource(&vdenc_context->status_bffuer.res);

    i965_free_gpe_resource(&vdenc_context->mfx_intra_row_store_scratch_res);
    i965_free_gpe_resource(&vdenc_context->mfx_deblocking_filter_row_store_scratch_res);
    i965_free_gpe_resource(&vdenc_context->mfx_bsd_mpc_row_store_scratch_res);
    i965_free_gpe_resource(&vdenc_context->vdenc_row_store_scratch_res);

    i965_free_gpe_resource(&vdenc_context->vdenc_streamin_res);
}

static void
gen9_vdenc_context_destroy(void *context)
{
    struct gen9_vdenc_context *vdenc_context = context;

    gen9_vdenc_free_resources(vdenc_context);

    free(vdenc_context);
}

static void
gen9_vdenc_allocate_resources(VADriverContextP ctx,
                              struct intel_encoder_context *encoder_context,
                              struct gen9_vdenc_context *vdenc_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    int i;

    ALLOC_VDENC_BUFFER_RESOURCE(vdenc_context->brc_init_reset_dmem_res,
                                ALIGN(sizeof(struct huc_brc_init_dmem), 64),
                                "HuC Init&Reset DMEM buffer");

    ALLOC_VDENC_BUFFER_RESOURCE(vdenc_context->brc_history_buffer_res,
                                ALIGN(HUC_BRC_HISTORY_BUFFER_SIZE, 0x1000),
                                "HuC History buffer");

    ALLOC_VDENC_BUFFER_RESOURCE(vdenc_context->brc_stream_in_res,
                                ALIGN(HUC_BRC_STREAM_INOUT_BUFFER_SIZE, 0x1000),
                                "HuC Stream In buffer");

    ALLOC_VDENC_BUFFER_RESOURCE(vdenc_context->brc_stream_out_res,
                                ALIGN(HUC_BRC_STREAM_INOUT_BUFFER_SIZE, 0x1000),
                                "HuC Stream Out buffer");

    ALLOC_VDENC_BUFFER_RESOURCE(vdenc_context->huc_dummy_res,
                                0x1000,
                                "HuC dummy buffer");

    for (i = 0; i < NUM_OF_BRC_PAK_PASSES; i++) {
        ALLOC_VDENC_BUFFER_RESOURCE(vdenc_context->brc_update_dmem_res[i],
                                    ALIGN(sizeof(struct huc_brc_update_dmem), 64),
                                    "HuC BRC Update buffer");
        i965_zero_gpe_resource(&vdenc_context->brc_update_dmem_res[i]);
    }

    ALLOC_VDENC_BUFFER_RESOURCE(vdenc_context->vdenc_statistics_res,
                                ALIGN(VDENC_STATISTICS_SIZE, 0x1000),
                                "VDENC statistics buffer");

    ALLOC_VDENC_BUFFER_RESOURCE(vdenc_context->pak_statistics_res,
                                ALIGN(PAK_STATISTICS_SIZE, 0x1000),
                                "PAK statistics buffer");

    ALLOC_VDENC_BUFFER_RESOURCE(vdenc_context->vdenc_avc_image_state_res,
                                ALIGN(VDENC_AVC_IMAGE_STATE_SIZE, 0x1000),
                                "VDENC/AVC image state buffer");

    ALLOC_VDENC_BUFFER_RESOURCE(vdenc_context->hme_detection_summary_buffer_res,
                                ALIGN(HME_DETECTION_SUMMARY_BUFFER_SIZE, 0x1000),
                                "HME summary buffer");

    ALLOC_VDENC_BUFFER_RESOURCE(vdenc_context->brc_constant_data_res,
                                ALIGN(BRC_CONSTANT_DATA_SIZE, 0x1000),
                                "BRC constant buffer");

    ALLOC_VDENC_BUFFER_RESOURCE(vdenc_context->second_level_batch_res,
                                ALIGN(VDENC_AVC_IMAGE_STATE_SIZE, 0x1000),
                                "Second level batch buffer");

    ALLOC_VDENC_BUFFER_RESOURCE(vdenc_context->huc_status_res,
                                0x1000,
                                "HuC Status buffer");

    ALLOC_VDENC_BUFFER_RESOURCE(vdenc_context->huc_status2_res,
                                0x1000,
                                "HuC Status buffer");
}

static void
gen9_vdenc_hw_interfaces_init(VADriverContextP ctx,
                              struct intel_encoder_context *encoder_context,
                              struct gen9_vdenc_context *vdenc_context)
{
    vdenc_context->is_frame_level_vdenc = 1;
}

static void
gen95_vdenc_hw_interfaces_init(VADriverContextP ctx,
                               struct intel_encoder_context *encoder_context,
                               struct gen9_vdenc_context *vdenc_context)
{
    vdenc_context->use_extended_pak_obj_cmd = 1;
}

static void
vdenc_hw_interfaces_init(VADriverContextP ctx,
                         struct intel_encoder_context *encoder_context,
                         struct gen9_vdenc_context *vdenc_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);

    if (IS_KBL(i965->intel.device_info) ||
        IS_GLK(i965->intel.device_info)) {
        gen95_vdenc_hw_interfaces_init(ctx, encoder_context, vdenc_context);
    } else {
        gen9_vdenc_hw_interfaces_init(ctx, encoder_context, vdenc_context);
    }
}

static VAStatus
gen9_vdenc_context_get_status(VADriverContextP ctx,
                              struct intel_encoder_context *encoder_context,
                              struct i965_coded_buffer_segment *coded_buffer_segment)
{
    struct gen9_vdenc_status *vdenc_status = (struct gen9_vdenc_status *)coded_buffer_segment->codec_private_data;

    coded_buffer_segment->base.size = vdenc_status->bytes_per_frame;

    return VA_STATUS_SUCCESS;
}

Bool
gen9_vdenc_context_init(VADriverContextP ctx, struct intel_encoder_context *encoder_context)
{
    struct gen9_vdenc_context *vdenc_context = calloc(1, sizeof(struct gen9_vdenc_context));

    if (!vdenc_context)
        return False;

    vdenc_context->brc_initted = 0;
    vdenc_context->brc_need_reset = 0;
    vdenc_context->is_low_delay = 0;
    vdenc_context->current_pass = 0;
    vdenc_context->num_passes = 1;
    vdenc_context->vdenc_streamin_enable = 0;
    vdenc_context->vdenc_pak_threshold_check_enable = 0;
    vdenc_context->is_frame_level_vdenc = 0;

    vdenc_hw_interfaces_init(ctx, encoder_context, vdenc_context);
    gen9_vdenc_allocate_resources(ctx, encoder_context, vdenc_context);

    encoder_context->mfc_context = vdenc_context;
    encoder_context->mfc_context_destroy = gen9_vdenc_context_destroy;
    encoder_context->mfc_pipeline = gen9_vdenc_pipeline;
    encoder_context->mfc_brc_prepare = gen9_vdenc_context_brc_prepare;
    encoder_context->get_status = gen9_vdenc_context_get_status;

    return True;
}
