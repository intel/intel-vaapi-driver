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
