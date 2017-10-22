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
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Xiang, Haihao <haihao.xiang@intel.com>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "intel_batchbuffer.h"
#include "intel_driver.h"

#include "i965_defines.h"
#include "i965_drv_video.h"
#include "i965_encoder.h"
#include "i965_encoder_vp8.h"

extern struct i965_kernel vp8_kernels_brc_init_reset[NUM_VP8_BRC_RESET];
extern struct i965_kernel vp8_kernels_scaling[NUM_VP8_SCALING];
extern struct i965_kernel vp8_kernels_me[NUM_VP8_ME];
extern struct i965_kernel vp8_kernels_mbenc[NUM_VP8_MBENC];
extern struct i965_kernel vp8_kernels_mpu[NUM_VP8_MPU];
extern struct i965_kernel vp8_kernels_tpu[NUM_VP8_TPU];
extern struct i965_kernel vp8_kernels_brc_update[NUM_VP8_BRC_UPDATE];

static const uint32_t gen8_brc_init_bin_vp8[][4] = {
#include "shaders/brc/bsw/vp8_brc_init_genx_0.g8b"
};

static const uint32_t gen8_brc_reset_bin_vp8[][4] = {
#include "shaders/brc/bsw/vp8_brc_reset_genx_0.g8b"
};

static const uint32_t gen8_scaling_bin_vp8[][4] = {
#include "shaders/brc/bsw/hme_downscale_genx_0.g8b"
};

static const uint32_t gen8_me_bin_vp8[][4] = {
#include "shaders/brc/bsw/hme_genx_0.g8b"
};

static const uint32_t gen8_mbenc_i_frame_dist_bin_vp8[][4] = {
#include "shaders/brc/bsw/vp8_intra_distortion_genx_0.g8b"
};

static const uint32_t gen8_mbenc_i_frame_luma_bin_vp8[][4] = {
#include "shaders/brc/bsw/vp8_enc_genx_0.g8b"
};

static const uint32_t gen8_mbenc_i_frame_chroma_bin_vp8[][4] = {
#include "shaders/brc/bsw/vp8_enc_genx_1.g8b"
};

static const uint32_t gen8_mbenc_p_frame_bin_vp8[][4] = {
#include "shaders/brc/bsw/vp8_enc_genx_2.g8b"
};

static const uint32_t gen8_mpu_bin_vp8[][4] = {
#include "shaders/brc/bsw/vp8_mpu_genx_0.g8b"
};

static const uint32_t gen8_tpu_bin_vp8[][4] = {
#include "shaders/brc/bsw/vp8_tpu_genx_0.g8b"
};

static const uint32_t gen8_brc_update_bin_vp8[][4] = {
#include "shaders/brc/bsw/vp8_brc_update_genx_0.g8b"
};

Bool
gen8_encoder_vp8_context_init(VADriverContextP ctx,
                              struct intel_encoder_context *encoder_context,
                              struct i965_encoder_vp8_context *vp8_context)
{
    vp8_kernels_brc_init_reset[VP8_BRC_INIT].bin = gen8_brc_init_bin_vp8;
    vp8_kernels_brc_init_reset[VP8_BRC_INIT].size = sizeof(gen8_brc_init_bin_vp8);
    vp8_kernels_brc_init_reset[VP8_BRC_RESET].bin = gen8_brc_reset_bin_vp8;
    vp8_kernels_brc_init_reset[VP8_BRC_RESET].size = sizeof(gen8_brc_reset_bin_vp8);

    /* scaling 4x and 16x use the same kernel */
    vp8_kernels_scaling[VP8_SCALING_4X].bin = gen8_scaling_bin_vp8;
    vp8_kernels_scaling[VP8_SCALING_4X].size = sizeof(gen8_scaling_bin_vp8);
    vp8_kernels_scaling[VP8_SCALING_16X].bin = gen8_scaling_bin_vp8;
    vp8_kernels_scaling[VP8_SCALING_16X].size = sizeof(gen8_scaling_bin_vp8);

    /* me 4x and 16x use the same kernel */
    vp8_kernels_me[VP8_ME_4X].bin = gen8_me_bin_vp8;
    vp8_kernels_me[VP8_ME_4X].size = sizeof(gen8_me_bin_vp8);
    vp8_kernels_me[VP8_ME_16X].bin = gen8_me_bin_vp8;
    vp8_kernels_me[VP8_ME_16X].size = sizeof(gen8_me_bin_vp8);

    vp8_kernels_mbenc[VP8_MBENC_I_FRAME_DIST].bin = gen8_mbenc_i_frame_dist_bin_vp8;
    vp8_kernels_mbenc[VP8_MBENC_I_FRAME_DIST].size = sizeof(gen8_mbenc_i_frame_dist_bin_vp8);
    vp8_kernels_mbenc[VP8_MBENC_I_FRAME_LUMA].bin = gen8_mbenc_i_frame_luma_bin_vp8;
    vp8_kernels_mbenc[VP8_MBENC_I_FRAME_LUMA].size = sizeof(gen8_mbenc_i_frame_luma_bin_vp8);
    vp8_kernels_mbenc[VP8_MBENC_I_FRAME_CHROMA].bin = gen8_mbenc_i_frame_chroma_bin_vp8;
    vp8_kernels_mbenc[VP8_MBENC_I_FRAME_CHROMA].size = sizeof(gen8_mbenc_i_frame_chroma_bin_vp8);
    vp8_kernels_mbenc[VP8_MBENC_P_FRAME].bin = gen8_mbenc_p_frame_bin_vp8;
    vp8_kernels_mbenc[VP8_MBENC_P_FRAME].size = sizeof(gen8_mbenc_p_frame_bin_vp8);

    vp8_kernels_mpu[VP8_MPU].bin = gen8_mpu_bin_vp8;
    vp8_kernels_mpu[VP8_MPU].size = sizeof(gen8_mpu_bin_vp8);

    vp8_kernels_brc_update[VP8_BRC_UPDATE].bin = gen8_brc_update_bin_vp8;
    vp8_kernels_brc_update[VP8_BRC_UPDATE].size = sizeof(gen8_brc_update_bin_vp8);

    vp8_kernels_tpu[VP8_TPU].bin = gen8_tpu_bin_vp8;
    vp8_kernels_tpu[VP8_TPU].size = sizeof(gen8_tpu_bin_vp8);

    vp8_context->idrt_entry_size = ALIGN(sizeof(struct gen8_interface_descriptor_data), 64);
    vp8_context->mocs = 0;

    return True;
}
