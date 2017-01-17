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
gen9_free_surfaces_avc(void **data)
{
    struct gen9_surface_avc *avc_surface;

    if (!data || !*data)
        return;

    avc_surface = *data;

    if (avc_surface->scaled_4x_surface_obj) {
        i965_DestroySurfaces(avc_surface->ctx, &avc_surface->scaled_4x_surface_id, 1);
        avc_surface->scaled_4x_surface_id = VA_INVALID_SURFACE;
        avc_surface->scaled_4x_surface_obj = NULL;
    }

    if (avc_surface->scaled_16x_surface_obj) {
        i965_DestroySurfaces(avc_surface->ctx, &avc_surface->scaled_16x_surface_id, 1);
        avc_surface->scaled_16x_surface_id = VA_INVALID_SURFACE;
        avc_surface->scaled_16x_surface_obj = NULL;
    }

    if (avc_surface->scaled_32x_surface_obj) {
        i965_DestroySurfaces(avc_surface->ctx, &avc_surface->scaled_32x_surface_id, 1);
        avc_surface->scaled_32x_surface_id = VA_INVALID_SURFACE;
        avc_surface->scaled_32x_surface_obj = NULL;
    }

    i965_free_gpe_resource(&avc_surface->res_mb_code_surface);
    i965_free_gpe_resource(&avc_surface->res_mv_data_surface);
    i965_free_gpe_resource(&avc_surface->res_ref_pic_select_surface);

    dri_bo_unreference(avc_surface->dmv_top);
    avc_surface->dmv_top = NULL;
    dri_bo_unreference(avc_surface->dmv_bottom);
    avc_surface->dmv_bottom = NULL;

    free(avc_surface);

    *data = NULL;

    return;
}

static VAStatus
gen9_avc_init_check_surfaces(VADriverContextP ctx,
                             struct object_surface *obj_surface,
                             struct intel_encoder_context *encoder_context,
                             struct avc_surface_param *surface_param)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct encoder_vme_mfc_context * vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    struct avc_enc_state * avc_state = (struct avc_enc_state * )vme_context->private_enc_state;
    struct generic_enc_codec_state * generic_state = (struct generic_enc_codec_state * )vme_context->generic_enc_state;

    struct gen9_surface_avc *avc_surface;
    int downscaled_width_4x, downscaled_height_4x;
    int downscaled_width_16x, downscaled_height_16x;
    int downscaled_width_32x, downscaled_height_32x;
    int size = 0;
    unsigned int frame_width_in_mbs = ALIGN(surface_param->frame_width,16) / 16;
    unsigned int frame_height_in_mbs = ALIGN(surface_param->frame_height,16) / 16;
    unsigned int frame_mb_nums = frame_width_in_mbs * frame_height_in_mbs;
    int allocate_flag = 1;
    int width,height;

    if (!obj_surface || !obj_surface->bo)
        return VA_STATUS_ERROR_INVALID_SURFACE;

    if (obj_surface->private_data &&
        obj_surface->free_private_data != gen9_free_surfaces_avc) {
        obj_surface->free_private_data(&obj_surface->private_data);
        obj_surface->private_data = NULL;
    }

    if (obj_surface->private_data) {
        return VA_STATUS_SUCCESS;
    }

    avc_surface = calloc(1, sizeof(struct gen9_surface_avc));

    if (!avc_surface)
        return VA_STATUS_ERROR_ALLOCATION_FAILED;

    avc_surface->ctx = ctx;
    obj_surface->private_data = avc_surface;
    obj_surface->free_private_data = gen9_free_surfaces_avc;

    downscaled_width_4x = generic_state->frame_width_4x;
    downscaled_height_4x = generic_state->frame_height_4x;

    i965_CreateSurfaces(ctx,
                        downscaled_width_4x,
                        downscaled_height_4x,
                        VA_RT_FORMAT_YUV420,
                        1,
                        &avc_surface->scaled_4x_surface_id);

    avc_surface->scaled_4x_surface_obj = SURFACE(avc_surface->scaled_4x_surface_id);

    if (!avc_surface->scaled_4x_surface_obj) {
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }

    i965_check_alloc_surface_bo(ctx, avc_surface->scaled_4x_surface_obj, 1,
                                VA_FOURCC('N', 'V', '1', '2'), SUBSAMPLE_YUV420);

    downscaled_width_16x = generic_state->frame_width_16x;
    downscaled_height_16x = generic_state->frame_height_16x;
    i965_CreateSurfaces(ctx,
                        downscaled_width_16x,
                        downscaled_height_16x,
                        VA_RT_FORMAT_YUV420,
                        1,
                        &avc_surface->scaled_16x_surface_id);
    avc_surface->scaled_16x_surface_obj = SURFACE(avc_surface->scaled_16x_surface_id);

    if (!avc_surface->scaled_16x_surface_obj) {
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }

    i965_check_alloc_surface_bo(ctx, avc_surface->scaled_16x_surface_obj, 1,
                                VA_FOURCC('N', 'V', '1', '2'), SUBSAMPLE_YUV420);

    downscaled_width_32x = generic_state->frame_width_32x;
    downscaled_height_32x = generic_state->frame_height_32x;
    i965_CreateSurfaces(ctx,
                        downscaled_width_32x,
                        downscaled_height_32x,
                        VA_RT_FORMAT_YUV420,
                        1,
                        &avc_surface->scaled_32x_surface_id);
    avc_surface->scaled_32x_surface_obj = SURFACE(avc_surface->scaled_32x_surface_id);

    if (!avc_surface->scaled_32x_surface_obj) {
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }

    i965_check_alloc_surface_bo(ctx, avc_surface->scaled_32x_surface_obj, 1,
                                VA_FOURCC('N', 'V', '1', '2'), SUBSAMPLE_YUV420);

    /*mb code and mv data for each frame*/
    size = frame_mb_nums * 16 * 4;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
        &avc_surface->res_mb_code_surface,
        ALIGN(size,0x1000),
        "mb code buffer");
    if (!allocate_flag)
        goto failed_allocation;

    size = frame_mb_nums * 32 * 4;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
        &avc_surface->res_mv_data_surface,
        ALIGN(size,0x1000),
        "mv data buffer");
    if (!allocate_flag)
        goto failed_allocation;

    /* ref pic list*/
    if(avc_state->ref_pic_select_list_supported)
    {
        width = ALIGN(frame_width_in_mbs * 8,64);
        height= frame_height_in_mbs ;
        allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                     &avc_surface->res_ref_pic_select_surface,
                                     width, height,
                                     width,
                                     "Ref pic select list buffer");
        if (!allocate_flag)
            goto failed_allocation;
    }

    /*direct mv*/
    avc_surface->dmv_top =
        dri_bo_alloc(i965->intel.bufmgr,
        "direct mv top Buffer",
        68 * frame_mb_nums,
        64);
    avc_surface->dmv_bottom =
        dri_bo_alloc(i965->intel.bufmgr,
        "direct mv bottom Buffer",
        68 * frame_mb_nums,
        64);
    assert(avc_surface->dmv_top);
    assert(avc_surface->dmv_bottom);

    return VA_STATUS_SUCCESS;

failed_allocation:
    return VA_STATUS_ERROR_ALLOCATION_FAILED;
}

static VAStatus
gen9_avc_allocate_resources(VADriverContextP ctx,
                            struct encode_state *encode_state,
                            struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct encoder_vme_mfc_context * vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    struct gen9_avc_encoder_context * avc_ctx = (struct gen9_avc_encoder_context * )vme_context->private_enc_ctx;
    struct generic_enc_codec_state * generic_state = (struct generic_enc_codec_state * )vme_context->generic_enc_state;
    struct avc_enc_state * avc_state = (struct avc_enc_state * )vme_context->private_enc_state;
    unsigned int size  = 0;
    unsigned int width  = 0;
    unsigned int height  = 0;
    unsigned char * data  = NULL;
    int allocate_flag = 1;
    int i = 0;

    /*all the surface/buffer are allocated here*/

    /*second level batch buffer for image state write when cqp etc*/
    i965_free_gpe_resource(&avc_ctx->res_image_state_batch_buffer_2nd_level);
    size = INTEL_AVC_IMAGE_STATE_CMD_SIZE ;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                             &avc_ctx->res_image_state_batch_buffer_2nd_level,
                             ALIGN(size,0x1000),
                             "second levle batch (image state write) buffer");
    if (!allocate_flag)
        goto failed_allocation;

    i965_free_gpe_resource(&avc_ctx->res_slice_batch_buffer_2nd_level);
    /* include (dw) 2* (ref_id + weight_state + pak_insert_obj) + slice state(11) + slice/pps/sps headers, no mb code size
       2*(10 + 98 + X) + 11*/
    size = 4096 + (320 * 4 + 80 + 16) * encode_state->num_slice_params_ext;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                             &avc_ctx->res_slice_batch_buffer_2nd_level,
                             ALIGN(size,0x1000),
                             "second levle batch (slice) buffer");
    if (!allocate_flag)
        goto failed_allocation;

    /* scaling related surface   */
    if(avc_state->mb_status_supported)
    {
        i965_free_gpe_resource(&avc_ctx->res_mb_status_buffer);
        size = (generic_state->frame_width_in_mbs * generic_state->frame_height_in_mbs * 16 * 4 + 1023)&~0x3ff;
        allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                 &avc_ctx->res_mb_status_buffer,
                                 ALIGN(size,0x1000),
                                 "MB statistics output buffer");
        if (!allocate_flag)
            goto failed_allocation;
        i965_zero_gpe_resource(&avc_ctx->res_mb_status_buffer);
    }

    if(avc_state->flatness_check_supported)
    {
        width = generic_state->frame_width_in_mbs * 4;
        height= generic_state->frame_height_in_mbs * 4;
        i965_free_gpe_resource(&avc_ctx->res_flatness_check_surface);
        allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                     &avc_ctx->res_flatness_check_surface,
                                     width, height,
                                     ALIGN(width,64),
                                     "Flatness check buffer");
        if (!allocate_flag)
            goto failed_allocation;
    }
    /* me related surface */
    width = generic_state->downscaled_width_4x_in_mb * 8;
    height= generic_state->downscaled_height_4x_in_mb * 4 * 10;
    i965_free_gpe_resource(&avc_ctx->s4x_memv_distortion_buffer);
    allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                 &avc_ctx->s4x_memv_distortion_buffer,
                                 width, height,
                                 ALIGN(width,64),
                                 "4x MEMV distortion buffer");
    if (!allocate_flag)
        goto failed_allocation;
    i965_zero_gpe_resource(&avc_ctx->s4x_memv_distortion_buffer);

    width = (generic_state->downscaled_width_4x_in_mb + 7)/8 * 64;
    height= (generic_state->downscaled_height_4x_in_mb + 1)/2 * 8;
    i965_free_gpe_resource(&avc_ctx->s4x_memv_min_distortion_brc_buffer);
    allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                 &avc_ctx->s4x_memv_min_distortion_brc_buffer,
                                 width, height,
                                 width,
                                 "4x MEMV min distortion brc buffer");
    if (!allocate_flag)
        goto failed_allocation;
    i965_zero_gpe_resource(&avc_ctx->s4x_memv_min_distortion_brc_buffer);


    width = ALIGN(generic_state->downscaled_width_4x_in_mb * 32,64);
    height= generic_state->downscaled_height_4x_in_mb * 4 * 2 * 10;
    i965_free_gpe_resource(&avc_ctx->s4x_memv_data_buffer);
    allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                 &avc_ctx->s4x_memv_data_buffer,
                                 width, height,
                                 width,
                                 "4x MEMV data buffer");
    if (!allocate_flag)
        goto failed_allocation;
    i965_zero_gpe_resource(&avc_ctx->s4x_memv_data_buffer);


    width = ALIGN(generic_state->downscaled_width_16x_in_mb * 32,64);
    height= generic_state->downscaled_height_16x_in_mb * 4 * 2 * 10 ;
    i965_free_gpe_resource(&avc_ctx->s16x_memv_data_buffer);
    allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                 &avc_ctx->s16x_memv_data_buffer,
                                 width, height,
                                 width,
                                 "16x MEMV data buffer");
    if (!allocate_flag)
        goto failed_allocation;
    i965_zero_gpe_resource(&avc_ctx->s16x_memv_data_buffer);


    width = ALIGN(generic_state->downscaled_width_32x_in_mb * 32,64);
    height= generic_state->downscaled_height_32x_in_mb * 4 * 2 * 10 ;
    i965_free_gpe_resource(&avc_ctx->s32x_memv_data_buffer);
    allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                 &avc_ctx->s32x_memv_data_buffer,
                                 width, height,
                                 width,
                                 "32x MEMV data buffer");
    if (!allocate_flag)
        goto failed_allocation;
    i965_zero_gpe_resource(&avc_ctx->s32x_memv_data_buffer);


    if(!generic_state->brc_allocated)
    {
        /*brc related surface */
        i965_free_gpe_resource(&avc_ctx->res_brc_history_buffer);
        size = 864;
        allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                 &avc_ctx->res_brc_history_buffer,
                                 ALIGN(size,0x1000),
                                 "brc history buffer");
        if (!allocate_flag)
            goto failed_allocation;

        i965_free_gpe_resource(&avc_ctx->res_brc_pre_pak_statistics_output_buffer);
        size = 64;//44
        allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                 &avc_ctx->res_brc_pre_pak_statistics_output_buffer,
                                 ALIGN(size,0x1000),
                                 "brc pak statistic buffer");
        if (!allocate_flag)
            goto failed_allocation;

        i965_free_gpe_resource(&avc_ctx->res_brc_image_state_read_buffer);
        size = INTEL_AVC_IMAGE_STATE_CMD_SIZE * 7;
        allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                 &avc_ctx->res_brc_image_state_read_buffer,
                                 ALIGN(size,0x1000),
                                 "brc image state read buffer");
        if (!allocate_flag)
            goto failed_allocation;

        i965_free_gpe_resource(&avc_ctx->res_brc_image_state_write_buffer);
        size = INTEL_AVC_IMAGE_STATE_CMD_SIZE * 7;
        allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                 &avc_ctx->res_brc_image_state_write_buffer,
                                 ALIGN(size,0x1000),
                                 "brc image state write buffer");
        if (!allocate_flag)
            goto failed_allocation;

        width = ALIGN(64,64);
        height= 44;
        i965_free_gpe_resource(&avc_ctx->res_brc_const_data_buffer);
        allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                     &avc_ctx->res_brc_const_data_buffer,
                                     width, height,
                                     width,
                                     "brc const data buffer");
        if (!allocate_flag)
            goto failed_allocation;

        if(generic_state->brc_distortion_buffer_supported)
        {
            width = ALIGN(generic_state->downscaled_width_4x_in_mb * 8,64);
            height= ALIGN(generic_state->downscaled_height_4x_in_mb * 4,8);
            width = (generic_state->downscaled_width_4x_in_mb + 7)/8 * 64;
            height= (generic_state->downscaled_height_4x_in_mb + 1)/2 * 8;
            i965_free_gpe_resource(&avc_ctx->res_brc_dist_data_surface);
            allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                         &avc_ctx->res_brc_dist_data_surface,
                                         width, height,
                                         width,
                                         "brc dist data buffer");
            if (!allocate_flag)
                goto failed_allocation;
            i965_zero_gpe_resource(&avc_ctx->res_brc_dist_data_surface);
        }

        if(generic_state->brc_roi_enable)
        {
            width = ALIGN(generic_state->downscaled_width_4x_in_mb * 16,64);
            height= ALIGN(generic_state->downscaled_height_4x_in_mb * 4,8);
            i965_free_gpe_resource(&avc_ctx->res_mbbrc_roi_surface);
            allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                         &avc_ctx->res_mbbrc_roi_surface,
                                         width, height,
                                         width,
                                         "mbbrc roi buffer");
            if (!allocate_flag)
                goto failed_allocation;
            i965_zero_gpe_resource(&avc_ctx->res_mbbrc_roi_surface);
        }

        /*mb qp in mb brc*/
        width = ALIGN(generic_state->downscaled_width_4x_in_mb * 4,64);
        height= ALIGN(generic_state->downscaled_height_4x_in_mb * 4,8);
        i965_free_gpe_resource(&avc_ctx->res_mbbrc_mb_qp_data_surface);
        allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                     &avc_ctx->res_mbbrc_mb_qp_data_surface,
                                     width, height,
                                     width,
                                     "mbbrc mb qp buffer");
        if (!allocate_flag)
            goto failed_allocation;

        i965_free_gpe_resource(&avc_ctx->res_mbbrc_const_data_buffer);
        size = 16 * 52 * 4;
        allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                 &avc_ctx->res_mbbrc_const_data_buffer,
                                 ALIGN(size,0x1000),
                                 "mbbrc const data buffer");
        if (!allocate_flag)
            goto failed_allocation;

        generic_state->brc_allocated = 1;
    }

    /*mb qp external*/
    if(avc_state->mb_qp_data_enable)
    {
        width = ALIGN(generic_state->downscaled_width_4x_in_mb * 4,64);
        height= ALIGN(generic_state->downscaled_height_4x_in_mb * 4,8);
        i965_free_gpe_resource(&avc_ctx->res_mb_qp_data_surface);
        allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                     &avc_ctx->res_mb_qp_data_surface,
                                     width, height,
                                     width,
                                     "external mb qp buffer");
        if (!allocate_flag)
            goto failed_allocation;
    }


    /* maybe it is not needed by now. it is used in crypt mode*/
    i965_free_gpe_resource(&avc_ctx->res_brc_mbenc_curbe_write_buffer);
    size = ALIGN(sizeof(gen9_avc_mbenc_curbe_data), 64) + ALIGN(sizeof(struct gen8_interface_descriptor_data), 64) ;//* NUM_GEN9_AVC_KERNEL_MBENC;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                             &avc_ctx->res_brc_mbenc_curbe_write_buffer,
                             size,
                             "mbenc curbe data buffer");
    if (!allocate_flag)
        goto failed_allocation;

    /*     mbenc related surface. it share most of surface with other kernels     */
    if(avc_state->arbitrary_num_mbs_in_slice)
    {
        width = (generic_state->frame_width_in_mbs + 1) * 64;
        height= generic_state->frame_height_in_mbs ;
        i965_free_gpe_resource(&avc_ctx->res_mbenc_slice_map_surface);
        allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                     &avc_ctx->res_mbenc_slice_map_surface,
                                     width, height,
                                     width,
                                     "slice map buffer");
        if (!allocate_flag)
            goto failed_allocation;

        /*generate slice map,default one slice per frame.*/
    }

    /* sfd related surface  */
    if(avc_state->sfd_enable)
    {
        i965_free_gpe_resource(&avc_ctx->res_sfd_output_buffer);
        size = 128;
        allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                 &avc_ctx->res_sfd_output_buffer,
                                 size,
                                 "sfd output buffer");
        if (!allocate_flag)
            goto failed_allocation;

        i965_free_gpe_resource(&avc_ctx->res_sfd_cost_table_p_frame_buffer);
        size = ALIGN(52,64);
        allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                 &avc_ctx->res_sfd_cost_table_p_frame_buffer,
                                 size,
                                 "sfd P frame cost table buffer");
        if (!allocate_flag)
            goto failed_allocation;
        data = i965_map_gpe_resource(&(avc_ctx->res_sfd_cost_table_p_frame_buffer));
        assert(data);
        memcpy(data,gen9_avc_sfd_cost_table_p_frame,sizeof(unsigned char) *52);
        i965_unmap_gpe_resource(&(avc_ctx->res_sfd_cost_table_p_frame_buffer));

        i965_free_gpe_resource(&avc_ctx->res_sfd_cost_table_b_frame_buffer);
        size = ALIGN(52,64);
        allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                 &avc_ctx->res_sfd_cost_table_b_frame_buffer,
                                 size,
                                 "sfd B frame cost table buffer");
        if (!allocate_flag)
            goto failed_allocation;
        data = i965_map_gpe_resource(&(avc_ctx->res_sfd_cost_table_b_frame_buffer));
        assert(data);
        memcpy(data,gen9_avc_sfd_cost_table_b_frame,sizeof(unsigned char) *52);
        i965_unmap_gpe_resource(&(avc_ctx->res_sfd_cost_table_b_frame_buffer));
    }

    /* wp related surfaces */
    if(avc_state->weighted_prediction_supported)
    {
        for(i = 0; i < 2 ; i++)
        {
            if (avc_ctx->wp_output_pic_select_surface_obj[i]) {
                continue;
            }

            width = generic_state->frame_width_in_pixel;
            height= generic_state->frame_height_in_pixel ;
            i965_CreateSurfaces(ctx,
                                width,
                                height,
                                VA_RT_FORMAT_YUV420,
                                1,
                                &avc_ctx->wp_output_pic_select_surface_id[i]);
            avc_ctx->wp_output_pic_select_surface_obj[i] = SURFACE(avc_ctx->wp_output_pic_select_surface_id[i]);

            if (!avc_ctx->wp_output_pic_select_surface_obj[i]) {
                goto failed_allocation;
            }

            i965_check_alloc_surface_bo(ctx, avc_ctx->wp_output_pic_select_surface_obj[i], 1,
                                VA_FOURCC('N', 'V', '1', '2'), SUBSAMPLE_YUV420);
        }
        i965_free_gpe_resource(&avc_ctx->res_wp_output_pic_select_surface_list[0]);
        i965_object_surface_to_2d_gpe_resource(&avc_ctx->res_wp_output_pic_select_surface_list[0], avc_ctx->wp_output_pic_select_surface_obj[0]);
        i965_free_gpe_resource(&avc_ctx->res_wp_output_pic_select_surface_list[1]);
        i965_object_surface_to_2d_gpe_resource(&avc_ctx->res_wp_output_pic_select_surface_list[1], avc_ctx->wp_output_pic_select_surface_obj[1]);
    }

    /* other   */

    i965_free_gpe_resource(&avc_ctx->res_mad_data_buffer);
    size = 4 * 1;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                 &avc_ctx->res_mad_data_buffer,
                                 ALIGN(size,0x1000),
                                 "MAD data buffer");
    if (!allocate_flag)
        goto failed_allocation;

    return VA_STATUS_SUCCESS;

failed_allocation:
    return VA_STATUS_ERROR_ALLOCATION_FAILED;
}

static void
gen9_avc_free_resources(struct encoder_vme_mfc_context * vme_context)
{
    if(!vme_context)
        return;

    struct gen9_avc_encoder_context * avc_ctx = (struct gen9_avc_encoder_context * )vme_context->private_enc_ctx;
    VADriverContextP ctx = avc_ctx->ctx;
    int i = 0;

    /* free all the surface/buffer here*/
    i965_free_gpe_resource(&avc_ctx->res_image_state_batch_buffer_2nd_level);
    i965_free_gpe_resource(&avc_ctx->res_slice_batch_buffer_2nd_level);
    i965_free_gpe_resource(&avc_ctx->res_mb_status_buffer);
    i965_free_gpe_resource(&avc_ctx->res_flatness_check_surface);
    i965_free_gpe_resource(&avc_ctx->s4x_memv_distortion_buffer);
    i965_free_gpe_resource(&avc_ctx->s4x_memv_min_distortion_brc_buffer);
    i965_free_gpe_resource(&avc_ctx->s4x_memv_data_buffer);
    i965_free_gpe_resource(&avc_ctx->s16x_memv_data_buffer);
    i965_free_gpe_resource(&avc_ctx->s32x_memv_data_buffer);
    i965_free_gpe_resource(&avc_ctx->res_brc_history_buffer);
    i965_free_gpe_resource(&avc_ctx->res_brc_pre_pak_statistics_output_buffer);
    i965_free_gpe_resource(&avc_ctx->res_brc_image_state_read_buffer);
    i965_free_gpe_resource(&avc_ctx->res_brc_image_state_write_buffer);
    i965_free_gpe_resource(&avc_ctx->res_brc_const_data_buffer);
    i965_free_gpe_resource(&avc_ctx->res_brc_dist_data_surface);
    i965_free_gpe_resource(&avc_ctx->res_mbbrc_roi_surface);
    i965_free_gpe_resource(&avc_ctx->res_mbbrc_mb_qp_data_surface);
    i965_free_gpe_resource(&avc_ctx->res_mb_qp_data_surface);
    i965_free_gpe_resource(&avc_ctx->res_mbbrc_const_data_buffer);
    i965_free_gpe_resource(&avc_ctx->res_brc_mbenc_curbe_write_buffer);
    i965_free_gpe_resource(&avc_ctx->res_mbenc_slice_map_surface);
    i965_free_gpe_resource(&avc_ctx->res_sfd_output_buffer);
    i965_free_gpe_resource(&avc_ctx->res_sfd_cost_table_p_frame_buffer);
    i965_free_gpe_resource(&avc_ctx->res_sfd_cost_table_b_frame_buffer);
    i965_free_gpe_resource(&avc_ctx->res_wp_output_pic_select_surface_list[0]);
    i965_free_gpe_resource(&avc_ctx->res_wp_output_pic_select_surface_list[1]);
    i965_free_gpe_resource(&avc_ctx->res_mad_data_buffer);

    for(i = 0;i < 2 ; i++)
    {
        if (avc_ctx->wp_output_pic_select_surface_obj[i]) {
            i965_DestroySurfaces(ctx, &avc_ctx->wp_output_pic_select_surface_id[i], 1);
            avc_ctx->wp_output_pic_select_surface_id[i] = VA_INVALID_SURFACE;
            avc_ctx->wp_output_pic_select_surface_obj[i] = NULL;
        }
    }

}
