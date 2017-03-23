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

#ifndef VP9_PROBS_H
#define VP9_PROBS_H

#include <stdbool.h>

#define TX_SIZE_CONTEXTS 2
#define TX_SIZES 4
#define PLANE_TYPES 2
#define SKIP_CONTEXTS 3
#define INTER_MODE_CONTEXTS 7
#define INTER_MODES 4
#define SWITCHABLE_FILTERS 3
#define SWITCHABLE_FILTER_CONTEXTS (SWITCHABLE_FILTERS + 1)
#define MAX_SEGMENTS 8
#define PREDICTION_PROBS 3
#define SEG_TREE_PROBS (MAX_SEGMENTS-1)
#define MV_JOINTS 4
#define INTRA_INTER_CONTEXTS 4
#define COMP_INTER_CONTEXTS 5
#define REF_CONTEXTS 5
#define BLOCK_SIZE_GROUPS 4
#define INTRA_MODES 10
#define PARTITION_PLOFFSET   4  // number of probability models per block size
#define PARTITION_CONTEXTS (4 * PARTITION_PLOFFSET)
#define PARTITION_TYPES 4
#define REF_TYPES 2  // intra=0, inter=1
#define COEF_BANDS 6
#define COEFF_CONTEXTS 6
#define UNCONSTRAINED_NODES         3
#define MV_CLASSES 11
#define CLASS0_BITS    1  /* bits at integer precision for class 0 */
#define CLASS0_SIZE    (1 << CLASS0_BITS)
#define MV_OFFSET_BITS (MV_CLASSES + CLASS0_BITS - 2)
#define MV_MAX_BITS    (MV_CLASSES + CLASS0_BITS + 2)
#define MV_FP_SIZE 4
#define FRAME_CONTEXTS_LOG2 2
#define FRAME_CONTEXTS (1 << FRAME_CONTEXTS_LOG2)

#define COEFF_PROB_SIZE 132
#define COEFF_PROB_NUM 3

#define TX_PROBS_IDX 0
#define COEFF_PROBS_IDX 64
#define INTRA_PROBS_IDX 1603
#define SEG_PROBS_IDX 2010

typedef uint8_t vp9_prob;

#define vpx_memset  memset
#define vpx_memcpy  memcpy

#define vp9_zero(dest) memset(&dest, 0, sizeof(dest))

#define vp9_copy(dest, src) {            \
    assert(sizeof(dest) == sizeof(src)); \
    vpx_memcpy(dest, src, sizeof(src));  \
}

struct tx_probs {
    vp9_prob p8x8[TX_SIZE_CONTEXTS][TX_SIZES - 3];
    vp9_prob p16x16[TX_SIZE_CONTEXTS][TX_SIZES - 2];
    vp9_prob p32x32[TX_SIZE_CONTEXTS][TX_SIZES - 1];
};

struct tx_counts {
    unsigned int p32x32[TX_SIZE_CONTEXTS][TX_SIZES];
    unsigned int p16x16[TX_SIZE_CONTEXTS][TX_SIZES - 1];
    unsigned int p8x8[TX_SIZE_CONTEXTS][TX_SIZES - 2];
    unsigned int tx_totals[TX_SIZES];
};

typedef struct {
    vp9_prob sign;
    vp9_prob classes[MV_CLASSES - 1];
    vp9_prob class0[CLASS0_SIZE - 1];
    vp9_prob bits[MV_OFFSET_BITS];
} nmv_component;

//Modified the nmv_context from libvpx to suit our HW needs
typedef struct {
    vp9_prob joints[MV_JOINTS - 1];
    nmv_component comps[2];
    vp9_prob class0_fp0[CLASS0_SIZE][MV_FP_SIZE - 1];
    vp9_prob fp0[MV_FP_SIZE - 1];
    vp9_prob class0_fp1[CLASS0_SIZE][MV_FP_SIZE - 1];
    vp9_prob fp1[MV_FP_SIZE - 1];
    vp9_prob class0_hp[2];
    vp9_prob hp[2];
} nmv_context;

//Modified the FRAME_CONTEXT from libvpx to suit our HW needs
typedef struct frame_contexts {
    struct tx_probs tx_probs;
    vp9_prob dummy1[52];
    vp9_prob coeff_probs4x4[COEFF_PROB_SIZE][COEFF_PROB_NUM];
    vp9_prob coeff_probs8x8[COEFF_PROB_SIZE][COEFF_PROB_NUM];
    vp9_prob coeff_probs16x16[COEFF_PROB_SIZE][COEFF_PROB_NUM];
    vp9_prob coeff_probs32x32[COEFF_PROB_SIZE][COEFF_PROB_NUM];
    vp9_prob dummy2[16];
    vp9_prob skip_probs[SKIP_CONTEXTS];
    vp9_prob inter_mode_probs[INTER_MODE_CONTEXTS][INTER_MODES - 1];
    vp9_prob switchable_interp_prob[SWITCHABLE_FILTER_CONTEXTS]
    [SWITCHABLE_FILTERS - 1];
    vp9_prob intra_inter_prob[INTRA_INTER_CONTEXTS];
    vp9_prob comp_inter_prob[COMP_INTER_CONTEXTS];
    vp9_prob single_ref_prob[REF_CONTEXTS][2];
    vp9_prob comp_ref_prob[REF_CONTEXTS];
    vp9_prob y_mode_prob[BLOCK_SIZE_GROUPS][INTRA_MODES - 1];
    vp9_prob partition_prob[PARTITION_CONTEXTS][PARTITION_TYPES - 1];
    nmv_context nmvc;
    vp9_prob dummy3[47];
    vp9_prob uv_mode_prob[INTRA_MODES][INTRA_MODES - 1];
    vp9_prob seg_tree_probs[SEG_TREE_PROBS];
    vp9_prob seg_pred_probs[PREDICTION_PROBS];
    vp9_prob dummy4[28];
    int initialized;
} FRAME_CONTEXT;


extern struct tx_probs default_tx_probs;

extern vp9_prob default_skip_probs[SKIP_CONTEXTS];

extern vp9_prob default_inter_mode_probs[INTER_MODE_CONTEXTS]
[INTER_MODES - 1];

extern vp9_prob default_switchable_interp_prob[SWITCHABLE_FILTER_CONTEXTS]
[SWITCHABLE_FILTERS - 1];

extern vp9_prob default_intra_inter_p[INTRA_INTER_CONTEXTS];

extern vp9_prob default_comp_inter_p[COMP_INTER_CONTEXTS];

extern vp9_prob default_single_ref_p[REF_CONTEXTS][2];

extern vp9_prob default_comp_ref_p[REF_CONTEXTS];

extern vp9_prob vp9_kf_uv_mode_prob[INTRA_MODES][INTRA_MODES - 1];

extern vp9_prob default_if_y_probs[BLOCK_SIZE_GROUPS][INTRA_MODES - 1];

extern vp9_prob default_if_uv_probs[INTRA_MODES][INTRA_MODES - 1];

extern vp9_prob default_seg_tree_probs[SEG_TREE_PROBS];

extern vp9_prob default_seg_pred_probs[PREDICTION_PROBS];

extern vp9_prob vp9_kf_partition_probs[PARTITION_CONTEXTS]
[PARTITION_TYPES - 1];

extern vp9_prob default_partition_probs[PARTITION_CONTEXTS]
[PARTITION_TYPES - 1];

extern nmv_context default_nmv_context;

extern vp9_prob default_coef_probs_4x4[COEFF_PROB_SIZE][COEFF_PROB_NUM];

extern vp9_prob default_coef_probs_8x8[COEFF_PROB_SIZE][COEFF_PROB_NUM];

extern vp9_prob default_coef_probs_16x16[COEFF_PROB_SIZE][COEFF_PROB_NUM];

extern vp9_prob default_coef_probs_32x32[COEFF_PROB_SIZE][COEFF_PROB_NUM];

extern void intel_init_default_vp9_probs(FRAME_CONTEXT *frame_context);

extern void intel_vp9_copy_frame_context(FRAME_CONTEXT *dst,
                                         FRAME_CONTEXT *src,
                                         bool inter_flag);

extern void intel_update_intra_frame_context(FRAME_CONTEXT *frame_context);


typedef struct _vp9_header_bitoffset_ {
    unsigned int    bit_offset_ref_lf_delta;
    unsigned int    bit_offset_mode_lf_delta;
    unsigned int    bit_offset_lf_level;
    unsigned int    bit_offset_qindex;
    unsigned int    bit_offset_first_partition_size;
    unsigned int    bit_offset_segmentation;
    unsigned int    bit_size_segmentation;
} vp9_header_bitoffset;

struct encode_state;
extern bool intel_write_uncompressed_header(struct encode_state *encode_state,
                                            int codec_profile,
                                            char *header_data,
                                            int *header_length,
                                            vp9_header_bitoffset *header_bitoffset);

typedef enum {
    ONLY_4X4            = 0,        // only 4x4 transform used
    ALLOW_8X8           = 1,        // allow block transform size up to 8x8
    ALLOW_16X16         = 2,        // allow block transform size up to 16x16
    ALLOW_32X32         = 3,        // allow block transform size up to 32x32
    TX_MODE_SELECT      = 4,        // transform specified for each block
    TX_MODES            = 5,
} TX_MODE;

typedef enum {
    SINGLE_REFERENCE      = 0,
    COMPOUND_REFERENCE    = 1,
    REFERENCE_MODE_SELECT = 2,
    REFERENCE_MODES       = 3,
} REFERENCE_MODE;

extern const unsigned short vp9_quant_dc[256];
extern const unsigned short vp9_quant_ac[256];

#endif /*VP9_PROBS_H */
