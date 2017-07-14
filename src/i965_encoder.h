/*
 * Copyright © 2010 Intel Corporation
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
 *    Zhou chang <chang.zhou@intel.com>
 *
 */

#ifndef _I965_ENCODER_H_
#define _I965_ENCODER_H_

#include <drm.h>
#include <i915_drm.h>
#include <intel_bufmgr.h>

#include "i965_structs.h"
#include "i965_drv_video.h"

#define I965_BRC_NONE                   0
#define I965_BRC_CBR                    1
#define I965_BRC_VBR                    2
#define I965_BRC_CQP                    3

#define WIDTH_IN_MACROBLOCKS(width)     (ALIGN(width, 16) >> 4)
#define HEIGHT_IN_MACROBLOCKS(height)   (ALIGN(height, 16) >> 4)
#define MAX_TEMPORAL_LAYERS         4

struct intel_roi {
    short left;
    short right;
    short top;
    short bottom;

    char  value;
};

struct intel_fraction {
    unsigned int num;
    unsigned int den;
};

struct intel_encoder_context {
    struct hw_context base;
    VADriverContextP ctx;
    int codec;
    VASurfaceID input_yuv_surface;
    unsigned int rate_control_mode;
    unsigned int quality_level;
    unsigned int quality_range;
    unsigned int num_frames_in_sequence;
    unsigned int frame_width_in_pixel;
    unsigned int frame_height_in_pixel;
    unsigned int max_slice_or_seg_num;

    struct {
        unsigned int num_layers;
        unsigned int size_frame_layer_ids;
        unsigned int frame_layer_ids[32];
        unsigned int curr_frame_layer_id;
    } layer;

    struct {
        unsigned short gop_size;
        unsigned short num_iframes_in_gop;
        unsigned short num_pframes_in_gop;
        unsigned short num_bframes_in_gop;
        unsigned int bits_per_second[MAX_TEMPORAL_LAYERS];
        struct intel_fraction framerate[MAX_TEMPORAL_LAYERS];
        unsigned int mb_rate_control[MAX_TEMPORAL_LAYERS];
        unsigned int target_percentage[MAX_TEMPORAL_LAYERS];
        unsigned int hrd_buffer_size;
        unsigned int hrd_initial_buffer_fullness;
        unsigned int window_size;
        unsigned int initial_qp;
        unsigned int min_qp;
        unsigned int need_reset;

        unsigned int num_roi;
        unsigned int roi_max_delta_qp;
        unsigned int roi_min_delta_qp;
        unsigned int roi_value_is_qp_delta;
        struct intel_roi roi[I965_MAX_NUM_ROI_REGIONS];
    } brc;

    void *vme_context;
    void *mfc_context;
    void *enc_priv_state;

    unsigned int is_tmp_id: 1;
    unsigned int low_power_mode: 1;
    unsigned int soft_batch_force: 1;
    unsigned int context_roi: 1;
    unsigned int is_new_sequence: 1; /* Currently only valid for H.264, TODO for other codecs */

    void (*vme_context_destroy)(void *vme_context);
    VAStatus(*vme_pipeline)(VADriverContextP ctx,
                            VAProfile profile,
                            struct encode_state *encode_state,
                            struct intel_encoder_context *encoder_context);
    void (*mfc_context_destroy)(void *mfc_context);
    VAStatus(*mfc_pipeline)(VADriverContextP ctx,
                            VAProfile profile,
                            struct encode_state *encode_state,
                            struct intel_encoder_context *encoder_context);
    void (*mfc_brc_prepare)(struct encode_state *encode_state,
                            struct intel_encoder_context *encoder_context);

    VAStatus(*get_status)(VADriverContextP ctx,
                          struct intel_encoder_context *encoder_context,
                          struct i965_coded_buffer_segment *coded_buffer_segment);
};

extern struct hw_context *
gen75_enc_hw_context_init(VADriverContextP ctx, struct object_config *obj_config);

extern struct hw_context *
gen8_enc_hw_context_init(VADriverContextP ctx, struct object_config *obj_config);

extern struct hw_context *
gen9_enc_hw_context_init(VADriverContextP ctx, struct object_config *obj_config);
#endif  /* _I965_ENCODER_H_ */


