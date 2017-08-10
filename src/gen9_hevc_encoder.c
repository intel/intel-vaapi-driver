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
 * SOFTWAR OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Chen, Peng <chen.c.peng@intel.com>
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
#include "i965_encoder_common.h"
#include "i965_encoder_utils.h"
#include "i965_encoder_api.h"
#include "gen9_hevc_enc_kernels.h"
#include "gen9_hevc_enc_kernels_binary.h"
#include "gen9_hevc_enc_utils.h"
#include "gen9_hevc_encoder.h"
#include "gen9_hevc_enc_const_def.h"

static void *hevc_enc_kernel_ptr = NULL;
static int hevc_enc_kernel_size = 0;

static bool
gen9_hevc_get_kernel_header_and_size(void *pvbinary,
                                     int binary_size,
                                     GEN9_ENC_OPERATION operation,
                                     int krnstate_idx,
                                     struct i965_kernel *ret_kernel)
{
    typedef uint32_t BIN_PTR[4];

    char *bin_start;
    gen9_hevc_enc_kernels_header_bxt *pkh_table = NULL;
    gen9_hevc_enc_kernel_header *pcurr_header = NULL, *pinvalid_entry, *pnext_header;
    int next_krnoffset;

    if (!pvbinary || !ret_kernel)
        return false;

    bin_start = (char *)pvbinary;
    pkh_table = (gen9_hevc_enc_kernels_header_bxt *)pvbinary;
    pinvalid_entry = (gen9_hevc_enc_kernel_header *)(pkh_table + 1);
    next_krnoffset = binary_size;

    switch (operation) {
    case GEN9_ENC_SCALING2X:
        pcurr_header = &pkh_table->HEVC_ENC_I_2xDownSampling_Kernel;
        break;
    case GEN9_ENC_SCALING4X:
        pcurr_header = &pkh_table->HEVC_ENC_I_DS4HME;
        break;
    case GEN9_ENC_ME:
        if (krnstate_idx)
            pcurr_header = &pkh_table->HEVC_ENC_P_HME;
        else
            pcurr_header = &pkh_table->HEVC_ENC_B_HME;
        break;
    case GEN9_ENC_BRC:
        switch (krnstate_idx) {
        case GEN9_HEVC_ENC_BRC_COARSE_INTRA:
            pcurr_header = &pkh_table->HEVC_ENC_I_COARSE;
            break;
        case GEN9_HEVC_ENC_BRC_INIT:
            pcurr_header = &pkh_table->HEVC_ENC_BRC_Init;
            break;
        case GEN9_HEVC_ENC_BRC_RESET:
            pcurr_header = &pkh_table->HEVC_ENC_BRC_Reset;
            break;
        case GEN9_HEVC_ENC_BRC_FRAME_UPDATE:
            pcurr_header = &pkh_table->HEVC_ENC_BRC_Update;
            break;
        case GEN9_HEVC_ENC_BRC_LCU_UPDATE:
            pcurr_header = &pkh_table->HEVC_ENC_BRC_LCU_Update;
            break;
        default:
            break;
        }
        break;
    case GEN9_ENC_MBENC:
        switch (krnstate_idx) {
        case GEN9_HEVC_ENC_MBENC_2xSCALING:
        case GEN9_HEVC_ENC_MBENC_32x32MD:
        case GEN9_HEVC_ENC_MBENC_16x16SAD:
        case GEN9_HEVC_ENC_MBENC_16x16MD:
        case GEN9_HEVC_ENC_MBENC_8x8PU:
        case GEN9_HEVC_ENC_MBENC_8x8FMODE:
        case GEN9_HEVC_ENC_MBENC_32x32INTRACHECK:
        case GEN9_HEVC_ENC_MBENC_BENC:
            pcurr_header = &pkh_table->HEVC_ENC_I_2xDownSampling_Kernel;
            pcurr_header += krnstate_idx;
            break;
        case GEN9_HEVC_ENC_MBENC_BPAK:
            pcurr_header = &pkh_table->HEVC_ENC_PB_Pak;
            break;
        case GEN9_HEVC_ENC_MBENC_WIDI:
            pcurr_header = &pkh_table->HEVC_ENC_PB_Widi;
            break;
        case GEN9_HEVC_MBENC_PENC:
            pcurr_header = &pkh_table->HEVC_ENC_P_MB;
            break;
        case GEN9_HEVC_MBENC_P_WIDI:
            pcurr_header = &pkh_table->HEVC_ENC_P_Widi;
            break;
        case GEN9_HEVC_ENC_MBENC_DS_COMBINED:
            pcurr_header = &pkh_table->HEVC_ENC_DS_Combined;
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }

    if (!pcurr_header ||
        ((pcurr_header->kernel_start_pointer << 6) >= binary_size))
        return false;

    ret_kernel->bin = (const BIN_PTR *)(bin_start + (pcurr_header->kernel_start_pointer << 6));

    pnext_header = (pcurr_header + 1);
    if (pnext_header < pinvalid_entry)
        next_krnoffset = pnext_header->kernel_start_pointer << 6;

    ret_kernel->size = next_krnoffset - (pcurr_header->kernel_start_pointer << 6);

    return true;
}

static void
gen9_hevc_store_reg_mem(VADriverContextP ctx,
                        struct intel_batchbuffer *batch,
                        dri_bo *bo,
                        unsigned int offset,
                        unsigned int mmio_offset)
{
    struct gpe_mi_store_register_mem_parameter mi_store_reg_mem_param;

    memset((void *)&mi_store_reg_mem_param, 0,
           sizeof(mi_store_reg_mem_param));
    mi_store_reg_mem_param.bo = bo;
    mi_store_reg_mem_param.offset = offset;
    mi_store_reg_mem_param.mmio_offset = mmio_offset;
    gen8_gpe_mi_store_register_mem(ctx, batch, &mi_store_reg_mem_param);
}

static void
gen9_hevc_load_reg_mem(VADriverContextP ctx,
                       struct intel_batchbuffer *batch,
                       dri_bo *bo,
                       unsigned int offset,
                       unsigned int mmio_offset)
{
    struct gpe_mi_load_register_mem_parameter mi_load_reg_mem_param;

    memset((void *)&mi_load_reg_mem_param, 0,
           sizeof(mi_load_reg_mem_param));
    mi_load_reg_mem_param.bo = bo;
    mi_load_reg_mem_param.offset = offset;
    mi_load_reg_mem_param.mmio_offset = mmio_offset;
    gen8_gpe_mi_load_register_mem(ctx, batch, &mi_load_reg_mem_param);
}

static void
gen9_hevc_conditional_end(VADriverContextP ctx,
                          struct intel_batchbuffer *batch,
                          dri_bo *bo,
                          unsigned int offset,
                          unsigned int data)
{
    struct gpe_mi_conditional_batch_buffer_end_parameter mi_cond_end;

    memset(&mi_cond_end, 0, sizeof(mi_cond_end));
    mi_cond_end.offset = offset;
    mi_cond_end.bo = bo;
    mi_cond_end.compare_data = data;
    mi_cond_end.compare_mask_mode_disabled = 0;
    gen9_gpe_mi_conditional_batch_buffer_end(ctx, batch,
                                             &mi_cond_end);
}

static VAStatus
gen9_hevc_ensure_surface(VADriverContextP ctx,
                         struct gen9_hevc_encoder_state *priv_state,
                         struct object_surface *obj_surface,
                         int reallocate_flag)
{
    VAStatus va_status = VA_STATUS_SUCCESS;
    int update = 0;
    unsigned int fourcc = VA_FOURCC_NV12;

    if (!obj_surface) {
        va_status = VA_STATUS_ERROR_INVALID_PARAMETER;

        goto EXIT;
    }

    if ((priv_state->bit_depth_luma_minus8 > 0)
        || (priv_state->bit_depth_chroma_minus8 > 0)) {
        if (obj_surface->fourcc != VA_FOURCC_P010) {
            update = 1;
            fourcc = VA_FOURCC_P010;
        }
    } else if (obj_surface->fourcc != VA_FOURCC_NV12) {
        update = 1;
        fourcc = VA_FOURCC_NV12;
    }

    /* (Re-)allocate the underlying surface buffer store, if necessary */
    if (!obj_surface->bo || update) {
        if (reallocate_flag) {
            struct i965_driver_data * const i965 = i965_driver_data(ctx);

            i965_destroy_surface_storage(obj_surface);

            va_status = i965_check_alloc_surface_bo(ctx,
                                                    obj_surface,
                                                    i965->codec_info->has_tiled_surface,
                                                    fourcc,
                                                    SUBSAMPLE_YUV420);
        } else
            va_status = VA_STATUS_ERROR_INVALID_PARAMETER;
    }

EXIT:
    return va_status;
}

static void
gen9_hevc_free_surface_private(void **data)
{
    struct gen9_hevc_surface_priv *surface_priv = NULL;
    int i = 0;

    surface_priv = (struct gen9_hevc_surface_priv *)(*data);
    if (!surface_priv)
        return;

    for (i = 0; i < HEVC_SCALED_SURFS_NUM; i++) {
        if (surface_priv->scaled_surface_obj[i]) {
            i965_DestroySurfaces(surface_priv->ctx, &surface_priv->scaled_surface_id[i], 1);
            surface_priv->scaled_surface_id[i] = VA_INVALID_SURFACE;
            surface_priv->scaled_surface_obj[i] = NULL;
        }
    }

    if (surface_priv->surface_obj_nv12) {
        i965_DestroySurfaces(surface_priv->ctx, &surface_priv->surface_id_nv12, 1);
        surface_priv->surface_id_nv12 = VA_INVALID_SURFACE;
        surface_priv->surface_obj_nv12 = NULL;
    }

    if (surface_priv->motion_vector_temporal_bo)
        dri_bo_unreference(surface_priv->motion_vector_temporal_bo);

    free(surface_priv);
}

static VAStatus
gen9_hevc_init_surface_private(VADriverContextP ctx,
                               struct generic_enc_codec_state *generic_state,
                               struct gen9_hevc_encoder_state *priv_state,
                               struct object_surface *obj_surface)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct gen9_hevc_surface_priv *surface_priv = NULL;
    int size = 0;

    if (!obj_surface || !obj_surface->bo)
        return VA_STATUS_ERROR_INVALID_SURFACE;

    if (obj_surface->private_data &&
        obj_surface->free_private_data != gen9_hevc_free_surface_private) {
        obj_surface->free_private_data(&obj_surface->private_data);
        obj_surface->private_data = NULL;
    }

    if (obj_surface->private_data) {
        surface_priv = (struct gen9_hevc_surface_priv *)obj_surface->private_data;

        surface_priv->surface_nv12_valid = 0;
    } else {
        surface_priv = calloc(1, sizeof(*surface_priv));
        if (!surface_priv)
            return VA_STATUS_ERROR_ALLOCATION_FAILED;

        surface_priv->ctx = ctx;
        surface_priv->surface_reff = obj_surface;

        obj_surface->private_data = (void *)surface_priv;
        obj_surface->free_private_data = gen9_hevc_free_surface_private;

        // alloc motion vector temporal buffer
        size = MAX(((priv_state->picture_width + 63) >> 6) *
                   ((priv_state->picture_height + 15) >> 4),
                   ((priv_state->picture_width + 31) >> 5) *
                   ((priv_state->picture_height + 31) >> 5));
        size = ALIGN(size, 2) * 64;
        surface_priv->motion_vector_temporal_bo =
            dri_bo_alloc(i965->intel.bufmgr,
                         "motion vector temporal buffer",
                         size,
                         0x1000);
        if (!surface_priv->motion_vector_temporal_bo)
            return VA_STATUS_ERROR_ALLOCATION_FAILED;

        //alloc HME surfaces
        i965_CreateSurfaces(ctx,
                            priv_state->frame_width_4x,
                            priv_state->frame_height_4x,
                            VA_RT_FORMAT_YUV420,
                            1,
                            &surface_priv->scaled_surface_id[HEVC_SCALED_SURF_4X_ID]);

        surface_priv->scaled_surface_obj[HEVC_SCALED_SURF_4X_ID] =
            SURFACE(surface_priv->scaled_surface_id[HEVC_SCALED_SURF_4X_ID]);

        if (!surface_priv->scaled_surface_obj[HEVC_SCALED_SURF_4X_ID])
            return VA_STATUS_ERROR_ALLOCATION_FAILED;

        i965_check_alloc_surface_bo(ctx, surface_priv->scaled_surface_obj[HEVC_SCALED_SURF_4X_ID], 1,
                                    VA_FOURCC('N', 'V', '1', '2'), SUBSAMPLE_YUV420);

        if (generic_state->b16xme_supported) {
            i965_CreateSurfaces(ctx,
                                priv_state->frame_width_16x,
                                priv_state->frame_height_16x,
                                VA_RT_FORMAT_YUV420,
                                1,
                                &surface_priv->scaled_surface_id[HEVC_SCALED_SURF_16X_ID]);
            surface_priv->scaled_surface_obj[HEVC_SCALED_SURF_16X_ID] =
                SURFACE(surface_priv->scaled_surface_id[HEVC_SCALED_SURF_16X_ID]);

            if (!surface_priv->scaled_surface_obj[HEVC_SCALED_SURF_16X_ID])
                return VA_STATUS_ERROR_ALLOCATION_FAILED;

            i965_check_alloc_surface_bo(ctx, surface_priv->scaled_surface_obj[HEVC_SCALED_SURF_16X_ID], 1,
                                        VA_FOURCC('N', 'V', '1', '2'), SUBSAMPLE_YUV420);
        }

        if (generic_state->b32xme_supported) {
            i965_CreateSurfaces(ctx,
                                priv_state->frame_width_32x,
                                priv_state->frame_height_32x,
                                VA_RT_FORMAT_YUV420,
                                1,
                                &surface_priv->scaled_surface_id[HEVC_SCALED_SURF_32X_ID]);
            surface_priv->scaled_surface_obj[HEVC_SCALED_SURF_32X_ID] =
                SURFACE(surface_priv->scaled_surface_id[HEVC_SCALED_SURF_32X_ID]);

            if (!surface_priv->scaled_surface_obj[HEVC_SCALED_SURF_32X_ID])
                return VA_STATUS_ERROR_ALLOCATION_FAILED;

            i965_check_alloc_surface_bo(ctx, surface_priv->scaled_surface_obj[HEVC_SCALED_SURF_32X_ID], 1,
                                        VA_FOURCC('N', 'V', '1', '2'), SUBSAMPLE_YUV420);
        }

        if (obj_surface->fourcc == VA_FOURCC_P010) {
            i965_CreateSurfaces(ctx,
                                priv_state->frame_width_in_max_lcu,
                                priv_state->frame_height_in_max_lcu,
                                VA_RT_FORMAT_YUV420,
                                1,
                                &surface_priv->surface_id_nv12);
            surface_priv->surface_obj_nv12 = SURFACE(surface_priv->surface_id_nv12);

            if (!surface_priv->surface_obj_nv12)
                return VA_STATUS_ERROR_ALLOCATION_FAILED;

            i965_check_alloc_surface_bo(ctx, surface_priv->surface_obj_nv12, 1,
                                        VA_FOURCC('N', 'V', '1', '2'), SUBSAMPLE_YUV420);
        }
    }

    return VA_STATUS_SUCCESS;
}

static void
gen9_hevc_enc_free_resources(struct encoder_vme_mfc_context *vme_context)
{
    struct gen9_hevc_encoder_context *priv_ctx = NULL;

    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;

    i965_free_gpe_resource(&priv_ctx->res_brc_pic_states_write_buffer);
    i965_free_gpe_resource(&priv_ctx->res_brc_history_buffer);
    i965_free_gpe_resource(&priv_ctx->res_brc_intra_dist_buffer);
    i965_free_gpe_resource(&priv_ctx->res_brc_pak_statistic_buffer);
    i965_free_gpe_resource(&priv_ctx->res_brc_input_buffer_for_enc_kernels);
    i965_free_gpe_resource(&priv_ctx->res_brc_constant_data_buffer);

    i965_free_gpe_resource(&priv_ctx->res_mb_code_surface);

    // free VME buffers
    i965_free_gpe_resource(&priv_ctx->res_flatness_check_surface);
    i965_free_gpe_resource(&priv_ctx->res_brc_me_dist_buffer);
    i965_free_gpe_resource(&priv_ctx->s4x_memv_distortion_buffer);
    i965_free_gpe_resource(&priv_ctx->s4x_memv_data_buffer);
    i965_free_gpe_resource(&priv_ctx->s16x_memv_data_buffer);
    i965_free_gpe_resource(&priv_ctx->s32x_memv_data_buffer);
    i965_free_gpe_resource(&priv_ctx->res_32x32_pu_output_buffer);
    i965_free_gpe_resource(&priv_ctx->res_simplest_intra_buffer);
    i965_free_gpe_resource(&priv_ctx->res_kernel_debug);
    i965_free_gpe_resource(&priv_ctx->res_sad_16x16_pu_buffer);
    i965_free_gpe_resource(&priv_ctx->res_vme_8x8_mode_buffer);
    i965_free_gpe_resource(&priv_ctx->res_intra_mode_buffer);
    i965_free_gpe_resource(&priv_ctx->res_intra_distortion_buffer);
    i965_free_gpe_resource(&priv_ctx->res_vme_uni_sic_buffer);
    i965_free_gpe_resource(&priv_ctx->res_con_corrent_thread_buffer);
    i965_free_gpe_resource(&priv_ctx->res_mv_index_buffer);
    i965_free_gpe_resource(&priv_ctx->res_mvp_index_buffer);
    i965_free_gpe_resource(&priv_ctx->res_roi_buffer);
    i965_free_gpe_resource(&priv_ctx->res_mb_statistics_buffer);

    if (priv_ctx->scaled_2x_surface_obj) {
        i965_DestroySurfaces(priv_ctx->ctx, &priv_ctx->scaled_2x_surface_id, 1);
        priv_ctx->scaled_2x_surface_obj = NULL;
    }

    // free PAK buffers
    i965_free_gpe_resource(&priv_ctx->deblocking_filter_line_buffer);
    i965_free_gpe_resource(&priv_ctx->deblocking_filter_tile_line_buffer);
    i965_free_gpe_resource(&priv_ctx->deblocking_filter_tile_column_buffer);
    i965_free_gpe_resource(&priv_ctx->metadata_line_buffer);
    i965_free_gpe_resource(&priv_ctx->metadata_tile_line_buffer);
    i965_free_gpe_resource(&priv_ctx->metadata_tile_column_buffer);
    i965_free_gpe_resource(&priv_ctx->sao_line_buffer);
    i965_free_gpe_resource(&priv_ctx->sao_tile_line_buffer);
    i965_free_gpe_resource(&priv_ctx->sao_tile_column_buffer);

    priv_ctx->res_inited = 0;
}

#define ALLOC_GPE_RESOURCE(RES, NAME, SIZE)                 \
    do{                                                     \
        i965_free_gpe_resource(&priv_ctx->RES);             \
        if (!i965_allocate_gpe_resource(i965->intel.bufmgr, \
                                 &priv_ctx->RES,            \
                                 SIZE,                      \
                                 NAME))                     \
            goto FAIL;                                      \
    } while(0);

#define ALLOC_GPE_2D_RESOURCE(RES, NAME, W, H, P)               \
    do{                                                         \
        i965_free_gpe_resource(&priv_ctx->RES);                 \
        if (!i965_gpe_allocate_2d_resource(i965->intel.bufmgr,  \
                                 &priv_ctx->RES,                \
                                 (ALIGN(W, 64)), H,             \
                                 (ALIGN(P, 64)),                \
                                 NAME))                         \
            goto FAIL;                                          \
    } while(0);

static VAStatus
gen9_hevc_enc_alloc_resources(VADriverContextP ctx,
                              struct encode_state *encode_state,
                              struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct generic_enc_codec_state *generic_state = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    int res_size = 0, size_shift = 0;
    int width = 0, height = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    generic_state = (struct generic_enc_codec_state *)vme_context->generic_enc_state;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    if (priv_ctx->res_inited)
        return VA_STATUS_SUCCESS;

    res_size = priv_state->mb_code_size;
    ALLOC_GPE_RESOURCE(res_mb_code_surface,
                       "Mb code surface", res_size);

    res_size = priv_state->pic_state_size * generic_state->num_pak_passes;
    ALLOC_GPE_RESOURCE(res_brc_pic_states_write_buffer,
                       "Brc pic status write buffer",
                       res_size);

    res_size = priv_state->pic_state_size * generic_state->num_pak_passes;
    ALLOC_GPE_RESOURCE(res_brc_pic_states_read_buffer,
                       "Brc pic status read buffer",
                       res_size);

    res_size = GEN9_HEVC_ENC_BRC_HISTORY_BUFFER_SIZE;
    ALLOC_GPE_RESOURCE(res_brc_history_buffer,
                       "Brc history buffer",
                       res_size);

    res_size = GEN9_HEVC_ENC_BRC_PAK_STATISTCS_SIZE;
    ALLOC_GPE_RESOURCE(res_brc_pak_statistic_buffer,
                       "Brc pak statistic buffer",
                       res_size);

    res_size = 1024;
    ALLOC_GPE_RESOURCE(res_brc_input_buffer_for_enc_kernels,
                       "Brc input buffer for enc kernels buffer",
                       res_size);

    width = ALIGN(priv_state->downscaled_width_4x_in_mb * 8, 64);
    height = ALIGN(priv_state->downscaled_height_4x_in_mb * 4, 8) * 2;
    ALLOC_GPE_2D_RESOURCE(res_brc_intra_dist_buffer,
                          "Brc intra distortion buffer",
                          width, height, width);

    width = ALIGN(GEN9_HEVC_ENC_BRC_CONSTANT_SURFACE_WIDTH, 64);
    height = GEN9_HEVC_ENC_BRC_CONSTANT_SURFACE_HEIGHT;
    ALLOC_GPE_2D_RESOURCE(res_brc_constant_data_buffer,
                          "Brc constant data buffer",
                          width, height, width);

    width = ALIGN((priv_state->frame_width_4x * 4 + 31) >> 4, 64);
    height = ALIGN((priv_state->frame_height_4x * 4 + 31) >> 5, 4);
    ALLOC_GPE_2D_RESOURCE(res_brc_mb_qp_buffer,
                          "Brc mb qp buffer",
                          width, height, width);

    //HME scaling buffer allocation
    width = ALIGN(priv_state->downscaled_width_4x_in_mb * 8, 64);
    height = ALIGN(priv_state->downscaled_height_4x_in_mb * 4, 8);
    ALLOC_GPE_2D_RESOURCE(res_brc_me_dist_buffer,
                          "Brc me distortion buffer",
                          width, height, width);

    if (generic_state->hme_supported) {
        width = ALIGN(priv_state->downscaled_width_4x_in_mb * 8, 64);
        height = ALIGN(priv_state->downscaled_height_4x_in_mb * 4 * 10, 8) * 2;
        ALLOC_GPE_2D_RESOURCE(s4x_memv_distortion_buffer,
                              "4x MEMV distortion buffer",
                              width, height, width);

        width = ALIGN(priv_state->downscaled_width_4x_in_mb * 32, 64);
        height = priv_state->downscaled_height_4x_in_mb * 4 * 10;
        ALLOC_GPE_2D_RESOURCE(s4x_memv_data_buffer,
                              "4x MEMV data buffer",
                              width, height, width);

        if (generic_state->b16xme_supported) {
            width = ALIGN(priv_state->downscaled_width_16x_in_mb * 32, 64);
            height = priv_state->downscaled_height_16x_in_mb * 2 * 4 * 10;
            ALLOC_GPE_2D_RESOURCE(s16x_memv_data_buffer,
                                  "16x MEMV data buffer",
                                  width, height, width);

            if (generic_state->b32xme_supported) {
                width = ALIGN(priv_state->downscaled_width_32x_in_mb * 32, 64);
                height = priv_state->downscaled_height_32x_in_mb * 2 * 4 * 10;
                ALLOC_GPE_2D_RESOURCE(s32x_memv_data_buffer,
                                      "32x MEMV data buffer",
                                      width, height, width);
            }
        }
    }

    if (priv_state->flatness_check_supported) {
        width = ALIGN(priv_state->width_in_mb * 4, 64);
        height = priv_state->downscaled_height_4x_in_mb * 4;
        ALLOC_GPE_2D_RESOURCE(res_flatness_check_surface,
                              "Flatness check buffer",
                              width, height, width);
    }

    if (priv_ctx->scaled_2x_surface_obj)
        i965_DestroySurfaces(priv_ctx->ctx, &priv_ctx->scaled_2x_surface_id, 1);

    width = priv_state->frame_width_in_max_lcu >> 1;
    height = priv_state->frame_height_in_max_lcu >> 1;
    if (priv_state->bit_depth_luma_minus8) {
        width = ALIGN(width, 32);
        height = ALIGN(height, 32);
    }

    i965_CreateSurfaces(ctx,
                        width,
                        height,
                        VA_RT_FORMAT_YUV420,
                        1,
                        &priv_ctx->scaled_2x_surface_id);
    priv_ctx->scaled_2x_surface_obj =
        SURFACE(priv_ctx->scaled_2x_surface_id);

    if (!priv_ctx->scaled_2x_surface_obj)
        goto FAIL;

    i965_check_alloc_surface_bo(ctx, priv_ctx->scaled_2x_surface_obj, 1,
                                VA_FOURCC('N', 'V', '1', '2'), SUBSAMPLE_YUV420);

    res_size = (priv_state->frame_width_in_max_lcu >> 5) *
               (priv_state->frame_height_in_max_lcu >> 5) *
               32;
    ALLOC_GPE_RESOURCE(res_32x32_pu_output_buffer,
                       "32x32 pu output buffer",
                       res_size);

    width = priv_state->frame_width_in_max_lcu >> 3;
    height = priv_state->frame_height_in_max_lcu >> 5;
    ALLOC_GPE_2D_RESOURCE(res_slice_map_buffer,
                          "Slice map buffer",
                          width, height, width);

    res_size = 8192 * 1024;
    ALLOC_GPE_RESOURCE(res_kernel_debug,
                       "kernel debug",
                       res_size);

    width = priv_state->frame_width_in_max_lcu >> 3;
    height = priv_state->frame_height_in_max_lcu >> 5;
    ALLOC_GPE_2D_RESOURCE(res_simplest_intra_buffer,
                          "Simplest intra buffer",
                          width, height, width);

    res_size = (priv_state->frame_width_in_max_lcu >> 4) *
               (priv_state->frame_height_in_max_lcu >> 4) * 8 * 4;
    ALLOC_GPE_RESOURCE(res_sad_16x16_pu_buffer,
                       "Sad 16x16 pu",
                       res_size);

    res_size = (priv_state->frame_width_in_max_lcu >> 4) *
               (priv_state->frame_height_in_max_lcu >> 4) * 32;
    ALLOC_GPE_RESOURCE(res_vme_8x8_mode_buffer,
                       "Vme 8x8 mode",
                       res_size);

    res_size = (priv_state->frame_width_in_max_lcu >> 3) *
               (priv_state->frame_height_in_max_lcu >> 3) * 32;
    ALLOC_GPE_RESOURCE(res_intra_mode_buffer,
                       "Intra mode",
                       res_size);

    res_size = (priv_state->frame_width_in_max_lcu >> 4) *
               (priv_state->frame_height_in_max_lcu >> 4) * 16;
    ALLOC_GPE_RESOURCE(res_intra_distortion_buffer,
                       "Intra distortion",
                       res_size);

    width = priv_state->frame_width_in_max_lcu >> 1;
    height = priv_state->frame_height_in_max_lcu >> 4;
    ALLOC_GPE_2D_RESOURCE(res_min_distortion_buffer,
                          "Min distortion buffer",
                          width, height, width);

    res_size = priv_state->frame_width_in_max_lcu *
               priv_state->frame_height_in_max_lcu;
    ALLOC_GPE_RESOURCE(res_vme_uni_sic_buffer,
                       "Vme uni sic buffer",
                       res_size);

    width = sizeof(gen9_hevc_mbenc_control_region);
    height = GEN9_HEVC_ENC_CONCURRENT_SURFACE_HEIGHT;
    ALLOC_GPE_2D_RESOURCE(res_con_corrent_thread_buffer,
                          "Con corrent thread buffer",
                          width, height, width);

    res_size = priv_state->frame_width_in_max_lcu *
               priv_state->frame_height_in_max_lcu / 4;
    ALLOC_GPE_RESOURCE(res_mv_index_buffer,
                       "Mv index buffer",
                       res_size);

    res_size = priv_state->frame_width_in_max_lcu *
               priv_state->frame_height_in_max_lcu / 2;
    ALLOC_GPE_RESOURCE(res_mvp_index_buffer,
                       "Mvp index buffer",
                       res_size);

    width = ALIGN(priv_state->width_in_mb * 4, 64);
    height = ALIGN(priv_state->height_in_mb, 8);
    ALLOC_GPE_2D_RESOURCE(res_roi_buffer,
                          "ROI buffer",
                          width, height, width);

    res_size = priv_state->width_in_mb *
               priv_state->height_in_mb * 52;
    ALLOC_GPE_RESOURCE(res_mb_statistics_buffer,
                       "MB statistics buffer",
                       res_size);

    // PAK pipe buffer allocation
    size_shift = (priv_state->bit_depth_luma_minus8 ||
                  priv_state->bit_depth_chroma_minus8) ? 2 : 3;

    res_size = ALIGN(priv_state->picture_width, 32) >> size_shift;
    ALLOC_GPE_RESOURCE(deblocking_filter_line_buffer,
                       "Deblocking filter line buffer",
                       res_size << 6);
    ALLOC_GPE_RESOURCE(deblocking_filter_tile_line_buffer,
                       "Deblocking filter tile line buffer",
                       res_size << 6);

    res_size = ALIGN(priv_state->picture_height +
                     priv_state->height_in_lcu * 6, 32) >> size_shift;
    ALLOC_GPE_RESOURCE(deblocking_filter_tile_column_buffer,
                       "Deblocking filter tile column buffer",
                       res_size << 6);

    res_size = (((priv_state->picture_width + 15) >> 4) * 188 +
                priv_state->width_in_lcu * 9 + 1023) >> 9;
    ALLOC_GPE_RESOURCE(metadata_line_buffer,
                       "metadata line buffer",
                       res_size << 6);

    res_size = (((priv_state->picture_width + 15) >> 4) * 172 +
                priv_state->width_in_lcu * 9 + 1023) >> 9;
    ALLOC_GPE_RESOURCE(metadata_tile_line_buffer,
                       "metadata tile line buffer",
                       res_size << 6);

    res_size = (((priv_state->picture_height + 15) >> 4) * 176 +
                priv_state->height_in_lcu * 89 + 1023) >> 9;
    ALLOC_GPE_RESOURCE(metadata_tile_column_buffer,
                       "metadata tile column buffer",
                       res_size << 6);

    res_size = ALIGN(((priv_state->picture_width >> 1) +
                      priv_state->width_in_lcu * 3), 16) >> size_shift;
    ALLOC_GPE_RESOURCE(sao_line_buffer,
                       "sao line buffer",
                       res_size << 6);

    res_size = ALIGN(((priv_state->picture_width >> 1) +
                      priv_state->width_in_lcu * 6), 16) >> size_shift;
    ALLOC_GPE_RESOURCE(sao_tile_line_buffer,
                       "sao tile line buffer",
                       res_size << 6);

    res_size = ALIGN(((priv_state->picture_height >> 1) +
                      priv_state->height_in_lcu * 6), 16) >> size_shift;
    ALLOC_GPE_RESOURCE(sao_tile_column_buffer,
                       "sao tile column buffer",
                       res_size << 6);

    priv_ctx->res_inited = 1;
    return VA_STATUS_SUCCESS;

FAIL:
    gen9_hevc_enc_free_resources(vme_context);
    return VA_STATUS_ERROR_ALLOCATION_FAILED;
}

#define VME_IMPLEMENTATION_START

static void
gen9_hevc_set_gpe_1d_surface(VADriverContextP ctx,
                             struct gen9_hevc_encoder_context *priv_ctx,
                             struct i965_gpe_context *gpe_context,
                             enum GEN9_HEVC_ENC_SURFACE_TYPE surface_type,
                             int bti_idx,
                             int is_raw_buffer,
                             int size,
                             unsigned int offset,
                             struct i965_gpe_resource *gpe_buffer,
                             dri_bo *bo)
{
    if (!gpe_buffer && !bo) {
        gpe_buffer = priv_ctx->gpe_surfaces[surface_type].gpe_resource;
        bo = priv_ctx->gpe_surfaces[surface_type].bo;
    }

    if (gpe_buffer)
        gen9_add_buffer_gpe_surface(ctx, gpe_context,
                                    gpe_buffer, is_raw_buffer,
                                    size == 0 ? gpe_buffer->size - offset : size,
                                    offset, bti_idx);
    else if (bo)
        gen9_add_dri_buffer_gpe_surface(ctx, gpe_context,
                                        bo, is_raw_buffer,
                                        size == 0 ? bo->size - offset : size,
                                        offset, bti_idx);
}

static void
gen9_hevc_set_gpe_2d_surface(VADriverContextP ctx,
                             struct gen9_hevc_encoder_context *priv_ctx,
                             struct i965_gpe_context *gpe_context,
                             enum GEN9_HEVC_ENC_SURFACE_TYPE surface_type,
                             int bti_idx,
                             int has_uv_surface,
                             int is_media_block_rw,
                             unsigned int format,
                             struct i965_gpe_resource *gpe_buffer,
                             struct object_surface *surface_object)
{
    if (!gpe_buffer && !surface_object) {
        gpe_buffer = priv_ctx->gpe_surfaces[surface_type].gpe_resource;
        surface_object = priv_ctx->gpe_surfaces[surface_type].surface_object;
    }

    if (gpe_buffer) {
        gen9_add_buffer_2d_gpe_surface(ctx,
                                       gpe_context,
                                       gpe_buffer,
                                       is_media_block_rw,
                                       format,
                                       bti_idx);
    } else if (surface_object) {
        gen9_add_2d_gpe_surface(ctx, gpe_context,
                                surface_object,
                                0, is_media_block_rw, format,
                                bti_idx);

        if (has_uv_surface)
            gen9_add_2d_gpe_surface(ctx, gpe_context,
                                    surface_object,
                                    1, is_media_block_rw, format,
                                    bti_idx + 1);
    }
}

static void
gen9_hevc_set_gpe_adv_surface(VADriverContextP ctx,
                              struct gen9_hevc_encoder_context *priv_ctx,
                              struct i965_gpe_context *gpe_context,
                              enum GEN9_HEVC_ENC_SURFACE_TYPE surface_type,
                              int bti_idx,
                              struct object_surface *surface_object)
{
    if (!surface_object)
        surface_object = priv_ctx->gpe_surfaces[surface_type].surface_object;

    if (surface_object)
        gen9_add_adv_gpe_surface(ctx, gpe_context,
                                 surface_object, bti_idx);
}

static void
gen9_hevc_add_gpe_surface(struct gen9_hevc_encoder_context *priv_ctx,
                          enum GEN9_HEVC_ENC_SURFACE_TYPE surface_type,
                          struct i965_gpe_resource *gpe_buffer,
                          struct object_surface *surface_object)
{
    if (gpe_buffer && gpe_buffer->bo)
        priv_ctx->gpe_surfaces[surface_type].gpe_resource = gpe_buffer;
    else if (surface_object)
        priv_ctx->gpe_surfaces[surface_type].surface_object = surface_object;
}

static void
gen9_hevc_init_gpe_surfaces_table(VADriverContextP ctx,
                                  struct encode_state *encode_state,
                                  struct intel_encoder_context *encoder_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_surface_priv *surface_priv = NULL;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;

    if (encode_state->reconstructed_object->fourcc == VA_FOURCC_P010) {
        surface_priv = (struct gen9_hevc_surface_priv *)encode_state->reconstructed_object->private_data;

        gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_RAW_Y, NULL,
                                  surface_priv->surface_obj_nv12);
        gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_RAW_Y_UV, NULL,
                                  surface_priv->surface_obj_nv12);
        gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_RAW_VME, NULL,
                                  surface_priv->surface_obj_nv12);
    } else {
        gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_RAW_Y, NULL,
                                  encode_state->input_yuv_object);
        gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_RAW_Y_UV, NULL,
                                  encode_state->input_yuv_object);
        gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_RAW_VME, NULL,
                                  encode_state->input_yuv_object);
    }

    if (priv_ctx->scaled_2x_surface_obj) {
        gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_Y_2X, NULL,
                                  priv_ctx->scaled_2x_surface_obj);
        gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_Y_2X_VME, NULL,
                                  priv_ctx->scaled_2x_surface_obj);
    }

    gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_BRC_HISTORY,
                              &priv_ctx->res_brc_history_buffer,
                              NULL);
    gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_BRC_PAST_PAK_INFO,
                              &priv_ctx->res_brc_pak_statistic_buffer,
                              NULL);
    gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_HCP_PAK,
                              &priv_ctx->res_mb_code_surface,
                              NULL);
    gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_CU_RECORD,
                              &priv_ctx->res_mb_code_surface,
                              NULL);
    gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_32x32_PU_OUTPUT,
                              &priv_ctx->res_32x32_pu_output_buffer,
                              NULL);
    gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_SLICE_MAP,
                              &priv_ctx->res_slice_map_buffer,
                              NULL);
    gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_BRC_INPUT,
                              &priv_ctx->res_brc_input_buffer_for_enc_kernels,
                              NULL);
    gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_LCU_QP,
                              &priv_ctx->res_brc_mb_qp_buffer,
                              NULL);
    gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_ROI,
                              &priv_ctx->res_roi_buffer,
                              NULL);
    gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_RAW_MBSTAT,
                              &priv_ctx->res_mb_statistics_buffer,
                              NULL);
    gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_BRC_DATA,
                              &priv_ctx->res_brc_constant_data_buffer,
                              NULL);
    gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_KERNEL_DEBUG,
                              &priv_ctx->res_kernel_debug,
                              NULL);
    gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_SIMPLIFIED_INTRA,
                              &priv_ctx->res_simplest_intra_buffer,
                              NULL);
    gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_HME_MVP,
                              &priv_ctx->s4x_memv_data_buffer,
                              NULL);
    gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_HME_DIST,
                              &priv_ctx->s4x_memv_distortion_buffer,
                              NULL);
    gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_16x16PU_SAD,
                              &priv_ctx->res_sad_16x16_pu_buffer,
                              NULL);
    gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_VME_8x8,
                              &priv_ctx->res_vme_8x8_mode_buffer,
                              NULL);
    gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_INTRA_MODE,
                              &priv_ctx->res_intra_mode_buffer,
                              NULL);
    gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_INTRA_DIST,
                              &priv_ctx->res_intra_distortion_buffer,
                              NULL);
    gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_MIN_DIST,
                              &priv_ctx->res_min_distortion_buffer,
                              NULL);
    gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_VME_UNI_SIC_DATA,
                              &priv_ctx->res_vme_uni_sic_buffer,
                              NULL);
    gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_CONCURRENT_THREAD,
                              &priv_ctx->res_con_corrent_thread_buffer,
                              NULL);
    gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_MB_MV_INDEX,
                              &priv_ctx->res_mv_index_buffer,
                              NULL);
    gen9_hevc_add_gpe_surface(priv_ctx, HEVC_ENC_SURFACE_MVP_INDEX,
                              &priv_ctx->res_mvp_index_buffer,
                              NULL);
}

static VAStatus
gen9_hevc_enc_check_parameters(VADriverContextP ctx,
                               struct encode_state *encode_state,
                               struct intel_encoder_context *encoder_context)
{
    VAEncSequenceParameterBufferHEVC *seq_param = NULL;
    VAEncPictureParameterBufferHEVC *pic_param = NULL;
    VAEncSliceParameterBufferHEVC *slice_param = NULL;
    int i = 0, j = 0;

    seq_param = (VAEncSequenceParameterBufferHEVC *)encode_state->seq_param_ext->buffer;
    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;

    for (i = 0; i < encode_state->num_slice_params_ext; i++) {
        slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[i]->buffer;

        if (slice_param->slice_fields.bits.slice_temporal_mvp_enabled_flag &&
            slice_param->slice_fields.bits.collocated_from_l0_flag &&
            (pic_param->collocated_ref_pic_index == 0xff ||
             pic_param->collocated_ref_pic_index > GEN9_MAX_REF_SURFACES))
            slice_param->slice_fields.bits.slice_temporal_mvp_enabled_flag = 0;

        if (slice_param->num_ref_idx_l0_active_minus1 > GEN9_HEVC_NUM_MAX_REF_L0 - 1 ||
            slice_param->num_ref_idx_l1_active_minus1 > GEN9_HEVC_NUM_MAX_REF_L1 - 1)
            return VA_STATUS_ERROR_ATTR_NOT_SUPPORTED;
    }

    i = 1 << (seq_param->log2_diff_max_min_luma_coding_block_size +
              seq_param->log2_min_luma_coding_block_size_minus3 + 3);
    if (i < GEN9_HEVC_ENC_MIN_LCU_SIZE ||
        i > GEN9_HEVC_ENC_MAX_LCU_SIZE)
        return VA_STATUS_ERROR_ATTR_NOT_SUPPORTED;

    //The TU max size in SPS must be the same as the CU max size in SPS
    i = seq_param->log2_min_transform_block_size_minus2 +
        seq_param->log2_diff_max_min_transform_block_size + 2;
    j = seq_param->log2_min_luma_coding_block_size_minus3 +
        seq_param->log2_diff_max_min_luma_coding_block_size + 3;

    if (i != j)
        return VA_STATUS_ERROR_ATTR_NOT_SUPPORTED;

    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[0]->buffer;
    i = pic_param->pic_init_qp + slice_param->slice_qp_delta;
    j = -seq_param->seq_fields.bits.bit_depth_luma_minus8 * 6;
    if (i < j || i > 51)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    return VA_STATUS_SUCCESS;
}

static void
gen9_hevc_enc_init_seq_parameters(struct gen9_hevc_encoder_context *priv_ctx,
                                  struct generic_enc_codec_state *generic_state,
                                  struct gen9_hevc_encoder_state *priv_state,
                                  VAEncSequenceParameterBufferHEVC *seq_param)
{
    int new = 0, m = 0, n = 0;

    if (priv_state->picture_width != seq_param->pic_width_in_luma_samples ||
        priv_state->picture_height != seq_param->pic_height_in_luma_samples ||
        priv_state->bit_depth_luma_minus8 != seq_param->seq_fields.bits.bit_depth_luma_minus8 ||
        priv_state->bit_depth_chroma_minus8 != seq_param->seq_fields.bits.bit_depth_chroma_minus8)
        new = 1;

    if (!new)
        return;

    priv_state->bit_depth_luma_minus8 = seq_param->seq_fields.bits.bit_depth_luma_minus8;
    priv_state->bit_depth_chroma_minus8 = seq_param->seq_fields.bits.bit_depth_chroma_minus8;
    priv_state->cu_size = 1 << (seq_param->log2_min_luma_coding_block_size_minus3 + 3);
    priv_state->lcu_size = 1 << (seq_param->log2_diff_max_min_luma_coding_block_size +
                                 seq_param->log2_min_luma_coding_block_size_minus3 + 3);
    priv_state->picture_width = (seq_param->pic_width_in_luma_samples / priv_state->cu_size) * priv_state->cu_size;
    priv_state->picture_height = (seq_param->pic_height_in_luma_samples / priv_state->cu_size) * priv_state->cu_size;
    priv_state->width_in_lcu  = ALIGN(priv_state->picture_width, priv_state->lcu_size) / priv_state->lcu_size;
    priv_state->height_in_lcu = ALIGN(priv_state->picture_height, priv_state->lcu_size) / priv_state->lcu_size;
    priv_state->width_in_cu  = ALIGN(priv_state->picture_width, priv_state->cu_size) / priv_state->cu_size;
    priv_state->height_in_cu = ALIGN(priv_state->picture_height, priv_state->cu_size) / priv_state->cu_size;
    priv_state->width_in_mb  = ALIGN(priv_state->picture_width, 16) / 16;
    priv_state->height_in_mb = ALIGN(priv_state->picture_height, 16) / 16;

    m = (priv_state->picture_width + GEN9_HEVC_ENC_MIN_LCU_SIZE - 1) / GEN9_HEVC_ENC_MIN_LCU_SIZE;
    n = (priv_state->picture_height + GEN9_HEVC_ENC_MIN_LCU_SIZE - 1) / GEN9_HEVC_ENC_MIN_LCU_SIZE;
    priv_state->mb_data_offset = ALIGN(m * n * priv_state->pak_obj_size, 0x1000);

    m = ALIGN(priv_state->picture_width, GEN9_HEVC_ENC_MAX_LCU_SIZE) / 8;
    n = ALIGN(priv_state->picture_height, GEN9_HEVC_ENC_MAX_LCU_SIZE) / 8;
    priv_state->mb_code_size = priv_state->mb_data_offset +
                               ALIGN(m * n * priv_state->cu_record_size, 0x1000);

    priv_state->frame_width_in_max_lcu = ALIGN(priv_state->picture_width, 32);
    priv_state->frame_height_in_max_lcu = ALIGN(priv_state->picture_height, 32);
    priv_state->frame_width_4x = ALIGN(priv_state->picture_width / 4, 16);
    priv_state->frame_height_4x = ALIGN(priv_state->picture_height / 4, 16);
    priv_state->frame_width_16x = ALIGN(priv_state->picture_width / 16, 16);
    priv_state->frame_height_16x = ALIGN(priv_state->picture_height / 16, 16);
    priv_state->frame_width_32x = ALIGN(priv_state->picture_width / 32, 16);
    priv_state->frame_height_32x = ALIGN(priv_state->picture_height / 32, 16);

    priv_state->downscaled_width_4x_in_mb = priv_state->frame_width_4x / 16;
    if (priv_state->bit_depth_luma_minus8) {
        priv_state->downscaled_width_4x_in_mb = ALIGN(priv_state->downscaled_width_4x_in_mb * 16, 32) /
                                                16;
        priv_state->frame_width_4x = priv_state->downscaled_width_4x_in_mb * 16;
    }

    priv_state->downscaled_height_4x_in_mb = priv_state->frame_height_4x / 16;
    priv_state->downscaled_width_16x_in_mb = priv_state->frame_width_16x / 16;
    priv_state->downscaled_height_16x_in_mb = priv_state->frame_height_16x / 16;
    priv_state->downscaled_width_32x_in_mb = priv_state->frame_width_32x / 16;
    priv_state->downscaled_height_32x_in_mb = priv_state->frame_height_32x / 16;
    priv_state->flatness_check_enable = priv_state->flatness_check_supported;
    priv_state->widi_first_intra_refresh = 1;

    generic_state->hme_supported = GEN9_HEVC_HME_SUPPORTED;
    generic_state->b16xme_supported = GEN9_HEVC_16XME_SUPPORTED;
    generic_state->b32xme_supported = GEN9_HEVC_32XME_SUPPORTED;
    if (generic_state->hme_supported &&
        (priv_state->frame_width_4x < GEN9_HEVC_VME_MIN_ALLOWED_SIZE ||
         priv_state->frame_height_4x < GEN9_HEVC_VME_MIN_ALLOWED_SIZE)) {
        generic_state->b16xme_supported = 0;
        generic_state->b32xme_supported = 0;

        if (priv_state->frame_width_4x < GEN9_HEVC_VME_MIN_ALLOWED_SIZE) {
            priv_state->frame_width_4x = GEN9_HEVC_VME_MIN_ALLOWED_SIZE;
            priv_state->downscaled_width_4x_in_mb = priv_state->frame_width_4x / 16;
        }

        if (priv_state->frame_height_4x < GEN9_HEVC_VME_MIN_ALLOWED_SIZE) {
            priv_state->frame_height_4x = GEN9_HEVC_VME_MIN_ALLOWED_SIZE;
            priv_state->downscaled_height_4x_in_mb = priv_state->frame_height_4x / 16;
        }
    } else if (generic_state->b16xme_supported &&
               (priv_state->frame_width_16x < GEN9_HEVC_VME_MIN_ALLOWED_SIZE ||
                priv_state->frame_height_16x < GEN9_HEVC_VME_MIN_ALLOWED_SIZE)) {
        generic_state->b32xme_supported = 0;

        if (priv_state->frame_width_16x < GEN9_HEVC_VME_MIN_ALLOWED_SIZE) {
            priv_state->frame_width_16x = GEN9_HEVC_VME_MIN_ALLOWED_SIZE;
            priv_state->downscaled_width_16x_in_mb = priv_state->frame_width_16x / 16;
        }

        if (priv_state->frame_height_16x < GEN9_HEVC_VME_MIN_ALLOWED_SIZE) {
            priv_state->frame_height_16x = GEN9_HEVC_VME_MIN_ALLOWED_SIZE;
            priv_state->downscaled_height_16x_in_mb = priv_state->frame_height_16x / 16;
        }
    } else if (generic_state->b32xme_supported &&
               (priv_state->frame_width_32x < GEN9_HEVC_VME_MIN_ALLOWED_SIZE ||
                priv_state->frame_height_32x < GEN9_HEVC_VME_MIN_ALLOWED_SIZE)) {
        if (priv_state->frame_width_32x < GEN9_HEVC_VME_MIN_ALLOWED_SIZE) {
            priv_state->frame_width_32x = GEN9_HEVC_VME_MIN_ALLOWED_SIZE;
            priv_state->downscaled_width_32x_in_mb = priv_state->frame_width_32x / 16;
        }

        if (priv_state->frame_height_32x < GEN9_HEVC_VME_MIN_ALLOWED_SIZE) {
            priv_state->frame_height_32x = GEN9_HEVC_VME_MIN_ALLOWED_SIZE;
            priv_state->downscaled_height_32x_in_mb = priv_state->frame_height_32x / 16;
        }
    }

    priv_state->gop_ref_dist = seq_param->ip_period;
    priv_state->gop_size = seq_param->intra_period;
    priv_state->frame_number = 0;

    priv_ctx->res_inited = 0;
}

static void
gen9_hevc_enc_init_pic_parameters(struct generic_enc_codec_state *generic_state,
                                  struct gen9_hevc_encoder_state *priv_state,
                                  VAEncSequenceParameterBufferHEVC *seq_param,
                                  VAEncPictureParameterBufferHEVC *pic_param,
                                  VAEncSliceParameterBufferHEVC *slice_param)
{
    unsigned int log2_max_coding_block_size = 0, raw_ctu_bits = 0;

    priv_state->picture_coding_type = slice_param->slice_type;

    priv_state->ctu_max_bitsize_allowed = pic_param->ctu_max_bitsize_allowed;
    log2_max_coding_block_size  = seq_param->log2_min_luma_coding_block_size_minus3 + 3 +
                                  seq_param->log2_diff_max_min_luma_coding_block_size;
    raw_ctu_bits = (1 << (2 * log2_max_coding_block_size + 3)) +
                   (1 << (2 * log2_max_coding_block_size + 2));
    raw_ctu_bits = (5 * raw_ctu_bits / 3);

    if (priv_state->ctu_max_bitsize_allowed == 0 ||
        priv_state->ctu_max_bitsize_allowed > raw_ctu_bits)
        priv_state->ctu_max_bitsize_allowed = raw_ctu_bits;
}

static void
gen9_hevc_enc_init_slice_parameters(VADriverContextP ctx,
                                    struct encode_state *encode_state,
                                    struct intel_encoder_context *encoder_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    VAEncPictureParameterBufferHEVC *pic_param = NULL;
    VAEncSliceParameterBufferHEVC *slice_param = NULL;
    int i = 0, j = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;
    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;

    priv_state->low_delay = 1;
    priv_state->arbitrary_num_mb_in_slice = 0;

    for (i = 0; i < encode_state->num_slice_params_ext; i++) {
        slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[i]->buffer;

        if (slice_param->slice_type == HEVC_SLICE_B && priv_state->low_delay) {
            for (j = 0; j <= slice_param->num_ref_idx_l0_active_minus1; j++) {
                if (pic_param->decoded_curr_pic.pic_order_cnt <
                    slice_param->ref_pic_list0[j].pic_order_cnt)
                    priv_state->low_delay = 0;
            }

            for (j = 0; j <= slice_param->num_ref_idx_l1_active_minus1; j++) {
                if (pic_param->decoded_curr_pic.pic_order_cnt <
                    slice_param->ref_pic_list1[j].pic_order_cnt)
                    priv_state->low_delay = 0;
            }
        }

        if (!priv_state->arbitrary_num_mb_in_slice &&
            (slice_param->num_ctu_in_slice % priv_state->width_in_lcu))
            priv_state->arbitrary_num_mb_in_slice = 1;
    }
}

static VAStatus
gen9_hevc_enc_init_parameters(VADriverContextP ctx,
                              struct encode_state *encode_state,
                              struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct generic_enc_codec_state *generic_state = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    VAEncPictureParameterBufferHEVC *pic_param = NULL;
    VAEncSequenceParameterBufferHEVC *seq_param = NULL;
    VAEncSliceParameterBufferHEVC *slice_param = NULL;
    struct object_buffer *obj_buffer = NULL;
    VAStatus va_status = VA_STATUS_SUCCESS;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    generic_state = (struct generic_enc_codec_state *)vme_context->generic_enc_state;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;
    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;
    seq_param = (VAEncSequenceParameterBufferHEVC *)encode_state->seq_param_ext->buffer;
    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[0]->buffer;

    if (!pic_param || !seq_param || !slice_param)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    va_status = gen9_hevc_enc_check_parameters(ctx, encode_state, encoder_context);
    if (va_status |= VA_STATUS_SUCCESS)
        return va_status;

    gen9_hevc_enc_init_seq_parameters(priv_ctx, generic_state, priv_state, seq_param);
    gen9_hevc_enc_init_pic_parameters(generic_state, priv_state, seq_param, pic_param, slice_param);
    gen9_hevc_enc_init_slice_parameters(ctx, encode_state, encoder_context);

    if (priv_state->picture_coding_type == HEVC_SLICE_I) {
        generic_state->hme_enabled = 0;
        generic_state->b16xme_enabled = 0;
        generic_state->b32xme_enabled = 0;
    } else {
        generic_state->hme_enabled = generic_state->hme_supported;
        generic_state->b16xme_enabled = generic_state->b16xme_supported;
        generic_state->b32xme_enabled = generic_state->b32xme_supported;
    }

    obj_buffer = BUFFER(pic_param->coded_buf);
    if (!obj_buffer ||
        !obj_buffer->buffer_store ||
        !obj_buffer->buffer_store->bo) {
        va_status = VA_STATUS_ERROR_INVALID_PARAMETER;
        goto EXIT;
    }
    encode_state->coded_buf_object = obj_buffer;
    priv_state->status_buffer.bo = obj_buffer->buffer_store->bo;

    va_status = gen9_hevc_ensure_surface(ctx, priv_state,
                                         encode_state->input_yuv_object, 0);
    if (va_status != VA_STATUS_SUCCESS)
        goto EXIT;

    va_status = gen9_hevc_ensure_surface(ctx, priv_state,
                                         encode_state->reconstructed_object, 1);
    if (va_status != VA_STATUS_SUCCESS)
        goto EXIT;

#if 0
    if (encode_state->input_yuv_object->orig_width > priv_state->picture_width)
        encode_state->input_yuv_object->orig_width = priv_state->picture_width;

    if (encode_state->input_yuv_object->orig_height > priv_state->picture_height)
        encode_state->input_yuv_object->orig_height = priv_state->picture_height;


    if (encode_state->reconstructed_object->orig_width > priv_state->picture_width)
        encode_state->reconstructed_object->orig_width = priv_state->picture_width;

    if (encode_state->reconstructed_object->orig_height > priv_state->picture_height)
        encode_state->reconstructed_object->orig_height = priv_state->picture_height;
#endif

    va_status = gen9_hevc_init_surface_private(ctx, generic_state, priv_state,
                                               encode_state->reconstructed_object);
    if (va_status != VA_STATUS_SUCCESS)
        goto EXIT;

    {
        struct gen9_hevc_surface_priv *surface_priv = NULL;

        surface_priv = (struct gen9_hevc_surface_priv *)encode_state->reconstructed_object->private_data;

        surface_priv->qp_value = pic_param->pic_init_qp + slice_param->slice_qp_delta;
    }

    va_status = gen9_hevc_enc_alloc_resources(ctx, encode_state,
                                              encoder_context);
    if (va_status != VA_STATUS_SUCCESS)
        goto EXIT;

    gen9_hevc_init_gpe_surfaces_table(ctx, encode_state,
                                      encoder_context);

EXIT:
    return va_status;
}

// VME&BRC implementation

static void
gen9_hevc_vme_init_gpe_context(VADriverContextP ctx,
                               struct i965_gpe_context *gpe_context,
                               unsigned int curbe_size,
                               unsigned int inline_data_size)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);

    gpe_context->curbe.length = curbe_size;

    gpe_context->sampler.entry_size = 0;
    gpe_context->sampler.max_entries = 0;

    gpe_context->idrt.entry_size = ALIGN(sizeof(struct gen8_interface_descriptor_data), 64);
    gpe_context->idrt.max_entries = 1;

    gpe_context->surface_state_binding_table.max_entries = MAX_HEVC_KERNELS_ENCODER_SURFACES;
    gpe_context->surface_state_binding_table.binding_table_offset = 0;
    gpe_context->surface_state_binding_table.surface_state_offset = ALIGN(MAX_HEVC_KERNELS_ENCODER_SURFACES * 4, 64);
    gpe_context->surface_state_binding_table.length = ALIGN(MAX_HEVC_KERNELS_ENCODER_SURFACES * 4, 64) + ALIGN(MAX_HEVC_KERNELS_ENCODER_SURFACES * SURFACE_STATE_PADDED_SIZE_GEN9, 64);

    if (i965->intel.eu_total > 0)
        gpe_context->vfe_state.max_num_threads = 6 * i965->intel.eu_total;
    else
        gpe_context->vfe_state.max_num_threads = 112;

    gpe_context->vfe_state.curbe_allocation_size = MAX(1, ALIGN(gpe_context->curbe.length, 32) >> 5);
    gpe_context->vfe_state.urb_entry_size = MAX(1, ALIGN(inline_data_size, 32) >> 5);
    gpe_context->vfe_state.num_urb_entries = (MAX_HEVC_KERNELS_URB_SIZE -
                                              gpe_context->vfe_state.curbe_allocation_size -
                                              ((gpe_context->idrt.entry_size >> 5) *
                                               gpe_context->idrt.max_entries)) /
                                             gpe_context->vfe_state.urb_entry_size;
    gpe_context->vfe_state.num_urb_entries = CLAMP(gpe_context->vfe_state.num_urb_entries, 1, 64);
    gpe_context->vfe_state.gpgpu_mode = 0;
}

static void
gen9_hevc_vme_init_scoreboard(struct i965_gpe_context *gpe_context,
                              unsigned int mask,
                              unsigned int enable,
                              unsigned int type)
{
    gpe_context->vfe_desc5.scoreboard0.mask = mask;
    gpe_context->vfe_desc5.scoreboard0.type = type;
    gpe_context->vfe_desc5.scoreboard0.enable = enable;

    gpe_context->vfe_desc6.scoreboard1.delta_x0 = 0xF;
    gpe_context->vfe_desc6.scoreboard1.delta_y0 = 0x0;

    gpe_context->vfe_desc6.scoreboard1.delta_x1 = 0x0;
    gpe_context->vfe_desc6.scoreboard1.delta_y1 = 0xF;

    gpe_context->vfe_desc6.scoreboard1.delta_x2 = 0x1;
    gpe_context->vfe_desc6.scoreboard1.delta_y2 = 0xF;

    gpe_context->vfe_desc6.scoreboard1.delta_x3 = 0xF;
    gpe_context->vfe_desc6.scoreboard1.delta_y3 = 0xF;

    gpe_context->vfe_desc7.scoreboard2.delta_x4 = 0xF;
    gpe_context->vfe_desc7.scoreboard2.delta_y4 = 0x1;

    gpe_context->vfe_desc7.scoreboard2.delta_x5 = 0x0;
    gpe_context->vfe_desc7.scoreboard2.delta_y5 = 0xE;

    gpe_context->vfe_desc7.scoreboard2.delta_x6 = 0x1;
    gpe_context->vfe_desc7.scoreboard2.delta_y6 = 0xE;

    gpe_context->vfe_desc7.scoreboard2.delta_x7 = 0xF;
    gpe_context->vfe_desc7.scoreboard2.delta_y7 = 0xE;
}

static void
gen9_hevc_vme_set_scoreboard_26z(struct i965_gpe_context *gpe_context,
                                 unsigned int mask,
                                 unsigned int enable,
                                 unsigned int type)
{
    gpe_context->vfe_desc5.scoreboard0.mask = mask;
    gpe_context->vfe_desc5.scoreboard0.type = type;
    gpe_context->vfe_desc5.scoreboard0.enable = enable;

    gpe_context->vfe_desc6.scoreboard1.delta_x0 = -1;
    gpe_context->vfe_desc6.scoreboard1.delta_y0 = 3;

    gpe_context->vfe_desc6.scoreboard1.delta_x1 = -1;
    gpe_context->vfe_desc6.scoreboard1.delta_y1 = 1;

    gpe_context->vfe_desc6.scoreboard1.delta_x2 = -1;
    gpe_context->vfe_desc6.scoreboard1.delta_y2 = -1;

    gpe_context->vfe_desc6.scoreboard1.delta_x3 = 0;
    gpe_context->vfe_desc6.scoreboard1.delta_y3 = -1;

    gpe_context->vfe_desc7.scoreboard2.delta_x4 = 0;
    gpe_context->vfe_desc7.scoreboard2.delta_y4 = -2;

    gpe_context->vfe_desc7.scoreboard2.delta_x5 = 0;
    gpe_context->vfe_desc7.scoreboard2.delta_y5 = -3;

    gpe_context->vfe_desc7.scoreboard2.delta_x6 = 1;
    gpe_context->vfe_desc7.scoreboard2.delta_y6 = -2;

    gpe_context->vfe_desc7.scoreboard2.delta_x7 = 1;
    gpe_context->vfe_desc7.scoreboard2.delta_y7 = -3;
}

static void
gen9_hevc_vme_set_scoreboard_26(struct i965_gpe_context *gpe_context,
                                unsigned int mask,
                                unsigned int enable,
                                unsigned int type)
{
    gpe_context->vfe_desc5.scoreboard0.mask = mask;
    gpe_context->vfe_desc5.scoreboard0.type = type;
    gpe_context->vfe_desc5.scoreboard0.enable = enable;

    gpe_context->vfe_desc6.scoreboard1.delta_x0 = -1;
    gpe_context->vfe_desc6.scoreboard1.delta_y0 = 0;

    gpe_context->vfe_desc6.scoreboard1.delta_x1 = -1;
    gpe_context->vfe_desc6.scoreboard1.delta_y1 = -1;

    gpe_context->vfe_desc6.scoreboard1.delta_x2 = 0;
    gpe_context->vfe_desc6.scoreboard1.delta_y2 = -1;

    gpe_context->vfe_desc6.scoreboard1.delta_x3 = 1;
    gpe_context->vfe_desc6.scoreboard1.delta_y3 = -1;
}

static void
gen9_hevc_init_object_walker(struct hevc_enc_kernel_walker_parameter *hevc_walker_param,
                             struct gpe_media_object_walker_parameter *gpe_param)
{
    memset(gpe_param, 0, sizeof(*gpe_param));

    gpe_param->use_scoreboard = hevc_walker_param->use_scoreboard;
    gpe_param->block_resolution.x = hevc_walker_param->resolution_x;
    gpe_param->block_resolution.y = hevc_walker_param->resolution_y;
    gpe_param->global_resolution.x = hevc_walker_param->resolution_x;
    gpe_param->global_resolution.y = hevc_walker_param->resolution_y;
    gpe_param->global_outer_loop_stride.x = hevc_walker_param->resolution_x;
    gpe_param->global_outer_loop_stride.y = 0;
    gpe_param->global_inner_loop_unit.x = 0;
    gpe_param->global_inner_loop_unit.y = hevc_walker_param->resolution_y;
    gpe_param->local_loop_exec_count = 0xFFFF;
    gpe_param->global_loop_exec_count = 0xFFFF;

    if (hevc_walker_param->no_dependency) {
        gpe_param->scoreboard_mask = 0;
        gpe_param->use_scoreboard = 0;
        gpe_param->local_outer_loop_stride.x = 0;
        gpe_param->local_outer_loop_stride.y = 1;
        gpe_param->local_inner_loop_unit.x = 1;
        gpe_param->local_inner_loop_unit.y = 0;
        gpe_param->local_end.x = hevc_walker_param->resolution_x - 1;
        gpe_param->local_end.y = 0;
    }
}

static void
gen9_hevc_init_object_walker_26z(struct gen9_hevc_encoder_state *priv_state,
                                 struct i965_gpe_context *gpe_context,
                                 struct gpe_media_object_walker_parameter *gpe_param,
                                 int split_count,
                                 unsigned int max_slight_height,
                                 int use_hw_scoreboard,
                                 int scoreboard_type)
{
    int width = priv_state->width_in_mb;
    int height = max_slight_height * 2;
    int ts_width = ((width + 3) & 0xfffc) >> 1;
    int lcu_width = (width + 1) >> 1;
    int lcu_height = (height + 1) >> 1;
    int tmp1 = ((lcu_width + 1) >> 1) + ((lcu_width + ((lcu_height - 1) << 1)) + (2 * split_count - 1)) /
               (2 * split_count);

    gpe_param->use_scoreboard  = use_hw_scoreboard;
    gpe_param->scoreboard_mask = 0xFF;
    gpe_param->global_resolution.x = ts_width;
    gpe_param->global_resolution.y = 4 * tmp1;
    gpe_param->global_start.x = 0;
    gpe_param->global_start.y = 0;
    gpe_param->global_outer_loop_stride.x = ts_width;
    gpe_param->global_outer_loop_stride.y = 0;
    gpe_param->global_inner_loop_unit.x = 0;
    gpe_param->global_inner_loop_unit.y = 4 * tmp1;
    gpe_param->block_resolution.x = ts_width;
    gpe_param->block_resolution.y = 4 * tmp1;
    gpe_param->local_start.x = ts_width;
    gpe_param->local_start.y = 0;
    gpe_param->local_end.x = 0;
    gpe_param->local_end.y = 0;
    gpe_param->local_outer_loop_stride.x = 1;
    gpe_param->local_outer_loop_stride.y = 0;
    gpe_param->local_inner_loop_unit.x = -2;
    gpe_param->local_inner_loop_unit.y = 4;
    gpe_param->middle_loop_extra_steps = 3;
    gpe_param->mid_loop_unit_x = 0;
    gpe_param->mid_loop_unit_y = 1;
    gpe_param->global_loop_exec_count = 0;
    gpe_param->local_loop_exec_count = ((lcu_width + (lcu_height - 1) * 2 + 2 * split_count - 1) /
                                        (2 * split_count)) * 2 -
                                       1;

    gen9_hevc_vme_set_scoreboard_26z(gpe_context, 0xff, use_hw_scoreboard, scoreboard_type);
}

static void
gen9_hevc_init_object_walker_26(struct gen9_hevc_encoder_state *priv_state,
                                struct i965_gpe_context *gpe_context,
                                struct gpe_media_object_walker_parameter *gpe_param,
                                int split_count,
                                unsigned int max_slight_height,
                                int use_hw_scoreboard,
                                int scoreboard_type)
{
    int width = priv_state->width_in_mb;
    int height = max_slight_height;
    int ts_width = (width + 1) & 0xfffe;
    int ts_height = (height + 1) & 0xfffe;
    int tmp1 = ((ts_width + 1) >> 1) +
               ((ts_width + ((ts_height - 1) << 1)) +
                (2 * split_count - 1)) / (2 * split_count);

    gpe_param->use_scoreboard = use_hw_scoreboard;
    gpe_param->scoreboard_mask = 0x0f;
    gpe_param->global_resolution.x = ts_width;
    gpe_param->global_resolution.y = tmp1;
    gpe_param->global_start.x = 0;
    gpe_param->global_start.y = 0;
    gpe_param->global_outer_loop_stride.x = ts_width;
    gpe_param->global_outer_loop_stride.y = 0;
    gpe_param->global_inner_loop_unit.x = 0;
    gpe_param->global_inner_loop_unit.y = tmp1;
    gpe_param->block_resolution.x = ts_width;
    gpe_param->block_resolution.y = tmp1;
    gpe_param->local_start.x = ts_width;
    gpe_param->local_start.y = 0;
    gpe_param->local_end.x = 0;
    gpe_param->local_end.y = 0;
    gpe_param->local_outer_loop_stride.x = 1;
    gpe_param->local_outer_loop_stride.y = 0;
    gpe_param->local_inner_loop_unit.x = -2;
    gpe_param->local_inner_loop_unit.y = 1;
    gpe_param->middle_loop_extra_steps = 0;
    gpe_param->mid_loop_unit_x = 0;
    gpe_param->mid_loop_unit_y = 0;
    gpe_param->global_loop_exec_count = 0;
    gpe_param->local_loop_exec_count = (width + (height - 1) * 2 + split_count - 1) /
                                       split_count;

    gen9_hevc_vme_set_scoreboard_26(gpe_context, 0xff, use_hw_scoreboard, scoreboard_type);
}

static void
gen9_hevc_run_object_walker(VADriverContextP ctx,
                            struct intel_encoder_context *encoder_context,
                            struct i965_gpe_context *gpe_context,
                            struct gpe_media_object_walker_parameter *param,
                            int media_state)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;

    intel_batchbuffer_start_atomic(batch, 0x1000);

    intel_batchbuffer_emit_mi_flush(batch);

    gen9_gpe_pipeline_setup(ctx, gpe_context, batch);
    gen8_gpe_media_object_walker(ctx, gpe_context, batch, param);
    gen8_gpe_media_state_flush(ctx, gpe_context, batch);
    gen9_gpe_pipeline_end(ctx, gpe_context, batch);

    intel_batchbuffer_end_atomic(batch);

    intel_batchbuffer_flush(batch);
}

static void
gen9_hevc_run_object(VADriverContextP ctx,
                     struct intel_encoder_context *encoder_context,
                     struct i965_gpe_context *gpe_context,
                     struct gpe_media_object_parameter *param,
                     int media_state)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;

    intel_batchbuffer_start_atomic(batch, 0x1000);

    intel_batchbuffer_emit_mi_flush(batch);

    gen9_gpe_pipeline_setup(ctx, gpe_context, batch);
    gen8_gpe_media_object(ctx, gpe_context, batch, param);
    gen8_gpe_media_state_flush(ctx, gpe_context, batch);

    gen9_gpe_pipeline_end(ctx, gpe_context, batch);

    intel_batchbuffer_end_atomic(batch);

    intel_batchbuffer_flush(batch);
}

static void
gen9_hevc_get_b_mbenc_default_curbe(enum HEVC_TU_MODE tu_mode,
                                    int slice_type,
                                    void **curbe_ptr,
                                    int *curbe_size)
{
    if (tu_mode == HEVC_TU_BEST_SPEED) {
        if (slice_type == HEVC_SLICE_I) {
            *curbe_size = sizeof(HEVC_ENC_ENCB_TU7_I_CURBE_DATA);
            *curbe_ptr = (void *)HEVC_ENC_ENCB_TU7_I_CURBE_DATA;
        } else if (slice_type == HEVC_SLICE_P) {
            *curbe_size = sizeof(HEVC_ENC_ENCB_TU7_P_CURBE_DATA);
            *curbe_ptr = (void *)HEVC_ENC_ENCB_TU7_P_CURBE_DATA;
        } else {
            *curbe_size = sizeof(HEVC_ENC_ENCB_TU7_B_CURBE_DATA);
            *curbe_ptr = (void *)HEVC_ENC_ENCB_TU7_B_CURBE_DATA;
        }
    } else if (tu_mode == HEVC_TU_RT_SPEED) {
        if (slice_type == HEVC_SLICE_P) {
            *curbe_size = sizeof(HEVC_ENC_ENCB_TU4_P_CURBE_DATA);
            *curbe_ptr = (void *)HEVC_ENC_ENCB_TU4_P_CURBE_DATA;
        } else {
            *curbe_size = sizeof(HEVC_ENC_ENCB_TU4_B_CURBE_DATA);
            *curbe_ptr = (void *)HEVC_ENC_ENCB_TU4_B_CURBE_DATA;
        }
    } else {
        if (slice_type == HEVC_SLICE_P) {
            *curbe_size = sizeof(HEVC_ENC_ENCB_TU1_P_CURBE_DATA);
            *curbe_ptr = (void *)HEVC_ENC_ENCB_TU1_P_CURBE_DATA;
        } else {
            *curbe_size = sizeof(HEVC_ENC_ENCB_TU1_B_CURBE_DATA);
            *curbe_ptr = (void *)HEVC_ENC_ENCB_TU1_B_CURBE_DATA;
        }
    }
}

// BRC start

static void
gen9_hevc_configure_roi(struct encode_state *encode_state,
                        struct intel_encoder_context *encoder_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    int i = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    priv_state->num_roi = MIN(encoder_context->brc.num_roi, I965_MAX_NUM_ROI_REGIONS);
    priv_state->roi_value_is_qp_delta = encoder_context->brc.roi_value_is_qp_delta;

    for (i = 0; i < priv_state->num_roi; i++) {
        priv_state->roi[i].left = encoder_context->brc.roi[i].left >> 4;
        priv_state->roi[i].right = encoder_context->brc.roi[i].right >> 4;
        priv_state->roi[i].top = encoder_context->brc.roi[i].top >> 4;
        priv_state->roi[i].bottom = encoder_context->brc.roi[i].bottom >> 4;
        priv_state->roi[i].value = encoder_context->brc.roi[i].value;
    }
}

static void
gen9_hevc_brc_prepare(struct encode_state *encode_state,
                      struct intel_encoder_context *encoder_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct generic_enc_codec_state *generic_state = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    enum HEVC_BRC_METHOD brc_method = HEVC_BRC_CQP;
    int internal_tu_mode = encoder_context->quality_level;
    int brc_reset = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    generic_state = (struct generic_enc_codec_state *)vme_context->generic_enc_state;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    if (encoder_context->rate_control_mode & VA_RC_CBR)
        brc_method = HEVC_BRC_CBR;
    else if (encoder_context->rate_control_mode & VA_RC_VBR)
        brc_method = HEVC_BRC_VBR;
    else if (encoder_context->rate_control_mode & VA_RC_VCM)
        brc_method = HEVC_BRC_VCM;

    if (internal_tu_mode >= HEVC_TU_RT_SPEED ||
        internal_tu_mode == 0)
        internal_tu_mode = internal_tu_mode >= HEVC_TU_BEST_SPEED ?
                           HEVC_TU_BEST_SPEED : HEVC_TU_RT_SPEED;
    else
        internal_tu_mode = HEVC_TU_BEST_QUALITY;

    brc_reset = priv_state->brc_method != brc_method ||
                priv_state->tu_mode != internal_tu_mode;

    if (!generic_state->brc_inited ||
        encoder_context->brc.need_reset ||
        brc_reset) {
        priv_state->tu_mode = internal_tu_mode;
        if (priv_state->tu_mode == HEVC_TU_BEST_QUALITY)
            priv_state->num_regions_in_slice = 1;
        else
            priv_state->num_regions_in_slice = 4;

        if (internal_tu_mode == HEVC_TU_BEST_SPEED)
            priv_state->walking_pattern_26 = 1;
        else
            priv_state->walking_pattern_26 = 0;

        if (brc_method == HEVC_BRC_CQP) {
            generic_state->brc_enabled = 0;
            generic_state->num_pak_passes = 1;
            priv_state->lcu_brc_enabled = 0;
        } else {
            generic_state->brc_enabled = 1;
            generic_state->num_pak_passes = 4;

            if (brc_method == HEVC_BRC_VCM ||
                encoder_context->brc.mb_rate_control[0] == 0)
                priv_state->lcu_brc_enabled = (priv_state->tu_mode == HEVC_TU_BEST_QUALITY);
            else if (brc_method == HEVC_BRC_ICQ ||
                     encoder_context->brc.mb_rate_control[0] == 1)
                priv_state->lcu_brc_enabled = 1;
            else
                priv_state->lcu_brc_enabled = 0;

            if (brc_method == HEVC_BRC_CBR) {
                priv_state->target_bit_rate_in_kbs =
                    ALIGN(encoder_context->brc.bits_per_second[0], HEVC_BRC_KBPS) /
                    HEVC_BRC_KBPS;
                priv_state->max_bit_rate_in_kbs = priv_state->target_bit_rate_in_kbs;
                priv_state->min_bit_rate_in_kbs = priv_state->target_bit_rate_in_kbs;
            } else {
                if (encoder_context->brc.target_percentage[0] > HEVC_BRC_MIN_TARGET_PERCENTAGE) {
                    priv_state->target_bit_rate_in_kbs =
                        ALIGN(encoder_context->brc.bits_per_second[0], HEVC_BRC_KBPS) /
                        HEVC_BRC_KBPS;
                    priv_state->max_bit_rate_in_kbs = priv_state->target_bit_rate_in_kbs;
                    priv_state->min_bit_rate_in_kbs = priv_state->target_bit_rate_in_kbs *
                                                      (2 * encoder_context->brc.target_percentage[0] - 100) /
                                                      100;
                    priv_state->target_bit_rate_in_kbs = priv_state->max_bit_rate_in_kbs *
                                                         encoder_context->brc.target_percentage[0] / 100;

                    brc_reset = 1;
                }
            }

            if (encoder_context->brc.framerate[0].den)
                priv_state->frames_per_100s = encoder_context->brc.framerate[0].num * 100 /
                                              encoder_context->brc.framerate[0].den;

            priv_state->init_vbv_buffer_fullness_in_bit =
                encoder_context->brc.hrd_initial_buffer_fullness;
            priv_state->vbv_buffer_size_in_bit =
                encoder_context->brc.hrd_buffer_size;
        }

        priv_state->brc_method = brc_method;
        generic_state->brc_need_reset = brc_reset;
        encoder_context->brc.need_reset = 0;
    }

    gen9_hevc_configure_roi(encode_state, encoder_context);
}

static void
gen9_hevc_brc_init_rest_set_curbe(VADriverContextP ctx,
                                  struct encode_state *encode_state,
                                  struct intel_encoder_context *encoder_context,
                                  struct i965_gpe_context *gpe_context,
                                  int reset)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    gen9_hevc_brc_initreset_curbe_data *cmd = NULL;
    VAEncSequenceParameterBufferHEVC *seq_param = NULL;
    double input_bits_per_frame = 0;
    double bps_ratio = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;
    seq_param = (VAEncSequenceParameterBufferHEVC *)encode_state->seq_param_ext->buffer;

    cmd = i965_gpe_context_map_curbe(gpe_context);
    if (!cmd)
        return;

    memcpy((void *)cmd, GEN9_HEVC_BRCINIT_CURBE_DATA, sizeof(GEN9_HEVC_BRCINIT_CURBE_DATA));

    cmd->dw0.profile_level_max_frame = gen9_hevc_get_profile_level_max_frame(seq_param,
                                                                             priv_state->user_max_frame_size,
                                                                             priv_state->frames_per_100s);
    cmd->dw1.init_buf_full = priv_state->init_vbv_buffer_fullness_in_bit;
    cmd->dw2.buf_size = priv_state->vbv_buffer_size_in_bit;
    cmd->dw3.targe_bit_rate = priv_state->target_bit_rate_in_kbs * HEVC_BRC_KBPS;
    cmd->dw4.maximum_bit_rate = priv_state->max_bit_rate_in_kbs * HEVC_BRC_KBPS;

    cmd->dw9.frame_width = priv_state->picture_width;
    cmd->dw10.frame_height = priv_state->picture_height;
    cmd->dw12.number_slice = encode_state->num_slice_params_ext;
    cmd->dw6.frame_rate_m = priv_state->frames_per_100s;
    cmd->dw7.frame_rate_d = 100;
    cmd->dw8.brc_flag = 0;
    cmd->dw8.brc_flag |= (priv_state->lcu_brc_enabled) ? 0 : HEVC_BRCINIT_DISABLE_MBBRC;
    cmd->dw25.acqp_buffer = 1;

    if (priv_state->brc_method == HEVC_BRC_CBR) {
        cmd->dw4.maximum_bit_rate = cmd->dw3.targe_bit_rate;
        cmd->dw8.brc_flag |= HEVC_BRCINIT_ISCBR;
    } else if (priv_state->brc_method == HEVC_BRC_VBR) {
        if (cmd->dw4.maximum_bit_rate < cmd->dw3.targe_bit_rate)
            cmd->dw4.maximum_bit_rate = cmd->dw3.targe_bit_rate * 2;
        cmd->dw8.brc_flag |= HEVC_BRCINIT_ISVBR;
    } else if (priv_state->brc_method == HEVC_BRC_AVBR) {
        cmd->dw4.maximum_bit_rate = priv_state->target_bit_rate_in_kbs * HEVC_BRC_KBPS;
        cmd->dw8.brc_flag |= HEVC_BRCINIT_ISAVBR;
    } else if (priv_state->brc_method == HEVC_BRC_ICQ) {
        cmd->dw25.acqp_buffer = priv_state->crf_quality_factor;
        cmd->dw8.brc_flag |= HEVC_BRCINIT_ISICQ;
    } else if (priv_state->brc_method == HEVC_BRC_VCM) {
        cmd->dw4.maximum_bit_rate = priv_state->target_bit_rate_in_kbs * HEVC_BRC_KBPS;
        cmd->dw8.brc_flag |= HEVC_BRCINIT_ISVCM;
    }

    if (priv_state->num_b_in_gop[1] ||
        priv_state->num_b_in_gop[2]) {
        cmd->dw8.brc_param_a = priv_state->gop_size / priv_state->gop_ref_dist;
        cmd->dw9.brc_param_b = cmd->dw8.brc_param_a;
        cmd->dw13.brc_param_c = cmd->dw8.brc_param_a * 2;
        cmd->dw14.brc_param_d = priv_state->gop_size - cmd->dw8.brc_param_a -
                                cmd->dw9.brc_param_b - cmd->dw13.brc_param_c;

        if (!priv_state->num_b_in_gop[2])
            cmd->dw14.max_brc_level = 3;
        else
            cmd->dw14.max_brc_level = 4;
    } else {
        cmd->dw14.max_brc_level = 1;
        cmd->dw8.brc_param_a = priv_state->gop_ref_dist ? (priv_state->gop_size - 1) / priv_state->gop_ref_dist : 0;
        cmd->dw9.brc_param_b = priv_state->gop_size - 1 - cmd->dw8.brc_param_a;
    }

    cmd->dw10.avbr_accuracy = GEN9_HEVC_AVBR_ACCURACY;
    cmd->dw11.avbr_convergence = GEN9_HEVC_AVBR_CONVERGENCE;

    input_bits_per_frame = (double)(cmd->dw4.maximum_bit_rate) *
                           (double)(cmd->dw7.frame_rate_d) /
                           (double)(cmd->dw6.frame_rate_m);
    if (cmd->dw2.buf_size < (unsigned int)input_bits_per_frame * 4)
        cmd->dw2.buf_size = (unsigned int)input_bits_per_frame * 4;

    if (cmd->dw1.init_buf_full == 0)
        cmd->dw1.init_buf_full = cmd->dw2.buf_size * 7 / 8;

    if (cmd->dw1.init_buf_full < (unsigned int)(input_bits_per_frame * 2))
        cmd->dw1.init_buf_full = (unsigned int)(input_bits_per_frame * 2);

    if (cmd->dw1.init_buf_full > cmd->dw2.buf_size)
        cmd->dw1.init_buf_full = cmd->dw2.buf_size;

    if (priv_state->brc_method == HEVC_BRC_AVBR) {
        cmd->dw2.buf_size = priv_state->target_bit_rate_in_kbs * 2 * HEVC_BRC_KBPS;
        cmd->dw1.init_buf_full = (unsigned int)(cmd->dw2.buf_size * 3 / 4);
    }

    bps_ratio = input_bits_per_frame / (cmd->dw2.buf_size / 30);
    bps_ratio = (bps_ratio < 0.1) ? 0.1 : (bps_ratio > 3.5) ? 3.5 : bps_ratio;

    cmd->dw19.deviation_threshold0_pbframe = (unsigned int)(-50 * pow(0.90, bps_ratio));
    cmd->dw19.deviation_threshold1_pbframe = (unsigned int)(-50 * pow(0.66, bps_ratio));
    cmd->dw19.deviation_threshold2_pbframe = (unsigned int)(-50 * pow(0.46, bps_ratio));
    cmd->dw19.deviation_threshold3_pbframe = (unsigned int)(-50 * pow(0.3, bps_ratio));

    cmd->dw20.deviation_threshold4_pbframe = (unsigned int)(50 *  pow(0.3, bps_ratio));
    cmd->dw20.deviation_threshold5_pbframe = (unsigned int)(50 * pow(0.46, bps_ratio));
    cmd->dw20.deviation_threshold6_pbframe = (unsigned int)(50 * pow(0.7,  bps_ratio));
    cmd->dw20.deviation_threshold7_pbframe = (unsigned int)(50 * pow(0.9,  bps_ratio));

    cmd->dw21.deviation_threshold0_vbr_control = (unsigned int)(-50 * pow(0.9, bps_ratio));
    cmd->dw21.deviation_threshold1_vbr_control = (unsigned int)(-50 * pow(0.7, bps_ratio));
    cmd->dw21.deviation_threshold2_vbr_control = (unsigned int)(-50 * pow(0.5, bps_ratio));
    cmd->dw21.deviation_threshold3_vbr_control = (unsigned int)(-50 * pow(0.3, bps_ratio));

    cmd->dw22.deviation_threshold4_vbr_control = (unsigned int)(100 * pow(0.4, bps_ratio));
    cmd->dw22.deviation_threshold5_vbr_control = (unsigned int)(100 * pow(0.5, bps_ratio));
    cmd->dw22.deviation_threshold6_vbr_control = (unsigned int)(100 * pow(0.75, bps_ratio));
    cmd->dw22.deviation_threshold7_vbr_control = (unsigned int)(100 * pow(0.9, bps_ratio));

    cmd->dw23.deviation_threshold0_iframe = (unsigned int)(-50 * pow(0.8, bps_ratio));
    cmd->dw23.deviation_threshold1_iframe = (unsigned int)(-50 * pow(0.6, bps_ratio));
    cmd->dw23.deviation_threshold2_iframe = (unsigned int)(-50 * pow(0.34, bps_ratio));
    cmd->dw23.deviation_threshold3_iframe = (unsigned int)(-50 * pow(0.2, bps_ratio));

    cmd->dw24.deviation_threshold4_iframe = (unsigned int)(50 * pow(0.2,  bps_ratio));
    cmd->dw24.deviation_threshold5_iframe = (unsigned int)(50 * pow(0.4,  bps_ratio));
    cmd->dw24.deviation_threshold6_iframe = (unsigned int)(50 * pow(0.66, bps_ratio));
    cmd->dw24.deviation_threshold7_iframe = (unsigned int)(50 * pow(0.9,  bps_ratio));

    if (!reset)
        priv_state->brc_init_current_target_buf_full_in_bits = cmd->dw1.init_buf_full;

    priv_state->brc_init_reset_buf_size_in_bits = (double)cmd->dw2.buf_size;
    priv_state->brc_init_reset_input_bits_per_frame = input_bits_per_frame;

    i965_gpe_context_unmap_curbe(gpe_context);
}

static void
gen9_hevc_brc_init_rest_set_surfaces(VADriverContextP ctx,
                                     struct intel_encoder_context *encoder_context,
                                     struct i965_gpe_context *gpe_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    int bti_idx = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_BRC_HISTORY, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_BRC_ME_DIST, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 &priv_ctx->res_brc_me_dist_buffer, NULL);
}

static void
gen9_hevc_brc_init_reset(VADriverContextP ctx,
                         struct encode_state *encode_state,
                         struct intel_encoder_context *encoder_context,
                         int reset)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;

    struct i965_gpe_context *gpe_context = NULL;
    struct gpe_media_object_parameter param;
    int media_state = HEVC_ENC_MEDIA_STATE_BRC_INIT_RESET;
    int gpe_idx = reset ? HEVC_BRC_RESET_IDX : HEVC_BRC_INIT_IDX;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    gpe_context = &priv_ctx->brc_context.gpe_contexts[gpe_idx];

    gen8_gpe_context_init(ctx, gpe_context);
    gen9_gpe_reset_binding_table(ctx, gpe_context);
    gen9_hevc_brc_init_rest_set_curbe(ctx, encode_state, encoder_context, gpe_context,
                                      reset);
    gen9_hevc_brc_init_rest_set_surfaces(ctx, encoder_context, gpe_context);
    gen8_gpe_setup_interface_data(ctx, gpe_context);

    memset(&param, 0, sizeof(param));
    gen9_hevc_run_object(ctx, encoder_context, gpe_context, &param,
                         media_state);
}

static void
gen9_hevc_brc_intra_dist_set_curbe(VADriverContextP ctx,
                                   struct encode_state *encode_state,
                                   struct intel_encoder_context *encoder_context,
                                   struct i965_gpe_context *gpe_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    gen9_hevc_brc_coarse_intra_curbe_data *cmd = NULL;
    int bti_idx = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    cmd = i965_gpe_context_map_curbe(gpe_context);
    if (!cmd)
        return;

    memset((void *)cmd, 0, sizeof(*cmd));

    cmd->dw0.picture_width_in_luma_samples = priv_state->frame_width_4x;
    cmd->dw0.picture_height_in_luma_samples = priv_state->frame_height_4x;

    cmd->dw1.inter_sad = 2;
    cmd->dw1.intra_sad = 2;

    cmd->dw8.bti_src_y4 = bti_idx++;
    cmd->dw9.bti_intra_dist = bti_idx++;
    cmd->dw10.bti_vme_intra = bti_idx++;

    i965_gpe_context_unmap_curbe(gpe_context);
}

static void
gen9_hevc_brc_intra_dist_set_surfaces(VADriverContextP ctx,
                                      struct encode_state *encode_state,
                                      struct intel_encoder_context *encoder_context,
                                      struct i965_gpe_context *gpe_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct object_surface *obj_surface;
    struct gen9_hevc_surface_priv *surface_priv;
    int bti_idx = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;

    obj_surface = encode_state->reconstructed_object;
    surface_priv = (struct gen9_hevc_surface_priv *)(obj_surface->private_data);
    obj_surface = surface_priv->scaled_surface_obj[HEVC_SCALED_SURF_4X_ID];

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_Y_4X, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, obj_surface);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_BRC_ME_DIST, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 &priv_ctx->res_brc_intra_dist_buffer, NULL);

    gen9_hevc_set_gpe_adv_surface(ctx, priv_ctx, gpe_context,
                                  HEVC_ENC_SURFACE_Y_4X_VME, bti_idx++,
                                  obj_surface);
}

static void
gen9_hevc_brc_intra_dist(VADriverContextP ctx,
                         struct encode_state *encode_state,
                         struct intel_encoder_context *encoder_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    struct i965_gpe_context *gpe_context = NULL;
    struct gpe_media_object_walker_parameter param;
    struct hevc_enc_kernel_walker_parameter hevc_walker_param;
    int media_state = HEVC_ENC_MEDIA_STATE_ENC_I_FRAME_DIST;
    int gpe_idx = HEVC_BRC_COARSE_INTRA_IDX;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;
    gpe_context = &priv_ctx->brc_context.gpe_contexts[gpe_idx];

    gen8_gpe_context_init(ctx, gpe_context);
    gen9_gpe_reset_binding_table(ctx, gpe_context);
    gen9_hevc_brc_intra_dist_set_curbe(ctx, encode_state, encoder_context, gpe_context);
    gen9_hevc_brc_intra_dist_set_surfaces(ctx, encode_state, encoder_context, gpe_context);
    gen8_gpe_setup_interface_data(ctx, gpe_context);

    memset((void *)&hevc_walker_param, 0, sizeof(hevc_walker_param));
    hevc_walker_param.resolution_x = priv_state->downscaled_width_4x_in_mb;
    hevc_walker_param.resolution_y = priv_state->downscaled_height_4x_in_mb;
    hevc_walker_param.no_dependency = 1;
    gen9_hevc_init_object_walker(&hevc_walker_param, &param);
    gen9_hevc_run_object_walker(ctx, encoder_context, gpe_context, &param,
                                media_state);
}

static GEN9_HEVC_BRC_UPDATE_FRAME_TYPE gen9_hevc_get_brc_frame_type(unsigned int pic_type,
                                                                    int low_delay)
{
    if (pic_type == HEVC_SLICE_I)
        return HEVC_BRC_FTYPE_I;
    else if (pic_type == HEVC_SLICE_P)
        return HEVC_BRC_FTYPE_P_OR_LB;
    else
        return low_delay ? HEVC_BRC_FTYPE_P_OR_LB : HEVC_BRC_FTYPE_B;
}

static void
gen9_hevc_brc_update_set_roi_curbe(VADriverContextP ctx,
                                   struct encode_state *encode_state,
                                   struct intel_encoder_context *encoder_context,
                                   gen9_hevc_brc_udpate_curbe_data *cmd)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct generic_enc_codec_state *generic_state = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    struct intel_roi *roi_par = NULL;
    unsigned int roi_size = 0, roi_ratio = 0;
    int i = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    generic_state = (struct generic_enc_codec_state *)vme_context->generic_enc_state;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    cmd->dw6.cqp_value = 0;
    cmd->dw6.roi_flag = 1 | (generic_state->brc_enabled << 1) |
                        (priv_state->video_surveillance_flag << 2);

    for (i = 0; i < priv_state->num_roi; i++) {
        roi_par = &priv_state->roi[i];
        roi_size += abs(roi_par->right - roi_par->left) *
                    abs(roi_par->bottom - roi_par->top) * 256;
    }

    if (roi_size)
        roi_ratio = MIN(2 * (priv_state->width_in_mb * priv_state->height_in_mb * 256 / roi_size - 1),
                        51);

    cmd->dw6.roi_ratio = roi_ratio;
    cmd->dw7.frame_width_in_lcu = priv_state->frame_width_in_max_lcu;

    if (!generic_state->brc_enabled) {
        VAEncPictureParameterBufferHEVC *pic_param = NULL;
        VAEncSliceParameterBufferHEVC *slice_param = NULL;

        pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;
        slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[0]->buffer;

        cmd->dw1.frame_number = priv_state->frame_number;
        cmd->dw6.cqp_value = pic_param->pic_init_qp + slice_param->slice_qp_delta;
        cmd->dw5.curr_frame_type = gen9_hevc_get_brc_frame_type(priv_state->picture_coding_type,
                                                                priv_state->low_delay);
    }
}

static void
gen9_hevc_brc_update_lcu_based_set_roi_parameters(VADriverContextP ctx,
                                                  struct encode_state *encode_state,
                                                  struct intel_encoder_context *encoder_context,
                                                  struct i965_gpe_context *gpe_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct generic_enc_codec_state *generic_state = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    struct intel_roi *roi_par = NULL;
    unsigned int width_in_mb_aligned = 0;
    unsigned int roi_level, qp_delta;
    unsigned int mb_num = 0;
    unsigned int *pdata = NULL;
    unsigned int out_data = 0;
    int i = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    generic_state = (struct generic_enc_codec_state *)vme_context->generic_enc_state;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    width_in_mb_aligned = ALIGN(priv_state->width_in_mb * 4, 64);
    mb_num = priv_state->width_in_mb * priv_state->height_in_mb;

    pdata = i965_map_gpe_resource(&priv_ctx->res_roi_buffer);
    if (!pdata)
        return;

    for (i = 0 ; i < mb_num; i++) {
        int cur_mb_y = i / priv_state->width_in_mb;
        int cur_mb_x = i - cur_mb_y * priv_state->width_in_mb;
        int roi_idx = 0;

        out_data = 0;

        for (roi_idx = (priv_state->num_roi - 1); roi_idx >= 0; roi_idx--) {
            roi_par = &priv_state->roi[roi_idx];

            roi_level = qp_delta = 0;
            if (generic_state->brc_enabled && !priv_state->roi_value_is_qp_delta)
                roi_level = roi_par->value * 5;
            else
                qp_delta = roi_par->value;

            if (roi_level == 0 && qp_delta == 0)
                continue;

            if ((cur_mb_x >= roi_par->left) &&
                (cur_mb_x < roi_par->right) &&
                (cur_mb_y >= roi_par->top) &&
                (cur_mb_y < roi_par->bottom))
                out_data = 15 | (((roi_level) & 0xFF) << 8) | ((qp_delta & 0xFF) << 16);
            else if ((cur_mb_x >= roi_par->left - 1) &&
                     (cur_mb_x < roi_par->right + 1) &&
                     (cur_mb_y >= roi_par->top - 1) &&
                     (cur_mb_y < roi_par->bottom + 1))
                out_data = 14 | (((roi_level) & 0xFF) << 8) | ((qp_delta & 0xFF) << 16);
            else if ((cur_mb_x >= roi_par->left - 2) &&
                     (cur_mb_x < roi_par->right + 2) &&
                     (cur_mb_y >= roi_par->top - 2) &&
                     (cur_mb_y < roi_par->bottom + 2))
                out_data = 13 | (((roi_level) & 0xFF) << 8) | ((qp_delta & 0xFF) << 16);
            else if ((cur_mb_x >= roi_par->left - 3) &&
                     (cur_mb_x < roi_par->right + 3) &&
                     (cur_mb_y >= roi_par->top - 3) &&
                     (cur_mb_y < roi_par->bottom + 3)) {
                out_data = 12 | (((roi_level) & 0xFF) << 8) | ((qp_delta & 0xFF) << 16);
            }
        }

        pdata[(cur_mb_y * (width_in_mb_aligned >> 2)) + cur_mb_x] = out_data;
    }

    i965_unmap_gpe_resource(&priv_ctx->res_roi_buffer);
}

static void
gen9_hevc_brc_update_lcu_based_set_curbe(VADriverContextP ctx,
                                         struct encode_state *encode_state,
                                         struct intel_encoder_context *encoder_context,
                                         struct i965_gpe_context *gpe_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct generic_enc_codec_state *generic_state = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    gen9_hevc_brc_udpate_curbe_data *cmd = NULL, *frame_cmd = NULL;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    generic_state = (struct generic_enc_codec_state *)vme_context->generic_enc_state;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    cmd = i965_gpe_context_map_curbe(gpe_context);
    if (!cmd)
        return;

    if (generic_state->brc_enabled) {
        frame_cmd = i965_gpe_context_map_curbe(&priv_ctx->brc_context.gpe_contexts[HEVC_BRC_FRAME_UPDATE_IDX]);

        if (!frame_cmd)
            return;

        memcpy((void *)cmd, (void *)frame_cmd, sizeof(*cmd));

        i965_gpe_context_unmap_curbe(&priv_ctx->brc_context.gpe_contexts[HEVC_BRC_FRAME_UPDATE_IDX]);
    } else {
        memcpy((void *)cmd, GEN9_HEVC_BRCUPDATE_CURBE_DATA,
               sizeof(GEN9_HEVC_BRCUPDATE_CURBE_DATA));
    }

    if (priv_state->num_roi)
        gen9_hevc_brc_update_set_roi_curbe(ctx, encode_state, encoder_context, cmd);

    i965_gpe_context_unmap_curbe(gpe_context);
}

static void
gen9_hevc_brc_update_lcu_based_set_surfaces(VADriverContextP ctx,
                                            struct encode_state *encode_state,
                                            struct intel_encoder_context *encoder_context,
                                            struct i965_gpe_context *gpe_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    int bti_idx = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_BRC_HISTORY, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_BRC_ME_DIST, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 priv_state->picture_coding_type == HEVC_SLICE_I ?
                                 &priv_ctx->res_brc_intra_dist_buffer :
                                 &priv_ctx->res_brc_me_dist_buffer,
                                 NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_BRC_ME_DIST, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 &priv_ctx->res_brc_intra_dist_buffer, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_HME_MVP, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_LCU_QP, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_ROI, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);
}

static void
gen9_hevc_brc_update_lcu_based(VADriverContextP ctx,
                               struct encode_state *encode_state,
                               struct intel_encoder_context *encoder_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    struct i965_gpe_context *gpe_context = NULL;
    struct gpe_media_object_walker_parameter param;
    struct hevc_enc_kernel_walker_parameter hevc_walker_param;
    int media_state = HEVC_ENC_MEDIA_STATE_HEVC_BRC_LCU_UPDATE;
    int gpe_idx = HEVC_BRC_LCU_UPDATE_IDX;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;
    gpe_context = &priv_ctx->brc_context.gpe_contexts[gpe_idx];

    gen8_gpe_context_init(ctx, gpe_context);
    gen9_gpe_reset_binding_table(ctx, gpe_context);

    if (priv_state->num_roi)
        gen9_hevc_brc_update_lcu_based_set_roi_parameters(ctx, encode_state, encoder_context,
                                                          gpe_context);

    gen9_hevc_brc_update_lcu_based_set_curbe(ctx, encode_state, encoder_context, gpe_context);
    gen9_hevc_brc_update_lcu_based_set_surfaces(ctx, encode_state, encoder_context, gpe_context);
    gen8_gpe_setup_interface_data(ctx, gpe_context);

    memset((void *)&hevc_walker_param, 0, sizeof(hevc_walker_param));
    hevc_walker_param.resolution_x = ALIGN(priv_state->picture_width, 128) >> 7;
    hevc_walker_param.resolution_y = ALIGN(priv_state->picture_height, 128) >> 7;
    hevc_walker_param.no_dependency = 1;
    gen9_hevc_init_object_walker(&hevc_walker_param, &param);
    gen9_hevc_run_object_walker(ctx, encoder_context, gpe_context, &param,
                                media_state);
}

static void
gen9_hevc_add_pic_state(VADriverContextP ctx,
                        struct encode_state *encode_state,
                        struct intel_encoder_context *encoder_context,
                        struct i965_gpe_resource *pic_state_ptr,
                        int pic_state_offset,
                        int brc_update)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct encoder_vme_mfc_context *pak_context = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    VAEncPictureParameterBufferHEVC *pic_param = NULL;
    VAEncSequenceParameterBufferHEVC *seq_param = NULL;
    unsigned int tmp_data[31], *cmd_ptr = NULL;
    int cmd_size = 0;

    pak_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_state = (struct gen9_hevc_encoder_state *)pak_context->private_enc_state;

    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;
    seq_param = (VAEncSequenceParameterBufferHEVC *)encode_state->seq_param_ext->buffer;

    cmd_ptr = tmp_data;
    cmd_size = (IS_KBL(i965->intel.device_info) || IS_GLK(i965->intel.device_info)) ? 31 : 19;
    memset((void *)tmp_data, 0, 4 * cmd_size);

    if (IS_KBL(i965->intel.device_info) || IS_GLK(i965->intel.device_info))
        *cmd_ptr++ = HCP_PIC_STATE | (31 - 2);
    else
        *cmd_ptr++ = HCP_PIC_STATE | (19 - 2);

    *cmd_ptr++ = (priv_state->height_in_cu - 1) << 16 |
                 0 << 14 |
                 (priv_state->width_in_cu - 1);
    *cmd_ptr++ = (seq_param->log2_min_transform_block_size_minus2 +
                  seq_param->log2_diff_max_min_transform_block_size) << 6 |
                 seq_param->log2_min_transform_block_size_minus2 << 4 |
                 (seq_param->log2_min_luma_coding_block_size_minus3 +
                  seq_param->log2_diff_max_min_luma_coding_block_size) << 2 |
                 seq_param->log2_min_luma_coding_block_size_minus3;
    *cmd_ptr++ = 0;
    *cmd_ptr++ = ((IS_KBL(i965->intel.device_info) || IS_GLK(i965->intel.device_info)) ? 1 : 0) << 27 |
                 seq_param->seq_fields.bits.strong_intra_smoothing_enabled_flag << 26 |
                 pic_param->pic_fields.bits.transquant_bypass_enabled_flag << 25 |
                 ((IS_KBL(i965->intel.device_info) || IS_GLK(i965->intel.device_info)) ? 0 : priv_state->ctu_max_bitsize_allowed > 0) << 24 |
                 seq_param->seq_fields.bits.amp_enabled_flag << 23 |
                 pic_param->pic_fields.bits.transform_skip_enabled_flag << 22 |
                 0 << 21 |
                 0 << 20 |
                 pic_param->pic_fields.bits.weighted_pred_flag << 19 |
                 pic_param->pic_fields.bits.weighted_bipred_flag << 18 |
                 0 << 17 |
                 pic_param->pic_fields.bits.entropy_coding_sync_enabled_flag << 16 |
                 0 << 15 |
                 pic_param->pic_fields.bits.sign_data_hiding_enabled_flag << 13 |
                 pic_param->log2_parallel_merge_level_minus2 << 10 |
                 pic_param->pic_fields.bits.constrained_intra_pred_flag << 9 |
                 seq_param->seq_fields.bits.pcm_loop_filter_disabled_flag << 8 |
                 (pic_param->diff_cu_qp_delta_depth & 0x03) << 6 |
                 pic_param->pic_fields.bits.cu_qp_delta_enabled_flag << 5 |
                 0 << 4 |
                 seq_param->seq_fields.bits.sample_adaptive_offset_enabled_flag << 3 |
                 0;
    *cmd_ptr++ = seq_param->seq_fields.bits.bit_depth_luma_minus8 << 27 |
                 seq_param->seq_fields.bits.bit_depth_chroma_minus8 << 24 |
                 ((IS_KBL(i965->intel.device_info) || IS_GLK(i965->intel.device_info)) ? 0 : 7) << 20 |
                 ((IS_KBL(i965->intel.device_info) || IS_GLK(i965->intel.device_info)) ? 0 : 7) << 16 |
                 seq_param->max_transform_hierarchy_depth_inter << 13 |
                 seq_param->max_transform_hierarchy_depth_intra << 10 |
                 (pic_param->pps_cr_qp_offset & 0x1f) << 5 |
                 (pic_param->pps_cb_qp_offset & 0x1f);

    *cmd_ptr++ = 0 << 29 |
                 priv_state->ctu_max_bitsize_allowed;
    if (brc_update)
        *(cmd_ptr - 1) |= 0 << 31 |
                          1 << 26 |
                          1 << 25 |
                          0 << 24 |
                          (pic_state_offset ? 1 : 0) << 16;

    *cmd_ptr++ = 0 << 31 |
                 0;
    *cmd_ptr++ = 0 << 31 |
                 0;
    *cmd_ptr++ = 0 << 16 |
                 0;
    *cmd_ptr++ = 0;
    *cmd_ptr++ = 0;
    *cmd_ptr++ = 0;
    *cmd_ptr++ = 0;
    *cmd_ptr++ = 0;
    *cmd_ptr++ = 0;
    *cmd_ptr++ = 0;
    *cmd_ptr++ = 0;
    *cmd_ptr++ = 0 << 30 |
                 0;

    if (IS_KBL(i965->intel.device_info) || IS_GLK(i965->intel.device_info)) {
        int i = 0;

        for (i = 0; i < 12; i++)
            *cmd_ptr++ = 0;
    }

    if (pic_state_ptr) {
        char *pdata = i965_map_gpe_resource(pic_state_ptr);

        if (!pdata)
            return;

        memcpy(pdata + pic_state_offset, tmp_data, cmd_size * 4);

        pdata += pic_state_offset + cmd_size * 4;

        *(unsigned int *)pdata++ = MI_BATCH_BUFFER_END;

        i965_unmap_gpe_resource(pic_state_ptr);
    } else {
        struct intel_batchbuffer *batch = encoder_context->base.batch;

        BEGIN_BCS_BATCH(batch, cmd_size);

        intel_batchbuffer_data(batch, tmp_data, cmd_size * 4);

        ADVANCE_BCS_BATCH(batch);
    }
}

static void
gen9_hevc_brc_update_set_pic_states(VADriverContextP ctx,
                                    struct encode_state *encode_state,
                                    struct intel_encoder_context *encoder_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct generic_enc_codec_state *generic_state = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    int i = 0, offset = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    generic_state = (struct generic_enc_codec_state *)vme_context->generic_enc_state;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    for (i = 0; i < generic_state->num_pak_passes; i++) {
        gen9_hevc_add_pic_state(ctx, encode_state, encoder_context,
                                &priv_ctx->res_brc_pic_states_read_buffer,
                                offset, 1);

        offset += priv_state->pic_state_size;
    }
}

static void
gen9_hevc_brc_update_set_constant(VADriverContextP ctx,
                                  struct encode_state *encode_state,
                                  struct intel_encoder_context *encoder_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    unsigned int width, height, size;
    unsigned char *pdata = NULL;
    int idx = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    pdata = i965_map_gpe_resource(&priv_ctx->res_brc_constant_data_buffer);
    if (!pdata)
        return;

    width = ALIGN(GEN9_HEVC_ENC_BRC_CONSTANT_SURFACE_WIDTH, 64);
    height = GEN9_HEVC_ENC_BRC_CONSTANT_SURFACE_HEIGHT;
    size = width * height;
    memset((void *)pdata, 0, size);

    memcpy((void *)pdata, GEN9_HEVC_BRCUPDATE_QP_ADJUST, GEN9_HEVC_BRCUPDATE_QP_ADJUST_SIZE);
    pdata += GEN9_HEVC_BRCUPDATE_QP_ADJUST_SIZE;

    if (priv_state->picture_coding_type == HEVC_SLICE_I)
        memset((void *)pdata, 0, GEN9_HEVC_ENC_SKIP_VAL_SIZE);
    else {
        gen9_hevc_mbenc_b_mb_enc_curbe_data *curbe_cmd = NULL;
        int curbe_size = 0;

        gen9_hevc_get_b_mbenc_default_curbe(priv_state->tu_mode,
                                            priv_state->picture_coding_type,
                                            (void **)&curbe_cmd,
                                            &curbe_size);

        idx = curbe_cmd->dw3.block_based_skip_enable ? 1 : 0;
        memcpy((void *)pdata, GEN9_HEVC_ENC_SKIP_THREAD[idx], sizeof(GEN9_HEVC_ENC_SKIP_THREAD[idx]));
    }
    pdata += GEN9_HEVC_ENC_SKIP_VAL_SIZE;

    memcpy((void *)pdata, GEN9_HEVC_ENC_BRC_LAMBDA_HAAR, sizeof(GEN9_HEVC_ENC_BRC_LAMBDA_HAAR));
    pdata += GEN9_HEVC_ENC_BRC_LAMBDA_TABLE_SIZE;

    idx = (priv_state->picture_coding_type == HEVC_SLICE_I) ? 0 :
          (priv_state->picture_coding_type == HEVC_SLICE_P) ? 1 : 2;
    memcpy((void *)pdata, GEN9_HEVC_ENC_BRC_MVCOST_HAAR[idx], sizeof(GEN9_HEVC_ENC_BRC_MVCOST_HAAR[idx]));

    i965_unmap_gpe_resource(&priv_ctx->res_brc_constant_data_buffer);
}

static
unsigned int gen9_hevc_get_start_code_offset(unsigned char *ptr,
                                             unsigned int size)
{
    unsigned int count = 0;

    while (count < size && *ptr != 0x01) {
        if (*ptr != 0)
            break;

        count++;
        ptr++;
    }

    return count + 1;
}

static
unsigned int gen9_hevc_get_emulation_num(unsigned char *ptr,
                                         unsigned int size)
{
    unsigned int emulation_num = 0;
    unsigned int header_offset = 0;
    unsigned int zero_count = 0;
    int i = 0;

    header_offset = gen9_hevc_get_start_code_offset(ptr, size);
    ptr += header_offset;

    for (i = 0 ; i < (size - header_offset); i++, ptr++) {
        if (zero_count == 2 && !(*ptr & 0xFC)) {
            zero_count = 0;
            emulation_num++;
        }

        if (*ptr == 0x00)
            zero_count++;
        else
            zero_count = 0;
    }

    return emulation_num;
}

#define HEVC_ENC_START_CODE_NAL_OFFSET                  (2)

static unsigned int
gen9_hevc_get_pic_header_size(struct encode_state *encode_state)
{
    VAEncPackedHeaderParameterBuffer *param = NULL;
    unsigned int header_begin = 0;
    unsigned int accum_size = 0;
    unsigned char *header_data = NULL;
    unsigned int length_in_bytes = 0;
    int packed_type = 0;
    int idx = 0, count = 0, idx_offset = 0;
    int i = 0, slice_idx = 0, start_index = 0;

    for (i = 0; i < 4; i++) {
        idx_offset = 0;
        switch (i) {
        case 0:
            packed_type = VAEncPackedHeaderHEVC_VPS;
            break;
        case 1:
            packed_type = VAEncPackedHeaderHEVC_VPS;
            idx_offset = 1;
            break;
        case 2:
            packed_type = VAEncPackedHeaderHEVC_PPS;
            break;
        case 3:
            packed_type = VAEncPackedHeaderHEVC_SEI;
            break;
        default:
            break;
        }

        idx = va_enc_packed_type_to_idx(packed_type) + idx_offset;
        if (encode_state->packed_header_data[idx]) {
            param = (VAEncPackedHeaderParameterBuffer *)encode_state->packed_header_param[idx]->buffer;
            header_data = (unsigned char *)encode_state->packed_header_data[idx]->buffer;
            length_in_bytes = (param->bit_length + 7) / 8;

            header_begin = gen9_hevc_get_start_code_offset(header_data, length_in_bytes) +
                           HEVC_ENC_START_CODE_NAL_OFFSET;

            accum_size += length_in_bytes;
            if (!param->has_emulation_bytes)
                accum_size += gen9_hevc_get_emulation_num(header_data,
                                                          length_in_bytes);
        }
    }

    for (slice_idx = 0; slice_idx < encode_state->num_slice_params_ext; slice_idx++) {
        count = encode_state->slice_rawdata_count[slice_idx];
        start_index = encode_state->slice_rawdata_index[slice_idx] &
                      SLICE_PACKED_DATA_INDEX_MASK;

        for (i = 0; i < count; i++) {
            param = (VAEncPackedHeaderParameterBuffer *)
                    (encode_state->packed_header_params_ext[start_index + i]->buffer);

            if (param->type == VAEncPackedHeaderSlice)
                continue;

            header_data = (unsigned char *)encode_state->packed_header_data[start_index]->buffer;
            length_in_bytes = (param->bit_length + 7) / 8;

            accum_size += length_in_bytes;
            if (!param->has_emulation_bytes)
                accum_size += gen9_hevc_get_emulation_num(header_data,
                                                          length_in_bytes);
        }
    }

    header_begin = MIN(header_begin, accum_size);

    return ((accum_size - header_begin) * 8);
}

static void
gen9_hevc_brc_update_set_curbe(VADriverContextP ctx,
                               struct encode_state *encode_state,
                               struct intel_encoder_context *encoder_context,
                               struct i965_gpe_context *gpe_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct generic_enc_codec_state *generic_state = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    gen9_hevc_brc_udpate_curbe_data *cmd = NULL;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    generic_state = (struct generic_enc_codec_state *)vme_context->generic_enc_state;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    cmd = i965_gpe_context_map_curbe(gpe_context);
    if (!cmd)
        return;

    memcpy((void *)cmd, GEN9_HEVC_BRCUPDATE_CURBE_DATA,
           sizeof(GEN9_HEVC_BRCUPDATE_CURBE_DATA));

    cmd->dw5.target_size_flag = 0;
    if (priv_state->brc_init_current_target_buf_full_in_bits >
        (double)priv_state->brc_init_reset_buf_size_in_bits) {
        priv_state->brc_init_current_target_buf_full_in_bits -=
            (double)priv_state->brc_init_reset_buf_size_in_bits;
        cmd->dw5.target_size_flag = 1;
    }

    if (priv_state->num_skip_frames) {
        cmd->dw6.num_skipped_frames = priv_state->num_skip_frames;
        cmd->dw15.size_of_skipped_frames = priv_state->size_skip_frames;

        priv_state->brc_init_current_target_buf_full_in_bits +=
            priv_state->brc_init_reset_input_bits_per_frame * priv_state->num_skip_frames;
    }

    cmd->dw0.target_size = (unsigned int)priv_state->brc_init_current_target_buf_full_in_bits;
    cmd->dw1.frame_number = priv_state->frame_number;
    cmd->dw2.picture_header_size = gen9_hevc_get_pic_header_size(encode_state);

    cmd->dw5.brc_flag = 0;
    cmd->dw5.curr_frame_type = gen9_hevc_get_brc_frame_type(priv_state->picture_coding_type,
                                                            priv_state->low_delay);

    cmd->dw5.max_num_paks = generic_state->num_pak_passes;
    cmd->dw14.parallel_mode = priv_state->parallel_brc;

    priv_state->brc_init_current_target_buf_full_in_bits +=
        priv_state->brc_init_reset_input_bits_per_frame;

    if (priv_state->brc_method == HEVC_BRC_AVBR) {
        cmd->dw3.start_gadj_frame0 = (unsigned int)((10 * GEN9_HEVC_AVBR_CONVERGENCE) / (double)150);
        cmd->dw3.start_gadj_frame1 = (unsigned int)((50 * GEN9_HEVC_AVBR_CONVERGENCE) / (double)150);
        cmd->dw4.start_gadj_frame2 = (unsigned int)((100 * GEN9_HEVC_AVBR_CONVERGENCE) / (double)150);
        cmd->dw4.start_gadj_frame3 = (unsigned int)((150 * GEN9_HEVC_AVBR_CONVERGENCE) / (double)150);
        cmd->dw11.g_rate_ratio_threshold0 = (unsigned int)((100 - (GEN9_HEVC_AVBR_ACCURACY / (double)30) * (100 - 40)));
        cmd->dw11.g_rate_ratio_threshold1 = (unsigned int)((100 - (GEN9_HEVC_AVBR_ACCURACY / (double)30) * (100 - 75)));
        cmd->dw12.g_rate_ratio_threshold2 = (unsigned int)((100 - (GEN9_HEVC_AVBR_ACCURACY / (double)30) * (100 - 97)));
        cmd->dw12.g_rate_ratio_threshold3 = (unsigned int)((100 + (GEN9_HEVC_AVBR_ACCURACY / (double)30) * (103 - 100)));
        cmd->dw12.g_rate_ratio_threshold4 = (unsigned int)((100 + (GEN9_HEVC_AVBR_ACCURACY / (double)30) * (125 - 100)));
        cmd->dw12.g_rate_ratio_threshold5 = (unsigned int)((100 + (GEN9_HEVC_AVBR_ACCURACY / (double)30) * (160 - 100)));
    } else {
        cmd->dw3.start_gadj_frame0 = 10;
        cmd->dw3.start_gadj_frame1 = 50;
        cmd->dw4.start_gadj_frame2 = 100;
        cmd->dw4.start_gadj_frame3 = 150;
        cmd->dw11.g_rate_ratio_threshold0 = 40;
        cmd->dw11.g_rate_ratio_threshold1 = 75;
        cmd->dw12.g_rate_ratio_threshold2 = 97;
        cmd->dw12.g_rate_ratio_threshold3 = 103;
        cmd->dw12.g_rate_ratio_threshold4 = 125;
        cmd->dw12.g_rate_ratio_threshold5 = 160;
    }

    i965_gpe_context_unmap_curbe(gpe_context);
}

static void
gen9_hevc_brc_update_set_surfaces(VADriverContextP ctx,
                                  struct intel_encoder_context *encoder_context,
                                  struct i965_gpe_context *gpe_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    int bti_idx = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_BRC_HISTORY, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_BRC_PAST_PAK_INFO, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_BRC_HCP_PIC_STATE, bti_idx++,
                                 0, 0, 0, &priv_ctx->res_brc_pic_states_read_buffer,
                                 NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_BRC_HCP_PIC_STATE, bti_idx++,
                                 0, 0, 0, &priv_ctx->res_brc_pic_states_write_buffer,
                                 NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_BRC_INPUT, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_BRC_ME_DIST, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 priv_state->picture_coding_type == HEVC_SLICE_I ?
                                 &priv_ctx->res_brc_intra_dist_buffer :
                                 &priv_ctx->res_brc_me_dist_buffer,
                                 NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_BRC_DATA, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);
}

static void
gen9_hevc_brc_update(VADriverContextP ctx,
                     struct encode_state *encode_state,
                     struct intel_encoder_context *encoder_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    struct i965_gpe_context *gpe_context = NULL;
    struct gpe_media_object_parameter param;
    int media_state = HEVC_ENC_MEDIA_STATE_BRC_UPDATE;
    int gpe_idx = HEVC_BRC_FRAME_UPDATE_IDX;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;
    gpe_context = &priv_ctx->brc_context.gpe_contexts[gpe_idx];

    gen8_gpe_context_init(ctx, gpe_context);
    gen9_gpe_reset_binding_table(ctx, gpe_context);

    gen9_hevc_brc_update_set_pic_states(ctx, encode_state, encoder_context);
    gen9_hevc_brc_update_set_constant(ctx, encode_state, encoder_context);
    gen9_hevc_brc_update_set_curbe(ctx, encode_state, encoder_context, gpe_context);
    gen9_hevc_brc_update_set_surfaces(ctx, encoder_context, gpe_context);

    gen8_gpe_setup_interface_data(ctx, gpe_context);

    memset(&param, 0, sizeof(param));
    gen9_hevc_run_object(ctx, encoder_context, gpe_context, &param,
                         media_state);

    if (priv_state->lcu_brc_enabled ||
        priv_state->num_roi)
        gen9_hevc_brc_update_lcu_based(ctx, encode_state, encoder_context);
}

// Depth converstion for 10bits

static void
gen9_hevc_frame_depth_conversion_set_curbe(VADriverContextP ctx,
                                           struct encode_state *encode_state,
                                           struct intel_encoder_context *encoder_context,
                                           struct i965_gpe_context *gpe_context,
                                           GEN9_HEVC_DOWNSCALE_STAGE scale_stage)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    gen95_hevc_mbenc_ds_combined_curbe_data *cmd = NULL;
    int bti_idx = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    cmd = i965_gpe_context_map_curbe(gpe_context);
    if (!cmd)
        return;

    memset((void *)cmd, 0, sizeof(*cmd));

    cmd->dw0.pak_bitdepth_chroma = 10;
    cmd->dw0.pak_bitdepth_luma = 10;
    cmd->dw0.enc_bitdepth_chroma = 8;
    cmd->dw0.enc_bitdepth_luma = 8;
    cmd->dw0.rounding_value = 1;
    cmd->dw1.pic_format = 0;
    cmd->dw1.pic_convert_flag = 1;
    cmd->dw1.pic_down_scale = scale_stage;
    cmd->dw1.pic_mb_stat_output_cntrl = 0;
    cmd->dw2.orig_pic_width = priv_state->picture_width;
    cmd->dw2.orig_pic_height = priv_state->picture_height;

    cmd->dw3.bti_surface_p010 = bti_idx++;
    bti_idx++;
    cmd->dw4.bti_surface_nv12 = bti_idx++;
    bti_idx++;
    cmd->dw5.bti_src_y_4xdownscaled = bti_idx++;
    cmd->dw6.bti_surf_mbstate = bti_idx++;
    cmd->dw7.bit_src_y_2xdownscaled = bti_idx++;

    i965_gpe_context_unmap_curbe(gpe_context);
}

static void
gen9_hevc_frame_depth_conversion_set_surfaces(VADriverContextP ctx,
                                              struct encode_state *encode_state,
                                              struct intel_encoder_context *encoder_context,
                                              struct i965_gpe_context *gpe_context,
                                              struct object_surface *src_surface,
                                              struct object_surface *dst_surface)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_surface_priv *surface_priv = NULL;
    int bti_idx = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    surface_priv = (struct gen9_hevc_surface_priv *)dst_surface->private_data;

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_RAW_10bit_Y_UV, bti_idx++,
                                 1, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, src_surface);
    bti_idx++;

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_RAW_FC_8bit_Y_UV, bti_idx++,
                                 1, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL,
                                 surface_priv->surface_obj_nv12);
    bti_idx++;

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_Y_4X, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R32_UNORM,
                                 NULL,
                                 surface_priv->scaled_surface_obj[HEVC_SCALED_SURF_4X_ID]);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_RAW_MBSTAT, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_Y_2X, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R32_UNORM,
                                 NULL, NULL);
}

static void
gen9_hevc_frame_depth_conversion(VADriverContextP ctx,
                                 struct encode_state *encode_state,
                                 struct intel_encoder_context *encoder_context,
                                 struct object_surface *src_surface,
                                 struct object_surface *dst_surface,
                                 GEN9_HEVC_DOWNSCALE_STAGE scale_stage)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    struct gpe_media_object_walker_parameter param;
    struct hevc_enc_kernel_walker_parameter hevc_walker_param;
    struct i965_gpe_context *gpe_context = NULL;
    int media_state = HEVC_ENC_MEDIA_STATE_DS_COMBINED;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    gpe_context = &priv_ctx->mbenc_context.gpe_contexts[HEVC_MBENC_DS_COMBINED_IDX];

    gen8_gpe_context_init(ctx, gpe_context);
    gen9_gpe_reset_binding_table(ctx, gpe_context);
    gen9_hevc_frame_depth_conversion_set_curbe(ctx, encode_state, encoder_context, gpe_context,
                                               scale_stage);
    gen9_hevc_frame_depth_conversion_set_surfaces(ctx, encode_state, encoder_context, gpe_context,
                                                  src_surface, dst_surface);
    gen8_gpe_setup_interface_data(ctx, gpe_context);

    memset((void *)&hevc_walker_param, 0, sizeof(hevc_walker_param));
    hevc_walker_param.resolution_x = ALIGN(priv_state->picture_width >> 2, 32) >> 3;
    hevc_walker_param.resolution_y = ALIGN(priv_state->picture_height >> 2, 32) >> 3;
    hevc_walker_param.no_dependency = 1;
    gen9_hevc_init_object_walker(&hevc_walker_param, &param);
    gen9_hevc_run_object_walker(ctx, encoder_context, gpe_context, &param,
                                media_state);
}

static void
gen9_hevc_ref_frame_depth_conversion(VADriverContextP ctx,
                                     struct encode_state *encode_state,
                                     struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct gen9_hevc_surface_priv *surface_priv = NULL;
    VAEncSliceParameterBufferHEVC *slice_param = NULL;
    struct object_surface *obj_surface = NULL;
    int i = 0;

    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[0]->buffer;

    for (i = 0; i < slice_param->num_ref_idx_l0_active_minus1 + 1; i++) {
        obj_surface = SURFACE(slice_param->ref_pic_list0[i].picture_id);
        if (obj_surface) {
            surface_priv = (struct gen9_hevc_surface_priv *)obj_surface->private_data;

            if (!surface_priv->surface_nv12_valid) {
                gen9_hevc_frame_depth_conversion(ctx, encode_state, encoder_context,
                                                 obj_surface, obj_surface,
                                                 HEVC_ENC_DS_DISABLED);

                surface_priv->surface_reff = surface_priv->surface_obj_nv12;
                surface_priv->surface_nv12_valid = 1;
            }
        }
    }

    for (i = 0; i < slice_param->num_ref_idx_l1_active_minus1 + 1; i++) {
        obj_surface = SURFACE(slice_param->ref_pic_list1[i].picture_id);
        if (obj_surface) {
            surface_priv = (struct gen9_hevc_surface_priv *)obj_surface->private_data;

            if (!surface_priv->surface_nv12_valid) {
                gen9_hevc_frame_depth_conversion(ctx, encode_state, encoder_context,
                                                 obj_surface, obj_surface,
                                                 HEVC_ENC_DS_DISABLED);

                surface_priv->surface_reff = surface_priv->surface_obj_nv12;
                surface_priv->surface_nv12_valid = 1;
            }
        }
    }
}

// Scaling implementation

static void
gen9_hevc_scaling_set_curbe_2x(struct i965_gpe_context *gpe_context,
                               struct gen9_hevc_scaling_parameter *scaling_param)
{
    gen9_hevc_scaling2x_curbe_data *cmd;

    cmd = i965_gpe_context_map_curbe(gpe_context);
    if (!cmd)
        return;

    memset((void *)cmd, 0, sizeof(*cmd));

    cmd->dw0.input_picture_width  = scaling_param->input_frame_width;
    cmd->dw0.input_picture_height = scaling_param->input_frame_height;

    cmd->dw8.input_y_bti = GEN9_HEVC_SCALING_FRAME_SRC_Y_INDEX;
    cmd->dw9.output_y_bti = GEN9_HEVC_SCALING_FRAME_DST_Y_INDEX;

    i965_gpe_context_unmap_curbe(gpe_context);
}

static void
gen9_hevc_scaling_set_curbe_4x(struct i965_gpe_context *gpe_context,
                               struct gen9_hevc_scaling_parameter *scaling_param)
{
    gen9_hevc_scaling4x_curbe_data *cmd;

    cmd = i965_gpe_context_map_curbe(gpe_context);
    if (!cmd)
        return;

    memset((void *)cmd, 0, sizeof(*cmd));

    cmd->dw0.input_picture_width  = scaling_param->input_frame_width;
    cmd->dw0.input_picture_height = scaling_param->input_frame_height;

    cmd->dw1.input_y_bti = GEN9_HEVC_SCALING_FRAME_SRC_Y_INDEX;
    cmd->dw2.output_y_bti = GEN9_HEVC_SCALING_FRAME_DST_Y_INDEX;

    cmd->dw5.flatness_threshold = 0;
    cmd->dw6.enable_mb_flatness_check = scaling_param->enable_mb_flatness_check;
    cmd->dw7.enable_mb_variance_output = scaling_param->enable_mb_variance_output;
    cmd->dw8.enable_mb_pixel_average_output = scaling_param->enable_mb_pixel_average_output;

    if (cmd->dw6.enable_mb_flatness_check ||
        cmd->dw7.enable_mb_variance_output ||
        cmd->dw8.enable_mb_pixel_average_output)
        cmd->dw10.mbv_proc_stat_bti = GEN9_HEVC_SCALING_FRAME_MBVPROCSTATS_DST_INDEX;

    i965_gpe_context_unmap_curbe(gpe_context);
}

static void
gen9_hevc_scaling_set_curbe(struct gen9_hevc_scaling_parameter *scaling_param,
                            struct i965_gpe_context *gpe_context)
{
    if (scaling_param->use_32x_scaling)
        gen9_hevc_scaling_set_curbe_2x(gpe_context, scaling_param);
    else
        gen9_hevc_scaling_set_curbe_4x(gpe_context, scaling_param);
}

static void
gen9_hevc_scaling_set_surfaces(VADriverContextP ctx,
                               struct encode_state *encode_state,
                               struct intel_encoder_context *encoder_context,
                               struct gen9_hevc_scaling_parameter *scaling_param,
                               struct i965_gpe_context *gpe_context)
{
    unsigned int surface_format;

    if (scaling_param->scaling_out_use_32unorm_surf_fmt)
        surface_format = I965_SURFACEFORMAT_R32_UNORM;
    else if (scaling_param->scaling_out_use_16unorm_surf_fmt)
        surface_format = I965_SURFACEFORMAT_R16_UNORM;
    else
        surface_format = I965_SURFACEFORMAT_R8_UNORM;

    gen9_add_2d_gpe_surface(ctx, gpe_context,
                            scaling_param->input_surface,
                            0, 1, surface_format,
                            GEN9_HEVC_SCALING_FRAME_SRC_Y_INDEX);

    gen9_add_2d_gpe_surface(ctx, gpe_context,
                            scaling_param->output_surface,
                            0, 1, surface_format,
                            GEN9_HEVC_SCALING_FRAME_DST_Y_INDEX);

    if ((scaling_param->enable_mb_flatness_check ||
         scaling_param->enable_mb_variance_output ||
         scaling_param->enable_mb_pixel_average_output) &&
        scaling_param->use_4x_scaling) {
        gen9_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                       scaling_param->pres_mbv_proc_stat_buffer,
                                       1,
                                       I965_SURFACEFORMAT_R8_UNORM,
                                       GEN9_HEVC_SCALING_FRAME_MBVPROCSTATS_DST_INDEX);
    }
}

static void
gen9_hevc_kernel_scaling(VADriverContextP ctx,
                         struct encode_state *encode_state,
                         struct intel_encoder_context *encoder_context,
                         enum HEVC_HME_TYPE hme_type)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    struct gen9_hevc_surface_priv *surface_priv = NULL;
    struct gen9_hevc_scaling_parameter scaling_param;
    struct gpe_media_object_walker_parameter param;
    unsigned int downscaled_width_in_mb, downscaled_height_in_mb;
    struct hevc_enc_kernel_walker_parameter hevc_walker_param;
    struct i965_gpe_context *gpe_context = NULL;
    int gpe_idx = 0, media_state = 0;;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    surface_priv = (struct gen9_hevc_surface_priv *)encode_state->reconstructed_object->private_data;

    memset((void *)&scaling_param, 0, sizeof(scaling_param));
    switch (hme_type) {
    case HEVC_HME_4X:
        media_state = HEVC_ENC_MEDIA_STATE_4X_SCALING;
        gpe_idx = HEVC_ENC_SCALING_4X;
        downscaled_width_in_mb = priv_state->downscaled_width_4x_in_mb;
        downscaled_height_in_mb = priv_state->downscaled_height_4x_in_mb;

        scaling_param.input_surface = encode_state->input_yuv_object;
        scaling_param.input_frame_width = priv_state->picture_width;
        scaling_param.input_frame_height = priv_state->picture_height;

        scaling_param.output_surface = surface_priv->scaled_surface_obj[HEVC_SCALED_SURF_4X_ID];
        scaling_param.output_frame_width = priv_state->frame_width_4x;
        scaling_param.output_frame_height = priv_state->frame_height_4x;

        scaling_param.enable_mb_flatness_check = priv_state->flatness_check_enable;
        scaling_param.enable_mb_variance_output = 0;
        scaling_param.enable_mb_pixel_average_output = 0;
        scaling_param.pres_mbv_proc_stat_buffer = &(priv_ctx->res_flatness_check_surface);

        scaling_param.blk8x8_stat_enabled = 0;
        scaling_param.use_4x_scaling  = 1;
        scaling_param.use_16x_scaling = 0;
        scaling_param.use_32x_scaling = 0;
        break;
    case HEVC_HME_16X:
        media_state = HEVC_ENC_MEDIA_STATE_16X_SCALING;
        gpe_idx = HEVC_ENC_SCALING_16X;
        downscaled_width_in_mb = priv_state->downscaled_width_16x_in_mb;
        downscaled_height_in_mb = priv_state->downscaled_height_16x_in_mb;

        scaling_param.input_surface = surface_priv->scaled_surface_obj[HEVC_SCALED_SURF_4X_ID];
        scaling_param.input_frame_width = priv_state->frame_width_4x;
        scaling_param.input_frame_height = priv_state->frame_height_4x;

        scaling_param.output_surface = surface_priv->scaled_surface_obj[HEVC_SCALED_SURF_16X_ID];
        scaling_param.output_frame_width = priv_state->frame_width_16x;
        scaling_param.output_frame_height = priv_state->frame_height_16x;

        scaling_param.enable_mb_flatness_check = 0;
        scaling_param.enable_mb_variance_output = 0;
        scaling_param.enable_mb_pixel_average_output = 0;

        scaling_param.blk8x8_stat_enabled = 0;
        scaling_param.use_4x_scaling  = 0;
        scaling_param.use_16x_scaling = 1;
        scaling_param.use_32x_scaling = 0;
        break;
    case HEVC_HME_32X:
        media_state = HEVC_ENC_MEDIA_STATE_32X_SCALING;
        gpe_idx = HEVC_ENC_SCALING_32X;
        downscaled_width_in_mb = priv_state->downscaled_width_32x_in_mb;
        downscaled_height_in_mb = priv_state->downscaled_height_32x_in_mb;

        scaling_param.input_surface = surface_priv->scaled_surface_obj[HEVC_SCALED_SURF_16X_ID];
        scaling_param.input_frame_width = priv_state->frame_width_16x;
        scaling_param.input_frame_height = priv_state->frame_height_16x;

        scaling_param.output_surface = surface_priv->scaled_surface_obj[HEVC_SCALED_SURF_32X_ID];
        scaling_param.output_frame_width = priv_state->frame_width_32x;
        scaling_param.output_frame_height = priv_state->frame_height_32x;

        scaling_param.enable_mb_flatness_check = 0;
        scaling_param.enable_mb_variance_output = 0;
        scaling_param.enable_mb_pixel_average_output = 0;

        scaling_param.blk8x8_stat_enabled = 0;
        scaling_param.use_4x_scaling  = 0;
        scaling_param.use_16x_scaling = 0;
        scaling_param.use_32x_scaling = 1;
        break;
    default:
        return;
    }

    gpe_context = &priv_ctx->scaling_context.gpe_contexts[gpe_idx];
    gen8_gpe_context_init(ctx, gpe_context);
    gen9_gpe_reset_binding_table(ctx, gpe_context);
    gen9_hevc_scaling_set_curbe(&scaling_param, gpe_context);

    if (hme_type == HEVC_HME_32X) {
        scaling_param.scaling_out_use_16unorm_surf_fmt = 1;
        scaling_param.scaling_out_use_32unorm_surf_fmt = 0;
    } else {
        scaling_param.scaling_out_use_16unorm_surf_fmt = 0;
        scaling_param.scaling_out_use_32unorm_surf_fmt = 1;
    }

    gen9_hevc_scaling_set_surfaces(ctx, encode_state, encoder_context, &scaling_param,
                                   gpe_context);
    gen8_gpe_setup_interface_data(ctx, gpe_context);

    memset((void *)&hevc_walker_param, 0, sizeof(hevc_walker_param));
    if (hme_type == HEVC_HME_32X) {
        hevc_walker_param.resolution_x = downscaled_width_in_mb;
        hevc_walker_param.resolution_y = downscaled_height_in_mb;
    } else {
        hevc_walker_param.resolution_x = downscaled_width_in_mb * 2;
        hevc_walker_param.resolution_y = downscaled_height_in_mb * 2;
    }
    hevc_walker_param.no_dependency = 1;
    gen9_hevc_init_object_walker(&hevc_walker_param, &param);
    gen9_hevc_run_object_walker(ctx, encoder_context, gpe_context, &param,
                                media_state);
}

static void
gen9_hevc_hme_scaling(VADriverContextP ctx,
                      struct encode_state *encode_state,
                      struct intel_encoder_context *encoder_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct generic_enc_codec_state *generic_state = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    generic_state = (struct generic_enc_codec_state *)vme_context->generic_enc_state;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    if (priv_state->bit_depth_luma_minus8)
        gen9_hevc_frame_depth_conversion(ctx, encode_state, encoder_context,
                                         encode_state->input_yuv_object,
                                         encode_state->reconstructed_object,
                                         HEVC_ENC_2xDS_4xDS_STAGE);
    else
        gen9_hevc_kernel_scaling(ctx, encode_state, encoder_context, HEVC_HME_4X);

    if (generic_state->b16xme_supported) {
        gen9_hevc_kernel_scaling(ctx, encode_state, encoder_context, HEVC_HME_16X);

        if (generic_state->b32xme_supported)
            gen9_hevc_kernel_scaling(ctx, encode_state, encoder_context, HEVC_HME_32X);
    }
}

// ME implementation

static void
gen9_hevc_me_set_curbe(VADriverContextP ctx,
                       struct encode_state *encode_state,
                       struct intel_encoder_context *encoder_context,
                       enum HEVC_HME_TYPE hme_type,
                       struct i965_gpe_context *gpe_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct generic_enc_codec_state *generic_state = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    gen9_hevc_me_curbe_data *cmd = NULL;
    VAEncPictureParameterBufferHEVC *pic_param = NULL;
    VAEncSliceParameterBufferHEVC *slice_param = NULL;
    unsigned int use_mv_from_prev_step = 0;
    unsigned int write_distortions = 0;
    unsigned int slice_qp = 0;
    unsigned int me_method = 0;
    unsigned int mv_shift_factor = 0, prev_mv_read_pos_factor = 0;
    unsigned int downscaled_width_in_mb, downscaled_height_in_mb;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    generic_state = (struct generic_enc_codec_state *)vme_context->generic_enc_state;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;
    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;
    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[0]->buffer;

    switch (hme_type) {
    case HEVC_HME_4X :
        use_mv_from_prev_step = (generic_state->b16xme_enabled) ? 1 : 0;;
        write_distortions = 1;
        mv_shift_factor = 2;
        prev_mv_read_pos_factor = 0;
        downscaled_width_in_mb = ALIGN(priv_state->picture_width / 4, 16) / 16;
        downscaled_height_in_mb = ALIGN(priv_state->picture_height / 4, 16) / 16;
        break;
    case HEVC_HME_16X :
        use_mv_from_prev_step = (generic_state->b32xme_enabled) ? 1 : 0;
        write_distortions = 0;
        mv_shift_factor = 2;
        prev_mv_read_pos_factor = 1;
        downscaled_width_in_mb = ALIGN(priv_state->picture_width / 16, 16) / 16;
        downscaled_height_in_mb = ALIGN(priv_state->picture_height / 16, 16) / 16;
        break;
    case HEVC_HME_32X :
        use_mv_from_prev_step = 0;
        write_distortions = 0;
        mv_shift_factor = 1;
        prev_mv_read_pos_factor = 0;
        downscaled_width_in_mb = ALIGN(priv_state->picture_width / 32, 16) / 16;
        downscaled_height_in_mb = ALIGN(priv_state->picture_height / 32, 16) / 16;
        break;
    default:
        return;
    }

    me_method = GEN9_HEVC_ME_METHOD[priv_state->tu_mode];
    slice_qp = pic_param->pic_init_qp + slice_param->slice_qp_delta;

    cmd = i965_gpe_context_map_curbe(gpe_context);
    if (!cmd)
        return;

    memcpy((void *)cmd, GEN9_HEVC_ME_INIT_CURBE_DATA, sizeof(gen9_hevc_me_curbe_data));

    cmd->dw3.sub_pel_mode = 3;
    cmd->dw4.picture_height_minus1 = downscaled_height_in_mb - 1;
    cmd->dw4.picture_width = downscaled_width_in_mb;
    cmd->dw5.qp_prime_y = slice_qp;
    cmd->dw6.use_mv_from_prev_step = use_mv_from_prev_step;
    cmd->dw6.write_distortions = write_distortions;
    cmd->dw6.super_combine_dist = GEN9_HEVC_SUPER_COMBINE_DIST[priv_state->tu_mode];
    cmd->dw6.max_vmvr = 512;

    if (priv_state->picture_coding_type != HEVC_SLICE_I) {
        cmd->dw13.num_ref_idx_l0_minus1 = slice_param->num_ref_idx_l0_active_minus1;
        if (priv_state->picture_coding_type == HEVC_SLICE_B) {
            cmd->dw1.bi_weight = 32;
            cmd->dw13.num_ref_idx_l1_minus1 = slice_param->num_ref_idx_l1_active_minus1;
        }
    }

    cmd->dw15.prev_mv_read_pos_factor = prev_mv_read_pos_factor;
    cmd->dw15.mv_shift_factor = mv_shift_factor;

    memcpy(&cmd->dw16, table_enc_search_path[GEN9_HEVC_ENC_MEMETHOD_TABLE][me_method], 14 * sizeof(unsigned int));

    cmd->dw32._4x_memv_output_data_surf_index = GEN9_HEVC_ME_MV_DATA_SURFACE_INDEX;
    cmd->dw33._16x_32x_memv_input_data_surf_index = (hme_type == HEVC_HME_32X) ?
                                                    GEN9_HEVC_ME_32X_MV_DATA_SURFACE_INDEX : GEN9_HEVC_ME_16X_MV_DATA_SURFACE_INDEX;
    cmd->dw34._4x_me_output_dist_surf_index = GEN9_HEVC_ME_DISTORTION_SURFACE_INDEX;
    cmd->dw35._4x_me_output_brc_dist_surf_index = GEN9_HEVC_ME_BRC_DISTORTION_INDEX;
    cmd->dw36.vme_fwd_inter_pred_surf_index = GEN9_HEVC_ME_CURR_FOR_FWD_REF_INDEX;
    cmd->dw37.vme_bdw_inter_pred_surf_index = GEN9_HEVC_ME_CURR_FOR_BWD_REF_INDEX;

    i965_gpe_context_unmap_curbe(gpe_context);
}

static void
gen9_hevc_me_set_surfaces(VADriverContextP ctx,
                          struct encode_state *encode_state,
                          struct intel_encoder_context *encoder_context,
                          enum HEVC_HME_TYPE hme_type,
                          struct i965_gpe_context *gpe_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct generic_enc_codec_state *generic_state = NULL;
    VAEncSliceParameterBufferHEVC *slice_param = NULL;
    struct gen9_hevc_surface_priv *surface_priv = NULL;
    struct object_surface *obj_surface = NULL;
    int scaled_surf_id = VA_INVALID_SURFACE, surface_id = VA_INVALID_SURFACE;
    int i = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    generic_state = (struct generic_enc_codec_state *)vme_context->generic_enc_state;
    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[0]->buffer;

    switch (hme_type) {
    case HEVC_HME_4X:
        scaled_surf_id = HEVC_SCALED_SURF_4X_ID;
        gen9_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                       &priv_ctx->s4x_memv_data_buffer,
                                       1,
                                       I965_SURFACEFORMAT_R8_UNORM,
                                       GEN9_HEVC_ME_MV_DATA_SURFACE_INDEX);

        if (generic_state->b16xme_enabled)
            gen9_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                           &priv_ctx->s16x_memv_data_buffer,
                                           1,
                                           I965_SURFACEFORMAT_R8_UNORM,
                                           GEN9_HEVC_ME_16X_MV_DATA_SURFACE_INDEX);

        gen9_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                       &priv_ctx->res_brc_me_dist_buffer,
                                       1,
                                       I965_SURFACEFORMAT_R8_UNORM,
                                       GEN9_HEVC_ME_BRC_DISTORTION_INDEX);

        gen9_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                       &priv_ctx->s4x_memv_distortion_buffer,
                                       1,
                                       I965_SURFACEFORMAT_R8_UNORM,
                                       GEN9_HEVC_ME_DISTORTION_SURFACE_INDEX);
        break;
    case HEVC_HME_16X:
        scaled_surf_id = HEVC_SCALED_SURF_16X_ID;

        gen9_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                       &priv_ctx->s16x_memv_data_buffer,
                                       1,
                                       I965_SURFACEFORMAT_R8_UNORM,
                                       GEN9_HEVC_ME_MV_DATA_SURFACE_INDEX);

        if (generic_state->b32xme_enabled)
            gen9_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                           &priv_ctx->s32x_memv_data_buffer,
                                           1,
                                           I965_SURFACEFORMAT_R8_UNORM,
                                           GEN9_HEVC_ME_32X_MV_DATA_SURFACE_INDEX);
        else
            gen9_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                           &priv_ctx->s16x_memv_data_buffer,
                                           1,
                                           I965_SURFACEFORMAT_R8_UNORM,
                                           GEN9_HEVC_ME_16X_MV_DATA_SURFACE_INDEX);
        break;
    case HEVC_HME_32X:
        scaled_surf_id = HEVC_SCALED_SURF_32X_ID;
        gen9_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                       &priv_ctx->s32x_memv_data_buffer,
                                       1,
                                       I965_SURFACEFORMAT_R8_UNORM,
                                       GEN9_HEVC_ME_MV_DATA_SURFACE_INDEX);
        break;
    default:
        return;
    }

    obj_surface = encode_state->reconstructed_object;
    surface_priv = (struct gen9_hevc_surface_priv *)obj_surface->private_data;
    gen9_add_adv_gpe_surface(ctx, gpe_context,
                             surface_priv->scaled_surface_obj[scaled_surf_id],
                             GEN9_HEVC_ME_CURR_FOR_FWD_REF_INDEX);

    for (i = 0; i < slice_param->num_ref_idx_l0_active_minus1 + 1; i++) {
        surface_id = slice_param->ref_pic_list0[i].picture_id;
        obj_surface = SURFACE(surface_id);
        if (!obj_surface || !obj_surface->private_data)
            break;

        surface_priv = (struct gen9_hevc_surface_priv *)obj_surface->private_data;
        gen9_add_adv_gpe_surface(ctx, gpe_context,
                                 surface_priv->scaled_surface_obj[scaled_surf_id],
                                 GEN9_HEVC_ME_CURR_FOR_FWD_REF_INDEX + i * 2 + 1);
    }

    obj_surface = encode_state->reconstructed_object;
    surface_priv = (struct gen9_hevc_surface_priv *)obj_surface->private_data;
    gen9_add_adv_gpe_surface(ctx, gpe_context,
                             surface_priv->scaled_surface_obj[scaled_surf_id],
                             GEN9_HEVC_ME_CURR_FOR_BWD_REF_INDEX);

    for (i = 0; i < slice_param->num_ref_idx_l1_active_minus1 + 1; i++) {
        surface_id = slice_param->ref_pic_list1[i].picture_id;
        obj_surface = SURFACE(surface_id);
        if (!obj_surface || !obj_surface->private_data)
            break;

        surface_priv = (struct gen9_hevc_surface_priv *)obj_surface->private_data;
        gen9_add_adv_gpe_surface(ctx, gpe_context,
                                 surface_priv->scaled_surface_obj[scaled_surf_id],
                                 GEN9_HEVC_ME_CURR_FOR_BWD_REF_INDEX + i * 2 + 1);
    }
}

static void
gen9_hevc_kernel_me(VADriverContextP ctx,
                    struct encode_state *encode_state,
                    struct intel_encoder_context *encoder_context,
                    enum HEVC_HME_TYPE hme_type)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    struct gpe_media_object_walker_parameter param;
    struct hevc_enc_kernel_walker_parameter hevc_walker_param;
    unsigned int downscaled_width_in_mb, downscaled_height_in_mb;
    struct i965_gpe_context *gpe_context;
    int media_state = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    switch (hme_type) {
    case HEVC_HME_4X:
        media_state = HEVC_ENC_MEDIA_STATE_4X_ME;
        downscaled_width_in_mb = priv_state->downscaled_width_4x_in_mb;
        downscaled_height_in_mb = priv_state->downscaled_height_4x_in_mb;
        break;
    case HEVC_HME_16X:
        media_state = HEVC_ENC_MEDIA_STATE_16X_ME;
        downscaled_width_in_mb = priv_state->downscaled_width_16x_in_mb;
        downscaled_height_in_mb = priv_state->downscaled_height_16x_in_mb;
        break;
    case HEVC_HME_32X:
        media_state = HEVC_ENC_MEDIA_STATE_32X_ME;
        downscaled_width_in_mb = priv_state->downscaled_width_32x_in_mb;
        downscaled_height_in_mb = priv_state->downscaled_height_32x_in_mb;
        break;
    default:
        return;
    }

    if (priv_state->picture_coding_type == HEVC_SLICE_P)
        gpe_context = &priv_ctx->me_context.gpe_context[hme_type][HEVC_ENC_ME_P];
    else
        gpe_context = &priv_ctx->me_context.gpe_context[hme_type][HEVC_ENC_ME_B];

    gen8_gpe_context_init(ctx, gpe_context);
    gen9_gpe_reset_binding_table(ctx, gpe_context);
    gen9_hevc_me_set_curbe(ctx, encode_state, encoder_context, hme_type, gpe_context);
    gen9_hevc_me_set_surfaces(ctx, encode_state, encoder_context, hme_type, gpe_context);
    gen8_gpe_setup_interface_data(ctx, gpe_context);

    memset((void *)&hevc_walker_param, 0, sizeof(hevc_walker_param));
    hevc_walker_param.resolution_x = downscaled_width_in_mb;
    hevc_walker_param.resolution_y = downscaled_height_in_mb;
    hevc_walker_param.no_dependency = 1;
    gen9_hevc_init_object_walker(&hevc_walker_param, &param);

    gen9_hevc_run_object_walker(ctx, encoder_context, gpe_context, &param,
                                media_state);
}

static void
gen9_hevc_hme_encode_me(VADriverContextP ctx,
                        struct encode_state *encode_state,
                        struct intel_encoder_context *encoder_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct generic_enc_codec_state *generic_state = NULL;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    generic_state = (struct generic_enc_codec_state *)vme_context->generic_enc_state;

    if (generic_state->b16xme_enabled) {
        if (generic_state->b32xme_enabled)
            gen9_hevc_kernel_me(ctx, encode_state, encoder_context, HEVC_HME_32X);

        gen9_hevc_kernel_me(ctx, encode_state, encoder_context, HEVC_HME_16X);
    }

    gen9_hevc_kernel_me(ctx, encode_state, encoder_context, HEVC_HME_4X);
}

// MBENC kernels setting start

static unsigned char
map_44_lut_value(unsigned int value,
                 unsigned char max)
{
    unsigned int max_cost = 0;
    int data = 0;
    unsigned char ret = 0;

    if (value == 0)
        return 0;

    max_cost = ((max & 15) << (max >> 4));
    if (value >= max_cost)
        return max;

    data = (int)(log((double)value) / log(2.)) - 3;
    if (data < 0)
        data = 0;

    ret = (unsigned char)((data << 4) +
                          (int)((value + (data == 0 ? 0 : (1 << (data - 1)))) >> data));
    ret = (ret & 0xf) == 0 ? (ret | 8) : ret;

    return ret;
}

static void
gen9_hevc_mbenc_set_costs(struct gen9_hevc_encoder_context *priv_ctx,
                          int slice_type,
                          int intra_trans_type,
                          unsigned int slice_qp,
                          unsigned char *mode_cost,
                          unsigned char *mv_cost,
                          unsigned char *mode_cost_sp,
                          unsigned int *simplest_intra_inter_threshold)
{
    float had_bias = intra_trans_type == HEVC_ENC_INTRA_TRANS_HADAMARD ?
                     1.67f : 2.0f;
    double lambda_md, lambda_me;

    lambda_md = priv_ctx->lambda_md_table[slice_type][slice_qp];
    lambda_me = priv_ctx->lambda_md_table[slice_type][slice_qp];

    if (mode_cost) {
        mode_cost[0] = map_44_lut_value((unsigned int)(lambda_md * GEN9_HEVC_ENC_Mode_COST[slice_type][0] * had_bias), 0x6f);
        mode_cost[1] = map_44_lut_value((unsigned int)(lambda_md * GEN9_HEVC_ENC_Mode_COST[slice_type][1] * had_bias), 0x8f);
        mode_cost[2] = map_44_lut_value((unsigned int)(lambda_md * GEN9_HEVC_ENC_Mode_COST[slice_type][2] * had_bias), 0x8f);
        mode_cost[3] = map_44_lut_value((unsigned int)(lambda_md * GEN9_HEVC_ENC_Mode_COST[slice_type][3] * had_bias), 0x8f);
        mode_cost[4] = map_44_lut_value((unsigned int)(lambda_md * GEN9_HEVC_ENC_Mode_COST[slice_type][4] * had_bias), 0x8f);
        mode_cost[5] = map_44_lut_value((unsigned int)(lambda_md * GEN9_HEVC_ENC_Mode_COST[slice_type][5] * had_bias), 0x6f);
        mode_cost[6] = map_44_lut_value((unsigned int)(lambda_md * GEN9_HEVC_ENC_Mode_COST[slice_type][6] * had_bias), 0x6f);
        mode_cost[7] = map_44_lut_value((unsigned int)(lambda_md * GEN9_HEVC_ENC_Mode_COST[slice_type][7] * had_bias), 0x6f);
        mode_cost[8] = map_44_lut_value((unsigned int)(lambda_md * GEN9_HEVC_ENC_Mode_COST[slice_type][8] * had_bias), 0x8f);
        mode_cost[9] = map_44_lut_value((unsigned int)(lambda_md * GEN9_HEVC_ENC_Mode_COST[slice_type][9] * had_bias), 0x6f);
        mode_cost[10] = map_44_lut_value((unsigned int)(lambda_md * GEN9_HEVC_ENC_Mode_COST[slice_type][10] * had_bias), 0x6f);
        mode_cost[11] = map_44_lut_value((unsigned int)(lambda_md * GEN9_HEVC_ENC_Mode_COST[slice_type][11] * had_bias), 0x6f);
    }

    if (mv_cost) {
        mv_cost[0] = map_44_lut_value((unsigned int)(lambda_me * GEN9_HEVC_ENC_MV_COST[slice_type][0] * had_bias), 0x6f);
        mv_cost[1] = map_44_lut_value((unsigned int)(lambda_me * GEN9_HEVC_ENC_MV_COST[slice_type][1] * had_bias), 0x6f);
        mv_cost[2] = map_44_lut_value((unsigned int)(lambda_me * GEN9_HEVC_ENC_MV_COST[slice_type][2] * had_bias), 0x6f);
        mv_cost[3] = map_44_lut_value((unsigned int)(lambda_me * GEN9_HEVC_ENC_MV_COST[slice_type][3] * had_bias), 0x6f);
        mv_cost[4] = map_44_lut_value((unsigned int)(lambda_me * GEN9_HEVC_ENC_MV_COST[slice_type][4] * had_bias), 0x6f);
        mv_cost[5] = map_44_lut_value((unsigned int)(lambda_me * GEN9_HEVC_ENC_MV_COST[slice_type][5] * had_bias), 0x6f);
        mv_cost[6] = map_44_lut_value((unsigned int)(lambda_me * GEN9_HEVC_ENC_MV_COST[slice_type][6] * had_bias), 0x6f);
        mv_cost[7] = map_44_lut_value((unsigned int)(lambda_me * GEN9_HEVC_ENC_MV_COST[slice_type][7] * had_bias), 0x6f);
    }

    if (mode_cost_sp)
        *mode_cost_sp = map_44_lut_value((unsigned int)(lambda_md * 45 * had_bias), 0x8f);

    if (simplest_intra_inter_threshold) {
        lambda_md *= had_bias;
        *simplest_intra_inter_threshold = 0;
        if (GEN9_HEVC_ENC_Mode_COST[slice_type][1] < GEN9_HEVC_ENC_Mode_COST[slice_type][3])
            *simplest_intra_inter_threshold = (unsigned int)(lambda_md *
                                                             (GEN9_HEVC_ENC_Mode_COST[slice_type][3] - GEN9_HEVC_ENC_Mode_COST[slice_type][1]) + 0.5);
    }
}

static void
gen9_hevc_set_lambda_tables(struct gen9_hevc_encoder_context *priv_ctx,
                            int slice_type,
                            int intra_trans_type)
{
    if (slice_type != HEVC_SLICE_I) {
        if (priv_ctx->lambda_table_inited)
            return;

        memcpy((void *)&priv_ctx->lambda_me_table[slice_type], &GEN9_HEVC_ENC_QP_LAMBDA_ME[slice_type],
               sizeof(GEN9_HEVC_ENC_QP_LAMBDA_ME[slice_type]));
        memcpy((void *)&priv_ctx->lambda_md_table[slice_type], &GEN9_HEVC_ENC_QP_LAMBDA_ME[slice_type],
               sizeof(GEN9_HEVC_ENC_QP_LAMBDA_ME[slice_type]));
    } else if (intra_trans_type != priv_ctx->lambda_intra_trans_type ||
               !priv_ctx->lambda_table_inited) {
        double temp = 0.0;
        double lambda = 0.0;
        int qp = 0;

        for (qp = 0; qp < 52; qp++) {
            temp = (double)qp - 12;
            lambda = 0.85 * pow(2.0, temp / 3.0);

            if ((intra_trans_type != HEVC_ENC_INTRA_TRANS_HAAR) &&
                (intra_trans_type != HEVC_ENC_INTRA_TRANS_HADAMARD))
                lambda *= 0.95;

            priv_ctx->lambda_md_table[slice_type][qp] =
                priv_ctx->lambda_me_table[slice_type][qp] = sqrt(lambda);
        }

        priv_ctx->lambda_intra_trans_type = intra_trans_type;
    }
}

static void
gen9_hevc_lambda_tables_init(struct gen9_hevc_encoder_context *priv_ctx)
{
    gen9_hevc_set_lambda_tables(priv_ctx, HEVC_SLICE_B, HEVC_ENC_INTRA_TRANS_HAAR);
    gen9_hevc_set_lambda_tables(priv_ctx, HEVC_SLICE_P, HEVC_ENC_INTRA_TRANS_HAAR);
    gen9_hevc_set_lambda_tables(priv_ctx, HEVC_SLICE_I, HEVC_ENC_INTRA_TRANS_HAAR);

    priv_ctx->lambda_table_inited = 1;
}

static void
gen9_hevc_8x8_b_pak_set_curbe(VADriverContextP ctx,
                              struct encode_state *encode_state,
                              struct intel_encoder_context *encoder_context,
                              struct i965_gpe_context *gpe_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct generic_enc_codec_state *generic_state = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    gen9_hevc_mbenc_b_pak_curbe_data *cmd = NULL;
    VAEncPictureParameterBufferHEVC *pic_param = NULL;
    VAEncSliceParameterBufferHEVC *slice_param = NULL;
    unsigned int slice_qp = 0;
    int bti_idx = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    generic_state = (struct generic_enc_codec_state *)vme_context->generic_enc_state;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;
    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;
    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[0]->buffer;

    slice_qp = pic_param->pic_init_qp + slice_param->slice_qp_delta;
    cmd = i965_gpe_context_map_curbe(gpe_context);
    if (!cmd)
        return;

    memset((void *)cmd, 0, sizeof(*cmd));

    cmd->dw0.frame_width = ALIGN(priv_state->picture_width, 16);
    cmd->dw0.frame_height = ALIGN(priv_state->picture_height, 16);
    cmd->dw1.max_vmvr = 511 * 4;
    cmd->dw1.qp = slice_qp;
    cmd->dw2.brc_enable = generic_state->brc_enabled;
    cmd->dw2.lcu_brc_enable = priv_state->lcu_brc_enabled;
    cmd->dw2.screen_content = !!pic_param->pic_fields.bits.screen_content_flag;
    cmd->dw2.slice_type = priv_state->picture_coding_type;
    cmd->dw2.roi_enable = (priv_state->num_roi > 0);
    cmd->dw2.fast_surveillance_flag = priv_state->picture_coding_type == HEVC_SLICE_I ?
                                      0 : priv_state->video_surveillance_flag;
    cmd->dw2.kbl_control_flag = (IS_KBL(i965->intel.device_info) || IS_GLK(i965->intel.device_info));
    cmd->dw2.enable_rolling_intra = priv_state->rolling_intra_refresh;
    cmd->dw2.simplest_intra_enable = (priv_state->tu_mode == HEVC_TU_BEST_SPEED);
    cmd->dw3.widi_intra_refresh_qp_delta = priv_state->widi_intra_refresh_qp_delta;
    cmd->dw3.widi_intra_refresh_mb_num = priv_state->widi_intra_insertion_location;
    cmd->dw3.widi_intra_refresh_unit_in_mb = priv_state->widi_intra_insertion_size;

    cmd->dw16.bti_cu_record = bti_idx++;
    cmd->dw17.bti_pak_obj = bti_idx++;
    cmd->dw18.bti_slice_map = bti_idx++;
    cmd->dw19.bti_brc_input = bti_idx++;
    cmd->dw20.bti_lcu_qp = bti_idx++;
    cmd->dw21.bti_brc_data = bti_idx++;
    cmd->dw22.bti_mb_data = bti_idx++;
    cmd->dw23.bti_mvp_surface = bti_idx++;
    cmd->dw24.bti_debug = bti_idx++;

    i965_gpe_context_unmap_curbe(gpe_context);
}

static void
gen9_hevc_8x8_b_pak_set_surfaces(VADriverContextP ctx,
                                 struct encode_state *encode_state,
                                 struct intel_encoder_context *encoder_context,
                                 struct i965_gpe_context *gpe_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    int bti_idx = 0;
    int size = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    size = priv_state->width_in_cu * priv_state->height_in_cu *
           priv_state->cu_record_size;
    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_CU_RECORD, bti_idx++,
                                 0, size, priv_state->mb_data_offset, NULL, NULL);

    size = priv_state->mb_data_offset;
    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_HCP_PAK, bti_idx++,
                                 0, size, 0, NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_SLICE_MAP, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_BRC_INPUT, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_LCU_QP, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_BRC_DATA, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_MB_MV_INDEX, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_MVP_INDEX, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_KERNEL_DEBUG, bti_idx++,
                                 0, 0, 0, NULL, NULL);
}

static void
gen9_hevc_8x8_b_pak(VADriverContextP ctx,
                    struct encode_state *encode_state,
                    struct intel_encoder_context *encoder_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    struct gpe_media_object_walker_parameter param;
    struct hevc_enc_kernel_walker_parameter hevc_walker_param;
    struct i965_gpe_context *gpe_context = NULL;
    int media_state = HEVC_ENC_MEDIA_STATE_HEVC_B_PAK;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    gpe_context = &priv_ctx->mbenc_context.gpe_contexts[HEVC_MBENC_BPAK_IDX];

    gen8_gpe_context_init(ctx, gpe_context);
    gen9_gpe_reset_binding_table(ctx, gpe_context);
    gen9_hevc_8x8_b_pak_set_curbe(ctx, encode_state, encoder_context, gpe_context);
    gen9_hevc_8x8_b_pak_set_surfaces(ctx, encode_state, encoder_context, gpe_context);
    gen8_gpe_setup_interface_data(ctx, gpe_context);

    memset((void *)&hevc_walker_param, 0, sizeof(hevc_walker_param));
    hevc_walker_param.resolution_x = ALIGN(priv_state->picture_width, 32) >> 5;
    hevc_walker_param.resolution_y = ALIGN(priv_state->picture_height, 32) >> 5;
    hevc_walker_param.no_dependency = 1;
    gen9_hevc_init_object_walker(&hevc_walker_param, &param);
    gen9_hevc_run_object_walker(ctx, encoder_context, gpe_context, &param,
                                media_state);
}

static const unsigned char ftq_25i[27] = {
    0, 0, 0, 0,
    1, 3, 6, 8, 11,
    13, 16, 19, 22, 26,
    30, 34, 39, 44, 50,
    56, 62, 69, 77, 85,
    94, 104, 115
};

static void
gen9_hevc_set_forward_coeff_thd(unsigned char *pcoeff,
                                int qp)
{
    int idx = (qp + 1) >> 1;

    memset((void *)pcoeff, ftq_25i[idx], 7);
}

static int
gen9_hevc_get_qp_from_ref_list(VADriverContextP ctx,
                               VAEncSliceParameterBufferHEVC *slice_param,
                               int list_idx,
                               int ref_frame_idx)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct gen9_hevc_surface_priv *hevc_priv_surface = NULL;
    struct object_surface *obj_surface = NULL;
    VASurfaceID surface_id;

    if (list_idx == 0) {
        if (ref_frame_idx < slice_param->num_ref_idx_l0_active_minus1 + 1)
            surface_id = slice_param->ref_pic_list0[ref_frame_idx].picture_id;
        else
            goto FAIL;
    } else {
        if (ref_frame_idx < slice_param->num_ref_idx_l1_active_minus1 + 1)
            surface_id = slice_param->ref_pic_list1[ref_frame_idx].picture_id;
        else
            goto FAIL;
    }

    obj_surface = SURFACE(surface_id);
    if (obj_surface && obj_surface->private_data) {
        hevc_priv_surface = obj_surface->private_data;
        return hevc_priv_surface->qp_value;
    }

FAIL:
    return 0;
}

static short
gen9_hevc_get_poc_diff_from_ref_list(VAEncPictureParameterBufferHEVC *pic_param,
                                     VAEncSliceParameterBufferHEVC *slice_param,
                                     int list_idx,
                                     int ref_frame_idx)
{
    short poc_diff = 0;

    if (list_idx == 0 &&
        ref_frame_idx < slice_param->num_ref_idx_l0_active_minus1 + 1)
        poc_diff = pic_param->decoded_curr_pic.pic_order_cnt -
                   slice_param->ref_pic_list0[ref_frame_idx].pic_order_cnt;
    else if (list_idx == 1 &&
             ref_frame_idx < slice_param->num_ref_idx_l1_active_minus1 + 1)
        poc_diff = pic_param->decoded_curr_pic.pic_order_cnt -
                   slice_param->ref_pic_list1[ref_frame_idx].pic_order_cnt;

    return CLAMP(-128, 127, poc_diff);
}

static void
gen9_hevc_set_control_region(VADriverContextP ctx,
                             struct encode_state *encode_state,
                             struct intel_encoder_context *encoder_context,
                             struct i965_gpe_context *gpe_context,
                             struct gen9_hevc_walking_pattern_parameter *param)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    VAEncSliceParameterBufferHEVC *slice_param = NULL;
    gen9_hevc_mbenc_control_region *p_region = NULL;
    unsigned int slice, num_regions, height, num_slices, num_units_in_region;
    unsigned int frame_width_in_units, frame_height_in_units;
    unsigned short region_start_table[64];
    unsigned int offset_to_the_region_start[16];
    unsigned short temp_data[32][32];
    int is_arbitrary_slices = 0;
    int slice_start_y[I965_MAX_NUM_SLICE + 1];
    int max_height;
    int k = 0, i = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    memset(slice_start_y, 0, sizeof(int) * (I965_MAX_NUM_SLICE + 1));
    memset(region_start_table, 0, sizeof(region_start_table));
    memset(temp_data, 0, sizeof(temp_data));
    memset(offset_to_the_region_start, 0, sizeof(offset_to_the_region_start));

    if (priv_state->num_regions_in_slice < 1)
        priv_state->num_regions_in_slice = 1;

    if (priv_state->num_regions_in_slice > 16)
        priv_state->num_regions_in_slice = 16;

    if (priv_state->walking_pattern_26) {
        frame_width_in_units = ALIGN(priv_state->picture_width, 16) / 16;
        frame_height_in_units = ALIGN(priv_state->picture_height, 16) / 16;
    } else {
        frame_width_in_units = ALIGN(priv_state->picture_width, 32) / 32;
        frame_height_in_units = ALIGN(priv_state->picture_height, 32) / 32;
    }

    for (slice = 0; slice < encode_state->num_slice_params_ext; slice++) {
        slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[slice]->buffer;
        if (slice_param->slice_segment_address %
            ALIGN(priv_state->picture_width, 32)) {
            is_arbitrary_slices = 1;
        } else {
            slice_start_y[slice] = slice_param->slice_segment_address /
                                   ALIGN(priv_state->picture_width, 32);

            if (priv_state->walking_pattern_26) {
                slice_start_y[slice] *= 2;
            }
        }
    }

    slice_start_y[encode_state->num_slice_params_ext] = frame_height_in_units;

    region_start_table[0] = 0;
    region_start_table[GEN9_HEVC_ENC_REGION_START_Y_OFFSET + 0] = 0;
    num_regions = 1;

    if (is_arbitrary_slices) {
        height = frame_height_in_units;
        num_slices = 1;
        max_height = height;
        if (priv_state->num_regions_in_slice > 1) {
            num_units_in_region =
                (frame_width_in_units + 2 * (frame_height_in_units - 1) + priv_state->num_regions_in_slice - 1) / priv_state->num_regions_in_slice;

            num_regions = priv_state->num_regions_in_slice;

            for (i = 1; i < priv_state->num_regions_in_slice; i++) {
                unsigned int front = i * num_units_in_region;

                if (front < frame_width_in_units)
                    region_start_table[i] = (unsigned int)front;
                else if (((front - frame_width_in_units + 1) & 1) == 0)
                    region_start_table[i] = (unsigned int)frame_width_in_units - 1;
                else
                    region_start_table[i] = (unsigned int)frame_width_in_units - 2;

                region_start_table[GEN9_HEVC_ENC_REGION_START_Y_OFFSET + i] = (unsigned int)((front - region_start_table[i]) >> 1);
            }
        }
    } else {
        int start_y;
        int slice_is_merged = 0;

        max_height = 0;
        num_slices = encode_state->num_slice_params_ext;

        for (slice = 0; slice < num_slices; slice++) {
            int sliceHeight = slice_start_y[slice + 1] - slice_start_y[slice];

            if (sliceHeight > max_height)
                max_height = sliceHeight;
        }

        while (!slice_is_merged) {
            int newNumSlices = 1;

            start_y = 0;

            for (slice = 1; slice < num_slices; slice++) {
                if ((slice_start_y[slice + 1] - start_y) <= max_height)
                    slice_start_y[slice] = -1;
                else
                    start_y = slice_start_y[slice];
            }

            for (slice = 1; slice < num_slices; slice++) {
                if (slice_start_y[slice] > 0) {
                    slice_start_y[newNumSlices] = slice_start_y[slice];
                    newNumSlices++;
                }
            }

            num_slices = newNumSlices;
            slice_start_y[num_slices] = frame_height_in_units;

            if (num_slices * priv_state->num_regions_in_slice <= GEN9_HEVC_MEDIA_WALKER_MAX_COLORS)
                slice_is_merged = 1;
            else {
                int num = 1;

                max_height = frame_height_in_units;

                for (slice = 0; slice < num_slices - 1; slice++) {
                    if ((slice_start_y[slice + 2] - slice_start_y[slice]) <= max_height) {
                        max_height = slice_start_y[slice + 2] - slice_start_y[slice];
                        num = slice + 1;
                    }
                }

                for (slice = num; slice < num_slices; slice++)
                    slice_start_y[slice] = slice_start_y[slice + 1];

                num_slices--;
            }
        }

        num_units_in_region = (frame_width_in_units + 2 * (max_height - 1) + priv_state->num_regions_in_slice - 1) /
                              priv_state->num_regions_in_slice;
        num_regions = num_slices * priv_state->num_regions_in_slice;

        for (slice = 0; slice < num_slices; slice++) {
            region_start_table[slice * priv_state->num_regions_in_slice] = 0;
            region_start_table[GEN9_HEVC_ENC_REGION_START_Y_OFFSET + (slice * priv_state->num_regions_in_slice)] = (unsigned int)slice_start_y[slice];

            for (i = 1; i < priv_state->num_regions_in_slice; i++) {
                int front = i * num_units_in_region;

                if ((unsigned int)front < frame_width_in_units)
                    region_start_table[slice * priv_state->num_regions_in_slice + i] = (unsigned int)front;
                else if (((front - frame_width_in_units + 1) & 1) == 0)
                    region_start_table[slice * priv_state->num_regions_in_slice + i] = (unsigned int)frame_width_in_units - 1;
                else
                    region_start_table[slice * priv_state->num_regions_in_slice + i] = (unsigned int)frame_width_in_units - 2;

                region_start_table[GEN9_HEVC_ENC_REGION_START_Y_OFFSET + (slice * priv_state->num_regions_in_slice + i)] = (unsigned int)slice_start_y[slice] +
                                                                                                                           ((front - region_start_table[i]) >> 1);
            }
        }
        height = max_height;
    }

    for (k = 0; k < num_slices; k++) {
        int i;
        int nearest_reg = 0;
        int min_delta = priv_state->picture_height;

        if (priv_state->walking_pattern_26) {
            int cur_lcu_pel_y = region_start_table[GEN9_HEVC_ENC_REGION_START_Y_OFFSET + (k * priv_state->num_regions_in_slice)] << 4;
            int ts_width = priv_state->picture_width >> 4;
            int ts_height = height;
            int offset_y = -((ts_width + 1) >> 1);
            int offset_delta = ((ts_width + ((ts_height - 1) << 1)) + (priv_state->num_regions_in_slice - 1)) / (priv_state->num_regions_in_slice);

            for (i = 0; i < (int)num_regions; i++) {
                if (region_start_table[i] == 0) {
                    int delta = cur_lcu_pel_y - (region_start_table[GEN9_HEVC_ENC_REGION_START_Y_OFFSET + i] << 4);

                    if (delta >= 0) {
                        if (delta < min_delta) {
                            min_delta = delta;
                            nearest_reg = i;
                        }
                    }
                }

                offset_to_the_region_start[k] = 2 * region_start_table[GEN9_HEVC_ENC_REGION_START_Y_OFFSET + nearest_reg];
            }

            for (i = 0; i < priv_state->num_regions_in_slice; i++) {
                int tmp_y = region_start_table[GEN9_HEVC_ENC_REGION_START_Y_OFFSET + (nearest_reg + priv_state->num_regions_in_slice)];

                temp_data[k * priv_state->num_regions_in_slice + i][0] = region_start_table[nearest_reg + i];
                temp_data[k * priv_state->num_regions_in_slice + i][1] = region_start_table[GEN9_HEVC_ENC_REGION_START_Y_OFFSET + (nearest_reg + i)];
                temp_data[k * priv_state->num_regions_in_slice + i][2] = region_start_table[GEN9_HEVC_ENC_REGION_START_Y_OFFSET + nearest_reg];
                temp_data[k * priv_state->num_regions_in_slice + i][3] = (unsigned int)((tmp_y != 0) ? tmp_y : (priv_state->picture_height) >> 4);
                temp_data[k * priv_state->num_regions_in_slice + i][4] = offset_to_the_region_start[k] & 0x0FFFF;
                temp_data[k * priv_state->num_regions_in_slice + i][5] = 0;
                temp_data[k * priv_state->num_regions_in_slice + i][6] = 0;
                temp_data[k * priv_state->num_regions_in_slice + i][7] = (unsigned int)(offset_y + region_start_table[GEN9_HEVC_ENC_REGION_START_Y_OFFSET + nearest_reg] + ((i * offset_delta) >> 1));
            }
        } else {
            int cur_lcu_pel_y = region_start_table[GEN9_HEVC_ENC_REGION_START_Y_OFFSET +
                                                   (k * priv_state->num_regions_in_slice)] << 5;
            int ts_width = (priv_state->picture_width + 16) >> 5;
            int ts_height = height;
            int offset_y = -4 * ((ts_width + 1) >> 1);
            int offset_delta = ((ts_width + ((ts_height - 1) << 1)) + (priv_state->num_regions_in_slice - 1)) / (priv_state->num_regions_in_slice);

            for (i = 0; i < (int)num_regions; i++) {
                if (region_start_table[i] == 0) {
                    int delta = cur_lcu_pel_y - (region_start_table[GEN9_HEVC_ENC_REGION_START_Y_OFFSET + i] << 5);

                    if (delta >= 0) {
                        if (delta < min_delta) {
                            min_delta = delta;
                            nearest_reg = i;
                        }
                    }
                }

                offset_to_the_region_start[k] = 2 * region_start_table[GEN9_HEVC_ENC_REGION_START_Y_OFFSET + nearest_reg];
            }

            for (i = 0; i < priv_state->num_regions_in_slice; i++) {
                int tmp_y = 2 * region_start_table[GEN9_HEVC_ENC_REGION_START_Y_OFFSET + (nearest_reg + priv_state->num_regions_in_slice)];

                temp_data[k * priv_state->num_regions_in_slice + i][0] = region_start_table[nearest_reg + i];
                temp_data[k * priv_state->num_regions_in_slice + i][1] = 2 * region_start_table[GEN9_HEVC_ENC_REGION_START_Y_OFFSET + (nearest_reg + i)];
                temp_data[k * priv_state->num_regions_in_slice + i][2] = 2 * region_start_table[GEN9_HEVC_ENC_REGION_START_Y_OFFSET + nearest_reg];
                temp_data[k * priv_state->num_regions_in_slice + i][3] = (unsigned int)((tmp_y != 0) ? tmp_y : (priv_state->picture_height) >> 4);
                temp_data[k * priv_state->num_regions_in_slice + i][4] = offset_to_the_region_start[k] & 0x0FFFF;
                temp_data[k * priv_state->num_regions_in_slice + i][5] = 0;
                temp_data[k * priv_state->num_regions_in_slice + i][6] = 0;
                temp_data[k * priv_state->num_regions_in_slice + i][7] = (unsigned int)(offset_y + 4 * region_start_table[GEN9_HEVC_ENC_REGION_START_Y_OFFSET + nearest_reg] + (4 * ((i * offset_delta) >> 1)));
            }
        }
    }

    if (priv_state->walking_pattern_26)
        gen9_hevc_init_object_walker_26(priv_state, gpe_context, &param->gpe_param,
                                        priv_state->num_regions_in_slice, max_height,
                                        priv_state->use_hw_scoreboard,
                                        priv_state->use_hw_non_stalling_scoreborad);
    else
        gen9_hevc_init_object_walker_26z(priv_state, gpe_context, &param->gpe_param,
                                         priv_state->num_regions_in_slice, max_height,
                                         priv_state->use_hw_scoreboard,
                                         priv_state->use_hw_non_stalling_scoreborad);

    p_region = (gen9_hevc_mbenc_control_region *)i965_map_gpe_resource(&priv_ctx->res_con_corrent_thread_buffer);
    if (!p_region)
        return;

    memset((void *)p_region, 0, sizeof(*p_region) * GEN9_HEVC_ENC_CONCURRENT_SURFACE_HEIGHT);

    for (i = 0; i < 1024 ; i += 64)
        memcpy(((unsigned char *)p_region) + i, (unsigned char *)temp_data[i / 64], 32);

    param->max_height_in_region = priv_state->walking_pattern_26 ? max_height : max_height * 2;;
    param->num_region = num_regions;
    param->num_units_in_region = (frame_width_in_units + 2 * (max_height - 1) + priv_state->num_regions_in_slice - 1) /
                                 priv_state->num_regions_in_slice;

    i965_unmap_gpe_resource(&priv_ctx->res_con_corrent_thread_buffer);
}

static const char hevc_qpc_table[22] = {
    29, 30, 31, 32, 32, 33, 34, 34, 35, 35, 36, 36, 37, 37, 37, 38, 38, 38, 39, 39, 39, 39
};

static void
gen9_hevc_8x8_b_mbenc_set_curbe(VADriverContextP ctx,
                                struct encode_state *encode_state,
                                struct intel_encoder_context *encoder_context,
                                struct i965_gpe_context *gpe_context,
                                struct gen9_hevc_walking_pattern_parameter *param)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct generic_enc_codec_state *generic_state = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    gen9_hevc_mbenc_b_mb_enc_curbe_data *cmd = NULL;
    VAEncSequenceParameterBufferHEVC *seq_param = NULL;
    VAEncPictureParameterBufferHEVC *pic_param = NULL;
    VAEncSliceParameterBufferHEVC *slice_param = NULL;
    unsigned int slice_qp = 0, slice_type = HEVC_SLICE_I;
    unsigned char mode_cost[12], mv_cost[8], mode_cost_sp;
    unsigned char forward_trans_thd[7];
    unsigned int simplest_intra_inter_threshold;
    int transform_8x8_mode_flag = 1;
    int qp_bd_offset_c, q_pi, qp_c;
    void *default_curbe_ptr = NULL;
    int default_curbe_size = 0;
    int max_sp_len = 57;
    int bti_idx = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    generic_state = (struct generic_enc_codec_state *)vme_context->generic_enc_state;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;
    seq_param = (VAEncSequenceParameterBufferHEVC *)encode_state->seq_param_ext->buffer;
    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;
    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[0]->buffer;

    slice_qp = pic_param->pic_init_qp + slice_param->slice_qp_delta;
    slice_type = priv_state->picture_coding_type;

    if (priv_state->tu_mode == HEVC_TU_BEST_SPEED) {
        gen9_hevc_set_lambda_tables(priv_ctx, slice_type,
                                    HEVC_ENC_INTRA_TRANS_HAAR);

        max_sp_len = 25;
    }

    gen9_hevc_mbenc_set_costs(priv_ctx, slice_type, HEVC_ENC_INTRA_TRANS_REGULAR, slice_qp,
                              mode_cost, mv_cost, &mode_cost_sp,
                              &simplest_intra_inter_threshold);

    gen9_hevc_set_forward_coeff_thd(forward_trans_thd, slice_qp);

    gen9_hevc_get_b_mbenc_default_curbe(priv_state->tu_mode,
                                        slice_type, &default_curbe_ptr,
                                        &default_curbe_size);

    cmd = i965_gpe_context_map_curbe(gpe_context);
    if (!cmd)
        return;

    memcpy((void *)cmd, default_curbe_ptr, default_curbe_size);

    cmd->dw0.adaptive_en = 1;
    cmd->dw0.t_8x8_flag_for_inter_en = transform_8x8_mode_flag;
    cmd->dw2.pic_width = priv_state->width_in_mb;
    cmd->dw2.len_sp = max_sp_len;
    cmd->dw3.src_access = 0;
    cmd->dw3.ref_access = 0;
    cmd->dw3.ft_enable = (HEVC_ENC_FTQ_BASED_SKIP[priv_state->tu_mode] >> 1) & 0x01;
    cmd->dw4.pic_height_minus1 = priv_state->height_in_mb - 1;
    cmd->dw4.hme_enable = !!generic_state->hme_enabled;
    cmd->dw4.slice_type = slice_type;
    cmd->dw4.use_actual_ref_qp_value = 0;
    cmd->dw6.frame_width = priv_state->width_in_mb * 16;
    cmd->dw6.frame_height = priv_state->height_in_mb * 16;
    cmd->dw7.intra_part_mask = 3;
    cmd->dw8.mode0_cost = mode_cost[0];
    cmd->dw8.mode1_cost = mode_cost[1];
    cmd->dw8.mode2_cost = mode_cost[2];
    cmd->dw8.mode3_cost = mode_cost[3];
    cmd->dw9.mode4_cost = mode_cost[4];
    cmd->dw9.mode5_cost = mode_cost[5];
    cmd->dw9.mode6_cost = mode_cost[6];
    cmd->dw9.mode7_cost = mode_cost[7];
    cmd->dw10.mode8_cost = mode_cost[8];
    cmd->dw10.mode9_cost = mode_cost[9];
    cmd->dw10.ref_id_cost = mode_cost[10];
    cmd->dw10.chroma_intra_mode_cost = mode_cost[11];
    cmd->dw11.mv0_cost = mv_cost[0];
    cmd->dw11.mv1_cost = mv_cost[1];
    cmd->dw11.mv2_cost = mv_cost[2];
    cmd->dw11.mv3_cost = mv_cost[3];
    cmd->dw12.mv4_cost = mv_cost[4];
    cmd->dw12.mv5_cost = mv_cost[5];
    cmd->dw12.mv6_cost = mv_cost[6];
    cmd->dw12.mv7_cost = mv_cost[7];

    cmd->dw13.qp_prime_y = slice_qp;
    qp_bd_offset_c = 6 * priv_state->bit_depth_chroma_minus8;
    q_pi = CLAMP(-qp_bd_offset_c, 51, slice_qp + pic_param->pps_cb_qp_offset);
    qp_c = (q_pi < 30) ? q_pi : hevc_qpc_table[q_pi - 30];
    cmd->dw13.qp_prime_cb = qp_c;
    q_pi = CLAMP(-qp_bd_offset_c, 51, slice_qp + pic_param->pps_cr_qp_offset);
    qp_c = (q_pi < 30) ? q_pi : hevc_qpc_table[q_pi - 30];
    cmd->dw13.qp_prime_cr = qp_c;

    cmd->dw14.sic_fwd_trans_coeff_thread_0 = forward_trans_thd[0];
    cmd->dw14.sic_fwd_trans_coeff_thread_1 = forward_trans_thd[1];
    cmd->dw14.sic_fwd_trans_coeff_thread_2 = forward_trans_thd[2];
    cmd->dw15.sic_fwd_trans_coeff_thread_3 = forward_trans_thd[3];
    cmd->dw15.sic_fwd_trans_coeff_thread_4 = forward_trans_thd[4];
    cmd->dw15.sic_fwd_trans_coeff_thread_5 = forward_trans_thd[5];
    cmd->dw15.sic_fwd_trans_coeff_thread_6 = forward_trans_thd[6];
    cmd->dw32.skip_val =
        HEVC_ENC_SKIPVAL_B[cmd->dw3.block_based_skip_enable][transform_8x8_mode_flag][slice_qp];

    if (priv_state->picture_coding_type == HEVC_SLICE_I)
        *(float *)&cmd->dw34.lambda_me = 0.0;
    else
        *(float *)&cmd->dw34.lambda_me = (float)priv_ctx->lambda_me_table[slice_type][slice_qp];

    cmd->dw35.mode_cost_sp = mode_cost_sp;
    cmd->dw35.simp_intra_inter_threashold = simplest_intra_inter_threshold;
    cmd->dw36.num_refidx_l0_minus_one = slice_param->num_ref_idx_l0_active_minus1;
    cmd->dw36.num_refidx_l1_minus_one = slice_param->num_ref_idx_l1_active_minus1;
    cmd->dw36.brc_enable = !!generic_state->brc_enabled;
    cmd->dw36.lcu_brc_enable = !!priv_state->lcu_brc_enabled;
    cmd->dw36.power_saving = priv_state->power_saving;
    cmd->dw36.roi_enable = (priv_state->num_roi > 0);
    cmd->dw36.fast_surveillance_flag = priv_state->picture_coding_type == HEVC_SLICE_I ?
                                       0 : priv_state->video_surveillance_flag;

    if (priv_state->picture_coding_type != HEVC_SLICE_I) {
        cmd->dw37.actual_qp_refid0_list0 = gen9_hevc_get_qp_from_ref_list(ctx, slice_param, 0, 0);
        cmd->dw37.actual_qp_refid1_list0 = gen9_hevc_get_qp_from_ref_list(ctx, slice_param, 0, 1);
        cmd->dw37.actual_qp_refid2_list0 = gen9_hevc_get_qp_from_ref_list(ctx, slice_param, 0, 2);
        cmd->dw37.actual_qp_refid3_list0 = gen9_hevc_get_qp_from_ref_list(ctx, slice_param, 0, 3);

        if (priv_state->picture_coding_type == HEVC_SLICE_B) {
            cmd->dw39.actual_qp_refid0_list1 = gen9_hevc_get_qp_from_ref_list(ctx, slice_param, 1, 0);
            cmd->dw39.actual_qp_refid1_list1 = gen9_hevc_get_qp_from_ref_list(ctx, slice_param, 1, 1);
        }
    }

    cmd->dw44.max_vmvr = 511 * 4;
    cmd->dw44.max_num_merge_candidates = slice_param->max_num_merge_cand;

    if (priv_state->picture_coding_type != HEVC_SLICE_I) {
        cmd->dw44.max_num_ref_list0 = cmd->dw36.num_refidx_l0_minus_one + 1;
        cmd->dw44.max_num_ref_list1 = cmd->dw36.num_refidx_l1_minus_one + 1;
        cmd->dw45.temporal_mvp_enable_flag = slice_param->slice_fields.bits.slice_temporal_mvp_enabled_flag;
        cmd->dw45.hme_combine_len_pslice = 8;
        if (priv_state->picture_coding_type == HEVC_SLICE_B)
            cmd->dw45.hme_combine_len_bslice = 8;
    }

    cmd->dw45.log2_parallel_merge_level = pic_param->log2_parallel_merge_level_minus2 + 2;
    cmd->dw46.log2_min_tu_size = seq_param->log2_min_transform_block_size_minus2 + 2;
    cmd->dw46.log2_max_tu_size = cmd->dw46.log2_min_tu_size + seq_param->log2_diff_max_min_transform_block_size;
    cmd->dw46.log2_min_cu_size = seq_param->log2_min_luma_coding_block_size_minus3 + 3;
    cmd->dw46.log2_max_cu_size = cmd->dw46.log2_min_cu_size + seq_param->log2_diff_max_min_luma_coding_block_size;
    cmd->dw47.num_regions_in_slice = priv_state->num_regions_in_slice;
    cmd->dw47.type_of_walking_pattern = priv_state->walking_pattern_26;
    cmd->dw47.chroma_flatness_check_flag = (priv_state->tu_mode == HEVC_TU_BEST_SPEED) ? 0 : 1;
    cmd->dw47.enable_intra_early_exit = (priv_state->tu_mode == HEVC_TU_RT_SPEED);
    cmd->dw47.skip_intra_krn_flag = (priv_state->tu_mode == HEVC_TU_BEST_SPEED);
    cmd->dw47.collocated_from_l0_flag = slice_param->slice_fields.bits.collocated_from_l0_flag;
    cmd->dw47.is_low_delay = priv_state->low_delay;
    cmd->dw47.screen_content_flag = !!pic_param->pic_fields.bits.screen_content_flag;
    cmd->dw47.multi_slice_flag = (encode_state->num_slice_params_ext > 1);
    cmd->dw47.arbitary_slice_flag = priv_state->arbitrary_num_mb_in_slice;
    cmd->dw47.num_region_minus1 = param->num_region - 1;

    if (priv_state->picture_coding_type != HEVC_SLICE_I) {
        cmd->dw48.current_td_l0_0 = gen9_hevc_get_poc_diff_from_ref_list(pic_param, slice_param, 0, 0);
        cmd->dw48.current_td_l0_1 = gen9_hevc_get_poc_diff_from_ref_list(pic_param, slice_param, 0, 1);
        cmd->dw49.current_td_l0_2 = gen9_hevc_get_poc_diff_from_ref_list(pic_param, slice_param, 0, 2);
        cmd->dw49.current_td_l0_3 = gen9_hevc_get_poc_diff_from_ref_list(pic_param, slice_param, 0, 3);

        if (priv_state->picture_coding_type == HEVC_SLICE_B) {
            cmd->dw50.current_td_l1_0 = gen9_hevc_get_poc_diff_from_ref_list(pic_param, slice_param, 1, 0);
            cmd->dw50.current_td_l1_1 = gen9_hevc_get_poc_diff_from_ref_list(pic_param, slice_param, 1, 1);
        }
    }

    cmd->dw52.num_of_units_in_region = param->num_units_in_region;
    cmd->dw52.max_height_in_region = param->max_height_in_region;

    if (priv_state->rolling_intra_refresh) {
        cmd->dw35.widi_intra_refresh_en = 1;
        cmd->dw35.widi_first_intra_refresh = priv_state->widi_first_intra_refresh;
        cmd->dw35.half_update_mixed_lcu = 0;
        cmd->dw35.enable_rolling_intra = 1;
        cmd->dw38.widi_num_frame_in_gob = priv_state->widi_frame_num_in_gob;
        cmd->dw38.widi_num_intra_refresh_off_frames = priv_state->widi_frame_num_without_intra_refresh;
        cmd->dw51.widi_intra_refresh_qp_delta = priv_state->widi_intra_refresh_qp_delta;
        cmd->dw51.widi_intra_refresh_mb_num = priv_state->widi_intra_insertion_location;
        cmd->dw51.widi_intra_refresh_unit_in_mb = priv_state->widi_intra_insertion_size;
        cmd->dw53.widi_intra_refresh_ref_height = 40;
        cmd->dw53.widi_intra_refresh_ref_width = 48;

        priv_state->widi_first_intra_refresh = 0;
        priv_state->widi_frame_num_without_intra_refresh = 0;
    } else if (priv_state->picture_coding_type != HEVC_SLICE_I)
        priv_state->widi_frame_num_without_intra_refresh++;

    cmd->dw56.bti_cu_record = bti_idx++;
    cmd->dw57.bti_pak_cmd = bti_idx++;
    cmd->dw58.bti_src_y = bti_idx++;
    bti_idx++;
    cmd->dw59.bti_intra_dist = bti_idx++;
    cmd->dw60.bti_min_dist = bti_idx++;
    cmd->dw61.bti_hme_mv_pred_fwd_bwd_surf_index = bti_idx++;
    cmd->dw62.bti_hme_dist_surf_index = bti_idx++;
    cmd->dw63.bti_slice_map = bti_idx++;
    cmd->dw64.bti_vme_saved_uni_sic = bti_idx++;
    cmd->dw65.bti_simplest_intra = bti_idx++;
    cmd->dw66.bti_collocated_refframe = bti_idx++;
    cmd->dw67.bti_reserved = bti_idx++;
    cmd->dw68.bti_brc_input = bti_idx++;
    cmd->dw69.bti_lcu_qp = bti_idx++;
    cmd->dw70.bti_brc_data = bti_idx++;
    cmd->dw71.bti_vme_inter_prediction_surf_index = bti_idx++;
    bti_idx += 16;

    if (priv_state->picture_coding_type == HEVC_SLICE_P) {
        cmd->dw72.bti_concurrent_thread_map = bti_idx++;
        cmd->dw73.bti_mb_data_cur_frame = bti_idx++;
        cmd->dw74.bti_mvp_cur_frame = bti_idx++;
        cmd->dw75.bti_debug = bti_idx++;
    } else {
        cmd->dw72.bti_vme_inter_prediction_b_surf_index = bti_idx++;
        bti_idx += 8;

        cmd->dw73.bti_concurrent_thread_map = bti_idx++;
        cmd->dw74.bti_mb_data_cur_frame = bti_idx++;
        cmd->dw75.bti_mvp_cur_frame = bti_idx++;
        cmd->dw76.bti_debug = bti_idx++;
    }

    i965_gpe_context_unmap_curbe(gpe_context);
}

static void
gen9_hevc_8x8_b_mbenc_set_surfaces(VADriverContextP ctx,
                                   struct encode_state *encode_state,
                                   struct intel_encoder_context *encoder_context,
                                   struct i965_gpe_context *gpe_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    VAEncSliceParameterBufferHEVC *slice_param = NULL;
    VAEncPictureParameterBufferHEVC *pic_param = NULL;
    struct gen9_hevc_surface_priv *surface_priv = NULL;
    struct object_surface *obj_surface = NULL;
    dri_bo *collocated_mv_temporal_bo = NULL;
    int bti_idx = 0;
    int size = 0;
    int i = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;
    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;
    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[0]->buffer;

    if (priv_state->picture_coding_type != HEVC_SLICE_I &&
        slice_param->slice_fields.bits.slice_temporal_mvp_enabled_flag &&
        slice_param->slice_fields.bits.collocated_from_l0_flag) {
        if (pic_param->collocated_ref_pic_index != 0xff &&
            pic_param->collocated_ref_pic_index < GEN9_MAX_REF_SURFACES) {
            VASurfaceID idx = VA_INVALID_SURFACE;

            idx = pic_param->reference_frames[pic_param->collocated_ref_pic_index].picture_id;
            obj_surface = SURFACE(idx);
            if (obj_surface) {
                surface_priv = (struct gen9_hevc_surface_priv *)obj_surface->private_data;
                if (surface_priv)
                    collocated_mv_temporal_bo = surface_priv->motion_vector_temporal_bo;
            }
        }
    }

    size = priv_state->width_in_cu * priv_state->height_in_cu *
           priv_state->cu_record_size;
    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_CU_RECORD, bti_idx++,
                                 0, size, priv_state->mb_data_offset, NULL, NULL);

    size = priv_state->mb_data_offset;
    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_HCP_PAK, bti_idx++,
                                 0, size, 0, NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_RAW_Y_UV, bti_idx++,
                                 1, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);
    bti_idx++;

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_INTRA_DIST, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_MIN_DIST, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_HME_MVP, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_HME_DIST, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_SLICE_MAP, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_VME_UNI_SIC_DATA, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_SIMPLIFIED_INTRA, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    if (collocated_mv_temporal_bo)
        gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                     HEVC_ENC_SURFACE_COL_MB_MV, bti_idx++,
                                     0, 0, 0, NULL, collocated_mv_temporal_bo);
    else
        bti_idx++;

    bti_idx++;

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_BRC_INPUT, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_LCU_QP, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_BRC_DATA, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_adv_surface(ctx, priv_ctx, gpe_context,
                                  HEVC_ENC_SURFACE_RAW_VME, bti_idx++,
                                  NULL);

    for (i = 0; i < 8; i++) {
        if (i <= slice_param->num_ref_idx_l0_active_minus1)
            obj_surface = SURFACE(slice_param->ref_pic_list0[i].picture_id);
        else
            obj_surface = NULL;

        if (obj_surface) {
            surface_priv = (struct gen9_hevc_surface_priv *)obj_surface->private_data;

            gen9_hevc_set_gpe_adv_surface(ctx, priv_ctx, gpe_context,
                                          HEVC_ENC_SURFACE_REF_FRAME_VME, bti_idx++,
                                          surface_priv->surface_reff);
        } else
            bti_idx++;

        if (i <= slice_param->num_ref_idx_l1_active_minus1)
            obj_surface = SURFACE(slice_param->ref_pic_list1[i].picture_id);
        else
            obj_surface = NULL;

        if (obj_surface) {
            surface_priv = (struct gen9_hevc_surface_priv *)obj_surface->private_data;

            gen9_hevc_set_gpe_adv_surface(ctx, priv_ctx, gpe_context,
                                          HEVC_ENC_SURFACE_REF_FRAME_VME, bti_idx++,
                                          surface_priv->surface_reff);
        } else
            bti_idx++;
    }

    if (priv_state->picture_coding_type != HEVC_SLICE_P) {
        gen9_hevc_set_gpe_adv_surface(ctx, priv_ctx, gpe_context,
                                      HEVC_ENC_SURFACE_RAW_VME, bti_idx++,
                                      NULL);

        for (i = 0; i < 4; i++) {
            if (i <= slice_param->num_ref_idx_l1_active_minus1)
                obj_surface = SURFACE(slice_param->ref_pic_list1[i].picture_id);
            else
                obj_surface = NULL;

            if (obj_surface) {
                surface_priv = (struct gen9_hevc_surface_priv *)obj_surface->private_data;

                gen9_hevc_set_gpe_adv_surface(ctx, priv_ctx, gpe_context,
                                              HEVC_ENC_SURFACE_REF_FRAME_VME, bti_idx++,
                                              surface_priv->surface_reff);
            } else
                bti_idx++;

            bti_idx++;
        }
    }

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_CONCURRENT_THREAD, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_MB_MV_INDEX, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_MVP_INDEX, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_KERNEL_DEBUG, bti_idx++,
                                 0, 0, 0, NULL, NULL);
}

static void
gen9_hevc_8x8_b_mbenc(VADriverContextP ctx,
                      struct encode_state *encode_state,
                      struct intel_encoder_context *encoder_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    struct gen9_hevc_walking_pattern_parameter param;
    struct i965_gpe_context *gpe_context = NULL;
    int media_state = HEVC_ENC_MEDIA_STATE_HEVC_B_MBENC;
    int gpe_idx = HEVC_MBENC_BENC_IDX;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    if (priv_state->picture_coding_type == HEVC_SLICE_P)
        gpe_idx = priv_state->rolling_intra_refresh ? HEVC_MBENC_P_WIDI_IDX : HEVC_MBENC_PENC_IDX;
    else if (priv_state->picture_coding_type != HEVC_SLICE_I)
        gpe_idx = priv_state->rolling_intra_refresh ? HEVC_MBENC_MBENC_WIDI_IDX : HEVC_MBENC_BENC_IDX;

    gpe_context = &priv_ctx->mbenc_context.gpe_contexts[gpe_idx];

    gen8_gpe_context_init(ctx, gpe_context);
    gen9_gpe_reset_binding_table(ctx, gpe_context);

    memset((void *)&param, 0, sizeof(param));
    gen9_hevc_set_control_region(ctx, encode_state, encoder_context, gpe_context, &param);

    gen9_hevc_8x8_b_mbenc_set_curbe(ctx, encode_state, encoder_context, gpe_context, &param);
    gen9_hevc_8x8_b_mbenc_set_surfaces(ctx, encode_state, encoder_context, gpe_context);
    gen8_gpe_setup_interface_data(ctx, gpe_context);

    param.gpe_param.color_count_minus1 = param.num_region - 1;
    gen9_hevc_run_object_walker(ctx, encoder_context, gpe_context, &param.gpe_param,
                                media_state);
}

static void
gen9_hevc_8x8_pu_fmode_set_curbe(VADriverContextP ctx,
                                 struct encode_state *encode_state,
                                 struct intel_encoder_context *encoder_context,
                                 struct i965_gpe_context *gpe_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct generic_enc_codec_state *generic_state = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    gen9_hevc_mbenc_8x8_pu_fmode_curbe_data *cmd = NULL;
    VAEncSequenceParameterBufferHEVC *seq_param = NULL;
    VAEncPictureParameterBufferHEVC *pic_param = NULL;
    VAEncSliceParameterBufferHEVC *slice_param = NULL;
    unsigned int slice_qp = 0;
    int bti_idx = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    generic_state = (struct generic_enc_codec_state *)vme_context->generic_enc_state;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;
    seq_param = (VAEncSequenceParameterBufferHEVC *)encode_state->seq_param_ext->buffer;
    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;
    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[0]->buffer;

    slice_qp = pic_param->pic_init_qp + slice_param->slice_qp_delta;

    cmd = i965_gpe_context_map_curbe(gpe_context);
    if (!cmd)
        return;

    memset((void *)cmd, 0, sizeof(*cmd));

    cmd->dw0.frame_width = ALIGN(priv_state->picture_width, 16);
    cmd->dw0.frame_height = ALIGN(priv_state->picture_height, 16);
    cmd->dw1.slice_type = priv_state->picture_coding_type;
    cmd->dw1.pu_type = 2;
    cmd->dw1.pak_reording_flag = priv_state->picture_coding_type == HEVC_SLICE_I ? 1 : 0;

    if (seq_param->log2_min_luma_coding_block_size_minus3 + 3 +
        seq_param->log2_diff_max_min_luma_coding_block_size == 6)
        cmd->dw1.lcu_type = 0;
    else
        cmd->dw1.lcu_type = 1;

    cmd->dw1.screen_content_flag = !!pic_param->pic_fields.bits.screen_content_flag;
    cmd->dw1.enable_intra_early_exit = priv_state->tu_mode == HEVC_TU_RT_SPEED ?
                                       (priv_state->picture_coding_type == HEVC_SLICE_I ? 0 : 1) : 0;
    cmd->dw1.enable_debug_dump = 0;
    cmd->dw1.brc_enable = generic_state->brc_enabled;
    cmd->dw1.lcu_brc_enable = priv_state->lcu_brc_enabled;
    cmd->dw1.roi_enable = (priv_state->num_roi > 0);
    cmd->dw1.fast_surveillance_flag = priv_state->picture_coding_type == HEVC_SLICE_I ?
                                      0 : priv_state->video_surveillance_flag;
    cmd->dw1.enable_rolling_intra = priv_state->rolling_intra_refresh;
    cmd->dw1.widi_intra_refresh_en = priv_state->rolling_intra_refresh;
    cmd->dw1.half_update_mixed_lcu = 0;
    cmd->dw2.luma_lambda = priv_state->fixed_point_lambda_for_luma;

    if (priv_state->picture_coding_type != HEVC_SLICE_I) {
        double lambda_md;
        float had_bias = 2.0f;

        lambda_md = priv_ctx->lambda_md_table[cmd->dw1.slice_type][slice_qp];
        lambda_md = lambda_md * had_bias;
        cmd->dw3.lambda_for_dist_calculation = (unsigned int)(lambda_md * (1 << 10));
    }

    cmd->dw4.mode_cost_for_8x8_pu_tu8 = 0;
    cmd->dw5.mode_cost_for_8x8_pu_tu4 = 0;
    cmd->dw6.satd_16x16_pu_threshold = MAX(200 * ((int)slice_qp - 12), 0);
    cmd->dw6.bias_factor_toward_8x8 = pic_param->pic_fields.bits.screen_content_flag ? 1024 : 1126 + 102;
    cmd->dw7.qp = slice_qp;
    cmd->dw7.qp_for_inter = 0;
    cmd->dw8.simplified_flag_for_inter = 0;
    cmd->dw8.kbl_control_flag = (IS_KBL(i965->intel.device_info) || IS_GLK(i965->intel.device_info));
    cmd->dw9.widi_intra_refresh_qp_delta = priv_state->widi_intra_refresh_qp_delta;
    cmd->dw9.widi_intra_refresh_mb_num = priv_state->widi_intra_insertion_location;
    cmd->dw9.widi_intra_refresh_unit_in_mb = priv_state->widi_intra_insertion_size;

    cmd->dw16.bti_pak_object = bti_idx++;
    cmd->dw17.bti_vme_8x8_mode = bti_idx++;
    cmd->dw18.bti_intra_mode = bti_idx++;
    cmd->dw19.bti_pak_command = bti_idx++;
    cmd->dw20.bti_slice_map = bti_idx++;
    cmd->dw21.bti_intra_dist = bti_idx++;
    cmd->dw22.bti_brc_input = bti_idx++;
    cmd->dw23.bti_simplest_intra = bti_idx++;
    cmd->dw24.bti_lcu_qp_surface = bti_idx++;
    cmd->dw25.bti_brc_data = bti_idx++;
    cmd->dw26.bti_debug = bti_idx++;

    i965_gpe_context_unmap_curbe(gpe_context);
}

static void
gen9_hevc_8x8_pu_fmode_set_surfaces(VADriverContextP ctx,
                                    struct encode_state *encode_state,
                                    struct intel_encoder_context *encoder_context,
                                    struct i965_gpe_context *gpe_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    int bti_idx = 0;
    int size = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    size = priv_state->width_in_cu * priv_state->height_in_cu *
           priv_state->cu_record_size;
    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_CU_RECORD, bti_idx++,
                                 0, size, priv_state->mb_data_offset, NULL, NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_VME_8x8, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_INTRA_MODE, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    size = priv_state->mb_data_offset;
    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_HCP_PAK, bti_idx++,
                                 0, size, 0, NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_SLICE_MAP, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_INTRA_DIST, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_BRC_INPUT, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_SIMPLIFIED_INTRA, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_LCU_QP, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_BRC_DATA, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_KERNEL_DEBUG, bti_idx++,
                                 0, 0, 0, NULL, NULL);
}

static void
gen9_hevc_8x8_pu_fmode(VADriverContextP ctx,
                       struct encode_state *encode_state,
                       struct intel_encoder_context *encoder_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    struct gpe_media_object_walker_parameter param;
    struct hevc_enc_kernel_walker_parameter hevc_walker_param;
    struct i965_gpe_context *gpe_context = NULL;
    int media_state = HEVC_ENC_MEDIA_STATE_8x8_PU_FMODE;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    gpe_context = &priv_ctx->mbenc_context.gpe_contexts[HEVC_MBENC_8x8FMODE_IDX];

    gen8_gpe_context_init(ctx, gpe_context);
    gen9_gpe_reset_binding_table(ctx, gpe_context);
    gen9_hevc_8x8_pu_fmode_set_curbe(ctx, encode_state, encoder_context, gpe_context);
    gen9_hevc_8x8_pu_fmode_set_surfaces(ctx, encode_state, encoder_context, gpe_context);
    gen8_gpe_setup_interface_data(ctx, gpe_context);

    memset((void *)&hevc_walker_param, 0, sizeof(hevc_walker_param));
    hevc_walker_param.resolution_x = priv_state->width_in_lcu;
    hevc_walker_param.resolution_y = priv_state->height_in_lcu;
    hevc_walker_param.no_dependency = 1;
    gen9_hevc_init_object_walker(&hevc_walker_param, &param);
    gen9_hevc_run_object_walker(ctx, encoder_context, gpe_context, &param,
                                media_state);
}

static void
gen9_hevc_8x8_pu_mode_set_curbe(VADriverContextP ctx,
                                struct encode_state *encode_state,
                                struct intel_encoder_context *encoder_context,
                                struct i965_gpe_context *gpe_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct generic_enc_codec_state *generic_state = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    gen9_hevc_mbenc_8x8_pu_curbe_data *cmd = NULL;
    VAEncSequenceParameterBufferHEVC *seq_param = NULL;
    VAEncPictureParameterBufferHEVC *pic_param = NULL;
    VAEncSliceParameterBufferHEVC *slice_param = NULL;
    int bti_idx = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    generic_state = (struct generic_enc_codec_state *)vme_context->generic_enc_state;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;
    seq_param = (VAEncSequenceParameterBufferHEVC *)encode_state->seq_param_ext->buffer;
    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;
    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[0]->buffer;

    cmd = i965_gpe_context_map_curbe(gpe_context);
    if (!cmd)
        return;

    memset((void *)cmd, 0, sizeof(*cmd));

    cmd->dw0.frame_width = ALIGN(priv_state->picture_width, 16);
    cmd->dw0.frame_height = ALIGN(priv_state->picture_height, 16);
    cmd->dw1.slice_type = priv_state->picture_coding_type;
    cmd->dw1.pu_type = 2;
    cmd->dw1.dc_filter_flag = 1;
    cmd->dw1.angle_refine_flag = 1;
    if (seq_param->log2_min_luma_coding_block_size_minus3 + 3 +
        seq_param->log2_diff_max_min_luma_coding_block_size == 6)
        cmd->dw1.lcu_type = 0;
    else
        cmd->dw1.lcu_type = 1;
    cmd->dw1.screen_content_flag = !!pic_param->pic_fields.bits.screen_content_flag;
    cmd->dw1.enable_intra_early_exit = priv_state->tu_mode == HEVC_TU_RT_SPEED ?
                                       (priv_state->picture_coding_type == HEVC_SLICE_I ? 0 : 1) : 0;
    cmd->dw1.enable_debug_dump = 0;
    cmd->dw1.brc_enable = generic_state->brc_enabled;
    cmd->dw1.lcu_brc_enable = priv_state->lcu_brc_enabled;
    cmd->dw1.roi_enable = (priv_state->num_roi > 0);
    cmd->dw1.fast_surveillance_flag = priv_state->picture_coding_type == HEVC_SLICE_I ?
                                      0 : priv_state->video_surveillance_flag;
    if (priv_state->rolling_intra_refresh) {
        cmd->dw1.enable_rolling_intra = 1;
        cmd->dw1.widi_intra_refresh_en = 1;
        cmd->dw1.half_update_mixed_lcu = 0;

        cmd->dw5.widi_intra_refresh_mb_num = priv_state->widi_intra_insertion_location;
        cmd->dw5.widi_intra_refresh_mb_num = priv_state->widi_intra_insertion_location;
        cmd->dw5.widi_intra_refresh_unit_in_mb = priv_state->widi_intra_insertion_size;

        cmd->dw1.qp_value = pic_param->pic_init_qp + slice_param->slice_qp_delta;
    }

    cmd->dw2.luma_lambda = priv_state->fixed_point_lambda_for_luma;
    cmd->dw3.chroma_lambda = priv_state->fixed_point_lambda_for_chroma;
    cmd->dw4.simplified_flag_for_inter = 0;
    cmd->dw4.harr_trans_form_flag = priv_state->picture_coding_type == HEVC_SLICE_I ? 0 : 1;

    cmd->dw8.bti_src_y = bti_idx++;
    bti_idx++;
    cmd->dw9.bti_slice_map = bti_idx++;
    cmd->dw10.bti_vme_8x8_mode = bti_idx++;
    cmd->dw11.bti_intra_mode = bti_idx++;
    cmd->dw12.bti_brc_input = bti_idx++;
    cmd->dw13.bti_simplest_intra = bti_idx++;
    cmd->dw14.bti_lcu_qp_surface = bti_idx++;
    cmd->dw15.bti_brc_data = bti_idx++;
    cmd->dw16.bti_debug = bti_idx++;

    i965_gpe_context_unmap_curbe(gpe_context);
}

static void
gen9_hevc_8x8_pu_mode_set_surfaces(VADriverContextP ctx,
                                   struct encode_state *encode_state,
                                   struct intel_encoder_context *encoder_context,
                                   struct i965_gpe_context *gpe_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    int bti_idx = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_RAW_Y_UV, bti_idx++,
                                 1, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);
    bti_idx++;

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_SLICE_MAP, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_VME_8x8, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_INTRA_MODE, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_BRC_INPUT, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_SIMPLIFIED_INTRA, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_LCU_QP, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_BRC_DATA, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_KERNEL_DEBUG, bti_idx++,
                                 0, 0, 0, NULL, NULL);
}

static void
gen9_hevc_8x8_pu_mode(VADriverContextP ctx,
                      struct encode_state *encode_state,
                      struct intel_encoder_context *encoder_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    struct gpe_media_object_walker_parameter param;
    struct hevc_enc_kernel_walker_parameter hevc_walker_param;
    struct i965_gpe_context *gpe_context = NULL;
    int media_state = HEVC_ENC_MEDIA_STATE_8x8_PU;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    gpe_context = &priv_ctx->mbenc_context.gpe_contexts[HEVC_MBENC_8x8PU_IDX];

    gen8_gpe_context_init(ctx, gpe_context);
    gen9_gpe_reset_binding_table(ctx, gpe_context);
    gen9_hevc_8x8_pu_mode_set_curbe(ctx, encode_state, encoder_context, gpe_context);
    gen9_hevc_8x8_pu_mode_set_surfaces(ctx, encode_state, encoder_context, gpe_context);
    gen8_gpe_setup_interface_data(ctx, gpe_context);

    memset((void *)&hevc_walker_param, 0, sizeof(hevc_walker_param));
    hevc_walker_param.resolution_x = ALIGN(priv_state->picture_width, 16) >> 3;
    hevc_walker_param.resolution_y = ALIGN(priv_state->picture_height, 16) >> 3;
    hevc_walker_param.no_dependency = 1;
    gen9_hevc_init_object_walker(&hevc_walker_param, &param);
    gen9_hevc_run_object_walker(ctx, encoder_context, gpe_context, &param,
                                media_state);
}

static void
gen9_hevc_16x16_pu_mode_set_curbe(VADriverContextP ctx,
                                  struct encode_state *encode_state,
                                  struct intel_encoder_context *encoder_context,
                                  struct i965_gpe_context *gpe_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct generic_enc_codec_state *generic_state = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    gen9_hevc_enc_16x16_pu_curbe_data *cmd = NULL;
    VAEncSequenceParameterBufferHEVC *seq_param = NULL;
    VAEncPictureParameterBufferHEVC *pic_param = NULL;
    VAEncSliceParameterBufferHEVC *slice_param = NULL;
    double squred_lambda, qp_lambda, lambda_scaling_factor;
    unsigned int slice_qp = 0, slice_type = HEVC_SLICE_I;
    unsigned int new_point_lambda_for_luma;
    unsigned char mode_cost[12];
    int bti_idx = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    generic_state = (struct generic_enc_codec_state *)vme_context->generic_enc_state;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;
    seq_param = (VAEncSequenceParameterBufferHEVC *)encode_state->seq_param_ext->buffer;
    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;
    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[0]->buffer;

    slice_qp = pic_param->pic_init_qp + slice_param->slice_qp_delta;
    slice_type = priv_state->picture_coding_type;

    lambda_scaling_factor = 0.46 + slice_qp - 22;
    if (lambda_scaling_factor < 0)
        lambda_scaling_factor = 0.46;
    else if (lambda_scaling_factor > 15)
        lambda_scaling_factor = 15;

    squred_lambda = lambda_scaling_factor * pow(2.0, ((double)slice_qp - 12.0) / 6);
    priv_state->fixed_point_lambda_for_luma = (unsigned int)(squred_lambda * (1 << 10));

    lambda_scaling_factor = 1.0;
    qp_lambda = priv_ctx->lambda_md_table[slice_type][slice_qp];
    squred_lambda = qp_lambda * qp_lambda;
    priv_state->fixed_point_lambda_for_chroma = (unsigned int)(lambda_scaling_factor * squred_lambda * (1 << 10));

    qp_lambda = sqrt(0.57 * pow(2.0, ((double)slice_qp - 12.0) / 3));
    squred_lambda = qp_lambda * qp_lambda;
    new_point_lambda_for_luma = (unsigned int)(squred_lambda * (1 << 10));

    gen9_hevc_mbenc_set_costs(priv_ctx, slice_type, HEVC_ENC_INTRA_TRANS_HAAR, slice_qp,
                              mode_cost, NULL, NULL, NULL);

    cmd = i965_gpe_context_map_curbe(gpe_context);
    if (!cmd)
        return;

    memset((void *)cmd, 0, sizeof(*cmd));

    cmd->dw0.frame_width = ALIGN(priv_state->picture_width, 16);
    cmd->dw0.frame_height = ALIGN(priv_state->picture_height, 16);
    cmd->dw1.log2_min_cu_size = seq_param->log2_min_luma_coding_block_size_minus3 + 3;
    cmd->dw1.log2_max_cu_size = cmd->dw1.log2_min_cu_size + seq_param->log2_diff_max_min_luma_coding_block_size;
    cmd->dw1.log2_min_tu_size = seq_param->log2_min_transform_block_size_minus2 + 2;
    cmd->dw1.slice_qp = slice_qp;
    cmd->dw2.fixed_point_lambda_pred_mode = priv_state->fixed_point_lambda_for_chroma;
    cmd->dw3.lambda_scaling_factor = 1;
    cmd->dw3.slice_type = slice_type;
    cmd->dw3.enable_intra_early_exit = priv_state->tu_mode == HEVC_TU_RT_SPEED ?
                                       (priv_state->picture_coding_type == HEVC_SLICE_I ? 0 : 1) : 0;
    cmd->dw3.brc_enable = !!generic_state->brc_enabled;
    cmd->dw3.lcu_brc_enable = !!priv_state->lcu_brc_enabled;
    cmd->dw3.roi_enable = (priv_state->num_roi > 0);
    cmd->dw3.fast_surveillance_flag = priv_state->picture_coding_type == HEVC_SLICE_I ?
                                      0 : priv_state->video_surveillance_flag;
    cmd->dw3.enable_rolling_intra = priv_state->rolling_intra_refresh;
    cmd->dw3.widi_intra_refresh_en = priv_state->rolling_intra_refresh;
    cmd->dw3.half_update_mixed_lcu = 0;
    cmd->dw4.penalty_for_intra_8x8_non_dc_pred_mode = 0;
    cmd->dw4.intra_compute_type = 1;
    cmd->dw4.avc_intra_8x8_mask = 0;
    cmd->dw4.intra_sad_adjust = 2;
    cmd->dw5.fixed_point_lambda_cu_mode_for_cost_calculation = new_point_lambda_for_luma;
    cmd->dw6.screen_content_flag = !!pic_param->pic_fields.bits.screen_content_flag;
    cmd->dw7.mode_cost_intra_non_pred = mode_cost[0];
    cmd->dw7.mode_cost_intra_16x16 = mode_cost[1];
    cmd->dw7.mode_cost_intra_8x8 = mode_cost[2];
    cmd->dw7.mode_cost_intra_4x4 = mode_cost[3];
    cmd->dw8.fixed_point_lambda_cu_mode_for_luma = priv_state->fixed_point_lambda_for_luma;

    if (priv_state->rolling_intra_refresh) {
        cmd->dw9.widi_intra_refresh_mb_num = priv_state->widi_intra_insertion_location;
        cmd->dw9.widi_intra_refresh_mb_num = priv_state->widi_intra_insertion_location;
        cmd->dw9.widi_intra_refresh_unit_in_mb = priv_state->widi_intra_insertion_size;
    }

    cmd->dw10.simplified_flag_for_inter = 0;
    cmd->dw10.haar_transform_mode = priv_state->picture_coding_type == HEVC_SLICE_I ? 0 : 1;

    cmd->dw16.bti_src_y = bti_idx++;
    bti_idx++;
    cmd->dw17.bti_sad_16x16_pu = bti_idx++;
    cmd->dw18.bti_pak_object = bti_idx++;
    cmd->dw19.bti_sad_32x32_pu_mode = bti_idx++;
    cmd->dw20.bti_vme_mode_8x8 = bti_idx++;
    cmd->dw21.bti_slice_map = bti_idx++;
    cmd->dw22.bti_vme_src = bti_idx++;
    cmd->dw23.bti_brc_input = bti_idx++;
    cmd->dw24.bti_simplest_intra = bti_idx++;
    cmd->dw25.bti_lcu_qp_surface = bti_idx++;
    cmd->dw26.bti_brc_data = bti_idx++;
    cmd->dw27.bti_debug = bti_idx++;

    i965_gpe_context_unmap_curbe(gpe_context);
}

static void
gen9_hevc_16x16_pu_mode_set_surfaces(VADriverContextP ctx,
                                     struct encode_state *encode_state,
                                     struct intel_encoder_context *encoder_context,
                                     struct i965_gpe_context *gpe_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    int bti_idx = 0;
    int size = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_RAW_Y_UV, bti_idx++,
                                 1, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);
    bti_idx++;

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_16x16PU_SAD, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    size = priv_state->width_in_cu * priv_state->height_in_cu *
           priv_state->cu_record_size;
    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_CU_RECORD, bti_idx++,
                                 0, size, priv_state->mb_data_offset, NULL, NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_32x32_PU_OUTPUT, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_VME_8x8, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_SLICE_MAP, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_adv_surface(ctx, priv_ctx, gpe_context,
                                  HEVC_ENC_SURFACE_RAW_VME, bti_idx++,
                                  NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_BRC_INPUT, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_SIMPLIFIED_INTRA, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_LCU_QP, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_BRC_DATA, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_KERNEL_DEBUG, bti_idx++,
                                 0, 0, 0, NULL, NULL);
}

static void
gen9_hevc_16x16_pu_mode(VADriverContextP ctx,
                        struct encode_state *encode_state,
                        struct intel_encoder_context *encoder_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    struct gpe_media_object_walker_parameter param;
    struct hevc_enc_kernel_walker_parameter hevc_walker_param;
    struct i965_gpe_context *gpe_context = NULL;
    int media_state = HEVC_ENC_MEDIA_STATE_16x16_PU_MODE_DECISION;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    gpe_context = &priv_ctx->mbenc_context.gpe_contexts[HEVC_MBENC_16x16MD_IDX];

    gen8_gpe_context_init(ctx, gpe_context);
    gen9_gpe_reset_binding_table(ctx, gpe_context);
    gen9_hevc_16x16_pu_mode_set_curbe(ctx, encode_state, encoder_context, gpe_context);
    gen9_hevc_16x16_pu_mode_set_surfaces(ctx, encode_state, encoder_context, gpe_context);
    gen8_gpe_setup_interface_data(ctx, gpe_context);

    memset((void *)&hevc_walker_param, 0, sizeof(hevc_walker_param));
    hevc_walker_param.resolution_x = ALIGN(priv_state->picture_width, 32) >> 5;
    hevc_walker_param.resolution_y = ALIGN(priv_state->picture_height, 32) >> 5;
    hevc_walker_param.no_dependency = 1;
    gen9_hevc_init_object_walker(&hevc_walker_param, &param);
    gen9_hevc_run_object_walker(ctx, encoder_context, gpe_context, &param,
                                media_state);
}

static void
gen9_hevc_16x16_sad_pu_comp_set_curbe(VADriverContextP ctx,
                                      struct encode_state *encode_state,
                                      struct intel_encoder_context *encoder_context,
                                      struct i965_gpe_context *gpe_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    gen9_hevc_mbenc_16x16_sad_curbe_data *cmd = NULL;
    VAEncSequenceParameterBufferHEVC *seq_param = NULL;
    int bti_idx = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;
    seq_param = (VAEncSequenceParameterBufferHEVC *)encode_state->seq_param_ext->buffer;

    cmd = i965_gpe_context_map_curbe(gpe_context);
    if (!cmd)
        return;

    memset((void *)cmd, 0, sizeof(*cmd));

    cmd->dw0.frame_width = ALIGN(priv_state->picture_width, 16);
    cmd->dw0.frame_height = ALIGN(priv_state->picture_height, 16);
    cmd->dw1.log2_min_cu_size = seq_param->log2_min_luma_coding_block_size_minus3 + 3;
    cmd->dw1.log2_max_cu_size = cmd->dw1.log2_min_cu_size + seq_param->log2_diff_max_min_luma_coding_block_size;
    cmd->dw1.log2_min_tu_size = seq_param->log2_min_transform_block_size_minus2 + 2;
    cmd->dw1.enable_intra_early_exit = priv_state->tu_mode == HEVC_TU_RT_SPEED ?
                                       (priv_state->picture_coding_type == HEVC_SLICE_I ? 0 : 1) : 0;
    cmd->dw2.sim_flag_for_inter = 0;
    cmd->dw2.slice_type = priv_state->picture_coding_type;
    cmd->dw2.fast_surveillance_flag = priv_state->picture_coding_type == HEVC_SLICE_I ?
                                      0 : priv_state->video_surveillance_flag;

    cmd->dw8.bti_src_y = bti_idx++;
    bti_idx++;
    cmd->dw9.bti_sad_16x16_pu_output = bti_idx++;
    cmd->dw10.bti_32x32_pu_mode_decision = bti_idx++;
    cmd->dw11.bti_slice_map = bti_idx++;
    cmd->dw12.bti_simplest_intra = bti_idx++;
    cmd->dw13.bti_debug = bti_idx++;

    i965_gpe_context_unmap_curbe(gpe_context);
}

static void
gen9_hevc_16x16_sad_pu_comp_set_surfaces(VADriverContextP ctx,
                                         struct encode_state *encode_state,
                                         struct intel_encoder_context *encoder_context,
                                         struct i965_gpe_context *gpe_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    int bti_idx = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_RAW_Y_UV, bti_idx++,
                                 1, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);
    bti_idx++;

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_16x16PU_SAD, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_32x32_PU_OUTPUT, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_SLICE_MAP, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_SIMPLIFIED_INTRA, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_KERNEL_DEBUG, bti_idx++,
                                 0, 0, 0, NULL, NULL);
}

static void
gen9_hevc_16x16_sad_pu_computation(VADriverContextP ctx,
                                   struct encode_state *encode_state,
                                   struct intel_encoder_context *encoder_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    struct gpe_media_object_walker_parameter param;
    struct hevc_enc_kernel_walker_parameter hevc_walker_param;
    struct i965_gpe_context *gpe_context = NULL;
    int media_state = HEVC_ENC_MEDIA_STATE_16x16_PU_SAD;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    gpe_context = &priv_ctx->mbenc_context.gpe_contexts[HEVC_MBENC_16x16SAD_IDX];

    gen8_gpe_context_init(ctx, gpe_context);
    gen9_gpe_reset_binding_table(ctx, gpe_context);
    gen9_hevc_16x16_sad_pu_comp_set_curbe(ctx, encode_state, encoder_context, gpe_context);
    gen9_hevc_16x16_sad_pu_comp_set_surfaces(ctx, encode_state, encoder_context, gpe_context);
    gen8_gpe_setup_interface_data(ctx, gpe_context);

    memset((void *)&hevc_walker_param, 0, sizeof(hevc_walker_param));
    hevc_walker_param.resolution_x = ALIGN(priv_state->picture_width, 16) >> 4;
    hevc_walker_param.resolution_y = ALIGN(priv_state->picture_height, 16) >> 4;
    hevc_walker_param.no_dependency = 1;
    gen9_hevc_init_object_walker(&hevc_walker_param, &param);
    gen9_hevc_run_object_walker(ctx, encoder_context, gpe_context, &param,
                                media_state);
}

static void
gen9_hevc_32x32_b_intra_set_curbe(VADriverContextP ctx,
                                  struct encode_state *encode_state,
                                  struct intel_encoder_context *encoder_context,
                                  struct i965_gpe_context *gpe_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct generic_enc_codec_state *generic_state = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    gen9_hevc_mbenc_b_32x32_pu_intra_curbe_data *cmd = NULL;
    VAEncSequenceParameterBufferHEVC *seq_param = NULL;
    int bti_idx = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    generic_state = (struct generic_enc_codec_state *)vme_context->generic_enc_state;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;
    seq_param = (VAEncSequenceParameterBufferHEVC *)encode_state->seq_param_ext->buffer;

    cmd = i965_gpe_context_map_curbe(gpe_context);
    if (!cmd)
        return;

    memset((void *)cmd, 0, sizeof(*cmd));

    cmd->dw0.frame_width = ALIGN(priv_state->picture_width, 16);
    cmd->dw0.frame_height = ALIGN(priv_state->picture_height, 16);
    cmd->dw1.enable_debug_dump = 0;
    cmd->dw1.enable_intra_early_exit = priv_state->tu_mode == HEVC_TU_RT_SPEED ? 1 : 0;
    cmd->dw1.flags = 0;
    cmd->dw1.log2_min_tu_size = seq_param->log2_min_transform_block_size_minus2 + 2;
    cmd->dw1.slice_type = priv_state->picture_coding_type;
    cmd->dw1.hme_enable = generic_state->hme_enabled;
    cmd->dw1.fast_surveillance_flag = priv_state->picture_coding_type == HEVC_SLICE_I ?
                                      0 : priv_state->video_surveillance_flag;

    cmd->dw2.qp_multiplier = 100;
    cmd->dw2.qp_value = 0;

    cmd->dw8.bti_per_32x32_pu_intra_checck = bti_idx++;
    cmd->dw9.bti_src_y = bti_idx++;
    bti_idx++;
    cmd->dw10.bti_src_y2x = bti_idx++;
    cmd->dw11.bti_slice_map = bti_idx++;
    cmd->dw12.bti_vme_y2x = bti_idx++;
    cmd->dw13.bti_simplest_intra = bti_idx++;
    cmd->dw14.bti_hme_mv_pred = bti_idx++;
    cmd->dw15.bti_hme_dist = bti_idx++;
    cmd->dw16.bti_lcu_skip = bti_idx++;
    cmd->dw17.bti_debug = bti_idx++;

    i965_gpe_context_unmap_curbe(gpe_context);
}

static void
gen9_hevc_32x32_b_intra_set_surfaces(VADriverContextP ctx,
                                     struct encode_state *encode_state,
                                     struct intel_encoder_context *encoder_context,
                                     struct i965_gpe_context *gpe_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    int bti_idx = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_32x32_PU_OUTPUT, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_RAW_Y_UV, bti_idx++,
                                 1, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);
    bti_idx++;

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_Y_2X, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_SLICE_MAP, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_adv_surface(ctx, priv_ctx, gpe_context,
                                  HEVC_ENC_SURFACE_Y_2X_VME, bti_idx++,
                                  NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_SIMPLIFIED_INTRA, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_HME_MVP, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_HME_DIST, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_LCU_QP, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_KERNEL_DEBUG, bti_idx,
                                 0, 0, 0, NULL, NULL);
}

static void
gen9_hevc_32x32_b_intra(VADriverContextP ctx,
                        struct encode_state *encode_state,
                        struct intel_encoder_context *encoder_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    struct gpe_media_object_walker_parameter param;
    struct hevc_enc_kernel_walker_parameter hevc_walker_param;
    struct i965_gpe_context *gpe_context = NULL;
    int media_state = HEVC_ENC_MEDIA_STATE_32x32_B_INTRA_CHECK;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    gpe_context = &priv_ctx->mbenc_context.gpe_contexts[HEVC_MBENC_32x32INTRACHECK_IDX];

    gen8_gpe_context_init(ctx, gpe_context);
    gen9_gpe_reset_binding_table(ctx, gpe_context);
    gen9_hevc_32x32_b_intra_set_curbe(ctx, encode_state, encoder_context, gpe_context);
    gen9_hevc_32x32_b_intra_set_surfaces(ctx, encode_state, encoder_context, gpe_context);
    gen8_gpe_setup_interface_data(ctx, gpe_context);

    memset((void *)&hevc_walker_param, 0, sizeof(hevc_walker_param));
    hevc_walker_param.resolution_x = ALIGN(priv_state->picture_width, 32) >> 5;
    hevc_walker_param.resolution_y = ALIGN(priv_state->picture_height, 32) >> 5;
    hevc_walker_param.no_dependency = 1;
    gen9_hevc_init_object_walker(&hevc_walker_param, &param);
    gen9_hevc_run_object_walker(ctx, encoder_context, gpe_context, &param,
                                media_state);
}

static void
gen9_hevc_32x32_pu_mode_set_curbe(VADriverContextP ctx,
                                  struct encode_state *encode_state,
                                  struct intel_encoder_context *encoder_context,
                                  struct i965_gpe_context *gpe_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct generic_enc_codec_state *generic_state = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    VAEncPictureParameterBufferHEVC *pic_param = NULL;
    VAEncSliceParameterBufferHEVC *slice_param = NULL;
    gen9_hevc_mbenc_32x32_pu_mode_curbe_data *cmd = NULL;
    double lambda_scaling_factor = 1.0, qp_lambda = 0.0, squared_qp_lambda = 0.0;
    unsigned int slice_qp = 0, fixed_point_lambda = 0;
    int bti_idx = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    generic_state = (struct generic_enc_codec_state *)vme_context->generic_enc_state;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;
    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;
    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[0]->buffer;

    slice_qp = pic_param->pic_init_qp + slice_param->slice_qp_delta;
    gen9_hevc_set_lambda_tables(priv_ctx, HEVC_SLICE_I, HEVC_ENC_INTRA_TRANS_HAAR);
    lambda_scaling_factor = 1.0;
    qp_lambda = priv_ctx->lambda_md_table[HEVC_SLICE_I][slice_qp];
    squared_qp_lambda = qp_lambda * qp_lambda;
    fixed_point_lambda = (unsigned int)(lambda_scaling_factor * squared_qp_lambda * (1 << 10));

    cmd = i965_gpe_context_map_curbe(gpe_context);
    if (!cmd)
        return;

    memset((void *)cmd, 0, sizeof(*cmd));

    cmd->dw0.frame_width = ALIGN(priv_state->picture_width, 16);
    cmd->dw0.frame_height = ALIGN(priv_state->picture_height, 16);
    cmd->dw1.enable_debug_dump = 0;
    cmd->dw1.lcu_type = priv_state->lcu_size == 64 ? 0 : 1;
    cmd->dw1.pu_type = 0;
    cmd->dw1.brc_enable = !!generic_state->brc_enabled;
    cmd->dw1.lcu_brc_enable = priv_state->lcu_brc_enabled;
    cmd->dw1.slice_type = priv_state->picture_coding_type;
    cmd->dw1.fast_surveillance_flag = priv_state->picture_coding_type == HEVC_SLICE_I ?
                                      0 : priv_state->video_surveillance_flag;
    cmd->dw1.roi_enable = (priv_state->num_roi > 0);

    cmd->dw2.lambda = fixed_point_lambda;
    cmd->dw3.mode_cost_32x32 = 0;
    cmd->dw4.early_exit = (unsigned int) - 1;

    cmd->dw8.bti_32x32_pu_output = bti_idx++;
    cmd->dw9.bti_src_y = bti_idx++;
    bti_idx++;
    cmd->dw10.bti_src_y2x = bti_idx++;
    cmd->dw11.bti_slice_map = bti_idx++;
    cmd->dw12.bti_src_y2x_vme = bti_idx++;
    cmd->dw13.bti_brc_input = bti_idx++;
    cmd->dw14.bti_lcu_qp_surface = bti_idx++;
    cmd->dw15.bti_brc_data = bti_idx++;
    cmd->dw16.bti_kernel_debug = bti_idx++;

    i965_gpe_context_unmap_curbe(gpe_context);
}

static void
gen9_hevc_32x32_pu_mode_set_surfaces(VADriverContextP ctx,
                                     struct encode_state *encode_state,
                                     struct intel_encoder_context *encoder_context,
                                     struct i965_gpe_context *gpe_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    int bti_idx = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_32x32_PU_OUTPUT, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_RAW_Y, bti_idx++,
                                 1, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);
    bti_idx++;

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_Y_2X, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_SLICE_MAP, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_adv_surface(ctx, priv_ctx, gpe_context,
                                  HEVC_ENC_SURFACE_Y_2X_VME, bti_idx++,
                                  NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_BRC_INPUT, bti_idx++,
                                 0, 0, 0, NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_LCU_QP, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_BRC_DATA, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R8_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_1d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_KERNEL_DEBUG, bti_idx,
                                 0, 0, 0, NULL, NULL);
}

static void
gen9_hevc_32x32_pu_mode(VADriverContextP ctx,
                        struct encode_state *encode_state,
                        struct intel_encoder_context *encoder_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    struct gpe_media_object_walker_parameter param;
    struct hevc_enc_kernel_walker_parameter hevc_walker_param;
    struct i965_gpe_context *gpe_context = NULL;
    int media_state = HEVC_ENC_MEDIA_STATE_32x32_PU_MODE_DECISION;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    gpe_context = &priv_ctx->mbenc_context.gpe_contexts[HEVC_MBENC_32x32MD_IDX];

    gen8_gpe_context_init(ctx, gpe_context);
    gen9_gpe_reset_binding_table(ctx, gpe_context);
    gen9_hevc_32x32_pu_mode_set_curbe(ctx, encode_state, encoder_context, gpe_context);
    gen9_hevc_32x32_pu_mode_set_surfaces(ctx, encode_state, encoder_context, gpe_context);
    gen8_gpe_setup_interface_data(ctx, gpe_context);

    memset((void *)&hevc_walker_param, 0, sizeof(hevc_walker_param));
    hevc_walker_param.resolution_x = ALIGN(priv_state->picture_width, 32) >> 5;
    hevc_walker_param.resolution_y = ALIGN(priv_state->picture_height, 32) >> 5;
    hevc_walker_param.no_dependency = 1;
    gen9_hevc_init_object_walker(&hevc_walker_param, &param);
    gen9_hevc_run_object_walker(ctx, encoder_context, gpe_context, &param,
                                media_state);
}

static void
gen9_hevc_2x_scaling_set_curbe(VADriverContextP ctx,
                               struct encode_state *encode_state,
                               struct intel_encoder_context *encoder_context,
                               struct i965_gpe_context *gpe_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    gen9_hevc_mbenc_downscaling2x_curbe_data *cmd = NULL;
    int bti_idx = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    cmd = i965_gpe_context_map_curbe(gpe_context);
    if (!cmd)
        return;

    memset((void *)cmd, 0, sizeof(gen9_hevc_mbenc_downscaling2x_curbe_data));

    cmd->dw0.pic_width = ALIGN(priv_state->picture_width, 16);
    cmd->dw0.pic_height = ALIGN(priv_state->picture_height, 16);

    cmd->dw8.bti_src_y = bti_idx++;
    cmd->dw9.bit_dst_y = bti_idx;

    i965_gpe_context_unmap_curbe(gpe_context);
}

static void
gen9_hevc_2x_scaling_set_surfaces(VADriverContextP ctx,
                                  struct encode_state *encode_state,
                                  struct intel_encoder_context *encoder_context,
                                  struct i965_gpe_context *gpe_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    int bti_idx = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_RAW_Y, bti_idx++,
                                 0, 1, I965_SURFACEFORMAT_R16_UNORM,
                                 NULL, NULL);

    gen9_hevc_set_gpe_2d_surface(ctx, priv_ctx, gpe_context,
                                 HEVC_ENC_SURFACE_Y_2X, bti_idx,
                                 0, 1, I965_SURFACEFORMAT_R16_UNORM,
                                 NULL, NULL);
}

static void
gen9_hevc_2x_scaling(VADriverContextP ctx,
                     struct encode_state *encode_state,
                     struct intel_encoder_context *encoder_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    struct gpe_media_object_walker_parameter param;
    struct hevc_enc_kernel_walker_parameter hevc_walker_param;
    struct i965_gpe_context *gpe_context = NULL;
    int media_state = HEVC_ENC_MEDIA_STATE_2X_SCALING;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    gpe_context = &priv_ctx->mbenc_context.gpe_contexts[HEVC_MBENC_2xSCALING_IDX];

    gen8_gpe_context_init(ctx, gpe_context);
    gen9_gpe_reset_binding_table(ctx, gpe_context);
    gen9_hevc_2x_scaling_set_curbe(ctx, encode_state, encoder_context, gpe_context);
    gen9_hevc_2x_scaling_set_surfaces(ctx, encode_state, encoder_context, gpe_context);
    gen8_gpe_setup_interface_data(ctx, gpe_context);

    memset((void *)&hevc_walker_param, 0, sizeof(hevc_walker_param));
    hevc_walker_param.resolution_x = ALIGN(priv_state->picture_width, 32) >> 5;
    hevc_walker_param.resolution_y = ALIGN(priv_state->picture_height, 32) >> 5;
    hevc_walker_param.no_dependency = 1;
    gen9_hevc_init_object_walker(&hevc_walker_param, &param);

    gen9_hevc_run_object_walker(ctx, encoder_context, gpe_context, &param,
                                media_state);
}

static void
gen9_hevc_mbenc(VADriverContextP ctx,
                struct encode_state *encode_state,
                struct intel_encoder_context *encoder_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    int fast_encoding = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;
    fast_encoding = (priv_state->tu_mode == HEVC_TU_BEST_SPEED);

    if (!fast_encoding) {
        if (!priv_state->bit_depth_luma_minus8)
            gen9_hevc_2x_scaling(ctx, encode_state, encoder_context);

        if (priv_state->picture_coding_type == HEVC_SLICE_I)
            gen9_hevc_32x32_pu_mode(ctx, encode_state, encoder_context);
        else
            gen9_hevc_32x32_b_intra(ctx, encode_state, encoder_context);

        gen9_hevc_16x16_sad_pu_computation(ctx, encode_state, encoder_context);
        gen9_hevc_16x16_pu_mode(ctx, encode_state, encoder_context);
        gen9_hevc_8x8_pu_mode(ctx, encode_state, encoder_context);
        gen9_hevc_8x8_pu_fmode(ctx, encode_state, encoder_context);
    }

    if (priv_state->picture_coding_type != HEVC_SLICE_I ||
        fast_encoding) {
        gen9_hevc_8x8_b_mbenc(ctx, encode_state, encoder_context);
        gen9_hevc_8x8_b_pak(ctx, encode_state, encoder_context);
    }
}

static VAStatus
gen9_hevc_vme_gpe_init(VADriverContextP ctx,
                       struct encode_state *encode_state,
                       struct intel_encoder_context *encoder_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    VAEncSliceParameterBufferHEVC *slice_param = NULL;
    int i = 0, j = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    i965_zero_gpe_resource(&priv_ctx->res_mb_code_surface);

    i965_zero_gpe_resource(&priv_ctx->res_slice_map_buffer);
    if (encode_state->num_slice_params_ext > 1) {
        struct gen9_hevc_slice_map *pslice_map = NULL;
        int width = priv_state->width_in_lcu;
        int pitch = ALIGN(priv_state->frame_width_in_max_lcu >> 3, 64);
        void *ptr_start = NULL;
        int lcu_count = 0;

        ptr_start = (void *)i965_map_gpe_resource(&priv_ctx->res_slice_map_buffer);

        if (!ptr_start)
            return VA_STATUS_ERROR_UNKNOWN;

        pslice_map = (struct gen9_hevc_slice_map *)ptr_start;
        for (i = 0; i < encode_state->num_slice_params_ext; i++) {
            slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[i]->buffer;

            for (j = 0; j < slice_param->num_ctu_in_slice; j++) {
                pslice_map[lcu_count++].slice_id = i;

                if (lcu_count >= width) {
                    lcu_count = 0;
                    ptr_start += pitch;

                    pslice_map = (struct gen9_hevc_slice_map *)ptr_start;
                }
            }
        }

        i965_unmap_gpe_resource(&priv_ctx->res_slice_map_buffer);
    }

    return VA_STATUS_SUCCESS;
}

static VAStatus
gen9_hevc_vme_gpe_run(VADriverContextP ctx,
                      struct encode_state *encode_state,
                      struct intel_encoder_context *encoder_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct generic_enc_codec_state *generic_state = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;

    vme_context = encoder_context->vme_context;
    generic_state = vme_context->generic_enc_state;
    priv_state = vme_context->private_enc_state;

    if (generic_state->brc_enabled &&
        (generic_state->brc_need_reset || !generic_state->brc_inited)) {
        gen9_hevc_brc_init_reset(ctx, encode_state, encoder_context,
                                 generic_state->brc_inited ? 1 : 0);
        generic_state->brc_need_reset = 0;
        generic_state->brc_inited = 1;
    }

    if (generic_state->hme_supported || generic_state->brc_enabled) {
        gen9_hevc_hme_scaling(ctx, encode_state, encoder_context);

        if (generic_state->brc_enabled)
            gen9_hevc_brc_intra_dist(ctx, encode_state, encoder_context);

        if (generic_state->hme_enabled)
            gen9_hevc_hme_encode_me(ctx, encode_state, encoder_context);

        if (generic_state->brc_enabled)
            gen9_hevc_brc_update(ctx, encode_state, encoder_context);
    }

    if (priv_state->num_roi && !generic_state->brc_enabled)
        gen9_hevc_brc_update_lcu_based(ctx, encode_state, encoder_context);

    if (priv_state->bit_depth_luma_minus8)
        gen9_hevc_ref_frame_depth_conversion(ctx, encode_state, encoder_context);

    gen9_hevc_mbenc(ctx, encode_state, encoder_context);

    return VA_STATUS_SUCCESS;
}

static VAStatus
gen9_hevc_vme_pipeline(VADriverContextP ctx,
                       VAProfile profile,
                       struct encode_state *encode_state,
                       struct intel_encoder_context *encoder_context)
{
    VAStatus va_status = VA_STATUS_SUCCESS;

    va_status = gen9_hevc_enc_init_parameters(ctx, encode_state, encoder_context);
    if (va_status != VA_STATUS_SUCCESS)
        goto EXIT;

    va_status = gen9_hevc_vme_gpe_init(ctx, encode_state, encoder_context);
    if (va_status != VA_STATUS_SUCCESS)
        goto EXIT;

    va_status = gen9_hevc_vme_gpe_run(ctx, encode_state, encoder_context);

EXIT:
    return va_status;
}

static void
gen9_hevc_vme_scaling_context_init(VADriverContextP ctx,
                                   struct intel_encoder_context *encoder_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    struct i965_gpe_context *gpe_context = NULL;
    struct i965_kernel kernel_info;
    struct gen9_hevc_scaling_context *scaling_ctx = NULL;
    GEN9_ENC_OPERATION kernel_idx;
    int curbe_size = 0;
    int i = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    scaling_ctx = &priv_ctx->scaling_context;

    for (i = 0; i < NUM_HEVC_ENC_SCALING; i++) {
        if (i == HEVC_ENC_SCALING_4X ||
            i == HEVC_ENC_SCALING_16X) {
            curbe_size = sizeof(gen9_hevc_scaling4x_curbe_data);
            kernel_idx = GEN9_ENC_SCALING4X;
        } else if (i == HEVC_ENC_SCALING_32X) {
            curbe_size = sizeof(gen9_hevc_scaling2x_curbe_data);
            kernel_idx = GEN9_ENC_SCALING2X;
        }

        gpe_context = &scaling_ctx->gpe_contexts[i];

        gen9_hevc_vme_init_gpe_context(ctx, gpe_context,
                                       curbe_size,
                                       curbe_size);
        gen9_hevc_vme_init_scoreboard(gpe_context,
                                      0xFF,
                                      priv_state->use_hw_scoreboard,
                                      priv_state->use_hw_non_stalling_scoreborad);

        memset(&kernel_info, 0, sizeof(kernel_info));
        gen9_hevc_get_kernel_header_and_size((void *)hevc_enc_kernel_ptr,
                                             hevc_enc_kernel_size,
                                             kernel_idx,
                                             0,
                                             &kernel_info);
        gen8_gpe_load_kernels(ctx,
                              gpe_context,
                              &kernel_info,
                              1);
    }
}

static void
gen9_hevc_vme_me_context_init(VADriverContextP ctx,
                              struct intel_encoder_context *encoder_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    struct i965_gpe_context *gpe_context = NULL;
    struct i965_kernel kernel_info;
    struct gen9_hevc_me_context *me_ctx = NULL;
    int i = 0, j = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    me_ctx = &priv_ctx->me_context;

    for (i = 0; i < NUM_HEVC_ENC_ME; i++) {
        for (j = 0; j < NUM_HEVC_ENC_ME_TYPES; j++) {
            gpe_context = &me_ctx->gpe_context[j][i];

            gen9_hevc_vme_init_gpe_context(ctx, gpe_context,
                                           sizeof(gen9_hevc_me_curbe_data),
                                           0);
            gen9_hevc_vme_init_scoreboard(gpe_context,
                                          0xFF,
                                          priv_state->use_hw_scoreboard,
                                          priv_state->use_hw_non_stalling_scoreborad);

            memset(&kernel_info, 0, sizeof(kernel_info));
            gen9_hevc_get_kernel_header_and_size((void *)hevc_enc_kernel_ptr,
                                                 hevc_enc_kernel_size,
                                                 GEN9_ENC_ME,
                                                 i,
                                                 &kernel_info);
            gen8_gpe_load_kernels(ctx,
                                  gpe_context,
                                  &kernel_info,
                                  1);
        }
    }
}

static void
gen9_hevc_vme_mbenc_context_init(VADriverContextP ctx,
                                 struct intel_encoder_context *encoder_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    struct i965_gpe_context *gpe_context = NULL;
    struct i965_kernel kernel_info;
    struct gen9_hevc_mbenc_context *mbenc_ctx = NULL;
    int i = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    mbenc_ctx = &priv_ctx->mbenc_context;

    mbenc_ctx->kernel_num = GEN8_HEVC_ENC_MBENC_TOTAL_NUM;

    for (i = 0; i < mbenc_ctx->kernel_num; i++) {
        gpe_context = &mbenc_ctx->gpe_contexts[i];

        gen9_hevc_vme_init_gpe_context(ctx, gpe_context,
                                       hevc_mbenc_curbe_size[i],
                                       0);
        gen9_hevc_vme_init_scoreboard(gpe_context,
                                      0xFF,
                                      priv_state->use_hw_scoreboard,
                                      priv_state->use_hw_non_stalling_scoreborad);

        memset(&kernel_info, 0, sizeof(kernel_info));
        gen9_hevc_get_kernel_header_and_size((void *)hevc_enc_kernel_ptr,
                                             hevc_enc_kernel_size,
                                             GEN9_ENC_MBENC,
                                             i,
                                             &kernel_info);
        gen8_gpe_load_kernels(ctx,
                              gpe_context,
                              &kernel_info,
                              1);
    }
}

static void
gen9_hevc_vme_brc_context_init(VADriverContextP ctx,
                               struct intel_encoder_context *encoder_context)
{
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    struct i965_gpe_context *gpe_context = NULL;
    struct i965_kernel kernel_info;
    struct gen9_hevc_brc_context *brc_ctx = NULL;
    int i = 0;

    vme_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)vme_context->private_enc_state;

    brc_ctx = &priv_ctx->brc_context;

    for (i = 0; i < GEN9_HEVC_ENC_BRC_NUM; i++) {
        gpe_context = &brc_ctx->gpe_contexts[i];

        gen9_hevc_vme_init_gpe_context(ctx, gpe_context,
                                       hevc_brc_curbe_size[i],
                                       0);
        gen9_hevc_vme_init_scoreboard(gpe_context,
                                      0xFF,
                                      priv_state->use_hw_scoreboard,
                                      priv_state->use_hw_non_stalling_scoreborad);

        memset(&kernel_info, 0, sizeof(kernel_info));
        gen9_hevc_get_kernel_header_and_size((void *)hevc_enc_kernel_ptr,
                                             hevc_enc_kernel_size,
                                             GEN9_ENC_BRC,
                                             i,
                                             &kernel_info);
        gen8_gpe_load_kernels(ctx,
                              gpe_context,
                              &kernel_info,
                              1);
    }
}

static void
gen9_hevc_vme_scaling_context_destroy(struct gen9_hevc_scaling_context *scaling_context)
{
    int i;

    for (i = 0; i < NUM_HEVC_ENC_SCALING; i++)
        gen8_gpe_context_destroy(&scaling_context->gpe_contexts[i]);
}

static void
gen9_hevc_vme_me_context_destroy(struct gen9_hevc_me_context *me_context)
{
    int i, j;

    for (i = 0; i < NUM_HEVC_ENC_ME; i++)
        for (j = 0; j < NUM_HEVC_ENC_ME_TYPES; j++)
            gen8_gpe_context_destroy(&me_context->gpe_context[j][i]);
}

static void
gen9_hevc_vme_mbenc_context_destroy(struct gen9_hevc_mbenc_context *mbenc_context)
{
    int i;

    for (i = 0; i < mbenc_context->kernel_num; i++)
        gen8_gpe_context_destroy(&mbenc_context->gpe_contexts[i]);
}

static void
gen9_hevc_vme_brc_context_destroy(struct gen9_hevc_brc_context *brc_context)
{
    int i;

    for (i = 0; i < GEN9_HEVC_ENC_BRC_NUM; i++)
        gen8_gpe_context_destroy(&brc_context->gpe_contexts[i]);
}

static void
gen9_hevc_vme_kernels_context_init(VADriverContextP ctx,
                                   struct intel_encoder_context *encoder_context)
{
    gen9_hevc_vme_scaling_context_init(ctx, encoder_context);
    gen9_hevc_vme_me_context_init(ctx, encoder_context);
    gen9_hevc_vme_mbenc_context_init(ctx, encoder_context);
    gen9_hevc_vme_brc_context_init(ctx, encoder_context);
}

static void
gen9_hevc_vme_kernels_context_destroy(struct encoder_vme_mfc_context *vme_context)
{
    struct gen9_hevc_encoder_context *priv_ctx = NULL;

    priv_ctx = (struct gen9_hevc_encoder_context *)vme_context->private_enc_ctx;

    gen9_hevc_vme_scaling_context_destroy(&priv_ctx->scaling_context);
    gen9_hevc_vme_me_context_destroy(&priv_ctx->me_context);
    gen9_hevc_vme_mbenc_context_destroy(&priv_ctx->mbenc_context);
    gen9_hevc_vme_brc_context_destroy(&priv_ctx->brc_context);
}

static void
gen9_hevc_vme_context_destroy(void *context)
{
    struct encoder_vme_mfc_context *vme_context = (struct encoder_vme_mfc_context *)context;

    if (!vme_context)
        return;

    gen9_hevc_enc_free_resources(vme_context);

    gen9_hevc_vme_kernels_context_destroy(vme_context);

    if (vme_context->private_enc_ctx) free(vme_context->private_enc_ctx);
    if (vme_context->generic_enc_state) free(vme_context->generic_enc_state);
    if (vme_context->private_enc_state) free(vme_context->private_enc_state);

    free(vme_context);
}

#define PAK_IMPLEMENTATION_START

static void
gen9_hevc_pak_pipe_mode_select(VADriverContextP ctx,
                               struct encode_state *encode_state,
                               struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;

    BEGIN_BCS_BATCH(batch, 6);

    OUT_BCS_BATCH(batch, HCP_PIPE_MODE_SELECT | (6 - 2));
    OUT_BCS_BATCH(batch,
                  (0 << 5) |
                  (0 << 3) |
                  HCP_CODEC_SELECT_ENCODE);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_hevc_pak_add_surface_state(VADriverContextP ctx,
                                struct encode_state *encode_state,
                                struct intel_encoder_context *encoder_context,
                                struct object_surface *obj_surface,
                                enum GEN9_HEVC_ENC_SURFACE_TYPE type)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;

    BEGIN_BCS_BATCH(batch, 3);

    OUT_BCS_BATCH(batch, HCP_SURFACE_STATE | (3 - 2));
    OUT_BCS_BATCH(batch,
                  (type << 28) |
                  (obj_surface->width - 1));
    OUT_BCS_BATCH(batch,
                  (((obj_surface->fourcc == VA_FOURCC_P010) ?
                    SURFACE_FORMAT_P010 :
                    SURFACE_FORMAT_PLANAR_420_8) << 28) |
                  (obj_surface->y_cb_offset));

    ADVANCE_BCS_BATCH(batch);
}

#define OUT_BUFFER(buf_bo, is_target, ma)                               \
    do {                                                                \
        if (buf_bo) {                                                   \
            OUT_RELOC64(batch,                                          \
                          buf_bo,                                       \
                          I915_GEM_DOMAIN_RENDER,                       \
                          is_target ? I915_GEM_DOMAIN_RENDER : 0,       \
                          0);                                           \
        } else {                                                        \
            OUT_BCS_BATCH(batch, 0);                                    \
            OUT_BCS_BATCH(batch, 0);                                    \
        }                                                               \
        if (ma)                                                         \
            OUT_BCS_BATCH(batch, priv_ctx->mocs);                       \
    } while (0);

#define OUT_BUFFER_MA_TARGET(buf_bo)       OUT_BUFFER(buf_bo, 1, 1)
#define OUT_BUFFER_MA_REFERENCE(buf_bo)    OUT_BUFFER(buf_bo, 0, 1)
#define OUT_BUFFER_NMA_REFERENCE(buf_bo)   OUT_BUFFER(buf_bo, 0, 0)

static void
gen9_hevc_pak_add_pipe_buf_addr_state(VADriverContextP ctx,
                                      struct encode_state *encode_state,
                                      struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct encoder_vme_mfc_context *pak_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    dri_bo *bo = NULL;
    int i = 0;

    pak_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)pak_context->private_enc_ctx;

    if (IS_KBL(i965->intel.device_info) || IS_GLK(i965->intel.device_info)) {
        BEGIN_BCS_BATCH(batch, 104);

        OUT_BCS_BATCH(batch, HCP_PIPE_BUF_ADDR_STATE | (104 - 2));
    } else {
        BEGIN_BCS_BATCH(batch, 95);

        OUT_BCS_BATCH(batch, HCP_PIPE_BUF_ADDR_STATE | (95 - 2));
    }

    OUT_BUFFER_MA_TARGET(priv_ctx->reconstructed_object.obj_surface->bo);
    OUT_BUFFER_MA_TARGET(priv_ctx->deblocking_filter_line_buffer.bo);
    OUT_BUFFER_MA_TARGET(priv_ctx->deblocking_filter_tile_line_buffer.bo);
    OUT_BUFFER_MA_TARGET(priv_ctx->deblocking_filter_tile_column_buffer.bo);
    OUT_BUFFER_MA_TARGET(priv_ctx->metadata_line_buffer.bo);
    OUT_BUFFER_MA_TARGET(priv_ctx->metadata_tile_line_buffer.bo);
    OUT_BUFFER_MA_TARGET(priv_ctx->metadata_tile_column_buffer.bo);
    OUT_BUFFER_MA_TARGET(priv_ctx->sao_line_buffer.bo);
    OUT_BUFFER_MA_TARGET(priv_ctx->sao_tile_line_buffer.bo);
    OUT_BUFFER_MA_TARGET(priv_ctx->sao_tile_column_buffer.bo);
    OUT_BUFFER_MA_TARGET(priv_ctx->
                         mv_temporal_buffer[GEN9_MAX_MV_TEMPORAL_BUFFERS - 1].bo);
    OUT_BUFFER_MA_TARGET(NULL);

    for (i = 0; i < GEN9_MAX_REF_SURFACES; i++) {
        if (priv_ctx->reference_surfaces[i].obj_surface &&
            priv_ctx->reference_surfaces[i].obj_surface->bo) {
            bo = priv_ctx->reference_surfaces[i].obj_surface->bo;

            OUT_BUFFER_NMA_REFERENCE(bo);
        } else
            OUT_BUFFER_NMA_REFERENCE(NULL);
    }
    OUT_BCS_BATCH(batch, priv_ctx->mocs);

    OUT_BUFFER_MA_TARGET(priv_ctx->
                         uncompressed_picture_source.obj_surface->bo);
    OUT_BUFFER_MA_TARGET(NULL);
    OUT_BUFFER_MA_TARGET(NULL);
    OUT_BUFFER_MA_TARGET(NULL);

    for (i = 0; i < GEN9_MAX_MV_TEMPORAL_BUFFERS - 1; i++) {
        bo = priv_ctx->mv_temporal_buffer[i].bo;

        if (bo) {
            OUT_BUFFER_NMA_REFERENCE(bo);
        } else
            OUT_BUFFER_NMA_REFERENCE(NULL);
    }
    OUT_BCS_BATCH(batch, priv_ctx->mocs);

    OUT_BUFFER_MA_TARGET(NULL);
    OUT_BUFFER_MA_TARGET(NULL);
    OUT_BUFFER_MA_TARGET(NULL);
    OUT_BUFFER_MA_TARGET(NULL);

    if (IS_KBL(i965->intel.device_info) || IS_GLK(i965->intel.device_info)) {
        for (i = 0; i < 9; i++)
            OUT_BCS_BATCH(batch, 0);
    }

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_hevc_pak_add_ind_obj_base_addr_state(VADriverContextP ctx,
                                          struct encode_state *encode_state,
                                          struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct encoder_vme_mfc_context *pak_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;

    pak_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)pak_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)pak_context->private_enc_state;

    BEGIN_BCS_BATCH(batch, 14);

    OUT_BCS_BATCH(batch, HCP_IND_OBJ_BASE_ADDR_STATE | (14 - 2));
    OUT_BUFFER_MA_REFERENCE(NULL);
    OUT_BUFFER_NMA_REFERENCE(NULL);

    OUT_RELOC64(batch,
                priv_ctx->res_mb_code_surface.bo,
                I915_GEM_DOMAIN_INSTRUCTION, 0,
                priv_state->mb_data_offset);
    OUT_BCS_BATCH(batch, priv_ctx->mocs);

    OUT_RELOC64(batch,
                priv_ctx->indirect_pak_bse_object.bo,
                I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER,
                priv_ctx->indirect_pak_bse_object.offset);
    OUT_BCS_BATCH(batch, priv_ctx->mocs);

    OUT_RELOC64(batch,
                priv_ctx->indirect_pak_bse_object.bo,
                I915_GEM_DOMAIN_RENDER, 0,
                priv_ctx->indirect_pak_bse_object.end_offset);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_hevc_pak_set_qm(VADriverContextP ctx,
                     int size_id,
                     int color_component,
                     int pred_type,
                     int dc,
                     unsigned int *qm,
                     int qm_length,
                     struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    unsigned int qm_buffer[16];

    memset(qm_buffer, 0, sizeof(qm_buffer));
    memcpy(qm_buffer, qm, qm_length * 4);

    BEGIN_BCS_BATCH(batch, 18);

    OUT_BCS_BATCH(batch, HCP_QM_STATE | (18 - 2));
    OUT_BCS_BATCH(batch,
                  dc << 5 |
                  color_component << 3 |
                  size_id << 1 |
                  pred_type);
    intel_batchbuffer_data(batch, qm_buffer, 16 * 4);

    ADVANCE_BCS_BATCH(batch);
}

static unsigned int qm_default[16] = {
    0x10101010, 0x10101010, 0x10101010, 0x10101010,
    0x10101010, 0x10101010, 0x10101010, 0x10101010,
    0x10101010, 0x10101010, 0x10101010, 0x10101010,
    0x10101010, 0x10101010, 0x10101010, 0x10101010
};

static void
gen9_hevc_pak_add_qm_state(VADriverContextP ctx,
                           struct encode_state *encode_state,
                           struct intel_encoder_context *encoder_context)
{
    int i;

    for (i = 0; i < 6; i++)
        gen9_hevc_pak_set_qm(ctx,
                             0, i % 3, i / 3, 0,
                             qm_default, 4,
                             encoder_context);

    for (i = 0; i < 6; i++)
        gen9_hevc_pak_set_qm(ctx,
                             1, i % 3, i / 3, 0,
                             qm_default, 16,
                             encoder_context);

    for (i = 0; i < 6; i++)
        gen9_hevc_pak_set_qm(ctx,
                             2, i % 3, i / 3, 16,
                             qm_default, 16,
                             encoder_context);

    for (i = 0; i < 2; i++)
        gen9_hevc_pak_set_qm(ctx,
                             3, 0, i % 2, 16,
                             qm_default, 16,
                             encoder_context);
}

static void
gen9_hevc_pak_set_fqm(VADriverContextP ctx,
                      int size_id,
                      int color_component,
                      int pred_type,
                      int dc,
                      unsigned int *fqm,
                      int fqm_length,
                      struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    unsigned int fqm_buffer[32];

    memset(fqm_buffer, 0, sizeof(fqm_buffer));
    memcpy(fqm_buffer, fqm, fqm_length * 4);

    BEGIN_BCS_BATCH(batch, 34);

    OUT_BCS_BATCH(batch, HCP_FQM_STATE | (34 - 2));
    OUT_BCS_BATCH(batch,
                  dc << 16 |
                  color_component << 3 |
                  size_id << 1 |
                  pred_type);
    intel_batchbuffer_data(batch, fqm_buffer, 32 * 4);

    ADVANCE_BCS_BATCH(batch);
}

static unsigned int fm_default[32] = {
    0x10001000, 0x10001000, 0x10001000, 0x10001000,
    0x10001000, 0x10001000, 0x10001000, 0x10001000,
    0x10001000, 0x10001000, 0x10001000, 0x10001000,
    0x10001000, 0x10001000, 0x10001000, 0x10001000,
    0x10001000, 0x10001000, 0x10001000, 0x10001000,
    0x10001000, 0x10001000, 0x10001000, 0x10001000,
    0x10001000, 0x10001000, 0x10001000, 0x10001000,
    0x10001000, 0x10001000, 0x10001000, 0x10001000
};

static void
gen9_hevc_pak_add_fm_state(VADriverContextP ctx,
                           struct encode_state *encode_state,
                           struct intel_encoder_context *encoder_context)
{
    gen9_hevc_pak_set_fqm(ctx,
                          0, 0, 0, 0,
                          fm_default, 8,
                          encoder_context);
    gen9_hevc_pak_set_fqm(ctx,
                          0, 0, 1, 0,
                          fm_default, 8,
                          encoder_context);
    gen9_hevc_pak_set_fqm(ctx,
                          1, 0, 0, 0,
                          fm_default, 32,
                          encoder_context);
    gen9_hevc_pak_set_fqm(ctx,
                          1, 0, 1, 0,
                          fm_default, 32,
                          encoder_context);
    gen9_hevc_pak_set_fqm(ctx,
                          2, 0, 0, 0x1000,
                          fm_default, 0,
                          encoder_context);
    gen9_hevc_pak_set_fqm(ctx,
                          2, 0, 1, 0x1000,
                          fm_default, 0,
                          encoder_context);
    gen9_hevc_pak_set_fqm(ctx,
                          3, 0, 0, 0x1000,
                          fm_default, 0,
                          encoder_context);
    gen9_hevc_pak_set_fqm(ctx,
                          3, 0, 1, 0x1000,
                          fm_default, 0,
                          encoder_context);
}

static void
gen9_hevc_set_reflist(VADriverContextP ctx,
                      struct gen9_hevc_encoder_context *priv_ctx,
                      VAEncPictureParameterBufferHEVC *pic_param,
                      VAEncSliceParameterBufferHEVC *slice_param,
                      int list_idx,
                      struct intel_batchbuffer *batch)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    int num_ref_minus1 = (list_idx ?
                          slice_param->num_ref_idx_l1_active_minus1 :
                          slice_param->num_ref_idx_l0_active_minus1);
    VAPictureHEVC *ref_list = (list_idx ?
                               slice_param->ref_pic_list1 :
                               slice_param->ref_pic_list0);
    VAPictureHEVC *curr_pic = &pic_param->decoded_curr_pic;
    struct object_surface *obj_surface = NULL;
    int frame_idx;
    int i = 0, j = 0;

    BEGIN_BCS_BATCH(batch, 18);

    OUT_BCS_BATCH(batch, HCP_REF_IDX_STATE | (18 - 2));
    OUT_BCS_BATCH(batch,
                  num_ref_minus1 << 1 |
                  list_idx);

    for (i = 0; i < 16; i++) {
        frame_idx = -1;
        obj_surface = SURFACE(ref_list[i].picture_id);
        if (i < MIN((num_ref_minus1 + 1), GEN9_MAX_REF_SURFACES) && obj_surface) {
            for (j = 0; j < GEN9_MAX_REF_SURFACES; j++) {
                if (obj_surface == priv_ctx->reference_surfaces[j].obj_surface) {
                    frame_idx = j;
                    break;
                }
            }
        }

        if (i < MIN((num_ref_minus1 + 1), GEN9_MAX_REF_SURFACES) &&
            frame_idx >= 0) {
            OUT_BCS_BATCH(batch,
                          1 << 15 |
                          0 << 14 |
                          !!(ref_list[i].flags & VA_PICTURE_HEVC_LONG_TERM_REFERENCE) << 13 |
                          0 << 12 |
                          0 << 11 |
                          frame_idx << 8 |
                          (CLAMP(-128, 127, curr_pic->pic_order_cnt -
                                 ref_list[i].pic_order_cnt) & 0xff));
        } else {
            OUT_BCS_BATCH(batch, 0);
        }
    }

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_hevc_pak_add_slice_state(VADriverContextP ctx,
                              struct encode_state *encode_state,
                              struct intel_encoder_context *encoder_context,
                              int slice_idx,
                              struct intel_batchbuffer *batch)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct encoder_vme_mfc_context *pak_context = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    VAEncPictureParameterBufferHEVC *pic_param = NULL;
    VAEncSliceParameterBufferHEVC *slice_param = NULL;
    int slice_hor_pos, slice_ver_pos, next_slice_hor_pos, next_slice_ver_pos;
    int slice_type = 0, slice_end = 0, last_slice = 0;
    int collocated_ref_idx = 0;

    pak_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_state = (struct gen9_hevc_encoder_state *)pak_context->private_enc_state;
    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;
    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[slice_idx]->buffer;

    slice_type = slice_param->slice_type;
    slice_end = slice_param->slice_segment_address + slice_param->num_ctu_in_slice;
    slice_hor_pos = slice_param->slice_segment_address % priv_state->width_in_lcu;
    slice_ver_pos = slice_param->slice_segment_address / priv_state->width_in_lcu;
    next_slice_hor_pos = slice_end % priv_state->width_in_lcu;
    next_slice_ver_pos = slice_end / priv_state->width_in_lcu;

    if (slice_end >= priv_state->width_in_lcu * priv_state->height_in_lcu ||
        slice_idx == encode_state->num_slice_params_ext - 1)
        last_slice = 1;

    if (priv_state->picture_coding_type != HEVC_SLICE_I &&
        slice_param->slice_fields.bits.slice_temporal_mvp_enabled_flag &&
        slice_param->slice_fields.bits.collocated_from_l0_flag)
        collocated_ref_idx = pic_param->collocated_ref_pic_index;

    if (IS_KBL(i965->intel.device_info) || IS_GLK(i965->intel.device_info)) {
        BEGIN_BCS_BATCH(batch, 11);

        OUT_BCS_BATCH(batch, HCP_SLICE_STATE | (11 - 2));
    } else {
        BEGIN_BCS_BATCH(batch, 9);

        OUT_BCS_BATCH(batch, HCP_SLICE_STATE | (9 - 2));
    }

    OUT_BCS_BATCH(batch,
                  slice_ver_pos << 16 |
                  slice_hor_pos);
    OUT_BCS_BATCH(batch,
                  next_slice_ver_pos << 16 |
                  next_slice_hor_pos);
    OUT_BCS_BATCH(batch,
                  (slice_param->slice_cr_qp_offset & 0x1f) << 17 |
                  (slice_param->slice_cb_qp_offset & 0x1f) << 12 |
                  (pic_param->pic_init_qp + slice_param->slice_qp_delta) << 6 |
                  slice_param->slice_fields.bits.slice_temporal_mvp_enabled_flag << 5 |
                  slice_param->slice_fields.bits.dependent_slice_segment_flag << 4 |
                  last_slice << 2 |
                  slice_type);
    OUT_BCS_BATCH(batch,
                  collocated_ref_idx << 26 |
                  (slice_param->max_num_merge_cand - 1)  << 23 |
                  slice_param->slice_fields.bits.cabac_init_flag << 22 |
                  slice_param->luma_log2_weight_denom << 19 |
                  (slice_param->luma_log2_weight_denom + slice_param->delta_chroma_log2_weight_denom) << 16 |
                  slice_param->slice_fields.bits.collocated_from_l0_flag << 15 |
                  priv_state->low_delay << 14 |
                  slice_param->slice_fields.bits.mvd_l1_zero_flag << 13 |
                  slice_param->slice_fields.bits.slice_sao_luma_flag << 12 |
                  slice_param->slice_fields.bits.slice_sao_chroma_flag << 11 |
                  slice_param->slice_fields.bits.slice_loop_filter_across_slices_enabled_flag << 10 |
                  (slice_param->slice_beta_offset_div2 & 0xf) << 5 |
                  (slice_param->slice_tc_offset_div2 & 0xf) << 1 |
                  slice_param->slice_fields.bits.slice_deblocking_filter_disabled_flag);
    OUT_BCS_BATCH(batch, 0);

    if (!pic_param->pic_fields.bits.reference_pic_flag &&
        priv_state->picture_coding_type != HEVC_SLICE_I)
        OUT_BCS_BATCH(batch, 0 << 26 |
                      8 << 20);
    else
        OUT_BCS_BATCH(batch, 5 << 26 |
                      11 << 20);

    OUT_BCS_BATCH(batch,
                  1 << 10 |
                  1 << 9  |
                  1 << 2  |
                  1 << 1  |
                  0);
    OUT_BCS_BATCH(batch, 0);

    if (IS_KBL(i965->intel.device_info) || IS_GLK(i965->intel.device_info)) {
        OUT_BCS_BATCH(batch, 0);
        OUT_BCS_BATCH(batch, 0);
    }

    ADVANCE_BCS_BATCH(batch);
}

static int
gen9_hevc_find_skipemulcnt(unsigned char *buf, unsigned int bits_length)
{
    int skip_cnt = 0, i = 0;

    if ((bits_length >> 3) < 6)
        return 0;

    for (i = 0; i < 3; i++)
        if (buf[i] != 0)
            break;

    if (i > 1) {
        if (buf[i] == 1)
            skip_cnt = i + 3;
    }

    return skip_cnt;
}

static void
gen9_hevc_pak_insert_object(unsigned int *data_buffer,
                            unsigned int data_size,
                            unsigned char emulation_flag,
                            int is_last_header,
                            int is_end_of_slice,
                            int skip_emul_byte_cnt,
                            struct intel_batchbuffer *batch)
{
    int length_in_dws = ALIGN(data_size, 32) >> 5;
    int data_bits_in_last_dw = data_size & 0x1f;
    int skip_cnt = skip_emul_byte_cnt;

    if (data_bits_in_last_dw == 0)
        data_bits_in_last_dw = 32;

    if (emulation_flag) {
        if (!skip_cnt)
            skip_cnt = gen9_hevc_find_skipemulcnt((unsigned char *)data_buffer,
                                                  data_size);
    }

    BEGIN_BCS_BATCH(batch, length_in_dws + 2);

    OUT_BCS_BATCH(batch, HCP_INSERT_PAK_OBJECT | (length_in_dws + 2 - 2));
    OUT_BCS_BATCH(batch,
                  (0 << 31) |
                  (0 << 16) |
                  (0 << 15) |
                  (data_bits_in_last_dw << 8) |
                  (skip_cnt << 4) |
                  ((!!emulation_flag) << 3) |
                  ((!!is_last_header) << 2) |
                  ((!!is_end_of_slice) << 1) |
                  (0 << 0));
    intel_batchbuffer_data(batch, data_buffer, length_in_dws * 4);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_hevc_pak_add_refs(VADriverContextP ctx,
                       struct encode_state *encode_state,
                       struct intel_encoder_context *encoder_context,
                       int slice_idx,
                       struct intel_batchbuffer *batch)
{
    struct encoder_vme_mfc_context *pak_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    VAEncPictureParameterBufferHEVC *pic_param = NULL;
    VAEncSliceParameterBufferHEVC *slice_param = NULL;

    pak_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)pak_context->private_enc_ctx;
    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;
    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[slice_idx]->buffer;

    if (slice_param->slice_type == HEVC_SLICE_I)
        return;

    gen9_hevc_set_reflist(ctx, priv_ctx, pic_param, slice_param, 0, batch);

    if (slice_param->slice_type == HEVC_SLICE_P)
        return;

    gen9_hevc_set_reflist(ctx, priv_ctx, pic_param, slice_param, 1, batch);
}

static void
gen9_hevc_pak_insert_packed_data(VADriverContextP ctx,
                                 struct encode_state *encode_state,
                                 struct intel_encoder_context *encoder_context,
                                 int slice_idx,
                                 struct intel_batchbuffer *batch)
{
    VAEncPackedHeaderParameterBuffer *param = NULL;
    unsigned int *header_data = NULL;
    unsigned int length_in_bits = 0;
    int packed_type = 0;
    int idx = 0, idx_offset = 0;
    int i = 0;

    for (i = 0; i < 4; i++) {
        idx_offset = 0;
        switch (i) {
        case 0:
            packed_type = VAEncPackedHeaderHEVC_VPS;
            break;
        case 1:
            packed_type = VAEncPackedHeaderHEVC_VPS;
            idx_offset = 1;
            break;
        case 2:
            packed_type = VAEncPackedHeaderHEVC_PPS;
            break;
        case 3:
            packed_type = VAEncPackedHeaderHEVC_SEI;
            break;
        default:
            break;
        }

        idx = va_enc_packed_type_to_idx(packed_type) + idx_offset;
        if (encode_state->packed_header_data[idx]) {
            param = (VAEncPackedHeaderParameterBuffer *)encode_state->packed_header_param[idx]->buffer;
            header_data = (unsigned int *)encode_state->packed_header_data[idx]->buffer;
            length_in_bits = param->bit_length;

            gen9_hevc_pak_insert_object(header_data, length_in_bits,
                                        !param->has_emulation_bytes, 0, 0, 0,
                                        batch);
        }
    }
}

static void
gen9_hevc_pak_insert_slice_header(VADriverContextP ctx,
                                  struct encode_state *encode_state,
                                  struct intel_encoder_context *encoder_context,
                                  int slice_idx,
                                  struct intel_batchbuffer *batch)
{
    VAEncPackedHeaderParameterBuffer *param = NULL;
    unsigned int *header_data = NULL;
    unsigned int length_in_bits = 0;
    int count = 0, start_index = -1;
    int i = 0;

    count = encode_state->slice_rawdata_count[slice_idx];
    start_index = encode_state->slice_rawdata_index[slice_idx] &
                  SLICE_PACKED_DATA_INDEX_MASK;

    for (i = 0; i < count; i++) {
        param = (VAEncPackedHeaderParameterBuffer *)
                (encode_state->packed_header_params_ext[start_index + i]->buffer);

        if (param->type == VAEncPackedHeaderSlice)
            continue;

        header_data = (unsigned int *)encode_state->packed_header_data_ext[start_index]->buffer;
        length_in_bits = param->bit_length;
        gen9_hevc_pak_insert_object(header_data, length_in_bits,
                                    !param->has_emulation_bytes, 0, 0, 0,
                                    batch);
    }

    start_index = -1;
    if (encode_state->slice_header_index[slice_idx] & SLICE_PACKED_DATA_INDEX_TYPE)
        start_index = encode_state->slice_header_index[slice_idx] &
                      SLICE_PACKED_DATA_INDEX_MASK;
    if (start_index == -1) {
        VAEncSequenceParameterBufferHEVC *seq_param = (VAEncSequenceParameterBufferHEVC *)encode_state->seq_param_ext->buffer;
        VAEncPictureParameterBufferHEVC *pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;
        VAEncSliceParameterBufferHEVC *slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[slice_idx]->buffer;
        unsigned char *slice_header = NULL;
        int slice_header_bits = 0;

        slice_header_bits = build_hevc_slice_header(seq_param,
                                                    pic_param,
                                                    slice_param,
                                                    &slice_header,
                                                    0);

        gen9_hevc_pak_insert_object((unsigned int *)slice_header, slice_header_bits,
                                    1, 1, 0, 5,
                                    batch);

        free(slice_header);
    } else {
        param = (VAEncPackedHeaderParameterBuffer *)
                (encode_state->packed_header_params_ext[start_index]->buffer);
        header_data = (unsigned int *)encode_state->packed_header_data_ext[start_index]->buffer;
        length_in_bits = param->bit_length;

        gen9_hevc_pak_insert_object(header_data, length_in_bits,
                                    !param->has_emulation_bytes, 1, 0, 0,
                                    batch);
    }
}

static VAStatus
gen9_hevc_pak_slice_level(VADriverContextP ctx,
                          struct encode_state *encode_state,
                          struct intel_encoder_context *encoder_context,
                          int slice_idx)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct encoder_vme_mfc_context *pak_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct generic_enc_codec_state *generic_state = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    VAEncSliceParameterBufferHEVC *slice_param = NULL;
    struct intel_batchbuffer *slice_batch = NULL;
    struct gpe_mi_batch_buffer_start_parameter second_level_batch;
    VAStatus va_status = VA_STATUS_SUCCESS;
    int slice_offset = 0;

    pak_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)pak_context->private_enc_ctx;
    generic_state = (struct generic_enc_codec_state *)pak_context->generic_enc_state;
    priv_state = (struct gen9_hevc_encoder_state *)pak_context->private_enc_state;
    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[slice_idx]->buffer;

    slice_batch = priv_ctx->res_pak_slice_batch_buffer;

    if (generic_state->curr_pak_pass == 0) {
        slice_offset = intel_batchbuffer_used_size(slice_batch);
        priv_state->slice_batch_offset[slice_idx] = slice_offset;

        if (slice_idx < encode_state->num_slice_params_ext - 1)
            priv_state->slice_start_lcu[slice_idx + 1] =
                priv_state->slice_start_lcu[slice_idx] +
                slice_param->num_ctu_in_slice;

        gen9_hevc_pak_add_refs(ctx, encode_state, encoder_context,
                               slice_idx, slice_batch);
        gen9_hevc_pak_add_slice_state(ctx, encode_state, encoder_context,
                                      slice_idx, slice_batch);

        if (slice_idx == 0)
            gen9_hevc_pak_insert_packed_data(ctx, encode_state, encoder_context,
                                             slice_idx, slice_batch);

        gen9_hevc_pak_insert_slice_header(ctx, encode_state, encoder_context,
                                          slice_idx, slice_batch);

        BEGIN_BCS_BATCH(slice_batch, 2);
        OUT_BCS_BATCH(slice_batch, 0);
        OUT_BCS_BATCH(slice_batch, MI_BATCH_BUFFER_END);
        ADVANCE_BCS_BATCH(slice_batch);
    } else
        slice_offset = priv_state->slice_batch_offset[slice_idx];

    memset(&second_level_batch, 0, sizeof(second_level_batch));
    second_level_batch.offset = slice_offset;
    second_level_batch.is_second_level = 1;
    second_level_batch.bo = slice_batch->buffer;
    gen8_gpe_mi_batch_buffer_start(ctx, batch, &second_level_batch);

    memset(&second_level_batch, 0, sizeof(second_level_batch));
    second_level_batch.offset = priv_state->slice_start_lcu[slice_idx] *
                                priv_state->pak_obj_size;
    second_level_batch.is_second_level = 1;
    second_level_batch.bo = priv_ctx->res_mb_code_surface.bo;
    gen8_gpe_mi_batch_buffer_start(ctx, batch, &second_level_batch);

    return va_status;
}

static VAStatus
gen9_hevc_pak_pipeline_prepare(VADriverContextP ctx,
                               struct encode_state *encode_state,
                               struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct encoder_vme_mfc_context *pak_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    struct gen9_hevc_surface_priv *surface_priv;
    VAEncPictureParameterBufferHEVC *pic_param = NULL;
    struct i965_coded_buffer_segment *coded_buffer_segment = NULL;
    struct object_surface *obj_surface = NULL;
    struct object_buffer *obj_buffer = NULL;
    dri_bo *bo = NULL;
    int i = 0;

    pak_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)pak_context->private_enc_ctx;
    priv_state = (struct gen9_hevc_encoder_state *)pak_context->private_enc_state;
    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;

    if (priv_ctx->uncompressed_picture_source.obj_surface &&
        priv_ctx->uncompressed_picture_source.obj_surface->bo)
        dri_bo_unreference(priv_ctx->uncompressed_picture_source.obj_surface->bo);
    priv_ctx->uncompressed_picture_source.obj_surface = encode_state->input_yuv_object;
    priv_ctx->uncompressed_picture_source.surface_id = encoder_context->input_yuv_surface;
    dri_bo_reference(priv_ctx->uncompressed_picture_source.obj_surface->bo);

    if (priv_ctx->reconstructed_object.obj_surface &&
        priv_ctx->reconstructed_object.obj_surface->bo)
        dri_bo_unreference(priv_ctx->reconstructed_object.obj_surface->bo);
    priv_ctx->reconstructed_object.obj_surface = encode_state->reconstructed_object;
    priv_ctx->reconstructed_object.surface_id = pic_param->decoded_curr_pic.picture_id;
    dri_bo_reference(priv_ctx->reconstructed_object.obj_surface->bo);

    surface_priv = (struct gen9_hevc_surface_priv *)encode_state->reconstructed_object->private_data;
    if (surface_priv) {
        if (priv_ctx->mv_temporal_buffer[GEN9_MAX_MV_TEMPORAL_BUFFERS - 1].bo)
            dri_bo_unreference(priv_ctx->mv_temporal_buffer[GEN9_MAX_MV_TEMPORAL_BUFFERS - 1].bo);
        priv_ctx->mv_temporal_buffer[GEN9_MAX_MV_TEMPORAL_BUFFERS - 1].bo =
            surface_priv->motion_vector_temporal_bo;
        dri_bo_reference(surface_priv->motion_vector_temporal_bo);
    }

    if (priv_state->picture_coding_type != HEVC_SLICE_I) {
        for (i = 0; i < GEN9_MAX_REF_SURFACES; i++) {
            obj_surface = encode_state->reference_objects[i];
            if (obj_surface && obj_surface->bo) {
                if (priv_ctx->reference_surfaces[i].obj_surface &&
                    priv_ctx->reference_surfaces[i].obj_surface->bo)
                    dri_bo_unreference(priv_ctx->reference_surfaces[i].obj_surface->bo);
                priv_ctx->reference_surfaces[i].obj_surface = obj_surface;
                priv_ctx->reference_surfaces[i].surface_id = pic_param->reference_frames[i].picture_id;
                dri_bo_reference(obj_surface->bo);

                surface_priv = (struct gen9_hevc_surface_priv *) obj_surface->private_data;
                if (surface_priv) {
                    if (priv_ctx->mv_temporal_buffer[i].bo)
                        dri_bo_unreference(priv_ctx->mv_temporal_buffer[i].bo);
                    priv_ctx->mv_temporal_buffer[i].bo = surface_priv->motion_vector_temporal_bo;
                    dri_bo_reference(surface_priv->motion_vector_temporal_bo);
                }
            } else {
                break;
            }
        }
    }

    obj_buffer = encode_state->coded_buf_object;
    bo = obj_buffer->buffer_store->bo;

    if (priv_ctx->indirect_pak_bse_object.bo)
        dri_bo_unreference(priv_ctx->indirect_pak_bse_object.bo);
    priv_ctx->indirect_pak_bse_object.offset = I965_CODEDBUFFER_HEADER_SIZE;
    priv_ctx->indirect_pak_bse_object.end_offset = ALIGN((obj_buffer->size_element - 0x1000), 0x1000);
    priv_ctx->indirect_pak_bse_object.bo = bo;
    dri_bo_reference(priv_ctx->indirect_pak_bse_object.bo);

    dri_bo_map(bo, 1);

    if (!bo->virtual)
        return VA_STATUS_ERROR_INVALID_VALUE;

    coded_buffer_segment = (struct i965_coded_buffer_segment *)bo->virtual;
    coded_buffer_segment->mapped = 0;
    coded_buffer_segment->codec = encoder_context->codec;
    coded_buffer_segment->status_support = 1;

    dri_bo_unmap(bo);

    if (priv_ctx->res_pak_slice_batch_buffer)
        intel_batchbuffer_free(priv_ctx->res_pak_slice_batch_buffer);

    priv_ctx->res_pak_slice_batch_buffer =
        intel_batchbuffer_new(&i965->intel, I915_EXEC_BSD,
                              GEN9_HEVC_ENC_PAK_SLICE_STATE_SIZE *
                              encode_state->num_slice_params_ext);
    if (!priv_ctx->res_pak_slice_batch_buffer)
        return VA_STATUS_ERROR_ALLOCATION_FAILED;

    for (i = 0; i < I965_MAX_NUM_SLICE; i++) {
        priv_state->slice_batch_offset[i] = 0;
        priv_state->slice_start_lcu[i] = 0;
    }

    return VA_STATUS_SUCCESS;
}

static VAStatus
gen9_hevc_pak_picture_level(VADriverContextP ctx,
                            struct encode_state *encode_state,
                            struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct encoder_vme_mfc_context *pak_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct generic_enc_codec_state *generic_state = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    struct gpe_mi_batch_buffer_start_parameter second_level_batch;
    struct hevc_encode_status_buffer *status_buffer = NULL;
    VAStatus va_status = VA_STATUS_SUCCESS;
    int i = 0;

    pak_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)pak_context->private_enc_ctx;
    generic_state = (struct generic_enc_codec_state *)pak_context->generic_enc_state;
    priv_state = (struct gen9_hevc_encoder_state *)pak_context->private_enc_state;
    status_buffer = &priv_state->status_buffer;

    if (generic_state->brc_enabled &&
        generic_state->curr_pak_pass) {
        gen9_hevc_conditional_end(ctx,
                                  batch, status_buffer->bo,
                                  status_buffer->status_image_mask_offset,
                                  0);

        gen9_hevc_load_reg_mem(ctx,
                               batch, status_buffer->bo,
                               status_buffer->status_image_ctrl_offset,
                               status_buffer->mmio_image_ctrl_offset);

        gen9_hevc_store_reg_mem(ctx, batch, priv_ctx->res_brc_pak_statistic_buffer.bo,
                                offsetof(GEN9_HEVC_PAK_STATES, HEVC_ENC_IMAGE_STATUS_CONTROL_FOR_LAST_PASS),
                                status_buffer->mmio_image_ctrl_offset);

        gen9_hevc_store_reg_mem(ctx, batch, status_buffer->bo,
                                status_buffer->status_image_ctrl_last_pass_offset,
                                status_buffer->mmio_image_ctrl_offset);
    }

    gen9_hevc_pak_pipe_mode_select(ctx, encode_state, encoder_context);
    gen9_hevc_pak_add_surface_state(ctx, encode_state, encoder_context,
                                    encode_state->input_yuv_object,
                                    GEN9_HEVC_ENC_SURFACE_SOURCE);
    gen9_hevc_pak_add_surface_state(ctx, encode_state, encoder_context,
                                    encode_state->reconstructed_object,
                                    GEN9_HEVC_ENC_SURFACE_RECON);
    gen9_hevc_pak_add_pipe_buf_addr_state(ctx, encode_state, encoder_context);
    gen9_hevc_pak_add_ind_obj_base_addr_state(ctx, encode_state, encoder_context);
    gen9_hevc_pak_add_qm_state(ctx, encode_state, encoder_context);
    gen9_hevc_pak_add_fm_state(ctx, encode_state, encoder_context);

    if (generic_state->brc_enabled) {
        memset(&second_level_batch, 0, sizeof(second_level_batch));

        second_level_batch.offset = generic_state->curr_pak_pass * priv_state->pic_state_size;
        second_level_batch.bo = priv_ctx->res_brc_pic_states_write_buffer.bo;
        second_level_batch.is_second_level = 1;
        gen8_gpe_mi_batch_buffer_start(ctx, batch, &second_level_batch);
    } else
        gen9_hevc_add_pic_state(ctx, encode_state, encoder_context, NULL, 0, 0);

    for (i = 0; i < encode_state->num_slice_params_ext; i++) {
        va_status = gen9_hevc_pak_slice_level(ctx, encode_state, encoder_context, i);
        if (va_status != VA_STATUS_SUCCESS)
            goto EXIT;
    }

EXIT:
    return va_status;
}

static void
gen9_hevc_pak_read_status(VADriverContextP ctx,
                          struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct encoder_vme_mfc_context *pak_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct generic_enc_codec_state *generic_state = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    struct gpe_mi_flush_dw_parameter mi_flush_dw_param;
    struct hevc_encode_status_buffer *status_buffer = NULL;

    pak_context = (struct encoder_vme_mfc_context *)encoder_context->vme_context;
    priv_ctx = (struct gen9_hevc_encoder_context *)pak_context->private_enc_ctx;
    generic_state = (struct generic_enc_codec_state *)pak_context->generic_enc_state;
    priv_state = (struct gen9_hevc_encoder_state *)pak_context->private_enc_state;

    status_buffer = &priv_state->status_buffer;

    memset(&mi_flush_dw_param, 0, sizeof(mi_flush_dw_param));
    gen8_gpe_mi_flush_dw(ctx, batch, &mi_flush_dw_param);

    gen9_hevc_store_reg_mem(ctx, batch, status_buffer->bo,
                            status_buffer->status_bs_byte_count_offset,
                            status_buffer->mmio_bs_frame_offset);

    gen9_hevc_store_reg_mem(ctx, batch, status_buffer->bo,
                            status_buffer->status_image_mask_offset,
                            status_buffer->mmio_image_mask_offset);

    gen9_hevc_store_reg_mem(ctx, batch, status_buffer->bo,
                            status_buffer->status_image_ctrl_offset,
                            status_buffer->mmio_image_ctrl_offset);

    if (generic_state->brc_enabled) {
        gen9_hevc_store_reg_mem(ctx, batch, priv_ctx->res_brc_pak_statistic_buffer.bo,
                                offsetof(GEN9_HEVC_PAK_STATES, HEVC_ENC_BYTECOUNT_FRAME),
                                status_buffer->mmio_bs_frame_offset);
        gen9_hevc_store_reg_mem(ctx, batch, priv_ctx->res_brc_pak_statistic_buffer.bo,
                                offsetof(GEN9_HEVC_PAK_STATES, HEVC_ENC_BYTECOUNT_FRAME_NOHEADER),
                                status_buffer->mmio_bs_frame_no_header_offset);
        gen9_hevc_store_reg_mem(ctx, batch, priv_ctx->res_brc_pak_statistic_buffer.bo,
                                offsetof(GEN9_HEVC_PAK_STATES, HEVC_ENC_IMAGE_STATUS_CONTROL),
                                status_buffer->mmio_image_ctrl_offset);
    }

    gen8_gpe_mi_flush_dw(ctx, batch, &mi_flush_dw_param);
}

static VAStatus
gen9_hevc_pak_pipeline(VADriverContextP ctx,
                       VAProfile profile,
                       struct encode_state *encode_state,
                       struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct encoder_vme_mfc_context *pak_context = encoder_context->vme_context;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct generic_enc_codec_state *generic_state = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;
    VAStatus va_status = VA_STATUS_SUCCESS;

    if (!pak_context || !pak_context->generic_enc_state || !batch) {
        va_status = VA_STATUS_ERROR_INVALID_CONTEXT;
        goto EXIT;
    }

    va_status = gen9_hevc_pak_pipeline_prepare(ctx, encode_state, encoder_context);
    if (va_status != VA_STATUS_SUCCESS)
        goto EXIT;

    if (i965->intel.has_bsd2)
        intel_batchbuffer_start_atomic_bcs_override(batch, 0x1000, BSD_RING0);
    else
        intel_batchbuffer_start_atomic_bcs(batch, 0x1000);

    intel_batchbuffer_emit_mi_flush(batch);

    generic_state = (struct generic_enc_codec_state *)pak_context->generic_enc_state;
    priv_state = (struct gen9_hevc_encoder_state *)pak_context->private_enc_state;
    priv_ctx = (struct gen9_hevc_encoder_context *)pak_context->private_enc_ctx;

    for (generic_state->curr_pak_pass = 0;
         generic_state->curr_pak_pass < generic_state->num_pak_passes;
         generic_state->curr_pak_pass++) {
        va_status = gen9_hevc_pak_picture_level(ctx, encode_state, encoder_context);
        if (va_status != VA_STATUS_SUCCESS)
            goto EXIT;

        gen9_hevc_pak_read_status(ctx, encoder_context);
    }

    if (priv_ctx->res_pak_slice_batch_buffer) {
        intel_batchbuffer_free(priv_ctx->res_pak_slice_batch_buffer);

        priv_ctx->res_pak_slice_batch_buffer = NULL;
    }

    intel_batchbuffer_end_atomic(batch);

    intel_batchbuffer_flush(batch);

    priv_state->frame_number++;

EXIT:
    return va_status;
}

static void
gen9_hevc_pak_context_destroy(void *context)
{
    struct encoder_vme_mfc_context *pak_context = (struct encoder_vme_mfc_context *)context;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    int i = 0;

    priv_ctx = (struct gen9_hevc_encoder_context *)pak_context->private_enc_ctx;

    if (priv_ctx->res_pak_slice_batch_buffer) {
        intel_batchbuffer_free(priv_ctx->res_pak_slice_batch_buffer);
        priv_ctx->res_pak_slice_batch_buffer = NULL;
    }

    dri_bo_unreference(priv_ctx->indirect_pak_bse_object.bo);
    priv_ctx->indirect_pak_bse_object.bo = NULL;

    if (priv_ctx->uncompressed_picture_source.obj_surface &&
        priv_ctx->uncompressed_picture_source.obj_surface->bo)
        i965_destroy_surface_storage(priv_ctx->uncompressed_picture_source.obj_surface);

    if (priv_ctx->reconstructed_object.obj_surface &&
        priv_ctx->reconstructed_object.obj_surface->bo)
        i965_destroy_surface_storage(priv_ctx->reconstructed_object.obj_surface);

    for (i = 0; i < GEN9_MAX_REF_SURFACES; i++)
        if (priv_ctx->reference_surfaces[i].obj_surface &&
            priv_ctx->reference_surfaces[i].obj_surface->bo)
            i965_destroy_surface_storage(priv_ctx->reference_surfaces[i].obj_surface);
}

#define STATUS_IMPLEMENTATION_START

static void
gen9_hevc_status_buffer_init(struct hevc_encode_status_buffer *status_buffer)
{
    uint32_t base_offset = offsetof(struct i965_coded_buffer_segment, codec_private_data);

    status_buffer->mmio_bs_frame_offset = MMIO_HCP_ENC_BITSTREAM_BYTECOUNT_FRAME_OFFSET;
    status_buffer->mmio_bs_frame_no_header_offset = MMIO_HCP_ENC_BITSTREAM_BYTECOUNT_FRAME_NO_HEADER_OFFSET;
    status_buffer->mmio_image_mask_offset = MMIO_HCP_ENC_IMAGE_STATUS_MASK_OFFSET;
    status_buffer->mmio_image_ctrl_offset = MMIO_HCP_ENC_IMAGE_STATUS_CTRL_OFFSET;

    status_buffer->status_image_mask_offset = base_offset +
                                              offsetof(struct hevc_encode_status, image_status_mask);
    status_buffer->status_image_ctrl_offset = base_offset +
                                              offsetof(struct hevc_encode_status, image_status_ctrl);
    status_buffer->status_image_ctrl_last_pass_offset = base_offset +
                                                        offsetof(struct hevc_encode_status, image_status_ctrl_last_pass);
    status_buffer->status_bs_byte_count_offset = base_offset +
                                                 offsetof(struct hevc_encode_status, bs_byte_count);
    status_buffer->status_media_state_offset = base_offset +
                                               offsetof(struct hevc_encode_status, media_state);
    status_buffer->status_pass_num_offset = base_offset +
                                            offsetof(struct hevc_encode_status, pass_num);
}

static VAStatus
gen9_hevc_get_coded_status(VADriverContextP ctx,
                           struct intel_encoder_context *encoder_context,
                           struct i965_coded_buffer_segment *coded_buf_seg)
{
    struct hevc_encode_status *encode_status;
    struct hevc_enc_image_status_ctrl *image_status_ctrl, *image_status_ctrl_last_pass;

    if (!encoder_context || !coded_buf_seg)
        return VA_STATUS_ERROR_INVALID_BUFFER;

    encode_status = (struct hevc_encode_status *)coded_buf_seg->codec_private_data;

    coded_buf_seg->base.size = encode_status->bs_byte_count;

    image_status_ctrl = (struct hevc_enc_image_status_ctrl *)&encode_status->image_status_ctrl;
    image_status_ctrl_last_pass = (struct hevc_enc_image_status_ctrl *)&encode_status->image_status_ctrl_last_pass;

    if (image_status_ctrl->total_pass && image_status_ctrl->cumulative_frame_delta_qp == 0)
        image_status_ctrl->cumulative_frame_delta_qp = image_status_ctrl_last_pass->cumulative_frame_delta_qp;

    image_status_ctrl_last_pass->cumulative_frame_delta_qp = 0;

    return VA_STATUS_SUCCESS;
}

// External initial APIs

Bool
gen9_hevc_vme_context_init(VADriverContextP ctx,
                           struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct encoder_vme_mfc_context *vme_context = NULL;
    struct gen9_hevc_encoder_context *priv_ctx = NULL;
    struct generic_enc_codec_state *generic_state = NULL;
    struct gen9_hevc_encoder_state *priv_state = NULL;

    hevc_enc_kernel_ptr = (void *)gen9_hevc_encoder_kernels;
    hevc_enc_kernel_size = sizeof(gen9_hevc_encoder_kernels);

    vme_context = calloc(1, sizeof(*vme_context));
    priv_ctx = calloc(1, sizeof(*priv_ctx));
    generic_state = calloc(1, sizeof(*generic_state));
    priv_state = calloc(1, sizeof(*priv_state));

    if (!vme_context || !generic_state ||
        !priv_ctx || !priv_state) {
        if (vme_context) free(vme_context);
        if (generic_state) free(generic_state);
        if (priv_ctx) free(priv_ctx);
        if (priv_state) free(priv_state);

        return false;
    }

    encoder_context->vme_context = (void *)vme_context;
    vme_context->private_enc_ctx = (void *)priv_ctx;
    vme_context->generic_enc_state = (void *)generic_state;
    vme_context->private_enc_state = (void *)priv_state;

    priv_ctx->ctx = ctx;
    priv_ctx->mocs = i965->intel.mocs_state;

    generic_state->num_pak_passes = 1;
    generic_state->brc_enabled = 0;

    priv_state->tu_mode = HEVC_TU_RT_SPEED;
    priv_state->use_hw_scoreboard = 1;
    priv_state->use_hw_non_stalling_scoreborad = 1;
    priv_state->rolling_intra_refresh = 0;
    priv_state->flatness_check_supported = 0;
    priv_state->walking_pattern_26 = 0;
    priv_state->num_regions_in_slice = 4;
    priv_state->frames_per_100s = 30000;
    priv_state->user_max_frame_size = 0;
    priv_state->brc_method = HEVC_BRC_CQP;
    priv_state->lcu_brc_enabled = 0;
    priv_state->parallel_brc = 0;
    priv_state->pak_obj_size = ((IS_KBL(i965->intel.device_info) || IS_GLK(i965->intel.device_info)) ?
                                GEN95_HEVC_ENC_PAK_OBJ_SIZE :
                                GEN9_HEVC_ENC_PAK_OBJ_SIZE) *
                               4;
    priv_state->cu_record_size = GEN9_HEVC_ENC_PAK_CU_RECORD_SIZE * 4;
    priv_state->pic_state_size = GEN9_HEVC_ENC_BRC_PIC_STATE_SIZE;

    gen9_hevc_status_buffer_init(&priv_state->status_buffer);
    gen9_hevc_vme_kernels_context_init(ctx, encoder_context);
    gen9_hevc_lambda_tables_init(priv_ctx);

    encoder_context->vme_pipeline = gen9_hevc_vme_pipeline;
    encoder_context->vme_context_destroy = gen9_hevc_vme_context_destroy;
    return true;
}

Bool
gen9_hevc_pak_context_init(VADriverContextP ctx,
                           struct intel_encoder_context *encoder_context)
{
    struct encoder_vme_mfc_context *pak_context = encoder_context->vme_context;

    if (!pak_context)
        return false;

    encoder_context->mfc_context = pak_context;
    encoder_context->mfc_context_destroy = gen9_hevc_pak_context_destroy;
    encoder_context->mfc_pipeline = gen9_hevc_pak_pipeline;
    encoder_context->mfc_brc_prepare = gen9_hevc_brc_prepare;
    encoder_context->get_status = gen9_hevc_get_coded_status;
    return true;
}
