/*
 * Copyright ? 2016 Intel Corporation
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
 * SOFTWAR
 *
 * Authors:
 *    Pengfei Qu <Pengfei.qu@intel.com>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <va/va.h>

#include "intel_batchbuffer.h"
#include "intel_driver.h"

#include "i965_defines.h"
#include "i965_structs.h"
#include "i965_drv_video.h"
#include "i965_encoder.h"
#include "i965_encoder_utils.h"
#include "intel_media.h"

#include "i965_gpe_utils.h"
#include "i965_encoder_common.h"
#include "i965_avc_encoder_common.h"
#include "gen9_avc_encoder_kernels.h"
#include "gen9_avc_encoder.h"
#include "gen9_avc_const_def.h"

#define MAX_URB_SIZE                    4096 /* In register */
#define NUM_KERNELS_PER_GPE_CONTEXT     1
#define MBENC_KERNEL_BASE GEN9_AVC_KERNEL_MBENC_QUALITY_I

#define OUT_BUFFER_2DW(batch, bo, is_target, delta)  do {               \
        if (bo) {                                                       \
            OUT_BCS_RELOC(batch,                                        \
                            bo,                                         \
                            I915_GEM_DOMAIN_INSTRUCTION,                \
                            is_target ? I915_GEM_DOMAIN_INSTRUCTION : 0,     \
                            delta);                                     \
            OUT_BCS_BATCH(batch, 0);                                    \
        } else {                                                        \
            OUT_BCS_BATCH(batch, 0);                                    \
            OUT_BCS_BATCH(batch, 0);                                    \
        }                                                               \
    } while (0)

#define OUT_BUFFER_3DW(batch, bo, is_target, delta, attr)  do { \
        OUT_BUFFER_2DW(batch, bo, is_target, delta);            \
        OUT_BCS_BATCH(batch, attr);                             \
    } while (0)


static const uint32_t qm_flat[16] = {
    0x10101010, 0x10101010, 0x10101010, 0x10101010,
    0x10101010, 0x10101010, 0x10101010, 0x10101010,
    0x10101010, 0x10101010, 0x10101010, 0x10101010,
    0x10101010, 0x10101010, 0x10101010, 0x10101010
};

static const uint32_t fqm_flat[32] = {
    0x10001000, 0x10001000, 0x10001000, 0x10001000,
    0x10001000, 0x10001000, 0x10001000, 0x10001000,
    0x10001000, 0x10001000, 0x10001000, 0x10001000,
    0x10001000, 0x10001000, 0x10001000, 0x10001000,
    0x10001000, 0x10001000, 0x10001000, 0x10001000,
    0x10001000, 0x10001000, 0x10001000, 0x10001000,
    0x10001000, 0x10001000, 0x10001000, 0x10001000,
    0x10001000, 0x10001000, 0x10001000, 0x10001000
};

static unsigned int slice_type_kernel[3] = {1,2,0};

const gen9_avc_brc_init_reset_curbe_data gen9_avc_brc_init_reset_curbe_init_data =
{
    // unsigned int 0
    {
            0
    },

    // unsigned int 1
    {
            0
    },

    // unsigned int 2
    {
            0
    },

    // unsigned int 3
    {
            0
    },

    // unsigned int 4
    {
            0
    },

    // unsigned int 5
    {
            0
    },

    // unsigned int 6
    {
            0
    },

    // unsigned int 7
    {
            0
    },

    // unsigned int 8
    {
            0,
            0
    },

    // unsigned int 9
    {
            0,
            0
    },

    // unsigned int 10
    {
            0,
            0
    },

    // unsigned int 11
    {
            0,
            1
    },

    // unsigned int 12
    {
            51,
            0
    },

    // unsigned int 13
    {
            40,
            60,
            80,
            120
    },

    // unsigned int 14
    {
            35,
            60,
            80,
            120
    },

    // unsigned int 15
    {
            40,
            60,
            90,
            115
    },

    // unsigned int 16
    {
            0,
            0,
            0,
            0
    },

    // unsigned int 17
    {
            0,
            0,
            0,
            0
    },

    // unsigned int 18
    {
            0,
            0,
            0,
            0
    },

    // unsigned int 19
    {
            0,
            0,
            0,
            0
    },

    // unsigned int 20
    {
            0,
            0,
            0,
            0
    },

    // unsigned int 21
    {
            0,
            0,
            0,
            0
    },

    // unsigned int 22
    {
            0,
            0,
            0,
            0
    },

    // unsigned int 23
    {
            0
    }
};

const gen9_avc_frame_brc_update_curbe_data gen9_avc_frame_brc_update_curbe_init_data =
{
    // unsigned int 0
    {
            0
    },

    // unsigned int 1
    {
            0
    },

    // unsigned int 2
    {
            0
    },

    // unsigned int 3
    {
            10,
            50
    },

    // unsigned int 4
    {
            100,
            150
    },

    // unsigned int 5
    {
            0,
            0,
            0,
            0
    },

    // unsigned int 6
    {
            0,
            0,
            0,
            0,
            0,
            0
    },

    // unsigned int 7
    {
            0
    },

    // unsigned int 8
    {
            1,
            1,
            3,
            2
    },

    // unsigned int 9
    {
            1,
            40,
            5,
            5
    },

    // unsigned int 10
    {
            3,
            1,
            7,
            18
    },

    // unsigned int 11
    {
            25,
            37,
            40,
            75
    },

    // unsigned int 12
    {
            97,
            103,
            125,
            160
    },

    // unsigned int 13
    {
            -3,
            -2,
            -1,
            0
    },

    // unsigned int 14
    {
            1,
            2,
            3,
            0xff
    },

    // unsigned int 15
    {
            0,
            0,
            0,
            0
    },

    // unsigned int 16
    {
            0
    },

    // unsigned int 17
    {
            0
    },

    // unsigned int 18
    {
            0
    },

    // unsigned int 19
    {
            0
    },

    // unsigned int 20
    {
            0
    },

    // unsigned int 21
    {
            0
    },

    // unsigned int 22
    {
            0
    },

    // unsigned int 23
    {
            0
    },

};

static void
gen9_avc_update_rate_control_parameters(VADriverContextP ctx,
                                        struct intel_encoder_context *encoder_context,
                                        VAEncMiscParameterRateControl *misc)
{
    struct encoder_vme_mfc_context * vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    struct generic_enc_codec_state * generic_state = (struct generic_enc_codec_state * )vme_context->generic_enc_state;

    generic_state->max_bit_rate = ALIGN(misc->bits_per_second, 1000) / 1000;
    generic_state->window_size = misc->window_size;

    if (generic_state->internal_rate_mode == INTEL_BRC_CBR) {
        generic_state->min_bit_rate = generic_state->max_bit_rate;
        generic_state->mb_brc_enabled = misc->rc_flags.bits.mb_rate_control;

        if (generic_state->target_bit_rate != generic_state->max_bit_rate) {
            generic_state->target_bit_rate = generic_state->max_bit_rate;
            generic_state->brc_need_reset = 1;
        }
    } else if (generic_state->internal_rate_mode == INTEL_BRC_VBR) {
        generic_state->min_bit_rate = generic_state->max_bit_rate * (2 * misc->target_percentage - 100) / 100;
        generic_state->mb_brc_enabled = misc->rc_flags.bits.mb_rate_control;

        if (generic_state->target_bit_rate != generic_state->max_bit_rate * misc->target_percentage / 100) {
            generic_state->target_bit_rate = generic_state->max_bit_rate * misc->target_percentage / 100;
            generic_state->brc_need_reset = 1;
        }
    }
}

static void
gen9_avc_update_hrd_parameters(VADriverContextP ctx,
                               struct intel_encoder_context *encoder_context,
                               VAEncMiscParameterHRD *misc)
{
    struct encoder_vme_mfc_context * vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    struct generic_enc_codec_state * generic_state = (struct generic_enc_codec_state * )vme_context->generic_enc_state;

    if (generic_state->internal_rate_mode == INTEL_BRC_CQP)
        return;

    generic_state->vbv_buffer_size_in_bit = misc->buffer_size;
    generic_state->init_vbv_buffer_fullness_in_bit = misc->initial_buffer_fullness;
}

static void
gen9_avc_update_framerate_parameters(VADriverContextP ctx,
                                     struct intel_encoder_context *encoder_context,
                                     VAEncMiscParameterFrameRate *misc)
{
    struct encoder_vme_mfc_context * vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    struct generic_enc_codec_state * generic_state = (struct generic_enc_codec_state * )vme_context->generic_enc_state;

    generic_state->frames_per_100s = misc->framerate * 100; /* misc->framerate is multiple of 100 */
    generic_state->frame_rate = misc->framerate ;
}

static void
gen9_avc_update_roi_parameters(VADriverContextP ctx,
                               struct intel_encoder_context *encoder_context,
                               VAEncMiscParameterBufferROI *misc)
{
    struct encoder_vme_mfc_context * vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    struct generic_enc_codec_state * generic_state = (struct generic_enc_codec_state * )vme_context->generic_enc_state;
    int i;

    if (!misc || !misc->roi) {
        generic_state->num_roi = 0;
        return;
    }

    generic_state->num_roi = MIN(misc->num_roi, 3);
    generic_state->max_delta_qp = misc->max_delta_qp;
    generic_state->min_delta_qp = misc->min_delta_qp;

    for (i = 0; i < generic_state->num_roi; i++) {
        generic_state->roi[i].left = misc->roi->roi_rectangle.x;
        generic_state->roi[i].right = generic_state->roi[i].left + misc->roi->roi_rectangle.width;
        generic_state->roi[i].top = misc->roi->roi_rectangle.y;
        generic_state->roi[i].bottom = generic_state->roi[i].top + misc->roi->roi_rectangle.height;
        generic_state->roi[i].value = misc->roi->roi_value;

        generic_state->roi[i].left /= 16;
        generic_state->roi[i].right /= 16;
        generic_state->roi[i].top /= 16;
        generic_state->roi[i].bottom /= 16;
    }
}

static void
gen9_avc_update_misc_parameters(VADriverContextP ctx,
                                struct encode_state *encode_state,
                                struct intel_encoder_context *encoder_context)
{
    int i,j;
    VAEncMiscParameterBuffer *misc_param;

    for (i = 0; i < ARRAY_ELEMS(encode_state->misc_param); i++) {
        for (j = 0; j < ARRAY_ELEMS(encode_state->misc_param[0]); j++) {
            if (!encode_state->misc_param[i][j] || !encode_state->misc_param[i][j]->buffer)
                continue;

            misc_param = (VAEncMiscParameterBuffer *)encode_state->misc_param[i][0]->buffer;

            switch (misc_param->type) {
            case VAEncMiscParameterTypeFrameRate:
                gen9_avc_update_framerate_parameters(ctx,
                                                     encoder_context,
                                                     (VAEncMiscParameterFrameRate *)misc_param->data);
                break;

            case VAEncMiscParameterTypeRateControl:
                gen9_avc_update_rate_control_parameters(ctx,
                                                        encoder_context,
                                                        (VAEncMiscParameterRateControl *)misc_param->data);
                break;

            case VAEncMiscParameterTypeHRD:
                gen9_avc_update_hrd_parameters(ctx,
                                               encoder_context,
                                               (VAEncMiscParameterHRD *)misc_param->data);
                break;

            case VAEncMiscParameterTypeROI:
                gen9_avc_update_roi_parameters(ctx,
                                               encoder_context,
                                               (VAEncMiscParameterBufferROI *)misc_param->data);
                break;

            default:
                break;
            }
        }
    }
}
