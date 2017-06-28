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
 * SOFTWAR
 *
 * Authors:
 *    Zhao, Yakui <yakui.zhao@intel.com>
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
#include "i965_drv_video.h"
#include "i965_encoder.h"
#include "gen9_vp9_encapi.h"
#include "gen9_vp9_encoder.h"
#include "gen9_vp9_encoder_kernels.h"
#include "vp9_probs.h"
#include "gen9_vp9_const_def.h"

#define MAX_VP9_ENCODER_SURFACES        64

#define MAX_URB_SIZE                    4096 /* In register */
#define NUM_KERNELS_PER_GPE_CONTEXT     1

#define VP9_BRC_KBPS                    1000

#define BRC_KERNEL_CBR                  0x0010
#define BRC_KERNEL_VBR                  0x0020
#define BRC_KERNEL_AVBR                 0x0040
#define BRC_KERNEL_CQL                  0x0080

#define VP9_PIC_STATE_BUFFER_SIZE 192

typedef struct _intel_kernel_header_ {
    uint32_t       reserved                        : 6;
    uint32_t       kernel_start_pointer            : 26;
} intel_kernel_header;

typedef struct _intel_vp9_kernel_header {
    int nKernelCount;
    intel_kernel_header PLY_DSCALE;
    intel_kernel_header VP9_ME_P;
    intel_kernel_header VP9_Enc_I_32x32;
    intel_kernel_header VP9_Enc_I_16x16;
    intel_kernel_header VP9_Enc_P;
    intel_kernel_header VP9_Enc_TX;
    intel_kernel_header VP9_DYS;

    intel_kernel_header VP9BRC_Intra_Distortion;
    intel_kernel_header VP9BRC_Init;
    intel_kernel_header VP9BRC_Reset;
    intel_kernel_header VP9BRC_Update;
} intel_vp9_kernel_header;

#define DYS_1X_FLAG    0x01
#define DYS_4X_FLAG    0x02
#define DYS_16X_FLAG   0x04

struct vp9_surface_param {
    uint32_t frame_width;
    uint32_t frame_height;
};

static uint32_t intel_convert_sign_mag(int val, int sign_bit_pos)
{
    uint32_t ret_val = 0;
    if (val < 0) {
        val = -val;
        ret_val = ((1 << (sign_bit_pos - 1)) | (val & ((1 << (sign_bit_pos - 1)) - 1)));
    } else {
        ret_val = val & ((1 << (sign_bit_pos - 1)) - 1);
    }
    return ret_val;
}

static bool
intel_vp9_get_kernel_header_and_size(
    void                             *pvbinary,
    int                              binary_size,
    INTEL_VP9_ENC_OPERATION          operation,
    int                              krnstate_idx,
    struct i965_kernel               *ret_kernel)
{
    typedef uint32_t BIN_PTR[4];

    char *bin_start;
    intel_vp9_kernel_header      *pkh_table;
    intel_kernel_header          *pcurr_header, *pinvalid_entry, *pnext_header;
    int next_krnoffset;

    if (!pvbinary || !ret_kernel)
        return false;

    bin_start = (char *)pvbinary;
    pkh_table = (intel_vp9_kernel_header *)pvbinary;
    pinvalid_entry = &(pkh_table->VP9BRC_Update) + 1;
    next_krnoffset = binary_size;

    if ((operation == INTEL_VP9_ENC_SCALING4X) || (operation == INTEL_VP9_ENC_SCALING2X)) {
        pcurr_header = &pkh_table->PLY_DSCALE;
    } else if (operation == INTEL_VP9_ENC_ME) {
        pcurr_header = &pkh_table->VP9_ME_P;
    } else if (operation == INTEL_VP9_ENC_MBENC) {
        pcurr_header = &pkh_table->VP9_Enc_I_32x32;
    } else if (operation == INTEL_VP9_ENC_DYS) {
        pcurr_header = &pkh_table->VP9_DYS;
    } else if (operation == INTEL_VP9_ENC_BRC) {
        pcurr_header = &pkh_table->VP9BRC_Intra_Distortion;
    } else {
        return false;
    }

    pcurr_header += krnstate_idx;
    ret_kernel->bin = (const BIN_PTR *)(bin_start + (pcurr_header->kernel_start_pointer << 6));

    pnext_header = (pcurr_header + 1);
    if (pnext_header < pinvalid_entry) {
        next_krnoffset = pnext_header->kernel_start_pointer << 6;
    }
    ret_kernel->size = next_krnoffset - (pcurr_header->kernel_start_pointer << 6);

    return true;
}


static void
gen9_free_surfaces_vp9(void **data)
{
    struct gen9_surface_vp9 *vp9_surface;

    if (!data || !*data)
        return;

    vp9_surface = *data;

    if (vp9_surface->scaled_4x_surface_obj) {
        i965_DestroySurfaces(vp9_surface->ctx, &vp9_surface->scaled_4x_surface_id, 1);
        vp9_surface->scaled_4x_surface_id = VA_INVALID_SURFACE;
        vp9_surface->scaled_4x_surface_obj = NULL;
    }

    if (vp9_surface->scaled_16x_surface_obj) {
        i965_DestroySurfaces(vp9_surface->ctx, &vp9_surface->scaled_16x_surface_id, 1);
        vp9_surface->scaled_16x_surface_id = VA_INVALID_SURFACE;
        vp9_surface->scaled_16x_surface_obj = NULL;
    }

    if (vp9_surface->dys_4x_surface_obj) {
        i965_DestroySurfaces(vp9_surface->ctx, &vp9_surface->dys_4x_surface_id, 1);
        vp9_surface->dys_4x_surface_id = VA_INVALID_SURFACE;
        vp9_surface->dys_4x_surface_obj = NULL;
    }

    if (vp9_surface->dys_16x_surface_obj) {
        i965_DestroySurfaces(vp9_surface->ctx, &vp9_surface->dys_16x_surface_id, 1);
        vp9_surface->dys_16x_surface_id = VA_INVALID_SURFACE;
        vp9_surface->dys_16x_surface_obj = NULL;
    }

    if (vp9_surface->dys_surface_obj) {
        i965_DestroySurfaces(vp9_surface->ctx, &vp9_surface->dys_surface_id, 1);
        vp9_surface->dys_surface_id = VA_INVALID_SURFACE;
        vp9_surface->dys_surface_obj = NULL;
    }

    free(vp9_surface);

    *data = NULL;

    return;
}

static VAStatus
gen9_vp9_init_check_surfaces(VADriverContextP ctx,
                             struct object_surface *obj_surface,
                             struct vp9_surface_param *surface_param)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct gen9_surface_vp9 *vp9_surface;
    int downscaled_width_4x, downscaled_height_4x;
    int downscaled_width_16x, downscaled_height_16x;

    if (!obj_surface || !obj_surface->bo)
        return VA_STATUS_ERROR_INVALID_SURFACE;

    if (obj_surface->private_data &&
        obj_surface->free_private_data != gen9_free_surfaces_vp9) {
        obj_surface->free_private_data(&obj_surface->private_data);
        obj_surface->private_data = NULL;
    }

    if (obj_surface->private_data) {
        /* if the frame width/height is already the same as the expected,
         * it is unncessary to reallocate it.
         */
        vp9_surface = (struct gen9_surface_vp9 *)(obj_surface->private_data);
        if (vp9_surface->frame_width >= surface_param->frame_width ||
            vp9_surface->frame_height >= surface_param->frame_height)
            return VA_STATUS_SUCCESS;

        obj_surface->free_private_data(&obj_surface->private_data);
        obj_surface->private_data = NULL;
        vp9_surface = NULL;
    }

    vp9_surface = calloc(1, sizeof(struct gen9_surface_vp9));

    if (!vp9_surface)
        return VA_STATUS_ERROR_ALLOCATION_FAILED;

    vp9_surface->ctx = ctx;
    obj_surface->private_data = vp9_surface;
    obj_surface->free_private_data = gen9_free_surfaces_vp9;

    vp9_surface->frame_width = surface_param->frame_width;
    vp9_surface->frame_height = surface_param->frame_height;

    downscaled_width_4x = ALIGN(surface_param->frame_width / 4, 16);
    downscaled_height_4x = ALIGN(surface_param->frame_height / 4, 16);

    i965_CreateSurfaces(ctx,
                        downscaled_width_4x,
                        downscaled_height_4x,
                        VA_RT_FORMAT_YUV420,
                        1,
                        &vp9_surface->scaled_4x_surface_id);

    vp9_surface->scaled_4x_surface_obj = SURFACE(vp9_surface->scaled_4x_surface_id);

    if (!vp9_surface->scaled_4x_surface_obj) {
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }

    i965_check_alloc_surface_bo(ctx, vp9_surface->scaled_4x_surface_obj, 1,
                                VA_FOURCC('N', 'V', '1', '2'), SUBSAMPLE_YUV420);

    downscaled_width_16x = ALIGN(surface_param->frame_width / 16, 16);
    downscaled_height_16x = ALIGN(surface_param->frame_height / 16, 16);
    i965_CreateSurfaces(ctx,
                        downscaled_width_16x,
                        downscaled_height_16x,
                        VA_RT_FORMAT_YUV420,
                        1,
                        &vp9_surface->scaled_16x_surface_id);
    vp9_surface->scaled_16x_surface_obj = SURFACE(vp9_surface->scaled_16x_surface_id);

    if (!vp9_surface->scaled_16x_surface_obj) {
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }

    i965_check_alloc_surface_bo(ctx, vp9_surface->scaled_16x_surface_obj, 1,
                                VA_FOURCC('N', 'V', '1', '2'), SUBSAMPLE_YUV420);

    return VA_STATUS_SUCCESS;
}

static VAStatus
gen9_vp9_check_dys_surfaces(VADriverContextP ctx,
                            struct object_surface *obj_surface,
                            struct vp9_surface_param *surface_param)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct gen9_surface_vp9 *vp9_surface;
    int dys_width_4x, dys_height_4x;
    int dys_width_16x, dys_height_16x;

    /* As this is handled after the surface checking, it is unnecessary
     * to check the surface bo and vp9_priv_surface again
     */

    vp9_surface = (struct gen9_surface_vp9 *)(obj_surface->private_data);

    if (!vp9_surface)
        return VA_STATUS_ERROR_INVALID_SURFACE;

    /* if the frame_width/height of dys_surface is the same as
     * the expected, it is unnecessary to allocate it again
     */
    if (vp9_surface->dys_frame_width == surface_param->frame_width &&
        vp9_surface->dys_frame_width == surface_param->frame_width)
        return VA_STATUS_SUCCESS;

    if (vp9_surface->dys_4x_surface_obj) {
        i965_DestroySurfaces(vp9_surface->ctx, &vp9_surface->dys_4x_surface_id, 1);
        vp9_surface->dys_4x_surface_id = VA_INVALID_SURFACE;
        vp9_surface->dys_4x_surface_obj = NULL;
    }

    if (vp9_surface->dys_16x_surface_obj) {
        i965_DestroySurfaces(vp9_surface->ctx, &vp9_surface->dys_16x_surface_id, 1);
        vp9_surface->dys_16x_surface_id = VA_INVALID_SURFACE;
        vp9_surface->dys_16x_surface_obj = NULL;
    }

    if (vp9_surface->dys_surface_obj) {
        i965_DestroySurfaces(vp9_surface->ctx, &vp9_surface->dys_surface_id, 1);
        vp9_surface->dys_surface_id = VA_INVALID_SURFACE;
        vp9_surface->dys_surface_obj = NULL;
    }

    vp9_surface->dys_frame_width = surface_param->frame_width;
    vp9_surface->dys_frame_height = surface_param->frame_height;

    i965_CreateSurfaces(ctx,
                        surface_param->frame_width,
                        surface_param->frame_height,
                        VA_RT_FORMAT_YUV420,
                        1,
                        &vp9_surface->dys_surface_id);
    vp9_surface->dys_surface_obj = SURFACE(vp9_surface->dys_surface_id);

    if (!vp9_surface->dys_surface_obj) {
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }

    i965_check_alloc_surface_bo(ctx, vp9_surface->dys_surface_obj, 1,
                                VA_FOURCC('N', 'V', '1', '2'), SUBSAMPLE_YUV420);

    dys_width_4x = ALIGN(surface_param->frame_width / 4, 16);
    dys_height_4x = ALIGN(surface_param->frame_width / 4, 16);

    i965_CreateSurfaces(ctx,
                        dys_width_4x,
                        dys_height_4x,
                        VA_RT_FORMAT_YUV420,
                        1,
                        &vp9_surface->dys_4x_surface_id);

    vp9_surface->dys_4x_surface_obj = SURFACE(vp9_surface->dys_4x_surface_id);

    if (!vp9_surface->dys_4x_surface_obj) {
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }

    i965_check_alloc_surface_bo(ctx, vp9_surface->dys_4x_surface_obj, 1,
                                VA_FOURCC('N', 'V', '1', '2'), SUBSAMPLE_YUV420);

    dys_width_16x = ALIGN(surface_param->frame_width / 16, 16);
    dys_height_16x = ALIGN(surface_param->frame_width / 16, 16);
    i965_CreateSurfaces(ctx,
                        dys_width_16x,
                        dys_height_16x,
                        VA_RT_FORMAT_YUV420,
                        1,
                        &vp9_surface->dys_16x_surface_id);
    vp9_surface->dys_16x_surface_obj = SURFACE(vp9_surface->dys_16x_surface_id);

    if (!vp9_surface->dys_16x_surface_obj) {
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }

    i965_check_alloc_surface_bo(ctx, vp9_surface->dys_16x_surface_obj, 1,
                                VA_FOURCC('N', 'V', '1', '2'), SUBSAMPLE_YUV420);

    return VA_STATUS_SUCCESS;
}

static VAStatus
gen9_vp9_allocate_resources(VADriverContextP ctx,
                            struct encode_state *encode_state,
                            struct intel_encoder_context *encoder_context,
                            int allocate)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct gen9_encoder_context_vp9 *vme_context = encoder_context->vme_context;
    struct gen9_vp9_state *vp9_state;
    int allocate_flag, i;
    int res_size;
    uint32_t        frame_width_in_sb, frame_height_in_sb, frame_sb_num;
    unsigned int width, height;

    vp9_state = (struct gen9_vp9_state *) encoder_context->enc_priv_state;

    if (!vp9_state || !vp9_state->pic_param)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    /* the buffer related with BRC is not changed. So it is allocated
     * based on the input parameter
     */
    if (allocate) {
        i965_free_gpe_resource(&vme_context->res_brc_history_buffer);
        i965_free_gpe_resource(&vme_context->res_brc_const_data_buffer);
        i965_free_gpe_resource(&vme_context->res_brc_mbenc_curbe_write_buffer);
        i965_free_gpe_resource(&vme_context->res_pic_state_brc_read_buffer);
        i965_free_gpe_resource(&vme_context->res_pic_state_brc_write_hfw_read_buffer);
        i965_free_gpe_resource(&vme_context->res_pic_state_hfw_write_buffer);
        i965_free_gpe_resource(&vme_context->res_seg_state_brc_read_buffer);
        i965_free_gpe_resource(&vme_context->res_seg_state_brc_write_buffer);
        i965_free_gpe_resource(&vme_context->res_brc_bitstream_size_buffer);
        i965_free_gpe_resource(&vme_context->res_brc_hfw_data_buffer);
        i965_free_gpe_resource(&vme_context->res_brc_mmdk_pak_buffer);

        allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                                   &vme_context->res_brc_history_buffer,
                                                   VP9_BRC_HISTORY_BUFFER_SIZE,
                                                   "Brc History buffer");
        if (!allocate_flag)
            goto failed_allocation;
        allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                                   &vme_context->res_brc_const_data_buffer,
                                                   VP9_BRC_CONSTANTSURFACE_SIZE,
                                                   "Brc Constant buffer");
        if (!allocate_flag)
            goto failed_allocation;

        res_size = ALIGN(sizeof(vp9_mbenc_curbe_data), 64) + 128 +
                   ALIGN(sizeof(struct gen8_interface_descriptor_data), 64) * NUM_VP9_MBENC;
        allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                                   &vme_context->res_brc_mbenc_curbe_write_buffer,
                                                   res_size,
                                                   "Brc Curbe write");
        if (!allocate_flag)
            goto failed_allocation;

        res_size = VP9_PIC_STATE_BUFFER_SIZE * 4;
        allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                                   &vme_context->res_pic_state_brc_read_buffer,
                                                   res_size,
                                                   "Pic State Brc_read");
        if (!allocate_flag)
            goto failed_allocation;

        res_size = VP9_PIC_STATE_BUFFER_SIZE * 4;
        allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                                   &vme_context->res_pic_state_brc_write_hfw_read_buffer,
                                                   res_size,
                                                   "Pic State Brc_write Hfw_Read");
        if (!allocate_flag)
            goto failed_allocation;

        res_size = VP9_PIC_STATE_BUFFER_SIZE * 4;
        allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                                   &vme_context->res_pic_state_hfw_write_buffer,
                                                   res_size,
                                                   "Pic State Hfw Write");
        if (!allocate_flag)
            goto failed_allocation;

        res_size = VP9_SEGMENT_STATE_BUFFER_SIZE;
        allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                                   &vme_context->res_seg_state_brc_read_buffer,
                                                   res_size,
                                                   "Segment state brc_read");
        if (!allocate_flag)
            goto failed_allocation;

        res_size = VP9_SEGMENT_STATE_BUFFER_SIZE;
        allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                                   &vme_context->res_seg_state_brc_write_buffer,
                                                   res_size,
                                                   "Segment state brc_write");
        if (!allocate_flag)
            goto failed_allocation;

        res_size = VP9_BRC_BITSTREAM_SIZE_BUFFER_SIZE;
        allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                                   &vme_context->res_brc_bitstream_size_buffer,
                                                   res_size,
                                                   "Brc bitstream buffer");
        if (!allocate_flag)
            goto failed_allocation;

        res_size = VP9_HFW_BRC_DATA_BUFFER_SIZE;
        allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                                   &vme_context->res_brc_hfw_data_buffer,
                                                   res_size,
                                                   "mfw Brc data");
        if (!allocate_flag)
            goto failed_allocation;

        res_size = VP9_BRC_MMDK_PAK_BUFFER_SIZE;
        allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                                   &vme_context->res_brc_mmdk_pak_buffer,
                                                   res_size,
                                                   "Brc mmdk_pak");
        if (!allocate_flag)
            goto failed_allocation;
    }

    /* If the width/height of allocated buffer is greater than the expected,
     * it is unnecessary to allocate it again
     */
    if (vp9_state->res_width >= vp9_state->frame_width &&
        vp9_state->res_height >= vp9_state->frame_height) {

        return VA_STATUS_SUCCESS;
    }
    frame_width_in_sb = ALIGN(vp9_state->frame_width, 64) / 64;
    frame_height_in_sb = ALIGN(vp9_state->frame_height, 64) / 64;
    frame_sb_num  = frame_width_in_sb * frame_height_in_sb;

    i965_free_gpe_resource(&vme_context->res_hvd_line_buffer);
    res_size = frame_width_in_sb * 64;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_hvd_line_buffer,
                                               res_size,
                                               "VP9 hvd line line");
    if (!allocate_flag)
        goto failed_allocation;

    i965_free_gpe_resource(&vme_context->res_hvd_tile_line_buffer);
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_hvd_tile_line_buffer,
                                               res_size,
                                               "VP9 hvd tile_line line");
    if (!allocate_flag)
        goto failed_allocation;

    i965_free_gpe_resource(&vme_context->res_deblocking_filter_line_buffer);
    res_size = frame_width_in_sb * 18 * 64;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_deblocking_filter_line_buffer,
                                               res_size,
                                               "VP9 deblocking filter line");
    if (!allocate_flag)
        goto failed_allocation;

    i965_free_gpe_resource(&vme_context->res_deblocking_filter_tile_line_buffer);
    res_size = frame_width_in_sb * 18 * 64;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_deblocking_filter_tile_line_buffer,
                                               res_size,
                                               "VP9 deblocking tile line");
    if (!allocate_flag)
        goto failed_allocation;

    i965_free_gpe_resource(&vme_context->res_deblocking_filter_tile_col_buffer);
    res_size = frame_height_in_sb * 17 * 64;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_deblocking_filter_tile_col_buffer,
                                               res_size,
                                               "VP9 deblocking tile col");
    if (!allocate_flag)
        goto failed_allocation;

    i965_free_gpe_resource(&vme_context->res_metadata_line_buffer);
    res_size = frame_width_in_sb * 5 * 64;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_metadata_line_buffer,
                                               res_size,
                                               "VP9 metadata line");
    if (!allocate_flag)
        goto failed_allocation;

    i965_free_gpe_resource(&vme_context->res_metadata_tile_line_buffer);
    res_size = frame_width_in_sb * 5 * 64;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_metadata_tile_line_buffer,
                                               res_size,
                                               "VP9 metadata tile line");
    if (!allocate_flag)
        goto failed_allocation;

    i965_free_gpe_resource(&vme_context->res_metadata_tile_col_buffer);
    res_size = frame_height_in_sb * 5 * 64;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_metadata_tile_col_buffer,
                                               res_size,
                                               "VP9 metadata tile col");
    if (!allocate_flag)
        goto failed_allocation;

    i965_free_gpe_resource(&vme_context->res_prob_buffer);
    res_size = 2048;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_prob_buffer,
                                               res_size,
                                               "VP9 prob");
    if (!allocate_flag)
        goto failed_allocation;

    i965_free_gpe_resource(&vme_context->res_segmentid_buffer);
    res_size = frame_sb_num * 64;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_segmentid_buffer,
                                               res_size,
                                               "VP9 segment id");
    if (!allocate_flag)
        goto failed_allocation;

    i965_zero_gpe_resource(&vme_context->res_segmentid_buffer);

    i965_free_gpe_resource(&vme_context->res_prob_delta_buffer);
    res_size = 29 * 64;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_prob_delta_buffer,
                                               res_size,
                                               "VP9 prob delta");
    if (!allocate_flag)
        goto failed_allocation;

    i965_zero_gpe_resource(&vme_context->res_segmentid_buffer);

    i965_free_gpe_resource(&vme_context->res_prob_delta_buffer);
    res_size = 29 * 64;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_prob_delta_buffer,
                                               res_size,
                                               "VP9 prob delta");
    if (!allocate_flag)
        goto failed_allocation;

    i965_free_gpe_resource(&vme_context->res_compressed_input_buffer);
    res_size = 32 * 64;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_compressed_input_buffer,
                                               res_size,
                                               "VP9 compressed_input buffer");
    if (!allocate_flag)
        goto failed_allocation;

    i965_free_gpe_resource(&vme_context->res_prob_counter_buffer);
    res_size = 193 * 64;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_prob_counter_buffer,
                                               res_size,
                                               "VP9 prob counter");
    if (!allocate_flag)
        goto failed_allocation;

    i965_free_gpe_resource(&vme_context->res_tile_record_streamout_buffer);
    res_size = frame_sb_num * 64;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_tile_record_streamout_buffer,
                                               res_size,
                                               "VP9 tile record stream_out");
    if (!allocate_flag)
        goto failed_allocation;

    i965_free_gpe_resource(&vme_context->res_cu_stat_streamout_buffer);
    res_size = frame_sb_num * 64;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_cu_stat_streamout_buffer,
                                               res_size,
                                               "VP9 CU stat stream_out");
    if (!allocate_flag)
        goto failed_allocation;

    width = vp9_state->downscaled_width_4x_in_mb * 32;
    height = vp9_state->downscaled_height_4x_in_mb * 16;
    i965_free_gpe_resource(&vme_context->s4x_memv_data_buffer);
    allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                                  &vme_context->s4x_memv_data_buffer,
                                                  width, height,
                                                  ALIGN(width, 64),
                                                  "VP9 4x MEMV data");
    if (!allocate_flag)
        goto failed_allocation;

    width = vp9_state->downscaled_width_4x_in_mb * 8;
    height = vp9_state->downscaled_height_4x_in_mb * 16;
    i965_free_gpe_resource(&vme_context->s4x_memv_distortion_buffer);
    allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                                  &vme_context->s4x_memv_distortion_buffer,
                                                  width, height,
                                                  ALIGN(width, 64),
                                                  "VP9 4x MEMV distorion");
    if (!allocate_flag)
        goto failed_allocation;

    width = ALIGN(vp9_state->downscaled_width_16x_in_mb * 32, 64);
    height = vp9_state->downscaled_height_16x_in_mb * 16;
    i965_free_gpe_resource(&vme_context->s16x_memv_data_buffer);
    allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                                  &vme_context->s16x_memv_data_buffer,
                                                  width, height,
                                                  width,
                                                  "VP9 16x MEMV data");
    if (!allocate_flag)
        goto failed_allocation;

    width = vp9_state->frame_width_in_mb * 16;
    height = vp9_state->frame_height_in_mb * 8;
    i965_free_gpe_resource(&vme_context->res_output_16x16_inter_modes);
    allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                                  &vme_context->res_output_16x16_inter_modes,
                                                  width, height,
                                                  ALIGN(width, 64),
                                                  "VP9 output inter_mode");
    if (!allocate_flag)
        goto failed_allocation;

    res_size = vp9_state->frame_width_in_mb * vp9_state->frame_height_in_mb *
               16 * 4;
    for (i = 0; i < 2; i++) {
        i965_free_gpe_resource(&vme_context->res_mode_decision[i]);
        allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                                   &vme_context->res_mode_decision[i],
                                                   res_size,
                                                   "VP9 mode decision");
        if (!allocate_flag)
            goto failed_allocation;

    }

    res_size = frame_sb_num * 9 * 64;
    for (i = 0; i < 2; i++) {
        i965_free_gpe_resource(&vme_context->res_mv_temporal_buffer[i]);
        allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                                   &vme_context->res_mv_temporal_buffer[i],
                                                   res_size,
                                                   "VP9 temporal mv");
        if (!allocate_flag)
            goto failed_allocation;
    }

    vp9_state->mb_data_offset = ALIGN(frame_sb_num * 16, 4096) + 4096;
    res_size = vp9_state->mb_data_offset + frame_sb_num * 64 * 64 + 1000;
    i965_free_gpe_resource(&vme_context->res_mb_code_surface);
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_mb_code_surface,
                                               ALIGN(res_size, 4096),
                                               "VP9 mb_code surface");
    if (!allocate_flag)
        goto failed_allocation;

    res_size = 128;
    i965_free_gpe_resource(&vme_context->res_pak_uncompressed_input_buffer);
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_pak_uncompressed_input_buffer,
                                               ALIGN(res_size, 4096),
                                               "VP9 pak_uncompressed_input");
    if (!allocate_flag)
        goto failed_allocation;

    if (!vme_context->frame_header_data) {
        /* allocate 512 bytes for generating the uncompressed header */
        vme_context->frame_header_data = calloc(1, 512);
    }

    vp9_state->res_width = vp9_state->frame_width;
    vp9_state->res_height = vp9_state->frame_height;

    return VA_STATUS_SUCCESS;

failed_allocation:
    return VA_STATUS_ERROR_ALLOCATION_FAILED;
}

static void
gen9_vp9_free_resources(struct gen9_encoder_context_vp9 *vme_context)
{
    int i;
    struct gen9_vp9_state *vp9_state = (struct gen9_vp9_state *) vme_context->enc_priv_state;

    if (vp9_state->brc_enabled) {
        i965_free_gpe_resource(&vme_context->res_brc_history_buffer);
        i965_free_gpe_resource(&vme_context->res_brc_const_data_buffer);
        i965_free_gpe_resource(&vme_context->res_brc_mbenc_curbe_write_buffer);
        i965_free_gpe_resource(&vme_context->res_pic_state_brc_read_buffer);
        i965_free_gpe_resource(&vme_context->res_pic_state_brc_write_hfw_read_buffer);
        i965_free_gpe_resource(&vme_context->res_pic_state_hfw_write_buffer);
        i965_free_gpe_resource(&vme_context->res_seg_state_brc_read_buffer);
        i965_free_gpe_resource(&vme_context->res_seg_state_brc_write_buffer);
        i965_free_gpe_resource(&vme_context->res_brc_bitstream_size_buffer);
        i965_free_gpe_resource(&vme_context->res_brc_hfw_data_buffer);
        i965_free_gpe_resource(&vme_context->res_brc_mmdk_pak_buffer);
    }

    i965_free_gpe_resource(&vme_context->res_hvd_line_buffer);
    i965_free_gpe_resource(&vme_context->res_hvd_tile_line_buffer);
    i965_free_gpe_resource(&vme_context->res_deblocking_filter_line_buffer);
    i965_free_gpe_resource(&vme_context->res_deblocking_filter_tile_line_buffer);
    i965_free_gpe_resource(&vme_context->res_deblocking_filter_tile_col_buffer);
    i965_free_gpe_resource(&vme_context->res_metadata_line_buffer);
    i965_free_gpe_resource(&vme_context->res_metadata_tile_line_buffer);
    i965_free_gpe_resource(&vme_context->res_metadata_tile_col_buffer);
    i965_free_gpe_resource(&vme_context->res_prob_buffer);
    i965_free_gpe_resource(&vme_context->res_segmentid_buffer);
    i965_free_gpe_resource(&vme_context->res_prob_delta_buffer);
    i965_free_gpe_resource(&vme_context->res_prob_counter_buffer);
    i965_free_gpe_resource(&vme_context->res_tile_record_streamout_buffer);
    i965_free_gpe_resource(&vme_context->res_cu_stat_streamout_buffer);
    i965_free_gpe_resource(&vme_context->s4x_memv_data_buffer);
    i965_free_gpe_resource(&vme_context->s4x_memv_distortion_buffer);
    i965_free_gpe_resource(&vme_context->s16x_memv_data_buffer);
    i965_free_gpe_resource(&vme_context->res_output_16x16_inter_modes);
    for (i = 0; i < 2; i++) {
        i965_free_gpe_resource(&vme_context->res_mode_decision[i]);
    }

    for (i = 0; i < 2; i++) {
        i965_free_gpe_resource(&vme_context->res_mv_temporal_buffer[i]);
    }

    i965_free_gpe_resource(&vme_context->res_compressed_input_buffer);
    i965_free_gpe_resource(&vme_context->res_mb_code_surface);
    i965_free_gpe_resource(&vme_context->res_pak_uncompressed_input_buffer);

    if (vme_context->frame_header_data) {
        free(vme_context->frame_header_data);
        vme_context->frame_header_data = NULL;
    }
    return;
}

static void
gen9_init_media_object_walker_parameter(struct intel_encoder_context *encoder_context,
                                        struct gpe_encoder_kernel_walker_parameter *kernel_walker_param,
                                        struct gpe_media_object_walker_parameter *walker_param)
{
    memset(walker_param, 0, sizeof(*walker_param));

    walker_param->use_scoreboard = kernel_walker_param->use_scoreboard;

    walker_param->block_resolution.x = kernel_walker_param->resolution_x;
    walker_param->block_resolution.y = kernel_walker_param->resolution_y;

    walker_param->global_resolution.x = kernel_walker_param->resolution_x;
    walker_param->global_resolution.y = kernel_walker_param->resolution_y;

    walker_param->global_outer_loop_stride.x = kernel_walker_param->resolution_x;
    walker_param->global_outer_loop_stride.y = 0;

    walker_param->global_inner_loop_unit.x = 0;
    walker_param->global_inner_loop_unit.y = kernel_walker_param->resolution_y;

    walker_param->local_loop_exec_count = 0xFFFF;  //MAX VALUE
    walker_param->global_loop_exec_count = 0xFFFF;  //MAX VALUE

    if (kernel_walker_param->no_dependency) {
        walker_param->scoreboard_mask = 0;
        walker_param->use_scoreboard = 0;
        // Raster scan walking pattern
        walker_param->local_outer_loop_stride.x = 0;
        walker_param->local_outer_loop_stride.y = 1;
        walker_param->local_inner_loop_unit.x = 1;
        walker_param->local_inner_loop_unit.y = 0;
        walker_param->local_end.x = kernel_walker_param->resolution_x - 1;
        walker_param->local_end.y = 0;
    } else {
        walker_param->local_end.x = 0;
        walker_param->local_end.y = 0;

        if (kernel_walker_param->walker_degree == VP9_45Z_DEGREE) {
            // 45z degree
            walker_param->scoreboard_mask = 0x0F;

            walker_param->global_loop_exec_count = 0x3FF;
            walker_param->local_loop_exec_count = 0x3FF;

            walker_param->global_resolution.x = (unsigned int)(kernel_walker_param->resolution_x / 2.f) + 1;
            walker_param->global_resolution.y = 2 * kernel_walker_param->resolution_y;

            walker_param->global_start.x = 0;
            walker_param->global_start.y = 0;

            walker_param->global_outer_loop_stride.x = walker_param->global_resolution.x;
            walker_param->global_outer_loop_stride.y = 0;

            walker_param->global_inner_loop_unit.x = 0;
            walker_param->global_inner_loop_unit.y = walker_param->global_resolution.y;

            walker_param->block_resolution.x = walker_param->global_resolution.x;
            walker_param->block_resolution.y = walker_param->global_resolution.y;

            walker_param->local_start.x = 0;
            walker_param->local_start.y = 0;

            walker_param->local_outer_loop_stride.x = 1;
            walker_param->local_outer_loop_stride.y = 0;

            walker_param->local_inner_loop_unit.x = -1;
            walker_param->local_inner_loop_unit.y = 4;

            walker_param->middle_loop_extra_steps = 3;
            walker_param->mid_loop_unit_x = 0;
            walker_param->mid_loop_unit_y = 1;
        } else {
            // 26 degree
            walker_param->scoreboard_mask = 0x0F;
            walker_param->local_outer_loop_stride.x = 1;
            walker_param->local_outer_loop_stride.y = 0;
            walker_param->local_inner_loop_unit.x = -2;
            walker_param->local_inner_loop_unit.y = 1;
        }
    }
}

static void
gen9_run_kernel_media_object(VADriverContextP ctx,
                             struct intel_encoder_context *encoder_context,
                             struct i965_gpe_context *gpe_context,
                             int media_function,
                             struct gpe_media_object_parameter *param)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct vp9_encode_status_buffer_internal *status_buffer;
    struct gen9_vp9_state *vp9_state;
    struct gpe_mi_store_data_imm_parameter mi_store_data_imm;

    vp9_state = (struct gen9_vp9_state *)(encoder_context->enc_priv_state);
    if (!vp9_state || !batch)
        return;

    intel_batchbuffer_start_atomic(batch, 0x1000);

    status_buffer = &(vp9_state->status_buffer);
    memset(&mi_store_data_imm, 0, sizeof(mi_store_data_imm));
    mi_store_data_imm.bo = status_buffer->bo;
    mi_store_data_imm.offset = status_buffer->media_index_offset;
    mi_store_data_imm.dw0 = media_function;
    gen8_gpe_mi_store_data_imm(ctx, batch, &mi_store_data_imm);

    intel_batchbuffer_emit_mi_flush(batch);
    gen9_gpe_pipeline_setup(ctx, gpe_context, batch);
    gen8_gpe_media_object(ctx, gpe_context, batch, param);
    gen8_gpe_media_state_flush(ctx, gpe_context, batch);

    gen9_gpe_pipeline_end(ctx, gpe_context, batch);

    intel_batchbuffer_end_atomic(batch);

    intel_batchbuffer_flush(batch);
}

static void
gen9_run_kernel_media_object_walker(VADriverContextP ctx,
                                    struct intel_encoder_context *encoder_context,
                                    struct i965_gpe_context *gpe_context,
                                    int media_function,
                                    struct gpe_media_object_walker_parameter *param)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct vp9_encode_status_buffer_internal *status_buffer;
    struct gen9_vp9_state *vp9_state;
    struct gpe_mi_store_data_imm_parameter mi_store_data_imm;

    vp9_state = (struct gen9_vp9_state *)(encoder_context->enc_priv_state);
    if (!vp9_state || !batch)
        return;

    intel_batchbuffer_start_atomic(batch, 0x1000);

    intel_batchbuffer_emit_mi_flush(batch);

    status_buffer = &(vp9_state->status_buffer);
    memset(&mi_store_data_imm, 0, sizeof(mi_store_data_imm));
    mi_store_data_imm.bo = status_buffer->bo;
    mi_store_data_imm.offset = status_buffer->media_index_offset;
    mi_store_data_imm.dw0 = media_function;
    gen8_gpe_mi_store_data_imm(ctx, batch, &mi_store_data_imm);

    gen9_gpe_pipeline_setup(ctx, gpe_context, batch);
    gen8_gpe_media_object_walker(ctx, gpe_context, batch, param);
    gen8_gpe_media_state_flush(ctx, gpe_context, batch);

    gen9_gpe_pipeline_end(ctx, gpe_context, batch);

    intel_batchbuffer_end_atomic(batch);

    intel_batchbuffer_flush(batch);
}

static
void gen9_vp9_set_curbe_brc(VADriverContextP ctx,
                            struct encode_state *encode_state,
                            struct i965_gpe_context *gpe_context,
                            struct intel_encoder_context *encoder_context,
                            struct gen9_vp9_brc_curbe_param *param)
{
    VAEncSequenceParameterBufferVP9 *seq_param;
    VAEncPictureParameterBufferVP9  *pic_param;
    VAEncMiscParameterTypeVP9PerSegmantParam *segment_param;
    vp9_brc_curbe_data      *cmd;
    double                  dbps_ratio, dInputBitsPerFrame;
    struct gen9_vp9_state *vp9_state;

    vp9_state = (struct gen9_vp9_state *) encoder_context->enc_priv_state;

    pic_param      = param->ppic_param;
    seq_param      = param->pseq_param;
    segment_param  = param->psegment_param;

    cmd = i965_gpe_context_map_curbe(gpe_context);

    if (!cmd)
        return;

    memset(cmd, 0, sizeof(vp9_brc_curbe_data));

    if (!vp9_state->dys_enabled) {
        cmd->dw0.frame_width  = pic_param->frame_width_src;
        cmd->dw0.frame_height = pic_param->frame_height_src;
    } else {
        cmd->dw0.frame_width  = pic_param->frame_width_dst;
        cmd->dw0.frame_height = pic_param->frame_height_dst;
    }

    cmd->dw1.frame_type           = vp9_state->picture_coding_type;
    cmd->dw1.segmentation_enable  = 0;
    cmd->dw1.ref_frame_flags      = vp9_state->ref_frame_flag;
    cmd->dw1.num_tlevels          = 1;

    switch (param->media_state_type) {
    case VP9_MEDIA_STATE_BRC_INIT_RESET: {
        cmd->dw3.max_level_ratiot0 = 0;
        cmd->dw3.max_level_ratiot1 = 0;
        cmd->dw3.max_level_ratiot2 = 0;
        cmd->dw3.max_level_ratiot3 = 0;

        cmd->dw4.profile_level_max_frame    = seq_param->max_frame_width *
                                              seq_param->max_frame_height;
        cmd->dw5.init_buf_fullness         = vp9_state->init_vbv_buffer_fullness_in_bit;
        cmd->dw6.buf_size                  = vp9_state->vbv_buffer_size_in_bit;
        cmd->dw7.target_bit_rate           = (vp9_state->target_bit_rate  + VP9_BRC_KBPS - 1) / VP9_BRC_KBPS *
                                             VP9_BRC_KBPS;
        cmd->dw8.max_bit_rate           = (vp9_state->max_bit_rate  + VP9_BRC_KBPS - 1) / VP9_BRC_KBPS *
                                          VP9_BRC_KBPS;
        cmd->dw9.min_bit_rate           = (vp9_state->min_bit_rate  + VP9_BRC_KBPS - 1) / VP9_BRC_KBPS *
                                          VP9_BRC_KBPS;
        cmd->dw10.frame_ratem           = vp9_state->framerate.num;
        cmd->dw11.frame_rated           = vp9_state->framerate.den;

        cmd->dw14.avbr_accuracy         = 30;
        cmd->dw14.avbr_convergence      = 150;

        if (encoder_context->rate_control_mode == VA_RC_CBR) {
            cmd->dw12.brc_flag    = BRC_KERNEL_CBR;
            cmd->dw8.max_bit_rate  = cmd->dw7.target_bit_rate;
            cmd->dw9.min_bit_rate  = 0;
        } else if (encoder_context->rate_control_mode == VA_RC_VBR) {
            cmd->dw12.brc_flag    = BRC_KERNEL_VBR;
        } else {
            cmd->dw12.brc_flag = BRC_KERNEL_CQL;
            cmd->dw16.cq_level = 30;
        }
        cmd->dw12.gopp = seq_param->intra_period - 1;

        cmd->dw13.init_frame_width   = pic_param->frame_width_src;
        cmd->dw13.init_frame_height   = pic_param->frame_height_src;

        cmd->dw15.min_qp          = 0;
        cmd->dw15.max_qp          = 255;

        cmd->dw16.cq_level            = 30;

        cmd->dw17.enable_dynamic_scaling = vp9_state->dys_in_use;
        cmd->dw17.brc_overshoot_cbr_pct = 150;

        dInputBitsPerFrame = (double)cmd->dw8.max_bit_rate * (double)vp9_state->framerate.den / (double)vp9_state->framerate.num;
        dbps_ratio         = dInputBitsPerFrame / ((double)vp9_state->vbv_buffer_size_in_bit / 30.0);
        if (dbps_ratio < 0.1)
            dbps_ratio = 0.1;
        if (dbps_ratio > 3.5)
            dbps_ratio = 3.5;

        *param->pbrc_init_reset_buf_size_in_bits  = cmd->dw6.buf_size;
        *param->pbrc_init_reset_input_bits_per_frame  = dInputBitsPerFrame;
        *param->pbrc_init_current_target_buf_full_in_bits = cmd->dw6.buf_size >> 1;

        cmd->dw18.pframe_deviation_threshold0 = (uint32_t)(-50 * pow(0.90, dbps_ratio));
        cmd->dw18.pframe_deviation_threshold1  = (uint32_t)(-50 * pow(0.66, dbps_ratio));
        cmd->dw18.pframe_deviation_threshold2  = (uint32_t)(-50 * pow(0.46, dbps_ratio));
        cmd->dw18.pframe_deviation_threshold3  = (uint32_t)(-50 * pow(0.3, dbps_ratio));
        cmd->dw19.pframe_deviation_threshold4  = (uint32_t)(50 * pow(0.3, dbps_ratio));
        cmd->dw19.pframe_deviation_threshold5  = (uint32_t)(50 * pow(0.46, dbps_ratio));
        cmd->dw19.pframe_deviation_threshold6  = (uint32_t)(50 * pow(0.7, dbps_ratio));
        cmd->dw19.pframe_deviation_threshold7  = (uint32_t)(50 * pow(0.9, dbps_ratio));

        cmd->dw20.vbr_deviation_threshold0     = (uint32_t)(-50 * pow(0.9, dbps_ratio));
        cmd->dw20.vbr_deviation_threshold1     = (uint32_t)(-50 * pow(0.7, dbps_ratio));
        cmd->dw20.vbr_deviation_threshold2     = (uint32_t)(-50 * pow(0.5, dbps_ratio));
        cmd->dw20.vbr_deviation_threshold3     = (uint32_t)(-50 * pow(0.3, dbps_ratio));
        cmd->dw21.vbr_deviation_threshold4     = (uint32_t)(100 * pow(0.4, dbps_ratio));
        cmd->dw21.vbr_deviation_threshold5     = (uint32_t)(100 * pow(0.5, dbps_ratio));
        cmd->dw21.vbr_deviation_threshold6     = (uint32_t)(100 * pow(0.75, dbps_ratio));
        cmd->dw21.vbr_deviation_threshold7     = (uint32_t)(100 * pow(0.9, dbps_ratio));

        cmd->dw22.kframe_deviation_threshold0  = (uint32_t)(-50 * pow(0.8, dbps_ratio));
        cmd->dw22.kframe_deviation_threshold1  = (uint32_t)(-50 * pow(0.6, dbps_ratio));
        cmd->dw22.kframe_deviation_threshold2  = (uint32_t)(-50 * pow(0.34, dbps_ratio));
        cmd->dw22.kframe_deviation_threshold3  = (uint32_t)(-50 * pow(0.2, dbps_ratio));
        cmd->dw23.kframe_deviation_threshold4  = (uint32_t)(50 * pow(0.2, dbps_ratio));
        cmd->dw23.kframe_deviation_threshold5  = (uint32_t)(50 * pow(0.4, dbps_ratio));
        cmd->dw23.kframe_deviation_threshold6  = (uint32_t)(50 * pow(0.66, dbps_ratio));
        cmd->dw23.kframe_deviation_threshold7  = (uint32_t)(50 * pow(0.9, dbps_ratio));

        break;
    }
    case VP9_MEDIA_STATE_BRC_UPDATE: {
        cmd->dw15.min_qp          = 0;
        cmd->dw15.max_qp          = 255;

        cmd->dw25.frame_number    = param->frame_number;

        // Used in dynamic scaling. set to zero for now
        cmd->dw27.hrd_buffer_fullness_upper_limit = 0;
        cmd->dw28.hrd_buffer_fullness_lower_limit = 0;

        if (pic_param->pic_flags.bits.segmentation_enabled) {
            cmd->dw32.seg_delta_qp0              = segment_param->seg_data[0].segment_qindex_delta;
            cmd->dw32.seg_delta_qp1              = segment_param->seg_data[1].segment_qindex_delta;
            cmd->dw32.seg_delta_qp2              = segment_param->seg_data[2].segment_qindex_delta;
            cmd->dw32.seg_delta_qp3              = segment_param->seg_data[3].segment_qindex_delta;

            cmd->dw33.seg_delta_qp4              = segment_param->seg_data[4].segment_qindex_delta;
            cmd->dw33.seg_delta_qp5              = segment_param->seg_data[5].segment_qindex_delta;
            cmd->dw33.seg_delta_qp6              = segment_param->seg_data[6].segment_qindex_delta;
            cmd->dw33.seg_delta_qp7              = segment_param->seg_data[7].segment_qindex_delta;
        }

        //cmd->dw34.temporal_id                = pPicParams->temporal_idi;
        cmd->dw34.temporal_id                = 0;
        cmd->dw34.multi_ref_qp_check         = param->multi_ref_qp_check;

        cmd->dw35.max_num_pak_passes         = param->brc_num_pak_passes;
        cmd->dw35.sync_async                 = 0;
        cmd->dw35.mbrc                       = param->mbbrc_enabled;
        if (*param->pbrc_init_current_target_buf_full_in_bits >
            ((double)(*param->pbrc_init_reset_buf_size_in_bits))) {
            *param->pbrc_init_current_target_buf_full_in_bits -=
                (double)(*param->pbrc_init_reset_buf_size_in_bits);
            cmd->dw35.overflow = 1;
        } else
            cmd->dw35.overflow = 0;

        cmd->dw24.target_size                 = (uint32_t)(*param->pbrc_init_current_target_buf_full_in_bits);

        cmd->dw36.segmentation               = pic_param->pic_flags.bits.segmentation_enabled;

        *param->pbrc_init_current_target_buf_full_in_bits += *param->pbrc_init_reset_input_bits_per_frame;

        cmd->dw38.qdelta_ydc  = pic_param->luma_dc_qindex_delta;
        cmd->dw38.qdelta_uvdc = pic_param->chroma_dc_qindex_delta;
        cmd->dw38.qdelta_uvac = pic_param->chroma_ac_qindex_delta;

        break;
    }
    case VP9_MEDIA_STATE_ENC_I_FRAME_DIST:
        cmd->dw2.intra_mode_disable        = 0;
        break;
    default:
        break;
    }

    cmd->dw48.brc_y4x_input_bti                = VP9_BTI_BRC_SRCY4X_G9;
    cmd->dw49.brc_vme_coarse_intra_input_bti   = VP9_BTI_BRC_VME_COARSE_INTRA_G9;
    cmd->dw50.brc_history_buffer_bti           = VP9_BTI_BRC_HISTORY_G9;
    cmd->dw51.brc_const_data_input_bti         = VP9_BTI_BRC_CONSTANT_DATA_G9;
    cmd->dw52.brc_distortion_bti               = VP9_BTI_BRC_DISTORTION_G9;
    cmd->dw53.brc_mmdk_pak_output_bti          = VP9_BTI_BRC_MMDK_PAK_OUTPUT_G9;
    cmd->dw54.brc_enccurbe_input_bti           = VP9_BTI_BRC_MBENC_CURBE_INPUT_G9;
    cmd->dw55.brc_enccurbe_output_bti          = VP9_BTI_BRC_MBENC_CURBE_OUTPUT_G9;
    cmd->dw56.brc_pic_state_input_bti          = VP9_BTI_BRC_PIC_STATE_INPUT_G9;
    cmd->dw57.brc_pic_state_output_bti         = VP9_BTI_BRC_PIC_STATE_OUTPUT_G9;
    cmd->dw58.brc_seg_state_input_bti          = VP9_BTI_BRC_SEGMENT_STATE_INPUT_G9;
    cmd->dw59.brc_seg_state_output_bti         = VP9_BTI_BRC_SEGMENT_STATE_OUTPUT_G9;
    cmd->dw60.brc_bitstream_size_data_bti      = VP9_BTI_BRC_BITSTREAM_SIZE_G9;
    cmd->dw61.brc_hfw_data_output_bti          = VP9_BTI_BRC_HFW_DATA_G9;

    i965_gpe_context_unmap_curbe(gpe_context);
    return;
}

static void
gen9_brc_init_reset_add_surfaces_vp9(VADriverContextP ctx,
                                     struct encode_state *encode_state,
                                     struct intel_encoder_context *encoder_context,
                                     struct i965_gpe_context *gpe_context)
{
    struct gen9_encoder_context_vp9 *vme_context = encoder_context->vme_context;

    gen9_add_buffer_gpe_surface(ctx,
                                gpe_context,
                                &vme_context->res_brc_history_buffer,
                                0,
                                vme_context->res_brc_history_buffer.size,
                                0,
                                VP9_BTI_BRC_HISTORY_G9);

    gen9_add_buffer_2d_gpe_surface(ctx,
                                   gpe_context,
                                   &vme_context->s4x_memv_distortion_buffer,
                                   1,
                                   I965_SURFACEFORMAT_R8_UNORM,
                                   VP9_BTI_BRC_DISTORTION_G9);
}

/* The function related with BRC */
static VAStatus
gen9_vp9_brc_init_reset_kernel(VADriverContextP ctx,
                               struct encode_state *encode_state,
                               struct intel_encoder_context *encoder_context)
{
    struct gen9_encoder_context_vp9 *vme_context = encoder_context->vme_context;
    struct vp9_brc_context *brc_context = &vme_context->brc_context;
    struct gpe_media_object_parameter media_object_param;
    struct i965_gpe_context *gpe_context;
    int gpe_index = VP9_BRC_INIT;
    int media_function = VP9_MEDIA_STATE_BRC_INIT_RESET;
    struct gen9_vp9_brc_curbe_param                brc_initreset_curbe;
    VAEncPictureParameterBufferVP9 *pic_param;
    struct gen9_vp9_state *vp9_state;

    vp9_state = (struct gen9_vp9_state *) encoder_context->enc_priv_state;

    if (!vp9_state || !vp9_state->pic_param)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    pic_param = vp9_state->pic_param;

    if (vp9_state->brc_inited)
        gpe_index = VP9_BRC_RESET;

    gpe_context = &brc_context->gpe_contexts[gpe_index];

    gen8_gpe_context_init(ctx, gpe_context);
    gen9_gpe_reset_binding_table(ctx, gpe_context);

    brc_initreset_curbe.media_state_type    = media_function;
    brc_initreset_curbe.curr_frame          = pic_param->reconstructed_frame;
    brc_initreset_curbe.ppic_param          = vp9_state->pic_param;
    brc_initreset_curbe.pseq_param          = vp9_state->seq_param;
    brc_initreset_curbe.psegment_param      = vp9_state->segment_param;
    brc_initreset_curbe.frame_width         = vp9_state->frame_width;
    brc_initreset_curbe.frame_height        = vp9_state->frame_height;
    brc_initreset_curbe.pbrc_init_current_target_buf_full_in_bits =
        &vp9_state->brc_init_current_target_buf_full_in_bits;
    brc_initreset_curbe.pbrc_init_reset_buf_size_in_bits =
        &vp9_state->brc_init_reset_buf_size_in_bits;
    brc_initreset_curbe.pbrc_init_reset_input_bits_per_frame =
        &vp9_state->brc_init_reset_input_bits_per_frame;
    brc_initreset_curbe.picture_coding_type  = vp9_state->picture_coding_type;
    brc_initreset_curbe.initbrc            = !vp9_state->brc_inited;
    brc_initreset_curbe.mbbrc_enabled      = 0;
    brc_initreset_curbe.ref_frame_flag      = vp9_state->ref_frame_flag;

    vme_context->pfn_set_curbe_brc(ctx, encode_state,
                                   gpe_context,
                                   encoder_context,
                                   &brc_initreset_curbe);

    gen9_brc_init_reset_add_surfaces_vp9(ctx, encode_state, encoder_context, gpe_context);
    gen8_gpe_setup_interface_data(ctx, gpe_context);

    memset(&media_object_param, 0, sizeof(media_object_param));
    gen9_run_kernel_media_object(ctx, encoder_context, gpe_context, media_function, &media_object_param);

    return VA_STATUS_SUCCESS;
}

static void
gen9_brc_intra_dist_add_surfaces_vp9(VADriverContextP ctx,
                                     struct encode_state *encode_state,
                                     struct intel_encoder_context *encoder_context,
                                     struct i965_gpe_context *gpe_context)
{
    struct gen9_encoder_context_vp9 *vme_context = encoder_context->vme_context;

    struct object_surface *obj_surface;
    struct gen9_surface_vp9 *vp9_priv_surface;

    /* sScaled4xSurface surface */
    obj_surface = encode_state->reconstructed_object;

    vp9_priv_surface = (struct gen9_surface_vp9 *)(obj_surface->private_data);

    obj_surface = vp9_priv_surface->scaled_4x_surface_obj;
    gen9_add_2d_gpe_surface(ctx, gpe_context,
                            obj_surface,
                            0, 1,
                            I965_SURFACEFORMAT_R8_UNORM,
                            VP9_BTI_BRC_SRCY4X_G9
                           );

    gen9_add_adv_gpe_surface(ctx, gpe_context,
                             obj_surface,
                             VP9_BTI_BRC_VME_COARSE_INTRA_G9);

    gen9_add_buffer_2d_gpe_surface(ctx,
                                   gpe_context,
                                   &vme_context->s4x_memv_distortion_buffer,
                                   1,
                                   I965_SURFACEFORMAT_R8_UNORM,
                                   VP9_BTI_BRC_DISTORTION_G9);

    return;
}

/* The function related with BRC */
static VAStatus
gen9_vp9_brc_intra_dist_kernel(VADriverContextP ctx,
                               struct encode_state *encode_state,
                               struct intel_encoder_context *encoder_context)
{
    struct gen9_encoder_context_vp9 *vme_context = encoder_context->vme_context;
    struct vp9_brc_context *brc_context = &vme_context->brc_context;
    struct i965_gpe_context *gpe_context;
    int gpe_index = VP9_BRC_INTRA_DIST;
    int media_function = VP9_MEDIA_STATE_ENC_I_FRAME_DIST;
    struct gen9_vp9_brc_curbe_param                brc_intra_dist_curbe;
    VAEncPictureParameterBufferVP9 *pic_param;
    struct gen9_vp9_state *vp9_state;
    struct gpe_media_object_walker_parameter media_object_walker_param;
    struct gpe_encoder_kernel_walker_parameter kernel_walker_param;

    vp9_state = (struct gen9_vp9_state *) encoder_context->enc_priv_state;

    if (!vp9_state || !vp9_state->pic_param)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    pic_param = vp9_state->pic_param;

    gpe_context = &brc_context->gpe_contexts[gpe_index];

    gen8_gpe_context_init(ctx, gpe_context);
    gen9_gpe_reset_binding_table(ctx, gpe_context);

    brc_intra_dist_curbe.media_state_type    = media_function;
    brc_intra_dist_curbe.curr_frame          = pic_param->reconstructed_frame;
    brc_intra_dist_curbe.ppic_param          = vp9_state->pic_param;
    brc_intra_dist_curbe.pseq_param          = vp9_state->seq_param;
    brc_intra_dist_curbe.psegment_param      = vp9_state->segment_param;
    brc_intra_dist_curbe.frame_width         = vp9_state->frame_width;
    brc_intra_dist_curbe.frame_height        = vp9_state->frame_height;
    brc_intra_dist_curbe.pbrc_init_current_target_buf_full_in_bits =
        &vp9_state->brc_init_current_target_buf_full_in_bits;
    brc_intra_dist_curbe.pbrc_init_reset_buf_size_in_bits =
        &vp9_state->brc_init_reset_buf_size_in_bits;
    brc_intra_dist_curbe.pbrc_init_reset_input_bits_per_frame =
        &vp9_state->brc_init_reset_input_bits_per_frame;
    brc_intra_dist_curbe.picture_coding_type  = vp9_state->picture_coding_type;
    brc_intra_dist_curbe.initbrc            = !vp9_state->brc_inited;
    brc_intra_dist_curbe.mbbrc_enabled      = 0;
    brc_intra_dist_curbe.ref_frame_flag      = vp9_state->ref_frame_flag;

    vme_context->pfn_set_curbe_brc(ctx, encode_state,
                                   gpe_context,
                                   encoder_context,
                                   &brc_intra_dist_curbe);

    /* zero distortion buffer */
    i965_zero_gpe_resource(&vme_context->s4x_memv_distortion_buffer);

    gen9_brc_intra_dist_add_surfaces_vp9(ctx, encode_state, encoder_context, gpe_context);
    gen8_gpe_setup_interface_data(ctx, gpe_context);

    memset(&kernel_walker_param, 0, sizeof(kernel_walker_param));
    kernel_walker_param.resolution_x = vp9_state->downscaled_width_4x_in_mb;
    kernel_walker_param.resolution_y = vp9_state->downscaled_height_4x_in_mb;
    kernel_walker_param.no_dependency = 1;

    gen9_init_media_object_walker_parameter(encoder_context, &kernel_walker_param, &media_object_walker_param);

    gen9_run_kernel_media_object_walker(ctx, encoder_context,
                                        gpe_context,
                                        media_function,
                                        &media_object_walker_param);

    return VA_STATUS_SUCCESS;
}

static void
intel_vp9enc_construct_picstate_batchbuf(VADriverContextP ctx,
                                         struct encode_state *encode_state,
                                         struct intel_encoder_context *encoder_context,
                                         struct i965_gpe_resource *gpe_resource)
{
    struct gen9_vp9_state *vp9_state;
    VAEncPictureParameterBufferVP9 *pic_param;
    int frame_width_minus1, frame_height_minus1;
    int is_lossless = 0;
    int is_intra_only = 0;
    unsigned int last_frame_type;
    unsigned int ref_flags;
    unsigned int use_prev_frame_mvs, adapt_flag;
    struct gen9_surface_vp9 *vp9_surface = NULL;
    struct object_surface *obj_surface = NULL;
    uint32_t scale_h = 0;
    uint32_t scale_w = 0;

    char *pdata;
    int i, j;
    unsigned int *cmd_ptr, cmd_value, tmp;

    pdata = i965_map_gpe_resource(gpe_resource);
    vp9_state = (struct gen9_vp9_state *) encoder_context->enc_priv_state;

    if (!vp9_state || !vp9_state->pic_param || !pdata)
        return;

    pic_param = vp9_state->pic_param;
    frame_width_minus1 = ALIGN(pic_param->frame_width_dst, 8) - 1;
    frame_height_minus1 = ALIGN(pic_param->frame_height_dst, 8) - 1;
    if ((pic_param->luma_ac_qindex == 0) &&
        (pic_param->luma_dc_qindex_delta == 0) &&
        (pic_param->chroma_ac_qindex_delta == 0) &&
        (pic_param->chroma_dc_qindex_delta == 0))
        is_lossless = 1;

    if (pic_param->pic_flags.bits.frame_type)
        is_intra_only = pic_param->pic_flags.bits.intra_only;

    last_frame_type = vp9_state->vp9_last_frame.frame_type;

    use_prev_frame_mvs = 0;
    if (pic_param->pic_flags.bits.frame_type == HCP_VP9_KEY_FRAME) {
        last_frame_type = 0;
        ref_flags = 0;
    } else {
        ref_flags = ((pic_param->ref_flags.bits.ref_arf_sign_bias << 9) |
                     (pic_param->ref_flags.bits.ref_gf_sign_bias << 8) |
                     (pic_param->ref_flags.bits.ref_last_sign_bias << 7)
                    );
        if (!pic_param->pic_flags.bits.error_resilient_mode &&
            (pic_param->frame_width_dst == vp9_state->vp9_last_frame.frame_width) &&
            (pic_param->frame_height_dst == vp9_state->vp9_last_frame.frame_height) &&
            !pic_param->pic_flags.bits.intra_only &&
            vp9_state->vp9_last_frame.show_frame &&
            ((vp9_state->vp9_last_frame.frame_type == HCP_VP9_INTER_FRAME) &&
             !vp9_state->vp9_last_frame.intra_only)
           )
            use_prev_frame_mvs = 1;
    }
    adapt_flag = 0;
    if (!pic_param->pic_flags.bits.error_resilient_mode &&
        !pic_param->pic_flags.bits.frame_parallel_decoding_mode)
        adapt_flag = 1;

    for (i = 0; i < 4; i++) {
        uint32_t non_first_pass;
        non_first_pass = 1;
        if (i == 0)
            non_first_pass = 0;

        cmd_ptr = (unsigned int *)(pdata + i * VP9_PIC_STATE_BUFFER_SIZE);

        *cmd_ptr++ = (HCP_VP9_PIC_STATE | (33 - 2));
        *cmd_ptr++ = (frame_height_minus1 << 16 |
                      frame_width_minus1);
        /* dw2 */
        *cmd_ptr++ = (0 << 31 |  /* disable segment_in */
                      0 << 30 | /* disable segment_out */
                      is_lossless << 29 | /* loseless */
                      (pic_param->pic_flags.bits.segmentation_enabled && pic_param->pic_flags.bits.segmentation_temporal_update) << 28 | /* temporal update */
                      (pic_param->pic_flags.bits.segmentation_enabled && pic_param->pic_flags.bits.segmentation_update_map) << 27 | /* temporal update */
                      (pic_param->pic_flags.bits.segmentation_enabled << 26) |
                      (pic_param->sharpness_level << 23) |
                      (pic_param->filter_level << 17) |
                      (pic_param->pic_flags.bits.frame_parallel_decoding_mode << 16) |
                      (pic_param->pic_flags.bits.error_resilient_mode << 15) |
                      (pic_param->pic_flags.bits.refresh_frame_context << 14) |
                      (last_frame_type << 13) |
                      (vp9_state->tx_mode == TX_MODE_SELECT) << 12 |
                      (pic_param->pic_flags.bits.comp_prediction_mode == REFERENCE_MODE_SELECT) << 11 |
                      (use_prev_frame_mvs) << 10 |
                      ref_flags |
                      (pic_param->pic_flags.bits.mcomp_filter_type << 4) |
                      (pic_param->pic_flags.bits.allow_high_precision_mv << 3) |
                      (is_intra_only << 2) |
                      (adapt_flag << 1) |
                      (pic_param->pic_flags.bits.frame_type) << 0);

        *cmd_ptr++ = ((0 << 28) | /* VP9Profile0 */
                      (0 << 24) | /* 8-bit depth */
                      (0 << 22) | /* only 420 format */
                      (0 << 0)  | /* sse statistics */
                      (pic_param->log2_tile_rows << 8) |
                      (pic_param->log2_tile_columns << 0));

        /* dw4..6 */
        if (pic_param->pic_flags.bits.frame_type &&
            !pic_param->pic_flags.bits.intra_only) {
            for (j = 0; j < 3; j++) {
                obj_surface = encode_state->reference_objects[j];
                scale_w = 0;
                scale_h = 0;
                if (obj_surface && obj_surface->private_data) {
                    vp9_surface = obj_surface->private_data;
                    scale_w = (vp9_surface->frame_width  << 14) / pic_param->frame_width_dst;
                    scale_h = (vp9_surface->frame_height << 14) / pic_param->frame_height_dst;
                    *cmd_ptr++ = (scale_w << 16 |
                                  scale_h);
                } else
                    *cmd_ptr++ = 0;
            }
        } else {
            *cmd_ptr++ = 0;
            *cmd_ptr++ = 0;
            *cmd_ptr++ = 0;
        }
        /* dw7..9 */
        for (j = 0; j < 3; j++) {
            obj_surface = encode_state->reference_objects[j];
            vp9_surface = NULL;

            if (obj_surface && obj_surface->private_data) {
                vp9_surface = obj_surface->private_data;
                *cmd_ptr++ = (vp9_surface->frame_height - 1) << 16 |
                             (vp9_surface->frame_width - 1);
            } else
                *cmd_ptr++ = 0;
        }
        /* dw10 */
        *cmd_ptr++ = 0;
        /* dw11 */
        *cmd_ptr++ = (1 << 1);
        *cmd_ptr++ = 0;

        /* dw13 */
        *cmd_ptr++ = ((1 << 25) | /* header insertation for VP9 */
                      (0 << 24) | /* tail insertation */
                      (pic_param->luma_ac_qindex << 16) |
                      0 /* compressed header bin count */);

        /* dw14 */
        tmp = intel_convert_sign_mag(pic_param->luma_dc_qindex_delta, 5);
        cmd_value = (tmp << 16);
        tmp = intel_convert_sign_mag(pic_param->chroma_dc_qindex_delta, 5);
        cmd_value |= (tmp << 8);
        tmp = intel_convert_sign_mag(pic_param->chroma_ac_qindex_delta, 5);
        cmd_value |= tmp;
        *cmd_ptr++ = cmd_value;

        tmp = intel_convert_sign_mag(pic_param->ref_lf_delta[0], 7);
        cmd_value = tmp;
        tmp = intel_convert_sign_mag(pic_param->ref_lf_delta[1], 7);
        cmd_value |= (tmp << 8);
        tmp = intel_convert_sign_mag(pic_param->ref_lf_delta[2], 7);
        cmd_value |= (tmp << 16);
        tmp = intel_convert_sign_mag(pic_param->ref_lf_delta[3], 7);
        cmd_value |= (tmp << 24);
        *cmd_ptr++ = cmd_value;

        /* dw16 */
        tmp = intel_convert_sign_mag(pic_param->mode_lf_delta[0], 7);
        cmd_value = tmp;
        tmp = intel_convert_sign_mag(pic_param->mode_lf_delta[1], 7);
        cmd_value |= (tmp << 8);
        *cmd_ptr++ = cmd_value;

        /* dw17 */
        *cmd_ptr++ = vp9_state->frame_header.bit_offset_ref_lf_delta |
                     (vp9_state->frame_header.bit_offset_mode_lf_delta << 16);
        *cmd_ptr++ = vp9_state->frame_header.bit_offset_qindex |
                     (vp9_state->frame_header.bit_offset_lf_level << 16);

        /* dw19 */
        *cmd_ptr++ = (1 << 26 | (1 << 25) |
                      non_first_pass << 16);
        /* dw20 */
        *cmd_ptr++ = (1 << 31) | (256);

        /* dw21 */
        *cmd_ptr++ = (0 << 31) | 1;

        /* dw22-dw24. Frame_delta_qindex_range */
        *cmd_ptr++ = 0;
        *cmd_ptr++ = 0;
        *cmd_ptr++ = 0;

        /* dw25-26. frame_delta_lf_range */
        *cmd_ptr++ = 0;
        *cmd_ptr++ = 0;

        /* dw27. frame_delta_lf_min */
        *cmd_ptr++ = 0;

        /* dw28..30 */
        *cmd_ptr++ = 0;
        *cmd_ptr++ = 0;
        *cmd_ptr++ = 0;

        /* dw31 */
        *cmd_ptr++ = (0 << 30) | 1;
        /* dw32 */
        *cmd_ptr++ = vp9_state->frame_header.bit_offset_first_partition_size;

        *cmd_ptr++ = 0;
        *cmd_ptr++ = MI_BATCH_BUFFER_END;
    }

    i965_unmap_gpe_resource(gpe_resource);
}

static void
gen9_brc_update_add_surfaces_vp9(VADriverContextP ctx,
                                 struct encode_state *encode_state,
                                 struct intel_encoder_context *encoder_context,
                                 struct i965_gpe_context *brc_gpe_context,
                                 struct i965_gpe_context *mbenc_gpe_context)
{
    struct gen9_encoder_context_vp9 *vme_context = encoder_context->vme_context;

    /* 0. BRC history buffer */
    gen9_add_buffer_gpe_surface(ctx,
                                brc_gpe_context,
                                &vme_context->res_brc_history_buffer,
                                0,
                                vme_context->res_brc_history_buffer.size,
                                0,
                                VP9_BTI_BRC_HISTORY_G9);

    /* 1. Constant data buffer */
    gen9_add_buffer_gpe_surface(ctx,
                                brc_gpe_context,
                                &vme_context->res_brc_const_data_buffer,
                                0,
                                vme_context->res_brc_const_data_buffer.size,
                                0,
                                VP9_BTI_BRC_CONSTANT_DATA_G9);

    /* 2. Distortion 2D surface buffer */
    gen9_add_buffer_2d_gpe_surface(ctx,
                                   brc_gpe_context,
                                   &vme_context->s4x_memv_distortion_buffer,
                                   1,
                                   I965_SURFACEFORMAT_R8_UNORM,
                                   VP9_BTI_BRC_DISTORTION_G9);

    /* 3. pak buffer */
    gen9_add_buffer_gpe_surface(ctx,
                                brc_gpe_context,
                                &vme_context->res_brc_mmdk_pak_buffer,
                                0,
                                vme_context->res_brc_mmdk_pak_buffer.size,
                                0,
                                VP9_BTI_BRC_MMDK_PAK_OUTPUT_G9);
    /* 4. Mbenc curbe input buffer */
    gen9_add_dri_buffer_gpe_surface(ctx,
                                    brc_gpe_context,
                                    mbenc_gpe_context->curbe.bo,
                                    0,
                                    ALIGN(mbenc_gpe_context->curbe.length, 64),
                                    mbenc_gpe_context->curbe.offset,
                                    VP9_BTI_BRC_MBENC_CURBE_INPUT_G9);
    /* 5. Mbenc curbe output buffer */
    gen9_add_dri_buffer_gpe_surface(ctx,
                                    brc_gpe_context,
                                    mbenc_gpe_context->curbe.bo,
                                    0,
                                    ALIGN(mbenc_gpe_context->curbe.length, 64),
                                    mbenc_gpe_context->curbe.offset,
                                    VP9_BTI_BRC_MBENC_CURBE_OUTPUT_G9);

    /* 6. BRC_PIC_STATE read buffer */
    gen9_add_buffer_gpe_surface(ctx, brc_gpe_context,
                                &vme_context->res_pic_state_brc_read_buffer,
                                0,
                                vme_context->res_pic_state_brc_read_buffer.size,
                                0,
                                VP9_BTI_BRC_PIC_STATE_INPUT_G9);

    /* 7. BRC_PIC_STATE write buffer */
    gen9_add_buffer_gpe_surface(ctx, brc_gpe_context,
                                &vme_context->res_pic_state_brc_write_hfw_read_buffer,
                                0,
                                vme_context->res_pic_state_brc_write_hfw_read_buffer.size,
                                0,
                                VP9_BTI_BRC_PIC_STATE_OUTPUT_G9);

    /* 8. SEGMENT_STATE read buffer */
    gen9_add_buffer_gpe_surface(ctx, brc_gpe_context,
                                &vme_context->res_seg_state_brc_read_buffer,
                                0,
                                vme_context->res_seg_state_brc_read_buffer.size,
                                0,
                                VP9_BTI_BRC_SEGMENT_STATE_INPUT_G9);

    /* 9. SEGMENT_STATE write buffer */
    gen9_add_buffer_gpe_surface(ctx, brc_gpe_context,
                                &vme_context->res_seg_state_brc_write_buffer,
                                0,
                                vme_context->res_seg_state_brc_write_buffer.size,
                                0,
                                VP9_BTI_BRC_SEGMENT_STATE_OUTPUT_G9);

    /* 10. Bitstream size buffer */
    gen9_add_buffer_gpe_surface(ctx, brc_gpe_context,
                                &vme_context->res_brc_bitstream_size_buffer,
                                0,
                                vme_context->res_brc_bitstream_size_buffer.size,
                                0,
                                VP9_BTI_BRC_BITSTREAM_SIZE_G9);

    gen9_add_buffer_gpe_surface(ctx, brc_gpe_context,
                                &vme_context->res_brc_hfw_data_buffer,
                                0,
                                vme_context->res_brc_hfw_data_buffer.size,
                                0,
                                VP9_BTI_BRC_HFW_DATA_G9);

    return;
}

static VAStatus
gen9_vp9_brc_update_kernel(VADriverContextP ctx,
                           struct encode_state *encode_state,
                           struct intel_encoder_context *encoder_context)
{
    struct gen9_encoder_context_vp9 *vme_context = encoder_context->vme_context;
    struct vp9_brc_context *brc_context = &vme_context->brc_context;
    struct i965_gpe_context *brc_gpe_context, *mbenc_gpe_context;
    int mbenc_index, gpe_index = VP9_BRC_UPDATE;
    int media_function = VP9_MEDIA_STATE_BRC_UPDATE;
    int mbenc_function;
    struct gen9_vp9_brc_curbe_param        brc_update_curbe_param;
    VAEncPictureParameterBufferVP9 *pic_param;
    struct gen9_vp9_state *vp9_state;
    struct gen9_vp9_mbenc_curbe_param    mbenc_curbe_param;
    struct gpe_media_object_parameter media_object_param;

    vp9_state = (struct gen9_vp9_state *) encoder_context->enc_priv_state;
    if (!vp9_state || !vp9_state->pic_param)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    pic_param = vp9_state->pic_param;
    // Setup VP9 MbEnc Curbe
    if (vp9_state->picture_coding_type) {
        mbenc_function = VP9_MEDIA_STATE_MBENC_P;
        mbenc_index = VP9_MBENC_IDX_INTER;
    } else {
        mbenc_function = VP9_MEDIA_STATE_MBENC_I_32x32;
        mbenc_index = VP9_MBENC_IDX_KEY_32x32;
    }

    mbenc_gpe_context = &(vme_context->mbenc_context.gpe_contexts[mbenc_index]);

    memset(&mbenc_curbe_param, 0, sizeof(mbenc_curbe_param));

    mbenc_curbe_param.ppic_param             = vp9_state->pic_param;
    mbenc_curbe_param.pseq_param             = vp9_state->seq_param;
    mbenc_curbe_param.psegment_param         = vp9_state->segment_param;
    //mbenc_curbe_param.ppRefList              = &(vp9_state->pRefList[0]);
    mbenc_curbe_param.last_ref_obj           = vp9_state->last_ref_obj;
    mbenc_curbe_param.golden_ref_obj         = vp9_state->golden_ref_obj;
    mbenc_curbe_param.alt_ref_obj            = vp9_state->alt_ref_obj;
    mbenc_curbe_param.frame_width_in_mb      = ALIGN(vp9_state->frame_width, 16) / 16;
    mbenc_curbe_param.frame_height_in_mb     = ALIGN(vp9_state->frame_height, 16) / 16;
    mbenc_curbe_param.hme_enabled            = vp9_state->hme_enabled;
    mbenc_curbe_param.ref_frame_flag         = vp9_state->ref_frame_flag;
    mbenc_curbe_param.multi_ref_qp_check     = vp9_state->multi_ref_qp_check;
    mbenc_curbe_param.picture_coding_type    = vp9_state->picture_coding_type;
    mbenc_curbe_param.media_state_type       = mbenc_function;

    vme_context->pfn_set_curbe_mbenc(ctx, encode_state,
                                     mbenc_gpe_context,
                                     encoder_context,
                                     &mbenc_curbe_param);

    vp9_state->mbenc_curbe_set_in_brc_update = true;

    brc_gpe_context = &brc_context->gpe_contexts[gpe_index];

    gen8_gpe_context_init(ctx, brc_gpe_context);
    gen9_gpe_reset_binding_table(ctx, brc_gpe_context);

    memset(&brc_update_curbe_param, 0, sizeof(brc_update_curbe_param));

    // Setup BRC Update Curbe
    brc_update_curbe_param.media_state_type       = media_function;
    brc_update_curbe_param.curr_frame               = pic_param->reconstructed_frame;
    brc_update_curbe_param.ppic_param             = vp9_state->pic_param;
    brc_update_curbe_param.pseq_param             = vp9_state->seq_param;
    brc_update_curbe_param.psegment_param         = vp9_state->segment_param;
    brc_update_curbe_param.picture_coding_type    = vp9_state->picture_coding_type;
    brc_update_curbe_param.frame_width_in_mb      = ALIGN(vp9_state->frame_width, 16) / 16;
    brc_update_curbe_param.frame_height_in_mb     = ALIGN(vp9_state->frame_height, 16) / 16;
    brc_update_curbe_param.hme_enabled            = vp9_state->hme_enabled;
    brc_update_curbe_param.b_used_ref             = 1;
    brc_update_curbe_param.frame_number           = vp9_state->frame_number;
    brc_update_curbe_param.ref_frame_flag         = vp9_state->ref_frame_flag;
    brc_update_curbe_param.mbbrc_enabled          = 0;
    brc_update_curbe_param.multi_ref_qp_check     = vp9_state->multi_ref_qp_check;
    brc_update_curbe_param.brc_num_pak_passes     = vp9_state->num_pak_passes;

    brc_update_curbe_param.pbrc_init_current_target_buf_full_in_bits =
        &vp9_state->brc_init_current_target_buf_full_in_bits;
    brc_update_curbe_param.pbrc_init_reset_buf_size_in_bits =
        &vp9_state->brc_init_reset_buf_size_in_bits;
    brc_update_curbe_param.pbrc_init_reset_input_bits_per_frame =
        &vp9_state->brc_init_reset_input_bits_per_frame;

    vme_context->pfn_set_curbe_brc(ctx, encode_state,
                                   brc_gpe_context,
                                   encoder_context,
                                   &brc_update_curbe_param);


    // Check if the constant data surface is present
    if (vp9_state->brc_constant_buffer_supported) {
        char *brc_const_buffer;
        brc_const_buffer = i965_map_gpe_resource(&vme_context->res_brc_const_data_buffer);

        if (!brc_const_buffer)
            return VA_STATUS_ERROR_OPERATION_FAILED;

        if (vp9_state->picture_coding_type)
            memcpy(brc_const_buffer, vp9_brc_const_data_p_g9,
                   sizeof(vp9_brc_const_data_p_g9));
        else
            memcpy(brc_const_buffer, vp9_brc_const_data_i_g9,
                   sizeof(vp9_brc_const_data_i_g9));

        i965_unmap_gpe_resource(&vme_context->res_brc_const_data_buffer);
    }

    if (pic_param->pic_flags.bits.segmentation_enabled) {
        //reallocate the vme_state->mb_segment_map_surface
        /* this will be added later */
    }

    {
        pic_param->filter_level = 0;
        // clear the filter level value in picParams ebfore programming pic state, as this value will be determined and updated by BRC.
        intel_vp9enc_construct_picstate_batchbuf(ctx, encode_state,
                                                 encoder_context, &vme_context->res_pic_state_brc_read_buffer);
    }

    gen9_brc_update_add_surfaces_vp9(ctx, encode_state,
                                     encoder_context,
                                     brc_gpe_context,
                                     mbenc_gpe_context);

    gen8_gpe_setup_interface_data(ctx, brc_gpe_context);
    memset(&media_object_param, 0, sizeof(media_object_param));
    gen9_run_kernel_media_object(ctx, encoder_context,
                                 brc_gpe_context,
                                 media_function,
                                 &media_object_param);
    return VA_STATUS_SUCCESS;
}

static
void gen9_vp9_set_curbe_me(VADriverContextP ctx,
                           struct encode_state *encode_state,
                           struct i965_gpe_context *gpe_context,
                           struct intel_encoder_context *encoder_context,
                           struct gen9_vp9_me_curbe_param *param)
{
    vp9_me_curbe_data        *me_cmd;
    int enc_media_state;
    int                                       me_mode;
    unsigned int                                       width, height;
    uint32_t                                  l0_ref_frames;
    uint32_t                                  scale_factor;

    if (param->b16xme_enabled) {
        if (param->use_16x_me)
            me_mode = VP9_ENC_ME16X_BEFORE_ME4X;
        else
            me_mode = VP9_ENC_ME4X_AFTER_ME16X;
    } else {
        me_mode = VP9_ENC_ME4X_ONLY;
    }

    if (me_mode == VP9_ENC_ME16X_BEFORE_ME4X)
        scale_factor = 16;
    else
        scale_factor = 4;

    if (param->use_16x_me)
        enc_media_state = VP9_MEDIA_STATE_16X_ME;
    else
        enc_media_state = VP9_MEDIA_STATE_4X_ME;

    me_cmd = i965_gpe_context_map_curbe(gpe_context);

    if (!me_cmd)
        return;

    memset(me_cmd, 0, sizeof(vp9_me_curbe_data));

    me_cmd->dw1.max_num_mvs           = 0x10;
    me_cmd->dw1.bi_weight             = 0x00;

    me_cmd->dw2.max_num_su            = 0x39;
    me_cmd->dw2.max_len_sp            = 0x39;

    me_cmd->dw3.sub_mb_part_mask       = 0x77;
    me_cmd->dw3.inter_sad             = 0x00;
    me_cmd->dw3.intra_sad            = 0x00;
    me_cmd->dw3.bme_disable_fbr      = 0x01;
    me_cmd->dw3.sub_pel_mode         = 0x03;

    width = param->frame_width / scale_factor;
    height = param->frame_height / scale_factor;

    me_cmd->dw4.picture_width        = ALIGN(width, 16) / 16;
    me_cmd->dw4.picture_height_minus1       = ALIGN(height, 16) / 16 - 1;

    me_cmd->dw5.ref_width            = 0x30;
    me_cmd->dw5.ref_height           = 0x28;

    if (enc_media_state == VP9_MEDIA_STATE_4X_ME)
        me_cmd->dw6.write_distortions = 0x01;

    me_cmd->dw6.use_mv_from_prev_step   = me_mode == VP9_ENC_ME4X_AFTER_ME16X ? 1 : 0;
    me_cmd->dw6.super_combine_dist    = 0x5;
    me_cmd->dw6.max_vmvr              = 0x7fc;

    l0_ref_frames = (param->ref_frame_flag & 0x01) +
                    !!(param->ref_frame_flag & 0x02) +
                    !!(param->ref_frame_flag & 0x04);
    me_cmd->dw13.num_ref_idx_l0_minus1 = (l0_ref_frames > 0) ? l0_ref_frames - 1 : 0;
    me_cmd->dw13.num_ref_idx_l1_minus1 =  0;

    me_cmd->dw14.l0_ref_pic_polarity_bits = 0;
    me_cmd->dw14.l1_ref_pic_polarity_bits = 0;

    me_cmd->dw15.mv_shift_factor        = 0x02;

    {
        memcpy((void *)((char *)me_cmd + 64),
               vp9_diamond_ime_search_path_delta,
               sizeof(vp9_diamond_ime_search_path_delta));
    }


    me_cmd->dw32._4x_memv_output_data_surf_index     = VP9_BTI_ME_MV_DATA_SURFACE;
    me_cmd->dw33._16x_32x_memv_input_data_surf_index = VP9_BTI_16XME_MV_DATA_SURFACE;
    me_cmd->dw34._4x_me_output_dist_surf_index       = VP9_BTI_ME_DISTORTION_SURFACE;
    me_cmd->dw35._4x_me_output_brc_dist_surf_index   = VP9_BTI_ME_BRC_DISTORTION_SURFACE;
    me_cmd->dw36.vme_fwd_inter_pred_surf_index       = VP9_BTI_ME_CURR_PIC_L0;
    me_cmd->dw37.vme_bdw_inter_pred_surf_index       = VP9_BTI_ME_CURR_PIC_L1;

    i965_gpe_context_unmap_curbe(gpe_context);
}

static void
gen9_vp9_send_me_surface(VADriverContextP ctx,
                         struct encode_state *encode_state,
                         struct i965_gpe_context *gpe_context,
                         struct intel_encoder_context *encoder_context,
                         struct gen9_vp9_me_surface_param *param)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct object_surface *obj_surface;
    struct gen9_surface_vp9 *vp9_priv_surface;
    struct object_surface *input_surface;
    struct i965_gpe_resource *gpe_resource;
    int ref_bti;

    obj_surface = SURFACE(param->curr_pic);

    if (!obj_surface || !obj_surface->private_data)
        return;

    vp9_priv_surface = obj_surface->private_data;
    if (param->use_16x_me) {
        gpe_resource = param->pres_16x_memv_data_buffer;
    } else {
        gpe_resource = param->pres_4x_memv_data_buffer;
    }

    gen9_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                   gpe_resource,
                                   1,
                                   I965_SURFACEFORMAT_R8_UNORM,
                                   VP9_BTI_ME_MV_DATA_SURFACE);

    if (param->b16xme_enabled) {
        gpe_resource = param->pres_16x_memv_data_buffer;
        gen9_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                       gpe_resource,
                                       1,
                                       I965_SURFACEFORMAT_R8_UNORM,
                                       VP9_BTI_16XME_MV_DATA_SURFACE);
    }

    if (!param->use_16x_me) {
        gpe_resource = param->pres_me_brc_distortion_buffer;

        gen9_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                       gpe_resource,
                                       1,
                                       I965_SURFACEFORMAT_R8_UNORM,
                                       VP9_BTI_ME_BRC_DISTORTION_SURFACE);

        gpe_resource = param->pres_me_distortion_buffer;

        gen9_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                       gpe_resource,
                                       1,
                                       I965_SURFACEFORMAT_R8_UNORM,
                                       VP9_BTI_ME_DISTORTION_SURFACE);
    }

    if (param->use_16x_me)
        input_surface = vp9_priv_surface->scaled_16x_surface_obj;
    else
        input_surface = vp9_priv_surface->scaled_4x_surface_obj;

    gen9_add_adv_gpe_surface(ctx, gpe_context,
                             input_surface,
                             VP9_BTI_ME_CURR_PIC_L0);

    ref_bti = VP9_BTI_ME_CURR_PIC_L0 + 1;


    if (param->last_ref_pic) {
        obj_surface = param->last_ref_pic;
        vp9_priv_surface = obj_surface->private_data;

        if (param->use_16x_me)
            input_surface = vp9_priv_surface->scaled_16x_surface_obj;
        else
            input_surface = vp9_priv_surface->scaled_4x_surface_obj;

        if (param->dys_enabled &&
            ((vp9_priv_surface->frame_width != param->frame_width) ||
             (vp9_priv_surface->frame_height != param->frame_height))) {
            if (param->use_16x_me)
                input_surface = vp9_priv_surface->dys_16x_surface_obj;
            else
                input_surface = vp9_priv_surface->dys_4x_surface_obj;
        }
        gen9_add_adv_gpe_surface(ctx, gpe_context,
                                 input_surface,
                                 ref_bti);
        gen9_add_adv_gpe_surface(ctx, gpe_context,
                                 input_surface,
                                 ref_bti + 1);
        ref_bti += 2;
    }

    if (param->golden_ref_pic) {
        obj_surface = param->golden_ref_pic;
        vp9_priv_surface = obj_surface->private_data;

        if (param->use_16x_me)
            input_surface = vp9_priv_surface->scaled_16x_surface_obj;
        else
            input_surface = vp9_priv_surface->scaled_4x_surface_obj;

        if (param->dys_enabled &&
            ((vp9_priv_surface->frame_width != param->frame_width) ||
             (vp9_priv_surface->frame_height != param->frame_height))) {
            if (param->use_16x_me)
                input_surface = vp9_priv_surface->dys_16x_surface_obj;
            else
                input_surface = vp9_priv_surface->dys_4x_surface_obj;
        }

        gen9_add_adv_gpe_surface(ctx, gpe_context,
                                 input_surface,
                                 ref_bti);
        gen9_add_adv_gpe_surface(ctx, gpe_context,
                                 input_surface,
                                 ref_bti + 1);
        ref_bti += 2;
    }

    if (param->alt_ref_pic) {
        obj_surface = param->alt_ref_pic;
        vp9_priv_surface = obj_surface->private_data;

        if (param->use_16x_me)
            input_surface = vp9_priv_surface->scaled_16x_surface_obj;
        else
            input_surface = vp9_priv_surface->scaled_4x_surface_obj;

        if (param->dys_enabled &&
            ((vp9_priv_surface->frame_width != param->frame_width) ||
             (vp9_priv_surface->frame_height != param->frame_height))) {
            if (param->use_16x_me)
                input_surface = vp9_priv_surface->dys_16x_surface_obj;
            else
                input_surface = vp9_priv_surface->dys_4x_surface_obj;
        }
        gen9_add_adv_gpe_surface(ctx, gpe_context,
                                 input_surface,
                                 ref_bti);
        gen9_add_adv_gpe_surface(ctx, gpe_context,
                                 input_surface,
                                 ref_bti + 1);
        ref_bti += 2;
    }

    return;
}

static
void gen9_me_add_surfaces_vp9(VADriverContextP ctx,
                              struct encode_state *encode_state,
                              struct intel_encoder_context *encoder_context,
                              struct i965_gpe_context *gpe_context,
                              int use_16x_me)
{
    struct gen9_encoder_context_vp9 *vme_context = encoder_context->vme_context;
    struct gen9_vp9_me_surface_param  me_surface_param;
    struct gen9_vp9_state *vp9_state;

    vp9_state = (struct gen9_vp9_state *)(encoder_context->enc_priv_state);

    /* sScaled4xSurface surface */
    memset(&me_surface_param, 0, sizeof(me_surface_param));
    me_surface_param.last_ref_pic = vp9_state->last_ref_obj;
    me_surface_param.golden_ref_pic = vp9_state->golden_ref_obj;
    me_surface_param.alt_ref_pic = vp9_state->alt_ref_obj;
    me_surface_param.curr_pic = vp9_state->curr_frame;
    me_surface_param.pres_4x_memv_data_buffer  = &vme_context->s4x_memv_data_buffer;
    me_surface_param.pres_16x_memv_data_buffer = &vme_context->s16x_memv_data_buffer;
    me_surface_param.pres_me_distortion_buffer = &vme_context->s4x_memv_distortion_buffer;
    me_surface_param.pres_me_brc_distortion_buffer = &vme_context->s4x_memv_distortion_buffer;

    if (use_16x_me) {
        me_surface_param.downscaled_width_in_mb = vp9_state->downscaled_width_16x_in_mb;
        me_surface_param.downscaled_height_in_mb = vp9_state->downscaled_height_16x_in_mb;
    } else {
        me_surface_param.downscaled_width_in_mb = vp9_state->downscaled_width_4x_in_mb;
        me_surface_param.downscaled_height_in_mb = vp9_state->downscaled_height_4x_in_mb;
    }
    me_surface_param.frame_width  = vp9_state->frame_width;
    me_surface_param.frame_height  = vp9_state->frame_height;

    me_surface_param.use_16x_me = use_16x_me;
    me_surface_param.b16xme_enabled = vp9_state->b16xme_enabled;
    me_surface_param.dys_enabled = vp9_state->dys_in_use;

    vme_context->pfn_send_me_surface(ctx, encode_state,
                                     gpe_context,
                                     encoder_context,
                                     &me_surface_param);
    return;
}

static VAStatus
gen9_vp9_me_kernel(VADriverContextP ctx,
                   struct encode_state *encode_state,
                   struct intel_encoder_context *encoder_context,
                   int use_16x_me)
{
    struct gen9_encoder_context_vp9 *vme_context = encoder_context->vme_context;
    struct i965_gpe_context *gpe_context;
    int media_function;
    struct gen9_vp9_me_curbe_param me_curbe_param;
    struct gen9_vp9_state *vp9_state;
    struct gpe_media_object_walker_parameter media_object_walker_param;
    struct gpe_encoder_kernel_walker_parameter kernel_walker_param;

    vp9_state = (struct gen9_vp9_state *) encoder_context->enc_priv_state;
    if (!vp9_state || !vp9_state->pic_param)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    if (use_16x_me)
        media_function = VP9_MEDIA_STATE_16X_ME;
    else
        media_function = VP9_MEDIA_STATE_4X_ME;

    gpe_context = &(vme_context->me_context.gpe_context);

    gen8_gpe_context_init(ctx, gpe_context);
    gen9_gpe_reset_binding_table(ctx, gpe_context);

    memset(&me_curbe_param, 0, sizeof(me_curbe_param));
    me_curbe_param.ppic_param = vp9_state->pic_param;
    me_curbe_param.pseq_param = vp9_state->seq_param;
    me_curbe_param.frame_width = vp9_state->frame_width;
    me_curbe_param.frame_height = vp9_state->frame_height;
    me_curbe_param.ref_frame_flag = vp9_state->ref_frame_flag;
    me_curbe_param.use_16x_me = use_16x_me;
    me_curbe_param.b16xme_enabled = vp9_state->b16xme_enabled;
    vme_context->pfn_set_curbe_me(ctx, encode_state,
                                  gpe_context,
                                  encoder_context,
                                  &me_curbe_param);

    gen9_me_add_surfaces_vp9(ctx, encode_state,
                             encoder_context,
                             gpe_context,
                             use_16x_me);

    gen8_gpe_setup_interface_data(ctx, gpe_context);

    memset(&kernel_walker_param, 0, sizeof(kernel_walker_param));
    if (use_16x_me) {
        kernel_walker_param.resolution_x = vp9_state->downscaled_width_16x_in_mb;
        kernel_walker_param.resolution_y = vp9_state->downscaled_height_16x_in_mb;
    } else {
        kernel_walker_param.resolution_x = vp9_state->downscaled_width_4x_in_mb;
        kernel_walker_param.resolution_y = vp9_state->downscaled_height_4x_in_mb;
    }
    kernel_walker_param.no_dependency = 1;

    gen9_init_media_object_walker_parameter(encoder_context, &kernel_walker_param, &media_object_walker_param);

    gen9_run_kernel_media_object_walker(ctx, encoder_context,
                                        gpe_context,
                                        media_function,
                                        &media_object_walker_param);

    return VA_STATUS_SUCCESS;
}

static void
gen9_vp9_set_curbe_scaling_cm(VADriverContextP ctx,
                              struct encode_state *encode_state,
                              struct i965_gpe_context *gpe_context,
                              struct intel_encoder_context *encoder_context,
                              struct gen9_vp9_scaling_curbe_param *curbe_param)
{
    vp9_scaling4x_curbe_data_cm *curbe_cmd;

    curbe_cmd = i965_gpe_context_map_curbe(gpe_context);

    if (!curbe_cmd)
        return;

    memset(curbe_cmd, 0, sizeof(vp9_scaling4x_curbe_data_cm));

    curbe_cmd->dw0.input_picture_width = curbe_param->input_picture_width;
    curbe_cmd->dw0.input_picture_height = curbe_param->input_picture_height;

    curbe_cmd->dw1.input_y_bti = VP9_BTI_SCALING_FRAME_SRC_Y;
    curbe_cmd->dw2.output_y_bti = VP9_BTI_SCALING_FRAME_DST_Y;


    curbe_cmd->dw6.enable_mb_variance_output = 0;
    curbe_cmd->dw6.enable_mb_pixel_average_output = 0;
    curbe_cmd->dw6.enable_blk8x8_stat_output = 0;

    if (curbe_param->mb_variance_output_enabled ||
        curbe_param->mb_pixel_average_output_enabled) {
        curbe_cmd->dw10.mbv_proc_stat_bti = VP9_BTI_SCALING_FRAME_MBVPROCSTATS_DST_CM;
    }

    i965_gpe_context_unmap_curbe(gpe_context);
    return;
}

static void
gen9_vp9_send_scaling_surface(VADriverContextP ctx,
                              struct encode_state *encode_state,
                              struct i965_gpe_context *gpe_context,
                              struct intel_encoder_context *encoder_context,
                              struct gen9_vp9_scaling_surface_param *scaling_surface_param)
{
    vp9_bti_scaling_offset *scaling_bti;
    unsigned int surface_format;

    scaling_bti = scaling_surface_param->p_scaling_bti;

    if (scaling_surface_param->scaling_out_use_32unorm_surf_fmt)
        surface_format = I965_SURFACEFORMAT_R32_UNORM;
    else if (scaling_surface_param->scaling_out_use_16unorm_surf_fmt)
        surface_format = I965_SURFACEFORMAT_R16_UNORM;
    else
        surface_format = I965_SURFACEFORMAT_R8_UNORM;

    gen9_add_2d_gpe_surface(ctx, gpe_context,
                            scaling_surface_param->input_surface,
                            0, 1, surface_format,
                            scaling_bti->scaling_frame_src_y);

    gen9_add_2d_gpe_surface(ctx, gpe_context,
                            scaling_surface_param->output_surface,
                            0, 1, surface_format,
                            scaling_bti->scaling_frame_dst_y);


    return;
}

static VAStatus
gen9_vp9_scaling_kernel(VADriverContextP ctx,
                        struct encode_state *encode_state,
                        struct intel_encoder_context *encoder_context,
                        int use_16x_scaling)
{
    struct gen9_encoder_context_vp9 *vme_context = encoder_context->vme_context;
    struct i965_gpe_context *gpe_context;
    int media_function;
    struct gen9_vp9_scaling_curbe_param scaling_curbe_param;
    struct gen9_vp9_scaling_surface_param scaling_surface_param;
    struct gen9_vp9_state *vp9_state;
    VAEncPictureParameterBufferVP9  *pic_param;
    struct gpe_media_object_walker_parameter media_object_walker_param;
    struct gpe_encoder_kernel_walker_parameter kernel_walker_param;
    struct object_surface *obj_surface;
    struct object_surface *input_surface, *output_surface;
    struct gen9_surface_vp9 *vp9_priv_surface;
    unsigned int downscaled_width_in_mb, downscaled_height_in_mb;
    unsigned int input_frame_width, input_frame_height;
    unsigned int output_frame_width, output_frame_height;

    vp9_state = (struct gen9_vp9_state *) encoder_context->enc_priv_state;
    if (!vp9_state || !vp9_state->pic_param)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    pic_param = vp9_state->pic_param;

    if (use_16x_scaling)
        media_function = VP9_MEDIA_STATE_16X_SCALING;
    else
        media_function = VP9_MEDIA_STATE_4X_SCALING;

    gpe_context = &(vme_context->scaling_context.gpe_contexts[0]);

    gen8_gpe_context_init(ctx, gpe_context);
    gen9_gpe_reset_binding_table(ctx, gpe_context);

    obj_surface = encode_state->reconstructed_object;
    vp9_priv_surface = obj_surface->private_data;

    if (use_16x_scaling) {
        downscaled_width_in_mb      = vp9_state->downscaled_width_16x_in_mb;
        downscaled_height_in_mb      = vp9_state->downscaled_height_16x_in_mb;

        input_surface               = vp9_priv_surface->scaled_4x_surface_obj;
        input_frame_width           = vp9_state->frame_width_4x;
        input_frame_height          = vp9_state->frame_height_4x;

        output_surface              = vp9_priv_surface->scaled_16x_surface_obj;
        output_frame_width          = vp9_state->frame_width_16x;
        output_frame_height         = vp9_state->frame_height_16x;
    } else {
        downscaled_width_in_mb      = vp9_state->downscaled_width_4x_in_mb;
        downscaled_height_in_mb      = vp9_state->downscaled_height_4x_in_mb;

        if (vp9_state->dys_in_use &&
            ((pic_param->frame_width_src != pic_param->frame_width_dst) ||
             (pic_param->frame_height_src != pic_param->frame_height_dst)))
            input_surface               = vp9_priv_surface->dys_surface_obj;
        else
            input_surface               = encode_state->input_yuv_object;

        input_frame_width           = vp9_state->frame_width;
        input_frame_height          = vp9_state->frame_height;

        output_surface              = vp9_priv_surface->scaled_4x_surface_obj;
        output_frame_width          = vp9_state->frame_width_4x;
        output_frame_height         = vp9_state->frame_height_4x;
    }

    memset(&scaling_curbe_param, 0, sizeof(scaling_curbe_param));

    scaling_curbe_param.input_picture_width  = input_frame_width;
    scaling_curbe_param.input_picture_height = input_frame_height;

    scaling_curbe_param.use_16x_scaling = use_16x_scaling;
    scaling_curbe_param.use_32x_scaling = 0;

    if (use_16x_scaling)
        scaling_curbe_param.mb_variance_output_enabled = 0;
    else
        scaling_curbe_param.mb_variance_output_enabled = vp9_state->adaptive_transform_decision_enabled;

    scaling_curbe_param.blk8x8_stat_enabled = 0;

    vme_context->pfn_set_curbe_scaling(ctx, encode_state,
                                       gpe_context,
                                       encoder_context,
                                       &scaling_curbe_param);

    memset(&scaling_surface_param, 0, sizeof(scaling_surface_param));
    scaling_surface_param.p_scaling_bti = (void *)(&vme_context->scaling_context.scaling_4x_bti);
    scaling_surface_param.input_surface                      = input_surface;
    scaling_surface_param.input_frame_width                  = input_frame_width;
    scaling_surface_param.input_frame_height                 = input_frame_height;

    scaling_surface_param.output_surface                     = output_surface;
    scaling_surface_param.output_frame_width                 = output_frame_width;
    scaling_surface_param.output_frame_height                = output_frame_height;
    scaling_surface_param.scaling_out_use_16unorm_surf_fmt   = 0;
    scaling_surface_param.scaling_out_use_32unorm_surf_fmt   = 1;

    vme_context->pfn_send_scaling_surface(ctx, encode_state,
                                          gpe_context,
                                          encoder_context,
                                          &scaling_surface_param);

    gen8_gpe_setup_interface_data(ctx, gpe_context);

    memset(&kernel_walker_param, 0, sizeof(kernel_walker_param));
    /* the scaling is based on 8x8 blk level */
    kernel_walker_param.resolution_x = downscaled_width_in_mb * 2;
    kernel_walker_param.resolution_y = downscaled_height_in_mb * 2;
    kernel_walker_param.no_dependency = 1;

    gen9_init_media_object_walker_parameter(encoder_context, &kernel_walker_param, &media_object_walker_param);

    gen9_run_kernel_media_object_walker(ctx, encoder_context,
                                        gpe_context,
                                        media_function,
                                        &media_object_walker_param);

    return VA_STATUS_SUCCESS;
}

static void
gen9_vp9_dys_set_sampler_state(struct i965_gpe_context *gpe_context)
{
    struct gen9_sampler_8x8_avs                *sampler_cmd;

    if (!gpe_context)
        return;

    dri_bo_map(gpe_context->sampler.bo, 1);

    if (!gpe_context->sampler.bo->virtual)
        return;

    sampler_cmd = (struct gen9_sampler_8x8_avs *)
                  (gpe_context->sampler.bo->virtual + gpe_context->sampler.offset);

    memset(sampler_cmd, 0, sizeof(struct gen9_sampler_8x8_avs));

    sampler_cmd->dw0.r3c_coefficient                      = 15;
    sampler_cmd->dw0.r3x_coefficient                      = 6;
    sampler_cmd->dw0.strong_edge_threshold                = 8;
    sampler_cmd->dw0.weak_edge_threshold                  = 1;
    sampler_cmd->dw0.gain_factor                          = 32;

    sampler_cmd->dw2.r5c_coefficient                     = 3;
    sampler_cmd->dw2.r5cx_coefficient                    = 8;
    sampler_cmd->dw2.r5x_coefficient                     = 9;
    sampler_cmd->dw2.strong_edge_weight                  = 6;
    sampler_cmd->dw2.regular_weight                      = 3;
    sampler_cmd->dw2.non_edge_weight                     = 2;
    sampler_cmd->dw2.global_noise_estimation             = 255;

    sampler_cmd->dw3.enable_8tap_adaptive_filter         = 0;
    sampler_cmd->dw3.cos_alpha                           = 79;
    sampler_cmd->dw3.sin_alpha                           = 101;

    sampler_cmd->dw5.diamond_du                           = 0;
    sampler_cmd->dw5.hs_margin                            = 3;
    sampler_cmd->dw5.diamond_alpha                        = 100;

    sampler_cmd->dw7.inv_margin_vyl                       = 3300;

    sampler_cmd->dw8.inv_margin_vyu                       = 1600;

    sampler_cmd->dw10.y_slope2                            = 24;
    sampler_cmd->dw10.s0l                                 = 1792;

    sampler_cmd->dw12.y_slope1                            = 24;

    sampler_cmd->dw14.s0u                                = 256;

    sampler_cmd->dw15.s2u                                = 1792;
    sampler_cmd->dw15.s1u                                = 0;

    memcpy(sampler_cmd->coefficients,
           &gen9_vp9_avs_coeffs[0],
           17 * sizeof(struct gen8_sampler_8x8_avs_coefficients));

    sampler_cmd->dw152.default_sharpness_level     = 255;
    sampler_cmd->dw152.max_derivative_4_pixels     = 7;
    sampler_cmd->dw152.max_derivative_8_pixels     = 20;
    sampler_cmd->dw152.transition_area_with_4_pixels    = 4;
    sampler_cmd->dw152.transition_area_with_8_pixels    = 5;

    sampler_cmd->dw153.bypass_x_adaptive_filtering  = 1;
    sampler_cmd->dw153.bypass_y_adaptive_filtering  = 1;
    sampler_cmd->dw153.adaptive_filter_for_all_channel = 0;

    memcpy(sampler_cmd->extra_coefficients,
           &gen9_vp9_avs_coeffs[17 * 8],
           15 * sizeof(struct gen8_sampler_8x8_avs_coefficients));

    dri_bo_unmap(gpe_context->sampler.bo);
}

static void
gen9_vp9_set_curbe_dys(VADriverContextP ctx,
                       struct encode_state *encode_state,
                       struct i965_gpe_context *gpe_context,
                       struct intel_encoder_context *encoder_context,
                       struct gen9_vp9_dys_curbe_param *curbe_param)
{
    vp9_dys_curbe_data  *curbe_cmd;

    curbe_cmd = i965_gpe_context_map_curbe(gpe_context);

    if (!curbe_cmd)
        return;

    memset(curbe_cmd, 0, sizeof(vp9_dys_curbe_data));

    curbe_cmd->dw0.input_frame_width    = curbe_param->input_width;
    curbe_cmd->dw0.input_frame_height   = curbe_param->input_height;

    curbe_cmd->dw1.output_frame_width   = curbe_param->output_width;
    curbe_cmd->dw1.output_frame_height  = curbe_param->output_height;

    curbe_cmd->dw2.delta_u                 = 1.0f / curbe_param->output_width;
    curbe_cmd->dw3.delta_v                 = 1.0f / curbe_param->output_height;

    curbe_cmd->dw16.input_frame_nv12_bti  = VP9_BTI_DYS_INPUT_NV12;
    curbe_cmd->dw17.output_frame_y_bti    = VP9_BTI_DYS_OUTPUT_Y;
    curbe_cmd->dw18.avs_sample_idx            = 0;

    i965_gpe_context_unmap_curbe(gpe_context);
}

static void
gen9_vp9_send_dys_surface(VADriverContextP ctx,
                          struct encode_state *encode_state,
                          struct i965_gpe_context *gpe_context,
                          struct intel_encoder_context *encoder_context,
                          struct gen9_vp9_dys_surface_param *surface_param)
{

    if (surface_param->input_frame)
        gen9_add_adv_gpe_surface(ctx,
                                 gpe_context,
                                 surface_param->input_frame,
                                 VP9_BTI_DYS_INPUT_NV12);

    if (surface_param->output_frame) {
        gen9_add_2d_gpe_surface(ctx,
                                gpe_context,
                                surface_param->output_frame,
                                0,
                                1,
                                I965_SURFACEFORMAT_R8_UNORM,
                                VP9_BTI_DYS_OUTPUT_Y);

        gen9_add_2d_gpe_surface(ctx,
                                gpe_context,
                                surface_param->output_frame,
                                1,
                                1,
                                I965_SURFACEFORMAT_R16_UINT,
                                VP9_BTI_DYS_OUTPUT_UV);
    }

    return;
}

static VAStatus
gen9_vp9_dys_kernel(VADriverContextP ctx,
                    struct encode_state *encode_state,
                    struct intel_encoder_context *encoder_context,
                    gen9_vp9_dys_kernel_param *dys_kernel_param)
{
    struct gen9_encoder_context_vp9 *vme_context = encoder_context->vme_context;
    struct i965_gpe_context *gpe_context;
    int media_function;
    struct gen9_vp9_dys_curbe_param                 curbe_param;
    struct gen9_vp9_dys_surface_param               surface_param;
    struct gpe_media_object_walker_parameter        media_object_walker_param;
    struct gpe_encoder_kernel_walker_parameter      kernel_walker_param;
    unsigned int                                    resolution_x, resolution_y;

    media_function = VP9_MEDIA_STATE_DYS;
    gpe_context = &vme_context->dys_context.gpe_context;

    //gen8_gpe_context_init(ctx, gpe_context);
    gen9_gpe_reset_binding_table(ctx, gpe_context);

    /* sampler state is configured only when initializing the GPE context */

    memset(&curbe_param, 0, sizeof(curbe_param));
    curbe_param.input_width   = dys_kernel_param->input_width;
    curbe_param.input_height  = dys_kernel_param->input_height;
    curbe_param.output_width = dys_kernel_param->output_width;
    curbe_param.output_height = dys_kernel_param->output_height;
    vme_context->pfn_set_curbe_dys(ctx, encode_state,
                                   gpe_context,
                                   encoder_context,
                                   &curbe_param);

    // Add surface states
    memset(&surface_param, 0, sizeof(surface_param));
    surface_param.input_frame = dys_kernel_param->input_surface;
    surface_param.output_frame = dys_kernel_param->output_surface;
    surface_param.vert_line_stride = 0;
    surface_param.vert_line_stride_offset = 0;

    vme_context->pfn_send_dys_surface(ctx,
                                      encode_state,
                                      gpe_context,
                                      encoder_context,
                                      &surface_param);

    resolution_x = ALIGN(dys_kernel_param->output_width, 16) / 16;
    resolution_y = ALIGN(dys_kernel_param->output_height, 16) / 16;

    gen8_gpe_setup_interface_data(ctx, gpe_context);

    memset(&kernel_walker_param, 0, sizeof(kernel_walker_param));
    kernel_walker_param.resolution_x = resolution_x;
    kernel_walker_param.resolution_y = resolution_y;
    kernel_walker_param.no_dependency = 1;

    gen9_init_media_object_walker_parameter(encoder_context, &kernel_walker_param, &media_object_walker_param);

    gen9_run_kernel_media_object_walker(ctx, encoder_context,
                                        gpe_context,
                                        media_function,
                                        &media_object_walker_param);

    return VA_STATUS_SUCCESS;
}

static VAStatus
gen9_vp9_run_dys_refframes(VADriverContextP ctx,
                           struct encode_state *encode_state,
                           struct intel_encoder_context *encoder_context)
{
    struct gen9_vp9_state *vp9_state;
    VAEncPictureParameterBufferVP9  *pic_param;
    gen9_vp9_dys_kernel_param dys_kernel_param;
    struct object_surface *obj_surface;
    struct object_surface *input_surface, *output_surface;
    struct gen9_surface_vp9 *vp9_priv_surface;

    vp9_state = (struct gen9_vp9_state *) encoder_context->enc_priv_state;

    if (!vp9_state || !vp9_state->pic_param)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    pic_param = vp9_state->pic_param;

    if ((pic_param->frame_width_src != pic_param->frame_width_dst) ||
        (pic_param->frame_height_src != pic_param->frame_height_dst)) {
        input_surface = encode_state->input_yuv_object;
        obj_surface = encode_state->reconstructed_object;
        vp9_priv_surface = (struct gen9_surface_vp9 *)(obj_surface->private_data);
        output_surface = vp9_priv_surface->dys_surface_obj;

        memset(&dys_kernel_param, 0, sizeof(dys_kernel_param));
        dys_kernel_param.input_width = pic_param->frame_width_src;
        dys_kernel_param.input_height = pic_param->frame_height_src;
        dys_kernel_param.input_surface = input_surface;
        dys_kernel_param.output_width = pic_param->frame_width_dst;
        dys_kernel_param.output_height = pic_param->frame_height_dst;
        dys_kernel_param.output_surface = output_surface;
        gen9_vp9_dys_kernel(ctx, encode_state,
                            encoder_context,
                            &dys_kernel_param);
    }

    if ((vp9_state->dys_ref_frame_flag & VP9_LAST_REF) &&
        vp9_state->last_ref_obj) {
        obj_surface = vp9_state->last_ref_obj;
        vp9_priv_surface = (struct gen9_surface_vp9 *)(obj_surface->private_data);

        input_surface = obj_surface;
        output_surface = vp9_priv_surface->dys_surface_obj;

        dys_kernel_param.input_width = vp9_priv_surface->frame_width;
        dys_kernel_param.input_height = vp9_priv_surface->frame_height;
        dys_kernel_param.input_surface = input_surface;

        dys_kernel_param.output_width = pic_param->frame_width_dst;
        dys_kernel_param.output_height = pic_param->frame_height_dst;
        dys_kernel_param.output_surface = output_surface;

        gen9_vp9_dys_kernel(ctx, encode_state,
                            encoder_context,
                            &dys_kernel_param);

        if (vp9_state->hme_enabled) {
            dys_kernel_param.input_width = ALIGN((vp9_priv_surface->frame_width / 4), 16);
            dys_kernel_param.input_width = ALIGN((vp9_priv_surface->frame_height / 4), 16);
            dys_kernel_param.input_surface = vp9_priv_surface->scaled_4x_surface_obj;

            dys_kernel_param.output_width = vp9_state->frame_width_4x;
            dys_kernel_param.output_height = vp9_state->frame_height_4x;
            dys_kernel_param.output_surface = vp9_priv_surface->dys_4x_surface_obj;

            gen9_vp9_dys_kernel(ctx, encode_state,
                                encoder_context,
                                &dys_kernel_param);

            /* Does it really need to do the 16x HME if the
             * resolution is different?
             * Maybe it should be restricted
             */
            if (vp9_state->b16xme_enabled) {
                dys_kernel_param.input_width = ALIGN((vp9_priv_surface->frame_width / 16), 16);
                dys_kernel_param.input_height = ALIGN((vp9_priv_surface->frame_height / 16), 16);
                dys_kernel_param.input_surface = vp9_priv_surface->scaled_16x_surface_obj;

                dys_kernel_param.output_width = vp9_state->frame_width_16x;
                dys_kernel_param.output_height = vp9_state->frame_height_16x;
                dys_kernel_param.output_surface = vp9_priv_surface->dys_16x_surface_obj;

                gen9_vp9_dys_kernel(ctx, encode_state,
                                    encoder_context,
                                    &dys_kernel_param);
            }
        }
    }

    if ((vp9_state->dys_ref_frame_flag & VP9_GOLDEN_REF) &&
        vp9_state->golden_ref_obj) {
        obj_surface = vp9_state->golden_ref_obj;
        vp9_priv_surface = (struct gen9_surface_vp9 *)(obj_surface->private_data);

        input_surface = obj_surface;
        output_surface = vp9_priv_surface->dys_surface_obj;

        dys_kernel_param.input_width = vp9_priv_surface->frame_width;
        dys_kernel_param.input_height = vp9_priv_surface->frame_height;
        dys_kernel_param.input_surface = input_surface;

        dys_kernel_param.output_width = pic_param->frame_width_dst;
        dys_kernel_param.output_height = pic_param->frame_height_dst;
        dys_kernel_param.output_surface = output_surface;

        gen9_vp9_dys_kernel(ctx, encode_state,
                            encoder_context,
                            &dys_kernel_param);

        if (vp9_state->hme_enabled) {
            dys_kernel_param.input_width = ALIGN((vp9_priv_surface->frame_width / 4), 16);
            dys_kernel_param.input_width = ALIGN((vp9_priv_surface->frame_height / 4), 16);
            dys_kernel_param.input_surface = vp9_priv_surface->scaled_4x_surface_obj;

            dys_kernel_param.output_width = vp9_state->frame_width_4x;
            dys_kernel_param.output_height = vp9_state->frame_height_4x;
            dys_kernel_param.output_surface = vp9_priv_surface->dys_4x_surface_obj;

            gen9_vp9_dys_kernel(ctx, encode_state,
                                encoder_context,
                                &dys_kernel_param);

            /* Does it really need to do the 16x HME if the
             * resolution is different?
             * Maybe it should be restricted
             */
            if (vp9_state->b16xme_enabled) {
                dys_kernel_param.input_width = ALIGN((vp9_priv_surface->frame_width / 16), 16);
                dys_kernel_param.input_height = ALIGN((vp9_priv_surface->frame_height / 16), 16);
                dys_kernel_param.input_surface = vp9_priv_surface->scaled_16x_surface_obj;

                dys_kernel_param.output_width = vp9_state->frame_width_16x;
                dys_kernel_param.output_height = vp9_state->frame_height_16x;
                dys_kernel_param.output_surface = vp9_priv_surface->dys_16x_surface_obj;

                gen9_vp9_dys_kernel(ctx, encode_state,
                                    encoder_context,
                                    &dys_kernel_param);
            }
        }
    }

    if ((vp9_state->dys_ref_frame_flag & VP9_ALT_REF) &&
        vp9_state->alt_ref_obj) {
        obj_surface = vp9_state->alt_ref_obj;
        vp9_priv_surface = (struct gen9_surface_vp9 *)(obj_surface->private_data);

        input_surface = obj_surface;
        output_surface = vp9_priv_surface->dys_surface_obj;

        dys_kernel_param.input_width = vp9_priv_surface->frame_width;
        dys_kernel_param.input_height = vp9_priv_surface->frame_height;
        dys_kernel_param.input_surface = input_surface;

        dys_kernel_param.output_width = pic_param->frame_width_dst;
        dys_kernel_param.output_height = pic_param->frame_height_dst;
        dys_kernel_param.output_surface = output_surface;

        gen9_vp9_dys_kernel(ctx, encode_state,
                            encoder_context,
                            &dys_kernel_param);

        if (vp9_state->hme_enabled) {
            dys_kernel_param.input_width = ALIGN((vp9_priv_surface->frame_width / 4), 16);
            dys_kernel_param.input_width = ALIGN((vp9_priv_surface->frame_height / 4), 16);
            dys_kernel_param.input_surface = vp9_priv_surface->scaled_4x_surface_obj;

            dys_kernel_param.output_width = vp9_state->frame_width_4x;
            dys_kernel_param.output_height = vp9_state->frame_height_4x;
            dys_kernel_param.output_surface = vp9_priv_surface->dys_4x_surface_obj;

            gen9_vp9_dys_kernel(ctx, encode_state,
                                encoder_context,
                                &dys_kernel_param);

            /* Does it really need to do the 16x HME if the
             * resolution is different?
             * Maybe it should be restricted
             */
            if (vp9_state->b16xme_enabled) {
                dys_kernel_param.input_width = ALIGN((vp9_priv_surface->frame_width / 16), 16);
                dys_kernel_param.input_height = ALIGN((vp9_priv_surface->frame_height / 16), 16);
                dys_kernel_param.input_surface = vp9_priv_surface->scaled_16x_surface_obj;

                dys_kernel_param.output_width = vp9_state->frame_width_16x;
                dys_kernel_param.output_height = vp9_state->frame_height_16x;
                dys_kernel_param.output_surface = vp9_priv_surface->dys_16x_surface_obj;

                gen9_vp9_dys_kernel(ctx, encode_state,
                                    encoder_context,
                                    &dys_kernel_param);
            }
        }
    }

    return VA_STATUS_SUCCESS;
}

static void
gen9_vp9_set_curbe_mbenc(VADriverContextP ctx,
                         struct encode_state *encode_state,
                         struct i965_gpe_context *gpe_context,
                         struct intel_encoder_context *encoder_context,
                         struct gen9_vp9_mbenc_curbe_param *curbe_param)
{
    struct gen9_vp9_state *vp9_state;
    VAEncMiscParameterTypeVP9PerSegmantParam *seg_param, tmp_seg_param;
    vp9_mbenc_curbe_data  *curbe_cmd;
    VAEncPictureParameterBufferVP9  *pic_param;
    int i, segment_count;
    int seg_qindex;
    struct object_surface *obj_surface;
    struct gen9_surface_vp9 *vp9_priv_surface;

    vp9_state = (struct gen9_vp9_state *) encoder_context->enc_priv_state;

    if (!vp9_state || !vp9_state->pic_param)
        return;

    pic_param = curbe_param->ppic_param;
    seg_param = curbe_param->psegment_param;

    if (!seg_param) {
        memset(&tmp_seg_param, 0, sizeof(tmp_seg_param));
        seg_param = &tmp_seg_param;
    }

    curbe_cmd = i965_gpe_context_map_curbe(gpe_context);

    if (!curbe_cmd)
        return;

    memset(curbe_cmd, 0, sizeof(vp9_mbenc_curbe_data));

    if (vp9_state->dys_in_use) {
        curbe_cmd->dw0.frame_width = pic_param->frame_width_dst;
        curbe_cmd->dw0.frame_height = pic_param->frame_height_dst;
    } else {
        curbe_cmd->dw0.frame_width = pic_param->frame_width_src;
        curbe_cmd->dw0.frame_height = pic_param->frame_height_src;
    }

    curbe_cmd->dw1.frame_type = curbe_param->picture_coding_type;

    curbe_cmd->dw1.segmentation_enable = pic_param->pic_flags.bits.segmentation_enabled;
    if (pic_param->pic_flags.bits.segmentation_enabled)
        segment_count = 8;
    else
        segment_count = 1;

    curbe_cmd->dw1.ref_frame_flags = curbe_param->ref_frame_flag;

    //right now set them to normal settings
    if (curbe_param->picture_coding_type) {
        switch (vp9_state->target_usage) {
        case INTEL_ENC_VP9_TU_QUALITY:
            curbe_cmd->dw1.min_16for32_check    = 0x00;
            curbe_cmd->dw2.multi_pred           = 0x02;
            curbe_cmd->dw2.len_sp               = 0x39;
            curbe_cmd->dw2.search_x             = 0x30;
            curbe_cmd->dw2.search_y             = 0x28;
            curbe_cmd->dw3.min_ref_for32_check = 0x01;
            curbe_cmd->dw4.skip16_threshold     = 0x000A;
            curbe_cmd->dw4.disable_mr_threshold = 0x000C;

            memcpy(&curbe_cmd->dw16,
                   vp9_diamond_ime_search_path_delta,
                   14 * sizeof(unsigned int));
            break;
        case INTEL_ENC_VP9_TU_PERFORMANCE:
            curbe_cmd->dw1.min_16for32_check    = 0x02;
            curbe_cmd->dw2.multi_pred           = 0x00;
            curbe_cmd->dw2.len_sp               = 0x10;
            curbe_cmd->dw2.search_x             = 0x20;
            curbe_cmd->dw2.search_y             = 0x20;
            curbe_cmd->dw3.min_ref_for32_check = 0x03;
            curbe_cmd->dw4.skip16_threshold     = 0x0014;
            curbe_cmd->dw4.disable_mr_threshold = 0x0016;

            memcpy(&curbe_cmd->dw16,
                   vp9_fullspiral_ime_search_path_delta,
                   14 * sizeof(unsigned int));

            break;
        default:  // normal settings
            curbe_cmd->dw1.min_16for32_check     = 0x01;
            curbe_cmd->dw2.multi_pred           = 0x00;
            curbe_cmd->dw2.len_sp               = 0x19;
            curbe_cmd->dw2.search_x             = 0x30;
            curbe_cmd->dw2.search_y             = 0x28;
            curbe_cmd->dw3.min_ref_for32_check = 0x02;
            curbe_cmd->dw4.skip16_threshold     = 0x000F;
            curbe_cmd->dw4.disable_mr_threshold = 0x0011;

            memcpy(&curbe_cmd->dw16,
                   vp9_diamond_ime_search_path_delta,
                   14 * sizeof(unsigned int));
            break;
        }

        curbe_cmd->dw3.hme_enabled               = curbe_param->hme_enabled;
        curbe_cmd->dw3.multi_ref_qp_check         = curbe_param->multi_ref_qp_check;
        // co-located predictor must be disabled when dynamic scaling is enabled
        curbe_cmd->dw3.disable_temp_pred    = vp9_state->dys_in_use;
    }

    curbe_cmd->dw5.inter_round = 0;
    curbe_cmd->dw5.intra_round = 4;
    curbe_cmd->dw5.frame_qpindex = pic_param->luma_ac_qindex;

    for (i = 0; i < segment_count; i++) {
        seg_qindex = pic_param->luma_ac_qindex + pic_param->luma_dc_qindex_delta
                     + seg_param->seg_data[i].segment_qindex_delta;

        seg_qindex = CLAMP(0, 255, seg_qindex);

        if (curbe_param->picture_coding_type)
            memcpy(&curbe_cmd->segments[i],
                   &intel_vp9_costlut_p[seg_qindex * 16],
                   16 * sizeof(unsigned int));
        else
            memcpy(&curbe_cmd->segments[i],
                   &intel_vp9_costlut_key[seg_qindex * 16],
                   16 * sizeof(unsigned int));
    }

    if (curbe_param->picture_coding_type) {
        if (curbe_cmd->dw3.multi_ref_qp_check) {
            if (curbe_param->ref_frame_flag & 0x01) {
                obj_surface = curbe_param->last_ref_obj;
                vp9_priv_surface = (struct gen9_surface_vp9 *)(obj_surface->private_data);
                curbe_cmd->dw8.last_ref_qp = vp9_quant_dc[vp9_priv_surface->qp_value];
            }

            if (curbe_param->ref_frame_flag & 0x02) {
                obj_surface = curbe_param->golden_ref_obj;
                vp9_priv_surface = (struct gen9_surface_vp9 *)(obj_surface->private_data);
                curbe_cmd->dw8.golden_ref_qp = vp9_quant_dc[vp9_priv_surface->qp_value];
            }

            if (curbe_param->ref_frame_flag & 0x04) {
                obj_surface = curbe_param->alt_ref_obj;
                vp9_priv_surface = (struct gen9_surface_vp9 *)(obj_surface->private_data);
                curbe_cmd->dw9.alt_ref_qp = vp9_quant_dc[vp9_priv_surface->qp_value];
            }
        }
    }
    curbe_cmd->dw160.enc_curr_y_surf_bti           = VP9_BTI_MBENC_CURR_Y_G9;
    curbe_cmd->dw162.enc_curr_nv12_surf_bti        = VP9_BTI_MBENC_CURR_NV12_G9;
    curbe_cmd->dw166.segmentation_map_bti          = VP9_BTI_MBENC_SEGMENTATION_MAP_G9;
    curbe_cmd->dw172.mode_decision_bti           = VP9_BTI_MBENC_MODE_DECISION_G9;
    curbe_cmd->dw167.tx_curbe_bti                = VP9_BTI_MBENC_TX_CURBE_G9;
    curbe_cmd->dw168.hme_mvdata_bti             = VP9_BTI_MBENC_HME_MV_DATA_G9;
    curbe_cmd->dw169.hme_distortion_bti          = VP9_BTI_MBENC_HME_DISTORTION_G9;
    curbe_cmd->dw171.mode_decision_prev_bti      = VP9_BTI_MBENC_MODE_DECISION_PREV_G9;
    curbe_cmd->dw172.mode_decision_bti           = VP9_BTI_MBENC_MODE_DECISION_G9;
    curbe_cmd->dw173.output_16x16_inter_modes_bti = VP9_BTI_MBENC_OUT_16x16_INTER_MODES_G9;
    curbe_cmd->dw174.cu_record_bti               = VP9_BTI_MBENC_CU_RECORDS_G9;
    curbe_cmd->dw175.pak_data_bti                = VP9_BTI_MBENC_PAK_DATA_G9;

    i965_gpe_context_unmap_curbe(gpe_context);
    return;
}

static void
gen9_vp9_send_mbenc_surface(VADriverContextP ctx,
                            struct encode_state *encode_state,
                            struct i965_gpe_context *gpe_context,
                            struct intel_encoder_context *encoder_context,
                            struct gen9_vp9_mbenc_surface_param *mbenc_param)
{
    struct gen9_vp9_state *vp9_state;
    unsigned int            res_size;
    unsigned int            frame_width_in_sb, frame_height_in_sb;
    struct object_surface   *obj_surface, *tmp_input;
    struct gen9_surface_vp9 *vp9_priv_surface;
    int media_function;

    vp9_state = (struct gen9_vp9_state *) encoder_context->enc_priv_state;

    if (!vp9_state || !vp9_state->pic_param)
        return;

    frame_width_in_sb = ALIGN(mbenc_param->frame_width, 64) / 64;
    frame_height_in_sb = ALIGN(mbenc_param->frame_height, 64) / 64;
    media_function = mbenc_param->media_state_type;

    switch (media_function) {
    case VP9_MEDIA_STATE_MBENC_I_32x32: {
        obj_surface = mbenc_param->curr_frame_obj;

        gen9_add_2d_gpe_surface(ctx,
                                gpe_context,
                                obj_surface,
                                0,
                                1,
                                I965_SURFACEFORMAT_R8_UNORM,
                                VP9_BTI_MBENC_CURR_Y_G9);

        gen9_add_2d_gpe_surface(ctx,
                                gpe_context,
                                obj_surface,
                                1,
                                1,
                                I965_SURFACEFORMAT_R16_UINT,
                                VP9_BTI_MBENC_CURR_UV_G9);


        if (mbenc_param->segmentation_enabled) {
            gen9_add_buffer_2d_gpe_surface(ctx,
                                           gpe_context,
                                           mbenc_param->pres_segmentation_map,
                                           1,
                                           I965_SURFACEFORMAT_R8_UNORM,
                                           VP9_BTI_MBENC_SEGMENTATION_MAP_G9);

        }

        res_size = 16 * mbenc_param->frame_width_in_mb *
                   mbenc_param->frame_height_in_mb * sizeof(unsigned int);
        gen9_add_buffer_gpe_surface(ctx,
                                    gpe_context,
                                    mbenc_param->pres_mode_decision,
                                    0,
                                    res_size / 4,
                                    0,
                                    VP9_BTI_MBENC_MODE_DECISION_G9);

        break;
    }
    case VP9_MEDIA_STATE_MBENC_I_16x16: {
        obj_surface = mbenc_param->curr_frame_obj;

        gen9_add_2d_gpe_surface(ctx,
                                gpe_context,
                                obj_surface,
                                0,
                                1,
                                I965_SURFACEFORMAT_R8_UNORM,
                                VP9_BTI_MBENC_CURR_Y_G9);

        gen9_add_2d_gpe_surface(ctx,
                                gpe_context,
                                obj_surface,
                                1,
                                1,
                                I965_SURFACEFORMAT_R16_UINT,
                                VP9_BTI_MBENC_CURR_UV_G9);

        gen9_add_adv_gpe_surface(ctx, gpe_context,
                                 obj_surface,
                                 VP9_BTI_MBENC_CURR_NV12_G9);

        if (mbenc_param->segmentation_enabled) {
            gen9_add_buffer_2d_gpe_surface(ctx,
                                           gpe_context,
                                           mbenc_param->pres_segmentation_map,
                                           1,
                                           I965_SURFACEFORMAT_R8_UNORM,
                                           VP9_BTI_MBENC_SEGMENTATION_MAP_G9);

        }

        res_size = 16 * mbenc_param->frame_width_in_mb *
                   mbenc_param->frame_height_in_mb * sizeof(unsigned int);
        gen9_add_buffer_gpe_surface(ctx,
                                    gpe_context,
                                    mbenc_param->pres_mode_decision,
                                    0,
                                    res_size / 4,
                                    0,
                                    VP9_BTI_MBENC_MODE_DECISION_G9);

        res_size = 160;

        gen9_add_dri_buffer_gpe_surface(ctx,
                                        gpe_context,
                                        mbenc_param->gpe_context_tx->curbe.bo,
                                        0,
                                        ALIGN(res_size, 64),
                                        mbenc_param->gpe_context_tx->curbe.offset,
                                        VP9_BTI_MBENC_TX_CURBE_G9);

        break;
    }
    case VP9_MEDIA_STATE_MBENC_P: {
        obj_surface = mbenc_param->curr_frame_obj;

        gen9_add_2d_gpe_surface(ctx,
                                gpe_context,
                                obj_surface,
                                0,
                                1,
                                I965_SURFACEFORMAT_R8_UNORM,
                                VP9_BTI_MBENC_CURR_Y_G9);

        gen9_add_2d_gpe_surface(ctx, gpe_context,
                                obj_surface,
                                1,
                                1,
                                I965_SURFACEFORMAT_R16_UINT,
                                VP9_BTI_MBENC_CURR_UV_G9);

        gen9_add_adv_gpe_surface(ctx, gpe_context,
                                 obj_surface,
                                 VP9_BTI_MBENC_CURR_NV12_G9);

        if (mbenc_param->last_ref_obj) {
            obj_surface = mbenc_param->last_ref_obj;
            vp9_priv_surface = (struct gen9_surface_vp9 *)(obj_surface->private_data);

            if (vp9_state->dys_in_use &&
                ((vp9_priv_surface->frame_width != vp9_state->frame_width) ||
                 (vp9_priv_surface->frame_height != vp9_state->frame_height)))
                tmp_input = vp9_priv_surface->dys_surface_obj;
            else
                tmp_input = obj_surface;

            gen9_add_adv_gpe_surface(ctx, gpe_context,
                                     tmp_input,
                                     VP9_BTI_MBENC_LAST_NV12_G9);

            gen9_add_adv_gpe_surface(ctx, gpe_context,
                                     tmp_input,
                                     VP9_BTI_MBENC_LAST_NV12_G9 + 1);

        }

        if (mbenc_param->golden_ref_obj) {
            obj_surface = mbenc_param->golden_ref_obj;
            vp9_priv_surface = (struct gen9_surface_vp9 *)(obj_surface->private_data);

            if (vp9_state->dys_in_use &&
                ((vp9_priv_surface->frame_width != vp9_state->frame_width) ||
                 (vp9_priv_surface->frame_height != vp9_state->frame_height)))
                tmp_input = vp9_priv_surface->dys_surface_obj;
            else
                tmp_input = obj_surface;

            gen9_add_adv_gpe_surface(ctx, gpe_context,
                                     tmp_input,
                                     VP9_BTI_MBENC_GOLD_NV12_G9);

            gen9_add_adv_gpe_surface(ctx, gpe_context,
                                     tmp_input,
                                     VP9_BTI_MBENC_GOLD_NV12_G9 + 1);

        }

        if (mbenc_param->alt_ref_obj) {
            obj_surface = mbenc_param->alt_ref_obj;
            vp9_priv_surface = (struct gen9_surface_vp9 *)(obj_surface->private_data);

            if (vp9_state->dys_in_use &&
                ((vp9_priv_surface->frame_width != vp9_state->frame_width) ||
                 (vp9_priv_surface->frame_height != vp9_state->frame_height)))
                tmp_input = vp9_priv_surface->dys_surface_obj;
            else
                tmp_input = obj_surface;

            gen9_add_adv_gpe_surface(ctx, gpe_context,
                                     tmp_input,
                                     VP9_BTI_MBENC_ALTREF_NV12_G9);

            gen9_add_adv_gpe_surface(ctx, gpe_context,
                                     tmp_input,
                                     VP9_BTI_MBENC_ALTREF_NV12_G9 + 1);

        }

        if (mbenc_param->hme_enabled) {
            gen9_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                           mbenc_param->ps4x_memv_data_buffer,
                                           1,
                                           I965_SURFACEFORMAT_R8_UNORM,
                                           VP9_BTI_MBENC_HME_MV_DATA_G9);

            gen9_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                           mbenc_param->ps4x_memv_distortion_buffer,
                                           1,
                                           I965_SURFACEFORMAT_R8_UNORM,
                                           VP9_BTI_MBENC_HME_DISTORTION_G9);
        }

        if (mbenc_param->segmentation_enabled) {
            gen9_add_buffer_2d_gpe_surface(ctx,
                                           gpe_context,
                                           mbenc_param->pres_segmentation_map,
                                           1,
                                           I965_SURFACEFORMAT_R8_UNORM,
                                           VP9_BTI_MBENC_SEGMENTATION_MAP_G9);

        }

        res_size = 16 * mbenc_param->frame_width_in_mb *
                   mbenc_param->frame_height_in_mb * sizeof(unsigned int);
        gen9_add_buffer_gpe_surface(ctx,
                                    gpe_context,
                                    mbenc_param->pres_mode_decision_prev,
                                    0,
                                    res_size / 4,
                                    0,
                                    VP9_BTI_MBENC_MODE_DECISION_PREV_G9);

        gen9_add_buffer_gpe_surface(ctx,
                                    gpe_context,
                                    mbenc_param->pres_mode_decision,
                                    0,
                                    res_size / 4,
                                    0,
                                    VP9_BTI_MBENC_MODE_DECISION_G9);

        gen9_add_buffer_2d_gpe_surface(ctx,
                                       gpe_context,
                                       mbenc_param->pres_output_16x16_inter_modes,
                                       1,
                                       I965_SURFACEFORMAT_R8_UNORM,
                                       VP9_BTI_MBENC_OUT_16x16_INTER_MODES_G9);

        res_size = 160;

        gen9_add_dri_buffer_gpe_surface(ctx,
                                        gpe_context,
                                        mbenc_param->gpe_context_tx->curbe.bo,
                                        0,
                                        ALIGN(res_size, 64),
                                        mbenc_param->gpe_context_tx->curbe.offset,
                                        VP9_BTI_MBENC_TX_CURBE_G9);


        break;
    }
    case VP9_MEDIA_STATE_MBENC_TX: {
        obj_surface = mbenc_param->curr_frame_obj;

        gen9_add_2d_gpe_surface(ctx,
                                gpe_context,
                                obj_surface,
                                0,
                                1,
                                I965_SURFACEFORMAT_R8_UNORM,
                                VP9_BTI_MBENC_CURR_Y_G9);

        gen9_add_2d_gpe_surface(ctx,
                                gpe_context,
                                obj_surface,
                                1,
                                1,
                                I965_SURFACEFORMAT_R16_UINT,
                                VP9_BTI_MBENC_CURR_UV_G9);

        if (mbenc_param->segmentation_enabled) {
            gen9_add_buffer_2d_gpe_surface(ctx,
                                           gpe_context,
                                           mbenc_param->pres_segmentation_map,
                                           1,
                                           I965_SURFACEFORMAT_R8_UNORM,
                                           VP9_BTI_MBENC_SEGMENTATION_MAP_G9);

        }

        res_size = 16 * mbenc_param->frame_width_in_mb *
                   mbenc_param->frame_height_in_mb * sizeof(unsigned int);
        gen9_add_buffer_gpe_surface(ctx,
                                    gpe_context,
                                    mbenc_param->pres_mode_decision,
                                    0,
                                    res_size / 4,
                                    0,
                                    VP9_BTI_MBENC_MODE_DECISION_G9);

        res_size = frame_width_in_sb * frame_height_in_sb * 4 * sizeof(unsigned int);
        gen9_add_buffer_gpe_surface(ctx,
                                    gpe_context,
                                    mbenc_param->pres_mb_code_surface,
                                    0,
                                    res_size / 4,
                                    0,
                                    VP9_BTI_MBENC_PAK_DATA_G9);

        // CU Record
        res_size = frame_width_in_sb * frame_height_in_sb *
                   64 * 16 * sizeof(unsigned int);

        gen9_add_buffer_gpe_surface(ctx,
                                    gpe_context,
                                    mbenc_param->pres_mb_code_surface,
                                    0,
                                    res_size / 4,
                                    mbenc_param->mb_data_offset,
                                    VP9_BTI_MBENC_CU_RECORDS_G9);
    }
    default:
        break;
    }

    return;
}

static VAStatus
gen9_vp9_mbenc_kernel(VADriverContextP ctx,
                      struct encode_state *encode_state,
                      struct intel_encoder_context *encoder_context,
                      int media_function)
{
    struct gen9_encoder_context_vp9 *vme_context = encoder_context->vme_context;
    struct i965_gpe_context *gpe_context, *tx_gpe_context;
    struct gpe_media_object_walker_parameter        media_object_walker_param;
    struct gpe_encoder_kernel_walker_parameter      kernel_walker_param;
    unsigned int    resolution_x, resolution_y;
    struct gen9_vp9_state *vp9_state;
    VAEncPictureParameterBufferVP9  *pic_param;
    struct gen9_vp9_mbenc_curbe_param               curbe_param;
    struct gen9_vp9_mbenc_surface_param             surface_param;
    VAStatus    va_status = VA_STATUS_SUCCESS;
    int mbenc_gpe_index = 0;
    struct object_surface *obj_surface;
    struct gen9_surface_vp9 *vp9_priv_surface;

    vp9_state = (struct gen9_vp9_state *) encoder_context->enc_priv_state;

    if (!vp9_state || !vp9_state->pic_param)
        return VA_STATUS_ERROR_ENCODING_ERROR;

    pic_param = vp9_state->pic_param;

    switch (media_function) {
    case VP9_MEDIA_STATE_MBENC_I_32x32:
        mbenc_gpe_index = VP9_MBENC_IDX_KEY_32x32;
        break;

    case VP9_MEDIA_STATE_MBENC_I_16x16:
        mbenc_gpe_index = VP9_MBENC_IDX_KEY_16x16;
        break;

    case VP9_MEDIA_STATE_MBENC_P:
        mbenc_gpe_index = VP9_MBENC_IDX_INTER;
        break;

    case VP9_MEDIA_STATE_MBENC_TX:
        mbenc_gpe_index = VP9_MBENC_IDX_TX;
        break;

    default:
        va_status = VA_STATUS_ERROR_OPERATION_FAILED;
        return va_status;
    }

    gpe_context = &(vme_context->mbenc_context.gpe_contexts[mbenc_gpe_index]);
    tx_gpe_context = &(vme_context->mbenc_context.gpe_contexts[VP9_MBENC_IDX_TX]);

    gen9_gpe_reset_binding_table(ctx, gpe_context);

    // Set curbe
    if (!vp9_state->mbenc_curbe_set_in_brc_update) {
        if (media_function == VP9_MEDIA_STATE_MBENC_I_32x32 ||
            media_function == VP9_MEDIA_STATE_MBENC_P) {
            memset(&curbe_param, 0, sizeof(curbe_param));
            curbe_param.ppic_param            = vp9_state->pic_param;
            curbe_param.pseq_param            = vp9_state->seq_param;
            curbe_param.psegment_param        = vp9_state->segment_param;
            curbe_param.frame_width_in_mb     = vp9_state->frame_width_in_mb;
            curbe_param.frame_height_in_mb    = vp9_state->frame_height_in_mb;
            curbe_param.last_ref_obj          = vp9_state->last_ref_obj;
            curbe_param.golden_ref_obj        = vp9_state->golden_ref_obj;
            curbe_param.alt_ref_obj           = vp9_state->alt_ref_obj;
            curbe_param.hme_enabled           = vp9_state->hme_enabled;
            curbe_param.ref_frame_flag        = vp9_state->ref_frame_flag;
            curbe_param.picture_coding_type   = vp9_state->picture_coding_type;
            curbe_param.media_state_type      = media_function;
            curbe_param.mbenc_curbe_set_in_brc_update = vp9_state->mbenc_curbe_set_in_brc_update;

            vme_context->pfn_set_curbe_mbenc(ctx,
                                             encode_state,
                                             gpe_context,
                                             encoder_context,
                                             &curbe_param);
        }
    }

    memset(&surface_param, 0, sizeof(surface_param));
    surface_param.media_state_type             = media_function;
    surface_param.picture_coding_type          = vp9_state->picture_coding_type;
    surface_param.frame_width                  = vp9_state->frame_width;
    surface_param.frame_height                 = vp9_state->frame_height;
    surface_param.frame_width_in_mb            = vp9_state->frame_width_in_mb;
    surface_param.frame_height_in_mb           = vp9_state->frame_height_in_mb;
    surface_param.hme_enabled                  = vp9_state->hme_enabled;
    surface_param.segmentation_enabled         = pic_param->pic_flags.bits.segmentation_enabled;
    surface_param.pres_segmentation_map        = &vme_context->mb_segment_map_surface;
    surface_param.ps4x_memv_data_buffer        = &vme_context->s4x_memv_data_buffer;
    surface_param.ps4x_memv_distortion_buffer  = &vme_context->s4x_memv_distortion_buffer;
    surface_param.pres_mode_decision           =
        &vme_context->res_mode_decision[vp9_state->curr_mode_decision_index];
    surface_param.pres_mode_decision_prev      =
        &vme_context->res_mode_decision[!vp9_state->curr_mode_decision_index];
    surface_param.pres_output_16x16_inter_modes = &vme_context->res_output_16x16_inter_modes;
    surface_param.pres_mbenc_curbe_buffer      = NULL;
    surface_param.last_ref_obj               = vp9_state->last_ref_obj;
    surface_param.golden_ref_obj             = vp9_state->golden_ref_obj;
    surface_param.alt_ref_obj                  = vp9_state->alt_ref_obj;
    surface_param.pres_mb_code_surface         = &vme_context->res_mb_code_surface;
    surface_param.gpe_context_tx               = tx_gpe_context;
    surface_param.mb_data_offset             = vp9_state->mb_data_offset;

    obj_surface = encode_state->reconstructed_object;
    vp9_priv_surface = (struct gen9_surface_vp9 *)(obj_surface->private_data);
    if (vp9_state->dys_in_use &&
        (pic_param->frame_width_src != pic_param->frame_height_dst ||
         pic_param->frame_height_src != pic_param->frame_height_dst)) {
        obj_surface = vp9_priv_surface->dys_surface_obj;
    } else
        obj_surface = encode_state->input_yuv_object;

    surface_param.curr_frame_obj             = obj_surface;

    vme_context->pfn_send_mbenc_surface(ctx,
                                        encode_state,
                                        gpe_context,
                                        encoder_context,
                                        &surface_param);

    if (media_function == VP9_MEDIA_STATE_MBENC_I_32x32) {
        resolution_x = ALIGN(vp9_state->frame_width, 32) / 32;
        resolution_y = ALIGN(vp9_state->frame_height, 32) / 32;
    } else {
        resolution_x = ALIGN(vp9_state->frame_width, 16) / 16;
        resolution_y = ALIGN(vp9_state->frame_height, 16) / 16;
    }

    memset(&kernel_walker_param, 0, sizeof(kernel_walker_param));
    kernel_walker_param.resolution_x = resolution_x;
    kernel_walker_param.resolution_y = resolution_y;

    if (media_function == VP9_MEDIA_STATE_MBENC_P ||
        media_function == VP9_MEDIA_STATE_MBENC_I_16x16) {
        kernel_walker_param.use_scoreboard = 1;
        kernel_walker_param.no_dependency = 0;
        kernel_walker_param.walker_degree = VP9_45Z_DEGREE;
    } else {
        kernel_walker_param.use_scoreboard = 0;
        kernel_walker_param.no_dependency = 1;
    }

    gen8_gpe_setup_interface_data(ctx, gpe_context);

    gen9_init_media_object_walker_parameter(encoder_context, &kernel_walker_param, &media_object_walker_param);

    gen9_run_kernel_media_object_walker(ctx, encoder_context,
                                        gpe_context,
                                        media_function,
                                        &media_object_walker_param);
    return va_status;
}

static void
gen9_init_gpe_context_vp9(VADriverContextP ctx,
                          struct i965_gpe_context *gpe_context,
                          struct vp9_encoder_kernel_parameter *kernel_param)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);

    gpe_context->curbe.length = kernel_param->curbe_size; // in bytes

    gpe_context->sampler.entry_size = 0;
    gpe_context->sampler.max_entries = 0;

    if (kernel_param->sampler_size) {
        gpe_context->sampler.entry_size = ALIGN(kernel_param->sampler_size, 64);
        gpe_context->sampler.max_entries = 1;
    }

    gpe_context->idrt.entry_size = ALIGN(sizeof(struct gen8_interface_descriptor_data), 64); // 8 dws, 1 register
    gpe_context->idrt.max_entries = NUM_KERNELS_PER_GPE_CONTEXT;

    gpe_context->surface_state_binding_table.max_entries = MAX_VP9_ENCODER_SURFACES;
    gpe_context->surface_state_binding_table.binding_table_offset = 0;
    gpe_context->surface_state_binding_table.surface_state_offset = ALIGN(MAX_VP9_ENCODER_SURFACES * 4, 64);
    gpe_context->surface_state_binding_table.length = ALIGN(MAX_VP9_ENCODER_SURFACES * 4, 64) + ALIGN(MAX_VP9_ENCODER_SURFACES * SURFACE_STATE_PADDED_SIZE_GEN9, 64);

    if (i965->intel.eu_total > 0)
        gpe_context->vfe_state.max_num_threads = 6 * i965->intel.eu_total;
    else
        gpe_context->vfe_state.max_num_threads = 112; // 16 EU * 7 threads

    gpe_context->vfe_state.curbe_allocation_size = MAX(1, ALIGN(gpe_context->curbe.length, 32) >> 5); // in registers
    gpe_context->vfe_state.urb_entry_size = MAX(1, ALIGN(kernel_param->inline_data_size, 32) >> 5); // in registers
    gpe_context->vfe_state.num_urb_entries = (MAX_URB_SIZE -
                                              gpe_context->vfe_state.curbe_allocation_size -
                                              ((gpe_context->idrt.entry_size >> 5) *
                                               gpe_context->idrt.max_entries)) / gpe_context->vfe_state.urb_entry_size;
    gpe_context->vfe_state.num_urb_entries = CLAMP(1, 127, gpe_context->vfe_state.num_urb_entries);
    gpe_context->vfe_state.gpgpu_mode = 0;
}

static void
gen9_init_vfe_scoreboard_vp9(struct i965_gpe_context *gpe_context,
                             struct vp9_encoder_scoreboard_parameter *scoreboard_param)
{
    gpe_context->vfe_desc5.scoreboard0.mask = scoreboard_param->mask;
    gpe_context->vfe_desc5.scoreboard0.type = scoreboard_param->type;
    gpe_context->vfe_desc5.scoreboard0.enable = scoreboard_param->enable;

    if (scoreboard_param->walkpat_flag) {
        gpe_context->vfe_desc5.scoreboard0.mask = 0x0F;
        gpe_context->vfe_desc5.scoreboard0.type = 1;

        gpe_context->vfe_desc6.scoreboard1.delta_x0 = 0x0;
        gpe_context->vfe_desc6.scoreboard1.delta_y0 = 0xF;

        gpe_context->vfe_desc6.scoreboard1.delta_x1 = 0x0;
        gpe_context->vfe_desc6.scoreboard1.delta_y1 = 0xE;

        gpe_context->vfe_desc6.scoreboard1.delta_x2 = 0xF;
        gpe_context->vfe_desc6.scoreboard1.delta_y2 = 0x3;

        gpe_context->vfe_desc6.scoreboard1.delta_x3 = 0xF;
        gpe_context->vfe_desc6.scoreboard1.delta_y3 = 0x1;
    } else {
        // Scoreboard 0
        gpe_context->vfe_desc6.scoreboard1.delta_x0 = 0xF;
        gpe_context->vfe_desc6.scoreboard1.delta_y0 = 0x0;

        // Scoreboard 1
        gpe_context->vfe_desc6.scoreboard1.delta_x1 = 0x0;
        gpe_context->vfe_desc6.scoreboard1.delta_y1 = 0xF;

        // Scoreboard 2
        gpe_context->vfe_desc6.scoreboard1.delta_x2 = 0x1;
        gpe_context->vfe_desc6.scoreboard1.delta_y2 = 0xF;

        // Scoreboard 3
        gpe_context->vfe_desc6.scoreboard1.delta_x3 = 0xF;
        gpe_context->vfe_desc6.scoreboard1.delta_y3 = 0xF;

        // Scoreboard 4
        gpe_context->vfe_desc7.scoreboard2.delta_x4 = 0xF;
        gpe_context->vfe_desc7.scoreboard2.delta_y4 = 0x1;

        // Scoreboard 5
        gpe_context->vfe_desc7.scoreboard2.delta_x5 = 0x0;
        gpe_context->vfe_desc7.scoreboard2.delta_y5 = 0xE;

        // Scoreboard 6
        gpe_context->vfe_desc7.scoreboard2.delta_x6 = 0x1;
        gpe_context->vfe_desc7.scoreboard2.delta_y6 = 0xE;

        // Scoreboard 7
        gpe_context->vfe_desc7.scoreboard2.delta_x6 = 0xF;
        gpe_context->vfe_desc7.scoreboard2.delta_y6 = 0xE;
    }
}

#define VP9_VME_REF_WIN       48

static VAStatus
gen9_encode_vp9_check_parameter(VADriverContextP ctx,
                                struct encode_state *encode_state,
                                struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct gen9_vp9_state *vp9_state;
    VAEncPictureParameterBufferVP9  *pic_param;
    VAEncMiscParameterTypeVP9PerSegmantParam *seg_param;
    VAEncSequenceParameterBufferVP9 *seq_param;
    struct object_surface *obj_surface;
    struct object_buffer *obj_buffer;
    struct gen9_surface_vp9 *vp9_priv_surface;

    vp9_state = (struct gen9_vp9_state *) encoder_context->enc_priv_state;

    if (!encode_state->pic_param_ext ||
        !encode_state->pic_param_ext->buffer) {
        return VA_STATUS_ERROR_INVALID_PARAMETER;
    }
    pic_param = (VAEncPictureParameterBufferVP9 *)encode_state->pic_param_ext->buffer;

    obj_buffer = BUFFER(pic_param->coded_buf);

    if (!obj_buffer ||
        !obj_buffer->buffer_store ||
        !obj_buffer->buffer_store->bo)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    encode_state->coded_buf_object = obj_buffer;

    vp9_state->status_buffer.bo = obj_buffer->buffer_store->bo;

    encode_state->reconstructed_object = SURFACE(pic_param->reconstructed_frame);

    if (!encode_state->reconstructed_object ||
        !encode_state->input_yuv_object)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    vp9_state->curr_frame = pic_param->reconstructed_frame;
    vp9_state->ref_frame_flag = 0;
    if (pic_param->pic_flags.bits.frame_type == KEY_FRAME ||
        pic_param->pic_flags.bits.intra_only) {
        /* this will be regarded as I-frame type */
        vp9_state->picture_coding_type = 0;
        vp9_state->last_ref_obj = NULL;
        vp9_state->golden_ref_obj = NULL;
        vp9_state->alt_ref_obj = NULL;
    } else {
        vp9_state->picture_coding_type = 1;
        vp9_state->ref_frame_flag = pic_param->ref_flags.bits.ref_frame_ctrl_l0 |
                                    pic_param->ref_flags.bits.ref_frame_ctrl_l1;

        obj_surface = SURFACE(pic_param->reference_frames[pic_param->ref_flags.bits.ref_last_idx]);
        vp9_state->last_ref_obj = obj_surface;
        if (!obj_surface ||
            !obj_surface->bo ||
            !obj_surface->private_data) {
            vp9_state->last_ref_obj = NULL;
            vp9_state->ref_frame_flag &= ~(VP9_LAST_REF);
        }

        obj_surface = SURFACE(pic_param->reference_frames[pic_param->ref_flags.bits.ref_gf_idx]);
        vp9_state->golden_ref_obj = obj_surface;
        if (!obj_surface ||
            !obj_surface->bo ||
            !obj_surface->private_data) {
            vp9_state->golden_ref_obj = NULL;
            vp9_state->ref_frame_flag &= ~(VP9_GOLDEN_REF);
        }

        obj_surface = SURFACE(pic_param->reference_frames[pic_param->ref_flags.bits.ref_arf_idx]);
        vp9_state->alt_ref_obj = obj_surface;
        if (!obj_surface ||
            !obj_surface->bo ||
            !obj_surface->private_data) {
            vp9_state->alt_ref_obj = NULL;
            vp9_state->ref_frame_flag &= ~(VP9_ALT_REF);
        }

        /* remove the duplicated flag and ref frame list */
        if (vp9_state->ref_frame_flag & VP9_LAST_REF) {
            if (pic_param->reference_frames[pic_param->ref_flags.bits.ref_last_idx] ==
                pic_param->reference_frames[pic_param->ref_flags.bits.ref_gf_idx]) {
                vp9_state->ref_frame_flag &= ~(VP9_GOLDEN_REF);
                vp9_state->golden_ref_obj = NULL;
            }

            if (pic_param->reference_frames[pic_param->ref_flags.bits.ref_last_idx] ==
                pic_param->reference_frames[pic_param->ref_flags.bits.ref_arf_idx]) {
                vp9_state->ref_frame_flag &= ~(VP9_ALT_REF);
                vp9_state->alt_ref_obj = NULL;
            }
        }

        if (vp9_state->ref_frame_flag & VP9_GOLDEN_REF) {
            if (pic_param->reference_frames[pic_param->ref_flags.bits.ref_gf_idx] ==
                pic_param->reference_frames[pic_param->ref_flags.bits.ref_arf_idx]) {
                vp9_state->ref_frame_flag &= ~(VP9_ALT_REF);
                vp9_state->alt_ref_obj = NULL;
            }
        }

        if (vp9_state->ref_frame_flag == 0)
            return VA_STATUS_ERROR_INVALID_PARAMETER;
    }

    seg_param = NULL;
    if (pic_param->pic_flags.bits.segmentation_enabled) {
        if (!encode_state->q_matrix ||
            !encode_state->q_matrix->buffer) {
            return VA_STATUS_ERROR_INVALID_PARAMETER;
        }
        seg_param = (VAEncMiscParameterTypeVP9PerSegmantParam *)
                    encode_state->q_matrix->buffer;
    }

    seq_param = NULL;
    if (encode_state->seq_param_ext &&
        encode_state->seq_param_ext->buffer)
        seq_param = (VAEncSequenceParameterBufferVP9 *)encode_state->seq_param_ext->buffer;

    if (!seq_param) {
        seq_param = &vp9_state->bogus_seq_param;
    }

    vp9_state->pic_param = pic_param;
    vp9_state->segment_param = seg_param;
    vp9_state->seq_param = seq_param;

    obj_surface = encode_state->reconstructed_object;
    if (pic_param->frame_width_dst > obj_surface->orig_width ||
        pic_param->frame_height_dst > obj_surface->orig_height)
        return VA_STATUS_ERROR_INVALID_SURFACE;

    if (!vp9_state->dys_enabled &&
        ((pic_param->frame_width_src != pic_param->frame_width_dst) ||
         (pic_param->frame_height_src != pic_param->frame_height_dst)))
        return VA_STATUS_ERROR_UNIMPLEMENTED;

    if (vp9_state->brc_enabled) {
        if (vp9_state->first_frame || vp9_state->picture_coding_type == KEY_FRAME) {
            vp9_state->brc_reset = encoder_context->brc.need_reset || vp9_state->first_frame;

            if (!encoder_context->brc.framerate[0].num || !encoder_context->brc.framerate[0].den ||
                !encoder_context->brc.bits_per_second[0])
                return VA_STATUS_ERROR_INVALID_PARAMETER;

            vp9_state->gop_size = encoder_context->brc.gop_size;
            vp9_state->framerate = encoder_context->brc.framerate[0];

            if (encoder_context->rate_control_mode == VA_RC_CBR ||
                !encoder_context->brc.target_percentage[0]) {
                vp9_state->target_bit_rate = encoder_context->brc.bits_per_second[0];
                vp9_state->max_bit_rate = vp9_state->target_bit_rate;
                vp9_state->min_bit_rate = vp9_state->target_bit_rate;
            } else {
                vp9_state->max_bit_rate = encoder_context->brc.bits_per_second[0];
                vp9_state->target_bit_rate = vp9_state->max_bit_rate * encoder_context->brc.target_percentage[0] / 100;
                if (2 * vp9_state->target_bit_rate < vp9_state->max_bit_rate)
                    vp9_state->min_bit_rate = 0;
                else
                    vp9_state->min_bit_rate = 2 * vp9_state->target_bit_rate - vp9_state->max_bit_rate;
            }

            if (encoder_context->brc.hrd_buffer_size)
                vp9_state->vbv_buffer_size_in_bit = encoder_context->brc.hrd_buffer_size;
            else if (encoder_context->brc.window_size)
                vp9_state->vbv_buffer_size_in_bit = (uint64_t)vp9_state->max_bit_rate * encoder_context->brc.window_size / 1000;
            else
                vp9_state->vbv_buffer_size_in_bit = vp9_state->max_bit_rate;
            if (encoder_context->brc.hrd_initial_buffer_fullness)
                vp9_state->init_vbv_buffer_fullness_in_bit = encoder_context->brc.hrd_initial_buffer_fullness;
            else
                vp9_state->init_vbv_buffer_fullness_in_bit = vp9_state->vbv_buffer_size_in_bit / 2;
        }
    }

    vp9_state->frame_width = pic_param->frame_width_dst;
    vp9_state->frame_height = pic_param->frame_height_dst;

    vp9_state->frame_width_4x = ALIGN(vp9_state->frame_width / 4, 16);
    vp9_state->frame_height_4x = ALIGN(vp9_state->frame_height / 4, 16);

    vp9_state->frame_width_16x = ALIGN(vp9_state->frame_width / 16, 16);
    vp9_state->frame_height_16x = ALIGN(vp9_state->frame_height / 16, 16);

    vp9_state->frame_width_in_mb = ALIGN(vp9_state->frame_width, 16) / 16;
    vp9_state->frame_height_in_mb = ALIGN(vp9_state->frame_height, 16) / 16;

    vp9_state->downscaled_width_4x_in_mb = vp9_state->frame_width_4x / 16;
    vp9_state->downscaled_height_4x_in_mb = vp9_state->frame_height_4x / 16;
    vp9_state->downscaled_width_16x_in_mb = vp9_state->frame_width_16x / 16;
    vp9_state->downscaled_height_16x_in_mb = vp9_state->frame_height_16x / 16;

    vp9_state->dys_in_use = 0;
    if (pic_param->frame_width_src != pic_param->frame_width_dst ||
        pic_param->frame_height_src != pic_param->frame_height_dst)
        vp9_state->dys_in_use = 1;
    vp9_state->dys_ref_frame_flag = 0;
    /* check the dys setting. The dys is supported by default. */
    if (pic_param->pic_flags.bits.frame_type != KEY_FRAME &&
        !pic_param->pic_flags.bits.intra_only) {
        vp9_state->dys_ref_frame_flag = vp9_state->ref_frame_flag;

        if ((vp9_state->ref_frame_flag & VP9_LAST_REF) &&
            vp9_state->last_ref_obj) {
            obj_surface = vp9_state->last_ref_obj;
            vp9_priv_surface = (struct gen9_surface_vp9 *)(obj_surface->private_data);

            if (vp9_state->frame_width == vp9_priv_surface->frame_width &&
                vp9_state->frame_height == vp9_priv_surface->frame_height)
                vp9_state->dys_ref_frame_flag &= ~(VP9_LAST_REF);
        }
        if ((vp9_state->ref_frame_flag & VP9_GOLDEN_REF) &&
            vp9_state->golden_ref_obj) {
            obj_surface = vp9_state->golden_ref_obj;
            vp9_priv_surface = (struct gen9_surface_vp9 *)(obj_surface->private_data);

            if (vp9_state->frame_width == vp9_priv_surface->frame_width &&
                vp9_state->frame_height == vp9_priv_surface->frame_height)
                vp9_state->dys_ref_frame_flag &= ~(VP9_GOLDEN_REF);
        }
        if ((vp9_state->ref_frame_flag & VP9_ALT_REF) &&
            vp9_state->alt_ref_obj) {
            obj_surface = vp9_state->alt_ref_obj;
            vp9_priv_surface = (struct gen9_surface_vp9 *)(obj_surface->private_data);

            if (vp9_state->frame_width == vp9_priv_surface->frame_width &&
                vp9_state->frame_height == vp9_priv_surface->frame_height)
                vp9_state->dys_ref_frame_flag &= ~(VP9_ALT_REF);
        }
        if (vp9_state->dys_ref_frame_flag)
            vp9_state->dys_in_use = 1;
    }

    if (vp9_state->hme_supported) {
        vp9_state->hme_enabled = 1;
    } else {
        vp9_state->hme_enabled = 0;
    }

    if (vp9_state->b16xme_supported) {
        vp9_state->b16xme_enabled = 1;
    } else {
        vp9_state->b16xme_enabled = 0;
    }

    /* disable HME/16xME if the size is too small */
    if (vp9_state->frame_width_4x <= VP9_VME_REF_WIN ||
        vp9_state->frame_height_4x <= VP9_VME_REF_WIN) {
        vp9_state->hme_enabled = 0;
        vp9_state->b16xme_enabled = 0;
    }

    if (vp9_state->frame_width_16x < VP9_VME_REF_WIN ||
        vp9_state->frame_height_16x < VP9_VME_REF_WIN)
        vp9_state->b16xme_enabled = 0;

    if (pic_param->pic_flags.bits.frame_type == HCP_VP9_KEY_FRAME ||
        pic_param->pic_flags.bits.intra_only) {
        vp9_state->hme_enabled = 0;
        vp9_state->b16xme_enabled = 0;
    }

    vp9_state->mbenc_keyframe_dist_enabled = 0;
    if ((vp9_state->picture_coding_type == KEY_FRAME) &&
        vp9_state->brc_distortion_buffer_supported)
        vp9_state->mbenc_keyframe_dist_enabled = 1;

    return VA_STATUS_SUCCESS;
}

static VAStatus
gen9_vme_gpe_kernel_prepare_vp9(VADriverContextP ctx,
                                struct encode_state *encode_state,
                                struct intel_encoder_context *encoder_context)
{
    struct gen9_encoder_context_vp9 *vme_context = encoder_context->vme_context;
    struct vp9_surface_param surface_param;
    struct gen9_vp9_state *vp9_state;
    VAEncPictureParameterBufferVP9  *pic_param;
    struct object_surface *obj_surface;
    struct gen9_surface_vp9 *vp9_surface;
    int driver_header_flag = 0;
    VAStatus va_status;

    vp9_state = (struct gen9_vp9_state *) encoder_context->enc_priv_state;

    if (!vp9_state || !vp9_state->pic_param)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    pic_param = vp9_state->pic_param;

    /* this is to check whether the driver should generate the uncompressed header */
    driver_header_flag = 1;
    if (encode_state->packed_header_data_ext &&
        encode_state->packed_header_data_ext[0] &&
        pic_param->bit_offset_first_partition_size) {
        VAEncPackedHeaderParameterBuffer *param = NULL;

        param = (VAEncPackedHeaderParameterBuffer *)encode_state->packed_header_params_ext[0]->buffer;

        if (param->type == VAEncPackedHeaderRawData) {
            char *header_data;
            unsigned int length_in_bits;

            header_data = (char *)encode_state->packed_header_data_ext[0]->buffer;
            length_in_bits = param->bit_length;
            driver_header_flag = 0;

            vp9_state->frame_header.bit_offset_first_partition_size =
                pic_param->bit_offset_first_partition_size;
            vp9_state->header_length = ALIGN(length_in_bits, 8) >> 3;
            vp9_state->alias_insert_data = header_data;

            vp9_state->frame_header.bit_offset_ref_lf_delta = pic_param->bit_offset_ref_lf_delta;
            vp9_state->frame_header.bit_offset_mode_lf_delta = pic_param->bit_offset_mode_lf_delta;
            vp9_state->frame_header.bit_offset_lf_level = pic_param->bit_offset_lf_level;
            vp9_state->frame_header.bit_offset_qindex = pic_param->bit_offset_qindex;
            vp9_state->frame_header.bit_offset_segmentation = pic_param->bit_offset_segmentation;
            vp9_state->frame_header.bit_size_segmentation = pic_param->bit_size_segmentation;
        }
    }

    if (driver_header_flag) {
        memset(&vp9_state->frame_header, 0, sizeof(vp9_state->frame_header));
        intel_write_uncompressed_header(encode_state,
                                        VAProfileVP9Profile0,
                                        vme_context->frame_header_data,
                                        &vp9_state->header_length,
                                        &vp9_state->frame_header);
        vp9_state->alias_insert_data = vme_context->frame_header_data;
    }

    va_status = i965_check_alloc_surface_bo(ctx, encode_state->input_yuv_object,
                                            1, VA_FOURCC_NV12, SUBSAMPLE_YUV420);
    if (va_status != VA_STATUS_SUCCESS)
        return va_status;

    va_status = i965_check_alloc_surface_bo(ctx, encode_state->reconstructed_object,
                                            1, VA_FOURCC_NV12, SUBSAMPLE_YUV420);

    if (va_status != VA_STATUS_SUCCESS)
        return va_status;

    surface_param.frame_width = vp9_state->frame_width;
    surface_param.frame_height = vp9_state->frame_height;
    va_status = gen9_vp9_init_check_surfaces(ctx,
                                             encode_state->reconstructed_object,
                                             &surface_param);

    {
        vp9_surface = (struct gen9_surface_vp9*)encode_state->reconstructed_object;

        vp9_surface->qp_value = pic_param->luma_ac_qindex + pic_param->luma_dc_qindex_delta;
    }
    if (vp9_state->dys_in_use &&
        (pic_param->frame_width_src != pic_param->frame_width_dst ||
         pic_param->frame_height_src != pic_param->frame_height_dst)) {
        surface_param.frame_width = pic_param->frame_width_dst;
        surface_param.frame_height = pic_param->frame_height_dst;
        va_status = gen9_vp9_check_dys_surfaces(ctx,
                                                encode_state->reconstructed_object,
                                                &surface_param);

        if (va_status)
            return va_status;
    }

    if (vp9_state->dys_ref_frame_flag) {
        if ((vp9_state->dys_ref_frame_flag & VP9_LAST_REF) &&
            vp9_state->last_ref_obj) {
            obj_surface = vp9_state->last_ref_obj;
            surface_param.frame_width = vp9_state->frame_width;
            surface_param.frame_height = vp9_state->frame_height;
            va_status = gen9_vp9_check_dys_surfaces(ctx,
                                                    obj_surface,
                                                    &surface_param);

            if (va_status)
                return va_status;
        }
        if ((vp9_state->dys_ref_frame_flag & VP9_GOLDEN_REF) &&
            vp9_state->golden_ref_obj) {
            obj_surface = vp9_state->golden_ref_obj;
            surface_param.frame_width = vp9_state->frame_width;
            surface_param.frame_height = vp9_state->frame_height;
            va_status = gen9_vp9_check_dys_surfaces(ctx,
                                                    obj_surface,
                                                    &surface_param);

            if (va_status)
                return va_status;
        }
        if ((vp9_state->dys_ref_frame_flag & VP9_ALT_REF) &&
            vp9_state->alt_ref_obj) {
            obj_surface = vp9_state->alt_ref_obj;
            surface_param.frame_width = vp9_state->frame_width;
            surface_param.frame_height = vp9_state->frame_height;
            va_status = gen9_vp9_check_dys_surfaces(ctx,
                                                    obj_surface,
                                                    &surface_param);

            if (va_status)
                return va_status;
        }
    }

    if (va_status != VA_STATUS_SUCCESS)
        return va_status;
    /* check the corresponding ref_frame_flag && dys_ref_frame_flag */

    return VA_STATUS_SUCCESS;
}

static VAStatus
gen9_vme_gpe_kernel_init_vp9(VADriverContextP ctx,
                             struct encode_state *encode_state,
                             struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct gen9_encoder_context_vp9 *vme_context = encoder_context->vme_context;
    struct vp9_mbenc_context *mbenc_context = &vme_context->mbenc_context;
    struct vp9_dys_context *dys_context = &vme_context->dys_context;
    struct gpe_dynamic_state_parameter ds_param;
    int i;

    /*
     * BRC will update MBEnc curbe data buffer, so initialize GPE context for
     * MBEnc first
     */
    for (i = 0; i < NUM_VP9_MBENC; i++) {
        gen8_gpe_context_init(ctx, &mbenc_context->gpe_contexts[i]);
    }

    /*
     * VP9_MBENC_XXX uses the same dynamic state buffer as they share the same
     * curbe_buffer.
     */
    ds_param.bo_size = ALIGN(sizeof(vp9_mbenc_curbe_data), 64) + 128 +
                       ALIGN(sizeof(struct gen8_interface_descriptor_data), 64) * NUM_VP9_MBENC;
    mbenc_context->mbenc_bo_dys = dri_bo_alloc(i965->intel.bufmgr,
                                               "mbenc_dys",
                                               ds_param.bo_size,
                                               0x1000);
    mbenc_context->mbenc_bo_size = ds_param.bo_size;

    ds_param.bo = mbenc_context->mbenc_bo_dys;
    ds_param.curbe_offset = 0;
    ds_param.sampler_offset = ALIGN(sizeof(vp9_mbenc_curbe_data), 64);
    for (i = 0; i < NUM_VP9_MBENC; i++) {
        ds_param.idrt_offset = ds_param.sampler_offset + 128 +
                               ALIGN(sizeof(struct gen8_interface_descriptor_data), 64) * i;

        gen8_gpe_context_set_dynamic_buffer(ctx,
                                            &mbenc_context->gpe_contexts[i],
                                            &ds_param);
    }

    gen8_gpe_context_init(ctx, &dys_context->gpe_context);
    gen9_vp9_dys_set_sampler_state(&dys_context->gpe_context);

    return VA_STATUS_SUCCESS;
}

static VAStatus
gen9_vme_gpe_kernel_final_vp9(VADriverContextP ctx,
                              struct encode_state *encode_state,
                              struct intel_encoder_context *encoder_context)
{
    struct gen9_encoder_context_vp9 *vme_context = encoder_context->vme_context;
    struct vp9_mbenc_context *mbenc_context = &vme_context->mbenc_context;

    dri_bo_unreference(mbenc_context->mbenc_bo_dys);
    mbenc_context->mbenc_bo_dys = NULL;

    return VA_STATUS_SUCCESS;
}

static VAStatus
gen9_vme_gpe_kernel_run_vp9(VADriverContextP ctx,
                            struct encode_state *encode_state,
                            struct intel_encoder_context *encoder_context)
{
    struct gen9_encoder_context_vp9 *vme_context = encoder_context->vme_context;
    struct gen9_vp9_state *vp9_state;
    int i;

    vp9_state = (struct gen9_vp9_state *) encoder_context->enc_priv_state;

    if (!vp9_state || !vp9_state->pic_param)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    if (vp9_state->dys_in_use) {
        gen9_vp9_run_dys_refframes(ctx, encode_state, encoder_context);
    }

    if (vp9_state->brc_enabled && (vp9_state->brc_reset || !vp9_state->brc_inited)) {
        gen9_vp9_brc_init_reset_kernel(ctx, encode_state, encoder_context);
    }

    if (vp9_state->picture_coding_type == KEY_FRAME) {
        for (i = 0; i < 2; i++)
            i965_zero_gpe_resource(&vme_context->res_mode_decision[i]);
    }

    if (vp9_state->hme_supported) {
        gen9_vp9_scaling_kernel(ctx, encode_state,
                                encoder_context,
                                0);
        if (vp9_state->b16xme_supported) {
            gen9_vp9_scaling_kernel(ctx, encode_state,
                                    encoder_context,
                                    1);
        }
    }

    if (vp9_state->picture_coding_type && vp9_state->hme_enabled) {
        if (vp9_state->b16xme_enabled)
            gen9_vp9_me_kernel(ctx, encode_state,
                               encoder_context,
                               1);

        gen9_vp9_me_kernel(ctx, encode_state,
                           encoder_context,
                           0);
    }

    if (vp9_state->brc_enabled) {
        if (vp9_state->mbenc_keyframe_dist_enabled)
            gen9_vp9_brc_intra_dist_kernel(ctx,
                                           encode_state,
                                           encoder_context);

        gen9_vp9_brc_update_kernel(ctx, encode_state,
                                   encoder_context);
    }

    if (vp9_state->picture_coding_type == KEY_FRAME) {
        gen9_vp9_mbenc_kernel(ctx, encode_state,
                              encoder_context,
                              VP9_MEDIA_STATE_MBENC_I_32x32);
        gen9_vp9_mbenc_kernel(ctx, encode_state,
                              encoder_context,
                              VP9_MEDIA_STATE_MBENC_I_16x16);
    } else {
        gen9_vp9_mbenc_kernel(ctx, encode_state,
                              encoder_context,
                              VP9_MEDIA_STATE_MBENC_P);
    }

    gen9_vp9_mbenc_kernel(ctx, encode_state,
                          encoder_context,
                          VP9_MEDIA_STATE_MBENC_TX);

    vp9_state->curr_mode_decision_index ^= 1;
    if (vp9_state->brc_enabled) {
        vp9_state->brc_inited = 1;
        vp9_state->brc_reset = 0;
    }

    return VA_STATUS_SUCCESS;
}

static VAStatus
gen9_vme_pipeline_vp9(VADriverContextP ctx,
                      VAProfile profile,
                      struct encode_state *encode_state,
                      struct intel_encoder_context *encoder_context)
{
    VAStatus va_status;
    struct gen9_vp9_state *vp9_state;

    vp9_state = (struct gen9_vp9_state *) encoder_context->enc_priv_state;

    if (!vp9_state)
        return VA_STATUS_ERROR_INVALID_CONTEXT;

    va_status = gen9_encode_vp9_check_parameter(ctx, encode_state, encoder_context);
    if (va_status != VA_STATUS_SUCCESS)
        return va_status;

    va_status = gen9_vp9_allocate_resources(ctx, encode_state,
                                            encoder_context,
                                            !vp9_state->brc_allocated);

    if (va_status != VA_STATUS_SUCCESS)
        return va_status;
    vp9_state->brc_allocated = 1;

    va_status = gen9_vme_gpe_kernel_prepare_vp9(ctx, encode_state, encoder_context);

    if (va_status != VA_STATUS_SUCCESS)
        return va_status;

    va_status = gen9_vme_gpe_kernel_init_vp9(ctx, encode_state, encoder_context);
    if (va_status != VA_STATUS_SUCCESS)
        return va_status;

    va_status = gen9_vme_gpe_kernel_run_vp9(ctx, encode_state, encoder_context);
    if (va_status != VA_STATUS_SUCCESS)
        return va_status;

    gen9_vme_gpe_kernel_final_vp9(ctx, encode_state, encoder_context);

    return VA_STATUS_SUCCESS;
}

static void
gen9_vme_brc_context_destroy_vp9(struct vp9_brc_context *brc_context)
{
    int i;

    for (i = 0; i < NUM_VP9_BRC; i++)
        gen8_gpe_context_destroy(&brc_context->gpe_contexts[i]);
}

static void
gen9_vme_scaling_context_destroy_vp9(struct vp9_scaling_context *scaling_context)
{
    int i;

    for (i = 0; i < NUM_VP9_SCALING; i++)
        gen8_gpe_context_destroy(&scaling_context->gpe_contexts[i]);
}

static void
gen9_vme_me_context_destroy_vp9(struct vp9_me_context *me_context)
{
    gen8_gpe_context_destroy(&me_context->gpe_context);
}

static void
gen9_vme_mbenc_context_destroy_vp9(struct vp9_mbenc_context *mbenc_context)
{
    int i;

    for (i = 0; i < NUM_VP9_MBENC; i++)
        gen8_gpe_context_destroy(&mbenc_context->gpe_contexts[i]);
    dri_bo_unreference(mbenc_context->mbenc_bo_dys);
    mbenc_context->mbenc_bo_size = 0;
}

static void
gen9_vme_dys_context_destroy_vp9(struct vp9_dys_context *dys_context)
{
    gen8_gpe_context_destroy(&dys_context->gpe_context);
}

static void
gen9_vme_kernel_context_destroy_vp9(struct gen9_encoder_context_vp9 *vme_context)
{
    gen9_vp9_free_resources(vme_context);
    gen9_vme_scaling_context_destroy_vp9(&vme_context->scaling_context);
    gen9_vme_me_context_destroy_vp9(&vme_context->me_context);
    gen9_vme_mbenc_context_destroy_vp9(&vme_context->mbenc_context);
    gen9_vme_brc_context_destroy_vp9(&vme_context->brc_context);
    gen9_vme_dys_context_destroy_vp9(&vme_context->dys_context);

    return;
}

static void
gen9_vme_context_destroy_vp9(void *context)
{
    struct gen9_encoder_context_vp9 *vme_context = context;

    if (!vme_context)
        return;

    gen9_vme_kernel_context_destroy_vp9(vme_context);

    free(vme_context);

    return;
}

static void
gen9_vme_scaling_context_init_vp9(VADriverContextP ctx,
                                  struct gen9_encoder_context_vp9 *vme_context,
                                  struct vp9_scaling_context *scaling_context)
{
    struct i965_gpe_context *gpe_context = NULL;
    struct vp9_encoder_kernel_parameter kernel_param;
    struct vp9_encoder_scoreboard_parameter scoreboard_param;
    struct i965_kernel scale_kernel;

    kernel_param.curbe_size = sizeof(vp9_scaling4x_curbe_data_cm);
    kernel_param.inline_data_size = sizeof(vp9_scaling4x_inline_data_cm);
    kernel_param.sampler_size = 0;

    memset(&scoreboard_param, 0, sizeof(scoreboard_param));
    scoreboard_param.mask = 0xFF;
    scoreboard_param.enable = vme_context->use_hw_scoreboard;
    scoreboard_param.type = vme_context->use_hw_non_stalling_scoreboard;
    scoreboard_param.walkpat_flag = 0;

    gpe_context = &scaling_context->gpe_contexts[0];
    gen9_init_gpe_context_vp9(ctx, gpe_context, &kernel_param);
    gen9_init_vfe_scoreboard_vp9(gpe_context, &scoreboard_param);

    scaling_context->scaling_4x_bti.scaling_frame_src_y = VP9_BTI_SCALING_FRAME_SRC_Y;
    scaling_context->scaling_4x_bti.scaling_frame_dst_y = VP9_BTI_SCALING_FRAME_DST_Y;
    scaling_context->scaling_4x_bti.scaling_frame_mbv_proc_stat_dst =
        VP9_BTI_SCALING_FRAME_MBVPROCSTATS_DST_CM;

    memset(&scale_kernel, 0, sizeof(scale_kernel));

    intel_vp9_get_kernel_header_and_size((void *)media_vp9_kernels,
                                         sizeof(media_vp9_kernels),
                                         INTEL_VP9_ENC_SCALING4X,
                                         0,
                                         &scale_kernel);

    gen8_gpe_load_kernels(ctx,
                          gpe_context,
                          &scale_kernel,
                          1);

    kernel_param.curbe_size = sizeof(vp9_scaling2x_curbe_data_cm);
    kernel_param.inline_data_size = 0;
    kernel_param.sampler_size = 0;

    gpe_context = &scaling_context->gpe_contexts[1];
    gen9_init_gpe_context_vp9(ctx, gpe_context, &kernel_param);
    gen9_init_vfe_scoreboard_vp9(gpe_context, &scoreboard_param);

    memset(&scale_kernel, 0, sizeof(scale_kernel));

    intel_vp9_get_kernel_header_and_size((void *)media_vp9_kernels,
                                         sizeof(media_vp9_kernels),
                                         INTEL_VP9_ENC_SCALING2X,
                                         0,
                                         &scale_kernel);

    gen8_gpe_load_kernels(ctx,
                          gpe_context,
                          &scale_kernel,
                          1);

    scaling_context->scaling_2x_bti.scaling_frame_src_y = VP9_BTI_SCALING_FRAME_SRC_Y;
    scaling_context->scaling_2x_bti.scaling_frame_dst_y = VP9_BTI_SCALING_FRAME_DST_Y;
    return;
}

static void
gen9_vme_me_context_init_vp9(VADriverContextP ctx,
                             struct gen9_encoder_context_vp9 *vme_context,
                             struct vp9_me_context *me_context)
{
    struct i965_gpe_context *gpe_context = NULL;
    struct vp9_encoder_kernel_parameter kernel_param;
    struct vp9_encoder_scoreboard_parameter scoreboard_param;
    struct i965_kernel scale_kernel;

    kernel_param.curbe_size = sizeof(vp9_me_curbe_data);
    kernel_param.inline_data_size = 0;
    kernel_param.sampler_size = 0;

    memset(&scoreboard_param, 0, sizeof(scoreboard_param));
    scoreboard_param.mask = 0xFF;
    scoreboard_param.enable = vme_context->use_hw_scoreboard;
    scoreboard_param.type = vme_context->use_hw_non_stalling_scoreboard;
    scoreboard_param.walkpat_flag = 0;

    gpe_context = &me_context->gpe_context;
    gen9_init_gpe_context_vp9(ctx, gpe_context, &kernel_param);
    gen9_init_vfe_scoreboard_vp9(gpe_context, &scoreboard_param);

    memset(&scale_kernel, 0, sizeof(scale_kernel));

    intel_vp9_get_kernel_header_and_size((void *)media_vp9_kernels,
                                         sizeof(media_vp9_kernels),
                                         INTEL_VP9_ENC_ME,
                                         0,
                                         &scale_kernel);

    gen8_gpe_load_kernels(ctx,
                          gpe_context,
                          &scale_kernel,
                          1);

    return;
}

static void
gen9_vme_mbenc_context_init_vp9(VADriverContextP ctx,
                                struct gen9_encoder_context_vp9 *vme_context,
                                struct vp9_mbenc_context *mbenc_context)
{
    struct i965_gpe_context *gpe_context = NULL;
    struct vp9_encoder_kernel_parameter kernel_param;
    struct vp9_encoder_scoreboard_parameter scoreboard_param;
    int i;
    struct i965_kernel scale_kernel;

    kernel_param.curbe_size = sizeof(vp9_mbenc_curbe_data);
    kernel_param.inline_data_size = 0;
    kernel_param.sampler_size = 0;

    memset(&scoreboard_param, 0, sizeof(scoreboard_param));
    scoreboard_param.mask = 0xFF;
    scoreboard_param.enable = vme_context->use_hw_scoreboard;
    scoreboard_param.type = vme_context->use_hw_non_stalling_scoreboard;

    for (i = 0; i < NUM_VP9_MBENC; i++) {
        gpe_context = &mbenc_context->gpe_contexts[i];

        if ((i == VP9_MBENC_IDX_KEY_16x16) ||
            (i == VP9_MBENC_IDX_INTER)) {
            scoreboard_param.walkpat_flag = 1;
        } else
            scoreboard_param.walkpat_flag = 0;

        gen9_init_gpe_context_vp9(ctx, gpe_context, &kernel_param);
        gen9_init_vfe_scoreboard_vp9(gpe_context, &scoreboard_param);

        memset(&scale_kernel, 0, sizeof(scale_kernel));

        intel_vp9_get_kernel_header_and_size((void *)media_vp9_kernels,
                                             sizeof(media_vp9_kernels),
                                             INTEL_VP9_ENC_MBENC,
                                             i,
                                             &scale_kernel);

        gen8_gpe_load_kernels(ctx,
                              gpe_context,
                              &scale_kernel,
                              1);
    }
}

static void
gen9_vme_brc_context_init_vp9(VADriverContextP ctx,
                              struct gen9_encoder_context_vp9 *vme_context,
                              struct vp9_brc_context *brc_context)
{
    struct i965_gpe_context *gpe_context = NULL;
    struct vp9_encoder_kernel_parameter kernel_param;
    struct vp9_encoder_scoreboard_parameter scoreboard_param;
    int i;
    struct i965_kernel scale_kernel;

    kernel_param.curbe_size = sizeof(vp9_brc_curbe_data);
    kernel_param.inline_data_size = 0;
    kernel_param.sampler_size = 0;

    memset(&scoreboard_param, 0, sizeof(scoreboard_param));
    scoreboard_param.mask = 0xFF;
    scoreboard_param.enable = vme_context->use_hw_scoreboard;
    scoreboard_param.type = vme_context->use_hw_non_stalling_scoreboard;

    for (i = 0; i < NUM_VP9_BRC; i++) {
        gpe_context = &brc_context->gpe_contexts[i];
        gen9_init_gpe_context_vp9(ctx, gpe_context, &kernel_param);
        gen9_init_vfe_scoreboard_vp9(gpe_context, &scoreboard_param);

        memset(&scale_kernel, 0, sizeof(scale_kernel));

        intel_vp9_get_kernel_header_and_size((void *)media_vp9_kernels,
                                             sizeof(media_vp9_kernels),
                                             INTEL_VP9_ENC_BRC,
                                             i,
                                             &scale_kernel);

        gen8_gpe_load_kernels(ctx,
                              gpe_context,
                              &scale_kernel,
                              1);
    }
}

static void
gen9_vme_dys_context_init_vp9(VADriverContextP ctx,
                              struct gen9_encoder_context_vp9 *vme_context,
                              struct vp9_dys_context *dys_context)
{
    struct i965_gpe_context *gpe_context = NULL;
    struct vp9_encoder_kernel_parameter kernel_param;
    struct vp9_encoder_scoreboard_parameter scoreboard_param;
    struct i965_kernel scale_kernel;

    kernel_param.curbe_size = sizeof(vp9_dys_curbe_data);
    kernel_param.inline_data_size = 0;
    kernel_param.sampler_size = sizeof(struct gen9_sampler_8x8_avs);

    memset(&scoreboard_param, 0, sizeof(scoreboard_param));
    scoreboard_param.mask = 0xFF;
    scoreboard_param.enable = vme_context->use_hw_scoreboard;
    scoreboard_param.type = vme_context->use_hw_non_stalling_scoreboard;
    scoreboard_param.walkpat_flag = 0;

    gpe_context = &dys_context->gpe_context;
    gen9_init_gpe_context_vp9(ctx, gpe_context, &kernel_param);
    gen9_init_vfe_scoreboard_vp9(gpe_context, &scoreboard_param);

    memset(&scale_kernel, 0, sizeof(scale_kernel));

    intel_vp9_get_kernel_header_and_size((void *)media_vp9_kernels,
                                         sizeof(media_vp9_kernels),
                                         INTEL_VP9_ENC_DYS,
                                         0,
                                         &scale_kernel);

    gen8_gpe_load_kernels(ctx,
                          gpe_context,
                          &scale_kernel,
                          1);

    return;
}

static Bool
gen9_vme_kernels_context_init_vp9(VADriverContextP ctx,
                                  struct intel_encoder_context *encoder_context,
                                  struct gen9_encoder_context_vp9 *vme_context)
{
    gen9_vme_scaling_context_init_vp9(ctx, vme_context, &vme_context->scaling_context);
    gen9_vme_me_context_init_vp9(ctx, vme_context, &vme_context->me_context);
    gen9_vme_mbenc_context_init_vp9(ctx, vme_context, &vme_context->mbenc_context);
    gen9_vme_dys_context_init_vp9(ctx, vme_context, &vme_context->dys_context);
    gen9_vme_brc_context_init_vp9(ctx, vme_context, &vme_context->brc_context);

    vme_context->pfn_set_curbe_brc = gen9_vp9_set_curbe_brc;
    vme_context->pfn_set_curbe_me = gen9_vp9_set_curbe_me;
    vme_context->pfn_send_me_surface = gen9_vp9_send_me_surface;
    vme_context->pfn_send_scaling_surface = gen9_vp9_send_scaling_surface;

    vme_context->pfn_set_curbe_scaling = gen9_vp9_set_curbe_scaling_cm;

    vme_context->pfn_send_dys_surface = gen9_vp9_send_dys_surface;
    vme_context->pfn_set_curbe_dys = gen9_vp9_set_curbe_dys;
    vme_context->pfn_set_curbe_mbenc = gen9_vp9_set_curbe_mbenc;
    vme_context->pfn_send_mbenc_surface = gen9_vp9_send_mbenc_surface;
    return true;
}

static
void gen9_vp9_write_compressed_element(char *buffer,
                                       int index,
                                       int prob,
                                       bool value)
{
    struct vp9_compressed_element *base_element, *vp9_element;
    base_element = (struct vp9_compressed_element *)buffer;

    vp9_element = base_element + (index >> 1);
    if (index % 2) {
        vp9_element->b_valid = 1;
        vp9_element->b_probdiff_select = 1;
        vp9_element->b_prob_select = (prob == 252) ? 1 : 0;
        vp9_element->b_bin = value;
    } else {
        vp9_element->a_valid = 1;
        vp9_element->a_probdiff_select = 1;
        vp9_element->a_prob_select = (prob == 252) ? 1 : 0;
        vp9_element->a_bin = value;
    }
}

static void
intel_vp9enc_refresh_frame_internal_buffers(VADriverContextP ctx,
                                            struct intel_encoder_context *encoder_context)
{
    struct gen9_encoder_context_vp9 *pak_context = encoder_context->mfc_context;
    VAEncPictureParameterBufferVP9 *pic_param;
    struct gen9_vp9_state *vp9_state;
    char *buffer;
    int i;

    vp9_state = (struct gen9_vp9_state *)(encoder_context->enc_priv_state);

    if (!pak_context || !vp9_state || !vp9_state->pic_param)
        return;

    pic_param = vp9_state->pic_param;
    if ((pic_param->pic_flags.bits.frame_type == HCP_VP9_KEY_FRAME) ||
        (pic_param->pic_flags.bits.intra_only) ||
        pic_param->pic_flags.bits.error_resilient_mode) {
        /* reset current frame_context */
        intel_init_default_vp9_probs(&vp9_state->vp9_current_fc);
        if ((pic_param->pic_flags.bits.frame_type == HCP_VP9_KEY_FRAME) ||
            pic_param->pic_flags.bits.error_resilient_mode ||
            (pic_param->pic_flags.bits.reset_frame_context == 3)) {
            for (i = 0; i < 4; i++)
                memcpy(&vp9_state->vp9_frame_ctx[i],
                       &vp9_state->vp9_current_fc,
                       sizeof(FRAME_CONTEXT));
        } else if (pic_param->pic_flags.bits.reset_frame_context == 2) {
            i = pic_param->pic_flags.bits.frame_context_idx;
            memcpy(&vp9_state->vp9_frame_ctx[i],
                   &vp9_state->vp9_current_fc, sizeof(FRAME_CONTEXT));
        }
        /* reset the frame_ctx_idx = 0 */
        vp9_state->frame_ctx_idx = 0;
    } else {
        vp9_state->frame_ctx_idx = pic_param->pic_flags.bits.frame_context_idx;
    }

    i965_zero_gpe_resource(&pak_context->res_compressed_input_buffer);
    buffer = i965_map_gpe_resource(&pak_context->res_compressed_input_buffer);

    if (!buffer)
        return;

    /* write tx_size */
    if ((pic_param->luma_ac_qindex == 0) &&
        (pic_param->luma_dc_qindex_delta == 0) &&
        (pic_param->chroma_ac_qindex_delta == 0) &&
        (pic_param->chroma_dc_qindex_delta == 0)) {
        /* lossless flag */
        /* nothing is needed */
        gen9_vp9_write_compressed_element(buffer,
                                          0, 128, 0);
        gen9_vp9_write_compressed_element(buffer,
                                          1, 128, 0);
        gen9_vp9_write_compressed_element(buffer,
                                          2, 128, 0);
    } else {
        if (vp9_state->tx_mode == TX_MODE_SELECT) {
            gen9_vp9_write_compressed_element(buffer,
                                              0, 128, 1);
            gen9_vp9_write_compressed_element(buffer,
                                              1, 128, 1);
            gen9_vp9_write_compressed_element(buffer,
                                              2, 128, 1);
        } else if (vp9_state->tx_mode == ALLOW_32X32) {
            gen9_vp9_write_compressed_element(buffer,
                                              0, 128, 1);
            gen9_vp9_write_compressed_element(buffer,
                                              1, 128, 1);
            gen9_vp9_write_compressed_element(buffer,
                                              2, 128, 0);
        } else {
            unsigned int tx_mode;

            tx_mode = vp9_state->tx_mode;
            gen9_vp9_write_compressed_element(buffer,
                                              0, 128, ((tx_mode) & 2));
            gen9_vp9_write_compressed_element(buffer,
                                              1, 128, ((tx_mode) & 1));
            gen9_vp9_write_compressed_element(buffer,
                                              2, 128, 0);
        }

        if (vp9_state->tx_mode == TX_MODE_SELECT) {

            gen9_vp9_write_compressed_element(buffer,
                                              3, 128, 0);

            gen9_vp9_write_compressed_element(buffer,
                                              7, 128, 0);

            gen9_vp9_write_compressed_element(buffer,
                                              15, 128, 0);
        }
    }
    /*Setup all the input&output object*/

    {
        /* update the coeff_update flag */
        gen9_vp9_write_compressed_element(buffer,
                                          27, 128, 0);
        gen9_vp9_write_compressed_element(buffer,
                                          820, 128, 0);
        gen9_vp9_write_compressed_element(buffer,
                                          1613, 128, 0);
        gen9_vp9_write_compressed_element(buffer,
                                          2406, 128, 0);
    }


    if (pic_param->pic_flags.bits.frame_type && !pic_param->pic_flags.bits.intra_only) {
        bool allow_comp = !(
                              (pic_param->ref_flags.bits.ref_last_sign_bias && pic_param->ref_flags.bits.ref_gf_sign_bias && pic_param->ref_flags.bits.ref_arf_sign_bias) ||
                              (!pic_param->ref_flags.bits.ref_last_sign_bias && !pic_param->ref_flags.bits.ref_gf_sign_bias && !pic_param->ref_flags.bits.ref_arf_sign_bias)
                          );

        if (allow_comp) {
            if (pic_param->pic_flags.bits.comp_prediction_mode == REFERENCE_MODE_SELECT) {
                gen9_vp9_write_compressed_element(buffer,
                                                  3271, 128, 1);
                gen9_vp9_write_compressed_element(buffer,
                                                  3272, 128, 1);
            } else if (pic_param->pic_flags.bits.comp_prediction_mode == COMPOUND_REFERENCE) {
                gen9_vp9_write_compressed_element(buffer,
                                                  3271, 128, 1);
                gen9_vp9_write_compressed_element(buffer,
                                                  3272, 128, 0);
            } else {

                gen9_vp9_write_compressed_element(buffer,
                                                  3271, 128, 0);
                gen9_vp9_write_compressed_element(buffer,
                                                  3272, 128, 0);
            }
        }
    }

    i965_unmap_gpe_resource(&pak_context->res_compressed_input_buffer);
}


static void
gen9_pak_vp9_pipe_mode_select(VADriverContextP ctx,
                              struct encode_state *encode_state,
                              struct intel_encoder_context *encoder_context,
                              struct gen9_hcpe_pipe_mode_select_param *pipe_mode_param)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;

    BEGIN_BCS_BATCH(batch, 6);

    OUT_BCS_BATCH(batch, HCP_PIPE_MODE_SELECT | (6 - 2));
    OUT_BCS_BATCH(batch,
                  (pipe_mode_param->stream_out << 12) |
                  (pipe_mode_param->codec_mode << 5) |
                  (0 << 3) | /* disable Pic Status / Error Report */
                  (pipe_mode_param->stream_out << 2) |
                  HCP_CODEC_SELECT_ENCODE);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, (1 << 6));
    OUT_BCS_BATCH(batch, 0);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_vp9_add_surface_state(VADriverContextP ctx,
                           struct encode_state *encode_state,
                           struct intel_encoder_context *encoder_context,
                           hcp_surface_state *hcp_state)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    if (!hcp_state)
        return;

    BEGIN_BCS_BATCH(batch, 3);
    OUT_BCS_BATCH(batch, HCP_SURFACE_STATE | (3 - 2));
    OUT_BCS_BATCH(batch,
                  (hcp_state->dw1.surface_id << 28) |
                  (hcp_state->dw1.surface_pitch - 1)
                 );
    OUT_BCS_BATCH(batch,
                  (hcp_state->dw2.surface_format << 28) |
                  (hcp_state->dw2.y_cb_offset)
                 );
    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_pak_vp9_pipe_buf_addr_state(VADriverContextP ctx,
                                 struct encode_state *encode_state,
                                 struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct gen9_encoder_context_vp9 *pak_context = encoder_context->mfc_context;
    struct gen9_vp9_state *vp9_state;
    unsigned int i;
    struct object_surface *obj_surface;

    vp9_state = (struct gen9_vp9_state *)(encoder_context->enc_priv_state);

    if (!vp9_state || !vp9_state->pic_param)
        return;


    BEGIN_BCS_BATCH(batch, 104);

    OUT_BCS_BATCH(batch, HCP_PIPE_BUF_ADDR_STATE | (104 - 2));

    obj_surface = encode_state->reconstructed_object;

    /* reconstructed obj_surface is already checked. So this is skipped */
    /* DW 1..3 decoded surface */
    OUT_RELOC64(batch,
                obj_surface->bo,
                I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
                0);
    OUT_BCS_BATCH(batch, i965->intel.mocs_state);

    /* DW 4..6 deblocking line */
    OUT_RELOC64(batch,
                pak_context->res_deblocking_filter_line_buffer.bo,
                I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
                0);
    OUT_BCS_BATCH(batch, i965->intel.mocs_state);

    /* DW 7..9 deblocking tile line */
    OUT_RELOC64(batch,
                pak_context->res_deblocking_filter_tile_line_buffer.bo,
                I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
                0);
    OUT_BCS_BATCH(batch, i965->intel.mocs_state);

    /* DW 10..12 deblocking tile col */
    OUT_RELOC64(batch,
                pak_context->res_deblocking_filter_tile_col_buffer.bo,
                I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
                0);
    OUT_BCS_BATCH(batch, i965->intel.mocs_state);

    /* DW 13..15 metadata line */
    OUT_RELOC64(batch,
                pak_context->res_metadata_line_buffer.bo,
                I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
                0);
    OUT_BCS_BATCH(batch, i965->intel.mocs_state);

    /* DW 16..18 metadata tile line */
    OUT_RELOC64(batch,
                pak_context->res_metadata_tile_line_buffer.bo,
                I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
                0);
    OUT_BCS_BATCH(batch, i965->intel.mocs_state);

    /* DW 19..21 metadata tile col */
    OUT_RELOC64(batch,
                pak_context->res_metadata_tile_col_buffer.bo,
                I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
                0);
    OUT_BCS_BATCH(batch, i965->intel.mocs_state);

    /* DW 22..30 SAO is not used for VP9 */
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);

    /* DW 31..33 Current Motion vector temporal buffer */
    OUT_RELOC64(batch,
                pak_context->res_mv_temporal_buffer[vp9_state->curr_mv_temporal_index].bo,
                I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
                0);
    OUT_BCS_BATCH(batch, i965->intel.mocs_state);

    /* DW 34..36 Not used */
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);

    /* Only the first three reference_frame is used for VP9 */
    /* DW 37..52 for reference_frame */
    i = 0;
    if (vp9_state->picture_coding_type) {
        for (i = 0; i < 3; i++) {

            if (pak_context->reference_surfaces[i].bo) {
                OUT_RELOC64(batch,
                            pak_context->reference_surfaces[i].bo,
                            I915_GEM_DOMAIN_INSTRUCTION, 0,
                            0);
            } else {
                OUT_BCS_BATCH(batch, 0);
                OUT_BCS_BATCH(batch, 0);
            }
        }
    }

    for (; i < 8; i++) {
        OUT_BCS_BATCH(batch, 0);
        OUT_BCS_BATCH(batch, 0);
    }

    OUT_BCS_BATCH(batch, i965->intel.mocs_state);

    /* DW 54..56 for source input */
    OUT_RELOC64(batch,
                pak_context->uncompressed_picture_source.bo,
                I915_GEM_DOMAIN_INSTRUCTION, 0,
                0);
    OUT_BCS_BATCH(batch, i965->intel.mocs_state);

    /* DW 57..59 StreamOut is not used */
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);

    /* DW 60..62. Not used for encoder */
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);

    /* DW 63..65. ILDB Not used for encoder */
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);

    /* DW 66..81 For the collocated motion vector temporal buffer */
    if (vp9_state->picture_coding_type) {
        int prev_index = vp9_state->curr_mv_temporal_index ^ 0x01;
        OUT_RELOC64(batch,
                    pak_context->res_mv_temporal_buffer[prev_index].bo,
                    I915_GEM_DOMAIN_INSTRUCTION, 0,
                    0);
    } else {
        OUT_BCS_BATCH(batch, 0);
        OUT_BCS_BATCH(batch, 0);
    }

    for (i = 1; i < 8; i++) {
        OUT_BCS_BATCH(batch, 0);
        OUT_BCS_BATCH(batch, 0);
    }
    OUT_BCS_BATCH(batch, i965->intel.mocs_state);

    /* DW 83..85 VP9 prob buffer */
    OUT_RELOC64(batch,
                pak_context->res_prob_buffer.bo,
                I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
                0);

    OUT_BCS_BATCH(batch, i965->intel.mocs_state);

    /* DW 86..88 Segment id buffer */
    if (pak_context->res_segmentid_buffer.bo) {
        OUT_RELOC64(batch,
                    pak_context->res_segmentid_buffer.bo,
                    I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
                    0);
    } else {
        OUT_BCS_BATCH(batch, 0);
        OUT_BCS_BATCH(batch, 0);
    }
    OUT_BCS_BATCH(batch, i965->intel.mocs_state);

    /* DW 89..91 HVD line rowstore buffer */
    OUT_RELOC64(batch,
                pak_context->res_hvd_line_buffer.bo,
                I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
                0);
    OUT_BCS_BATCH(batch, i965->intel.mocs_state);

    /* DW 92..94 HVD tile line rowstore buffer */
    OUT_RELOC64(batch,
                pak_context->res_hvd_tile_line_buffer.bo,
                I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
                0);
    OUT_BCS_BATCH(batch, i965->intel.mocs_state);

    /* DW 95..97 SAO streamout. Not used for VP9 */
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);

    /* reserved for KBL. 98..100 */
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);

    /* 101..103 */
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_pak_vp9_ind_obj_base_addr_state(VADriverContextP ctx,
                                     struct encode_state *encode_state,
                                     struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct gen9_encoder_context_vp9 *pak_context = encoder_context->mfc_context;
    struct gen9_vp9_state *vp9_state;

    vp9_state = (struct gen9_vp9_state *)(encoder_context->enc_priv_state);

    /* to do */
    BEGIN_BCS_BATCH(batch, 29);

    OUT_BCS_BATCH(batch, HCP_IND_OBJ_BASE_ADDR_STATE | (29 - 2));

    /* indirect bitstream object base */
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    /* the upper bound of indirect bitstream object */
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);

    /* DW 6: Indirect CU object base address */
    OUT_RELOC64(batch,
                pak_context->res_mb_code_surface.bo,
                I915_GEM_DOMAIN_INSTRUCTION, 0,   /* No write domain */
                vp9_state->mb_data_offset);
    /* default attribute */
    OUT_BCS_BATCH(batch, i965->intel.mocs_state);

    /* DW 9..11, PAK-BSE */
    OUT_RELOC64(batch,
                pak_context->indirect_pak_bse_object.bo,
                I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
                pak_context->indirect_pak_bse_object.offset);
    OUT_BCS_BATCH(batch, i965->intel.mocs_state);

    /* DW 12..13 upper bound */
    OUT_RELOC64(batch,
                pak_context->indirect_pak_bse_object.bo,
                I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
                pak_context->indirect_pak_bse_object.end_offset);

    /* DW 14..16 compressed header buffer */
    OUT_RELOC64(batch,
                pak_context->res_compressed_input_buffer.bo,
                I915_GEM_DOMAIN_INSTRUCTION, 0,
                0);
    OUT_BCS_BATCH(batch, i965->intel.mocs_state);

    /* DW 17..19 prob counter streamout */
    OUT_RELOC64(batch,
                pak_context->res_prob_counter_buffer.bo,
                I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
                0);
    OUT_BCS_BATCH(batch, i965->intel.mocs_state);

    /* DW 20..22 prob delta streamin */
    OUT_RELOC64(batch,
                pak_context->res_prob_delta_buffer.bo,
                I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
                0);
    OUT_BCS_BATCH(batch, i965->intel.mocs_state);

    /* DW 23..25 Tile record streamout */
    OUT_RELOC64(batch,
                pak_context->res_tile_record_streamout_buffer.bo,
                I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
                0);
    OUT_BCS_BATCH(batch, i965->intel.mocs_state);

    /* DW 26..28 CU record streamout */
    OUT_RELOC64(batch,
                pak_context->res_cu_stat_streamout_buffer.bo,
                I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
                0);
    OUT_BCS_BATCH(batch, i965->intel.mocs_state);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_pak_vp9_segment_state(VADriverContextP ctx,
                           struct encode_state *encode_state,
                           struct intel_encoder_context *encoder_context,
                           VAEncSegParamVP9 *seg_param, uint8_t seg_id)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    uint32_t batch_value, tmp;
    VAEncPictureParameterBufferVP9 *pic_param;

    if (!encode_state->pic_param_ext ||
        !encode_state->pic_param_ext->buffer) {
        return;
    }

    pic_param = (VAEncPictureParameterBufferVP9 *)encode_state->pic_param_ext->buffer;

    batch_value = seg_param->seg_flags.bits.segment_reference;
    if (pic_param->pic_flags.bits.frame_type == HCP_VP9_KEY_FRAME ||
        pic_param->pic_flags.bits.intra_only)
        batch_value = 0;

    BEGIN_BCS_BATCH(batch, 8);

    OUT_BCS_BATCH(batch, HCP_VP9_SEGMENT_STATE | (8 - 2));
    OUT_BCS_BATCH(batch, seg_id << 0); /* DW 1 - SegmentID */
    OUT_BCS_BATCH(batch,
                  (seg_param->seg_flags.bits.segment_reference_enabled << 3) |
                  (batch_value << 1) |
                  (seg_param->seg_flags.bits.segment_reference_skipped << 0)
                 );

    /* DW 3..6 is not used for encoder */
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);

    /* DW 7 Mode */
    tmp = intel_convert_sign_mag(seg_param->segment_qindex_delta, 9);
    batch_value = tmp;
    tmp = intel_convert_sign_mag(seg_param->segment_lf_level_delta, 7);
    batch_value |= (tmp << 16);
    OUT_BCS_BATCH(batch, batch_value);

    ADVANCE_BCS_BATCH(batch);

}

static void
intel_vp9enc_construct_pak_insertobj_batchbuffer(VADriverContextP ctx,
                                                 struct intel_encoder_context *encoder_context,
                                                 struct i965_gpe_resource *obj_batch_buffer)
{
    struct gen9_encoder_context_vp9 *pak_context = encoder_context->mfc_context;
    struct gen9_vp9_state *vp9_state;
    int uncompressed_header_length;
    unsigned int *cmd_ptr;
    unsigned int dw_length, bits_in_last_dw;

    vp9_state = (struct gen9_vp9_state *)(encoder_context->enc_priv_state);

    if (!pak_context || !vp9_state || !vp9_state->pic_param)
        return;

    uncompressed_header_length = vp9_state->header_length;
    cmd_ptr = i965_map_gpe_resource(obj_batch_buffer);

    if (!cmd_ptr)
        return;

    bits_in_last_dw = uncompressed_header_length % 4;
    bits_in_last_dw *= 8;

    if (bits_in_last_dw == 0)
        bits_in_last_dw = 32;

    /* get the DWORD length of the inserted_data */
    dw_length = ALIGN(uncompressed_header_length, 4) / 4;
    *cmd_ptr++ = HCP_INSERT_PAK_OBJECT | dw_length;

    *cmd_ptr++ = ((0 << 31) | /* indirect payload */
                  (0 << 16) | /* the start offset in first DW */
                  (0 << 15) |
                  (bits_in_last_dw << 8) | /* bits_in_last_dw */
                  (0 << 4) |  /* skip emulation byte count. 0 for VP9 */
                  (0 << 3) |  /* emulation flag. 0 for VP9 */
                  (1 << 2) |  /* last header flag. */
                  (0 << 1));
    memcpy(cmd_ptr, vp9_state->alias_insert_data, dw_length * sizeof(unsigned int));

    cmd_ptr += dw_length;

    *cmd_ptr++ = MI_NOOP;
    *cmd_ptr++ = MI_BATCH_BUFFER_END;
    i965_unmap_gpe_resource(obj_batch_buffer);
}

static void
gen9_vp9_pak_picture_level(VADriverContextP ctx,
                           struct encode_state *encode_state,
                           struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct gen9_encoder_context_vp9 *pak_context = encoder_context->mfc_context;
    struct object_surface *obj_surface;
    VAEncPictureParameterBufferVP9 *pic_param;
    VAEncMiscParameterTypeVP9PerSegmantParam *seg_param, tmp_seg_param;
    struct gen9_vp9_state *vp9_state;
    struct gen9_surface_vp9 *vp9_priv_surface;
    int i;
    struct gen9_hcpe_pipe_mode_select_param mode_param;
    hcp_surface_state hcp_surface;
    struct gpe_mi_batch_buffer_start_parameter second_level_batch;
    int segment_count;

    vp9_state = (struct gen9_vp9_state *)(encoder_context->enc_priv_state);

    if (!pak_context || !vp9_state || !vp9_state->pic_param)
        return;

    pic_param = vp9_state->pic_param;
    seg_param = vp9_state->segment_param;

    if (vp9_state->curr_pak_pass == 0) {
        intel_vp9enc_construct_pak_insertobj_batchbuffer(ctx, encoder_context,
                                                         &pak_context->res_pak_uncompressed_input_buffer);

        // Check if driver already programmed pic state as part of BRC update kernel programming.
        if (!vp9_state->brc_enabled) {
            intel_vp9enc_construct_picstate_batchbuf(ctx, encode_state,
                                                     encoder_context, &pak_context->res_pic_state_brc_write_hfw_read_buffer);
        }
    }

    if (vp9_state->curr_pak_pass == 0) {
        intel_vp9enc_refresh_frame_internal_buffers(ctx, encoder_context);
    }

    {
        /* copy the frame_context[frame_idx] into curr_frame_context */
        memcpy(&vp9_state->vp9_current_fc,
               &(vp9_state->vp9_frame_ctx[vp9_state->frame_ctx_idx]),
               sizeof(FRAME_CONTEXT));
        {
            uint8_t *prob_ptr;

            prob_ptr = i965_map_gpe_resource(&pak_context->res_prob_buffer);

            if (!prob_ptr)
                return;

            /* copy the current fc to vp9_prob buffer */
            memcpy(prob_ptr, &vp9_state->vp9_current_fc, sizeof(FRAME_CONTEXT));
            if ((pic_param->pic_flags.bits.frame_type == HCP_VP9_KEY_FRAME) ||
                pic_param->pic_flags.bits.intra_only) {
                FRAME_CONTEXT *frame_ptr = (FRAME_CONTEXT *)prob_ptr;

                memcpy(frame_ptr->partition_prob, vp9_kf_partition_probs,
                       sizeof(vp9_kf_partition_probs));
                memcpy(frame_ptr->uv_mode_prob, vp9_kf_uv_mode_prob,
                       sizeof(vp9_kf_uv_mode_prob));
            }
            i965_unmap_gpe_resource(&pak_context->res_prob_buffer);
        }
    }

    if (vp9_state->brc_enabled && vp9_state->curr_pak_pass) {
        /* read image status and insert the conditional end cmd */
        /* image ctrl/status is already accessed */
        struct gpe_mi_conditional_batch_buffer_end_parameter mi_cond_end;
        struct vp9_encode_status_buffer_internal *status_buffer;

        status_buffer = &vp9_state->status_buffer;
        memset(&mi_cond_end, 0, sizeof(mi_cond_end));
        mi_cond_end.offset = status_buffer->image_status_mask_offset;
        mi_cond_end.bo = status_buffer->bo;
        mi_cond_end.compare_data = 0;
        mi_cond_end.compare_mask_mode_disabled = 1;
        gen9_gpe_mi_conditional_batch_buffer_end(ctx, batch,
                                                 &mi_cond_end);
    }

    mode_param.codec_mode = 1;
    mode_param.stream_out = 0;
    gen9_pak_vp9_pipe_mode_select(ctx, encode_state, encoder_context, &mode_param);

    /* reconstructed surface */
    memset(&hcp_surface, 0, sizeof(hcp_surface));
    obj_surface = encode_state->reconstructed_object;
    hcp_surface.dw1.surface_id = 0;
    hcp_surface.dw1.surface_pitch = obj_surface->width;
    hcp_surface.dw2.surface_format = SURFACE_FORMAT_PLANAR_420_8;
    hcp_surface.dw2.y_cb_offset = obj_surface->y_cb_offset;
    gen9_vp9_add_surface_state(ctx, encode_state, encoder_context,
                               &hcp_surface);

    /* Input surface */
    if (vp9_state->dys_in_use &&
        ((pic_param->frame_width_src != pic_param->frame_width_dst) ||
         (pic_param->frame_height_src != pic_param->frame_height_dst))) {
        vp9_priv_surface = (struct gen9_surface_vp9 *)(obj_surface->private_data);
        obj_surface = vp9_priv_surface->dys_surface_obj;
    } else {
        obj_surface = encode_state->input_yuv_object;
    }

    hcp_surface.dw1.surface_id = 1;
    hcp_surface.dw1.surface_pitch = obj_surface->width;
    hcp_surface.dw2.surface_format = SURFACE_FORMAT_PLANAR_420_8;
    hcp_surface.dw2.y_cb_offset = obj_surface->y_cb_offset;
    gen9_vp9_add_surface_state(ctx, encode_state, encoder_context,
                               &hcp_surface);

    if (vp9_state->picture_coding_type) {
        /* Add surface for last */
        if (vp9_state->last_ref_obj) {
            obj_surface = vp9_state->last_ref_obj;
            hcp_surface.dw1.surface_id = 2;
            hcp_surface.dw1.surface_pitch = obj_surface->width;
            hcp_surface.dw2.surface_format = SURFACE_FORMAT_PLANAR_420_8;
            hcp_surface.dw2.y_cb_offset = obj_surface->y_cb_offset;
            gen9_vp9_add_surface_state(ctx, encode_state, encoder_context,
                                       &hcp_surface);
        }
        if (vp9_state->golden_ref_obj) {
            obj_surface = vp9_state->golden_ref_obj;
            hcp_surface.dw1.surface_id = 3;
            hcp_surface.dw1.surface_pitch = obj_surface->width;
            hcp_surface.dw2.surface_format = SURFACE_FORMAT_PLANAR_420_8;
            hcp_surface.dw2.y_cb_offset = obj_surface->y_cb_offset;
            gen9_vp9_add_surface_state(ctx, encode_state, encoder_context,
                                       &hcp_surface);
        }
        if (vp9_state->alt_ref_obj) {
            obj_surface = vp9_state->alt_ref_obj;
            hcp_surface.dw1.surface_id = 4;
            hcp_surface.dw1.surface_pitch = obj_surface->width;
            hcp_surface.dw2.surface_format = SURFACE_FORMAT_PLANAR_420_8;
            hcp_surface.dw2.y_cb_offset = obj_surface->y_cb_offset;
            gen9_vp9_add_surface_state(ctx, encode_state, encoder_context,
                                       &hcp_surface);
        }
    }

    gen9_pak_vp9_pipe_buf_addr_state(ctx, encode_state, encoder_context);

    gen9_pak_vp9_ind_obj_base_addr_state(ctx, encode_state, encoder_context);

    // Using picstate zero with updated QP and LF deltas by HuC for repak, irrespective of how many Pak passes were run in multi-pass mode.
    memset(&second_level_batch, 0, sizeof(second_level_batch));

    if (vp9_state->curr_pak_pass == 0) {
        second_level_batch.offset = 0;
    } else
        second_level_batch.offset = vp9_state->curr_pak_pass * VP9_PIC_STATE_BUFFER_SIZE;

    second_level_batch.is_second_level = 1;
    second_level_batch.bo = pak_context->res_pic_state_brc_write_hfw_read_buffer.bo;

    gen8_gpe_mi_batch_buffer_start(ctx, batch, &second_level_batch);

    if (pic_param->pic_flags.bits.segmentation_enabled &&
        seg_param)
        segment_count = 8;
    else {
        segment_count = 1;
        memset(&tmp_seg_param, 0, sizeof(tmp_seg_param));
        seg_param = &tmp_seg_param;
    }
    for (i = 0; i < segment_count; i++) {
        gen9_pak_vp9_segment_state(ctx, encode_state,
                                   encoder_context,
                                   &seg_param->seg_data[i], i);
    }

    /* Insert the uncompressed header buffer */
    second_level_batch.is_second_level = 1;
    second_level_batch.offset = 0;
    second_level_batch.bo = pak_context->res_pak_uncompressed_input_buffer.bo;

    gen8_gpe_mi_batch_buffer_start(ctx, batch, &second_level_batch);

    /* PAK_OBJECT */
    second_level_batch.is_second_level = 1;
    second_level_batch.offset = 0;
    second_level_batch.bo = pak_context->res_mb_code_surface.bo;
    gen8_gpe_mi_batch_buffer_start(ctx, batch, &second_level_batch);

    return;
}

static void
gen9_vp9_read_mfc_status(VADriverContextP ctx, struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct gen9_encoder_context_vp9 *pak_context = encoder_context->mfc_context;
    struct gpe_mi_store_register_mem_parameter mi_store_reg_mem_param;
    struct gpe_mi_flush_dw_parameter mi_flush_dw_param;
    //struct gpe_mi_copy_mem_parameter mi_copy_mem_param;
    struct vp9_encode_status_buffer_internal *status_buffer;
    struct gen9_vp9_state *vp9_state;

    vp9_state = (struct gen9_vp9_state *)(encoder_context->enc_priv_state);
    if (!vp9_state || !pak_context || !batch)
        return;

    status_buffer = &(vp9_state->status_buffer);

    memset(&mi_flush_dw_param, 0, sizeof(mi_flush_dw_param));
    gen8_gpe_mi_flush_dw(ctx, batch, &mi_flush_dw_param);

    memset(&mi_store_reg_mem_param, 0, sizeof(mi_store_reg_mem_param));
    mi_store_reg_mem_param.bo = status_buffer->bo;
    mi_store_reg_mem_param.offset = status_buffer->bs_byte_count_offset;
    mi_store_reg_mem_param.mmio_offset = status_buffer->vp9_bs_frame_reg_offset;
    gen8_gpe_mi_store_register_mem(ctx, batch, &mi_store_reg_mem_param);

    mi_store_reg_mem_param.bo = pak_context->res_brc_bitstream_size_buffer.bo;
    mi_store_reg_mem_param.offset = 0;
    mi_store_reg_mem_param.mmio_offset = status_buffer->vp9_bs_frame_reg_offset;
    gen8_gpe_mi_store_register_mem(ctx, batch, &mi_store_reg_mem_param);

    /* Read HCP Image status */
    mi_store_reg_mem_param.bo = status_buffer->bo;
    mi_store_reg_mem_param.offset = status_buffer->image_status_mask_offset;
    mi_store_reg_mem_param.mmio_offset =
        status_buffer->vp9_image_mask_reg_offset;
    gen8_gpe_mi_store_register_mem(ctx, batch, &mi_store_reg_mem_param);

    mi_store_reg_mem_param.bo = status_buffer->bo;
    mi_store_reg_mem_param.offset = status_buffer->image_status_ctrl_offset;
    mi_store_reg_mem_param.mmio_offset =
        status_buffer->vp9_image_ctrl_reg_offset;
    gen8_gpe_mi_store_register_mem(ctx, batch, &mi_store_reg_mem_param);

    mi_store_reg_mem_param.bo = pak_context->res_brc_bitstream_size_buffer.bo;
    mi_store_reg_mem_param.offset = 4;
    mi_store_reg_mem_param.mmio_offset =
        status_buffer->vp9_image_ctrl_reg_offset;
    gen8_gpe_mi_store_register_mem(ctx, batch, &mi_store_reg_mem_param);

    gen8_gpe_mi_flush_dw(ctx, batch, &mi_flush_dw_param);

    return;
}

static VAStatus
gen9_vp9_pak_pipeline_prepare(VADriverContextP ctx,
                              struct encode_state *encode_state,
                              struct intel_encoder_context *encoder_context)
{
    struct gen9_encoder_context_vp9 *pak_context = encoder_context->mfc_context;
    struct object_surface *obj_surface;
    struct object_buffer *obj_buffer;
    struct i965_coded_buffer_segment *coded_buffer_segment;
    VAEncPictureParameterBufferVP9 *pic_param;
    struct gen9_vp9_state *vp9_state;
    dri_bo *bo;
    int i;

    vp9_state = (struct gen9_vp9_state *)(encoder_context->enc_priv_state);
    if (!vp9_state ||
        !vp9_state->pic_param)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    pic_param = vp9_state->pic_param;

    /* reconstructed surface */
    obj_surface = encode_state->reconstructed_object;
    i965_check_alloc_surface_bo(ctx, obj_surface, 1, VA_FOURCC_NV12, SUBSAMPLE_YUV420);

    dri_bo_unreference(pak_context->reconstructed_object.bo);

    pak_context->reconstructed_object.bo = obj_surface->bo;
    dri_bo_reference(pak_context->reconstructed_object.bo);

    /* set vp9 reference frames */
    for (i = 0; i < ARRAY_ELEMS(pak_context->reference_surfaces); i++) {
        if (pak_context->reference_surfaces[i].bo)
            dri_bo_unreference(pak_context->reference_surfaces[i].bo);
        pak_context->reference_surfaces[i].bo = NULL;
    }

    /* Three reference frames are enough for VP9 */
    if (pic_param->pic_flags.bits.frame_type &&
        !pic_param->pic_flags.bits.intra_only) {
        for (i = 0; i < 3; i++) {
            obj_surface = encode_state->reference_objects[i];
            if (obj_surface && obj_surface->bo) {
                pak_context->reference_surfaces[i].bo = obj_surface->bo;
                dri_bo_reference(obj_surface->bo);
            }
        }
    }

    /* input YUV surface */
    dri_bo_unreference(pak_context->uncompressed_picture_source.bo);
    pak_context->uncompressed_picture_source.bo = NULL;
    obj_surface = encode_state->reconstructed_object;
    if (vp9_state->dys_in_use &&
        ((pic_param->frame_width_src != pic_param->frame_width_dst) ||
         (pic_param->frame_height_src != pic_param->frame_height_dst))) {
        struct gen9_surface_vp9 *vp9_priv_surface =
            (struct gen9_surface_vp9 *)(obj_surface->private_data);
        obj_surface = vp9_priv_surface->dys_surface_obj;
    } else
        obj_surface = encode_state->input_yuv_object;

    pak_context->uncompressed_picture_source.bo = obj_surface->bo;
    dri_bo_reference(pak_context->uncompressed_picture_source.bo);

    /* coded buffer */
    dri_bo_unreference(pak_context->indirect_pak_bse_object.bo);
    pak_context->indirect_pak_bse_object.bo = NULL;
    obj_buffer = encode_state->coded_buf_object;
    bo = obj_buffer->buffer_store->bo;
    pak_context->indirect_pak_bse_object.offset = I965_CODEDBUFFER_HEADER_SIZE;
    pak_context->indirect_pak_bse_object.end_offset = ALIGN((obj_buffer->size_element - 0x1000), 0x1000);
    pak_context->indirect_pak_bse_object.bo = bo;
    dri_bo_reference(pak_context->indirect_pak_bse_object.bo);

    /* set the internal flag to 0 to indicate the coded size is unknown */
    dri_bo_map(bo, 1);
    coded_buffer_segment = (struct i965_coded_buffer_segment *)bo->virtual;
    coded_buffer_segment->mapped = 0;
    coded_buffer_segment->codec = encoder_context->codec;
    coded_buffer_segment->status_support = 1;
    dri_bo_unmap(bo);

    return VA_STATUS_SUCCESS;
}

static void
gen9_vp9_pak_brc_prepare(struct encode_state *encode_state,
                         struct intel_encoder_context *encoder_context)
{
}

static void
gen9_vp9_pak_context_destroy(void *context)
{
    struct gen9_encoder_context_vp9 *pak_context = context;
    int i;

    dri_bo_unreference(pak_context->reconstructed_object.bo);
    pak_context->reconstructed_object.bo = NULL;

    dri_bo_unreference(pak_context->uncompressed_picture_source.bo);
    pak_context->uncompressed_picture_source.bo = NULL;

    dri_bo_unreference(pak_context->indirect_pak_bse_object.bo);
    pak_context->indirect_pak_bse_object.bo = NULL;

    for (i = 0; i < 8; i++) {
        dri_bo_unreference(pak_context->reference_surfaces[i].bo);
        pak_context->reference_surfaces[i].bo = NULL;
    }

    /* vme & pak same the same structure, so don't free the context here */
}

static VAStatus
gen9_vp9_pak_pipeline(VADriverContextP ctx,
                      VAProfile profile,
                      struct encode_state *encode_state,
                      struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct gen9_encoder_context_vp9 *pak_context = encoder_context->mfc_context;
    VAStatus va_status;
    struct gen9_vp9_state *vp9_state;
    VAEncPictureParameterBufferVP9 *pic_param;
    int i;

    vp9_state = (struct gen9_vp9_state *)(encoder_context->enc_priv_state);

    if (!vp9_state || !vp9_state->pic_param || !pak_context)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    va_status = gen9_vp9_pak_pipeline_prepare(ctx, encode_state, encoder_context);

    if (va_status != VA_STATUS_SUCCESS)
        return va_status;

    if (i965->intel.has_bsd2)
        intel_batchbuffer_start_atomic_bcs_override(batch, 0x1000, BSD_RING0);
    else
        intel_batchbuffer_start_atomic_bcs(batch, 0x1000);

    intel_batchbuffer_emit_mi_flush(batch);

    BEGIN_BCS_BATCH(batch, 64);
    for (i = 0; i < 64; i++)
        OUT_BCS_BATCH(batch, MI_NOOP);

    ADVANCE_BCS_BATCH(batch);

    for (vp9_state->curr_pak_pass = 0;
         vp9_state->curr_pak_pass < vp9_state->num_pak_passes;
         vp9_state->curr_pak_pass++) {

        if (vp9_state->curr_pak_pass == 0) {
            /* Initialize the VP9 Image Ctrl reg for the first pass */
            struct gpe_mi_load_register_imm_parameter mi_load_reg_imm;
            struct vp9_encode_status_buffer_internal *status_buffer;

            status_buffer = &(vp9_state->status_buffer);
            memset(&mi_load_reg_imm, 0, sizeof(mi_load_reg_imm));
            mi_load_reg_imm.mmio_offset = status_buffer->vp9_image_ctrl_reg_offset;
            mi_load_reg_imm.data = 0;
            gen8_gpe_mi_load_register_imm(ctx, batch, &mi_load_reg_imm);
        }
        gen9_vp9_pak_picture_level(ctx, encode_state, encoder_context);
        gen9_vp9_read_mfc_status(ctx, encoder_context);
    }

    intel_batchbuffer_end_atomic(batch);
    intel_batchbuffer_flush(batch);

    pic_param = vp9_state->pic_param;
    vp9_state->vp9_last_frame.frame_width = pic_param->frame_width_dst;
    vp9_state->vp9_last_frame.frame_height = pic_param->frame_height_dst;
    vp9_state->vp9_last_frame.frame_type = pic_param->pic_flags.bits.frame_type;
    vp9_state->vp9_last_frame.show_frame = pic_param->pic_flags.bits.show_frame;
    vp9_state->vp9_last_frame.refresh_frame_context = pic_param->pic_flags.bits.refresh_frame_context;
    vp9_state->vp9_last_frame.frame_context_idx = pic_param->pic_flags.bits.frame_context_idx;
    vp9_state->vp9_last_frame.intra_only = pic_param->pic_flags.bits.intra_only;
    vp9_state->frame_number++;
    vp9_state->curr_mv_temporal_index ^= 1;
    vp9_state->first_frame = 0;

    return VA_STATUS_SUCCESS;
}

Bool
gen9_vp9_vme_context_init(VADriverContextP ctx, struct intel_encoder_context *encoder_context)
{
    struct gen9_encoder_context_vp9 *vme_context = NULL;
    struct gen9_vp9_state *vp9_state = NULL;

    vme_context = calloc(1, sizeof(struct gen9_encoder_context_vp9));
    vp9_state = calloc(1, sizeof(struct gen9_vp9_state));

    if (!vme_context || !vp9_state) {
        if (vme_context)
            free(vme_context);
        if (vp9_state)
            free(vp9_state);
        return false;
    }

    encoder_context->enc_priv_state = vp9_state;
    vme_context->enc_priv_state = vp9_state;

    /* Initialize the features that are supported by VP9 */
    vme_context->hme_supported = 1;
    vme_context->use_hw_scoreboard = 1;
    vme_context->use_hw_non_stalling_scoreboard = 1;

    vp9_state->tx_mode = TX_MODE_SELECT;
    vp9_state->multi_ref_qp_check = 0;
    vp9_state->target_usage = INTEL_ENC_VP9_TU_NORMAL;
    vp9_state->num_pak_passes = 1;
    vp9_state->hme_supported = vme_context->hme_supported;
    vp9_state->b16xme_supported = 1;

    if (encoder_context->rate_control_mode != VA_RC_NONE &&
        encoder_context->rate_control_mode != VA_RC_CQP) {
        vp9_state->brc_enabled = 1;
        vp9_state->brc_distortion_buffer_supported = 1;
        vp9_state->brc_constant_buffer_supported = 1;
        vp9_state->num_pak_passes = 4;
    }
    vp9_state->dys_enabled = 1; /* this is supported by default */
    vp9_state->first_frame = 1;

    /* the definition of status buffer offset for VP9 */
    {
        struct vp9_encode_status_buffer_internal *status_buffer;
        uint32_t base_offset = offsetof(struct i965_coded_buffer_segment, codec_private_data);

        status_buffer = &vp9_state->status_buffer;
        memset(status_buffer, 0,
               sizeof(struct vp9_encode_status_buffer_internal));

        status_buffer->bs_byte_count_offset = base_offset + offsetof(struct vp9_encode_status, bs_byte_count);
        status_buffer->image_status_mask_offset = base_offset + offsetof(struct vp9_encode_status, image_status_mask);
        status_buffer->image_status_ctrl_offset = base_offset + offsetof(struct vp9_encode_status, image_status_ctrl);
        status_buffer->media_index_offset       = base_offset + offsetof(struct vp9_encode_status, media_index);

        status_buffer->vp9_bs_frame_reg_offset = 0x1E9E0;
        status_buffer->vp9_image_mask_reg_offset = 0x1E9F0;
        status_buffer->vp9_image_ctrl_reg_offset = 0x1E9F4;
    }

    gen9_vme_kernels_context_init_vp9(ctx, encoder_context, vme_context);

    encoder_context->vme_context = vme_context;
    encoder_context->vme_pipeline = gen9_vme_pipeline_vp9;
    encoder_context->vme_context_destroy = gen9_vme_context_destroy_vp9;

    return true;
}

static VAStatus
gen9_vp9_get_coded_status(VADriverContextP ctx,
                          struct intel_encoder_context *encoder_context,
                          struct i965_coded_buffer_segment *coded_buf_seg)
{
    struct vp9_encode_status *vp9_encode_status;

    if (!encoder_context || !coded_buf_seg)
        return VA_STATUS_ERROR_INVALID_BUFFER;

    vp9_encode_status = (struct vp9_encode_status *)coded_buf_seg->codec_private_data;
    coded_buf_seg->base.size = vp9_encode_status->bs_byte_count;

    /* One VACodedBufferSegment for VP9 will be added later.
     * It will be linked to the next element of coded_buf_seg->base.next
     */

    return VA_STATUS_SUCCESS;
}

Bool
gen9_vp9_pak_context_init(VADriverContextP ctx, struct intel_encoder_context *encoder_context)
{
    /* VME & PAK share the same context */
    struct gen9_encoder_context_vp9 *pak_context = encoder_context->vme_context;

    if (!pak_context)
        return false;

    encoder_context->mfc_context = pak_context;
    encoder_context->mfc_context_destroy = gen9_vp9_pak_context_destroy;
    encoder_context->mfc_pipeline = gen9_vp9_pak_pipeline;
    encoder_context->mfc_brc_prepare = gen9_vp9_pak_brc_prepare;
    encoder_context->get_status = gen9_vp9_get_coded_status;
    return true;
}
