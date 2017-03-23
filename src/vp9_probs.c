/*
 * Copyright © 2016 Intel Corporation
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
 *    Zhao Yakui <yakui.zhao@intel.com>
 */
/*
 * This file defines some vp9 probability tables, and
 * they are ported from libvpx (https://github.com/webmproject/libvpx/).
 * The original copyright and licence statement as below.
 */

/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "vp9_probs.h"
#include "i965_drv_video.h"
#include <stdlib.h>

struct tx_probs default_tx_probs = {
    {   { 100 },
        { 66  }
    },

    {   { 20, 152 },
        { 15, 101 }
    },

    {   { 3, 136, 37 },
        { 5, 52,  13 }
    },
};

vp9_prob default_skip_probs[SKIP_CONTEXTS] = {
    192, 128, 64
};

vp9_prob default_inter_mode_probs[INTER_MODE_CONTEXTS]
[INTER_MODES - 1] = {
    {2,       173,   34},  // 0 = both zero mv
    {7,       145,   85},  // 1 = one zero mv + one a predicted mv
    {7,       166,   63},  // 2 = two predicted mvs
    {7,       94,    66},  // 3 = one predicted/zero and one new mv
    {8,       64,    46},  // 4 = two new mvs
    {17,      81,    31},  // 5 = one intra neighbour + x
    {25,      29,    30},  // 6 = two intra neighbours
};

vp9_prob default_switchable_interp_prob[SWITCHABLE_FILTER_CONTEXTS][SWITCHABLE_FILTERS - 1] = {
    { 235, 162, },
    { 36, 255, },
    { 34, 3, },
    { 149, 144, },
};

vp9_prob default_intra_inter_p[INTRA_INTER_CONTEXTS] = {
    9, 102, 187, 225
};

vp9_prob default_comp_inter_p[COMP_INTER_CONTEXTS] = {
    239, 183, 119,  96,  41
};

vp9_prob default_single_ref_p[REF_CONTEXTS][2] = {
    {  33,  16 },
    {  77,  74 },
    { 142, 142 },
    { 172, 170 },
    { 238, 247 }
};

vp9_prob default_comp_ref_p[REF_CONTEXTS] = {
    50, 126, 123, 221, 226
};

vp9_prob vp9_kf_uv_mode_prob[INTRA_MODES][INTRA_MODES - 1] = {
    { 144,  11,  54, 157, 195, 130,  46,  58, 108 },  // y = dc
    { 118,  15, 123, 148, 131, 101,  44,  93, 131 },  // y = v
    { 113,  12,  23, 188, 226, 142,  26,  32, 125 },  // y = h
    { 120,  11,  50, 123, 163, 135,  64,  77, 103 },  // y = d45
    { 113,   9,  36, 155, 111, 157,  32,  44, 161 },  // y = d135
    { 116,   9,  55, 176,  76,  96,  37,  61, 149 },  // y = d117
    { 115,   9,  28, 141, 161, 167,  21,  25, 193 },  // y = d153
    { 120,  12,  32, 145, 195, 142,  32,  38,  86 },  // y = d207
    { 116,  12,  64, 120, 140, 125,  49, 115, 121 },  // y = d63
    { 102,  19,  66, 162, 182, 122,  35,  59, 128 }   // y = tm
};

vp9_prob default_if_y_probs[BLOCK_SIZE_GROUPS][INTRA_MODES - 1] = {
    {  65,  32,  18, 144, 162, 194,  41,  51,  98 },  // block_size < 8x8
    { 132,  68,  18, 165, 217, 196,  45,  40,  78 },  // block_size < 16x16
    { 173,  80,  19, 176, 240, 193,  64,  35,  46 },  // block_size < 32x32
    { 221, 135,  38, 194, 248, 121,  96,  85,  29 }   // block_size >= 32x32
};

vp9_prob default_if_uv_probs[INTRA_MODES][INTRA_MODES - 1] = {
    { 120,   7,  76, 176, 208, 126,  28,  54, 103 },  // y = dc
    {  48,  12, 154, 155, 139,  90,  34, 117, 119 },  // y = v
    {  67,   6,  25, 204, 243, 158,  13,  21,  96 },  // y = h
    {  97,   5,  44, 131, 176, 139,  48,  68,  97 },  // y = d45
    {  83,   5,  42, 156, 111, 152,  26,  49, 152 },  // y = d135
    {  80,   5,  58, 178,  74,  83,  33,  62, 145 },  // y = d117
    {  86,   5,  32, 154, 192, 168,  14,  22, 163 },  // y = d153
    {  85,   5,  32, 156, 216, 148,  19,  29,  73 },  // y = d207
    {  77,   7,  64, 116, 132, 122,  37, 126, 120 },  // y = d63
    { 101,  21, 107, 181, 192, 103,  19,  67, 125 }   // y = tm
};

vp9_prob default_seg_tree_probs[SEG_TREE_PROBS] = {
    255, 255, 255, 255, 255, 255, 255
};

vp9_prob default_seg_pred_probs[PREDICTION_PROBS] = {
    255, 255, 255
};

vp9_prob vp9_kf_partition_probs[PARTITION_CONTEXTS]
[PARTITION_TYPES - 1] = {
    // 8x8 -> 4x4
    { 158,  97,  94 },  // a/l both not split
    {  93,  24,  99 },  // a split, l not split
    {  85, 119,  44 },  // l split, a not split
    {  62,  59,  67 },  // a/l both split
    // 16x16 -> 8x8
    { 149,  53,  53 },  // a/l both not split
    {  94,  20,  48 },  // a split, l not split
    {  83,  53,  24 },  // l split, a not split
    {  52,  18,  18 },  // a/l both split
    // 32x32 -> 16x16
    { 150,  40,  39 },  // a/l both not split
    {  78,  12,  26 },  // a split, l not split
    {  67,  33,  11 },  // l split, a not split
    {  24,   7,   5 },  // a/l both split
    // 64x64 -> 32x32
    { 174,  35,  49 },  // a/l both not split
    {  68,  11,  27 },  // a split, l not split
    {  57,  15,   9 },  // l split, a not split
    {  12,   3,   3 },  // a/l both split
};

vp9_prob default_partition_probs[PARTITION_CONTEXTS]
[PARTITION_TYPES - 1] = {
    // 8x8 -> 4x4
    { 199, 122, 141 },  // a/l both not split
    { 147,  63, 159 },  // a split, l not split
    { 148, 133, 118 },  // l split, a not split
    { 121, 104, 114 },  // a/l both split
    // 16x16 -> 8x8
    { 174,  73,  87 },  // a/l both not split
    {  92,  41,  83 },  // a split, l not split
    {  82,  99,  50 },  // l split, a not split
    {  53,  39,  39 },  // a/l both split
    // 32x32 -> 16x16
    { 177,  58,  59 },  // a/l both not split
    {  68,  26,  63 },  // a split, l not split
    {  52,  79,  25 },  // l split, a not split
    {  17,  14,  12 },  // a/l both split
    // 64x64 -> 32x32
    { 222,  34,  30 },  // a/l both not split
    {  72,  16,  44 },  // a split, l not split
    {  58,  32,  12 },  // l split, a not split
    {  10,   7,   6 },  // a/l both split
};

//Rearranged the values for better usage
nmv_context default_nmv_context = {
    {32, 64, 96},
    {
        {
            // Vertical component
            128,                                                  // sign
            {224, 144, 192, 168, 192, 176, 192, 198, 198, 245},   // class
            {216},                                                // class0
            {136, 140, 148, 160, 176, 192, 224, 234, 234, 240},   // bits
        },
        {
            // Horizontal component
            128,                                                  // sign
            {216, 128, 176, 160, 176, 176, 192, 198, 198, 208},   // class
            {208},                                                // class0
            {136, 140, 148, 160, 176, 192, 224, 234, 234, 240},   // bits
        }
    },
    {{128, 128, 64}, {96, 112, 64}},                      // class0_fp0
    {64, 96, 64},                                         // fp0
    {{128, 128, 64}, {96, 112, 64}},                      // class0_fp1
    {64, 96, 64},                                         // fp1
    {160, 128},                                           // class0_hp bit
    {160, 128}                                            // hp

};

//Rearranged the coeff probs for better usage
vp9_prob default_coef_probs_4x4[COEFF_PROB_SIZE][COEFF_PROB_NUM] = {
    // Y plane - Intra
    // Band 0
    { 195,  29, 183 }, {  84,  49, 136 }, {   8,  42,  71 },
    // Band 1
    {  31, 107, 169 }, {  35,  99, 159 }, {  17,  82, 140 },
    {   8,  66, 114 }, {   2,  44,  76 }, {   1,  19,  32 },
    // Band 2
    {  40, 132, 201 }, {  29, 114, 187 }, {  13,  91, 157 },
    {   7,  75, 127 }, {   3,  58,  95 }, {   1,  28,  47 },
    // Band 3
    {  69, 142, 221 }, {  42, 122, 201 }, {  15,  91, 159 },
    {   6,  67, 121 }, {   1,  42,  77 }, {   1,  17,  31 },
    // Band 4
    { 102, 148, 228 }, {  67, 117, 204 }, {  17,  82, 154 },
    {   6,  59, 114 }, {   2,  39,  75 }, {   1,  15,  29 },
    // Band 5
    { 156,  57, 233 }, { 119,  57, 212 }, {  58,  48, 163 },
    {  29,  40, 124 }, {  12,  30,  81 }, {   3,  12,  31 },

    // Y plane - Inter
    // Band 0
    { 191, 107, 226 }, { 124, 117, 204 }, {  25,  99, 155 },
    // Band 1
    {  29, 148, 210 }, {  37, 126, 194 }, {   8,  93, 157 },
    {   2,  68, 118 }, {   1,  39,  69 }, {   1,  17,  33 },
    // Band 2
    {  41, 151, 213 }, {  27, 123, 193 }, {   3,  82, 144 },
    {   1,  58, 105 }, {   1,  32,  60 }, {   1,  13,  26 },
    // Band 3
    {  59, 159, 220 }, {  23, 126, 198 }, {   4,  88, 151 },
    {   1,  66, 114 }, {   1,  38,  71 }, {   1,  18,  34 },
    // Band 4
    { 114, 136, 232 }, {  51, 114, 207 }, {  11,  83, 155 },
    {   3,  56, 105 }, {   1,  33,  65 }, {   1,  17,  34 },
    // Band 5
    { 149,  65, 234 }, { 121,  57, 215 }, {  61,  49, 166 },
    {  28,  36, 114 }, {  12,  25,  76 }, {   3,  16,  42 },

    // UV plane - Intra
    // Band 0
    { 214,  49, 220 }, { 132,  63, 188 }, {  42,  65, 137 },
    // Band 1
    {  85, 137, 221 }, { 104, 131, 216 }, {  49, 111, 192 },
    {  21,  87, 155 }, {   2,  49,  87 }, {   1,  16,  28 },
    // Band 2
    {  89, 163, 230 }, {  90, 137, 220 }, {  29, 100, 183 },
    {  10,  70, 135 }, {   2,  42,  81 }, {   1,  17,  33 },
    // Band 3
    { 108, 167, 237 }, {  55, 133, 222 }, {  15,  97, 179 },
    {   4,  72, 135 }, {   1,  45,  85 }, {   1,  19,  38 },
    // Band 4
    { 124, 146, 240 }, {  66, 124, 224 }, {  17,  88, 175 },
    {   4,  58, 122 }, {   1,  36,  75 }, {   1,  18,  37 },
    //  Band 5
    { 141,  79, 241 }, { 126,  70, 227 }, {  66,  58, 182 },
    {  30,  44, 136 }, {  12,  34,  96 }, {   2,  20,  47 },

    // UV plane - Inter
    // Band 0
    { 229,  99, 249 }, { 143, 111, 235 }, {  46, 109, 192 },
    // Band 1
    {  82, 158, 236 }, {  94, 146, 224 }, {  25, 117, 191 },
    {   9,  87, 149 }, {   3,  56,  99 }, {   1,  33,  57 },
    // Band 2
    {  83, 167, 237 }, {  68, 145, 222 }, {  10, 103, 177 },
    {   2,  72, 131 }, {   1,  41,  79 }, {   1,  20,  39 },
    // Band 3
    {  99, 167, 239 }, {  47, 141, 224 }, {  10, 104, 178 },
    {   2,  73, 133 }, {   1,  44,  85 }, {   1,  22,  47 },
    // Band 4
    { 127, 145, 243 }, {  71, 129, 228 }, {  17,  93, 177 },
    {   3,  61, 124 }, {   1,  41,  84 }, {   1,  21,  52 },
    // Band 5
    { 157,  78, 244 }, { 140,  72, 231 }, {  69,  58, 184 },
    {  31,  44, 137 }, {  14,  38, 105 }, {   8,  23,  61 }
};

vp9_prob default_coef_probs_8x8[COEFF_PROB_SIZE][COEFF_PROB_NUM] = {
    // Y plane - Intra
    // Band 0
    { 125,  34, 187 }, {  52,  41, 133 }, {   6,  31,  56 },
    // Band 1
    {  37, 109, 153 }, {  51, 102, 147 }, {  23,  87, 128 },
    {   8,  67, 101 }, {   1,  41,  63 }, {   1,  19,  29 },
    // Band 2
    {  31, 154, 185 }, {  17, 127, 175 }, {   6,  96, 145 },
    {   2,  73, 114 }, {   1,  51,  82 }, {   1,  28,  45 },
    // Band 3
    {  23, 163, 200 }, {  10, 131, 185 }, {   2,  93, 148 },
    {   1,  67, 111 }, {   1,  41,  69 }, {   1,  14,  24 },
    // Band 4
    {  29, 176, 217 }, {  12, 145, 201 }, {   3, 101, 156 },
    {   1,  69, 111 }, {   1,  39,  63 }, {   1,  14,  23 },
    // Band 5
    {  57, 192, 233 }, {  25, 154, 215 }, {   6, 109, 167 },
    {   3,  78, 118 }, {   1,  48,  69 }, {   1,  21,  29 },

    // Y plane - Inter
    // Band 0
    { 202, 105, 245 }, { 108, 106, 216 }, {  18,  90, 144 },
    // Band 1
    {  33, 172, 219 }, {  64, 149, 206 }, {  14, 117, 177 },
    {   5,  90, 141 }, {   2,  61,  95 }, {   1,  37,  57 },
    // Band 2
    {  33, 179, 220 }, {  11, 140, 198 }, {   1,  89, 148 },
    {   1,  60, 104 }, {   1,  33,  57 }, {   1,  12,  21 },
    // Band 3
    {  30, 181, 221 }, {   8, 141, 198 }, {   1,  87, 145 },
    {   1,  58, 100 }, {   1,  31,  55 }, {   1,  12,  20 },
    // Band 4
    {  32, 186, 224 }, {   7, 142, 198 }, {   1,  86, 143 },
    {   1,  58, 100 }, {   1,  31,  55 }, {   1,  12,  22 },
    // Band 5
    {  57, 192, 227 }, {  20, 143, 204 }, {   3,  96, 154 },
    {   1,  68, 112 }, {   1,  42,  69 }, {   1,  19,  32 },

    // UV plane - Intra
    // Band 0
    { 212,  35, 215 }, { 113,  47, 169 }, {  29,  48, 105 },
    // Band 1
    {  74, 129, 203 }, { 106, 120, 203 }, {  49, 107, 178 },
    {  19,  84, 144 }, {   4,  50,  84 }, {   1,  15,  25 },
    // Band 2
    {  71, 172, 217 }, {  44, 141, 209 }, {  15, 102, 173 },
    {   6,  76, 133 }, {   2,  51,  89 }, {   1,  24,  42 },
    // Band 3
    {  64, 185, 231 }, {  31, 148, 216 }, {   8, 103, 175 },
    {   3,  74, 131 }, {   1,  46,  81 }, {   1,  18,  30 },
    // Band 4
    {  65, 196, 235 }, {  25, 157, 221 }, {   5, 105, 174 },
    {   1,  67, 120 }, {   1,  38,  69 }, {   1,  15,  30 },
    // Band 5
    {  65, 204, 238 }, {  30, 156, 224 }, {   7, 107, 177 },
    {   2,  70, 124 }, {   1,  42,  73 }, {   1,  18,  34 },

    // UV Plane - Inter
    // Band 0
    { 225,  86, 251 }, { 144, 104, 235 }, {  42,  99, 181 },
    // Band 1
    {  85, 175, 239 }, { 112, 165, 229 }, {  29, 136, 200 },
    {  12, 103, 162 }, {   6,  77, 123 }, {   2,  53,  84 },
    // Band 2
    {  75, 183, 239 }, {  30, 155, 221 }, {   3, 106, 171 },
    {   1,  74, 128 }, {   1,  44,  76 }, {   1,  17,  28 },
    // Band 3
    {  73, 185, 240 }, {  27, 159, 222 }, {   2, 107, 172 },
    {   1,  75, 127 }, {   1,  42,  73 }, {   1,  17,  29 },
    // Band 4
    {  62, 190, 238 }, {  21, 159, 222 }, {   2, 107, 172 },
    {   1,  72, 122 }, {   1,  40,  71 }, {   1,  18,  32 },
    // Band 5
    {  61, 199, 240 }, {  27, 161, 226 }, {   4, 113, 180 },
    {   1,  76, 129 }, {   1,  46,  80 }, {   1,  23,  41 }
};

vp9_prob default_coef_probs_16x16[COEFF_PROB_SIZE][COEFF_PROB_NUM] = {
    // Y plane - Intra
    // Band 0
    {   7,  27, 153 }, {   5,  30,  95 }, {   1,  16,  30 },
    // Band 1
    {  50,  75, 127 }, {  57,  75, 124 }, {  27,  67, 108 },
    {  10,  54,  86 }, {   1,  33,  52 }, {   1,  12,  18 },
    // Band 2
    {  43, 125, 151 }, {  26, 108, 148 }, {   7,  83, 122 },
    {   2,  59,  89 }, {   1,  38,  60 }, {   1,  17,  27 },
    // Band 3
    {  23, 144, 163 }, {  13, 112, 154 }, {   2,  75, 117 },
    {   1,  50,  81 }, {   1,  31,  51 }, {   1,  14,  23 },
    // Band 4
    {  18, 162, 185 }, {   6, 123, 171 }, {   1,  78, 125 },
    {   1,  51,  86 }, {   1,  31,  54 }, {   1,  14,  23 },
    // Band 5
    {  15, 199, 227 }, {   3, 150, 204 }, {   1,  91, 146 },
    {   1,  55,  95 }, {   1,  30,  53 }, {   1,  11,  20 },

    // Y plane - Inter
    // Band 0
    {  19,  55, 240 }, {  19,  59, 196 }, {   3,  52, 105 },
    // Band 1
    {  41, 166, 207 }, { 104, 153, 199 }, {  31, 123, 181 },
    {  14, 101, 152 }, {   5,  72, 106 }, {   1,  36,  52 },
    // Band 2
    {  35, 176, 211 }, {  12, 131, 190 }, {   2,  88, 144 },
    {   1,  60, 101 }, {   1,  36,  60 }, {   1,  16,  28 },
    // Band 3
    {  28, 183, 213 }, {   8, 134, 191 }, {   1,  86, 142 },
    {   1,  56,  96 }, {   1,  30,  53 }, {   1,  12,  20 },
    // Band 4
    {  20, 190, 215 }, {   4, 135, 192 }, {   1,  84, 139 },
    {   1,  53,  91 }, {   1,  28,  49 }, {   1,  11,  20 },
    // Band 5
    {  13, 196, 216 }, {   2, 137, 192 }, {   1,  86, 143 },
    {   1,  57,  99 }, {   1,  32,  56 }, {   1,  13,  24 },

    // UV plane - Intra
    // Band 0
    { 211,  29, 217 }, {  96,  47, 156 }, {  22,  43,  87 },
    // Band 1
    {  78, 120, 193 }, { 111, 116, 186 }, {  46, 102, 164 },
    {  15,  80, 128 }, {   2,  49,  76 }, {   1,  18,  28 },
    // Band 2
    {  71, 161, 203 }, {  42, 132, 192 }, {  10,  98, 150 },
    {   3,  69, 109 }, {   1,  44,  70 }, {   1,  18,  29 },
    // Band 3
    {  57, 186, 211 }, {  30, 140, 196 }, {   4,  93, 146 },
    {   1,  62, 102 }, {   1,  38,  65 }, {   1,  16,  27 },
    // Band 4
    {  47, 199, 217 }, {  14, 145, 196 }, {   1,  88, 142 },
    {   1,  57,  98 }, {   1,  36,  62 }, {   1,  15,  26 },
    // Band 5
    {  26, 219, 229 }, {   5, 155, 207 }, {   1,  94, 151 },
    {   1,  60, 104 }, {   1,  36,  62 }, {   1,  16,  28 },

    // UV plane - Inter
    // Band 0
    { 233,  29, 248 }, { 146,  47, 220 }, {  43,  52, 140 },
    // Band 1
    { 100, 163, 232 }, { 179, 161, 222 }, {  63, 142, 204 },
    {  37, 113, 174 }, {  26,  89, 137 }, {  18,  68,  97 },
    // Band 2
    {  85, 181, 230 }, {  32, 146, 209 }, {   7, 100, 164 },
    {   3,  71, 121 }, {   1,  45,  77 }, {   1,  18,  30 },
    // Band 3
    {  65, 187, 230 }, {  20, 148, 207 }, {   2,  97, 159 },
    {   1,  68, 116 }, {   1,  40,  70 }, {   1,  14,  29 },
    // Band 4
    {  40, 194, 227 }, {   8, 147, 204 }, {   1,  94, 155 },
    {   1,  65, 112 }, {   1,  39,  66 }, {   1,  14,  26 },
    // Band 5
    {  16, 208, 228 }, {   3, 151, 207 }, {   1,  98, 160 },
    {   1,  67, 117 }, {   1,  41,  74 }, {   1,  17,  31 }
};

vp9_prob default_coef_probs_32x32[COEFF_PROB_SIZE][COEFF_PROB_NUM] = {
    // Y plane - Intra
    // Band 0
    {  17,  38, 140 }, {   7,  34,  80 }, {   1,  17,  29 },
    // Band 1
    {  37,  75, 128 }, {  41,  76, 128 }, {  26,  66, 116 },
    {  12,  52,  94 }, {   2,  32,  55 }, {   1,  10,  16 },
    // Band 2
    {  50, 127, 154 }, {  37, 109, 152 }, {  16,  82, 121 },
    {   5,  59,  85 }, {   1,  35,  54 }, {   1,  13,  20 },
    //Band 3
    {  40, 142, 167 }, {  17, 110, 157 }, {   2,  71, 112 },
    {   1,  44,  72 }, {   1,  27,  45 }, {   1,  11,  17 },
    // Band 4
    {  30, 175, 188 }, {   9, 124, 169 }, {   1,  74, 116 },
    {   1,  48,  78 }, {   1,  30,  49 }, {   1,  11,  18 },
    // Band 5
    {  10, 222, 223 }, {   2, 150, 194 }, {   1,  83, 128 },
    {   1,  48,  79 }, {   1,  27,  45 }, {   1,  11,  17 },

    // Y plane - Inter
    // Band 0
    {  36,  41, 235 }, {  29,  36, 193 }, {  10,  27, 111 },
    // Band 1
    {  85, 165, 222 }, { 177, 162, 215 }, { 110, 135, 195 },
    {  57, 113, 168 }, {  23,  83, 120 }, {  10,  49,  61 },
    // Band 2
    {  85, 190, 223 }, {  36, 139, 200 }, {   5,  90, 146 },
    {   1,  60, 103 }, {   1,  38,  65 }, {   1,  18,  30 },
    // Band 3
    {  72, 202, 223 }, {  23, 141, 199 }, {   2,  86, 140 },
    {   1,  56,  97 }, {   1,  36,  61 }, {   1,  16,  27 },
    // Band 4
    {  55, 218, 225 }, {  13, 145, 200 }, {   1,  86, 141 },
    {   1,  57,  99 }, {   1,  35,  61 }, {   1,  13,  22 },
    // Band 5
    {  15, 235, 212 }, {   1, 132, 184 }, {   1,  84, 139 },
    {   1,  57,  97 }, {   1,  34,  56 }, {   1,  14,  23 },

    // UV plane - Intra
    // Band 0
    { 181,  21, 201 }, {  61,  37, 123 }, {  10,  38,  71 },
    // Band 1
    {  47, 106, 172 }, {  95, 104, 173 }, {  42,  93, 159 },
    {  18,  77, 131 }, {   4,  50,  81 }, {   1,  17,  23 },
    // Band 2
    {  62, 147, 199 }, {  44, 130, 189 }, {  28, 102, 154 },
    {  18,  75, 115 }, {   2,  44,  65 }, {   1,  12,  19 },
    // Band 3
    {  55, 153, 210 }, {  24, 130, 194 }, {   3,  93, 146 },
    {   1,  61,  97 }, {   1,  31,  50 }, {   1,  10,  16 },
    // Band 4
    {  49, 186, 223 }, {  17, 148, 204 }, {   1,  96, 142 },
    {   1,  53,  83 }, {   1,  26,  44 }, {   1,  11,  17 },
    // Band 5
    {  13, 217, 212 }, {   2, 136, 180 }, {   1,  78, 124 },
    {   1,  50,  83 }, {   1,  29,  49 }, {   1,  14,  23 },

    // UV plane - Inter
    // Band 0
    { 197,  13, 247 }, {  82,  17, 222 }, {  25,  17, 162 },
    // Band 1
    { 126, 186, 247 }, { 234, 191, 243 }, { 176, 177, 234 },
    { 104, 158, 220 }, {  66, 128, 186 }, {  55,  90, 137 },
    // Band 2
    { 111, 197, 242 }, {  46, 158, 219 }, {   9, 104, 171 },
    {   2,  65, 125 }, {   1,  44,  80 }, {   1,  17,  91 },
    // Band 3
    { 104, 208, 245 }, {  39, 168, 224 }, {   3, 109, 162 },
    {   1,  79, 124 }, {   1,  50, 102 }, {   1,  43, 102 },
    // Band 4
    {  84, 220, 246 }, {  31, 177, 231 }, {   2, 115, 180 },
    {   1,  79, 134 }, {   1,  55,  77 }, {   1,  60,  79 },
    // Band 5
    {  43, 243, 240 }, {   8, 180, 217 }, {   1, 115, 166 },
    {   1,  84, 121 }, {   1,  51,  67 }, {   1,  16,   6 }

};

#define FC_INTRA_OFFSET      0
#define FC_INTRA_SIZE        1664

#define FC_INTER_OFFSET      1664
#define FC_INTER_SIZE        384

#define FC_SKIP_OFFSET       1664
#define FC_SKIP_SIZE         3

void intel_init_default_vp9_probs(FRAME_CONTEXT *frame_context)
{
    if (!frame_context)
        return;

    frame_context->tx_probs = default_tx_probs;
    //dummy 52
    memcpy(frame_context->coeff_probs4x4, default_coef_probs_4x4,
           sizeof(default_coef_probs_4x4));
    memcpy(frame_context->coeff_probs8x8, default_coef_probs_8x8,
           sizeof(default_coef_probs_8x8));
    memcpy(frame_context->coeff_probs16x16, default_coef_probs_16x16,
           sizeof(default_coef_probs_16x16));
    memcpy(frame_context->coeff_probs32x32, default_coef_probs_32x32,
           sizeof(default_coef_probs_16x16));
    //dummy 16
    memcpy(frame_context->skip_probs, default_skip_probs,
           sizeof(default_skip_probs));
    memcpy(frame_context->inter_mode_probs, default_inter_mode_probs,
           sizeof(default_inter_mode_probs));
    memcpy(frame_context->switchable_interp_prob,
           default_switchable_interp_prob,
           sizeof(default_switchable_interp_prob));
    memcpy(frame_context->intra_inter_prob, default_intra_inter_p,
           sizeof(default_intra_inter_p));
    memcpy(frame_context->comp_inter_prob, default_comp_inter_p,
           sizeof(default_comp_inter_p));
    memcpy(frame_context->single_ref_prob, default_single_ref_p,
           sizeof(default_single_ref_p));
    memcpy(frame_context->comp_ref_prob, default_comp_ref_p,
           sizeof(default_comp_ref_p));
    memcpy(frame_context->y_mode_prob, default_if_y_probs,
           sizeof(default_if_y_probs));
    memcpy(frame_context->partition_prob, default_partition_probs,
           sizeof(default_partition_probs));
    frame_context->nmvc = default_nmv_context;
    //dummy 47
    memcpy(frame_context->uv_mode_prob, default_if_uv_probs,
           sizeof(default_if_uv_probs));
    memcpy(frame_context->seg_tree_probs, default_seg_tree_probs,
           sizeof(default_seg_tree_probs));
    memcpy(frame_context->seg_pred_probs, default_seg_pred_probs,
           sizeof(default_seg_pred_probs));
    return;
}


void intel_vp9_copy_frame_context(FRAME_CONTEXT *dst,
                                  FRAME_CONTEXT *src,
                                  bool inter_flag)
{
    unsigned char *fc_dst, *fc_src;

    if (!dst || !src)
        return;

    fc_dst = (unsigned char *)dst;
    fc_src = (unsigned char *)src;
    /* update the prob related with tx_mode/tx_coeff */
    memcpy(fc_dst + FC_INTRA_OFFSET, fc_src + FC_INTRA_OFFSET, FC_INTRA_SIZE);

    if (inter_flag) {
        /* update the prob related with inter-frame */
        memcpy(fc_dst + FC_INTER_OFFSET, fc_src + FC_INTER_OFFSET, FC_INTER_SIZE);
    } else {
        memcpy(fc_dst + FC_SKIP_OFFSET, fc_src + FC_SKIP_OFFSET, FC_SKIP_SIZE);
    }
}

void intel_update_intra_frame_context(FRAME_CONTEXT *frame_context)
{
    if (!frame_context)
        return;

    memcpy(frame_context->partition_prob, vp9_kf_partition_probs,
           sizeof(vp9_kf_partition_probs));

    memcpy(frame_context->uv_mode_prob, vp9_kf_uv_mode_prob,
           sizeof(vp9_kf_uv_mode_prob));

}

struct vp9_write_bit_buffer {
    uint8_t *bit_buffer;
    int bit_offset;
};

static
void vp9_wb_write_bit(struct vp9_write_bit_buffer *wb, int bit)
{
    const int off = wb->bit_offset;
    const int p = off / 8;
    const int q = 7 - off % 8;
    if (q == 7) {
        wb->bit_buffer[p] = bit << q;
    } else {
        wb->bit_buffer[p] &= ~(1 << q);
        wb->bit_buffer[p] |= bit << q;
    }
    wb->bit_offset = off + 1;
}

static
void vp9_wb_write_literal(struct vp9_write_bit_buffer *wb, int data, int bits)
{
    int bit;
    for (bit = bits - 1; bit >= 0; bit--)
        vp9_wb_write_bit(wb, (data >> bit) & 1);
}

static
void write_bitdepth_colorspace_sampling(int codec_profile,
                                        struct vp9_write_bit_buffer *wb)
{
    int profile = VAProfileVP9Profile0;
    profile = profile + 0;

    if (codec_profile >= (VAProfileVP9Profile0 + 2)) {
        /* the bit-depth will be added for VP9Profile2/3 */
        /* this will be added later */
    }

    /* Add the default color-space */
    vp9_wb_write_literal(wb, 0, 3);
    vp9_wb_write_bit(wb, 0);  // 0: [16, 235] (i.e. xvYCC), 1: [0, 255]

    /* the sampling_x/y will be added for VP9Profile1/2/3 later */
}

#define    MAX_TILE_WIDTH_B64    64
#define    MIN_TILE_WIDTH_B64    4

static int get_min_log2_tile_cols(const int sb_cols)
{
    int min_log2 = 0;

    while ((MAX_TILE_WIDTH_B64 << min_log2) < sb_cols)
        ++min_log2;

    return min_log2;
}

static int get_max_log2_tile_cols(const int sb_cols)
{
    int max_log2 = 1;

    while ((sb_cols >> max_log2) >= MIN_TILE_WIDTH_B64)
        ++max_log2;

    return max_log2 - 1;
}


bool intel_write_uncompressed_header(struct encode_state *encode_state,
                                     int codec_profile,
                                     char *header_data,
                                     int *header_length,
                                     vp9_header_bitoffset *header_bitoffset)
{
#define    VP9_SYNC_CODE_0    0x49
#define    VP9_SYNC_CODE_1    0x83
#define    VP9_SYNC_CODE_2    0x42

#define    VP9_FRAME_MARKER   0x2

#define    REFS_PER_FRAME     3

#define    REF_FRAMES_LOG2    3
#define    REF_FRAMES         (1 << REF_FRAMES_LOG2)

#define    VP9_KEY_FRAME      0

    VAEncPictureParameterBufferVP9 *pic_param;
    VAEncMiscParameterTypeVP9PerSegmantParam *seg_param = NULL;
    struct vp9_write_bit_buffer *wb, vp9_wb;

    if (!encode_state->pic_param_ext || !encode_state->pic_param_ext->buffer)
        return false;

    if (!header_data || !header_bitoffset || !header_length)
        return false;

    memset(header_bitoffset, 0, sizeof(vp9_header_bitoffset));

    pic_param = (VAEncPictureParameterBufferVP9 *)encode_state->pic_param_ext->buffer;

    if (encode_state->q_matrix)
        seg_param = (VAEncMiscParameterTypeVP9PerSegmantParam *) encode_state->q_matrix->buffer;

    vp9_wb.bit_buffer = (uint8_t *)header_data;
    vp9_wb.bit_offset = 0;
    wb = &vp9_wb;
    vp9_wb_write_literal(wb, VP9_FRAME_MARKER, 2);

    if (codec_profile == VAProfileVP9Profile0) {
        vp9_wb_write_literal(wb, 0, 2);
    } else {
        /* Other VP9Profile1/2/3 will be added later */
    }

    vp9_wb_write_bit(wb, 0);  // show_existing_frame
    vp9_wb_write_bit(wb, pic_param->pic_flags.bits.frame_type);
    vp9_wb_write_bit(wb, pic_param->pic_flags.bits.show_frame);
    vp9_wb_write_bit(wb, pic_param->pic_flags.bits.error_resilient_mode);

    if (pic_param->pic_flags.bits.frame_type == VP9_KEY_FRAME) {
        vp9_wb_write_literal(wb, VP9_SYNC_CODE_0, 8);
        vp9_wb_write_literal(wb, VP9_SYNC_CODE_1, 8);
        vp9_wb_write_literal(wb, VP9_SYNC_CODE_2, 8);

        write_bitdepth_colorspace_sampling(codec_profile, wb);

        /* write the encoded frame size */
        vp9_wb_write_literal(wb, pic_param->frame_width_dst - 1, 16);
        vp9_wb_write_literal(wb, pic_param->frame_height_dst - 1, 16);
        /* write display size */
        if ((pic_param->frame_width_dst != pic_param->frame_width_src) ||
            (pic_param->frame_height_dst != pic_param->frame_height_src)) {
            vp9_wb_write_bit(wb, 1);
            vp9_wb_write_literal(wb, pic_param->frame_width_src - 1, 16);
            vp9_wb_write_literal(wb, pic_param->frame_height_src - 1, 16);
        } else
            vp9_wb_write_bit(wb, 0);
    } else {
        /* for the non-Key frame */
        if (!pic_param->pic_flags.bits.show_frame)
            vp9_wb_write_bit(wb, pic_param->pic_flags.bits.intra_only);

        if (!pic_param->pic_flags.bits.error_resilient_mode)
            vp9_wb_write_literal(wb, pic_param->pic_flags.bits.reset_frame_context, 2);

        if (pic_param->pic_flags.bits.intra_only) {
            vp9_wb_write_literal(wb, VP9_SYNC_CODE_0, 8);
            vp9_wb_write_literal(wb, VP9_SYNC_CODE_1, 8);
            vp9_wb_write_literal(wb, VP9_SYNC_CODE_2, 8);

            /* Add the bit_depth for VP9Profile1/2/3 */
            /* write the refreshed_frame_flags */
            vp9_wb_write_literal(wb, pic_param->refresh_frame_flags, REF_FRAMES);
            /* write the encoded frame size */
            vp9_wb_write_literal(wb, pic_param->frame_width_dst - 1, 16);
            vp9_wb_write_literal(wb, pic_param->frame_height_dst - 1, 16);
            /* write display size */
            if ((pic_param->frame_width_dst != pic_param->frame_width_src) ||
                (pic_param->frame_height_dst != pic_param->frame_height_src)) {
                vp9_wb_write_bit(wb, 1);
                vp9_wb_write_literal(wb, pic_param->frame_width_src - 1, 16);
                vp9_wb_write_literal(wb, pic_param->frame_height_src - 1, 16);
            } else
                vp9_wb_write_bit(wb, 0);

        } else {
            /* The refresh_frame_map is  for the next frame so that it can select Last/Godlen/Alt ref_index */
            /*
            if ((pic_param->ref_flags.bits.ref_frame_ctrl_l0) & (1 << 0))
                refresh_flags = 1 << pic_param->ref_flags.bits.ref_last_idx;
            if ((pic_param->ref_flags.bits.ref_frame_ctrl_l0) & (1 << 0))
                refresh_flags = 1 << pic_param->ref_flags.bits.ref_last_idx;
            if ((pic_param->ref_flags.bits.ref_frame_ctrl_l0) & (1 << 0))
                refresh_flags = 1 << pic_param->ref_flags.bits.ref_last_idx;
            */
            vp9_wb_write_literal(wb, pic_param->refresh_frame_flags, REF_FRAMES);

            vp9_wb_write_literal(wb, pic_param->ref_flags.bits.ref_last_idx, REF_FRAMES_LOG2);
            vp9_wb_write_bit(wb, pic_param->ref_flags.bits.ref_last_sign_bias);
            vp9_wb_write_literal(wb, pic_param->ref_flags.bits.ref_gf_idx, REF_FRAMES_LOG2);
            vp9_wb_write_bit(wb, pic_param->ref_flags.bits.ref_gf_sign_bias);
            vp9_wb_write_literal(wb, pic_param->ref_flags.bits.ref_arf_idx, REF_FRAMES_LOG2);
            vp9_wb_write_bit(wb, pic_param->ref_flags.bits.ref_arf_sign_bias);

            /* write three bits with zero so that it can parse width/height directly */
            vp9_wb_write_literal(wb, 0, 3);
            vp9_wb_write_literal(wb, pic_param->frame_width_dst - 1, 16);
            vp9_wb_write_literal(wb, pic_param->frame_height_dst - 1, 16);

            /* write display size */
            if ((pic_param->frame_width_dst != pic_param->frame_width_src) ||
                (pic_param->frame_height_dst != pic_param->frame_height_src)) {

                vp9_wb_write_bit(wb, 1);
                vp9_wb_write_literal(wb, pic_param->frame_width_src - 1, 16);
                vp9_wb_write_literal(wb, pic_param->frame_height_src - 1, 16);
            } else
                vp9_wb_write_bit(wb, 0);

            vp9_wb_write_bit(wb, pic_param->pic_flags.bits.allow_high_precision_mv);

#define    SWITCHABLE_FILTER    4
#define    FILTER_MASK          3

            if (pic_param->pic_flags.bits.mcomp_filter_type == SWITCHABLE_FILTER)
                vp9_wb_write_bit(wb, 1);
            else {
                const int filter_to_literal[4] = { 1, 0, 2, 3 };
                uint8_t filter_flag = pic_param->pic_flags.bits.mcomp_filter_type;
                filter_flag = filter_flag & FILTER_MASK;
                vp9_wb_write_bit(wb, 0);
                vp9_wb_write_literal(wb, filter_to_literal[filter_flag], 2);
            }
        }
    }

    /* write refresh_frame_context/paralle frame_decoding */
    if (!pic_param->pic_flags.bits.error_resilient_mode) {
        vp9_wb_write_bit(wb, pic_param->pic_flags.bits.refresh_frame_context);
        vp9_wb_write_bit(wb, pic_param->pic_flags.bits.frame_parallel_decoding_mode);
    }

    vp9_wb_write_literal(wb, pic_param->pic_flags.bits.frame_context_idx, 2);

    /* write loop filter */
    header_bitoffset->bit_offset_lf_level = wb->bit_offset;
    vp9_wb_write_literal(wb, pic_param->filter_level, 6);
    vp9_wb_write_literal(wb, pic_param->sharpness_level, 3);

    {
        int i, mode_flag;

        vp9_wb_write_bit(wb, 1);
        vp9_wb_write_bit(wb, 1);
        header_bitoffset->bit_offset_ref_lf_delta = wb->bit_offset;
        for (i = 0; i < 4; i++) {
            /*
             * This check is skipped to prepare the bit_offset_lf_ref
            if (pic_param->ref_lf_delta[i] == 0) {
                vp9_wb_write_bit(wb, 0);
                continue;
            }
             */

            vp9_wb_write_bit(wb, 1);
            mode_flag = pic_param->ref_lf_delta[i];
            if (mode_flag >= 0) {
                vp9_wb_write_literal(wb, mode_flag & (0x3F), 6);
                vp9_wb_write_bit(wb, 0);
            } else {
                mode_flag = -mode_flag;
                vp9_wb_write_literal(wb, mode_flag & (0x3F), 6);
                vp9_wb_write_bit(wb, 1);
            }
        }

        header_bitoffset->bit_offset_mode_lf_delta = wb->bit_offset;
        for (i = 0; i < 2; i++) {
            /*
             * This check is skipped to prepare the bit_offset_lf_ref
            if (pic_param->mode_lf_delta[i] == 0) {
                vp9_wb_write_bit(wb, 0);
                continue;
            }
             */
            vp9_wb_write_bit(wb, 1);
            mode_flag = pic_param->mode_lf_delta[i];
            if (mode_flag >= 0) {
                vp9_wb_write_literal(wb, mode_flag & (0x3F), 6);
                vp9_wb_write_bit(wb, 0);
            } else {
                mode_flag = -mode_flag;
                vp9_wb_write_literal(wb, mode_flag & (0x3F), 6);
                vp9_wb_write_bit(wb, 1);
            }
        }
    }

    /* write basic quantizer */
    header_bitoffset->bit_offset_qindex = wb->bit_offset;
    vp9_wb_write_literal(wb, pic_param->luma_ac_qindex, 8);
    if (pic_param->luma_dc_qindex_delta) {
        int delta_q = pic_param->luma_dc_qindex_delta;
        vp9_wb_write_bit(wb, 1);
        vp9_wb_write_literal(wb, abs(delta_q), 4);
        vp9_wb_write_bit(wb, delta_q < 0);
    } else
        vp9_wb_write_bit(wb, 0);

    if (pic_param->chroma_dc_qindex_delta) {
        int delta_q = pic_param->chroma_dc_qindex_delta;
        vp9_wb_write_bit(wb, 1);
        vp9_wb_write_literal(wb, abs(delta_q), 4);
        vp9_wb_write_bit(wb, delta_q < 0);
    } else
        vp9_wb_write_bit(wb, 0);

    if (pic_param->chroma_ac_qindex_delta) {
        int delta_q = pic_param->chroma_ac_qindex_delta;
        vp9_wb_write_bit(wb, 1);
        vp9_wb_write_literal(wb, abs(delta_q), 4);
        vp9_wb_write_bit(wb, delta_q < 0);
    } else
        vp9_wb_write_bit(wb, 0);

    vp9_wb_write_bit(wb, pic_param->pic_flags.bits.segmentation_enabled);
    if (pic_param->pic_flags.bits.segmentation_enabled) {
        int i;

#define VP9_MAX_PROB    255
        vp9_wb_write_bit(wb, pic_param->pic_flags.bits.segmentation_update_map);
        if (pic_param->pic_flags.bits.segmentation_update_map) {

            header_bitoffset->bit_offset_segmentation = wb->bit_offset;
            /* write the seg_tree_probs */
            /* segment_tree_probs/segment_pred_probs are not passed.
             * So the hard-coded prob is writen
             */
            for (i = 0; i < 7; i++) {
                vp9_wb_write_bit(wb, 1);
                vp9_wb_write_literal(wb, VP9_MAX_PROB, 8);
            }

            vp9_wb_write_bit(wb, pic_param->pic_flags.bits.segmentation_temporal_update);
            if (pic_param->pic_flags.bits.segmentation_temporal_update) {
                for (i = 0; i < 3; i++) {
                    vp9_wb_write_bit(wb, 1);
                    vp9_wb_write_literal(wb, VP9_MAX_PROB, 8);
                }
            }
        }

        /* write the segment_data info */
        if (seg_param == NULL) {
            vp9_wb_write_bit(wb, 0);
        } else {
            VAEncSegParamVP9 *seg_data;
            int seg_delta;

            /* update_data */
            vp9_wb_write_bit(wb, 1);
            /* abs_delta should be zero */
            vp9_wb_write_bit(wb, 0);
            for (i = 0; i < 8; i++) {
                seg_data = &seg_param->seg_data[i];

                /* The segment qindex delta */
                /* This check is skipped */
                /* if (seg_data->segment_qindex_delta != 0) */
                if (1) {
                    vp9_wb_write_bit(wb, 1);
                    seg_delta = seg_data->segment_qindex_delta;
                    vp9_wb_write_literal(wb, abs(seg_delta), 8);
                    vp9_wb_write_bit(wb, seg_delta < 0);
                } else
                    vp9_wb_write_bit(wb, 0);

                /* The segment lf delta */
                /* if (seg_data->segment_lf_level_delta != 0) */
                if (1) {
                    vp9_wb_write_bit(wb, 1);
                    seg_delta = seg_data->segment_lf_level_delta;
                    vp9_wb_write_literal(wb, abs(seg_delta), 6);
                    vp9_wb_write_bit(wb, seg_delta < 0);
                } else
                    vp9_wb_write_bit(wb, 0);

                /* segment reference flag */
                vp9_wb_write_bit(wb, seg_data->seg_flags.bits.segment_reference_enabled);
                if (seg_data->seg_flags.bits.segment_reference_enabled) {
                    vp9_wb_write_literal(wb, seg_data->seg_flags.bits.segment_reference, 2);
                }

                /* segment skip flag */
                vp9_wb_write_bit(wb, seg_data->seg_flags.bits.segment_reference_skipped);
            }
        }
    }

    /* write tile info */
    {
        int sb_cols = (pic_param->frame_width_dst + 63) / 64;
        int min_log2_tile_cols, max_log2_tile_cols;
        int col_data;

        /* write tile column info */
        min_log2_tile_cols = get_min_log2_tile_cols(sb_cols);
        max_log2_tile_cols = get_max_log2_tile_cols(sb_cols);

        col_data = pic_param->log2_tile_columns - min_log2_tile_cols;
        while (col_data--) {
            vp9_wb_write_bit(wb, 1);
        }
        if (pic_param->log2_tile_columns < max_log2_tile_cols)
            vp9_wb_write_bit(wb, 0);

        /* write tile row info */
        vp9_wb_write_bit(wb, pic_param->log2_tile_rows);
        if (pic_param->log2_tile_rows)
            vp9_wb_write_bit(wb, (pic_param->log2_tile_rows != 1));
    }

    /* get the bit_offset of the first partition size */
    header_bitoffset->bit_offset_first_partition_size = wb->bit_offset;

    /* reserve the space for writing the first partitions ize */
    vp9_wb_write_literal(wb, 0, 16);

    *header_length = (wb->bit_offset + 7) / 8;

    return true;
}

const unsigned short vp9_quant_dc[256] = {
    4,       8,    8,    9,   10,   11,   12,   12,
    13,     14,   15,   16,   17,   18,   19,   19,
    20,     21,   22,   23,   24,   25,   26,   26,
    27,     28,   29,   30,   31,   32,   32,   33,
    34,     35,   36,   37,   38,   38,   39,   40,
    41,     42,   43,   43,   44,   45,   46,   47,
    48,     48,   49,   50,   51,   52,   53,   53,
    54,     55,   56,   57,   57,   58,   59,   60,
    61,     62,   62,   63,   64,   65,   66,   66,
    67,     68,   69,   70,   70,   71,   72,   73,
    74,     74,   75,   76,   77,   78,   78,   79,
    80,     81,   81,   82,   83,   84,   85,   85,
    87,     88,   90,   92,   93,   95,   96,   98,
    99,    101,  102,  104,  105,  107,  108,  110,
    111,   113,  114,  116,  117,  118,  120,  121,
    123,   125,  127,  129,  131,  134,  136,  138,
    140,   142,  144,  146,  148,  150,  152,  154,
    156,   158,  161,  164,  166,  169,  172,  174,
    177,   180,  182,  185,  187,  190,  192,  195,
    199,   202,  205,  208,  211,  214,  217,  220,
    223,   226,  230,  233,  237,  240,  243,  247,
    250,   253,  257,  261,  265,  269,  272,  276,
    280,   284,  288,  292,  296,  300,  304,  309,
    313,   317,  322,  326,  330,  335,  340,  344,
    349,   354,  359,  364,  369,  374,  379,  384,
    389,   395,  400,  406,  411,  417,  423,  429,
    435,   441,  447,  454,  461,  467,  475,  482,
    489,   497,  505,  513,  522,  530,  539,  549,
    559,   569,  579,  590,  602,  614,  626,  640,
    654,   668,  684,  700,  717,  736,  755,  775,
    796,   819,  843,  869,  896,  925,  955,  988,
    1022, 1058, 1098, 1139, 1184, 1232, 1282, 1336,
};

const unsigned short vp9_quant_ac[256] = {
    4,       8,    9,   10,   11,   12,   13,   14,
    15,     16,   17,   18,   19,   20,   21,   22,
    23,     24,   25,   26,   27,   28,   29,   30,
    31,     32,   33,   34,   35,   36,   37,   38,
    39,     40,   41,   42,   43,   44,   45,   46,
    47,     48,   49,   50,   51,   52,   53,   54,
    55,     56,   57,   58,   59,   60,   61,   62,
    63,     64,   65,   66,   67,   68,   69,   70,
    71,     72,   73,   74,   75,   76,   77,   78,
    79,     80,   81,   82,   83,   84,   85,   86,
    87,     88,   89,   90,   91,   92,   93,   94,
    95,     96,   97,   98,   99,  100,  101,  102,
    104,   106,  108,  110,  112,  114,  116,  118,
    120,   122,  124,  126,  128,  130,  132,  134,
    136,   138,  140,  142,  144,  146,  148,  150,
    152,   155,  158,  161,  164,  167,  170,  173,
    176,   179,  182,  185,  188,  191,  194,  197,
    200,   203,  207,  211,  215,  219,  223,  227,
    231,   235,  239,  243,  247,  251,  255,  260,
    265,   270,  275,  280,  285,  290,  295,  300,
    305,   311,  317,  323,  329,  335,  341,  347,
    353,   359,  366,  373,  380,  387,  394,  401,
    408,   416,  424,  432,  440,  448,  456,  465,
    474,   483,  492,  501,  510,  520,  530,  540,
    550,   560,  571,  582,  593,  604,  615,  627,
    639,   651,  663,  676,  689,  702,  715,  729,
    743,   757,  771,  786,  801,  816,  832,  848,
    864,   881,  898,  915,  933,  951,  969,  988,
    1007, 1026, 1046, 1066, 1087, 1108, 1129, 1151,
    1173, 1196, 1219, 1243, 1267, 1292, 1317, 1343,
    1369, 1396, 1423, 1451, 1479, 1508, 1537, 1567,
    1597, 1628, 1660, 1692, 1725, 1759, 1793, 1828,
};
