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
 *    Zhao, Yakui <yakui.zhao@intel.com>
 *    Chen, Peng  <peng.c.chen@intel.com>
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
#include "intel_media.h"
#include "i965_defines.h"
#include "i965_drv_video.h"
#include "i965_encoder.h"
#include "i965_encoder_api.h"
#include "i965_encoder_utils.h"
#include "gen10_hcp_common.h"
#include "gen10_hevc_enc_common.h"
#include "gen10_hevc_enc_kernel.h"
#include "gen10_hevc_enc_const_def.h"
#include "gen10_hevc_enc_kernels_binary.h"
#include "gen10_hevc_encoder.h"

static bool
gen10_hevc_get_kernel_header_and_size(void *pvbinary,
                                      int binary_size,
                                      GEN10_HEVC_ENC_OPERATION operation,
                                      int krnstate_idx,
                                      struct i965_kernel *ret_kernel)
{
    typedef uint32_t BIN_PTR[4];

    gen10_hevc_kernel_header *pkh_table;
    gen10_intel_kernel_header *pcurr_header, *pinvalid_entry, *pnext_header;
    char *bin_start;
    int next_krnoffset;
    int not_found = 0;

    if (!pvbinary || !ret_kernel)
        return false;

    bin_start = (char *)pvbinary;
    pkh_table = (gen10_hevc_kernel_header *)pvbinary;
    pinvalid_entry = &(pkh_table->hevc_last) + 1;
    next_krnoffset = binary_size;

    switch (operation) {
    case GEN10_HEVC_ENC_SCALING_CONVERSION:
        pcurr_header = &pkh_table->hevc_ds_convert;
        break;
    case GEN10_HEVC_ENC_ME:
        pcurr_header = &pkh_table->hevc_hme;
        break;
    case GEN10_HEVC_ENC_BRC:
        switch (krnstate_idx) {
        case 0:
            pcurr_header = &pkh_table->hevc_brc_init;
            break;
        case 1:
            pcurr_header = &pkh_table->hevc_brc_init;
            break;
        case 2:
            pcurr_header = &pkh_table->hevc_brc_update;
            break;
        case 3:
            pcurr_header = &pkh_table->hevc_brc_lcuqp;
            break;
        default:
            not_found = 1;
            break;
        }
        break;

    case GEN10_HEVC_ENC_MBENC:
        switch (krnstate_idx) {
        case 0:
            pcurr_header = &pkh_table->hevc_intra;
            break;
        case 1:
            pcurr_header = &pkh_table->hevc_enc;
            break;
        case 2:
            pcurr_header = &pkh_table->hevc_enc_lcu64;
            break;
        default:
            not_found = 1;
            break;
        }

        break;
    default:
        not_found = 1;
        break;
    }

    if (not_found) {
        return false;
    }

    ret_kernel->bin = (const BIN_PTR *)(bin_start + (pcurr_header->kernel_start_pointer << 6));

    pnext_header = (pcurr_header + 1);
    if (pnext_header < pinvalid_entry)
        next_krnoffset = pnext_header->kernel_start_pointer << 6;

    ret_kernel->size = next_krnoffset - (pcurr_header->kernel_start_pointer << 6);

    return true;
}

#define MAX_HEVC_ENCODER_SURFACES        64
#define MAX_URB_SIZE                     4096
#define NUM_KERNELS_PER_GPE_CONTEXT      1

static void
gen10_hevc_init_gpe_context(VADriverContextP ctx,
                            struct i965_gpe_context *gpe_context,
                            struct gen10_hevc_enc_kernel_parameter *kernel_param)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);

    gpe_context->curbe.length = kernel_param->curbe_size; // in bytes

    gpe_context->sampler.entry_size = 0;
    gpe_context->sampler.max_entries = 0;
    if (kernel_param->sampler_size) {
        gpe_context->sampler.entry_size = kernel_param->sampler_size;
        gpe_context->sampler.max_entries = 1;
    }

    gpe_context->idrt.entry_size = ALIGN(sizeof(struct gen8_interface_descriptor_data), 64); // 8 dws, 1 register
    gpe_context->idrt.max_entries = NUM_KERNELS_PER_GPE_CONTEXT;

    gpe_context->surface_state_binding_table.max_entries = MAX_HEVC_ENCODER_SURFACES;
    gpe_context->surface_state_binding_table.binding_table_offset = 0;
    gpe_context->surface_state_binding_table.surface_state_offset = ALIGN(MAX_HEVC_ENCODER_SURFACES * 4, 64);
    gpe_context->surface_state_binding_table.length = ALIGN(MAX_HEVC_ENCODER_SURFACES * 4, 64) + ALIGN(MAX_HEVC_ENCODER_SURFACES * SURFACE_STATE_PADDED_SIZE_GEN9, 64);

    if (i965->intel.eu_total > 0)
        gpe_context->vfe_state.max_num_threads = 6 * i965->intel.eu_total;
    else
        gpe_context->vfe_state.max_num_threads = 112;

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
gen10_hevc_init_vfe_scoreboard(struct i965_gpe_context *gpe_context,
                               struct gen10_hevc_enc_scoreboard_parameter *scoreboard_param)
{
    if (!gpe_context || !scoreboard_param)
        return;

    gpe_context->vfe_desc5.scoreboard0.mask = scoreboard_param->mask;
    gpe_context->vfe_desc5.scoreboard0.type = scoreboard_param->type;
    gpe_context->vfe_desc5.scoreboard0.enable = scoreboard_param->enable;

    if (scoreboard_param->no_dependency) {
        gpe_context->vfe_desc5.scoreboard0.mask = 0x0;
        gpe_context->vfe_desc5.scoreboard0.enable = 0;
        gpe_context->vfe_desc5.scoreboard0.type = 0;

        gpe_context->vfe_desc6.dword = 0;
        gpe_context->vfe_desc7.dword = 0;
    } else {
        gpe_context->vfe_desc5.scoreboard0.mask = 0x7F;
        gpe_context->vfe_desc6.scoreboard1.delta_x0 = -1;
        gpe_context->vfe_desc6.scoreboard1.delta_y0 = 0;

        gpe_context->vfe_desc6.scoreboard1.delta_x1 = -1;
        gpe_context->vfe_desc6.scoreboard1.delta_y1 = -1;

        gpe_context->vfe_desc6.scoreboard1.delta_x2 = 0;
        gpe_context->vfe_desc6.scoreboard1.delta_y2 = -1;

        gpe_context->vfe_desc6.scoreboard1.delta_x3 = 1;
        gpe_context->vfe_desc6.scoreboard1.delta_y3 = -1;

        gpe_context->vfe_desc7.scoreboard2.delta_x4 = 0;
        gpe_context->vfe_desc7.scoreboard2.delta_y4 = 0;
        gpe_context->vfe_desc7.scoreboard2.delta_x5 = 0;
        gpe_context->vfe_desc7.scoreboard2.delta_y5 = 0;
        gpe_context->vfe_desc7.scoreboard2.delta_x6 = 0;
        gpe_context->vfe_desc7.scoreboard2.delta_y6 = 0;
        gpe_context->vfe_desc7.scoreboard2.delta_x7 = 0;
        gpe_context->vfe_desc7.scoreboard2.delta_y7 = 0;
    }
}

static void
gen10_hevc_vme_init_scaling_context(VADriverContextP ctx,
                                    struct gen10_hevc_enc_context *vme_context,
                                    struct gen10_scaling_context *scaling_context)
{
    struct gen10_hevc_enc_state *hevc_state;
    struct i965_gpe_context *gpe_context = NULL;
    struct gen10_hevc_enc_kernel_parameter kernel_param;
    struct gen10_hevc_enc_scoreboard_parameter scoreboard_param;
    struct i965_kernel scale_kernel;

    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;

    kernel_param.curbe_size = sizeof(gen10_hevc_scaling_curbe_data);
    kernel_param.inline_data_size = sizeof(gen10_hevc_scaling_curbe_data);
    kernel_param.sampler_size = 0;

    memset(&scoreboard_param, 0, sizeof(scoreboard_param));
    scoreboard_param.mask = 0xFF;
    scoreboard_param.enable = hevc_state->use_hw_scoreboard;
    scoreboard_param.type = hevc_state->use_hw_non_stalling_scoreboard;
    scoreboard_param.no_dependency = true;

    gpe_context = &scaling_context->gpe_context;
    gen10_hevc_init_gpe_context(ctx, gpe_context, &kernel_param);
    gen10_hevc_init_vfe_scoreboard(gpe_context, &scoreboard_param);

    memset(&scale_kernel, 0, sizeof(scale_kernel));

    gen10_hevc_get_kernel_header_and_size((void *)gen10_media_hevc_kernels,
                                          sizeof(gen10_media_hevc_kernels),
                                          GEN10_HEVC_ENC_SCALING_CONVERSION,
                                          0,
                                          &scale_kernel);

    gen8_gpe_load_kernels(ctx,
                          gpe_context,
                          &scale_kernel,
                          1);
}

static void
gen10_hevc_vme_init_me_context(VADriverContextP ctx,
                               struct gen10_hevc_enc_context *vme_context,
                               struct gen10_me_context *me_context)
{
    struct gen10_hevc_enc_state *hevc_state;
    struct i965_gpe_context *gpe_context = NULL;
    struct gen10_hevc_enc_kernel_parameter kernel_param;
    struct gen10_hevc_enc_scoreboard_parameter scoreboard_param;
    struct i965_kernel me_kernel;

    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;

    kernel_param.curbe_size = sizeof(gen10_hevc_me_curbe_data);
    kernel_param.inline_data_size = sizeof(gen10_hevc_me_curbe_data);
    kernel_param.sampler_size = 0;

    memset(&scoreboard_param, 0, sizeof(scoreboard_param));
    scoreboard_param.mask = 0xFF;
    scoreboard_param.enable = hevc_state->use_hw_scoreboard;
    scoreboard_param.type = hevc_state->use_hw_non_stalling_scoreboard;
    scoreboard_param.no_dependency = true;

    gpe_context = &me_context->gpe_context;
    gen10_hevc_init_gpe_context(ctx, gpe_context, &kernel_param);
    gen10_hevc_init_vfe_scoreboard(gpe_context, &scoreboard_param);

    memset(&me_kernel, 0, sizeof(me_kernel));

    gen10_hevc_get_kernel_header_and_size((void *)gen10_media_hevc_kernels,
                                          sizeof(gen10_media_hevc_kernels),
                                          GEN10_HEVC_ENC_ME,
                                          0,
                                          &me_kernel);

    gen8_gpe_load_kernels(ctx,
                          gpe_context,
                          &me_kernel,
                          1);
}

static void
gen10_hevc_vme_init_mbenc_context(VADriverContextP ctx,
                                  struct gen10_hevc_enc_context *vme_context,
                                  struct gen10_mbenc_context *mbenc_context)
{
    struct gen10_hevc_enc_state *hevc_state;
    struct i965_gpe_context *gpe_context = NULL;
    struct gen10_hevc_enc_kernel_parameter kernel_param;
    struct gen10_hevc_enc_scoreboard_parameter scoreboard_param;
    struct i965_kernel mbenc_kernel;

    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;

    memset(&scoreboard_param, 0, sizeof(scoreboard_param));
    scoreboard_param.mask = 0xFF;
    scoreboard_param.enable = hevc_state->use_hw_scoreboard;
    scoreboard_param.type = hevc_state->use_hw_non_stalling_scoreboard;

    gpe_context = &mbenc_context->gpe_contexts[GEN10_HEVC_MBENC_I_KRNIDX_G10];
    kernel_param.curbe_size = sizeof(gen10_hevc_mbenc_intra_curbe_data);
    kernel_param.inline_data_size = sizeof(gen10_hevc_mbenc_intra_curbe_data);
    kernel_param.sampler_size = 0;
    scoreboard_param.no_dependency = false;
    gen10_hevc_init_gpe_context(ctx, gpe_context, &kernel_param);

    gen10_hevc_init_vfe_scoreboard(gpe_context, &scoreboard_param);

    memset(&mbenc_kernel, 0, sizeof(mbenc_kernel));

    gen10_hevc_get_kernel_header_and_size((void *)gen10_media_hevc_kernels,
                                          sizeof(gen10_media_hevc_kernels),
                                          GEN10_HEVC_ENC_MBENC,
                                          GEN10_HEVC_MBENC_I_KRNIDX_G10,
                                          &  mbenc_kernel);

    gen8_gpe_load_kernels(ctx,
                          gpe_context,
                          &mbenc_kernel,
                          1);

    gpe_context = &mbenc_context->gpe_contexts[GEN10_HEVC_MBENC_INTER_LCU32_KRNIDX_G10];
    kernel_param.curbe_size = sizeof(gen10_hevc_mbenc_inter_curbe_data);
    kernel_param.inline_data_size = sizeof(gen10_hevc_mbenc_inter_curbe_data);
    kernel_param.sampler_size = 0;
    scoreboard_param.no_dependency = false;
    gen10_hevc_init_gpe_context(ctx, gpe_context, &kernel_param);
    gen10_hevc_init_vfe_scoreboard(gpe_context, &scoreboard_param);

    memset(&mbenc_kernel, 0, sizeof(mbenc_kernel));

    gen10_hevc_get_kernel_header_and_size((void *)gen10_media_hevc_kernels,
                                          sizeof(gen10_media_hevc_kernels),
                                          GEN10_HEVC_ENC_MBENC,
                                          GEN10_HEVC_MBENC_INTER_LCU32_KRNIDX_G10,
                                          &mbenc_kernel);
    gen8_gpe_load_kernels(ctx,
                          gpe_context,
                          &mbenc_kernel,
                          1);

    gpe_context = &mbenc_context->gpe_contexts[GEN10_HEVC_MBENC_INTER_LCU64_KRNIDX_G10];
    kernel_param.curbe_size = sizeof(gen10_hevc_mbenc_inter_curbe_data);
    kernel_param.inline_data_size = sizeof(gen10_hevc_mbenc_inter_curbe_data);
    kernel_param.sampler_size = 0;
    scoreboard_param.no_dependency = false;
    gen10_hevc_init_gpe_context(ctx, gpe_context, &kernel_param);
    gen10_hevc_init_vfe_scoreboard(gpe_context, &scoreboard_param);

    memset(&mbenc_kernel, 0, sizeof(mbenc_kernel));

    gen10_hevc_get_kernel_header_and_size((void *)gen10_media_hevc_kernels,
                                          sizeof(gen10_media_hevc_kernels),
                                          GEN10_HEVC_ENC_MBENC,
                                          GEN10_HEVC_MBENC_INTER_LCU64_KRNIDX_G10,
                                          &mbenc_kernel);

    gen8_gpe_load_kernels(ctx,
                          gpe_context,
                          &mbenc_kernel,
                          1);
}

static void
gen10_hevc_vme_init_brc_context(VADriverContextP ctx,
                                struct gen10_hevc_enc_context *vme_context,
                                struct gen10_brc_context *brc_context)
{
    struct gen10_hevc_enc_state *hevc_state;
    struct i965_gpe_context *gpe_context = NULL;
    struct gen10_hevc_enc_kernel_parameter kernel_param;
    struct gen10_hevc_enc_scoreboard_parameter scoreboard_param;
    struct i965_kernel brc_kernel;

    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;

    memset(&scoreboard_param, 0, sizeof(scoreboard_param));
    scoreboard_param.mask = 0xFF;
    scoreboard_param.enable = hevc_state->use_hw_scoreboard;
    scoreboard_param.type = hevc_state->use_hw_non_stalling_scoreboard;

    gpe_context = &brc_context->gpe_contexts[GEN10_HEVC_BRC_INIT];
    kernel_param.curbe_size = sizeof(gen10_hevc_brc_init_curbe_data);
    kernel_param.inline_data_size = sizeof(gen10_hevc_brc_init_curbe_data);
    kernel_param.sampler_size = 0;
    scoreboard_param.no_dependency = true;
    gen10_hevc_init_gpe_context(ctx, gpe_context, &kernel_param);
    gen10_hevc_init_vfe_scoreboard(gpe_context, &scoreboard_param);

    memset(&brc_kernel, 0, sizeof(brc_kernel));

    gen10_hevc_get_kernel_header_and_size((void *)gen10_media_hevc_kernels,
                                          sizeof(gen10_media_hevc_kernels),
                                          GEN10_HEVC_ENC_BRC,
                                          GEN10_HEVC_BRC_INIT,
                                          &brc_kernel);

    gen8_gpe_load_kernels(ctx,
                          gpe_context,
                          &brc_kernel,
                          1);

    gpe_context = &brc_context->gpe_contexts[GEN10_HEVC_BRC_RESET];
    kernel_param.curbe_size = sizeof(gen10_hevc_brc_init_curbe_data);
    kernel_param.inline_data_size = sizeof(gen10_hevc_brc_init_curbe_data);
    kernel_param.sampler_size = 0;
    scoreboard_param.no_dependency = true;
    gen10_hevc_init_gpe_context(ctx, gpe_context, &kernel_param);
    gen10_hevc_init_vfe_scoreboard(gpe_context, &scoreboard_param);

    memset(&brc_kernel, 0, sizeof(brc_kernel));

    gen10_hevc_get_kernel_header_and_size((void *)gen10_media_hevc_kernels,
                                          sizeof(gen10_media_hevc_kernels),
                                          GEN10_HEVC_ENC_BRC,
                                          GEN10_HEVC_BRC_RESET,
                                          &brc_kernel);

    gen8_gpe_load_kernels(ctx,
                          gpe_context,
                          &brc_kernel,
                          1);

    gpe_context = &brc_context->gpe_contexts[GEN10_HEVC_BRC_FRAME_UPDATE];
    kernel_param.curbe_size = sizeof(gen10_hevc_brc_update_curbe_data);
    kernel_param.inline_data_size = sizeof(gen10_hevc_brc_update_curbe_data);
    kernel_param.sampler_size = 0;
    scoreboard_param.no_dependency = true;
    gen10_hevc_init_gpe_context(ctx, gpe_context, &kernel_param);
    gen10_hevc_init_vfe_scoreboard(gpe_context, &scoreboard_param);

    memset(&brc_kernel, 0, sizeof(brc_kernel));

    gen10_hevc_get_kernel_header_and_size((void *)gen10_media_hevc_kernels,
                                          sizeof(gen10_media_hevc_kernels),
                                          GEN10_HEVC_ENC_BRC,
                                          GEN10_HEVC_BRC_FRAME_UPDATE,
                                          &brc_kernel);

    gen8_gpe_load_kernels(ctx,
                          gpe_context,
                          &brc_kernel,
                          1);

    gpe_context = &brc_context->gpe_contexts[GEN10_HEVC_BRC_LCU_UPDATE];
    kernel_param.curbe_size = sizeof(gen10_hevc_brc_update_curbe_data);
    kernel_param.inline_data_size = sizeof(gen10_hevc_brc_update_curbe_data);
    kernel_param.sampler_size = 0;
    scoreboard_param.no_dependency = true;
    gen10_hevc_init_gpe_context(ctx, gpe_context, &kernel_param);
    gen10_hevc_init_vfe_scoreboard(gpe_context, &scoreboard_param);

    memset(&brc_kernel, 0, sizeof(brc_kernel));

    gen10_hevc_get_kernel_header_and_size((void *)gen10_media_hevc_kernels,
                                          sizeof(gen10_media_hevc_kernels),
                                          GEN10_HEVC_ENC_BRC,
                                          GEN10_HEVC_BRC_LCU_UPDATE,
                                          &brc_kernel);

    gen8_gpe_load_kernels(ctx,
                          gpe_context,
                          &brc_kernel,
                          1);
}

static void
gen10_hevc_vme_init_kernels_context(VADriverContextP ctx,
                                    struct intel_encoder_context *encoder_context,
                                    struct gen10_hevc_enc_context *vme_context)
{
    gen10_hevc_vme_init_scaling_context(ctx, vme_context, &vme_context->scaling_context);
    gen10_hevc_vme_init_me_context(ctx, vme_context, &vme_context->me_context);
    gen10_hevc_vme_init_mbenc_context(ctx, vme_context, &vme_context->mbenc_context);
    gen10_hevc_vme_init_brc_context(ctx, vme_context, &vme_context->brc_context);
}

static void
gen10_hevc_free_surface(void **data)
{
    struct gen10_hevc_surface_priv *surface_priv;

    if (!data || !*data)
        return;

    surface_priv = *data;

    if (surface_priv->scaled_4x_surface) {
        i965_free_gpe_resource(&surface_priv->gpe_scaled_4x_surface);

        i965_DestroySurfaces(surface_priv->ctx, &surface_priv->scaled_4x_surface_id, 1);
        surface_priv->scaled_4x_surface_id = VA_INVALID_SURFACE;
        surface_priv->scaled_4x_surface = NULL;
    }

    if (surface_priv->scaled_16x_surface) {
        i965_free_gpe_resource(&surface_priv->gpe_scaled_16x_surface);

        i965_DestroySurfaces(surface_priv->ctx, &surface_priv->scaled_16x_surface_id, 1);
        surface_priv->scaled_16x_surface_id = VA_INVALID_SURFACE;
        surface_priv->scaled_16x_surface = NULL;
    }

    if (surface_priv->scaled_2x_surface) {
        i965_free_gpe_resource(&surface_priv->gpe_scaled_2x_surface);

        i965_DestroySurfaces(surface_priv->ctx, &surface_priv->scaled_2x_surface_id, 1);
        surface_priv->scaled_2x_surface_id = VA_INVALID_SURFACE;
        surface_priv->scaled_2x_surface = NULL;
    }

    if (surface_priv->converted_surface) {
        i965_free_gpe_resource(&surface_priv->gpe_converted_surface);

        i965_DestroySurfaces(surface_priv->ctx, &surface_priv->converted_surface_id, 1);
        surface_priv->converted_surface_id = VA_INVALID_SURFACE;
        surface_priv->converted_surface = NULL;
    }

    i965_free_gpe_resource(&surface_priv->motion_vector_temporal);

    free(surface_priv);

    *data = NULL;

    return;
}

static VAStatus
gen10_hevc_init_surface_priv(VADriverContextP ctx,
                             struct encode_state *encode_state,
                             struct intel_encoder_context *encoder_context,
                             struct object_surface *obj_surface)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct gen10_hevc_enc_context *vme_context = encoder_context->vme_context;
    struct gen10_hevc_enc_frame_info *frame_info;
    struct gen10_hevc_enc_state *hevc_state;
    struct gen10_hevc_surface_priv *surface_priv;
    int downscaled_width_4x = 0, downscaled_height_4x = 0;
    int downscaled_width_16x = 0, downscaled_height_16x = 0;
    int frame_width = 0, frame_height = 0, size;

    if (!obj_surface || !obj_surface->bo)
        return VA_STATUS_ERROR_INVALID_SURFACE;

    if (obj_surface->private_data &&
        obj_surface->free_private_data != gen10_hevc_free_surface) {
        obj_surface->free_private_data(&obj_surface->private_data);
        obj_surface->private_data = NULL;
    }

    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;
    frame_info = &vme_context->frame_info;

    if (obj_surface->private_data) {
        surface_priv = (struct gen10_hevc_surface_priv *)(obj_surface->private_data);

        if ((surface_priv->frame_width == frame_info->frame_width) &&
            (surface_priv->frame_height == frame_info->frame_height) &&
            (surface_priv->width_ctb == frame_info->width_in_lcu) &&
            (surface_priv->height_ctb == frame_info->height_in_lcu) &&
            (surface_priv->is_10bit == hevc_state->is_10bit) &&
            (surface_priv->is_64lcu == hevc_state->is_64lcu))
            return VA_STATUS_SUCCESS;

        obj_surface->free_private_data(&obj_surface->private_data);
        obj_surface->private_data = NULL;
        surface_priv = NULL;
    }

    surface_priv = calloc(1, sizeof(struct gen10_hevc_surface_priv));

    if (!surface_priv)
        return VA_STATUS_ERROR_ALLOCATION_FAILED;

    surface_priv->ctx = ctx;

    obj_surface->private_data = surface_priv;
    obj_surface->free_private_data = gen10_hevc_free_surface;

    if (hevc_state->is_64lcu) {
        frame_width = ALIGN(frame_info->frame_width, 64) >> 1;
        frame_height = ALIGN(frame_info->frame_height, 64) >> 1;

        if (i965_CreateSurfaces(ctx,
                                frame_width,
                                frame_height,
                                VA_RT_FORMAT_YUV420,
                                1,
                                &surface_priv->scaled_2x_surface_id) != VA_STATUS_SUCCESS)
            return VA_STATUS_ERROR_ALLOCATION_FAILED;

        surface_priv->scaled_2x_surface = SURFACE(surface_priv->scaled_2x_surface_id);

        if (!surface_priv->scaled_2x_surface)
            return VA_STATUS_ERROR_ALLOCATION_FAILED;

        i965_check_alloc_surface_bo(ctx, surface_priv->scaled_2x_surface, 1,
                                    VA_FOURCC('N', 'V', '1', '2'), SUBSAMPLE_YUV420);

        i965_object_surface_to_2d_gpe_resource(&surface_priv->gpe_scaled_2x_surface,
                                               surface_priv->scaled_2x_surface);
    }

    if (hevc_state->is_10bit) {
        if (i965_CreateSurfaces(ctx,
                                frame_info->frame_width,
                                frame_info->frame_height,
                                VA_RT_FORMAT_YUV420,
                                1,
                                &surface_priv->converted_surface_id) != VA_STATUS_SUCCESS)
            return VA_STATUS_ERROR_ALLOCATION_FAILED;

        surface_priv->converted_surface = SURFACE(surface_priv->converted_surface_id);

        if (!surface_priv->converted_surface)
            return VA_STATUS_ERROR_ALLOCATION_FAILED;

        i965_check_alloc_surface_bo(ctx, surface_priv->converted_surface, 1,
                                    VA_FOURCC('N', 'V', '1', '2'), SUBSAMPLE_YUV420);

        i965_object_surface_to_2d_gpe_resource(&surface_priv->gpe_converted_surface,
                                               surface_priv->converted_surface);
    }

    if (hevc_state->hme_supported) {
        downscaled_width_4x = ALIGN(frame_info->frame_width / 4, 32);
        downscaled_height_4x = ALIGN(frame_info->frame_height / 4, 32);

        if (i965_CreateSurfaces(ctx,
                                downscaled_width_4x,
                                downscaled_height_4x,
                                VA_RT_FORMAT_YUV420,
                                1,
                                &surface_priv->scaled_4x_surface_id) != VA_STATUS_SUCCESS)
            return VA_STATUS_ERROR_ALLOCATION_FAILED;

        surface_priv->scaled_4x_surface = SURFACE(surface_priv->scaled_4x_surface_id);

        if (!surface_priv->scaled_4x_surface)
            return VA_STATUS_ERROR_ALLOCATION_FAILED;

        i965_check_alloc_surface_bo(ctx, surface_priv->scaled_4x_surface, 1,
                                    VA_FOURCC('N', 'V', '1', '2'), SUBSAMPLE_YUV420);

        i965_object_surface_to_2d_gpe_resource(&surface_priv->gpe_scaled_4x_surface,
                                               surface_priv->scaled_4x_surface);
    }

    if (hevc_state->hme_supported &&
        hevc_state->b16xme_supported) {
        downscaled_width_16x = ALIGN(downscaled_width_4x / 4, 32);
        downscaled_height_16x = ALIGN(downscaled_height_4x / 4, 32);

        if (i965_CreateSurfaces(ctx,
                                downscaled_width_16x,
                                downscaled_height_16x,
                                VA_RT_FORMAT_YUV420,
                                1,
                                &surface_priv->scaled_16x_surface_id) != VA_STATUS_SUCCESS)
            return VA_STATUS_ERROR_ALLOCATION_FAILED;

        surface_priv->scaled_16x_surface = SURFACE(surface_priv->scaled_16x_surface_id);

        if (!surface_priv->scaled_16x_surface)
            return VA_STATUS_ERROR_ALLOCATION_FAILED;

        i965_check_alloc_surface_bo(ctx, surface_priv->scaled_16x_surface, 1,
                                    VA_FOURCC('N', 'V', '1', '2'), SUBSAMPLE_YUV420);

        i965_object_surface_to_2d_gpe_resource(&surface_priv->gpe_scaled_16x_surface,
                                               surface_priv->scaled_16x_surface);
    }

    frame_width = frame_info->frame_width;
    frame_height = frame_info->frame_height;

    size = MAX(((frame_width + 63) >> 6) * ((frame_height + 15) >> 4),
               ((frame_width + 31) >> 5) * ((frame_height + 31) >> 5));
    size = ALIGN(size, 2) * 64;
    if (!i965_allocate_gpe_resource(i965->intel.bufmgr,
                                    &surface_priv->motion_vector_temporal,
                                    size,
                                    "Motion vector temporal buffer"))
        return VA_STATUS_ERROR_ALLOCATION_FAILED;

    surface_priv->is_10bit = hevc_state->is_10bit;
    surface_priv->is_64lcu = hevc_state->is_64lcu;
    surface_priv->frame_width = frame_info->frame_width;
    surface_priv->frame_height = frame_info->frame_height;
    surface_priv->width_ctb = frame_info->width_in_lcu;
    surface_priv->height_ctb = frame_info->height_in_lcu;

    return VA_STATUS_SUCCESS;
}

static void
gen10_hevc_free_enc_resources(void *context)
{
    struct gen10_hevc_enc_context *vme_context = context;

    if (!vme_context)
        return;

    i965_free_gpe_resource(&vme_context->res_mb_code_surface);

    i965_free_gpe_resource(&vme_context->res_temp_curecord_lcu32_surface);
    i965_free_gpe_resource(&vme_context->res_16x16_qp_data_surface);
    i965_free_gpe_resource(&vme_context->res_lculevel_input_data_buffer);
    i965_free_gpe_resource(&vme_context->res_concurrent_tg_data);
    i965_free_gpe_resource(&vme_context->res_cu_split_surface);
    i965_free_gpe_resource(&vme_context->res_kernel_trace_data);
    i965_free_gpe_resource(&vme_context->res_enc_const_table_intra);
    i965_free_gpe_resource(&vme_context->res_enc_const_table_inter);
    i965_free_gpe_resource(&vme_context->res_enc_const_table_inter_lcu64);
    i965_free_gpe_resource(&vme_context->res_scratch_surface);

    i965_free_gpe_resource(&vme_context->res_temp2_curecord_lcu32_surface);
    i965_free_gpe_resource(&vme_context->res_temp_curecord_surface_lcu64);
    i965_free_gpe_resource(&vme_context->res_enc_scratch_buffer);
    i965_free_gpe_resource(&vme_context->res_enc_scratch_lcu64_buffer);
    i965_free_gpe_resource(&vme_context->res_64x64_dist_buffer);

    i965_free_gpe_resource(&vme_context->res_jbq_header_buffer);
    i965_free_gpe_resource(&vme_context->res_jbq_header_lcu64_buffer);
    i965_free_gpe_resource(&vme_context->res_jbq_data_lcu32_surface);
    i965_free_gpe_resource(&vme_context->res_jbq_data_lcu64_surface);
    i965_free_gpe_resource(&vme_context->res_residual_scratch_lcu32_surface);

    i965_free_gpe_resource(&vme_context->res_residual_scratch_lcu64_surface);
    i965_free_gpe_resource(&vme_context->res_mb_stat_surface);
    i965_free_gpe_resource(&vme_context->res_mb_split_surface);

    i965_free_gpe_resource(&vme_context->res_s4x_memv_data_surface);
    i965_free_gpe_resource(&vme_context->res_s4x_me_dist_surface);

    i965_free_gpe_resource(&vme_context->res_s16x_memv_data_surface);
    i965_free_gpe_resource(&vme_context->res_mv_dist_sum_buffer);

    i965_free_gpe_resource(&vme_context->res_brc_me_dist_surface);
    i965_free_gpe_resource(&vme_context->res_brc_input_enc_kernel_buffer);
    i965_free_gpe_resource(&vme_context->res_brc_history_buffer);
    i965_free_gpe_resource(&vme_context->res_brc_intra_dist_surface);
    i965_free_gpe_resource(&vme_context->res_brc_pak_statistics_buffer[0]);
    i965_free_gpe_resource(&vme_context->res_brc_pak_statistics_buffer[1]);
    i965_free_gpe_resource(&vme_context->res_brc_pic_image_state_write_buffer);
    i965_free_gpe_resource(&vme_context->res_brc_pic_image_state_read_buffer);
    i965_free_gpe_resource(&vme_context->res_brc_const_data_surface);
    i965_free_gpe_resource(&vme_context->res_brc_lcu_const_data_buffer);
    i965_free_gpe_resource(&vme_context->res_brc_mb_qp_surface);
}

static VAStatus
gen10_hevc_allocate_enc_resources(VADriverContextP ctx,
                                  struct encode_state *encode_state,
                                  struct intel_encoder_context *encoder_context)

{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct gen10_hevc_enc_context *vme_context;
    struct gen10_hevc_enc_state *hevc_state;
    struct gen10_hevc_enc_frame_info *frame_info;
    int dw_width, dw_height;
    int allocate_flag;
    int res_size;
    int i;

    vme_context = (struct gen10_hevc_enc_context *)encoder_context->vme_context;
    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;
    frame_info = &vme_context->frame_info;

    i965_free_gpe_resource(&vme_context->res_mb_code_surface);
    res_size = vme_context->frame_info.width_in_lcu * vme_context->frame_info.height_in_lcu;
    if (hevc_state->is_64lcu)
        res_size = res_size * 64 * 32;
    else
        res_size = res_size * 16 * 32;

    res_size = res_size + hevc_state->cu_records_offset;
    res_size = ALIGN(res_size, 4096);
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_mb_code_surface,
                                               res_size,
                                               "Mb Code_Surface");
    if (!allocate_flag)
        goto FAIL;

    i965_free_gpe_resource(&vme_context->res_temp_curecord_lcu32_surface);
    dw_width = ALIGN(hevc_state->frame_width, 64);
    dw_height = ALIGN(hevc_state->frame_height, 64);
    dw_width = ALIGN(dw_width, 64);
    res_size = dw_width * dw_height * 64 + 1024;
    allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                                  &vme_context->res_temp_curecord_lcu32_surface,
                                                  dw_width, dw_height, dw_width,
                                                  "Temp CURecord surfaces");
    if (!allocate_flag)
        goto FAIL;

    i965_free_gpe_resource(&vme_context->res_16x16_qp_data_surface);
    dw_width = ALIGN(hevc_state->frame_width, 64) >> 4;
    dw_height = ALIGN(hevc_state->frame_height, 64) >> 4;
    dw_width = ALIGN(dw_width, 64);
    dw_height = ALIGN(dw_height, 64);
    dw_width = ALIGN(dw_width, 64);
    allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                                  &vme_context->res_16x16_qp_data_surface,
                                                  dw_width, dw_height, dw_width,
                                                  "CU 16x16 input surface");
    if (!allocate_flag)
        goto FAIL;

    i965_free_gpe_resource(&vme_context->res_lculevel_input_data_buffer);
    res_size = vme_context->frame_info.width_in_lcu * vme_context->frame_info.height_in_lcu * 16;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_lculevel_input_data_buffer,
                                               res_size,
                                               "LCU Input data buffer");
    if (!allocate_flag)
        goto FAIL;

    i965_free_gpe_resource(&vme_context->res_concurrent_tg_data);
    res_size = 16 * 256;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_concurrent_tg_data,
                                               res_size,
                                               "Concurrent Thread_group data");
    if (!allocate_flag)
        goto FAIL;

    i965_free_gpe_resource(&vme_context->res_cu_split_surface);
    dw_width = ALIGN(hevc_state->frame_width, 64) >> 4;
    dw_height = ALIGN(hevc_state->frame_height, 64) >> 4;
    dw_width = ALIGN(dw_width, 64);
    allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                                  &vme_context->res_cu_split_surface,
                                                  dw_width, dw_height, dw_width,
                                                  "CU split surface");
    if (!allocate_flag)
        goto FAIL;

    i965_free_gpe_resource(&vme_context->res_kernel_trace_data);
    res_size = 4096;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_kernel_trace_data,
                                               res_size,
                                               "Kernel trace");
    if (!allocate_flag)
        goto FAIL;

    i965_free_gpe_resource(&vme_context->res_enc_const_table_intra);
    res_size = GEN10_HEVC_ENC_INTRA_CONST_LUT_SIZE ;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_enc_const_table_intra,
                                               res_size,
                                               "Constant data for Intra");
    if (!allocate_flag)
        goto FAIL;

    i965_free_gpe_resource(&vme_context->res_enc_const_table_inter);
    res_size = GEN10_HEVC_ENC_INTER_CONST_LUT32_SIZE ;

    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_enc_const_table_inter,
                                               res_size,
                                               "Constant data for Inter");
    if (!allocate_flag)
        goto FAIL;

    i965_free_gpe_resource(&vme_context->res_enc_const_table_inter_lcu64);
    if (hevc_state->is_64lcu) {
        res_size = GEN10_HEVC_ENC_INTER_CONST_LUT64_SIZE ;

        allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                                   &vme_context->res_enc_const_table_inter_lcu64,
                                                   res_size,
                                                   "Constant data for LCU64_Inter");
        if (!allocate_flag)
            goto FAIL;
    }

    i965_free_gpe_resource(&vme_context->res_scratch_surface);
    dw_width = ALIGN(hevc_state->frame_width, 64) >> 3;
    dw_height = ALIGN(hevc_state->frame_height, 64) >> 5;
    dw_width = ALIGN(dw_width, 64);
    allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                                  &vme_context->res_scratch_surface,
                                                  dw_width, dw_height, dw_width,
                                                  "CU scratch surface");
    if (!allocate_flag)
        goto FAIL;

    i965_free_gpe_resource(&vme_context->res_temp2_curecord_lcu32_surface);
    dw_width = ALIGN(hevc_state->frame_width, 64);
    dw_height = ALIGN(hevc_state->frame_height, 64);
    dw_width = ALIGN(dw_width, 64);
    allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                                  &vme_context->res_temp2_curecord_lcu32_surface,
                                                  dw_width, dw_height, dw_width,
                                                  "second temp CURecord surfaces");
    if (!allocate_flag)
        goto FAIL;

    if (hevc_state->is_64lcu) {
        i965_free_gpe_resource(&vme_context->res_temp_curecord_surface_lcu64);
        /* the max number of CU based on 8x8. */
        dw_width = ALIGN(hevc_state->frame_width, 64);
        dw_height = ALIGN(hevc_state->frame_height, 64) / 2;
        dw_width = ALIGN(dw_width, 64);
        allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                                      &vme_context->res_temp_curecord_surface_lcu64,
                                                      dw_width, dw_height, dw_width,
                                                      "temp CURecord LCU64 surfaces");
        if (!allocate_flag)
            goto FAIL;
    }

    i965_free_gpe_resource(&vme_context->res_enc_scratch_buffer);
    dw_width = ALIGN(hevc_state->frame_width, 64) >> 5;
    dw_height = ALIGN(hevc_state->frame_height, 64) >> 5;
    res_size = dw_width * dw_height * 13312 + 4096;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_enc_scratch_buffer,
                                               res_size,
                                               "Enc Scratch data");
    if (!allocate_flag)
        goto FAIL;

    i965_free_gpe_resource(&vme_context->res_enc_scratch_lcu64_buffer);
    dw_width = vme_context->frame_info.width_in_lcu;
    dw_height = vme_context->frame_info.height_in_lcu;
    res_size = dw_width * dw_height * 13312;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_enc_scratch_lcu64_buffer,
                                               res_size,
                                               "Enc Scratch data");
    if (!allocate_flag)
        goto FAIL;

    i965_free_gpe_resource(&vme_context->res_64x64_dist_buffer);
    dw_width = ALIGN(hevc_state->frame_width, 64) >> 6;
    dw_height = ALIGN(hevc_state->frame_height, 64) >> 6;
    res_size = dw_width * dw_height * 32;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_64x64_dist_buffer,
                                               res_size,
                                               "Res 64x64 Distortion");
    if (!allocate_flag)
        goto FAIL;

    i965_free_gpe_resource(&vme_context->res_jbq_header_buffer);
    dw_width = ALIGN(hevc_state->frame_width, 64) >> 5;
    dw_height = ALIGN(hevc_state->frame_height, 64) >> 5;
    res_size = dw_width * dw_height * 2656;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_jbq_header_buffer,
                                               res_size,
                                               "Job queue_header");
    if (!allocate_flag)
        goto FAIL;

    i965_free_gpe_resource(&vme_context->res_jbq_header_lcu64_buffer);
    dw_width = ALIGN(hevc_state->frame_width, 64) >> 5;
    dw_height = ALIGN(hevc_state->frame_height, 64) >> 5;
    res_size = dw_width * dw_height * 32;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_jbq_header_lcu64_buffer,
                                               res_size,
                                               "Job queue_header for Multi-thread LCU");
    if (!allocate_flag)
        goto FAIL;

    i965_free_gpe_resource(&vme_context->res_jbq_data_lcu32_surface);
    dw_width = ALIGN(hevc_state->frame_width, 64);
    dw_height = (ALIGN(hevc_state->frame_height, 64) >> 5) * 58;
    allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                                  &vme_context->res_jbq_data_lcu32_surface,
                                                  dw_width, dw_height, dw_width,
                                                  "Job queue data surface for Multi-thread LCU32");
    if (!allocate_flag)
        goto FAIL;

    i965_free_gpe_resource(&vme_context->res_jbq_data_lcu64_surface);
    dw_width = ALIGN(hevc_state->frame_width, 64) >> 1;
    dw_height = (ALIGN(hevc_state->frame_height, 64) >> 6) * 66;
    dw_width = ALIGN(dw_width, 64);
    allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                                  &vme_context->res_jbq_data_lcu64_surface,
                                                  dw_width, dw_height, dw_width,
                                                  "Job queue data surface for Multi-thread LCU64");
    if (!allocate_flag)
        goto FAIL;

    i965_free_gpe_resource(&vme_context->res_residual_scratch_lcu32_surface);
    dw_width = ALIGN(hevc_state->frame_width, 64) << 1;
    dw_height = ALIGN(hevc_state->frame_height, 64) << 2;
    dw_width = ALIGN(dw_width, 64);
    allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                                  &vme_context->res_residual_scratch_lcu32_surface,
                                                  dw_width, dw_height, dw_width,
                                                  "Resiudal scratch for LCU32");
    if (!allocate_flag)
        goto FAIL;

    i965_free_gpe_resource(&vme_context->res_residual_scratch_lcu64_surface);
    dw_width = ALIGN(hevc_state->frame_width, 64) << 1;
    dw_height = ALIGN(hevc_state->frame_height, 64) << 2;
    dw_width = ALIGN(dw_width, 64);
    allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                                  &vme_context->res_residual_scratch_lcu64_surface,
                                                  dw_width, dw_height, dw_width,
                                                  "Resiudal scratch for LCU64");
    if (!allocate_flag)
        goto FAIL;

    i965_free_gpe_resource(&vme_context->res_mb_stat_surface);
    dw_width = ALIGN(frame_info->width_in_mb * 4, 64);
    dw_height = ALIGN(frame_info->height_in_mb, 8) * 2;
    dw_width = ALIGN(dw_width, 64);
    allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                                  &vme_context->res_mb_stat_surface,
                                                  dw_width, dw_height, dw_width,
                                                  "MB 16x16 stat");
    if (!allocate_flag)
        goto FAIL;

    i965_free_gpe_resource(&vme_context->res_mb_split_surface);
    dw_width = ALIGN(hevc_state->frame_width, 64) >> 2;
    dw_height = ALIGN(hevc_state->frame_height, 64) >> 4;
    dw_width = ALIGN(dw_width, 64);
    allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                                  &vme_context->res_mb_split_surface,
                                                  dw_width, dw_height, dw_width,
                                                  "MB split surface");
    if (!allocate_flag)
        goto FAIL;

    if (hevc_state->hme_supported) {
        i965_free_gpe_resource(&vme_context->res_s4x_memv_data_surface);
        dw_width = hevc_state->frame_width_4x * 4;
        dw_height = hevc_state->frame_height_4x >> 3;
        dw_width = ALIGN(dw_width, 64);
        allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                                      &vme_context->res_s4x_memv_data_surface,
                                                      dw_width, dw_height, dw_width,
                                                      "HME MEMV Data");
        if (!allocate_flag)
            goto FAIL;

        i965_free_gpe_resource(&vme_context->res_s4x_me_dist_surface);
        dw_width = hevc_state->frame_width_4x;
        dw_height = hevc_state->frame_height_4x >> 1;
        dw_width = ALIGN(dw_width, 64);
        allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                                      &vme_context->res_s4x_me_dist_surface,
                                                      dw_width, dw_height, dw_width,
                                                      "HME Distorion");
        if (!allocate_flag)
            goto FAIL;
    }

    if (hevc_state->hme_supported &&
        hevc_state->b16xme_supported) {
        i965_free_gpe_resource(&vme_context->res_s16x_memv_data_surface);
        dw_width = hevc_state->frame_width_16x * 4;
        dw_height = hevc_state->frame_height_16x >> 3;
        dw_width = ALIGN(dw_width, 64);
        allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                                      &vme_context->res_s16x_memv_data_surface,
                                                      dw_width, dw_height, dw_width,
                                                      "16xME MEMV Data");
        if (!allocate_flag)
            goto FAIL;
    }

    i965_free_gpe_resource(&vme_context->res_mv_dist_sum_buffer);
    res_size = 64;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_mv_dist_sum_buffer,
                                               res_size,
                                               "MV_DIST_sum");
    if (!allocate_flag)
        goto FAIL;

    i965_free_gpe_resource(&vme_context->res_brc_me_dist_surface);
    dw_width = ALIGN(hevc_state->frame_width, 64) >> 4;
    dw_width = ALIGN(dw_width, 64);
    dw_height = ALIGN(hevc_state->frame_height, 64) >> 4;
    dw_height = ALIGN(dw_height, 64);
    allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                                  &vme_context->res_brc_me_dist_surface,
                                                  dw_width, dw_height, dw_width,
                                                  "ME BRC distortion");
    if (!allocate_flag)
        goto FAIL;

    i965_free_gpe_resource(&vme_context->res_brc_input_enc_kernel_buffer);
    res_size = 1024;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_brc_input_enc_kernel_buffer,
                                               res_size,
                                               "Brc Input for Enc Kernel");
    if (!allocate_flag)
        goto FAIL;

    i965_free_gpe_resource(&vme_context->res_brc_history_buffer);
    res_size = GEN10_HEVC_BRC_HISTORY_BUFFER_SIZE;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_brc_history_buffer,
                                               res_size,
                                               "Brc History buffer");
    if (!allocate_flag)
        goto FAIL;

    i965_zero_gpe_resource(&vme_context->res_brc_history_buffer);

    i965_free_gpe_resource(&vme_context->res_brc_intra_dist_surface);
    dw_width = ALIGN(hevc_state->frame_width_4x / 2, 64);
    dw_height = ALIGN(hevc_state->frame_height_4x / 4, 8) * 2;
    allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                                  &vme_context->res_brc_intra_dist_surface,
                                                  dw_width, dw_height, dw_width,
                                                  "Brc Intra distortion buffer");
    if (!allocate_flag)
        goto FAIL;

    i965_zero_gpe_resource(&vme_context->res_brc_intra_dist_surface);

    for (i = 0; i < 2; i++) {
        i965_free_gpe_resource(&vme_context->res_brc_pak_statistics_buffer[i]);
        res_size = 64;
        allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                                   &vme_context->res_brc_pak_statistics_buffer[i],
                                                   res_size,
                                                   "Brc Pak statistics buffer");
        if (!allocate_flag)
            goto FAIL;
    }

    i965_free_gpe_resource(&vme_context->res_brc_pic_image_state_write_buffer);
    res_size = GEN10_HEVC_BRC_IMG_STATE_SIZE_PER_PASS * 8;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_brc_pic_image_state_write_buffer,
                                               res_size,
                                               "Brc Pic State Write buffer");
    if (!allocate_flag)
        goto FAIL;

    i965_free_gpe_resource(&vme_context->res_brc_pic_image_state_read_buffer);
    res_size = GEN10_HEVC_BRC_IMG_STATE_SIZE_PER_PASS * 8;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_brc_pic_image_state_read_buffer,
                                               res_size,
                                               "Brc Pic State Read buffer");
    if (!allocate_flag)
        goto FAIL;

    i965_free_gpe_resource(&vme_context->res_brc_const_data_surface);
    dw_width = ALIGN(GEN10_HEVC_BRC_CONST_SURFACE_WIDTH, 64);
    dw_height = ALIGN(GEN10_HEVC_BRC_CONST_SURFACE_HEIGHT, 32);
    allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                                  &vme_context->res_brc_const_data_surface,
                                                  dw_width, dw_height, dw_width,
                                                  "Brc Const data buffer");
    if (!allocate_flag)
        goto FAIL;

    i965_free_gpe_resource(&vme_context->res_brc_lcu_const_data_buffer);
    res_size = 4096;
    allocate_flag = i965_allocate_gpe_resource(i965->intel.bufmgr,
                                               &vme_context->res_brc_lcu_const_data_buffer,
                                               res_size,
                                               "BRC LCU Const_data buffer");
    if (!allocate_flag)
        goto FAIL;

    i965_zero_gpe_resource(&vme_context->res_brc_lcu_const_data_buffer);

    i965_free_gpe_resource(&vme_context->res_brc_mb_qp_surface);
    dw_width = ALIGN(hevc_state->frame_width_4x * 4, 64) >> 4;
    dw_height = ALIGN(hevc_state->frame_height_4x * 4, 64) >> 5;

    dw_width = ALIGN(dw_width, 64);
    dw_height = ALIGN(dw_height, 8);
    allocate_flag = i965_gpe_allocate_2d_resource(i965->intel.bufmgr,
                                                  &vme_context->res_brc_mb_qp_surface,
                                                  dw_width, dw_height, dw_width,
                                                  "Brc LCU qp data buffer");
    if (!allocate_flag)
        goto FAIL;

    i965_zero_gpe_resource(&vme_context->res_brc_mb_qp_surface);

    return VA_STATUS_SUCCESS;

FAIL:
    return VA_STATUS_ERROR_ALLOCATION_FAILED;
}

static VAStatus
gen10_hevc_enc_init_const_resources(VADriverContextP ctx,
                                    struct encode_state *encode_state,
                                    struct intel_encoder_context *encoder_context)
{
    struct gen10_hevc_enc_context *vme_context = encoder_context->vme_context;
    struct gen10_hevc_enc_state *hevc_state;
    char *buffer_ptr;

    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;

    buffer_ptr = i965_map_gpe_resource(&vme_context->res_enc_const_table_intra);
    if (!buffer_ptr)
        return VA_STATUS_ERROR_OPERATION_FAILED;

    memcpy(buffer_ptr, gen10_hevc_enc_intra_const_lut,
           GEN10_HEVC_ENC_INTRA_CONST_LUT_SIZE);

    i965_unmap_gpe_resource(&vme_context->res_enc_const_table_intra);

    buffer_ptr = i965_map_gpe_resource(&vme_context->res_enc_const_table_inter);
    if (!buffer_ptr)
        return VA_STATUS_ERROR_OPERATION_FAILED;

    memcpy(buffer_ptr, gen10_hevc_enc_inter_const_lut32,
           GEN10_HEVC_ENC_INTER_CONST_LUT32_SIZE);

    i965_unmap_gpe_resource(&vme_context->res_enc_const_table_inter);

    if (hevc_state->is_64lcu) {
        buffer_ptr = i965_map_gpe_resource(&vme_context->res_enc_const_table_inter_lcu64);
        if (!buffer_ptr)
            return VA_STATUS_ERROR_OPERATION_FAILED;

        memcpy(buffer_ptr, gen10_hevc_enc_inter_const_lut64,
               GEN10_HEVC_ENC_INTER_CONST_LUT64_SIZE);

        i965_unmap_gpe_resource(&vme_context->res_enc_const_table_inter_lcu64);
    }

    buffer_ptr = i965_map_gpe_resource(&vme_context->res_brc_const_data_surface);
    if (!buffer_ptr)
        return VA_STATUS_ERROR_OPERATION_FAILED;

    memcpy(buffer_ptr, gen10_hevc_brc_qp_adjust_data, GEN10_HEVC_BRC_QP_ADJUST_SIZE);

    buffer_ptr += GEN10_HEVC_BRC_QP_ADJUST_SIZE;

    if (hevc_state->is_64lcu)
        memcpy(buffer_ptr, gen10_hevc_brc_lcu64_lambda_cost, GEN10_HEVC_BRC_LCU_LAMBDA_COST);
    else
        memcpy(buffer_ptr, gen10_hevc_brc_lcu32_lambda_cost, GEN10_HEVC_BRC_LCU_LAMBDA_COST);

    i965_unmap_gpe_resource(&vme_context->res_brc_const_data_surface);

    return VA_STATUS_SUCCESS;
}

static VAStatus
gen10_hevc_enc_check_parameters(VADriverContextP ctx,
                                struct encode_state *encode_state,
                                struct intel_encoder_context *encoder_context)
{
    VAEncSequenceParameterBufferHEVC *seq_param;
    VAEncPictureParameterBufferHEVC *pic_param;
    VAEncSliceParameterBufferHEVC *slice_param;
    int i = 0, j = 0;

    seq_param = (VAEncSequenceParameterBufferHEVC *)encode_state->seq_param_ext->buffer;
    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;
    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[0]->buffer;

    for (i = 0; i < encode_state->num_slice_params_ext; i++) {
        slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[i]->buffer;

        if (slice_param->slice_fields.bits.slice_temporal_mvp_enabled_flag &&
            slice_param->slice_fields.bits.collocated_from_l0_flag &&
            (pic_param->collocated_ref_pic_index == 0xff ||
             pic_param->collocated_ref_pic_index > GEN10_MAX_REF_SURFACES))
            slice_param->slice_fields.bits.slice_temporal_mvp_enabled_flag = 0;

        if (slice_param->num_ref_idx_l0_active_minus1 > GEN10_HEVC_NUM_MAX_REF_L0 - 1 ||
            slice_param->num_ref_idx_l1_active_minus1 > GEN10_HEVC_NUM_MAX_REF_L1 - 1)
            return VA_STATUS_ERROR_ATTR_NOT_SUPPORTED;

        if (slice_param->slice_type == HEVC_SLICE_P)
            return VA_STATUS_ERROR_ATTR_NOT_SUPPORTED;
    }

    i = seq_param->log2_diff_max_min_luma_coding_block_size +
        seq_param->log2_min_luma_coding_block_size_minus3 + 3;
    if (i < GEN10_HEVC_LOG2_MIN_HEVC_LCU ||
        i > GEN10_HEVC_LOG2_MAX_HEVC_LCU)
        return VA_STATUS_ERROR_ATTR_NOT_SUPPORTED;

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

    if (seq_param->seq_fields.bits.chroma_format_idc != 1)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    return VA_STATUS_SUCCESS;
}

static VAStatus
gen10_hevc_enc_init_misc_paramers(VADriverContextP ctx,
                                  struct encode_state *encode_state,
                                  struct intel_encoder_context *encoder_context)
{
    struct gen10_hevc_enc_context *vme_context = NULL;
    struct gen10_hevc_enc_state *hevc_state;
    struct gen10_hevc_enc_frame_info *frame_info;
    VAEncSequenceParameterBufferHEVC *seq_param;
    VAEncSliceParameterBufferHEVC *slice_param;
    uint32_t brc_method, brc_reset;

    vme_context = (struct gen10_hevc_enc_context *) encoder_context->vme_context;
    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;
    frame_info = &vme_context->frame_info;
    seq_param = (VAEncSequenceParameterBufferHEVC *)encode_state->seq_param_ext->buffer;
    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[0]->buffer;

    hevc_state->low_delay = frame_info->low_delay;

    hevc_state->frame_width = frame_info->frame_width;
    hevc_state->frame_height = frame_info->frame_height;

    hevc_state->frame_width_2x = ALIGN(frame_info->frame_width / 2, 32);
    hevc_state->frame_height_2x = ALIGN(frame_info->frame_height / 2, 32);

    hevc_state->frame_width_4x = ALIGN(frame_info->frame_width / 4, 32);
    hevc_state->frame_height_4x = ALIGN(frame_info->frame_height / 4, 32);

    hevc_state->frame_width_16x = ALIGN(hevc_state->frame_width_4x / 4, 32);
    hevc_state->frame_height_16x = ALIGN(hevc_state->frame_height_4x / 4, 32);

    hevc_state->cu_records_offset = ALIGN(frame_info->width_in_lcu *
                                          frame_info->height_in_lcu *
                                          32, 4096);

    hevc_state->hme_supported = 1;
    hevc_state->b16xme_supported = 1;

    if (hevc_state->frame_width_4x <= GEN10_HEVC_VME_REF_WIN ||
        hevc_state->frame_height_4x <= GEN10_HEVC_VME_REF_WIN) {
        hevc_state->b16xme_supported = 0;

        hevc_state->frame_width_4x = GEN10_HEVC_VME_REF_WIN;
        hevc_state->frame_height_4x = GEN10_HEVC_VME_REF_WIN;
    } else if (hevc_state->frame_width_16x <= GEN10_HEVC_VME_REF_WIN ||
               hevc_state->frame_height_16x <= GEN10_HEVC_VME_REF_WIN) {
        hevc_state->frame_width_16x = GEN10_HEVC_VME_REF_WIN;
        hevc_state->frame_height_16x = GEN10_HEVC_VME_REF_WIN;
    }

    if (slice_param->slice_type == HEVC_SLICE_I) {
        hevc_state->hme_enabled = 0;
        hevc_state->b16xme_enabled = 0;
    } else {
        hevc_state->hme_enabled = hevc_state->hme_supported;
        hevc_state->b16xme_enabled = hevc_state->b16xme_supported;
    }

    if (frame_info->lcu_size == 64)
        hevc_state->is_64lcu = 1;
    else
        hevc_state->is_64lcu = 0;

    if (frame_info->bit_depth_luma_minus8 ||
        frame_info->bit_depth_chroma_minus8)
        hevc_state->is_10bit = 1;
    else
        hevc_state->is_10bit = 0;

    brc_method = GEN10_HEVC_BRC_CQP;
    if (encoder_context->rate_control_mode & VA_RC_CBR)
        brc_method = GEN10_HEVC_BRC_CBR;
    else if (encoder_context->rate_control_mode & VA_RC_VBR)
        brc_method = GEN10_HEVC_BRC_VBR;

    brc_reset = hevc_state->brc.brc_method != brc_method ||
                frame_info->reallocate_flag;

    if (!hevc_state->brc.brc_inited ||
        encoder_context->brc.need_reset ||
        brc_reset) {
        if (brc_method == GEN10_HEVC_BRC_CQP) {
            hevc_state->brc.brc_enabled = 0;
            hevc_state->num_pak_passes = 1;
        } else {
            hevc_state->brc.brc_enabled = 1;
            hevc_state->num_pak_passes = 1;//2;

            if (brc_method == GEN10_HEVC_BRC_CBR) {
                hevc_state->brc.target_bit_rate = encoder_context->brc.bits_per_second[0];
                hevc_state->brc.max_bit_rate = encoder_context->brc.bits_per_second[0];
                hevc_state->brc.min_bit_rate = encoder_context->brc.bits_per_second[0];
                hevc_state->brc.window_size = encoder_context->brc.window_size;
            } else {
                hevc_state->brc.max_bit_rate = encoder_context->brc.bits_per_second[0];
                hevc_state->brc.target_bit_rate = encoder_context->brc.bits_per_second[0] *
                                                  encoder_context->brc.target_percentage[0] /
                                                  100;

                if (2 * hevc_state->brc.target_bit_rate < hevc_state->brc.max_bit_rate)
                    hevc_state->brc.min_bit_rate = 0;
                else
                    hevc_state->brc.min_bit_rate = 2 * hevc_state->brc.target_bit_rate -
                                                   hevc_state->brc.max_bit_rate;
            }
        }

        if (encoder_context->brc.hrd_buffer_size)
            hevc_state->brc.vbv_buffer_size_in_bit = encoder_context->brc.hrd_buffer_size;
        else if (encoder_context->brc.window_size)
            hevc_state->brc.vbv_buffer_size_in_bit = hevc_state->brc.max_bit_rate *
                                                     encoder_context->brc.window_size /
                                                     1000;
        else
            hevc_state->brc.vbv_buffer_size_in_bit = hevc_state->brc.max_bit_rate;

        if (encoder_context->brc.hrd_initial_buffer_fullness)
            hevc_state->brc.init_vbv_buffer_fullness_in_bit = encoder_context->brc.hrd_initial_buffer_fullness;
        else
            hevc_state->brc.init_vbv_buffer_fullness_in_bit = hevc_state->brc.vbv_buffer_size_in_bit / 2;

        hevc_state->brc.gop_size = encoder_context->brc.gop_size;
        hevc_state->brc.gop_p = encoder_context->brc.num_pframes_in_gop;
        hevc_state->brc.gop_b = encoder_context->brc.num_bframes_in_gop;

        hevc_state->brc.frame_rate_m = encoder_context->brc.framerate[0].num;
        hevc_state->brc.frame_rate_d = encoder_context->brc.framerate[0].den;

        hevc_state->brc.brc_method = brc_method;
        hevc_state->brc.brc_reset = brc_reset || encoder_context->brc.need_reset;

        if (brc_method == GEN10_HEVC_BRC_CQP && !hevc_state->brc.brc_inited) {
            hevc_state->brc.frame_rate_m = 30;
            hevc_state->brc.frame_rate_d = 1;

            hevc_state->brc.target_bit_rate = (hevc_state->frame_width >> 4) * (hevc_state->frame_height >> 4)
                                              * 30 * 384 / 10 * 8;
            hevc_state->brc.max_bit_rate = hevc_state->brc.target_bit_rate;
            hevc_state->brc.min_bit_rate = hevc_state->brc.target_bit_rate;
            hevc_state->brc.window_size = 1500;
            hevc_state->brc.vbv_buffer_size_in_bit = (hevc_state->brc.target_bit_rate / 1000) * 1500;
            hevc_state->brc.init_vbv_buffer_fullness_in_bit = hevc_state->brc.vbv_buffer_size_in_bit / 2;

            hevc_state->brc.gop_size = seq_param->intra_period < 2 ? 30 : seq_param->intra_period;
            hevc_state->brc.gop_p = (hevc_state->brc.gop_size - 1) /
                                    (!seq_param->ip_period ? 1 : seq_param->ip_period);
            hevc_state->brc.gop_b =  hevc_state->brc.gop_size - 1 - hevc_state->brc.gop_p;
        }

        hevc_state->profile_level_max_frame =
            gen10_hevc_enc_get_profile_level_max_frame(seq_param, 0,
                                                       hevc_state->brc.frame_rate_m /
                                                       hevc_state->brc.frame_rate_d);
    }

    hevc_state->sao_2nd_needed = 0;
    hevc_state->sao_first_pass_flag = 0;
    hevc_state->num_sao_passes = hevc_state->num_pak_passes;
    if (seq_param->seq_fields.bits.sample_adaptive_offset_enabled_flag &&
        (slice_param->slice_fields.bits.slice_sao_luma_flag ||
         slice_param->slice_fields.bits.slice_sao_chroma_flag)) {
        hevc_state->sao_2nd_needed = 1;
        hevc_state->sao_first_pass_flag = 1;
        hevc_state->num_sao_passes = hevc_state->num_pak_passes + 1;
    }

    hevc_state->brc.target_usage = encoder_context->quality_level;
    hevc_state->thread_num_per_ctb = gen10_hevc_tu_settings[GEN10_TOTAL_THREAD_NUM_PER_LCU_TU_PARAM]
                                     [(hevc_state->brc.target_usage + 1) >> 2];

    hevc_state->is_same_ref_list = frame_info->is_same_ref_list;

    return VA_STATUS_SUCCESS;
}

static VAStatus
gen10_hevc_enc_init_parameters(VADriverContextP ctx,
                               struct encode_state *encode_state,
                               struct intel_encoder_context *encoder_context)
{
    struct gen10_hevc_enc_context *vme_context;
    struct gen10_hevc_enc_state *hevc_state;
    struct gen10_hevc_enc_frame_info *frame_info;
    struct gen10_hevc_enc_common_res *common_res;
    VAStatus va_status = VA_STATUS_SUCCESS;

    va_status = gen10_hevc_enc_check_parameters(ctx, encode_state, encoder_context);
    if (va_status != VA_STATUS_SUCCESS)
        goto EXIT;

    vme_context = (struct gen10_hevc_enc_context *) encoder_context->vme_context;
    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;
    frame_info = &vme_context->frame_info;
    common_res = &vme_context->common_res;

    gen10_hevc_enc_init_frame_info(ctx, encode_state, encoder_context, frame_info);
    gen10_hevc_enc_init_status_buffer(ctx, encode_state, encoder_context,
                                      &vme_context->status_buffer);

    if (!hevc_state->lambda_init ||
        frame_info->reallocate_flag) {
        gen10_hevc_enc_init_lambda_param(&vme_context->lambda_param, frame_info->bit_depth_luma_minus8,
                                         frame_info->bit_depth_chroma_minus8);

        hevc_state->lambda_init = 1;
    }

    if (gen10_hevc_enc_init_common_resource(ctx, encode_state, encoder_context,
                                            common_res,
                                            frame_info,
                                            frame_info->picture_coding_type != HEVC_SLICE_I,
                                            0) < 0) {
        va_status = VA_STATUS_ERROR_ALLOCATION_FAILED;
        goto EXIT;
    }

    va_status = gen10_hevc_enc_init_misc_paramers(ctx, encode_state, encoder_context);
    if (va_status != VA_STATUS_SUCCESS)
        goto EXIT;

    va_status = gen10_hevc_enc_ensure_surface(ctx,
                                              common_res->uncompressed_pic.obj_surface,
                                              frame_info->bit_depth_luma_minus8,
                                              0);
    if (va_status != VA_STATUS_SUCCESS)
        goto EXIT;

    va_status = gen10_hevc_enc_ensure_surface(ctx,
                                              common_res->reconstructed_pic.obj_surface,
                                              frame_info->bit_depth_luma_minus8,
                                              1);
    if (va_status != VA_STATUS_SUCCESS)
        goto EXIT;

    va_status = gen10_hevc_init_surface_priv(ctx, encode_state, encoder_context,
                                             common_res->reconstructed_pic.obj_surface);
    if (va_status != VA_STATUS_SUCCESS)
        goto EXIT;

    if (frame_info->reallocate_flag) {
        va_status = gen10_hevc_allocate_enc_resources(ctx, encode_state,
                                                      encoder_context);
        if (va_status != VA_STATUS_SUCCESS)
            goto EXIT;

        hevc_state->frame_number = 0;
    }

    va_status = gen10_hevc_enc_init_const_resources(ctx, encode_state, encoder_context);
    if (va_status != VA_STATUS_SUCCESS)
        goto EXIT;

EXIT:
    return va_status;
}

#define GEN10_WALKER_26_DEGREE       0
#define GEN10_WALKER_26Z_DEGREE      1
#define GEN10_WALKER_26X_DEGREE      2
#define GEN10_WALKER_26ZX_DEGREE     3

static void
gen10_init_media_object_walker_parameter(struct gen10_hevc_enc_kernel_walker_parameter *kernel_walker_param,
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

    walker_param->local_loop_exec_count = 0xFFFF;
    walker_param->global_loop_exec_count = 0xFFFF;

    if (kernel_walker_param->no_dependency) {
        walker_param->scoreboard_mask = 0;
        walker_param->use_scoreboard = 0;
        walker_param->local_outer_loop_stride.x = 0;
        walker_param->local_outer_loop_stride.y = 1;
        walker_param->local_inner_loop_unit.x = 1;
        walker_param->local_inner_loop_unit.y = 0;
        walker_param->local_end.x = kernel_walker_param->resolution_x - 1;
        walker_param->local_end.y = 0;
    } else if (kernel_walker_param->use_vertical_scan) {
        walker_param->scoreboard_mask            = 0x1;
        walker_param->local_outer_loop_stride.x   = 1;
        walker_param->local_outer_loop_stride.y   = 0;
        walker_param->local_inner_loop_unit.x   = 0;
        walker_param->local_inner_loop_unit.y   = 1;
        walker_param->local_end.x             = 0;
        walker_param->local_end.y             = kernel_walker_param->resolution_y - 1;
    } else {
        walker_param->local_end.x = 0;
        walker_param->local_end.y = 0;
    }
}

static void
gen10_run_kernel_media_object(VADriverContextP ctx,
                              struct intel_encoder_context *encoder_context,
                              struct i965_gpe_context *gpe_context,
                              int media_function,
                              struct gpe_media_object_parameter *param)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct gen10_hevc_enc_context *vme_context = encoder_context->vme_context;
    struct gen10_hevc_enc_status_buffer *status_buffer;
    struct gpe_mi_store_data_imm_parameter mi_store_data_imm;

    status_buffer = &vme_context->status_buffer;

    intel_batchbuffer_start_atomic(batch, 0x1000);

    memset(&mi_store_data_imm, 0, sizeof(mi_store_data_imm));
    mi_store_data_imm.bo = status_buffer->gpe_res.bo;
    mi_store_data_imm.offset = status_buffer->status_media_state_offset;
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
gen10_run_kernel_media_object_walker(VADriverContextP ctx,
                                     struct intel_encoder_context *encoder_context,
                                     struct i965_gpe_context *gpe_context,
                                     int media_function,
                                     struct gpe_media_object_walker_parameter *param)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct gen10_hevc_enc_context *vme_context = encoder_context->vme_context;
    struct gen10_hevc_enc_status_buffer *status_buffer;
    struct gpe_mi_store_data_imm_parameter mi_store_data_imm;

    status_buffer = &vme_context->status_buffer;

    intel_batchbuffer_start_atomic(batch, 0x1000);

    intel_batchbuffer_emit_mi_flush(batch);

    memset(&mi_store_data_imm, 0, sizeof(mi_store_data_imm));
    mi_store_data_imm.bo = status_buffer->gpe_res.bo;
    mi_store_data_imm.offset = status_buffer->status_media_state_offset;
    mi_store_data_imm.dw0 = media_function;
    gen8_gpe_mi_store_data_imm(ctx, batch, &mi_store_data_imm);

    gen9_gpe_pipeline_setup(ctx, gpe_context, batch);
    gen8_gpe_media_object_walker(ctx, gpe_context, batch, param);
    gen8_gpe_media_state_flush(ctx, gpe_context, batch);

    gen9_gpe_pipeline_end(ctx, gpe_context, batch);

    intel_batchbuffer_end_atomic(batch);

    intel_batchbuffer_flush(batch);
}

#define BRC_CLIP(x, min, max)                                   \
    {                                                           \
        x = ((x > (max)) ? (max) : ((x < (min)) ? (min) : x));  \
    }

#define GEN10_HEVC_MAX_BRC_PASSES           4

#define GEN10_HEVC_BRCINIT_ISCBR            0x0010
#define GEN10_HEVC_BRCINIT_ISVBR            0x0020
#define GEN10_HEVC_BRCINIT_ISCQP            0x4000
#define GEN10_HEVC_BRCINIT_DISABLE_MBBRC    0x8000

static void
gen10_hevc_enc_brc_init_set_curbe(VADriverContextP ctx,
                                  struct encode_state *encode_state,
                                  struct intel_encoder_context *encoder_context,
                                  struct i965_gpe_context *gpe_context)
{
    struct gen10_hevc_enc_context *vme_context = encoder_context->vme_context;
    struct gen10_hevc_enc_state *hevc_state;
    gen10_hevc_brc_init_curbe_data *brc_curbe;
    double input_bits_per_frame, bps_ratio;

    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;

    brc_curbe = i965_gpe_context_map_curbe(gpe_context);

    if (!brc_curbe)
        return;

    memset(brc_curbe, 0, sizeof(gen10_hevc_brc_init_curbe_data));

    brc_curbe->dw0.profile_level_max_frame = hevc_state->profile_level_max_frame;
    brc_curbe->dw1.init_buf_full           = hevc_state->brc.init_vbv_buffer_fullness_in_bit;
    brc_curbe->dw2.buf_size                = hevc_state->brc.vbv_buffer_size_in_bit;
    brc_curbe->dw3.target_bit_rate         = hevc_state->brc.target_bit_rate;
    brc_curbe->dw4.maximum_bit_rate        = hevc_state->brc.max_bit_rate;
    brc_curbe->dw5.minimum_bit_rate        = 0;
    brc_curbe->dw6.frame_ratem             = hevc_state->brc.frame_rate_m;
    brc_curbe->dw7.frame_rated             = hevc_state->brc.frame_rate_d;
    if (hevc_state->brc.lcu_brc_enabled)
        brc_curbe->dw8.brc_flag            = 0;
    else
        brc_curbe->dw8.brc_flag            = GEN10_HEVC_BRCINIT_DISABLE_MBBRC;

    brc_curbe->dw25.ac_qp_buffer = 1;
    brc_curbe->dw25.log2_max_cu_size = hevc_state->is_64lcu ? 6 : 5;
    brc_curbe->dw25.sliding_wind_size = 30;

    if (hevc_state->brc.brc_method == GEN10_HEVC_BRC_CQP) {
        brc_curbe->dw8.brc_flag               = GEN10_HEVC_BRCINIT_ISCQP;
    } else if (hevc_state->brc.brc_method == GEN10_HEVC_BRC_CBR) {
        brc_curbe->dw8.brc_flag               |= GEN10_HEVC_BRCINIT_ISCBR;
    } else if (hevc_state->brc.brc_method == GEN10_HEVC_BRC_VBR) {
        brc_curbe->dw8.brc_flag               |= GEN10_HEVC_BRCINIT_ISVBR;
    }

    brc_curbe->dw9.frame_width   = hevc_state->frame_width;
    brc_curbe->dw10.frame_height = hevc_state->frame_height;
    brc_curbe->dw10.avbr_accuracy = 30;
    brc_curbe->dw11.avbr_convergence = 150;

    brc_curbe->dw14.max_brc_level = 1;
    brc_curbe->dw8.brc_gopp                  = hevc_state->brc.gop_p;
    brc_curbe->dw9.brc_gopb                  = hevc_state->brc.gop_b;

    brc_curbe->dw11.minimum_qp = 1;
    brc_curbe->dw12.maximum_qp = 51;

    brc_curbe->dw16.instant_rate_thr0_pframe      = 40;
    brc_curbe->dw16.instant_rate_thr1_pframe      = 60;
    brc_curbe->dw16.instant_rate_thr2_pframe      = 80;
    brc_curbe->dw16.instant_rate_thr3_pframe      = 120;
    brc_curbe->dw17.instant_rate_thr0_bframe      = 35;
    brc_curbe->dw17.instant_rate_thr1_bframe      = 60;
    brc_curbe->dw17.instant_rate_thr2_bframe      = 80;
    brc_curbe->dw17.instant_rate_thr3_bframe      = 120;
    brc_curbe->dw18.instant_rate_thr0_iframe      = 40;
    brc_curbe->dw18.instant_rate_thr1_iframe      = 60;
    brc_curbe->dw18.instant_rate_thr2_iframe      = 90;
    brc_curbe->dw18.instant_rate_thr3_iframe      = 115;

    input_bits_per_frame = (double)(brc_curbe->dw4.maximum_bit_rate) * ((double)(hevc_state->brc.frame_rate_d)) /
                           ((double)(hevc_state->brc.frame_rate_m));

    if (brc_curbe->dw2.buf_size < (uint32_t)input_bits_per_frame * 4)
        brc_curbe->dw2.buf_size = (uint32_t)input_bits_per_frame * 4;

    if (!brc_curbe->dw1.init_buf_full)
        brc_curbe->dw1.init_buf_full = 7 * brc_curbe->dw2.buf_size / 8;
    else if (brc_curbe->dw1.init_buf_full < (uint32_t)input_bits_per_frame * 2)
        brc_curbe->dw1.init_buf_full = (uint32_t)input_bits_per_frame * 2;
    else if (brc_curbe->dw1.init_buf_full > brc_curbe->dw2.buf_size)
        brc_curbe->dw1.init_buf_full = brc_curbe->dw2.buf_size;

    bps_ratio = input_bits_per_frame / ((double)(hevc_state->brc.vbv_buffer_size_in_bit) / 30);

    BRC_CLIP(bps_ratio, 0.1, 3.5);

    brc_curbe->dw19.deviation_thr0_pbframe      = (uint32_t)(-50 * pow(0.90, bps_ratio));
    brc_curbe->dw19.deviation_thr1_pbframe      = (uint32_t)(-50 * pow(0.66, bps_ratio));
    brc_curbe->dw19.deviation_thr2_pbframe      = (uint32_t)(-50 * pow(0.46, bps_ratio));
    brc_curbe->dw19.deviation_thr3_pbframe      = (uint32_t)(-50 * pow(0.3, bps_ratio));

    brc_curbe->dw20.deviation_thr4_pbframe      = (uint32_t)(50 * pow(0.3, bps_ratio));
    brc_curbe->dw20.deviation_thr5_pbframe      = (uint32_t)(50 * pow(0.46, bps_ratio));
    brc_curbe->dw20.deviation_thr6_pbframe      = (uint32_t)(50 * pow(0.7, bps_ratio));
    brc_curbe->dw20.deviation_thr7_pbframe      = (uint32_t)(50 * pow(0.9, bps_ratio));

    brc_curbe->dw21.deviation_thr0_vbrctrl   = (uint32_t)(-50 * pow(0.9, bps_ratio));
    brc_curbe->dw21.deviation_thr1_vbrctrl   = (uint32_t)(-50 * pow(0.7, bps_ratio));
    brc_curbe->dw21.deviation_thr2_vbrctrl   = (uint32_t)(-50 * pow(0.5, bps_ratio));
    brc_curbe->dw21.deviation_thr3_vbrctrl   = (uint32_t)(-50 * pow(0.3, bps_ratio));

    brc_curbe->dw22.deviation_thr4_vbrctrl   = (uint32_t)(100 * pow(0.4, bps_ratio));
    brc_curbe->dw22.deviation_thr5_vbrctrl   = (uint32_t)(100 * pow(0.5, bps_ratio));
    brc_curbe->dw22.deviation_thr6_vbrctrl   = (uint32_t)(100 * pow(0.75, bps_ratio));
    brc_curbe->dw22.deviation_thr7_vbrctrl   = (uint32_t)(100 * pow(0.9, bps_ratio));

    brc_curbe->dw23.deviation_thr0_iframe       = (uint32_t)(-50 * pow(0.8, bps_ratio));
    brc_curbe->dw23.deviation_thr1_iframe       = (uint32_t)(-50 * pow(0.6, bps_ratio));
    brc_curbe->dw23.deviation_thr2_iframe       = (uint32_t)(-50 * pow(0.34, bps_ratio));
    brc_curbe->dw23.deviation_thr3_iframe       = (uint32_t)(-50 * pow(0.2, bps_ratio));

    brc_curbe->dw24.deviation_thr4_iframe       = (uint32_t)(50 * pow(0.2, bps_ratio));
    brc_curbe->dw24.deviation_thr5_iframe       = (uint32_t)(50 * pow(0.4, bps_ratio));
    brc_curbe->dw24.deviation_thr6_iframe       = (uint32_t)(50 * pow(0.66, bps_ratio));
    brc_curbe->dw24.deviation_thr7_iframe       = (uint32_t)(50 * pow(0.9, bps_ratio));

    if (!hevc_state->brc.brc_inited)
        hevc_state->brc.brc_init_current_target_buf_full_in_bits = brc_curbe->dw1.init_buf_full;

    hevc_state->brc.brc_init_reset_buf_size_in_bits    = (double)brc_curbe->dw2.buf_size;
    hevc_state->brc.brc_init_reset_input_bits_per_frame  = input_bits_per_frame;

    i965_gpe_context_unmap_curbe(gpe_context);
}

static void
gen10_hevc_enc_brc_init_add_surfaces(VADriverContextP ctx,
                                     struct encode_state *encode_state,
                                     struct intel_encoder_context *encoder_context,
                                     struct i965_gpe_context *gpe_context)
{
    struct gen10_hevc_enc_context *vme_context = encoder_context->vme_context;

    i965_add_buffer_gpe_surface(ctx,
                                gpe_context,
                                &vme_context->res_brc_history_buffer,
                                0,
                                BYTES2UINT32(vme_context->res_brc_history_buffer.size),
                                0,
                                0);

    i965_add_buffer_2d_gpe_surface(ctx,
                                   gpe_context,
                                   &vme_context->res_brc_me_dist_surface,
                                   1,
                                   I965_SURFACEFORMAT_R8_UNORM,
                                   1);
}

static void
gen10_hevc_enc_brc_init_reset(VADriverContextP ctx,
                              struct encode_state *encode_state,
                              struct intel_encoder_context *encoder_context)
{
    struct gen10_hevc_enc_context *vme_context = encoder_context->vme_context;
    struct gen10_hevc_enc_state *hevc_state;
    struct gpe_media_object_parameter media_object_param;
    struct i965_gpe_context *gpe_context;
    int gpe_index = GEN10_HEVC_BRC_INIT;
    int media_function = GEN10_HEVC_MEDIA_STATE_BRC_INIT_RESET;

    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;

    if (hevc_state->brc.brc_inited)
        gpe_index = GEN10_HEVC_BRC_RESET;

    gpe_context = &(vme_context->brc_context.gpe_contexts[gpe_index]);

    gen8_gpe_context_init(ctx, gpe_context);
    gen9_gpe_reset_binding_table(ctx, gpe_context);

    gen10_hevc_enc_brc_init_set_curbe(ctx, encode_state, encoder_context, gpe_context);
    gen10_hevc_enc_brc_init_add_surfaces(ctx, encode_state, encoder_context, gpe_context);

    gen8_gpe_setup_interface_data(ctx, gpe_context);

    memset(&media_object_param, 0, sizeof(media_object_param));
    gen10_run_kernel_media_object(ctx, encoder_context, gpe_context, media_function, &media_object_param);
}

static void
gen10_hevc_brc_add_pic_img_state(VADriverContextP ctx,
                                 struct encode_state *encode_state,
                                 struct intel_encoder_context *encoder_context)
{
    struct gen10_hevc_enc_context *pak_context = encoder_context->mfc_context;
    struct gen10_hevc_enc_state *hevc_state;
    VAEncPictureParameterBufferHEVC  *pic_param;
    VAEncSequenceParameterBufferHEVC *seq_param;
    VAEncSliceParameterBufferHEVC *slice_param;
    unsigned int batch_value = 0, tmp_value, i;
    uint32_t *batch_ptr, *buffer_ptr;

    hevc_state = (struct gen10_hevc_enc_state *) pak_context->enc_priv_state;

    buffer_ptr = (uint32_t *)i965_map_gpe_resource(&pak_context->res_brc_pic_image_state_read_buffer);

    if (!buffer_ptr)
        return;

    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;
    seq_param = (VAEncSequenceParameterBufferHEVC *)encode_state->seq_param_ext->buffer;
    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[0]->buffer;

    for (i = 0; i < 4; i++) {
        batch_ptr = buffer_ptr + 32 * i;


        /* DW 0 */
        *(batch_ptr++) = HCP_PIC_STATE | (31 - 2);

        /* DW 1 */
        batch_value = (pak_context->frame_info.width_in_cu - 1) |
                      ((pak_context->frame_info.height_in_cu - 1) << 16);
        batch_value |= pic_param->pic_fields.bits.transform_skip_enabled_flag << 15;
        *(batch_ptr++) = batch_value;

        batch_value = (seq_param->log2_min_pcm_luma_coding_block_size_minus3 << 8) |
                      (seq_param->log2_max_pcm_luma_coding_block_size_minus3 << 10) |
                      (seq_param->log2_min_transform_block_size_minus2  << 4) |
                      ((seq_param->log2_min_transform_block_size_minus2 +
                        seq_param->log2_diff_max_min_transform_block_size) << 6) |
                      ((seq_param->log2_min_luma_coding_block_size_minus3 +
                        seq_param->log2_diff_max_min_luma_coding_block_size) << 2) |
                      (seq_param->log2_min_luma_coding_block_size_minus3 << 0);

        /* DW 2 */
        *(batch_ptr++) = batch_value;

        /* DW 3 */
        *(batch_ptr++) = 0;

        /* DW 4 */
        batch_value = 0;
        if ((slice_param->slice_fields.bits.slice_sao_luma_flag ||
             slice_param->slice_fields.bits.slice_sao_chroma_flag) &&
            !hevc_state->is_10bit)
            batch_value |= (1 << 3);

        if (pic_param->pic_fields.bits.cu_qp_delta_enabled_flag) {
            tmp_value = pic_param->diff_cu_qp_delta_depth;
            batch_value |= (1 << 5) | (tmp_value << 6);
        }
        batch_value |= (0 << 4) |
                       (seq_param->seq_fields.bits.pcm_loop_filter_disabled_flag << 8) |
                       (0 << 9) |
                       (0 << 10) | //(pic_param->log2_parallel_merge_level_minus2
                       (0 << 13) |
                       (0 << 15) |
                       (0 << 17) | //tile is disabled.
                       (pic_param->pic_fields.bits.weighted_bipred_flag << 18) |
                       (pic_param->pic_fields.bits.weighted_pred_flag << 19) |
                       (0 << 20) | //20/21 is reserved.
                       (pic_param->pic_fields.bits.transform_skip_enabled_flag << 22) |
                       (seq_param->seq_fields.bits.amp_enabled_flag << 23) |
                       (pic_param->pic_fields.bits.transquant_bypass_enabled_flag << 25) |
                       (seq_param->seq_fields.bits.strong_intra_smoothing_enabled_flag << 26) |
                       (0 << 27); // VME CU packet

        *(batch_ptr++) = batch_value;

        /* DW 5 */
        batch_value = (pic_param->pps_cr_qp_offset & 0x1f) << 5 |
                      (pic_param->pps_cb_qp_offset & 0x1f);
        batch_value |= (seq_param->max_transform_hierarchy_depth_inter << 13) |
                       (seq_param->max_transform_hierarchy_depth_intra << 10) |
                       (seq_param->pcm_sample_bit_depth_luma_minus1 << 20) |
                       (seq_param->pcm_sample_bit_depth_chroma_minus1 << 16) |
                       (seq_param->seq_fields.bits.bit_depth_luma_minus8 << 27) |
                       (seq_param->seq_fields.bits.bit_depth_chroma_minus8 << 24);
        *(batch_ptr++) = batch_value;

        /* DW6 */
        batch_value = pic_param->ctu_max_bitsize_allowed;
        batch_value |= (0 << 24 |
                        1 << 25 |
                        1 << 26 |
                        0 << 29); // bit 29 reload slice_pointer_flag.

        if (i == 0)
            batch_value |= (0 << 16); // Initial pass
        else
            batch_value |= (1 << 16); // subsequent pass
        *(batch_ptr++) = batch_value;

        /* DW 7. Frame_rate Max */
        *(batch_ptr++) = 0;

        /* Dw 8. Frame_rate Min */
        *(batch_ptr++) = 0;

        /* DW 9. Frame_rate Min/MAX slice_delta */
        *(batch_ptr++) = 0;

        /* DW 10..17 */
        *(batch_ptr++) = 0;
        *(batch_ptr++) = 0;
        *(batch_ptr++) = 0;
        *(batch_ptr++) = 0;
        *(batch_ptr++) = 0;
        *(batch_ptr++) = 0;
        *(batch_ptr++) = 0;
        *(batch_ptr++) = 0;

        /* DW 18 */
        *(batch_ptr++) = 0;

        /* DW 19..20 */
        *(batch_ptr++) = 0;
        *(batch_ptr++) = 0;

        /* DW 21..30 */
        *(batch_ptr++) = 0;
        *(batch_ptr++) = 0;
        *(batch_ptr++) = 0;
        *(batch_ptr++) = 0;
        *(batch_ptr++) = 0;
        *(batch_ptr++) = 0;
        *(batch_ptr++) = 0;
        *(batch_ptr++) = 0;
        *(batch_ptr++) = 0;
        *(batch_ptr++) = 0;

        /* DW 31 */
        *(batch_ptr++) = MI_BATCH_BUFFER_END;
    }

    i965_unmap_gpe_resource(&pak_context->res_brc_pic_image_state_read_buffer);
}

static VAStatus
gen10_hevc_enc_brc_frame_update_add_surfaces(VADriverContextP ctx,
                                             struct encode_state *encode_state,
                                             struct intel_encoder_context *encoder_context,
                                             struct i965_gpe_context *gpe_context)
{
    struct gen10_hevc_enc_context *vme_context = encoder_context->vme_context;
    struct gen10_hevc_enc_state *hevc_state;
    int pak_read_idx;

    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;

    i965_add_buffer_gpe_surface(ctx,
                                gpe_context,
                                &vme_context->res_brc_history_buffer,
                                0,
                                BYTES2UINT32(vme_context->res_brc_history_buffer.size),
                                0,
                                0);

    pak_read_idx = !hevc_state->curr_pak_stat_index;
    i965_add_buffer_gpe_surface(ctx,
                                gpe_context,
                                &vme_context->res_brc_pak_statistics_buffer[pak_read_idx],
                                0,
                                BYTES2UINT32(vme_context->res_brc_pak_statistics_buffer[pak_read_idx].size),
                                0,
                                1);

    i965_add_buffer_gpe_surface(ctx,
                                gpe_context,
                                &vme_context->res_brc_pic_image_state_read_buffer,
                                0,
                                BYTES2UINT32(vme_context->res_brc_pic_image_state_read_buffer.size),
                                0,
                                2);

    i965_add_buffer_gpe_surface(ctx,
                                gpe_context,
                                &vme_context->res_brc_pic_image_state_write_buffer,
                                0,
                                BYTES2UINT32(vme_context->res_brc_pic_image_state_write_buffer.size),
                                0,
                                3);

    i965_add_buffer_gpe_surface(ctx,
                                gpe_context,
                                &vme_context->res_brc_input_enc_kernel_buffer,
                                0,
                                BYTES2UINT32(vme_context->res_brc_input_enc_kernel_buffer.size),
                                0,
                                4);

    i965_add_buffer_2d_gpe_surface(ctx,
                                   gpe_context,
                                   &vme_context->res_brc_me_dist_surface,
                                   1,
                                   I965_SURFACEFORMAT_R8_UNORM,
                                   5);

    i965_add_buffer_2d_gpe_surface(ctx,
                                   gpe_context,
                                   &vme_context->res_brc_const_data_surface,
                                   1,
                                   I965_SURFACEFORMAT_R8_UNORM,
                                   6);

    i965_add_buffer_2d_gpe_surface(ctx,
                                   gpe_context,
                                   &vme_context->res_mb_stat_surface,
                                   1,
                                   I965_SURFACEFORMAT_R8_UNORM,
                                   7);

    i965_add_buffer_gpe_surface(ctx,
                                gpe_context,
                                &vme_context->res_mv_dist_sum_buffer,
                                0,
                                BYTES2UINT32(vme_context->res_mv_dist_sum_buffer.size),
                                0,
                                8);

    return VA_STATUS_SUCCESS;
}

static VAStatus
gen10_hevc_enc_brc_update_set_curbe(VADriverContextP ctx,
                                    struct encode_state *encode_state,
                                    struct intel_encoder_context *encoder_context,
                                    struct i965_gpe_context *gpe_context,
                                    int lcu_update)
{
    struct gen10_hevc_enc_context *vme_context = encoder_context->vme_context;
    struct gen10_hevc_enc_state *hevc_state;
    gen10_hevc_brc_update_curbe_data      *brc_update;
    VAEncSliceParameterBufferHEVC *slice_param;
    VAEncPictureParameterBufferHEVC  *pic_param;

    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;

    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[0]->buffer;
    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;

    brc_update = i965_gpe_context_map_curbe(gpe_context);

    if (!brc_update)
        return VA_STATUS_ERROR_OPERATION_FAILED;

    memset(brc_update, 0, sizeof(gen10_hevc_brc_update_curbe_data));

    if (hevc_state->brc.brc_init_current_target_buf_full_in_bits >
        (double)hevc_state->brc.brc_init_reset_buf_size_in_bits) {
        hevc_state->brc.brc_init_current_target_buf_full_in_bits -=
            (double)hevc_state->brc.brc_init_reset_buf_size_in_bits;
        brc_update->dw5.target_size_flag  = 1;
    }

    brc_update->dw0.target_size    = (uint32_t)(hevc_state->brc.brc_init_current_target_buf_full_in_bits);
    brc_update->dw1.frame_num      = hevc_state->frame_number;

    brc_update->dw2.picture_header_size = gen10_hevc_enc_get_pic_header_size(encode_state);

    if (slice_param->slice_type == HEVC_SLICE_I)
        brc_update->dw5.curr_frame_brclevel = 2;
    else if (slice_param->slice_type == HEVC_SLICE_P ||
             hevc_state->low_delay)
        brc_update->dw5.curr_frame_brclevel = 0;
    else
        brc_update->dw5.curr_frame_brclevel = 1;

    brc_update->dw5.max_num_paks = GEN10_HEVC_MAX_BRC_PASSES;

    if (hevc_state->brc.brc_method == GEN10_HEVC_BRC_CQP) {
        int qp_value;

        qp_value = pic_param->pic_init_qp + slice_param->slice_qp_delta;
        BRC_CLIP(qp_value, 1, 51);
        brc_update->dw6.cqp_value = qp_value;
    }

    brc_update->dw14.parallel_mode = 0;

    if (lcu_update == 1)
        hevc_state->brc.brc_init_current_target_buf_full_in_bits +=
            hevc_state->brc.brc_init_reset_input_bits_per_frame;

    brc_update->dw3.start_gadj_frame0 = 10;
    brc_update->dw3.start_gadj_frame1 = 50;
    brc_update->dw4.start_gadj_frame2 = 100;
    brc_update->dw4.start_gadj_frame3 = 150;

    brc_update->dw8.start_gadj_mult0 = 1;
    brc_update->dw8.start_gadj_mult1 = 1;
    brc_update->dw8.start_gadj_mult2 = 3;
    brc_update->dw8.start_gadj_mult3 = 2;
    brc_update->dw9.start_gadj_mult4 = 1;

    brc_update->dw9.start_gadj_divd0 = 40;
    brc_update->dw9.start_gadj_divd1 = 5;
    brc_update->dw9.start_gadj_divd2 = 5;
    brc_update->dw10.start_gadj_divd3 = 3;
    brc_update->dw10.start_gadj_divd4 = 1;

    brc_update->dw10.qp_threshold0 = 7;
    brc_update->dw10.qp_threshold1 = 18;
    brc_update->dw11.qp_threshold2 = 25;
    brc_update->dw11.qp_threshold3 = 37;

    brc_update->dw11.grate_ratio_thr0 = 40;
    brc_update->dw11.grate_ratio_thr1 = 75;
    brc_update->dw12.grate_ratio_thr2 = 97;
    brc_update->dw12.grate_ratio_thr3 = 103;
    brc_update->dw12.grate_ratio_thr4 = 125;
    brc_update->dw12.grate_ratio_thr5 = 160;

    brc_update->dw13.grate_ratio_thr6 = -3;
    brc_update->dw13.grate_ratio_thr7 = -2;
    brc_update->dw13.grate_ratio_thr8 = -1;
    brc_update->dw13.grate_ratio_thr9 = 0;

    brc_update->dw14.grate_ratio_thr10 = 1;
    brc_update->dw14.grate_ratio_thr11 = 2;
    brc_update->dw14.grate_ratio_thr12 = 3;

    i965_gpe_context_unmap_curbe(gpe_context);
    return VA_STATUS_SUCCESS;
}

static void
gen10_hevc_enc_brc_frame_update_kernel(VADriverContextP ctx,
                                       struct encode_state *encode_state,
                                       struct intel_encoder_context *encoder_context)
{
    struct gen10_hevc_enc_context *vme_context = encoder_context->vme_context;
    struct i965_gpe_context *gpe_context;
    int gpe_index = GEN10_HEVC_BRC_FRAME_UPDATE;
    int media_function = GEN10_HEVC_MEDIA_STATE_BRC_UPDATE;
    struct gpe_media_object_parameter media_object_param;

    gpe_context = &(vme_context->brc_context.gpe_contexts[gpe_index]);

    gen8_gpe_context_init(ctx, gpe_context);
    gen9_gpe_reset_binding_table(ctx, gpe_context);

    gen10_hevc_brc_add_pic_img_state(ctx, encode_state, encoder_context);
    gen10_hevc_enc_brc_update_set_curbe(ctx, encode_state, encoder_context, gpe_context, 0);
    gen10_hevc_enc_brc_frame_update_add_surfaces(ctx, encode_state, encoder_context, gpe_context);
    gen8_gpe_setup_interface_data(ctx, gpe_context);

    memset(&media_object_param, 0, sizeof(media_object_param));
    gen10_run_kernel_media_object(ctx, encoder_context, gpe_context, media_function, &media_object_param);
}

static void
gen10_hevc_enc_brc_lcu_update_add_surfaces(VADriverContextP ctx,
                                           struct encode_state *encode_state,
                                           struct intel_encoder_context *encoder_context,
                                           struct i965_gpe_context *gpe_context)
{
    struct gen10_hevc_enc_context *vme_context = encoder_context->vme_context;

    i965_add_buffer_gpe_surface(ctx,
                                gpe_context,
                                &vme_context->res_brc_history_buffer,
                                0,
                                BYTES2UINT32(vme_context->res_brc_history_buffer.size),
                                0,
                                0);

    i965_add_buffer_2d_gpe_surface(ctx,
                                   gpe_context,
                                   &vme_context->res_brc_me_dist_surface,
                                   1,
                                   I965_SURFACEFORMAT_R8_UNORM,
                                   1);

    i965_add_buffer_2d_gpe_surface(ctx,
                                   gpe_context,
                                   &vme_context->res_mb_stat_surface,
                                   1,
                                   I965_SURFACEFORMAT_R8_UNORM,
                                   2);

    i965_add_buffer_2d_gpe_surface(ctx,
                                   gpe_context,
                                   &vme_context->res_brc_mb_qp_surface,
                                   1,
                                   I965_SURFACEFORMAT_R8_UNORM,
                                   3);

    i965_add_buffer_2d_gpe_surface(ctx,
                                   gpe_context,
                                   &vme_context->res_mb_split_surface,
                                   1,
                                   I965_SURFACEFORMAT_R8_UNORM,
                                   4);

    i965_add_buffer_2d_gpe_surface(ctx,
                                   gpe_context,
                                   &vme_context->res_brc_intra_dist_surface,
                                   1,
                                   I965_SURFACEFORMAT_R8_UNORM,
                                   5);

    i965_add_buffer_2d_gpe_surface(ctx,
                                   gpe_context,
                                   &vme_context->res_cu_split_surface,
                                   1,
                                   I965_SURFACEFORMAT_R8_UNORM,
                                   6);
}

static void
gen10_hevc_enc_brc_lcu_update_kernel(VADriverContextP ctx,
                                     struct encode_state *encode_state,
                                     struct intel_encoder_context *encoder_context)
{
    struct gen10_hevc_enc_context *vme_context = encoder_context->vme_context;
    struct gen10_hevc_enc_state *hevc_state;
    struct i965_gpe_context *gpe_context;
    int gpe_index = GEN10_HEVC_BRC_LCU_UPDATE;
    int media_function = GEN10_HEVC_MEDIA_STATE_BRC_LCU_UPDATE;
    uint32_t resolution_x, resolution_y;
    struct gpe_media_object_walker_parameter media_object_walker_param;
    struct gen10_hevc_enc_kernel_walker_parameter kernel_walker_param;

    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;

    gpe_context = &(vme_context->brc_context.gpe_contexts[gpe_index]);

    gen8_gpe_context_init(ctx, gpe_context);
    gen9_gpe_reset_binding_table(ctx, gpe_context);

    gen10_hevc_enc_brc_update_set_curbe(ctx, encode_state, encoder_context, gpe_context, 1);
    gen10_hevc_enc_brc_lcu_update_add_surfaces(ctx, encode_state, encoder_context, gpe_context);

    gen8_gpe_setup_interface_data(ctx, gpe_context);

    memset(&kernel_walker_param, 0, sizeof(kernel_walker_param));

    resolution_x = ALIGN(hevc_state->frame_width, 16) >> 4;
    resolution_x = ALIGN(resolution_x, 16) >> 4;
    resolution_y = ALIGN(hevc_state->frame_height, 16) >> 4;
    resolution_y = ALIGN(resolution_y, 8) >> 3;
    kernel_walker_param.resolution_x = resolution_x;
    kernel_walker_param.resolution_y = resolution_y;
    kernel_walker_param.no_dependency = 1;

    gen10_init_media_object_walker_parameter(&kernel_walker_param, &media_object_walker_param);

    gen10_run_kernel_media_object_walker(ctx, encoder_context,
                                         gpe_context,
                                         media_function,
                                         &media_object_walker_param);
}

static void
gen10_hevc_enc_scaling_curbe(VADriverContextP ctx,
                             struct encode_state *encode_state,
                             struct intel_encoder_context *encoder_context,
                             struct i965_gpe_context *gpe_context,
                             struct gen10_hevc_scaling_conversion_param *scale_param)
{
    struct gen10_hevc_enc_context *vme_context = encoder_context->vme_context;
    struct gen10_hevc_enc_state *hevc_state;
    gen10_hevc_scaling_curbe_data      *scaling_curbe;

    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;
    scaling_curbe = i965_gpe_context_map_curbe(gpe_context);

    if (!scaling_curbe)
        return;

    memset(scaling_curbe, 0, sizeof(gen10_hevc_scaling_curbe_data));

    scaling_curbe->dw0.input_bit_depth_for_chroma = 10;
    scaling_curbe->dw0.input_bit_depth_for_luma   = 10;
    scaling_curbe->dw0.output_bit_depth_for_chroma = 8;
    scaling_curbe->dw0.output_bit_depth_for_luma   = 8;
    scaling_curbe->dw0.rounding_enabled   = 1;

    scaling_curbe->dw1.convert_flag                = scale_param->scale_flag.conv_enable;
    scaling_curbe->dw1.downscale_stage             = scale_param->scale_flag.ds_type;
    scaling_curbe->dw1.mb_statistics_dump_flag     = scale_param->scale_flag.dump_enable;
    if (scale_param->scale_flag.is_64lcu) {
        scaling_curbe->dw1.lcu_size                 = 0;
        scaling_curbe->dw1.job_queue_size           = 32;
    } else {
        scaling_curbe->dw1.lcu_size                 = 1;
        scaling_curbe->dw1.job_queue_size           = 2656;
    }

    scaling_curbe->dw2.orig_pic_width_in_pixel   = hevc_state->frame_width;
    scaling_curbe->dw2.orig_pic_height_in_pixel   = hevc_state->frame_height;

    scaling_curbe->dw3.bti_input_conversion_surface    = GEN10_HEVC_SCALING_10BIT_Y;
    scaling_curbe->dw4.bti_input_ds_surface            = GEN10_HEVC_SCALING_8BIT_Y;
    scaling_curbe->dw5.bti_4x_ds_surface               = GEN10_HEVC_SCALING_4xDS;
    scaling_curbe->dw6.bti_mbstat_surface              = GEN10_HEVC_SCALING_MB_STATS;
    scaling_curbe->dw7.bti_2x_ds_surface               = GEN10_HEVC_SCALING_2xDS;
    scaling_curbe->dw8.bti_mb_split_surface            = GEN10_HEVC_SCALING_MB_SPLIT_SURFACE;
    scaling_curbe->dw9.bti_lcu32_jobqueue_buffer_surface  = GEN10_HEVC_SCALING_LCU32_JOB_QUEUE_SCRATCH_SURFACE;
    scaling_curbe->dw10.bti_lcu64_lcu32_jobqueue_buffer_surface = GEN10_HEVC_SCALING_LCU64_JOB_QUEUE_SCRATCH_SURFACE;
    scaling_curbe->dw11.bti_lcu64_cu32_distortion_surface  = GEN10_HEVC_SCALING_LCU64_64x64_DISTORTION_SURFACE;

    i965_gpe_context_unmap_curbe(gpe_context);
}

static void
gen10_hevc_enc_scaling_surfaces(VADriverContextP ctx,
                                struct encode_state *encode_state,
                                struct intel_encoder_context *encoder_context,
                                struct i965_gpe_context *gpe_context,
                                struct gen10_hevc_scaling_conversion_param *scale_param)
{
    struct gen10_hevc_enc_context *vme_context = encoder_context->vme_context;
    int input_bti = 0;
    struct object_surface *obj_surface;

    if (scale_param->scale_flag.conv_enable) {
        obj_surface = scale_param->input_surface;
        i965_add_2d_gpe_surface(ctx,
                                gpe_context,
                                obj_surface,
                                0,
                                1,
                                I965_SURFACEFORMAT_R32_UNORM,
                                input_bti);
        input_bti++;

        i965_add_2d_gpe_surface(ctx,
                                gpe_context,
                                obj_surface,
                                1,
                                1,
                                I965_SURFACEFORMAT_R16G16_UNORM,
                                input_bti);
        input_bti++;

        obj_surface = scale_param->converted_output_surface;
        i965_add_2d_gpe_surface(ctx,
                                gpe_context,
                                obj_surface,
                                0,
                                1,
                                I965_SURFACEFORMAT_R8_UNORM,
                                input_bti);
        input_bti++;
        i965_add_2d_gpe_surface(ctx,
                                gpe_context,
                                obj_surface,
                                1,
                                1,
                                I965_SURFACEFORMAT_R16_UINT,
                                input_bti);
        input_bti++;
    } else {
        input_bti = 2;
        obj_surface = scale_param->input_surface;
        i965_add_2d_gpe_surface(ctx,
                                gpe_context,
                                obj_surface,
                                0,
                                1,
                                I965_SURFACEFORMAT_R32_UNORM,
                                input_bti);
        input_bti++;

        i965_add_2d_gpe_surface(ctx,
                                gpe_context,
                                obj_surface,
                                1,
                                1,
                                I965_SURFACEFORMAT_R16_UINT,
                                input_bti);
        input_bti++;
    }

    if (scale_param->scale_flag.ds_type == GEN10_4X_DS ||
        scale_param->scale_flag.ds_type == GEN10_16X_DS ||
        scale_param->scale_flag.ds_type == GEN10_2X_4X_DS) {
        obj_surface = scale_param->scaled_4x_surface;

        i965_add_2d_gpe_surface(ctx,
                                gpe_context,
                                obj_surface,
                                0,
                                1,
                                I965_SURFACEFORMAT_R32_UNORM,
                                input_bti);
        input_bti++;
    } else
        input_bti++;

    i965_add_buffer_2d_gpe_surface(ctx,
                                   gpe_context,
                                   &vme_context->res_mb_stat_surface,
                                   1,
                                   I965_SURFACEFORMAT_R8_UNORM,
                                   input_bti);
    input_bti++;

    if (scale_param->scale_flag.ds_type == GEN10_2X_DS ||
        scale_param->scale_flag.ds_type == GEN10_2X_4X_DS) {
        obj_surface = scale_param->scaled_2x_surface;

        i965_add_2d_gpe_surface(ctx,
                                gpe_context,
                                obj_surface,
                                0,
                                1,
                                I965_SURFACEFORMAT_R32_UNORM,
                                input_bti);
        input_bti++;
    } else
        input_bti++;

    i965_add_buffer_2d_gpe_surface(ctx,
                                   gpe_context,
                                   &vme_context->res_mb_split_surface,
                                   1,
                                   I965_SURFACEFORMAT_R8_UNORM,
                                   input_bti);
    input_bti++;

    i965_add_buffer_gpe_surface(ctx,
                                gpe_context,
                                &vme_context->res_jbq_header_buffer,
                                0,
                                BYTES2UINT32(vme_context->res_jbq_header_buffer.size),
                                0,
                                input_bti);
    input_bti++;

    i965_add_buffer_gpe_surface(ctx,
                                gpe_context,
                                &vme_context->res_jbq_header_lcu64_buffer,
                                0,
                                BYTES2UINT32(vme_context->res_jbq_header_lcu64_buffer.size),
                                0,
                                input_bti);
    input_bti++;

    i965_add_buffer_gpe_surface(ctx,
                                gpe_context,
                                &vme_context->res_64x64_dist_buffer,
                                0,
                                BYTES2UINT32(vme_context->res_64x64_dist_buffer.size),
                                0,
                                input_bti);
    input_bti++;
}

static void
gen10_hevc_enc_scaling_kernel(VADriverContextP ctx,
                              struct encode_state *encode_state,
                              struct intel_encoder_context *encoder_context,
                              struct gen10_hevc_scaling_conversion_param *scale_param)
{
    struct gen10_hevc_enc_context *vme_context = encoder_context->vme_context;
    struct gen10_hevc_enc_state *hevc_state;
    struct i965_gpe_context *gpe_context;
    int media_function;
    struct gpe_media_object_walker_parameter media_object_walker_param;
    struct gen10_hevc_enc_kernel_walker_parameter kernel_walker_param;

    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;

    gpe_context = &(vme_context->scaling_context.gpe_context);

    gen8_gpe_context_init(ctx, gpe_context);
    gen9_gpe_reset_binding_table(ctx, gpe_context);

    gen10_hevc_enc_scaling_curbe(ctx, encode_state, encoder_context, gpe_context, scale_param);
    gen10_hevc_enc_scaling_surfaces(ctx, encode_state, encoder_context, gpe_context, scale_param);

    gen8_gpe_setup_interface_data(ctx, gpe_context);

    memset(&kernel_walker_param, 0, sizeof(kernel_walker_param));
    if (scale_param->scale_flag.ds_type == GEN10_NONE_DS) {
        kernel_walker_param.resolution_x = hevc_state->frame_width >> 3;
        kernel_walker_param.resolution_y = hevc_state->frame_height >> 3;
        media_function = GEN10_HEVC_MEDIA_STATE_NO_SCALING;
    } else if (scale_param->scale_flag.ds_type == GEN10_2X_DS) {
        kernel_walker_param.resolution_x = ALIGN(hevc_state->frame_width >> 1, 64) >> 3;
        kernel_walker_param.resolution_y = ALIGN(hevc_state->frame_height >> 1, 64) >> 3;
        media_function = GEN10_HEVC_MEDIA_STATE_2X_SCALING;
    } else if (scale_param->scale_flag.ds_type == GEN10_4X_DS ||
               scale_param->scale_flag.ds_type == GEN10_2X_4X_DS) {
        kernel_walker_param.resolution_x = hevc_state->frame_width_4x >> 3;
        kernel_walker_param.resolution_y = hevc_state->frame_height_4x >> 3;

        if (scale_param->scale_flag.ds_type == GEN10_4X_DS)
            media_function = GEN10_HEVC_MEDIA_STATE_4X_SCALING;
        else
            media_function = GEN10_HEVC_MEDIA_STATE_2X_4X_SCALING;
    } else {
        kernel_walker_param.resolution_x = hevc_state->frame_width_16x >> 3;
        kernel_walker_param.resolution_y = hevc_state->frame_height_16x >> 3;

        media_function = GEN10_HEVC_MEDIA_STATE_16X_SCALING;
    }
    kernel_walker_param.no_dependency = 1;

    gen10_init_media_object_walker_parameter(&kernel_walker_param, &media_object_walker_param);

    gen10_run_kernel_media_object_walker(ctx, encoder_context,
                                         gpe_context,
                                         media_function,
                                         &media_object_walker_param);
}

static void
gen10_hevc_enc_conv_scaling_surface(VADriverContextP ctx,
                                    struct encode_state *encode_state,
                                    struct intel_encoder_context *encoder_context,
                                    struct object_surface *input_surface,
                                    struct object_surface *obj_surface,
                                    int only_for_reference)
{
    struct gen10_hevc_enc_context *vme_context;
    struct gen10_hevc_enc_state *hevc_state;
    struct gen10_hevc_scaling_conversion_param scale_param;
    struct gen10_hevc_surface_priv *surface_priv;

    vme_context = encoder_context->vme_context;
    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;
    surface_priv = (struct gen10_hevc_surface_priv *)(obj_surface->private_data);

    if (!hevc_state->is_10bit &&
        !hevc_state->is_64lcu &&
        !hevc_state->hme_supported)
        return;

    memset(&scale_param, 0, sizeof(scale_param));

    scale_param.input_surface = input_surface ? input_surface : obj_surface;
    scale_param.input_width = hevc_state->frame_width;
    scale_param.input_height = hevc_state->frame_height;
    scale_param.output_4x_width = hevc_state->frame_width_4x;
    scale_param.output_4x_height = hevc_state->frame_height_4x;
    scale_param.scaled_2x_surface = surface_priv->scaled_2x_surface;
    scale_param.scaled_4x_surface = surface_priv->scaled_4x_surface;
    scale_param.converted_output_surface = surface_priv->converted_surface;

    if (hevc_state->is_10bit)
        scale_param.scale_flag.conv_enable = GEN10_DEPTH_CONV_ENABLE;

    scale_param.scale_flag.is_64lcu = hevc_state->is_64lcu;

    scale_param.scale_flag.dump_enable = 0;
    if (hevc_state->is_64lcu && hevc_state->hme_supported) {
        scale_param.scale_flag.ds_type = GEN10_2X_4X_DS;
        scale_param.scale_flag.dump_enable = hevc_state->brc.brc_enabled ? 1 : 0;
    } else if (hevc_state->is_64lcu)
        scale_param.scale_flag.ds_type = GEN10_2X_DS;
    else if (hevc_state->hme_supported) {
        scale_param.scale_flag.ds_type = GEN10_4X_DS;
        scale_param.scale_flag.dump_enable = hevc_state->brc.brc_enabled ? 1 : 0;
    } else
        scale_param.scale_flag.ds_type = GEN10_NONE_DS;

    gen10_hevc_enc_scaling_kernel(ctx, encode_state,
                                  encoder_context,
                                  &scale_param);

    if (only_for_reference)
        surface_priv->conv_scaling_done = 1;

    if (!hevc_state->b16xme_supported ||
        only_for_reference)
        return;

    memset(&scale_param, 0, sizeof(scale_param));

    scale_param.input_surface = surface_priv->scaled_4x_surface;
    scale_param.scaled_4x_surface = surface_priv->scaled_16x_surface;
    scale_param.input_width = hevc_state->frame_width_4x;
    scale_param.input_height = hevc_state->frame_height_4x;
    scale_param.output_4x_width = hevc_state->frame_width_16x;
    scale_param.output_4x_height = hevc_state->frame_height_16x;

    scale_param.scale_flag.ds_type = GEN10_16X_DS;

    gen10_hevc_enc_scaling_kernel(ctx, encode_state,
                                  encoder_context,
                                  &scale_param);
}

#define GEN10_HEVC_HME_STAGE_4X_NO_16X       0
#define GEN10_HEVC_HME_STAGE_4X_AFTER_16X    1
#define GEN10_HEVC_HME_STAGE_16X             2

static void
gen10_hevc_enc_me_curbe(VADriverContextP ctx,
                        struct encode_state *encode_state,
                        struct intel_encoder_context *encoder_context,
                        struct i965_gpe_context *gpe_context,
                        uint32_t hme_level,
                        int dist_type)
{
    struct gen10_hevc_enc_context *vme_context = encoder_context->vme_context;
    struct gen10_hevc_enc_state *hevc_state;
    gen10_hevc_me_curbe_data      *me_curbe;
    VAEncSliceParameterBufferHEVC *slice_param;
    VAEncSequenceParameterBufferHEVC *seq_param;

    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;

    seq_param = (VAEncSequenceParameterBufferHEVC *)encode_state->seq_param_ext->buffer;
    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[0]->buffer;

    me_curbe = i965_gpe_context_map_curbe(gpe_context);

    if (!me_curbe)
        return;

    memset(me_curbe, 0, sizeof(gen10_hevc_me_curbe_data));

    me_curbe->dw0.rounded_frame_width_in_mv_for4x       = hevc_state->frame_width_4x >> 3;
    me_curbe->dw0.rounded_frame_height_in_mv_for4x      = hevc_state->frame_height_4x >> 3;

    me_curbe->dw2.sub_pel_mode                          = 3;
    me_curbe->dw2.bme_disable_fbr                       = 1;
    me_curbe->dw2.inter_sad_adj                         = 2;

    me_curbe->dw3.adaptive_search_en                    = 1;
    me_curbe->dw3.ime_ref_window_size                   = 1; // From the HW-spec

    me_curbe->dw4.quarter_quad_tree_cand                = 1; // 32x32 split is enabled.
    me_curbe->dw4.bi_weight                             = 32; // default weight.

    me_curbe->dw5.len_sp                                = 0x3F;
    me_curbe->dw5.max_num_su                            = 0x3F;
    me_curbe->dw5.start_center0_x                       = ((gen10_hevc_ime_ref_window_size[1][0] - 32) >> 3) & 0xF;
    me_curbe->dw5.start_center0_y                       = ((gen10_hevc_ime_ref_window_size[1][1] - 32) >> 3) & 0xF;

    me_curbe->dw6.slice_type                            = (dist_type == GEN10_HEVC_ME_DIST_TYPE_INTER_BRC) ? 1 : 0;
    if (dist_type == GEN10_HEVC_ME_DIST_TYPE_INTER_BRC) {
        if (hme_level == GEN10_HEVC_HME_LEVEL_4X)
            me_curbe->dw6.hme_stage =
                (hevc_state->b16xme_enabled) ? GEN10_HEVC_HME_STAGE_4X_AFTER_16X :
                GEN10_HEVC_HME_STAGE_4X_NO_16X;
        else
            me_curbe->dw6.hme_stage = GEN10_HEVC_HME_STAGE_16X;
    } else
        me_curbe->dw6.hme_stage = GEN10_HEVC_HME_STAGE_4X_NO_16X;

    if (slice_param->slice_type == HEVC_SLICE_I) {
        me_curbe->dw6.num_ref_l0 = 0;
        me_curbe->dw6.num_ref_l1 = 0;
    } else if (slice_param->slice_type == HEVC_SLICE_P) {
        me_curbe->dw6.num_ref_l0 = slice_param->num_ref_idx_l0_active_minus1 + 1;
        me_curbe->dw6.num_ref_l1 = 0;
    } else {
        me_curbe->dw6.num_ref_l0 = slice_param->num_ref_idx_l0_active_minus1 + 1;
        me_curbe->dw6.num_ref_l1 = hevc_state->low_delay ? 0 : slice_param->num_ref_idx_l1_active_minus1 + 1;
    }

    me_curbe->dw7.rounded_frame_width_in_mv_for16x = hevc_state->frame_width_16x >> 3;
    me_curbe->dw7.rounded_frame_height_in_mv_for16x = hevc_state->frame_height_16x >> 3;

    /* Search path */
    memcpy(&me_curbe->ime_search_path_03, gen10_hevc_me_search_path,
           sizeof(gen10_hevc_me_search_path));

    me_curbe->dw24.coding_unit_size = 1;
    me_curbe->dw24.coding_unit_partition_mode = 0;
    me_curbe->dw24.coding_unit_prediction_mode = 1;

    if (hme_level == GEN10_HEVC_HME_LEVEL_4X) {
        me_curbe->dw25.frame_width_in_pixel_cs = hevc_state->frame_width >> 2;
        me_curbe->dw25.frame_height_in_pixel_cs = hevc_state->frame_height >> 2;
    } else {
        me_curbe->dw25.frame_width_in_pixel_cs = hevc_state->frame_width >> 4;
        me_curbe->dw25.frame_height_in_pixel_cs = hevc_state->frame_height >> 4;
    }

    me_curbe->dw27.intra_compute_type = 1;

    me_curbe->dw28.penalty_intra32x32_nondc = 36;
    me_curbe->dw28.penalty_intra16x16_nondc = 12;
    me_curbe->dw28.penalty_intra8x8_nondc = 4;

    me_curbe->dw30.mode4_cost = 13;
    me_curbe->dw30.mode5_cost = 9;
    me_curbe->dw30.mode6_cost = 13;
    me_curbe->dw30.mode7_cost = 3;
    me_curbe->dw31.mode8_cost = 9;

    me_curbe->dw32.sicintra_neighbor_avail_flag         = 0x3F;
    me_curbe->dw32.sic_inter_sad_measure                = 0x02;
    me_curbe->dw32.sic_intra_sad_measure                = 0x02;

    me_curbe->dw33.sic_log2_min_cu_size                 = seq_param->log2_min_luma_coding_block_size_minus3 + 3;

    me_curbe->dw34.bti_hme_output_mv_data_surface       = GEN10_HEVC_HME_OUTPUT_MV_DATA;
    me_curbe->dw35.bti_16xinput_mv_data_surface         = GEN10_HEVC_HME_16xINPUT_MV_DATA;
    me_curbe->dw36.bti_4x_output_distortion_surface     = GEN10_HEVC_HME_4xOUTPUT_DISTORTION;
    me_curbe->dw37.bti_vme_input_surface                = GEN10_HEVC_HME_VME_PRED_CURR_PIC_IDX0;
    me_curbe->dw38.bti_4xds_surface                     = GEN10_HEVC_HME_4xDS_INPUT;
    me_curbe->dw39.bti_brc_distortion_surface           = GEN10_HEVC_HME_BRC_DISTORTION;
    me_curbe->dw40.bti_mv_and_distortion_sum_surface    = GEN10_HEVC_HME_MV_AND_DISTORTION_SUM;

    i965_gpe_context_unmap_curbe(gpe_context);
}

static void
gen10_hevc_enc_me_surfaces(VADriverContextP ctx,
                           struct encode_state *encode_state,
                           struct intel_encoder_context *encoder_context,
                           struct i965_gpe_context *gpe_context,
                           uint32_t hme_level,
                           int dist_type)
{
    struct gen10_hevc_enc_context *vme_context = encoder_context->vme_context;
    struct gen10_hevc_enc_state *hevc_state;
    struct gen10_hevc_enc_frame_info *frame_info;
    struct gen10_hevc_enc_common_res *common_res;
    struct object_surface *obj_surface, *vme_surface;
    struct gen10_hevc_surface_priv *surface_priv;
    struct i965_gpe_resource *res_source;
    int input_bti, i;

    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;
    frame_info = &vme_context->frame_info;
    common_res = &vme_context->common_res;

    obj_surface = encode_state->reconstructed_object;

    surface_priv = (struct gen10_hevc_surface_priv *)(obj_surface->private_data);

    if (hme_level == GEN10_HEVC_HME_LEVEL_4X) {
        vme_surface = surface_priv->scaled_4x_surface;
        res_source = &vme_context->res_s4x_memv_data_surface;
    } else {
        vme_surface = surface_priv->scaled_16x_surface;
        res_source = &vme_context->res_s16x_memv_data_surface;
    }

    input_bti = 0;
    i965_add_buffer_2d_gpe_surface(ctx, gpe_context, res_source,
                                   1, I965_SURFACEFORMAT_R8_UNORM,
                                   GEN10_HEVC_HME_OUTPUT_MV_DATA);

    if (hme_level == GEN10_HEVC_HME_LEVEL_4X && hevc_state->b16xme_enabled)
        i965_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                       &vme_context->res_s16x_memv_data_surface,
                                       1, I965_SURFACEFORMAT_R8_UNORM,
                                       GEN10_HEVC_HME_16xINPUT_MV_DATA);

    if (hme_level == GEN10_HEVC_HME_LEVEL_4X)
        i965_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                       &vme_context->res_s4x_me_dist_surface,
                                       1, I965_SURFACEFORMAT_R8_UNORM,
                                       GEN10_HEVC_HME_4xOUTPUT_DISTORTION);

    input_bti = GEN10_HEVC_HME_VME_PRED_CURR_PIC_IDX0;

    i965_add_adv_gpe_surface(ctx, gpe_context,
                             vme_surface,
                             input_bti);
    input_bti++;

    for (i = 0; i < 4; i++) {
        struct object_surface *tmp_surface, *input_surface;
        struct gen10_hevc_surface_priv *tmp_hevc_surface;

        if (frame_info->mapped_ref_idx_list0[i] >= 0)
            tmp_surface = common_res->reference_pics[frame_info->mapped_ref_idx_list0[i]].obj_surface;
        else
            tmp_surface = NULL;

        if (tmp_surface && tmp_surface->private_data) {
            tmp_hevc_surface = (struct gen10_hevc_surface_priv *)(tmp_surface->private_data);

            if (hme_level == GEN10_HEVC_HME_LEVEL_4X)
                input_surface = tmp_hevc_surface->scaled_4x_surface;
            else
                input_surface = tmp_hevc_surface->scaled_16x_surface;

            i965_add_adv_gpe_surface(ctx, gpe_context,
                                     input_surface,
                                     input_bti + 2 * i);
        } else
            i965_add_adv_gpe_surface(ctx, gpe_context,
                                     vme_surface,
                                     input_bti + 2 * i);

        if (frame_info->mapped_ref_idx_list1[i] >= 0)
            tmp_surface = common_res->reference_pics[frame_info->mapped_ref_idx_list1[i]].obj_surface;
        else
            tmp_surface = NULL;

        if (tmp_surface && tmp_surface->private_data) {
            tmp_hevc_surface = (struct gen10_hevc_surface_priv *)(tmp_surface->private_data);

            if (hme_level == GEN10_HEVC_HME_LEVEL_4X)
                input_surface = tmp_hevc_surface->scaled_4x_surface;
            else
                input_surface = tmp_hevc_surface->scaled_16x_surface;

            i965_add_adv_gpe_surface(ctx, gpe_context,
                                     input_surface,
                                     input_bti + 2 * i + 1);
        } else
            i965_add_adv_gpe_surface(ctx, gpe_context,
                                     vme_surface,
                                     input_bti + 2 * i + 1);
    }

    if (hme_level == GEN10_HEVC_HME_LEVEL_4X) {
        i965_add_2d_gpe_surface(ctx,
                                gpe_context,
                                vme_surface,
                                0,
                                1,
                                I965_SURFACEFORMAT_R8_UNORM,
                                GEN10_HEVC_HME_4xDS_INPUT);

        if (dist_type != GEN10_HEVC_ME_DIST_TYPE_INTRA)
            res_source = &vme_context->res_brc_me_dist_surface;
        else
            res_source = &vme_context->res_brc_intra_dist_surface;

        i965_add_buffer_2d_gpe_surface(ctx, gpe_context, res_source,
                                       1, I965_SURFACEFORMAT_R8_UNORM,
                                       GEN10_HEVC_HME_BRC_DISTORTION);
    }

    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_mv_dist_sum_buffer,
                                1,
                                vme_context->res_mv_dist_sum_buffer.size,
                                0,
                                GEN10_HEVC_HME_MV_AND_DISTORTION_SUM);
}

static void
gen10_hevc_enc_me_kernel(VADriverContextP ctx,
                         struct encode_state *encode_state,
                         struct intel_encoder_context *encoder_context,
                         int hme_level,
                         int dist_type)
{
    struct gen10_hevc_enc_context *vme_context = encoder_context->vme_context;
    struct gen10_hevc_enc_state *hevc_state;
    struct i965_gpe_context *gpe_context;
    int media_function;
    struct gpe_media_object_walker_parameter media_object_walker_param;
    struct gen10_hevc_enc_kernel_walker_parameter kernel_walker_param;

    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;

    gpe_context = &(vme_context->me_context.gpe_context);

    gen8_gpe_context_init(ctx, gpe_context);
    gen9_gpe_reset_binding_table(ctx, gpe_context);

    gen10_hevc_enc_me_curbe(ctx, encode_state, encoder_context, gpe_context, hme_level, dist_type);
    gen10_hevc_enc_me_surfaces(ctx, encode_state, encoder_context, gpe_context, hme_level, dist_type);

    gen8_gpe_setup_interface_data(ctx, gpe_context);

    memset(&kernel_walker_param, 0, sizeof(kernel_walker_param));

    if (hme_level == GEN10_HEVC_HME_LEVEL_4X) {
        kernel_walker_param.resolution_x = hevc_state->frame_width_4x >> 5;
        kernel_walker_param.resolution_y = hevc_state->frame_height_4x >> 5;

        media_function = GEN10_HEVC_MEDIA_STATE_4XME;
    } else {
        kernel_walker_param.resolution_x = hevc_state->frame_width_16x >> 5;
        kernel_walker_param.resolution_y = hevc_state->frame_height_16x >> 5;

        media_function = GEN10_HEVC_MEDIA_STATE_16XME;
    }

    kernel_walker_param.no_dependency = 1;

    gen10_init_media_object_walker_parameter(&kernel_walker_param, &media_object_walker_param);

    gen10_run_kernel_media_object_walker(ctx, encoder_context,
                                         gpe_context,
                                         media_function,
                                         &media_object_walker_param);
}

#define     LUTMODE_INTRA_NONPRED_HEVC  0x00
#define     LUTMODE_INTRA_32x32_HEVC    0x01
#define     LUTMODE_INTRA_16x16_HEVC    0x02
#define     LUTMODE_INTRA_8x8_HEVC      0x03
#define     LUTMODE_INTER_32x16_HEVC    0x04
#define     LUTMODE_INTER_16x32_HEVC    0x04
#define     LUTMODE_INTER_AMP_HEVC      0x04
#define     LUTMODE_INTER_16x16_HEVC    0x05
#define     LUTMODE_INTER_16x8_HEVC     0x06
#define     LUTMODE_INTER_8x16_HEVC     0x06
#define     LUTMODE_INTER_8x8_HEVC      0x07
#define     LUTMODE_INTER_32x32_HEVC    0x08
#define     LUTMODE_INTER_BIDIR_HEVC    0x09
#define     LUTMODE_REF_ID_HEVC         0x0A
#define     LUTMODE_INTRA_CHROMA_HEVC   0x0B

#define     LAMBDA_RD_IDX               0x10
#define     LAMBDA_MD_IDX               0x11
#define     TUSAD_THR_IDX               0x12

#define     MAX_MODE_COST               0x20

static uint8_t
map_44_lut_value(uint32_t value,
                 uint8_t max)
{
    uint32_t max_cost = 0;
    int data = 0;
    uint8_t ret = 0;

    if (value == 0)
        return 0;

    max_cost = ((max & 15) << (max >> 4));
    if (value >= max_cost)
        return max;

    data = (int)(log((double)value) / log(2.)) - 3;
    if (data < 0)
        data = 0;

    ret = (uint8_t)((data << 4) +
                    (int)((value + (data == 0 ? 0 : (1 << (data - 1)))) >> data));
    ret = (ret & 0xf) == 0 ? (ret | 8) : ret;

    return ret;
}

static void
gen10_hevc_calc_costs(uint32_t *mode_cost, int slice_type, int qp, bool b_lcu64)
{
    unsigned short lambda_md;
    unsigned int lambda_rd;
    unsigned int tu_sad_thres;
    float qp_value;
    double lambda;
    double intra_weigh_factor;
    double inter_weigh_factor;
    double qp_scale, cost_scale;
    int lcu_idx;

    if (!mode_cost)
        return;

    if (slice_type == HEVC_SLICE_I) {
        qp_scale = 5.0;
        cost_scale = 1.0;
    } else {
        qp_scale = 0.55;
        cost_scale = 2.0;
    }

    if (b_lcu64)
        lcu_idx = 1;
    else
        lcu_idx = 0;

    qp_value = qp - 12;
    if (qp_value < 0)
        qp_value = 0;

    lambda     = sqrt(qp_scale * pow(2.0, qp_value / 3.0));
    lambda_rd  = (unsigned int)(qp_scale * pow(2.0, qp_value / 3.0) * 256 + 0.5);
    lambda_md  = (unsigned short)(lambda * 256 + 0.5);
    tu_sad_thres = (unsigned int)(sqrt(0.85 * pow(2.0, qp_value / 3.0)) * 0.4 * 256 + 0.5);

    inter_weigh_factor = cost_scale * lambda;
    intra_weigh_factor = inter_weigh_factor * gen10_hevc_lambda_factor[slice_type][qp];

    mode_cost[LAMBDA_RD_IDX]   = lambda_rd;
    mode_cost[LAMBDA_MD_IDX]   = lambda_md;
    mode_cost[TUSAD_THR_IDX]   = tu_sad_thres;

    mode_cost[LUTMODE_INTRA_NONPRED_HEVC] = map_44_lut_value((uint32_t)(intra_weigh_factor * gen10_hevc_mode_bits[lcu_idx][slice_type][LUTMODE_INTRA_NONPRED_HEVC]), 0x6f);
    mode_cost[LUTMODE_INTRA_32x32_HEVC]   = map_44_lut_value((uint32_t)(intra_weigh_factor * gen10_hevc_mode_bits[lcu_idx][slice_type][LUTMODE_INTRA_32x32_HEVC]), 0x8f);
    mode_cost[LUTMODE_INTRA_16x16_HEVC]   = map_44_lut_value((uint32_t)(intra_weigh_factor * gen10_hevc_mode_bits[lcu_idx][slice_type][LUTMODE_INTRA_16x16_HEVC]), 0x8f);
    mode_cost[LUTMODE_INTRA_8x8_HEVC]     = map_44_lut_value((uint32_t)(intra_weigh_factor * gen10_hevc_mode_bits[lcu_idx][slice_type][LUTMODE_INTRA_8x8_HEVC]), 0x8f);
    mode_cost[LUTMODE_INTRA_CHROMA_HEVC]  = map_44_lut_value((uint32_t)(intra_weigh_factor * gen10_hevc_mode_bits[lcu_idx][slice_type][LUTMODE_INTRA_CHROMA_HEVC]), 0x6f);

    mode_cost[LUTMODE_INTER_32x32_HEVC]   = map_44_lut_value((uint32_t)(inter_weigh_factor * gen10_hevc_mode_bits[lcu_idx][slice_type][LUTMODE_INTER_32x32_HEVC]), 0x8f);
    mode_cost[LUTMODE_INTER_32x16_HEVC]   = map_44_lut_value((uint32_t)(inter_weigh_factor * gen10_hevc_mode_bits[lcu_idx][slice_type][LUTMODE_INTER_32x16_HEVC]), 0x8f);
    mode_cost[LUTMODE_INTER_16x16_HEVC]   = map_44_lut_value((uint32_t)(inter_weigh_factor * gen10_hevc_mode_bits[lcu_idx][slice_type][LUTMODE_INTER_16x16_HEVC]), 0x6f);
    mode_cost[LUTMODE_INTER_16x8_HEVC]    = map_44_lut_value((uint32_t)(inter_weigh_factor * gen10_hevc_mode_bits[lcu_idx][slice_type][LUTMODE_INTER_16x8_HEVC]), 0x6f);
    mode_cost[LUTMODE_INTER_8x8_HEVC]     = map_44_lut_value((uint32_t)(0.45 * gen10_hevc_mode_bits[lcu_idx][slice_type][LUTMODE_INTER_8x8_HEVC]), 0x6f);

    mode_cost[LUTMODE_INTER_BIDIR_HEVC]   = map_44_lut_value((uint32_t)(inter_weigh_factor * gen10_hevc_mode_bits[lcu_idx][slice_type][LUTMODE_INTER_BIDIR_HEVC]), 0x6f);
    if (slice_type != HEVC_SLICE_I)
        mode_cost[LUTMODE_REF_ID_HEVC]    = map_44_lut_value((uint32_t)(inter_weigh_factor * gen10_hevc_mode_bits[lcu_idx][slice_type][LUTMODE_REF_ID_HEVC]), 0x6f);
    else
        mode_cost[LUTMODE_REF_ID_HEVC]    = 0;
}

static void
gen10_hevc_enc_generate_regions_in_slice_control(VADriverContextP ctx,
                                                 struct encode_state *encode_state,
                                                 struct intel_encoder_context *encoder_context)
{
    struct gen10_hevc_enc_context *vme_context = encoder_context->vme_context;
    struct gen10_hevc_enc_state *hevc_state;
    VAEncSliceParameterBufferHEVC *slice_param;
    gen10_hevc_concurrent_tg_data *pregion;
    int i, k, slice, num_regions, height, num_slices;
    int num_wf_in_region;
    uint32_t  frame_width_in_ctb, frame_height_in_ctb;
    bool is_arbitary_slices;
    int slice_starty[I965_MAX_NUM_SLICE + 1];
    int regions_start_table[64];
    uint32_t start_offset_to_region[16];
    int16_t data_tmp[32][32];
    int max_height;
    int log2_lcu_size;
    int copy_blk_size = 0;

    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;

    memset(slice_starty, 0, sizeof(slice_starty));
    memset(regions_start_table, 0, sizeof(regions_start_table));
    memset(data_tmp, 0, sizeof(data_tmp));
    memset(&hevc_state->hevc_wf_param, 0, sizeof(hevc_state->hevc_wf_param));
    memset(start_offset_to_region, 0, sizeof(start_offset_to_region));

    frame_width_in_ctb = vme_context->frame_info.width_in_lcu;
    frame_height_in_ctb = vme_context->frame_info.height_in_lcu;
    if (hevc_state->is_64lcu) {
        log2_lcu_size = 6;
        copy_blk_size = 22;
    } else {
        log2_lcu_size = 5;
        copy_blk_size = 18;
    }

    is_arbitary_slices = false;
    for (slice = 0; slice < encode_state->num_slice_params_ext; slice++) {
        slice_param = NULL;
        if (encode_state->slice_params_ext[slice] &&
            encode_state->slice_params_ext[slice]->buffer)
            slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[slice]->buffer;

        if (!slice_param)
            continue;

        if (slice_param->slice_segment_address % frame_width_in_ctb) {
            is_arbitary_slices = true;
        } else {
            slice_starty[slice] = slice_param->slice_segment_address / frame_width_in_ctb;
        }
    }

    slice_starty[encode_state->num_slice_params_ext] = frame_height_in_ctb;

    regions_start_table[0] = 0;
    regions_start_table[GEN10_HEVC_REGION_START_Y_OFFSET + 0] = 0;
    num_regions = 1;

    if (is_arbitary_slices) {
        height = frame_height_in_ctb;
        num_slices = 1;
        max_height = height;
        if (hevc_state->num_regions_in_slice > 1) {
            num_wf_in_region = (frame_width_in_ctb + 2 * (frame_height_in_ctb - 1) + hevc_state->num_regions_in_slice - 1) /
                               hevc_state->num_regions_in_slice;

            num_regions = hevc_state->num_regions_in_slice;

            for (i = 1; i < hevc_state->num_regions_in_slice; i++) {
                int front = i * num_wf_in_region;

                if (front < frame_width_in_ctb) {
                    regions_start_table[i] = front;
                } else if (((front - frame_width_in_ctb + 1) & 1) == 0) {
                    regions_start_table[i] = frame_width_in_ctb - 1;
                } else {
                    regions_start_table[i] = frame_width_in_ctb - 2;
                }

                regions_start_table[GEN10_HEVC_REGION_START_Y_OFFSET + i] = (front - regions_start_table[i]) >> 1;
            }
        }
    } else {
        int start_y = 0, slice_height;
        int slice_is_merged = 0;

        max_height = 0;
        num_slices = encode_state->num_slice_params_ext;

        for (slice = 0; slice < num_slices; slice++) {
            slice_height = slice_starty[slice + 1] - slice_starty[slice];

            if (slice_height > max_height)
                max_height = slice_height;
        }

        while (!slice_is_merged) {
            int new_num_slices = 1;

            start_y = 0;

            for (slice = 1; slice < num_slices; slice++) {
                if ((slice_starty[slice + 1] - start_y) <= max_height) {
                    slice_starty[slice] = -1;
                } else {
                    start_y = slice_starty[slice];
                }
            }

            for (slice = 1; slice < num_slices; slice++) {
                if (slice_starty[slice] > 0) {
                    slice_starty[new_num_slices] = slice_starty[slice];
                    new_num_slices++;
                }
            }

            num_slices = new_num_slices;
            slice_starty[num_slices] = frame_height_in_ctb;

            if (num_slices * hevc_state->num_regions_in_slice <= 16) {
                slice_is_merged = 1;
            } else {
                int num = 1;

                max_height = frame_height_in_ctb;

                for (slice = 0; slice < num_slices - 1; slice++) {
                    if ((slice_starty[slice + 2] - slice_starty[slice]) <= max_height) {
                        max_height = slice_starty[slice + 2] - slice_starty[slice];
                        num = slice + 1;
                    }
                }

                for (slice = num; slice < num_slices; slice++)
                    slice_starty[slice] = slice_starty[slice + 1];

                num_slices--;
            }
        }

        num_wf_in_region = (frame_width_in_ctb + 2 * (max_height - 1) + hevc_state->num_regions_in_slice - 1) /
                           hevc_state->num_regions_in_slice;
        num_regions = num_slices * hevc_state->num_regions_in_slice;

        for (slice = 0; slice < num_slices; slice++) {
            regions_start_table[slice * hevc_state->num_regions_in_slice] = 0;
            regions_start_table[GEN10_HEVC_REGION_START_Y_OFFSET + (slice * hevc_state->num_regions_in_slice)] = slice_starty[slice];

            for (i = 1; i < hevc_state->num_regions_in_slice; i++) {
                int front = i * num_wf_in_region;

                if (front < frame_width_in_ctb)
                    regions_start_table[slice * hevc_state->num_regions_in_slice + i] = front;
                else if (((front - frame_width_in_ctb + 1) & 1) == 0)
                    regions_start_table[slice * hevc_state->num_regions_in_slice + i] = frame_width_in_ctb - 1;
                else
                    regions_start_table[slice * hevc_state->num_regions_in_slice + i] = frame_width_in_ctb - 2;

                regions_start_table[GEN10_HEVC_REGION_START_Y_OFFSET + (slice * hevc_state->num_regions_in_slice + i)] = slice_starty[slice] +
                                                                                                                         ((front - regions_start_table[i]) >> 1);
            }
        }
        height = max_height;
    }

    for (k = 0; k < num_slices; k++) {
        int nearest_reg = 0, delta, tmp_y;
        int min_delta = hevc_state->frame_height;
        int cur_lcu_pel_y = regions_start_table[GEN10_HEVC_REGION_START_Y_OFFSET + (k * hevc_state->num_regions_in_slice)] << log2_lcu_size;
        int ts_width   = frame_width_in_ctb;
        int ts_height  = height;
        int offset_y   = -((ts_width + 1) >> 1);
        int offset_delta = ((ts_width + ((ts_height - 1) << 1)) + (hevc_state->num_regions_in_slice - 1)) / (hevc_state->num_regions_in_slice);

        for (i = 0; i < num_regions; i++) {
            if (regions_start_table[i] == 0) {
                delta = cur_lcu_pel_y - (regions_start_table[GEN10_HEVC_REGION_START_Y_OFFSET + i] << log2_lcu_size);

                if (delta >= 0) {
                    if (delta < min_delta) {
                        min_delta = delta;
                        nearest_reg = i;
                    }
                }
            }

            start_offset_to_region[k] = 2 * regions_start_table[GEN10_HEVC_REGION_START_Y_OFFSET + nearest_reg];
        }
        for (i = 0; i < hevc_state->num_regions_in_slice; i++) {
            data_tmp[k * hevc_state->num_regions_in_slice + i][0] = slice_starty[k] * frame_width_in_ctb;
            data_tmp[k * hevc_state->num_regions_in_slice + i][1] = (k == (num_slices - 1)) ?
                                                                    frame_width_in_ctb * frame_height_in_ctb : slice_starty[k + 1] * frame_width_in_ctb;
            data_tmp[k * hevc_state->num_regions_in_slice + i][2] = k * hevc_state->num_regions_in_slice + i;
            if (!hevc_state->is_64lcu && hevc_state->num_regions_in_slice == 1) {
                continue;
            }

            data_tmp[k * hevc_state->num_regions_in_slice + i][3] = height;
            data_tmp[k * hevc_state->num_regions_in_slice + i][4] = regions_start_table[nearest_reg + i];
            data_tmp[k * hevc_state->num_regions_in_slice + i][5] = regions_start_table[GEN10_HEVC_REGION_START_Y_OFFSET + (nearest_reg + i)];
            data_tmp[k * hevc_state->num_regions_in_slice + i][6] = regions_start_table[GEN10_HEVC_REGION_START_Y_OFFSET + nearest_reg];
            tmp_y = regions_start_table[GEN10_HEVC_REGION_START_Y_OFFSET + (nearest_reg + hevc_state->num_regions_in_slice)];
            data_tmp[k * hevc_state->num_regions_in_slice + i][7] = (tmp_y != 0) ? tmp_y : frame_height_in_ctb;
            data_tmp[k * hevc_state->num_regions_in_slice + i][8] = offset_y + regions_start_table[GEN10_HEVC_REGION_START_Y_OFFSET + nearest_reg] + ((i * offset_delta) >> 1);
            if (hevc_state->is_64lcu) {
                data_tmp[k * hevc_state->num_regions_in_slice + i][9] = (frame_width_in_ctb + 2 * (max_height - 1) + hevc_state->num_regions_in_slice - 1) / hevc_state->num_regions_in_slice;
                data_tmp[k * hevc_state->num_regions_in_slice + i][10] = num_regions;
            }
        }
    }


    pregion = (gen10_hevc_concurrent_tg_data *) i965_map_gpe_resource(&vme_context->res_concurrent_tg_data);
    if (!pregion)
        return;

    memset(pregion, 0, vme_context->res_concurrent_tg_data.size);

    for (i = 0; i < 16; i++) {
        memcpy(pregion, data_tmp[i], copy_blk_size);
        pregion++;
    }

    hevc_state->hevc_wf_param.max_height_in_region = max_height;
    hevc_state->hevc_wf_param.num_regions = num_regions;
    hevc_state->hevc_wf_param.num_unit_in_wf = (frame_width_in_ctb + 2 * (max_height - 1) + hevc_state->num_regions_in_slice - 1) / hevc_state->num_regions_in_slice;

    i965_unmap_gpe_resource(&vme_context->res_concurrent_tg_data);
}

static void
gen10_hevc_enc_generate_lculevel_data(VADriverContextP ctx,
                                      struct encode_state *encode_state,
                                      struct intel_encoder_context *encoder_context)
{
    struct gen10_hevc_enc_context *vme_context = encoder_context->vme_context;
    VAEncPictureParameterBufferHEVC *pic_param;
    VAEncSliceParameterBufferHEVC *slice_param;
    gen10_hevc_lcu_level_data *plcu_level_data;
    int ui_start_lcu, slice_idx, i;

    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;

    plcu_level_data = (gen10_hevc_lcu_level_data *)
                      i965_map_gpe_resource(&vme_context->res_lculevel_input_data_buffer);

    if (!plcu_level_data)
        return;

    slice_idx = 0;
    for (ui_start_lcu = 0, slice_idx = 0; slice_idx < encode_state->num_slice_params_ext; slice_idx++) {

        slice_param = NULL;
        if (encode_state->slice_params_ext[slice_idx] &&
            encode_state->slice_params_ext[slice_idx]->buffer)
            slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[slice_idx]->buffer;

        if (!slice_param)
            continue;

        for (i = 0; i < slice_param->num_ctu_in_slice; i++, plcu_level_data++) {
            plcu_level_data->slice_start_lcu_idx = ui_start_lcu;
            plcu_level_data->slice_end_lcu_idx   = ui_start_lcu + slice_param->num_ctu_in_slice;
            plcu_level_data->slice_id            = slice_idx + 1;
            plcu_level_data->slice_qp            = pic_param->pic_init_qp + slice_param->slice_qp_delta;
        }

        ui_start_lcu += slice_param->num_ctu_in_slice;
    }

    i965_unmap_gpe_resource(&vme_context->res_lculevel_input_data_buffer);
}

static void
gen10_hevc_enc_mbenc_intra_curbe(VADriverContextP ctx,
                                 struct encode_state *encode_state,
                                 struct intel_encoder_context *encoder_context,
                                 struct i965_gpe_context *gpe_context)
{
    struct gen10_hevc_enc_context *vme_context = encoder_context->vme_context;
    struct gen10_hevc_enc_state *hevc_state;
    gen10_hevc_mbenc_intra_curbe_data *mbenc_curbe;
    VAEncSliceParameterBufferHEVC *slice_param;
    VAEncPictureParameterBufferHEVC *pic_param;
    VAEncSequenceParameterBufferHEVC *seq_param;
    int slice_qp;
    unsigned int mode_cost[MAX_MODE_COST];
    int tu_idx;

    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;

    seq_param = (VAEncSequenceParameterBufferHEVC *)encode_state->seq_param_ext->buffer;
    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[0]->buffer;
    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;
    mbenc_curbe = i965_gpe_context_map_curbe(gpe_context);

    if (!mbenc_curbe)
        return;

    memset(mbenc_curbe, 0, sizeof(gen10_hevc_mbenc_intra_curbe_data));

    if (hevc_state->brc.target_usage < 3)
        tu_idx = 0;
    else if (hevc_state->brc.target_usage < 7)
        tu_idx = 1;
    else
        tu_idx = 2;

    mbenc_curbe->dw0.frame_width_in_pixel  = hevc_state->frame_width;
    mbenc_curbe->dw0.frame_height_in_pixel = hevc_state->frame_height;

    mbenc_curbe->dw1.penalty_intra32x32_nondc_pred = 36;
    mbenc_curbe->dw1.penalty_intra16x16_nondc_pred = 12;
    mbenc_curbe->dw1.penalty_intra8x8_nondc_pred   = 4;

    mbenc_curbe->dw2.intra_sad_measure_adj =    2;
    slice_qp = slice_param->slice_qp_delta + pic_param->pic_init_qp;
    gen10_hevc_calc_costs(mode_cost, HEVC_SLICE_I, slice_qp, hevc_state->is_64lcu);

    mbenc_curbe->dw3.mode0_cost             = mode_cost[0];
    mbenc_curbe->dw3.mode1_cost             = mode_cost[1];
    mbenc_curbe->dw3.mode2_cost             = mode_cost[2];
    mbenc_curbe->dw3.mode3_cost             = mode_cost[3];

    mbenc_curbe->dw4.mode4_cost             = mode_cost[4];
    mbenc_curbe->dw4.mode5_cost             = mode_cost[5];
    mbenc_curbe->dw4.mode6_cost             = mode_cost[6];
    mbenc_curbe->dw4.mode7_cost             = mode_cost[7];

    mbenc_curbe->dw5.mode8_cost             = mode_cost[8];
    mbenc_curbe->dw5.mode9_cost             = mode_cost[9];
    mbenc_curbe->dw5.ref_id_cost             = mode_cost[10];
    mbenc_curbe->dw5.chroma_intra_mode_cost  = mode_cost[11];

    mbenc_curbe->dw6.log2_min_cu_size        = seq_param->log2_min_luma_coding_block_size_minus3 + 3;
    mbenc_curbe->dw6.log2_max_cu_size        = seq_param->log2_diff_max_min_luma_coding_block_size +
                                               seq_param->log2_min_luma_coding_block_size_minus3 + 3;
    mbenc_curbe->dw6.log2_max_tu_size        = seq_param->log2_diff_max_min_transform_block_size +
                                               seq_param->log2_min_transform_block_size_minus2 + 2;
    mbenc_curbe->dw6.log2_min_tu_size        = seq_param->log2_min_transform_block_size_minus2 + 2;
    if (seq_param->max_transform_hierarchy_depth_intra)
        mbenc_curbe->dw6.max_tr_depth_intra = gen10_hevc_tu_settings[GEN10_LOG2_TU_MAX_DEPTH_INTRA_TU_PARAM][tu_idx];
    else
        mbenc_curbe->dw6.max_tr_depth_intra = 0;

    mbenc_curbe->dw6.tu_split_flag          = 1;

    mbenc_curbe->dw7.concurrent_group_num   = 1;
    mbenc_curbe->dw7.slice_qp               = slice_qp;
    mbenc_curbe->dw7.enc_tu_decision_mode   = gen10_hevc_tu_settings[GEN10_ENC_TU_DECISION_MODE_TU_PARAM][tu_idx];

    mbenc_curbe->dw8.lambda_rd              = mode_cost[LAMBDA_RD_IDX];
    mbenc_curbe->dw9.lambda_md              = mode_cost[LAMBDA_MD_IDX];
    mbenc_curbe->dw10.intra_tusad_thr       = mode_cost[TUSAD_THR_IDX];

    mbenc_curbe->dw11.slice_type             = HEVC_SLICE_I;

    if (hevc_state->brc.brc_method == GEN10_HEVC_BRC_CQP)
        mbenc_curbe->dw11.qp_type           = GEN10_HEVC_QP_TYPE_CONSTANT;
    else
        mbenc_curbe->dw11.qp_type           = hevc_state->brc.lcu_brc_enabled ? GEN10_HEVC_QP_TYPE_CU_LEVEL : GEN10_HEVC_QP_TYPE_FRAME;

    mbenc_curbe->dw11.enc_qt_decision_mode  = gen10_hevc_tu_settings[GEN10_ENC_QT_DECISION_MODE_TU_PARAM][tu_idx];

    mbenc_curbe->dw12.pcm_8x8_sad_threshold = 4700;

    mbenc_curbe->dw16.bti_vme_intra_pred_surface           = GEN10_HEVC_MBENC_INTRA_VME_PRED_CURR_PIC_IDX0;
    mbenc_curbe->dw17.bti_curr_picture_y                   = GEN10_HEVC_MBENC_INTRA_CURR_Y;
    mbenc_curbe->dw18.bti_enc_curecord_surface             = GEN10_HEVC_MBENC_INTRA_INTERMEDIATE_CU_RECORD;
    mbenc_curbe->dw19.bti_pak_obj_cmd_surface              = GEN10_HEVC_MBENC_INTRA_PAK_OBJ0;
    mbenc_curbe->dw20.bti_cu_packet_for_pak_surface        = GEN10_HEVC_MBENC_INTRA_PAK_CU_RECORD;
    mbenc_curbe->dw21.bti_internal_scratch_surface         = GEN10_HEVC_MBENC_INTRA_SCRATCH_SURFACE;
    mbenc_curbe->dw22.bti_cu_based_qp_surface              = GEN10_HEVC_MBENC_INTRA_CU_QP_DATA;
    mbenc_curbe->dw23.bti_const_data_lut_surface           = GEN10_HEVC_MBENC_INTRA_CONST_DATA_LUT;
    mbenc_curbe->dw24.bti_lcu_level_data_input_surface     = GEN10_HEVC_MBENC_INTRA_LCU_LEVEL_DATA_INPUT;
    mbenc_curbe->dw25.bti_concurrent_tg_data_surface       = GEN10_HEVC_MBENC_INTRA_CONCURRENT_TG_DATA;
    mbenc_curbe->dw26.bti_brc_combined_enc_param_surface   = GEN10_HEVC_MBENC_INTRA_BRC_COMBINED_ENC_PARAMETER_SURFACE;
    mbenc_curbe->dw27.bti_cu_split_surface                 = GEN10_HEVC_MBENC_INTRA_CU_SPLIT_SURFACE,
                      mbenc_curbe->dw28.bti_debug_surface                    = GEN10_HEVC_MBENC_INTRA_DEBUG_DUMP;

    i965_gpe_context_unmap_curbe(gpe_context);
}

static int
gen10_hevc_compute_diff_poc(VADriverContextP ctx,
                            VAPictureHEVC *curr_pic,
                            VAPictureHEVC *ref_pic)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct object_surface *obj_surface = NULL;
    int diff_poc = 0;

    if (ref_pic->picture_id != VA_INVALID_SURFACE)
        obj_surface = SURFACE(ref_pic->picture_id);

    if (!obj_surface || (ref_pic->flags & VA_PICTURE_HEVC_INVALID))
        return diff_poc;

    diff_poc = curr_pic->pic_order_cnt - ref_pic->pic_order_cnt;

    if (diff_poc < -128)
        diff_poc = -128;
    else if (diff_poc > 127)
        diff_poc = 127;

    return diff_poc;
}

static void
gen10_hevc_enc_mbenc_inter_curbe(VADriverContextP ctx,
                                 struct encode_state *encode_state,
                                 struct intel_encoder_context *encoder_context,
                                 struct i965_gpe_context *gpe_context)
{
    struct gen10_hevc_enc_context *vme_context = encoder_context->vme_context;
    struct gen10_hevc_enc_state *hevc_state;
    gen10_hevc_mbenc_inter_curbe_data *mbenc_curbe;
    VAEncSliceParameterBufferHEVC *slice_param;
    VAEncPictureParameterBufferHEVC *pic_param;
    VAEncSequenceParameterBufferHEVC *seq_param;
    int slice_qp;
    int tu_idx;

    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;

    seq_param = (VAEncSequenceParameterBufferHEVC *)encode_state->seq_param_ext->buffer;
    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[0]->buffer;
    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;
    mbenc_curbe = i965_gpe_context_map_curbe(gpe_context);

    if (!mbenc_curbe)
        return;

    memset(mbenc_curbe, 0, sizeof(gen10_hevc_mbenc_inter_curbe_data));

    if (hevc_state->brc.target_usage < 3)
        tu_idx = 0;
    else if (hevc_state->brc.target_usage < 7)
        tu_idx = 1;
    else
        tu_idx = 2;

    slice_qp = slice_param->slice_qp_delta + pic_param->pic_init_qp;
    mbenc_curbe->dw0.frame_width_in_pixel  = hevc_state->frame_width;
    mbenc_curbe->dw0.frame_height_in_pixel = hevc_state->frame_height;

    mbenc_curbe->dw1.log2_min_cu_size        = seq_param->log2_min_luma_coding_block_size_minus3 + 3;
    mbenc_curbe->dw1.log2_max_cu_size        = seq_param->log2_diff_max_min_luma_coding_block_size +
                                               seq_param->log2_min_luma_coding_block_size_minus3 + 3;
    mbenc_curbe->dw1.log2_max_tu_size        = seq_param->log2_diff_max_min_transform_block_size +
                                               seq_param->log2_min_transform_block_size_minus2 + 2;
    mbenc_curbe->dw1.log2_min_tu_size        = seq_param->log2_min_transform_block_size_minus2 + 2;

    if (seq_param->max_transform_hierarchy_depth_intra)
        mbenc_curbe->dw1.max_tr_depth_intra = gen10_hevc_tu_settings[GEN10_LOG2_TU_MAX_DEPTH_INTRA_TU_PARAM][tu_idx];
    else
        mbenc_curbe->dw1.max_tr_depth_intra = 0;

    if (seq_param->max_transform_hierarchy_depth_inter)
        mbenc_curbe->dw1.max_tr_depth_inter = gen10_hevc_tu_settings[GEN10_LOG2_TU_MAX_DEPTH_INTER_TU_PARAM][tu_idx];
    else
        mbenc_curbe->dw1.max_tr_depth_inter = 0;
    mbenc_curbe->dw1.log2_para_merge_level = 2;
    mbenc_curbe->dw1.max_num_ime_search_center = 6;

    mbenc_curbe->dw2.hme_flag                  = hevc_state->hme_enabled ? 3 : 0;
    mbenc_curbe->dw2.super_hme_enable          = hevc_state->b16xme_enabled ? 1 : 0;
    mbenc_curbe->dw2.hme_coarse_stage          = 1;
    mbenc_curbe->dw2.hme_subpel_mode           = 3;
    if (hevc_state->brc.brc_method == GEN10_HEVC_BRC_CQP)
        mbenc_curbe->dw2.qp_type           = GEN10_HEVC_QP_TYPE_CONSTANT;
    else
        mbenc_curbe->dw2.qp_type           = hevc_state->brc.lcu_brc_enabled ? GEN10_HEVC_QP_TYPE_CU_LEVEL : GEN10_HEVC_QP_TYPE_FRAME;

    if (hevc_state->num_regions_in_slice > 1)
        mbenc_curbe->dw2.regions_in_slice_splits_enable = 1;
    else
        mbenc_curbe->dw2.regions_in_slice_splits_enable = 0;

    mbenc_curbe->dw3.active_num_child_threads_cu64   = 0;
    mbenc_curbe->dw3.active_num_child_threads_cu32_0 = 0;
    mbenc_curbe->dw3.active_num_child_threads_cu32_1 = 0;
    mbenc_curbe->dw3.active_num_child_threads_cu32_2 = 0;
    mbenc_curbe->dw3.active_num_child_threads_cu32_3 = 0;
    mbenc_curbe->dw3.slice_qp               = slice_qp;

    mbenc_curbe->dw4.skip_mode_enable       = 1;
    mbenc_curbe->dw4.adaptive_enable        = 1;
    mbenc_curbe->dw4.ime_ref_window_size    = 1;
    mbenc_curbe->dw4.hevc_min_cu_ctrl    = seq_param->log2_min_luma_coding_block_size_minus3;

    mbenc_curbe->dw5.subpel_mode            = 3;
    mbenc_curbe->dw5.inter_sad_measure            = 2;
    mbenc_curbe->dw5.intra_sad_measure            = 2;
    mbenc_curbe->dw5.len_sp            = 63;
    mbenc_curbe->dw5.max_num_su        = 63;
    mbenc_curbe->dw5.refid_cost_mode        = 1;

    mbenc_curbe->dw7.max_num_merge_cand     = slice_param->max_num_merge_cand;
    mbenc_curbe->dw7.slice_type     = slice_param->slice_type;
    mbenc_curbe->dw7.temporal_mvp_enable     = seq_param->seq_fields.bits.sps_temporal_mvp_enabled_flag;
    mbenc_curbe->dw7.mvp_collocated_from_l0  = slice_param->slice_fields.bits.collocated_from_l0_flag;
    mbenc_curbe->dw7.same_ref_list           = hevc_state->is_same_ref_list;
    if (slice_param->slice_type == HEVC_SLICE_B)
        mbenc_curbe->dw7.is_low_delay            = hevc_state->low_delay;
    else
        mbenc_curbe->dw7.is_low_delay            = 1;

    mbenc_curbe->dw7.num_ref_idx_l0          = slice_param->num_ref_idx_l0_active_minus1 + 1;
    if (slice_param->slice_type == HEVC_SLICE_B)
        mbenc_curbe->dw7.num_ref_idx_l1          = slice_param->num_ref_idx_l1_active_minus1 + 1;
    else
        mbenc_curbe->dw7.num_ref_idx_l1          = 0;

    mbenc_curbe->dw8.fwd_poc_num_l0_mtb_0   = gen10_hevc_compute_diff_poc(ctx, &pic_param->decoded_curr_pic,
                                                                          &slice_param->ref_pic_list0[0]);
    mbenc_curbe->dw8.fwd_poc_num_l0_mtb_1   = gen10_hevc_compute_diff_poc(ctx, &pic_param->decoded_curr_pic,
                                                                          &slice_param->ref_pic_list0[1]);
    mbenc_curbe->dw9.fwd_poc_num_l0_mtb_2   = gen10_hevc_compute_diff_poc(ctx, &pic_param->decoded_curr_pic,
                                                                          &slice_param->ref_pic_list0[2]);
    mbenc_curbe->dw9.fwd_poc_num_l0_mtb_3   = gen10_hevc_compute_diff_poc(ctx, &pic_param->decoded_curr_pic,
                                                                          &slice_param->ref_pic_list0[3]);
    if (slice_param->slice_type == HEVC_SLICE_B) {
        mbenc_curbe->dw8.bwd_poc_num_l1_mtb_0   = gen10_hevc_compute_diff_poc(ctx, &pic_param->decoded_curr_pic,
                                                                              &slice_param->ref_pic_list1[0]);
        mbenc_curbe->dw8.bwd_poc_num_l1_mtb_1   = gen10_hevc_compute_diff_poc(ctx, &pic_param->decoded_curr_pic,
                                                                              &slice_param->ref_pic_list1[1]);
        mbenc_curbe->dw9.bwd_poc_num_l1_mtb_2   = gen10_hevc_compute_diff_poc(ctx, &pic_param->decoded_curr_pic,
                                                                              &slice_param->ref_pic_list1[2]);
        mbenc_curbe->dw9.bwd_poc_num_l1_mtb_3   = gen10_hevc_compute_diff_poc(ctx, &pic_param->decoded_curr_pic,
                                                                              &slice_param->ref_pic_list1[3]);
    }

    mbenc_curbe->dw13.ref_frame_hor_size = hevc_state->frame_width;
    mbenc_curbe->dw13.ref_frame_ver_size = hevc_state->frame_height;

    mbenc_curbe->dw15.concurrent_gop_num = hevc_state->hevc_wf_param.num_regions;
    mbenc_curbe->dw15.total_thread_num_per_lcu = gen10_hevc_tu_settings[GEN10_TOTAL_THREAD_NUM_PER_LCU_TU_PARAM][tu_idx];
    mbenc_curbe->dw15.regions_in_slice_split_count = hevc_state->num_regions_in_slice;

    mbenc_curbe->dw1.max_num_ime_search_center               = gen10_hevc_tu_settings[GEN10_MAX_NUM_IME_SEARCH_CENTER_TU_PARAM][tu_idx];

    if (hevc_state->is_64lcu)
        mbenc_curbe->dw2.enable_cu64_check       = gen10_hevc_tu_settings[GEN10_ENABLE_CU64_CHECK_TU_PARAM][tu_idx];
    else
        mbenc_curbe->dw2.enable_cu64_check       = 0;

    mbenc_curbe->dw2.enc_trans_simplify          = gen10_hevc_tu_settings[GEN10_ENC_TRANSFORM_SIMPLIFY_TU_PARAM][tu_idx];
    mbenc_curbe->dw2.enc_tu_dec_mode             = gen10_hevc_tu_settings[GEN10_ENC_TU_DECISION_MODE_TU_PARAM][tu_idx];
    mbenc_curbe->dw2.enc_tu_dec_for_all_qt       = gen10_hevc_tu_settings[GEN10_ENC_TU_DECISION_FOR_ALL_QT_TU_PARAM][tu_idx];
    mbenc_curbe->dw2.coef_bit_est_mode           = gen10_hevc_tu_settings[GEN10_COEF_BIT_EST_MODE_TU_PARAM][tu_idx];
    mbenc_curbe->dw2.enc_skip_dec_mode           = gen10_hevc_tu_settings[GEN10_ENC_SKIP_DECISION_MODE_TU_PARAM][tu_idx];
    mbenc_curbe->dw2.enc_qt_dec_mode             = gen10_hevc_tu_settings[GEN10_ENC_QT_DECISION_MODE_TU_PARAM][tu_idx];
    mbenc_curbe->dw2.lcu32_enc_rd_dec_mode_for_all_qt   = gen10_hevc_tu_settings[GEN10_ENC_RD_DECISION_MODE_FOR_ALL_QT_TU_PARAM][tu_idx];
    mbenc_curbe->dw2.lcu64_cu64_skip_check_only  = (tu_idx == 1);
    mbenc_curbe->dw2.sic_dys_run_path_mode       = gen10_hevc_tu_settings[GEN10_SIC_DYNAMIC_RUN_PATH_MODE][tu_idx];

    if (hevc_state->is_64lcu) {
        mbenc_curbe->dw16.bti_curr_picture_y          =
            GEN10_HEVC_MBENC_INTER_LCU64_CURR_Y;
        mbenc_curbe->dw17.bti_enc_curecord_surface    =
            GEN10_HEVC_MBENC_INTER_LCU64_CU32_ENC_CU_RECORD;
        mbenc_curbe->dw18.bti_lcu64_enc_curecord2_surface  =
            GEN10_HEVC_MBENC_INTER_LCU64_SECOND_CU32_ENC_CU_RECORD;
        mbenc_curbe->dw19.bti_lcu64_pak_objcmd_surface =
            GEN10_HEVC_MBENC_INTER_LCU64_PAK_OBJ0;
        mbenc_curbe->dw20.bti_lcu64_pak_curecord_surface  =
            GEN10_HEVC_MBENC_INTER_LCU64_PAK_CU_RECORD;
        mbenc_curbe->dw21.bti_lcu64_vme_intra_inter_pred_surface =
            GEN10_HEVC_MBENC_INTER_LCU64_VME_PRED_CURR_PIC_IDX0;
        mbenc_curbe->dw22.bti_lcu64_cu16_qpdata_input_surface =
            GEN10_HEVC_MBENC_INTER_LCU64_CU16x16_QP_DATA;
        mbenc_curbe->dw23.bti_lcu64_cu32_enc_const_table_surface =
            GEN10_HEVC_MBENC_INTER_LCU64_CU32_ENC_CONST_TABLE;
        mbenc_curbe->dw24.bti_lcu64_colocated_mvdata_surface =
            GEN10_HEVC_MBENC_INTER_LCU64_COLOCATED_CU_MV_DATA;
        mbenc_curbe->dw25.bti_lcu64_hme_pred_surface         =
            GEN10_HEVC_MBENC_INTER_LCU64_HME_MOTION_PREDICTOR_DATA;
        mbenc_curbe->dw26.bti_lcu64_lculevel_data_input_surface      =
            GEN10_HEVC_MBENC_INTER_LCU64_LCU_LEVEL_DATA_INPUT;
        mbenc_curbe->dw27.bti_lcu64_cu32_enc_scratch_surface  =
            GEN10_HEVC_MBENC_INTER_LCU64_CU32_LCU_ENC_SCRATCH_SURFACE;
        mbenc_curbe->dw28.bti_lcu64_64x64_dist_surface        =
            GEN10_HEVC_MBENC_INTER_LCU64_64X64_DISTORTION_SURFACE;
        mbenc_curbe->dw29.bti_lcu64_concurrent_tg_data_surface =
            GEN10_HEVC_MBENC_INTER_LCU64_CONCURRENT_TG_DATA;
        mbenc_curbe->dw30.bti_lcu64_brc_combined_enc_param_surface  =
            GEN10_HEVC_MBENC_INTER_LCU64_BRC_COMBINED_ENC_PARAMETER_SURFACE;
        mbenc_curbe->dw31.bti_lcu64_cu32_jbq1d_buf_surface  =
            GEN10_HEVC_MBENC_INTER_LCU64_CU32_JOB_QUEUE_1D_SURFACE;
        mbenc_curbe->dw32.bti_lcu64_cu32_jbq2d_buf_surface  =
            GEN10_HEVC_MBENC_INTER_LCU64_CU32_JOB_QUEUE_2D_SURFACE;
        mbenc_curbe->dw33.bti_lcu64_cu32_residual_scratch_surface  =
            GEN10_HEVC_MBENC_INTER_LCU64_CU32_RESIDUAL_DATA_SCRATCH_SURFACE;
        mbenc_curbe->dw34.bti_lcu64_cusplit_surface =
            GEN10_HEVC_MBENC_INTER_LCU64_CU_SPLIT_DATA_SURFACE;
        mbenc_curbe->dw35.bti_lcu64_curr_picture_y_2xds   =
            GEN10_HEVC_MBENC_INTER_LCU64_CURR_Y_2xDS;
        mbenc_curbe->dw36.bti_lcu64_intermediate_curecord_surface =
            GEN10_HEVC_MBENC_INTER_LCU64_INTERMEDIATE_CU_RECORD;
        mbenc_curbe->dw37.bti_lcu64_const_data_lut_surface        =
            GEN10_HEVC_MBENC_INTER_LCU64_CONST64_DATA_LUT;
        mbenc_curbe->dw38.bti_lcu64_lcu_storage_surface           =
            GEN10_HEVC_MBENC_INTER_LCU64_LCU_STORAGE_SURFACE;
        mbenc_curbe->dw39.bti_lcu64_vme_inter_pred_2xds_surface   =
            GEN10_HEVC_MBENC_INTER_LCU64_VME_PRED_CURR_PIC_2xDS_IDX0;
        mbenc_curbe->dw40.bti_lcu64_cu64_jbq1d_surface            =
            GEN10_HEVC_MBENC_INTER_LCU64_JOB_QUEUE_1D_SURFACE;
        mbenc_curbe->dw41.bti_lcu64_cu64_jbq2d_surface            =
            GEN10_HEVC_MBENC_INTER_LCU64_JOB_QUEUE_2D_SURFACE;
        mbenc_curbe->dw42.bti_lcu64_cu64_residual_scratch_surface =
            GEN10_HEVC_MBENC_INTER_LCU64_RESIDUAL_DATA_SCRATCH_SURFACE;
        mbenc_curbe->dw43.bti_lcu64_debug_surface                 =
            GEN10_HEVC_MBENC_INTER_LCU64_DEBUG_SURFACE;
    } else {
        mbenc_curbe->dw16.bti_curr_picture_y                        =
            GEN10_HEVC_MBENC_INTER_LCU32_CURR_Y;
        mbenc_curbe->dw17.bti_enc_curecord_surface                  =
            GEN10_HEVC_MBENC_INTER_LCU32_ENC_CU_RECORD;
        mbenc_curbe->dw18.bti_lcu32_pak_objcmd_surface              =
            GEN10_HEVC_MBENC_INTER_LCU32_PAK_OBJ0;
        mbenc_curbe->dw19.bti_lcu32_pak_curecord_surface            =
            GEN10_HEVC_MBENC_INTER_LCU32_PAK_CU_RECORD;
        mbenc_curbe->dw20.bti_lcu32_vme_intra_inter_pred_surface    =
            GEN10_HEVC_MBENC_INTER_LCU32_VME_PRED_CURR_PIC_IDX0;
        mbenc_curbe->dw21.bti_lcu32_cu16_qpdata_input_surface   =
            GEN10_HEVC_MBENC_INTER_LCU32_CU16x16_QP_DATA;
        mbenc_curbe->dw22.bti_lcu32_enc_const_table_surface =
            GEN10_HEVC_MBENC_INTER_LCU32_ENC_CONST_TABLE;
        mbenc_curbe->dw23.bti_lcu32_colocated_mvdata_surface =
            GEN10_HEVC_MBENC_INTER_LCU32_COLOCATED_CU_MV_DATA;
        mbenc_curbe->dw24.bti_lcu32_hme_pred_data_surface    =
            GEN10_HEVC_MBENC_INTER_LCU32_HME_MOTION_PREDICTOR_DATA;
        mbenc_curbe->dw25.bti_lcu32_lculevel_data_input_surface   =
            GEN10_HEVC_MBENC_INTER_LCU32_LCU_LEVEL_DATA_INPUT;
        mbenc_curbe->dw26.bti_lcu32_enc_scratch_surface =
            GEN10_HEVC_MBENC_INTER_LCU32_LCU_ENC_SCRATCH_SURFACE;
        mbenc_curbe->dw27.bti_lcu32_concurrent_tg_data_surface =
            GEN10_HEVC_MBENC_INTER_LCU32_CONCURRENT_TG_DATA;
        mbenc_curbe->dw28.bti_lcu32_brc_combined_enc_param_surface  =
            GEN10_HEVC_MBENC_INTER_LCU32_BRC_COMBINED_ENC_PARAMETER_SURFACE;
        mbenc_curbe->dw29.bti_lcu32_jbq_scratch_surface  =
            GEN10_HEVC_MBENC_INTER_LCU32_JOB_QUEUE_SCRATCH_SURFACE;
        mbenc_curbe->dw30.bti_lcu32_cusplit_data_surface     =
            GEN10_HEVC_MBENC_INTER_LCU32_CU_SPLIT_DATA_SURFACE,
            mbenc_curbe->dw31.bti_lcu32_residual_scratch_surface =
                GEN10_HEVC_MBENC_INTER_LCU32_RESIDUAL_DATA_SCRATCH_SURFACE,
                mbenc_curbe->dw32.bti_lcu32_debug_surface =
                    GEN10_HEVC_MBENC_INTER_LCU32_DEBUG_SURFACE;
    }

    i965_gpe_context_unmap_curbe(gpe_context);
}

static void
gen10_hevc_enc_mbenc_intra_surfaces(VADriverContextP ctx,
                                    struct encode_state *encode_state,
                                    struct intel_encoder_context *encoder_context,
                                    struct i965_gpe_context *gpe_context)
{
    struct gen10_hevc_enc_context *vme_context = encoder_context->vme_context;
    struct gen10_hevc_enc_state *hevc_state;
    struct object_surface *obj_surface;
    struct object_surface *vme_surface;
    struct gen10_hevc_surface_priv *surface_priv;
    int input_bti, i;

    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;

    obj_surface = encode_state->reconstructed_object;

    surface_priv = (struct gen10_hevc_surface_priv *)(obj_surface->private_data);

    if (hevc_state->is_10bit)
        vme_surface = surface_priv->converted_surface;
    else
        vme_surface = encode_state->input_yuv_object;

    input_bti = GEN10_HEVC_MBENC_INTRA_VME_PRED_CURR_PIC_IDX0;
    i965_add_adv_gpe_surface(ctx, gpe_context,
                             vme_surface,
                             input_bti);
    input_bti++;

    for (i = 0; i < 8; i++) {
        i965_add_adv_gpe_surface(ctx, gpe_context,
                                 vme_surface,
                                 input_bti);
        input_bti++;
    }

    input_bti = GEN10_HEVC_MBENC_INTRA_CURR_Y;

    i965_add_2d_gpe_surface(ctx,
                            gpe_context,
                            vme_surface,
                            0,
                            1,
                            I965_SURFACEFORMAT_R8_UNORM,
                            input_bti);
    i965_add_2d_gpe_surface(ctx,
                            gpe_context,
                            vme_surface,
                            1,
                            1,
                            I965_SURFACEFORMAT_R16_UINT,
                            input_bti + 1);


    input_bti = GEN10_HEVC_MBENC_INTRA_INTERMEDIATE_CU_RECORD;
    i965_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                   &vme_context->res_temp_curecord_lcu32_surface,
                                   1, I965_SURFACEFORMAT_R8_UNORM,
                                   input_bti);

    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_mb_code_surface,
                                0,
                                BYTES2UINT32(hevc_state->cu_records_offset),
                                0,
                                input_bti + 1);


    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_mb_code_surface,
                                0,
                                BYTES2UINT32(vme_context->res_mb_code_surface.size - hevc_state->cu_records_offset),
                                hevc_state->cu_records_offset,
                                input_bti + 2);

    input_bti = GEN10_HEVC_MBENC_INTRA_SCRATCH_SURFACE;
    i965_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                   &vme_context->res_scratch_surface,
                                   1, I965_SURFACEFORMAT_R8_UNORM,
                                   input_bti);

    i965_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                   &vme_context->res_16x16_qp_data_surface,
                                   1, I965_SURFACEFORMAT_R8_UNORM,
                                   input_bti + 1);

    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_enc_const_table_intra,
                                0,
                                BYTES2UINT32(vme_context->res_enc_const_table_intra.size),
                                0,
                                input_bti + 2);

    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_lculevel_input_data_buffer,
                                0,
                                BYTES2UINT32(vme_context->res_lculevel_input_data_buffer.size),
                                0,
                                input_bti + 3);

    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_concurrent_tg_data,
                                0,
                                BYTES2UINT32(vme_context->res_concurrent_tg_data.size),
                                0,
                                input_bti + 4);

    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_brc_input_enc_kernel_buffer,
                                0,
                                BYTES2UINT32(vme_context->res_brc_input_enc_kernel_buffer.size),
                                0,
                                input_bti + 5);

    i965_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                   &vme_context->res_cu_split_surface,
                                   1, I965_SURFACEFORMAT_R8_UNORM,
                                   input_bti + 6);

    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_kernel_trace_data,
                                0,
                                BYTES2UINT32(vme_context->res_kernel_trace_data.size),
                                0,
                                input_bti + 7);
}

static void
gen10_hevc_enc_mbenc_inter_lcu32_surfaces(VADriverContextP ctx,
                                          struct encode_state *encode_state,
                                          struct intel_encoder_context *encoder_context,
                                          struct i965_gpe_context *gpe_context)
{
    struct gen10_hevc_enc_context *vme_context = encoder_context->vme_context;
    struct gen10_hevc_enc_state *hevc_state;
    struct gen10_hevc_enc_frame_info *frame_info;
    struct gen10_hevc_enc_common_res *common_res;
    VAEncSliceParameterBufferHEVC *slice_param;
    VAEncPictureParameterBufferHEVC *pic_param;
    struct object_surface *obj_surface, *vme_surface;
    struct gen10_hevc_surface_priv *surface_priv;
    struct object_surface *l0_surface = NULL, *l1_surface = NULL, *tmp_surface;
    int input_bti, i;

    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;
    frame_info = &vme_context->frame_info;
    common_res = &vme_context->common_res;

    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;
    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[0]->buffer;

    obj_surface = encode_state->reconstructed_object;

    surface_priv = (struct gen10_hevc_surface_priv *)(obj_surface->private_data);

    if (hevc_state->is_10bit)
        vme_surface = surface_priv->converted_surface;
    else
        vme_surface = encode_state->input_yuv_object;

    input_bti = GEN10_HEVC_MBENC_INTER_LCU32_CURR_Y;
    i965_add_2d_gpe_surface(ctx,
                            gpe_context,
                            vme_surface,
                            0,
                            1,
                            I965_SURFACEFORMAT_R8_UNORM,
                            input_bti);
    i965_add_2d_gpe_surface(ctx,
                            gpe_context,
                            vme_surface,
                            1,
                            1,
                            I965_SURFACEFORMAT_R16_UINT,
                            input_bti + 1);

    input_bti = GEN10_HEVC_MBENC_INTER_LCU32_ENC_CU_RECORD;
    i965_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                   &vme_context->res_temp_curecord_lcu32_surface,
                                   1, I965_SURFACEFORMAT_R8_UNORM,
                                   input_bti);

    input_bti = GEN10_HEVC_MBENC_INTER_LCU32_PAK_OBJ0;
    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_mb_code_surface,
                                0,
                                BYTES2UINT32(hevc_state->cu_records_offset),
                                0,
                                input_bti);
    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_mb_code_surface,
                                0,
                                BYTES2UINT32(vme_context->res_mb_code_surface.size -
                                             hevc_state->cu_records_offset),
                                hevc_state->cu_records_offset,
                                input_bti + 1);

    input_bti = GEN10_HEVC_MBENC_INTER_LCU32_VME_PRED_CURR_PIC_IDX0;

    i965_add_adv_gpe_surface(ctx, gpe_context,
                             vme_surface,
                             input_bti);

    if (frame_info->mapped_ref_idx_list0[0] >= 0)
        l0_surface = common_res->reference_pics[frame_info->mapped_ref_idx_list0[0]].obj_surface;
    else
        l0_surface = NULL;

    if (!l0_surface || !l0_surface->private_data)
        l0_surface = vme_surface;
    else {
        surface_priv = (struct gen10_hevc_surface_priv *)(l0_surface->private_data);
        if (hevc_state->is_10bit)
            l0_surface = surface_priv->converted_surface;
    }

    if (slice_param->slice_type == HEVC_SLICE_B) {
        if (frame_info->mapped_ref_idx_list1[0] > 0)
            l1_surface = common_res->reference_pics[frame_info->mapped_ref_idx_list1[0]].obj_surface;
        else
            l1_surface = NULL;

        if (!l1_surface || !l1_surface->private_data)
            l1_surface = l0_surface;
        else {
            surface_priv = (struct gen10_hevc_surface_priv *)(l1_surface->private_data);
            if (hevc_state->is_10bit)
                l1_surface = surface_priv->converted_surface;
        }
    }

    input_bti = GEN10_HEVC_MBENC_INTER_LCU32_VME_PRED_FWD_PIC_IDX0;
    for (i = 0; i < 4; i++) {
        if (frame_info->mapped_ref_idx_list0[i] >= 0)
            tmp_surface = common_res->reference_pics[frame_info->mapped_ref_idx_list0[i]].obj_surface;
        else
            tmp_surface = NULL;

        if (tmp_surface && tmp_surface->private_data) {
            surface_priv = (struct gen10_hevc_surface_priv *)(tmp_surface->private_data);
            if (hevc_state->is_10bit)
                tmp_surface = surface_priv->converted_surface;

            i965_add_adv_gpe_surface(ctx, gpe_context,
                                     tmp_surface,
                                     input_bti + 2 * i);
        } else
            i965_add_adv_gpe_surface(ctx, gpe_context,
                                     l0_surface,
                                     input_bti + 2 * i);

        if (slice_param->slice_type == HEVC_SLICE_B) {
            if (frame_info->mapped_ref_idx_list1[i] >= 0)
                tmp_surface = common_res->reference_pics[frame_info->mapped_ref_idx_list1[0]].obj_surface;
            else
                tmp_surface = NULL;

            if (tmp_surface && tmp_surface->private_data) {
                surface_priv = (struct gen10_hevc_surface_priv *)(tmp_surface->private_data);
                if (hevc_state->is_10bit)
                    tmp_surface = surface_priv->converted_surface;

                i965_add_adv_gpe_surface(ctx, gpe_context,
                                         tmp_surface,
                                         input_bti + 2 * i + 1);
            } else
                i965_add_adv_gpe_surface(ctx, gpe_context,
                                         l1_surface,
                                         input_bti + 2 * i + 1);
        }
    }

    input_bti = GEN10_HEVC_MBENC_INTER_LCU32_CU16x16_QP_DATA;
    i965_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                   &vme_context->res_16x16_qp_data_surface,
                                   1, I965_SURFACEFORMAT_R8_UNORM,
                                   input_bti);

    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_enc_const_table_inter,
                                0,
                                BYTES2UINT32(vme_context->res_enc_const_table_inter.size),
                                0,
                                input_bti + 1);

    if (slice_param->slice_fields.bits.slice_temporal_mvp_enabled_flag &&
        (pic_param->collocated_ref_pic_index != 0xFF)) {
        obj_surface = common_res->reference_pics[pic_param->collocated_ref_pic_index].obj_surface;
        if (obj_surface && obj_surface->private_data) {
            surface_priv = (struct gen10_hevc_surface_priv *)(obj_surface->private_data);

            i965_add_buffer_gpe_surface(ctx, gpe_context,
                                        &surface_priv->motion_vector_temporal,
                                        0,
                                        BYTES2UINT32(surface_priv->motion_vector_temporal.size),
                                        0,
                                        input_bti + 2);
        }
    }

    input_bti = GEN10_HEVC_MBENC_INTER_LCU32_HME_MOTION_PREDICTOR_DATA;
    if (hevc_state->hme_enabled) {
        i965_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                       &vme_context->res_s4x_memv_data_surface,
                                       1, I965_SURFACEFORMAT_R8_UNORM,
                                       input_bti);
    }

    input_bti = GEN10_HEVC_MBENC_INTER_LCU32_LCU_LEVEL_DATA_INPUT;
    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_lculevel_input_data_buffer,
                                0,
                                BYTES2UINT32(vme_context->res_lculevel_input_data_buffer.size),
                                0,
                                input_bti);

    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_enc_scratch_buffer,
                                0,
                                BYTES2UINT32(vme_context->res_enc_scratch_buffer.size),
                                0,
                                input_bti + 1);


    input_bti = GEN10_HEVC_MBENC_INTER_LCU32_CONCURRENT_TG_DATA;
    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_concurrent_tg_data,
                                0,
                                BYTES2UINT32(vme_context->res_concurrent_tg_data.size),
                                0,
                                input_bti);

    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_brc_input_enc_kernel_buffer,
                                0,
                                BYTES2UINT32(vme_context->res_brc_input_enc_kernel_buffer.size),
                                0,
                                input_bti + 1);

    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_jbq_header_buffer,
                                1,
                                vme_context->res_jbq_header_buffer.size,
                                0,
                                input_bti + 2);

    input_bti = GEN10_HEVC_MBENC_INTER_LCU32_CU_SPLIT_DATA_SURFACE;
    i965_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                   &vme_context->res_cu_split_surface,
                                   1, I965_SURFACEFORMAT_R8_UNORM,
                                   input_bti);

    i965_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                   &vme_context->res_residual_scratch_lcu32_surface,
                                   1, I965_SURFACEFORMAT_R8_UNORM,
                                   input_bti + 1);

    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_kernel_trace_data,
                                0,
                                BYTES2UINT32(vme_context->res_kernel_trace_data.size),
                                0,
                                input_bti + 2);
}

static void
gen10_hevc_enc_mbenc_inter_lcu64_surfaces(VADriverContextP ctx,
                                          struct encode_state *encode_state,
                                          struct intel_encoder_context *encoder_context,
                                          struct i965_gpe_context *gpe_context)
{
    struct gen10_hevc_enc_context *vme_context = encoder_context->vme_context;
    struct gen10_hevc_enc_state *hevc_state;
    struct gen10_hevc_enc_common_res *common_res;
    struct gen10_hevc_enc_frame_info *frame_info;
    struct object_surface *obj_surface, *vme_surface;
    struct gen10_hevc_surface_priv *surface_priv;
    struct object_surface *l0_surface, *l1_surface, *tmp_surface;
    VAEncSliceParameterBufferHEVC *slice_param;
    VAEncPictureParameterBufferHEVC *pic_param;
    int input_bti, i;

    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;
    frame_info = &vme_context->frame_info;
    common_res = &vme_context->common_res;

    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;
    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[0]->buffer;

    obj_surface = encode_state->reconstructed_object;

    surface_priv = (struct gen10_hevc_surface_priv *)(obj_surface->private_data);

    if (hevc_state->is_10bit)
        vme_surface = surface_priv->converted_surface;
    else
        vme_surface = encode_state->input_yuv_object;

    input_bti = GEN10_HEVC_MBENC_INTER_LCU64_CURR_Y;
    i965_add_2d_gpe_surface(ctx,
                            gpe_context,
                            vme_surface,
                            0,
                            1,
                            I965_SURFACEFORMAT_R8_UNORM,
                            input_bti);
    i965_add_2d_gpe_surface(ctx,
                            gpe_context,
                            vme_surface,
                            1,
                            1,
                            I965_SURFACEFORMAT_R16_UINT,
                            input_bti + 1);

    input_bti = GEN10_HEVC_MBENC_INTER_LCU64_CU32_ENC_CU_RECORD;
    i965_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                   &vme_context->res_temp_curecord_lcu32_surface,
                                   1, I965_SURFACEFORMAT_R8_UNORM,
                                   input_bti);

    i965_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                   &vme_context->res_temp2_curecord_lcu32_surface,
                                   1, I965_SURFACEFORMAT_R8_UNORM,
                                   input_bti + 1);

    input_bti = GEN10_HEVC_MBENC_INTER_LCU64_PAK_OBJ0;
    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_mb_code_surface,
                                1,
                                hevc_state->cu_records_offset,
                                0,
                                input_bti);
    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_mb_code_surface,
                                0,
                                vme_context->res_mb_code_surface.size,
                                hevc_state->cu_records_offset,
                                input_bti + 1);

    input_bti = GEN10_HEVC_MBENC_INTER_LCU64_VME_PRED_CURR_PIC_IDX0;

    i965_add_adv_gpe_surface(ctx, gpe_context,
                             vme_surface,
                             input_bti);

    if (frame_info->mapped_ref_idx_list0[0] >= 0)
        l0_surface = common_res->reference_pics[frame_info->mapped_ref_idx_list0[0]].obj_surface;
    else
        l0_surface = NULL;

    if (!l0_surface || !l0_surface->private_data)
        l0_surface = vme_surface;
    else {
        surface_priv = (struct gen10_hevc_surface_priv *)(l0_surface->private_data);
        if (hevc_state->is_10bit)
            l0_surface = surface_priv->converted_surface;
    }

    l1_surface = l0_surface;
    if (slice_param->slice_type == HEVC_SLICE_B) {
        if (frame_info->mapped_ref_idx_list1[0] > 0)
            l1_surface = common_res->reference_pics[frame_info->mapped_ref_idx_list1[0]].obj_surface;
        else
            l1_surface = NULL;

        if (!l1_surface || !l1_surface->private_data)
            l1_surface = l0_surface;
        else {
            surface_priv = (struct gen10_hevc_surface_priv *)(l1_surface->private_data);
            if (hevc_state->is_10bit)
                l1_surface = surface_priv->converted_surface;
        }
    }

    input_bti = GEN10_HEVC_MBENC_INTER_LCU64_VME_PRED_FWD_PIC_IDX0;
    for (i = 0; i < 4; i++) {
        if (frame_info->mapped_ref_idx_list0[i] >= 0)
            tmp_surface = common_res->reference_pics[frame_info->mapped_ref_idx_list0[i]].obj_surface;
        else
            tmp_surface = NULL;

        if (tmp_surface && tmp_surface->private_data) {
            surface_priv = (struct gen10_hevc_surface_priv *)(tmp_surface->private_data);
            if (hevc_state->is_10bit)
                tmp_surface = surface_priv->converted_surface;

            i965_add_adv_gpe_surface(ctx, gpe_context,
                                     tmp_surface,
                                     input_bti + 2 * i);
        } else
            i965_add_adv_gpe_surface(ctx, gpe_context,
                                     l0_surface,
                                     input_bti + 2 * i);

        if (slice_param->slice_type == HEVC_SLICE_B) {
            if (frame_info->mapped_ref_idx_list1[i] >= 0)
                tmp_surface = common_res->reference_pics[frame_info->mapped_ref_idx_list1[i]].obj_surface;
            else
                tmp_surface = NULL;

            if (tmp_surface && tmp_surface->private_data) {
                surface_priv = (struct gen10_hevc_surface_priv *)(tmp_surface->private_data);
                if (hevc_state->is_10bit)
                    tmp_surface = surface_priv->converted_surface;

                i965_add_adv_gpe_surface(ctx, gpe_context,
                                         tmp_surface,
                                         input_bti + 2 * i + 1);
            } else
                i965_add_adv_gpe_surface(ctx, gpe_context,
                                         l1_surface,
                                         input_bti + 2 * i + 1);
        }
    }

    input_bti = GEN10_HEVC_MBENC_INTER_LCU64_CU16x16_QP_DATA;
    i965_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                   &vme_context->res_16x16_qp_data_surface,
                                   1, I965_SURFACEFORMAT_R8_UNORM,
                                   input_bti);

    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_enc_const_table_inter,
                                0,
                                vme_context->res_enc_const_table_inter.size,
                                0,
                                input_bti + 1);

    if (slice_param->slice_fields.bits.slice_temporal_mvp_enabled_flag &&
        (pic_param->collocated_ref_pic_index != 0xFF)) {
        obj_surface = common_res->reference_pics[pic_param->collocated_ref_pic_index].obj_surface;
        if (obj_surface && obj_surface->private_data) {
            surface_priv = (struct gen10_hevc_surface_priv *)(obj_surface->private_data);

            i965_add_buffer_gpe_surface(ctx, gpe_context,
                                        &surface_priv->motion_vector_temporal,
                                        0,
                                        surface_priv->motion_vector_temporal.size,
                                        0,
                                        input_bti + 2);
        }
    }

    input_bti = GEN10_HEVC_MBENC_INTER_LCU64_HME_MOTION_PREDICTOR_DATA;
    if (hevc_state->hme_enabled) {
        i965_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                       &vme_context->res_s4x_memv_data_surface,
                                       1, I965_SURFACEFORMAT_R8_UNORM,
                                       input_bti);
    }

    input_bti = GEN10_HEVC_MBENC_INTER_LCU64_LCU_LEVEL_DATA_INPUT;
    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_lculevel_input_data_buffer,
                                0,
                                vme_context->res_lculevel_input_data_buffer.size,
                                0,
                                input_bti);

    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_enc_scratch_buffer,
                                0,
                                vme_context->res_enc_scratch_buffer.size,
                                0,
                                input_bti + 1);

    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_64x64_dist_buffer,
                                1,
                                vme_context->res_64x64_dist_buffer.size,
                                0,
                                input_bti + 2);



    input_bti = GEN10_HEVC_MBENC_INTER_LCU64_CONCURRENT_TG_DATA;
    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_concurrent_tg_data,
                                0,
                                vme_context->res_concurrent_tg_data.size,
                                0,
                                input_bti);

    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_brc_input_enc_kernel_buffer,
                                0,
                                vme_context->res_brc_input_enc_kernel_buffer.size,
                                0,
                                input_bti + 1);


    input_bti = GEN10_HEVC_MBENC_INTER_LCU64_CU32_JOB_QUEUE_1D_SURFACE;
    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_jbq_header_buffer,
                                1,
                                vme_context->res_jbq_header_buffer.size,
                                0,
                                input_bti);

    i965_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                   &vme_context->res_jbq_data_lcu32_surface,
                                   1, I965_SURFACEFORMAT_R8_UNORM,
                                   input_bti + 1);

    i965_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                   &vme_context->res_residual_scratch_lcu32_surface,
                                   1, I965_SURFACEFORMAT_R8_UNORM,
                                   input_bti + 2);


    input_bti = GEN10_HEVC_MBENC_INTER_LCU64_CU_SPLIT_DATA_SURFACE;
    i965_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                   &vme_context->res_cu_split_surface,
                                   1, I965_SURFACEFORMAT_R8_UNORM,
                                   input_bti);

    input_bti = GEN10_HEVC_MBENC_INTER_LCU64_CURR_Y_2xDS;
    obj_surface = encode_state->reconstructed_object;
    surface_priv = (struct gen10_hevc_surface_priv *)(obj_surface->private_data);
    vme_surface = surface_priv->scaled_2x_surface;

    i965_add_2d_gpe_surface(ctx,
                            gpe_context,
                            vme_surface,
                            0,
                            1,
                            I965_SURFACEFORMAT_R8_UNORM,
                            input_bti);

    input_bti = GEN10_HEVC_MBENC_INTER_LCU64_INTERMEDIATE_CU_RECORD;
    i965_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                   &vme_context->res_temp_curecord_surface_lcu64,
                                   1, I965_SURFACEFORMAT_R8_UNORM,
                                   input_bti);

    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_enc_const_table_inter_lcu64,
                                1,
                                vme_context->res_enc_const_table_inter_lcu64.size,
                                0,
                                input_bti + 1);

    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_enc_scratch_lcu64_buffer,
                                1,
                                vme_context->res_enc_scratch_lcu64_buffer.size,
                                0,
                                input_bti + 2);

    if (frame_info->mapped_ref_idx_list0[0] >= 0)
        l0_surface = common_res->reference_pics[frame_info->mapped_ref_idx_list0[0]].obj_surface;
    else
        l0_surface = NULL;

    if (!l0_surface || !l0_surface->private_data) {
        l0_surface = vme_surface;
    } else {
        surface_priv = (struct gen10_hevc_surface_priv *)(l0_surface->private_data);
        l0_surface = surface_priv->scaled_2x_surface;
    }

    l1_surface = l0_surface;
    if (slice_param->slice_type == HEVC_SLICE_B) {
        if (frame_info->mapped_ref_idx_list1[0] > 0)
            l1_surface = common_res->reference_pics[frame_info->mapped_ref_idx_list1[0]].obj_surface;
        else
            l1_surface = NULL;

        if (!l1_surface || !l1_surface->private_data)
            l1_surface = l0_surface;
        else {
            surface_priv = (struct gen10_hevc_surface_priv *)(l1_surface->private_data);
            l1_surface = surface_priv->scaled_2x_surface;
        }
    }

    input_bti = GEN10_HEVC_MBENC_INTER_LCU64_VME_PRED_CURR_PIC_2xDS_IDX0;
    i965_add_adv_gpe_surface(ctx, gpe_context,
                             vme_surface,
                             input_bti);

    input_bti = GEN10_HEVC_MBENC_INTER_LCU64_VME_PRED_CURR_PIC_2xDS_IDX0;
    for (i = 0; i < 4; i++) {
        if (frame_info->mapped_ref_idx_list0[i] >= 0)
            tmp_surface = common_res->reference_pics[frame_info->mapped_ref_idx_list0[i]].obj_surface;
        else
            tmp_surface = NULL;

        if (tmp_surface && tmp_surface->private_data) {
            surface_priv = (struct gen10_hevc_surface_priv *)(tmp_surface->private_data);
            tmp_surface = surface_priv->scaled_2x_surface;

            i965_add_adv_gpe_surface(ctx, gpe_context,
                                     tmp_surface,
                                     input_bti + 2 * i);
        } else {
            i965_add_adv_gpe_surface(ctx, gpe_context,
                                     l0_surface,
                                     input_bti + 2 * i);

        }

        if (slice_param->slice_type == HEVC_SLICE_B) {
            if (frame_info->mapped_ref_idx_list1[i] >= 0)
                tmp_surface = common_res->reference_pics[frame_info->mapped_ref_idx_list1[i]].obj_surface;
            else
                tmp_surface = NULL;

            if (tmp_surface && tmp_surface->private_data) {
                surface_priv = (struct gen10_hevc_surface_priv *)(tmp_surface->private_data);
                tmp_surface = surface_priv->scaled_2x_surface;

                i965_add_adv_gpe_surface(ctx, gpe_context,
                                         tmp_surface,
                                         input_bti + 2 * i + 1);
            } else
                i965_add_adv_gpe_surface(ctx, gpe_context,
                                         l1_surface,
                                         input_bti + 2 * i + 1);
        }
    }

    input_bti = GEN10_HEVC_MBENC_INTER_LCU64_JOB_QUEUE_1D_SURFACE;

    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_jbq_header_lcu64_buffer,
                                1,
                                vme_context->res_jbq_header_lcu64_buffer.size,
                                0,
                                input_bti);

    i965_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                   &vme_context->res_jbq_data_lcu64_surface,
                                   1, I965_SURFACEFORMAT_R8_UNORM,
                                   input_bti + 1);

    input_bti = GEN10_HEVC_MBENC_INTER_LCU64_RESIDUAL_DATA_SCRATCH_SURFACE;
    i965_add_buffer_2d_gpe_surface(ctx, gpe_context,
                                   &vme_context->res_residual_scratch_lcu64_surface,
                                   1, I965_SURFACEFORMAT_R8_UNORM,
                                   input_bti);


    input_bti = GEN10_HEVC_MBENC_INTER_LCU64_DEBUG_SURFACE;
    i965_add_buffer_gpe_surface(ctx, gpe_context,
                                &vme_context->res_kernel_trace_data,
                                0,
                                vme_context->res_kernel_trace_data.size,
                                0,
                                input_bti);
}

static void
gen10_hevc_mbenc_init_walker_param(struct gen10_hevc_enc_state *hevc_state,
                                   struct gen10_hevc_enc_kernel_walker_parameter *kernel_walker_param,
                                   struct gpe_media_object_walker_parameter *media_object_walker_param,
                                   struct gen10_hevc_gpe_scoreboard *hw_scoreboard)
{
    int mw_26zx_h_factor;

    if (kernel_walker_param->use_custom_walker == 0) {
        hw_scoreboard->scoreboard0.mask = 0x7F;
        hw_scoreboard->scoreboard0.enable = hevc_state->use_hw_scoreboard;
        hw_scoreboard->scoreboard0.type = hevc_state->use_hw_non_stalling_scoreboard;

        hw_scoreboard->dw1.scoreboard1.delta_x0 = -1;
        hw_scoreboard->dw1.scoreboard1.delta_y0 = 0;

        hw_scoreboard->dw1.scoreboard1.delta_x1 = 0;
        hw_scoreboard->dw1.scoreboard1.delta_y1 = -1;

        hw_scoreboard->dw1.scoreboard1.delta_x2 = 1;
        hw_scoreboard->dw1.scoreboard1.delta_y2 = -1;

        hw_scoreboard->dw1.scoreboard1.delta_x3 = -1;
        hw_scoreboard->dw1.scoreboard1.delta_y3 = -1;

        hw_scoreboard->dw2.scoreboard2.delta_x4 = 0;
        hw_scoreboard->dw2.scoreboard2.delta_y4 = 0;
        hw_scoreboard->dw2.scoreboard2.delta_x5 = 0;
        hw_scoreboard->dw2.scoreboard2.delta_y5 = 0;
        hw_scoreboard->dw2.scoreboard2.delta_x6 = 0;
        hw_scoreboard->dw2.scoreboard2.delta_y6 = 0;
        hw_scoreboard->dw2.scoreboard2.delta_x7 = 0;
        hw_scoreboard->dw2.scoreboard2.delta_y7 = 0;

        gen10_init_media_object_walker_parameter(kernel_walker_param, media_object_walker_param);
        return;
    }

    media_object_walker_param->color_count_minus1 = hevc_state->hevc_wf_param.num_regions - 1;

    media_object_walker_param->use_scoreboard = kernel_walker_param->use_scoreboard;

    media_object_walker_param->local_loop_exec_count = 0xFFF;
    media_object_walker_param->global_loop_exec_count = 0xFFF;

    switch (kernel_walker_param->walker_degree) {
    case GEN10_WALKER_26_DEGREE:
        if (hevc_state->num_regions_in_slice > 1) {
            int thread_space_width  = kernel_walker_param->resolution_x;
            int thread_space_height = hevc_state->hevc_wf_param.max_height_in_region;

            int ts_width  = thread_space_width;
            int ts_height = thread_space_height;
            int tmp_height = (ts_height + 1) & 0xfffe;
            ts_height = tmp_height;
            tmp_height     = ((ts_width + 1) >> 1) + ((ts_width + ((tmp_height - 1) << 1)) + (2 * hevc_state->num_regions_in_slice - 1)) / (2 * hevc_state->num_regions_in_slice);

            media_object_walker_param->block_resolution.x           = ts_width;
            media_object_walker_param->block_resolution.y           = tmp_height;

            media_object_walker_param->global_start.x               = 0;
            media_object_walker_param->global_start.y               = 0;

            media_object_walker_param->global_resolution.x          = ts_width;
            media_object_walker_param->global_resolution.y          = tmp_height;

            media_object_walker_param->local_start.x                = (ts_width + 1) & 0xfffe;;
            media_object_walker_param->local_start.y                = 0;

            media_object_walker_param->local_end.x                  = 0;
            media_object_walker_param->local_end.y                  = 0;

            media_object_walker_param->global_outer_loop_stride.x    = ts_width;
            media_object_walker_param->global_outer_loop_stride.y    = 0;

            media_object_walker_param->global_inner_loop_unit.x       = 0;
            media_object_walker_param->global_inner_loop_unit.y       = tmp_height;

            media_object_walker_param->scoreboard_mask              = 0x7F;
            media_object_walker_param->local_outer_loop_stride.x        = 1;
            media_object_walker_param->local_outer_loop_stride.y        = 0;
            media_object_walker_param->local_inner_loop_unit.x        = -2;
            media_object_walker_param->local_inner_loop_unit.y        = 1;

            media_object_walker_param->global_loop_exec_count       = 0;
            media_object_walker_param->local_loop_exec_count        = (thread_space_width + (ts_height - 1) * 2 + hevc_state->num_regions_in_slice - 1) / hevc_state->num_regions_in_slice;
        } else {
            media_object_walker_param->block_resolution.x        = kernel_walker_param->resolution_x;
            media_object_walker_param->block_resolution.y        = kernel_walker_param->resolution_y;

            media_object_walker_param->global_resolution.x       = media_object_walker_param->block_resolution.x;
            media_object_walker_param->global_resolution.y       = media_object_walker_param->block_resolution.y;

            media_object_walker_param->global_outer_loop_stride.x = media_object_walker_param->block_resolution.x;
            media_object_walker_param->global_outer_loop_stride.y = 0;

            media_object_walker_param->global_inner_loop_unit.x    = 0;
            media_object_walker_param->global_inner_loop_unit.y    = media_object_walker_param->block_resolution.y;

            media_object_walker_param->scoreboard_mask         = 0x7F;
            media_object_walker_param->local_outer_loop_stride.x   = 1;
            media_object_walker_param->local_outer_loop_stride.y   = 0;
            media_object_walker_param->local_inner_loop_unit.x   = -2;
            media_object_walker_param->local_inner_loop_unit.y   = 1;
        }

        {
            hw_scoreboard->scoreboard0.mask       = 0x7F;
            hw_scoreboard->scoreboard0.enable     = hevc_state->use_hw_scoreboard;

            hw_scoreboard->dw1.scoreboard1.delta_x0 = -1;
            hw_scoreboard->dw1.scoreboard1.delta_y0 = 0;

            hw_scoreboard->dw1.scoreboard1.delta_x1 = -1;
            hw_scoreboard->dw1.scoreboard1.delta_y1 = -1;

            hw_scoreboard->dw1.scoreboard1.delta_x2 = 0;
            hw_scoreboard->dw1.scoreboard1.delta_y2 = -1;

            hw_scoreboard->dw1.scoreboard1.delta_x3 = 1;
            hw_scoreboard->dw1.scoreboard1.delta_y3 = -1;

            hw_scoreboard->dw2.scoreboard2.delta_x4 = 0;
            hw_scoreboard->dw2.scoreboard2.delta_y4 = 0;

            hw_scoreboard->dw2.scoreboard2.delta_x5 = 0;
            hw_scoreboard->dw2.scoreboard2.delta_y5 = 0;

            hw_scoreboard->dw2.scoreboard2.delta_x6 = 0;
            hw_scoreboard->dw2.scoreboard2.delta_y6 = 0;

            hw_scoreboard->dw2.scoreboard2.delta_x7 = 0;
            hw_scoreboard->dw2.scoreboard2.delta_y7 = 0;
        }
        break;
    case GEN10_WALKER_26Z_DEGREE: {
        media_object_walker_param->scoreboard_mask           = 0x7f;

        media_object_walker_param->global_resolution.x       = kernel_walker_param->resolution_x;
        media_object_walker_param->global_resolution.y       = kernel_walker_param->resolution_y;

        media_object_walker_param->global_outer_loop_stride.x = 2;
        media_object_walker_param->global_outer_loop_stride.y = 0;

        media_object_walker_param->global_inner_loop_unit.x    = 0xFFF - 4 + 1;
        media_object_walker_param->global_inner_loop_unit.y    = 2;

        media_object_walker_param->local_outer_loop_stride.x     = 0;
        media_object_walker_param->local_outer_loop_stride.y     = 1;
        media_object_walker_param->local_inner_loop_unit.x     = 1;
        media_object_walker_param->local_inner_loop_unit.y     = 0;

        media_object_walker_param->block_resolution.x        = 2;
        media_object_walker_param->block_resolution.y        = 2;
    }

    {
        hw_scoreboard->scoreboard0.type           = hevc_state->use_hw_non_stalling_scoreboard;
        hw_scoreboard->scoreboard0.mask           = 0x7F;
        hw_scoreboard->scoreboard0.enable         = hevc_state->use_hw_scoreboard;

        hw_scoreboard->dw1.scoreboard1.delta_x0 = -1;
        hw_scoreboard->dw1.scoreboard1.delta_y0 = 1;

        hw_scoreboard->dw1.scoreboard1.delta_x1 = -1;
        hw_scoreboard->dw1.scoreboard1.delta_y1 = 0;

        hw_scoreboard->dw1.scoreboard1.delta_x2 = -1;
        hw_scoreboard->dw1.scoreboard1.delta_y2 = -1;

        hw_scoreboard->dw1.scoreboard1.delta_x3 = 0;
        hw_scoreboard->dw1.scoreboard1.delta_y3 = -1;

        hw_scoreboard->dw2.scoreboard2.delta_x4 = 1;
        hw_scoreboard->dw2.scoreboard2.delta_y4 = -1;
    }
    break;
    case GEN10_WALKER_26X_DEGREE:
        if (hevc_state->num_regions_in_slice > 1) {
            int thread_space_width  = ALIGN(hevc_state->frame_width, 32) >> 5;
            int ts_width            = thread_space_width;
            int ts_height           = hevc_state->hevc_wf_param.max_height_in_region;
            int tmp_height          = (ts_height + 1) & 0xfffe;
            ts_height               =  tmp_height;
            tmp_height              = ((ts_width + 1) >> 1) + ((ts_width + ((tmp_height - 1) << 1)) + (2 * hevc_state->num_regions_in_slice - 1)) / (2 * hevc_state->num_regions_in_slice);
            tmp_height             *= (hevc_state->thread_num_per_ctb);

            media_object_walker_param->scoreboard_mask                   = 0xff;

            media_object_walker_param->global_resolution.x               = ts_width;
            media_object_walker_param->global_resolution.y               = tmp_height;

            media_object_walker_param->global_start.x                    = 0;
            media_object_walker_param->global_start.y                    = 0;

            media_object_walker_param->local_start.x                     = (ts_width + 1) & 0xfffe;
            media_object_walker_param->local_start.y                     = 0;

            media_object_walker_param->local_end.x                       = 0;
            media_object_walker_param->local_end.y                       = 0;

            media_object_walker_param->global_outer_loop_stride.x         = ts_width;
            media_object_walker_param->global_outer_loop_stride.y         = 0;

            media_object_walker_param->global_inner_loop_unit.x            = 0;
            media_object_walker_param->global_inner_loop_unit.y            = tmp_height;

            media_object_walker_param->local_outer_loop_stride.x             = 1;
            media_object_walker_param->local_outer_loop_stride.y             = 0;
            media_object_walker_param->local_inner_loop_unit.x             = -2;
            media_object_walker_param->local_inner_loop_unit.y             = hevc_state->thread_num_per_ctb;
            media_object_walker_param->middle_loop_extra_steps             = hevc_state->thread_num_per_ctb - 1;
            media_object_walker_param->mid_loop_unit_x                     = 0;
            media_object_walker_param->mid_loop_unit_y                     = 1;

            media_object_walker_param->block_resolution.x                = media_object_walker_param->global_resolution.x;
            media_object_walker_param->block_resolution.y                = media_object_walker_param->global_resolution.y;

            media_object_walker_param->global_loop_exec_count            = 0;
            media_object_walker_param->local_loop_exec_count             = (thread_space_width + (ts_height - 1) * 2 + hevc_state->num_regions_in_slice - 1) / hevc_state->num_regions_in_slice;
        } else {
            media_object_walker_param->scoreboard_mask           = 0xff;

            media_object_walker_param->global_resolution.x       = kernel_walker_param->resolution_x;
            media_object_walker_param->global_resolution.y       = kernel_walker_param->resolution_y * hevc_state->thread_num_per_ctb;

            media_object_walker_param->global_outer_loop_stride.x = media_object_walker_param->global_resolution.x;
            media_object_walker_param->global_outer_loop_stride.y = 0;

            media_object_walker_param->global_inner_loop_unit.x    = 0;
            media_object_walker_param->global_inner_loop_unit.y    = media_object_walker_param->global_resolution.y;

            media_object_walker_param->local_outer_loop_stride.x     = 1;
            media_object_walker_param->local_outer_loop_stride.y     = 0;
            media_object_walker_param->local_inner_loop_unit.x     = 0xFFF - 2 + 1; // -2 in 2's compliment format;
            media_object_walker_param->local_inner_loop_unit.y     = hevc_state->thread_num_per_ctb;
            media_object_walker_param->middle_loop_extra_steps     = hevc_state->thread_num_per_ctb - 1;
            media_object_walker_param->mid_loop_unit_x             = 0;
            media_object_walker_param->mid_loop_unit_y             = 1;

            media_object_walker_param->block_resolution.x        = media_object_walker_param->global_resolution.x;
            media_object_walker_param->block_resolution.y        = media_object_walker_param->global_resolution.y;
        }

        {
            hw_scoreboard->scoreboard0.type           = hevc_state->use_hw_non_stalling_scoreboard;
            hw_scoreboard->scoreboard0.mask           = 0xff;
            hw_scoreboard->scoreboard0.enable         = hevc_state->use_hw_scoreboard;

            hw_scoreboard->dw1.scoreboard1.delta_x0 = -1;
            hw_scoreboard->dw1.scoreboard1.delta_y0 = hevc_state->thread_num_per_ctb - 1;

            hw_scoreboard->dw1.scoreboard1.delta_x1 = -1;
            hw_scoreboard->dw1.scoreboard1.delta_y1 = -1;

            hw_scoreboard->dw1.scoreboard1.delta_x2 = 0;
            hw_scoreboard->dw1.scoreboard1.delta_y2 = -1;

            hw_scoreboard->dw1.scoreboard1.delta_x3 = 1;
            hw_scoreboard->dw1.scoreboard1.delta_y3 = -1;

            hw_scoreboard->dw2.scoreboard2.delta_x4 = 0;
            hw_scoreboard->dw2.scoreboard2.delta_y4 = -hevc_state->thread_num_per_ctb;

            hw_scoreboard->dw2.scoreboard2.delta_x5 = 0;
            hw_scoreboard->dw2.scoreboard2.delta_y5 = -2;

            hw_scoreboard->dw2.scoreboard2.delta_x6 = 0;
            hw_scoreboard->dw2.scoreboard2.delta_y6 = -3;

            hw_scoreboard->dw2.scoreboard2.delta_x7 = 0;
            hw_scoreboard->dw2.scoreboard2.delta_y7 = -4;
        }

        break;
    case GEN10_WALKER_26ZX_DEGREE:
        mw_26zx_h_factor                            = 5;

        if (hevc_state->num_regions_in_slice > 1) {
            int thread_space_width  = ALIGN(hevc_state->frame_width, 64) >> 6;
            int thread_space_height = hevc_state->hevc_wf_param.max_height_in_region;
            int sp_width  = (thread_space_width + 1) & 0xfffe;
            int sp_height = (thread_space_height + 1) & 0xfffe;
            int wf_num = (sp_width + (sp_height - 1) * 2 + hevc_state->num_regions_in_slice - 1) / hevc_state->num_regions_in_slice;
            sp_height     = ((sp_width + 1) >> 1) + ((sp_width + ((sp_height - 1) << 1)) + (2 * hevc_state->num_regions_in_slice - 1)) / (2 * hevc_state->num_regions_in_slice);
            int ts_width  = sp_width * mw_26zx_h_factor;
            int ts_height = sp_height * (hevc_state->thread_num_per_ctb);

            media_object_walker_param->scoreboard_mask          = 0xff;

            media_object_walker_param->global_resolution.x      = ts_width;
            media_object_walker_param->global_resolution.y      = ts_height;

            media_object_walker_param->global_start.x           = 0;
            media_object_walker_param->global_start.y           = 0;

            media_object_walker_param->local_start.x            = media_object_walker_param->global_resolution.x;
            media_object_walker_param->local_start.y            = 0;

            media_object_walker_param->local_end.x              = 0;
            media_object_walker_param->local_end.y              = 0;

            media_object_walker_param->global_outer_loop_stride.x = media_object_walker_param->global_resolution.x;
            media_object_walker_param->global_outer_loop_stride.y = 0;

            media_object_walker_param->global_inner_loop_unit.x    = 0;
            media_object_walker_param->global_inner_loop_unit.y    = media_object_walker_param->global_resolution.y;

            media_object_walker_param->local_outer_loop_stride.x = 1;
            media_object_walker_param->local_outer_loop_stride.y = 0;
            media_object_walker_param->local_inner_loop_unit.x = -mw_26zx_h_factor * 2;
            media_object_walker_param->local_inner_loop_unit.y = hevc_state->thread_num_per_ctb;
            media_object_walker_param->middle_loop_extra_steps = hevc_state->thread_num_per_ctb - 1;
            media_object_walker_param->mid_loop_unit_x         = 0;
            media_object_walker_param->mid_loop_unit_y         = 1;

            media_object_walker_param->block_resolution.x = media_object_walker_param->global_resolution.x;
            media_object_walker_param->block_resolution.y = media_object_walker_param->global_resolution.y;

            media_object_walker_param->global_loop_exec_count = 0;
            media_object_walker_param->local_loop_exec_count  = (wf_num + 1) * mw_26zx_h_factor;
        } else {
            media_object_walker_param->scoreboard_mask = 0xff;

            media_object_walker_param->global_resolution.x = kernel_walker_param->resolution_x * mw_26zx_h_factor;
            media_object_walker_param->global_resolution.y = kernel_walker_param->resolution_y * hevc_state->thread_num_per_ctb;

            media_object_walker_param->global_outer_loop_stride.x = media_object_walker_param->global_resolution.x;
            media_object_walker_param->global_outer_loop_stride.y = 0;

            media_object_walker_param->global_inner_loop_unit.x = 0;
            media_object_walker_param->global_inner_loop_unit.y = media_object_walker_param->global_resolution.y;

            media_object_walker_param->local_outer_loop_stride.x = 1;
            media_object_walker_param->local_outer_loop_stride.y = 0;
            media_object_walker_param->local_inner_loop_unit.x = 0xFFF - 10 + 1; // -10 in 2's compliment format;
            media_object_walker_param->local_inner_loop_unit.y = hevc_state->thread_num_per_ctb;
            media_object_walker_param->middle_loop_extra_steps = hevc_state->thread_num_per_ctb - 1;
            media_object_walker_param->mid_loop_unit_x = 0;
            media_object_walker_param->mid_loop_unit_y = 1;

            media_object_walker_param->block_resolution.x = media_object_walker_param->global_resolution.x;
            media_object_walker_param->block_resolution.y = media_object_walker_param->global_resolution.y;
        }

        {
            hw_scoreboard->scoreboard0.mask = 0xff;
            hw_scoreboard->scoreboard0.type = hevc_state->use_hw_non_stalling_scoreboard;
            hw_scoreboard->scoreboard0.enable = hevc_state->use_hw_scoreboard;

            hw_scoreboard->dw1.scoreboard1.delta_x0 = -5;
            hw_scoreboard->dw1.scoreboard1.delta_y0 = -1;

            hw_scoreboard->dw1.scoreboard1.delta_x1 = -2;
            hw_scoreboard->dw1.scoreboard1.delta_y1 = -1;

            hw_scoreboard->dw1.scoreboard1.delta_x2 = 3;
            hw_scoreboard->dw1.scoreboard1.delta_y2 = -1;

            hw_scoreboard->dw1.scoreboard1.delta_x3 = -1;
            hw_scoreboard->dw1.scoreboard1.delta_y3 = 0;

            hw_scoreboard->dw2.scoreboard2.delta_x4 = -2;
            hw_scoreboard->dw2.scoreboard2.delta_y4 = 0;

            hw_scoreboard->dw2.scoreboard2.delta_x5 = -5;
            hw_scoreboard->dw2.scoreboard2.delta_y5 = hevc_state->thread_num_per_ctb - 1;

            hw_scoreboard->dw2.scoreboard2.delta_x6 = 0;
            hw_scoreboard->dw2.scoreboard2.delta_y6 = -1;

            hw_scoreboard->dw2.scoreboard2.delta_x7 = 5;
            hw_scoreboard->dw2.scoreboard2.delta_y7 = -1;
        }
        break;
    default:
        break;
    }

    return;
}

static void
gen10_hevc_update_scoreboard(struct i965_gpe_context *gpe_context,
                             struct gen10_hevc_gpe_scoreboard *scoreboard)
{
    if (!gpe_context || !scoreboard)
        return;

    gpe_context->vfe_desc5.scoreboard0.mask = scoreboard->scoreboard0.mask;
    gpe_context->vfe_desc5.scoreboard0.type = scoreboard->scoreboard0.type;
    gpe_context->vfe_desc5.scoreboard0.enable = scoreboard->scoreboard0.enable;

    gpe_context->vfe_desc6.dword = scoreboard->dw1.value;
    gpe_context->vfe_desc7.dword = scoreboard->dw2.value;
    return;
}

static void
gen10_hevc_enc_mbenc_kernel(VADriverContextP ctx,
                            struct encode_state *encode_state,
                            struct intel_encoder_context *encoder_context,
                            int mbenc_type)
{
    struct gen10_hevc_enc_context *vme_context = encoder_context->vme_context;
    struct gen10_hevc_enc_state *hevc_state;
    struct i965_gpe_context *gpe_context;
    int media_function;
    struct gpe_media_object_walker_parameter media_object_walker_param;
    struct gen10_hevc_enc_kernel_walker_parameter kernel_walker_param;
    struct gen10_hevc_gpe_scoreboard hw_scoreboard;
    int mbenc_idx;

    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;

    if (mbenc_type != GEN10_HEVC_MBENC_INTRA)
        gen10_hevc_enc_generate_regions_in_slice_control(ctx, encode_state, encoder_context);

    switch (mbenc_type) {
    case GEN10_HEVC_MBENC_INTER_LCU32:
        mbenc_idx = GEN10_HEVC_MBENC_INTER_LCU32_KRNIDX_G10;
        media_function = GEN10_HEVC_MEDIA_STATE_MBENC_LCU32;
        break;
    case GEN10_HEVC_MBENC_INTER_LCU64:
        mbenc_idx = GEN10_HEVC_MBENC_INTER_LCU64_KRNIDX_G10;
        media_function = GEN10_HEVC_MEDIA_STATE_MBENC_LCU64;
        break;
    case GEN10_HEVC_MBENC_INTRA:
    default:
        mbenc_idx = GEN10_HEVC_MBENC_I_KRNIDX_G10;
        media_function = GEN10_HEVC_MEDIA_STATE_MBENC_INTRA;
        break;
    }

    gpe_context = &(vme_context->mbenc_context.gpe_contexts[mbenc_idx]);

    memset(&hw_scoreboard, 0, sizeof(hw_scoreboard));
    memset(&kernel_walker_param, 0, sizeof(kernel_walker_param));
    gen8_gpe_context_init(ctx, gpe_context);
    gen9_gpe_reset_binding_table(ctx, gpe_context);

    kernel_walker_param.use_scoreboard = hevc_state->use_hw_scoreboard;
    kernel_walker_param.use_custom_walker = 0;
    if (mbenc_type == GEN10_HEVC_MBENC_INTRA)
        gen10_hevc_enc_mbenc_intra_curbe(ctx, encode_state, encoder_context, gpe_context);
    else
        gen10_hevc_enc_mbenc_inter_curbe(ctx, encode_state, encoder_context, gpe_context);

    if (mbenc_type == GEN10_HEVC_MBENC_INTRA) {
        gen10_hevc_enc_mbenc_intra_surfaces(ctx, encode_state, encoder_context, gpe_context);
        kernel_walker_param.resolution_x = ALIGN(hevc_state->frame_width, 32) >> 5;
        kernel_walker_param.resolution_y = ALIGN(hevc_state->frame_height, 32) >> 5;
        if (hevc_state->is_64lcu) {
            kernel_walker_param.walker_degree = GEN10_WALKER_26_DEGREE;// 26_DEGREE
            kernel_walker_param.use_custom_walker = 1;
        } else {
            kernel_walker_param.use_vertical_scan = 1;
        }
    } else if (mbenc_type == GEN10_HEVC_MBENC_INTER_LCU32) {
        gen10_hevc_enc_mbenc_inter_lcu32_surfaces(ctx, encode_state, encoder_context, gpe_context);
        kernel_walker_param.resolution_x = ALIGN(hevc_state->frame_width, 32) >> 5;
        kernel_walker_param.resolution_y = ALIGN(hevc_state->frame_height, 32) >> 5;
        kernel_walker_param.use_custom_walker = 1;
        if (hevc_state->brc.target_usage == 7)
            kernel_walker_param.walker_degree = GEN10_WALKER_26_DEGREE;
        else
            kernel_walker_param.walker_degree = GEN10_WALKER_26X_DEGREE;
    } else {
        gen10_hevc_enc_mbenc_inter_lcu64_surfaces(ctx, encode_state, encoder_context, gpe_context);
        kernel_walker_param.resolution_x = vme_context->frame_info.width_in_lcu;
        kernel_walker_param.resolution_y = vme_context->frame_info.height_in_lcu;
        kernel_walker_param.use_custom_walker = 1;
        kernel_walker_param.walker_degree = GEN10_WALKER_26ZX_DEGREE;
    }

    gen10_hevc_enc_generate_lculevel_data(ctx, encode_state, encoder_context);

    memset(&hw_scoreboard, 0, sizeof(hw_scoreboard));
    memset(&media_object_walker_param, 0, sizeof(media_object_walker_param));

    gen10_hevc_mbenc_init_walker_param(hevc_state, &kernel_walker_param,
                                       &media_object_walker_param,
                                       &hw_scoreboard);

    gen10_hevc_update_scoreboard(gpe_context, &hw_scoreboard);

    gen8_gpe_setup_interface_data(ctx, gpe_context);

    gen10_run_kernel_media_object_walker(ctx, encoder_context,
                                         gpe_context,
                                         media_function,
                                         &media_object_walker_param);
}

static VAStatus
gen10_hevc_vme_pipeline_prepare(VADriverContextP ctx,
                                struct encode_state *encode_state,
                                struct intel_encoder_context *encoder_context)
{
    struct gen10_hevc_enc_context *vme_context = encoder_context->vme_context;
    struct gen10_hevc_enc_state *hevc_state;
    struct gen10_hevc_enc_frame_info *frame_info;
    struct gen10_hevc_enc_common_res *common_res;
    int i;

    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;
    frame_info = &vme_context->frame_info;
    common_res = &vme_context->common_res;

    if (hevc_state->is_64lcu || hevc_state->is_10bit) {
        if (frame_info->picture_coding_type != HEVC_SLICE_I) {
            for (i = 0; i < 16; i++) {
                if (common_res->reference_pics[i].obj_surface == NULL)
                    continue;

                gen10_hevc_enc_conv_scaling_surface(ctx, encode_state,
                                                    encoder_context,
                                                    NULL,
                                                    common_res->reference_pics[i].obj_surface,
                                                    1);
            }
        }
    }

    gen10_hevc_enc_conv_scaling_surface(ctx, encode_state, encoder_context,
                                        common_res->uncompressed_pic.obj_surface,
                                        common_res->reconstructed_pic.obj_surface,
                                        0);

    return VA_STATUS_SUCCESS;
}

static VAStatus
gen10_hevc_vme_pipeline(VADriverContextP ctx,
                        VAProfile profile,
                        struct encode_state *encode_state,
                        struct intel_encoder_context *encoder_context)
{
    struct gen10_hevc_enc_context *vme_context = encoder_context->vme_context;
    struct gen10_hevc_enc_state *hevc_state;
    struct gen10_hevc_enc_frame_info *frame_info;
    VAStatus va_status = VA_STATUS_SUCCESS;

    if (!vme_context || !vme_context->enc_priv_state)
        return VA_STATUS_ERROR_INVALID_CONTEXT;

    hevc_state = (struct gen10_hevc_enc_state *)vme_context->enc_priv_state;
    frame_info = &vme_context->frame_info;

    va_status = gen10_hevc_enc_init_parameters(ctx, encode_state, encoder_context);
    if (va_status != VA_STATUS_SUCCESS)
        return va_status;

    va_status = gen10_hevc_vme_pipeline_prepare(ctx, encode_state, encoder_context);
    if (va_status != VA_STATUS_SUCCESS)
        return va_status;

    if (hevc_state->brc.brc_reset || !hevc_state->brc.brc_inited) {
        gen10_hevc_enc_brc_init_reset(ctx, encode_state, encoder_context);

        hevc_state->brc.brc_inited = 1;
        hevc_state->brc.brc_reset = 0;
    }

    if (frame_info->picture_coding_type == HEVC_SLICE_I) {
        gen10_hevc_enc_me_kernel(ctx, encode_state, encoder_context,
                                 GEN10_HEVC_HME_LEVEL_4X,
                                 GEN10_HEVC_ME_DIST_TYPE_INTRA_BRC);
    } else {
        if (hevc_state->hme_enabled) {
            if (hevc_state->b16xme_enabled)
                gen10_hevc_enc_me_kernel(ctx, encode_state, encoder_context,
                                         GEN10_HEVC_HME_LEVEL_16X,
                                         GEN10_HEVC_ME_DIST_TYPE_INTER_BRC);



            gen10_hevc_enc_me_kernel(ctx, encode_state, encoder_context,
                                     GEN10_HEVC_HME_LEVEL_4X,
                                     GEN10_HEVC_ME_DIST_TYPE_INTER_BRC);
        }
    }

    gen10_hevc_enc_me_kernel(ctx, encode_state, encoder_context,
                             GEN10_HEVC_HME_LEVEL_4X,
                             GEN10_HEVC_ME_DIST_TYPE_INTRA);

    gen10_hevc_enc_brc_frame_update_kernel(ctx, encode_state,
                                           encoder_context);

    gen10_hevc_enc_brc_lcu_update_kernel(ctx, encode_state,
                                         encoder_context);

    if (frame_info->picture_coding_type == HEVC_SLICE_I)
        gen10_hevc_enc_mbenc_kernel(ctx, encode_state, encoder_context,
                                    GEN10_HEVC_MBENC_INTRA);
    else
        gen10_hevc_enc_mbenc_kernel(ctx, encode_state, encoder_context,
                                    (hevc_state->is_64lcu ?
                                     GEN10_HEVC_MBENC_INTER_LCU64 :
                                     GEN10_HEVC_MBENC_INTER_LCU32));


#if 0
    if (hevc_state->frame_number == 0) {
        struct gen10_hevc_surface_priv *surface_priv = NULL;

        surface_priv = (struct gen10_hevc_surface_priv *)encode_state->reconstructed_object->private_data;
        //print_out_obj_surface(ctx, surface_priv->scaled_4x_surface_id, 1);

        //print_out_gpe_resource(&vme_context->res_mb_code_surface, 0,
        //                       hevc_state->cu_records_offset, 1, 0, 0, 64);
        //print_out_gpe_resource(&vme_context->res_mb_code_surface, 0,
        //                       0, 1, 0, 0, 64);
        //print_out_gpe_resource(&vme_context->res_s4x_me_dist_surface, 0,
        //                       0, 1, 0, 0, 64);

        //return VA_STATUS_ERROR_INVALID_PARAMETER;
    }
#endif
    return VA_STATUS_SUCCESS;
}

static void
gen10_hevc_hcp_pipe_mode_select(VADriverContextP ctx,
                                struct encode_state *encode_state,
                                struct intel_encoder_context *encoder_context,
                                struct intel_batchbuffer *batch)
{
    struct gen10_hevc_enc_context *pak_context;
    struct gen10_hevc_enc_state *hevc_state;
    gen10_hcp_pipe_mode_select_param param;

    pak_context = (struct gen10_hevc_enc_context *) encoder_context->mfc_context;
    hevc_state = (struct gen10_hevc_enc_state *)pak_context->enc_priv_state;

    memset(&param, 0, sizeof(param));

    param.dw1.codec_select = GEN10_HCP_ENCODE;
    param.dw1.codec_standard_select = GEN10_HCP_HEVC_CODEC;
    param.dw1.sao_first_pass = hevc_state->sao_first_pass_flag;
    param.dw1.rdoq_enabled = hevc_state->rdoq_enabled;
    param.dw1.pak_frame_level_streamout_enabled = 1;

    if (hevc_state->brc.brc_enabled &&
        hevc_state->curr_pak_idx != (hevc_state->num_sao_passes - 1))
        param.dw1.pak_streamout_enabled = 1;

    gen10_hcp_pipe_mode_select(ctx, batch, &param);
}

static void
gen10_hevc_hcp_multi_surfaces(VADriverContextP ctx,
                              struct encode_state *encode_state,
                              struct intel_encoder_context *encoder_context,
                              struct intel_batchbuffer *batch)
{
    struct gen10_hevc_enc_context *pak_context;
    gen10_hcp_surface_state_param param;
    struct object_surface *obj_surface;
    int i = 0;

    pak_context = (struct gen10_hevc_enc_context *) encoder_context->mfc_context;

    for (i = 0; i < 2; i++) {
        if (i == 0)
            obj_surface = pak_context->common_res.reconstructed_pic.obj_surface;
        else
            obj_surface = pak_context->common_res.uncompressed_pic.obj_surface;

        memset(&param, 0, sizeof(param));

        param.dw1.surface_pitch = obj_surface->width - 1;
        param.dw1.surface_id = (i == 0 ? GEN10_HCP_DECODE_SURFACE_ID :
                                GEN10_HCP_INPUT_SURFACE_ID);
        param.dw2.y_cb_offset = obj_surface->y_cb_offset;

        if (obj_surface->fourcc == VA_FOURCC_P010)
            param.dw2.surface_format = SURFACE_FORMAT_P010;
        else if (obj_surface->fourcc == VA_FOURCC_NV12)
            param.dw2.surface_format = SURFACE_FORMAT_PLANAR_420_8;
        else
            assert(0);

        gen10_hcp_surface_state(ctx, batch, &param);
    }
}

static void
gen10_hevc_hcp_pipe_buf_state(VADriverContextP ctx,
                              struct encode_state *encode_state,
                              struct intel_encoder_context *encoder_context,
                              struct intel_batchbuffer *batch)
{
    struct gen10_hevc_enc_context *pak_context;
    struct gen10_hevc_surface_priv *surface_priv;
    gen10_hcp_pipe_buf_addr_state_param param;
    struct gen10_hevc_enc_common_res *common_res;
    int i;

    pak_context = (struct gen10_hevc_enc_context *) encoder_context->mfc_context;

    common_res = &pak_context->common_res;
    surface_priv = (struct gen10_hevc_surface_priv *)common_res->
                   reconstructed_pic.obj_surface->private_data;

    memset(&param, 0, sizeof(param));

    param.reconstructed = &common_res->reconstructed_pic.gpe_res;
    param.deblocking_filter_line = &common_res->deblocking_filter_line_buffer;
    param.deblocking_filter_tile_line = &common_res->deblocking_filter_tile_line_buffer;
    param.deblocking_filter_tile_column = &common_res->deblocking_filter_tile_column_buffer;
    param.metadata_line = &common_res->metadata_line_buffer;
    param.metadata_tile_line = &common_res->metadata_tile_line_buffer;
    param.metadata_tile_column = &common_res->metadata_tile_column_buffer;
    param.sao_line = &common_res->sao_line_buffer;
    param.sao_tile_line = &common_res->sao_tile_line_buffer;
    param.sao_tile_column = &common_res->sao_tile_column_buffer;

    if (surface_priv)
        param.current_motion_vector_temporal = &surface_priv->motion_vector_temporal;

    for (i = 0; i < 8; i++) {
        if (common_res->reference_pics[i].obj_surface)
            param.reference_picture[i] = &common_res->reference_pics[i].gpe_res;
    }

    param.uncompressed_picture = &common_res->uncompressed_pic.gpe_res;
    param.streamout_data_destination = &common_res->streamout_data_destination_buffer;
    param.picture_status = &common_res->picture_status_buffer;
    param.ildb_streamout = &common_res->ildb_streamout_buffer;

    for (i = 0; i < 8; i++) {
        if (common_res->reference_pics[i].obj_surface) {
            surface_priv = (struct gen10_hevc_surface_priv *)common_res->
                           reference_pics[i].obj_surface->private_data;
            if (surface_priv)
                param.collocated_motion_vector_temporal[i] =
                    &surface_priv->motion_vector_temporal;
        }
    }

    param.sao_streamout_data_destination = &common_res->sao_streamout_data_destination_buffer;
    param.frame_statics_streamout_data_destination =
        &common_res->frame_statics_streamout_data_destination_buffer;
    param.sse_source_pixel_rowstore = &common_res->sse_source_pixel_rowstore_buffer;

    gen10_hcp_pipe_buf_addr_state(ctx, batch, &param);
}

static void
gen10_hevc_hcp_ind_obj_base_addr_state(VADriverContextP ctx,
                                       struct encode_state *encode_state,
                                       struct intel_encoder_context *encoder_context,
                                       struct intel_batchbuffer *batch)
{
    struct gen10_hevc_enc_context *pak_context;
    struct gen10_hevc_enc_state *hevc_state;
    gen10_hcp_ind_obj_base_addr_state_param param;

    pak_context = (struct gen10_hevc_enc_context *) encoder_context->mfc_context;
    hevc_state = (struct gen10_hevc_enc_state *)pak_context->enc_priv_state;

    memset(&param, 0, sizeof(param));

    param.ind_cu_obj_bse = &pak_context->res_mb_code_surface;
    param.ind_cu_obj_bse_offset = hevc_state->cu_records_offset;

    param.ind_pak_bse = &pak_context->common_res.compressed_bitstream.gpe_res;
    param.ind_pak_bse_offset = pak_context->common_res.compressed_bitstream.offset;
    param.ind_pak_bse_upper = pak_context->common_res.compressed_bitstream.end_offset;

    gen10_hcp_ind_obj_base_addr_state(ctx, batch, &param);
}

static void
gen10_hevc_hcp_qm_fqm_state(VADriverContextP ctx,
                            struct encode_state *encode_state,
                            struct intel_encoder_context *encoder_context,
                            struct intel_batchbuffer *batch)
{
    struct gen10_hevc_enc_context *pak_context;

    pak_context = (struct gen10_hevc_enc_context *) encoder_context->mfc_context;

    gen10_hevc_enc_hcp_set_qm_fqm_states(ctx, batch, &pak_context->frame_info);
}

static void
gen10_hevc_hcp_pic_state(VADriverContextP ctx,
                         struct encode_state *encode_state,
                         struct intel_encoder_context *encoder_context,
                         struct intel_batchbuffer *batch)
{
    struct gen10_hevc_enc_context *pak_context;
    struct gen10_hevc_enc_state *hevc_state;
    VAEncSequenceParameterBufferHEVC *seq_param;
    VAEncPictureParameterBufferHEVC *pic_param;
    VAEncSliceParameterBufferHEVC *slice_param;
    struct gen10_hevc_enc_frame_info *frame_info;
    gen10_hcp_pic_state_param param;

    pak_context = (struct gen10_hevc_enc_context *) encoder_context->mfc_context;
    hevc_state = (struct gen10_hevc_enc_state *)pak_context->enc_priv_state;

    frame_info = &pak_context->frame_info;
    seq_param = (VAEncSequenceParameterBufferHEVC *)encode_state->seq_param_ext->buffer;
    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;
    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[0]->buffer;

    memset(&param, 0, sizeof(param));
    param.dw1.frame_width_in_cu_minus1 = frame_info->width_in_cu - 1;
    param.dw1.frame_height_in_cu_minus1 = frame_info->height_in_cu - 1;
    param.dw1.pak_transform_skip = pic_param->pic_fields.bits.transform_skip_enabled_flag;

    param.dw2.min_cu_size = seq_param->log2_min_luma_coding_block_size_minus3;
    param.dw2.lcu_size = seq_param->log2_min_luma_coding_block_size_minus3 +
                         seq_param->log2_diff_max_min_luma_coding_block_size;
    param.dw2.min_tu_size = seq_param->log2_min_transform_block_size_minus2;
    param.dw2.max_tu_size = seq_param->log2_min_transform_block_size_minus2 +
                            seq_param->log2_diff_max_min_transform_block_size;
    param.dw2.min_pcm_size = 0;
    param.dw2.max_pcm_size = 0;

    if ((slice_param->slice_fields.bits.slice_sao_luma_flag ||
         slice_param->slice_fields.bits.slice_sao_chroma_flag) &&
        !frame_info->bit_depth_luma_minus8)
        param.dw4.sao_enabled_flag = 1;

    if (pic_param->pic_fields.bits.cu_qp_delta_enabled_flag) {
        param.dw4.cu_qp_delta_enabled_flag = 1;
        param.dw4.diff_cu_qp_delta_depth = pic_param->diff_cu_qp_delta_depth;
    }

    param.dw4.pcm_loop_filter_disable_flag = seq_param->seq_fields.bits.pcm_loop_filter_disabled_flag;
    param.dw4.weighted_bipred_flag = pic_param->pic_fields.bits.weighted_bipred_flag;
    param.dw4.weighted_pred_flag = pic_param->pic_fields.bits.weighted_pred_flag;
    param.dw4.transform_skip_enabled_flag = pic_param->pic_fields.bits.transform_skip_enabled_flag;
    param.dw4.amp_enabled_flag = seq_param->seq_fields.bits.amp_enabled_flag;
    param.dw4.transquant_bypass_enabled_flag = pic_param->pic_fields.bits.transquant_bypass_enabled_flag;
    param.dw4.strong_intra_smoothing_enabled_flag = seq_param->seq_fields.bits.strong_intra_smoothing_enabled_flag;

    param.dw5.pic_cb_qp_offset = pic_param->pps_cr_qp_offset & 0x1f;
    param.dw5.pic_cr_qp_offset = pic_param->pps_cb_qp_offset & 0x1f;
    param.dw5.max_transform_hierarchy_depth_intra = seq_param->max_transform_hierarchy_depth_intra;
    param.dw5.max_transform_hierarchy_depth_inter = seq_param->max_transform_hierarchy_depth_inter;
    param.dw5.pcm_sample_bit_depth_chroma_minus1 = seq_param->pcm_sample_bit_depth_chroma_minus1;
    param.dw5.pcm_sample_bit_depth_luma_minus1 = seq_param->pcm_sample_bit_depth_luma_minus1;
    param.dw5.bit_depth_chroma_minus8 = seq_param->seq_fields.bits.bit_depth_chroma_minus8;
    param.dw5.bit_depth_luma_minus8 = seq_param->seq_fields.bits.bit_depth_luma_minus8;

    param.dw6.lcu_max_bits_allowed = frame_info->ctu_max_bitsize_allowed;

    param.dw19.rho_domain_rc_enabled = 0;
    param.dw19.rho_domain_frame_qp = 0;
    param.dw19.fraction_qp_adj_enabled = 0;
    param.dw19.first_slice_segment_in_pic_flag = 1;
    param.dw19.nal_unit_type_flag = 1;
    param.dw19.sse_enabled = 1;
    param.dw19.rhoq_enabled = hevc_state->rdoq_enabled;

    gen10_hcp_pic_state(ctx, batch, &param);
}

static void
gen10_hevc_hcp_rdoq_state(VADriverContextP ctx,
                          struct encode_state *encode_state,
                          struct intel_encoder_context *encoder_context,
                          struct intel_batchbuffer *batch)
{
    struct gen10_hevc_enc_context *pak_context;
    gen10_hcp_rdoq_state_param param;

    pak_context = (struct gen10_hevc_enc_context *) encoder_context->mfc_context;

    memset(&param, 0, sizeof(param));

    memcpy(param.lambda_intra_luma, pak_context->lambda_param.lambda_intra[0],
           sizeof(param.lambda_intra_luma));
    memcpy(param.lambda_intra_chroma, pak_context->lambda_param.lambda_intra[1],
           sizeof(param.lambda_intra_chroma));
    memcpy(param.lambda_inter_luma, pak_context->lambda_param.lambda_inter[0],
           sizeof(param.lambda_inter_luma));
    memcpy(param.lambda_inter_chroma, pak_context->lambda_param.lambda_inter[1],
           sizeof(param.lambda_inter_chroma));

    gen10_hcp_rdoq_state(ctx, batch, &param);
}

static void
gen10_hevc_pak_picture_level(VADriverContextP ctx,
                             struct encode_state *encode_state,
                             struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct gen10_hevc_enc_context *pak_context;
    struct gen10_hevc_enc_state *hevc_state;

    pak_context = (struct gen10_hevc_enc_context *)encoder_context->mfc_context;
    hevc_state = (struct gen10_hevc_enc_state *) pak_context->enc_priv_state;

    gen10_hevc_hcp_pipe_mode_select(ctx, encode_state, encoder_context, batch);
    gen10_hevc_hcp_multi_surfaces(ctx, encode_state, encoder_context, batch);
    gen10_hevc_hcp_pipe_buf_state(ctx, encode_state, encoder_context, batch);
    gen10_hevc_hcp_ind_obj_base_addr_state(ctx, encode_state, encoder_context, batch);
    gen10_hevc_hcp_qm_fqm_state(ctx, encode_state, encoder_context, batch);

    if (hevc_state->brc.brc_enabled) {
        struct gpe_mi_batch_buffer_start_parameter second_level_batch;

        memset(&second_level_batch, 0, sizeof(second_level_batch));
        second_level_batch.offset = GEN10_HEVC_BRC_IMG_STATE_SIZE_PER_PASS *
                                    hevc_state->curr_pak_idx;
        second_level_batch.is_second_level = 1;
        second_level_batch.bo = pak_context->res_brc_pic_image_state_write_buffer.bo;

        gen8_gpe_mi_batch_buffer_start(ctx, batch, &second_level_batch);
    } else
        gen10_hevc_hcp_pic_state(ctx, encode_state, encoder_context, batch);

    if (hevc_state->rdoq_enabled)
        gen10_hevc_hcp_rdoq_state(ctx, encode_state, encoder_context, batch);
}

static void
gen10_hevc_hcp_weightoffset(VADriverContextP ctx,
                            struct encode_state *encode_state,
                            struct intel_encoder_context *encoder_context,
                            struct intel_batchbuffer *batch,
                            int slice_index)
{
    VAEncPictureParameterBufferHEVC *pic_param;
    VAEncSliceParameterBufferHEVC *slice_param;

    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;
    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[slice_index]->buffer;

    gen10_hevc_enc_hcp_set_weight_offsets(ctx, batch, pic_param, slice_param);
}

static void
gen10_hevc_ref_idx_lists(VADriverContextP ctx,
                         struct encode_state *encode_state,
                         struct intel_encoder_context *encoder_context,
                         struct intel_batchbuffer *batch,
                         int slice_index)
{
    VAEncPictureParameterBufferHEVC *pic_param;
    VAEncSliceParameterBufferHEVC *slice_param;

    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;
    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[slice_index]->buffer;

    if (slice_param->slice_type != HEVC_SLICE_I)
        gen10_hevc_enc_hcp_set_ref_idx_lists(ctx, batch, pic_param, slice_param);
}

static void
gen10_hevc_hcp_slice_state(VADriverContextP ctx,
                           struct encode_state *encode_state,
                           struct intel_encoder_context *encoder_context,
                           struct intel_batchbuffer *batch,
                           int slice_index)
{
    struct gen10_hevc_enc_context *pak_context;
    struct gen10_hevc_enc_state *hevc_state;
    VAEncPictureParameterBufferHEVC *pic_param;
    VAEncSliceParameterBufferHEVC *slice_param;
    gen10_hcp_slice_state_param param;
    int last_slice, slice_qp, qp_idx;

    pak_context = (struct gen10_hevc_enc_context *) encoder_context->mfc_context;
    hevc_state = (struct gen10_hevc_enc_state *)pak_context->enc_priv_state;

    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;
    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[slice_index]->buffer;

    memset(&param, 0, sizeof(param));

    param.dw1.slice_start_ctu_x = slice_param->slice_segment_address %
                                  pak_context->frame_info.width_in_lcu;
    param.dw1.slice_start_ctu_y = slice_param->slice_segment_address /
                                  pak_context->frame_info.width_in_lcu;

    if (slice_index == encode_state->num_slice_params_ext - 1) {
        param.dw2.next_slice_start_ctu_x = 0;
        param.dw2.next_slice_start_ctu_y = 0;

        last_slice = 1;
    } else {
        last_slice = slice_param->slice_segment_address + slice_param->num_ctu_in_slice;

        param.dw2.next_slice_start_ctu_x = last_slice %
                                           pak_context->frame_info.width_in_lcu;
        param.dw2.next_slice_start_ctu_y = last_slice /
                                           pak_context->frame_info.width_in_lcu;

        last_slice = 0;
    }

    param.dw3.slice_type = slice_param->slice_type;
    param.dw3.last_slice_flag = last_slice;
    param.dw3.slice_temporal_mvp_enabled = slice_param->slice_fields.bits.slice_temporal_mvp_enabled_flag;
    param.dw3.slice_qp = pic_param->pic_init_qp + slice_param->slice_qp_delta;
    param.dw3.slice_cb_qp_offset = slice_param->slice_cb_qp_offset;
    param.dw3.slice_cr_qp_offset = slice_param->slice_cr_qp_offset;

    param.dw4.deblocking_filter_disable = slice_param->slice_fields.bits.slice_deblocking_filter_disabled_flag;
    param.dw4.tc_offset_div2 = slice_param->slice_tc_offset_div2 & 0xf;
    param.dw4.beta_offset_div2 = slice_param->slice_beta_offset_div2 & 0xf;
    param.dw4.sao_chroma_flag = slice_param->slice_fields.bits.slice_sao_chroma_flag;
    param.dw4.sao_luma_flag = slice_param->slice_fields.bits.slice_sao_luma_flag;
    param.dw4.mvd_l1_zero_flag = slice_param->slice_fields.bits.mvd_l1_zero_flag;
    param.dw4.is_low_delay = slice_param->slice_type != HEVC_SLICE_B ? 1 : hevc_state->low_delay;
    param.dw4.collocated_from_l0_flag = slice_param->slice_fields.bits.collocated_from_l0_flag;
    param.dw4.chroma_log2_weight_denom = slice_param->luma_log2_weight_denom + slice_param->delta_chroma_log2_weight_denom;
    param.dw4.luma_log2_weight_denom = slice_param->luma_log2_weight_denom;
    param.dw4.cabac_init_flag = slice_param->slice_fields.bits.cabac_init_flag;
    param.dw4.max_merge_idx = slice_param->max_num_merge_cand - 1;

    if (pic_param->collocated_ref_pic_index != 0xFF)
        param.dw4.collocated_ref_idx = pic_param->collocated_ref_pic_index;

    param.dw6.round_intra = 10;
    param.dw6.round_inter = 4;

    param.dw7.cabac_zero_word_insertion_enabled = 1;
    param.dw7.emulation_byte_insert_enabled = 1;
    param.dw7.slice_data_enabled = 1;
    param.dw7.header_insertion_enabled = 1;

    if (pic_param->pic_fields.bits.transform_skip_enabled_flag) {
        slice_qp = pak_context->frame_info.slice_qp;

        if (slice_qp <= 22)
            qp_idx = 0;
        else if (slice_qp <= 27)
            qp_idx = 1;
        else if (slice_qp <= 32)
            qp_idx = 2;
        else
            qp_idx = 3;

        param.dw9.transform_skip_lambda = gen10_hevc_tr_lambda_coeffs[slice_qp];

        if (slice_param->slice_type == HEVC_SLICE_I) {
            param.dw10.transform_skip_zero_factor0 = gen10_hevc_tr_skip_coeffs[qp_idx][0][0][0][0];
            param.dw10.transform_skip_nonezero_factor0 = gen10_hevc_tr_skip_coeffs[qp_idx][0][0][1][0];
            param.dw10.transform_skip_zero_factor1 = gen10_hevc_tr_skip_coeffs[qp_idx][0][0][0][1] + 32;
            param.dw10.transform_skip_nonezero_factor1 = gen10_hevc_tr_skip_coeffs[qp_idx][0][0][1][1] + 32;
        } else {
            param.dw10.transform_skip_zero_factor0 = gen10_hevc_tr_skip_coeffs[qp_idx][1][0][0][0];
            param.dw10.transform_skip_nonezero_factor0 = gen10_hevc_tr_skip_coeffs[qp_idx][1][0][1][0];
            param.dw10.transform_skip_zero_factor1 = gen10_hevc_tr_skip_coeffs[qp_idx][1][0][0][1] + 32;
            param.dw10.transform_skip_nonezero_factor1 = gen10_hevc_tr_skip_coeffs[qp_idx][1][0][1][1] + 32;
        }
    }

    gen10_hcp_slice_state(ctx, batch, &param);
}

static void
gen10_hevc_pak_slice_level(VADriverContextP ctx,
                           struct encode_state *encode_state,
                           struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct gen10_hevc_enc_context *pak_context = encoder_context->mfc_context;
    struct gpe_mi_batch_buffer_start_parameter second_level_batch;
    VAEncSliceParameterBufferHEVC *slice_param;
    int slice_index;
    int i, j;

    slice_index = 0;
    for (i = 0; i < encode_state->num_slice_params_ext; i++) {
        for (j = 0; j < encode_state->slice_params_ext[i]->num_elements; j++) {
            slice_param = (VAEncSliceParameterBufferHEVC *)(encode_state->slice_params_ext[slice_index]->buffer);

            gen10_hevc_ref_idx_lists(ctx, encode_state, encoder_context, batch, slice_index);

            gen10_hevc_hcp_weightoffset(ctx, encode_state, encoder_context,
                                        batch, slice_index);

            gen10_hevc_hcp_slice_state(ctx, encode_state, encoder_context,
                                       batch, slice_index);

            if (slice_index == 0)
                gen10_hevc_enc_insert_packed_header(ctx, encode_state, encoder_context,
                                                    batch);

            gen10_hevc_enc_insert_slice_header(ctx, encode_state, encoder_context,
                                               batch, slice_index);


            memset(&second_level_batch, 0, sizeof(second_level_batch));
            second_level_batch.offset = 32 * slice_param->slice_segment_address;
            second_level_batch.is_second_level = 1;
            second_level_batch.bo = pak_context->res_mb_code_surface.bo;

            gen8_gpe_mi_batch_buffer_start(ctx, batch, &second_level_batch);

            slice_index++;
        }
    }
}

static void
gen10_hevc_read_mfc_status(VADriverContextP ctx, struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct gen10_hevc_enc_context *pak_context = encoder_context->mfc_context;
    struct gpe_mi_store_register_mem_parameter mi_store_reg_mem_param;
    struct gpe_mi_store_data_imm_parameter mi_store_data_imm_param;
    struct gpe_mi_flush_dw_parameter mi_flush_dw_param;
    struct gen10_hevc_enc_status_buffer *status_buffer;
    struct gen10_hevc_enc_state *hevc_state;
    int write_pak_idx;

    hevc_state = (struct gen10_hevc_enc_state *) pak_context->enc_priv_state;
    status_buffer = &pak_context->status_buffer;

    memset(&mi_flush_dw_param, 0, sizeof(mi_flush_dw_param));
    gen8_gpe_mi_flush_dw(ctx, batch, &mi_flush_dw_param);

    memset(&mi_store_reg_mem_param, 0, sizeof(mi_store_reg_mem_param));
    mi_store_reg_mem_param.bo = status_buffer->gpe_res.bo;
    mi_store_reg_mem_param.offset = status_buffer->status_bytes_per_frame_offset;
    mi_store_reg_mem_param.mmio_offset = status_buffer->mmio_bytes_per_frame_offset;
    gen8_gpe_mi_store_register_mem(ctx, batch, &mi_store_reg_mem_param);

    mi_store_reg_mem_param.bo = status_buffer->gpe_res.bo;
    mi_store_reg_mem_param.offset = status_buffer->status_image_mask_offset;
    mi_store_reg_mem_param.mmio_offset = status_buffer->mmio_image_mask_offset;
    gen8_gpe_mi_store_register_mem(ctx, batch, &mi_store_reg_mem_param);

    mi_store_reg_mem_param.bo = status_buffer->gpe_res.bo;
    mi_store_reg_mem_param.offset = status_buffer->status_image_ctrl_offset;
    mi_store_reg_mem_param.mmio_offset = status_buffer->mmio_image_ctrl_offset;
    gen8_gpe_mi_store_register_mem(ctx, batch, &mi_store_reg_mem_param);

    mi_store_reg_mem_param.bo = status_buffer->gpe_res.bo;
    mi_store_reg_mem_param.offset = status_buffer->status_qp_status_offset;
    mi_store_reg_mem_param.mmio_offset = status_buffer->mmio_qp_status_offset;
    gen8_gpe_mi_store_register_mem(ctx, batch, &mi_store_reg_mem_param);

    mi_store_reg_mem_param.bo = status_buffer->gpe_res.bo;
    mi_store_reg_mem_param.offset = status_buffer->status_bs_se_bitcount_offset;
    mi_store_reg_mem_param.mmio_offset = status_buffer->mmio_bs_se_bitcount_offset;
    gen8_gpe_mi_store_register_mem(ctx, batch, &mi_store_reg_mem_param);

    write_pak_idx = hevc_state->curr_pak_stat_index;
    mi_store_reg_mem_param.bo = pak_context->res_brc_pak_statistics_buffer[write_pak_idx].bo;
    mi_store_reg_mem_param.offset = offsetof(gen10_hevc_pak_stats_info, hcp_bs_frame);
    mi_store_reg_mem_param.mmio_offset = status_buffer->mmio_bytes_per_frame_offset;
    gen8_gpe_mi_store_register_mem(ctx, batch, &mi_store_reg_mem_param);

    mi_store_reg_mem_param.bo = pak_context->res_brc_pak_statistics_buffer[write_pak_idx].bo;
    mi_store_reg_mem_param.offset = offsetof(gen10_hevc_pak_stats_info, hcp_bs_frame_noheader);
    mi_store_reg_mem_param.mmio_offset = status_buffer->mmio_bs_frame_no_header_offset;
    gen8_gpe_mi_store_register_mem(ctx, batch, &mi_store_reg_mem_param);

    mi_store_reg_mem_param.bo = pak_context->res_brc_pak_statistics_buffer[write_pak_idx].bo;
    mi_store_reg_mem_param.offset = offsetof(gen10_hevc_pak_stats_info, hcp_image_status_control);
    mi_store_reg_mem_param.mmio_offset = status_buffer->mmio_image_ctrl_offset;
    gen8_gpe_mi_store_register_mem(ctx, batch, &mi_store_reg_mem_param);

    memset(&mi_store_data_imm_param, 0, sizeof(mi_store_data_imm_param));
    mi_store_data_imm_param.bo = pak_context->res_brc_pak_statistics_buffer[write_pak_idx].bo;
    mi_store_data_imm_param.offset = offsetof(gen10_hevc_pak_stats_info, hcp_image_status_ctl_last_pass);
    mi_store_data_imm_param.dw0 = hevc_state->curr_pak_idx;
    gen8_gpe_mi_store_data_imm(ctx, batch, &mi_store_data_imm_param);

    gen8_gpe_mi_flush_dw(ctx, batch, &mi_flush_dw_param);
}

static void
gen10_hevc_pak_brc_prepare(struct encode_state *encode_state,
                           struct intel_encoder_context *encoder_context)
{
    return;
}

static void
gen10_hevc_pak_context_destroy(void *context)
{
    return;
}

static VAStatus
gen10_hevc_pak_pipeline(VADriverContextP ctx,
                        VAProfile profile,
                        struct encode_state *encode_state,
                        struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct gen10_hevc_enc_context *pak_context = encoder_context->mfc_context;
    struct gen10_hevc_enc_status_buffer *status_buffer;
    struct gen10_hevc_enc_state *hevc_state;
    struct gpe_mi_conditional_batch_buffer_end_parameter mi_cond_end;
    struct gpe_mi_load_register_mem_parameter mi_load_reg_mem;
    struct gpe_mi_load_register_imm_parameter mi_load_reg_imm;
    int i;

    if (!pak_context || !pak_context->enc_priv_state)
        return VA_STATUS_ERROR_INVALID_CONTEXT;

    hevc_state = (struct gen10_hevc_enc_state *) pak_context->enc_priv_state;
    status_buffer = &pak_context->status_buffer;

    if (i965->intel.has_bsd2)
        intel_batchbuffer_start_atomic_bcs_override(batch, 0x1000, BSD_RING0);
    else
        intel_batchbuffer_start_atomic_bcs(batch, 0x1000);

    intel_batchbuffer_emit_mi_flush(batch);

    for (hevc_state->curr_pak_idx = 0;
         hevc_state->curr_pak_idx < hevc_state->num_pak_passes;
         hevc_state->curr_pak_idx++) {
        if (hevc_state->curr_pak_idx == 0) {
            memset(&mi_load_reg_imm, 0, sizeof(mi_load_reg_imm));
            mi_load_reg_imm.mmio_offset = status_buffer->mmio_image_ctrl_offset;
            mi_load_reg_imm.data = 0;
            gen8_gpe_mi_load_register_imm(ctx, batch, &mi_load_reg_imm);
        } else if (hevc_state->brc.brc_enabled) {
            memset(&mi_cond_end, 0, sizeof(mi_cond_end));
            mi_cond_end.offset = status_buffer->status_image_mask_offset;
            mi_cond_end.bo = status_buffer->gpe_res.bo;
            mi_cond_end.compare_data = 0;
            gen9_gpe_mi_conditional_batch_buffer_end(ctx, batch,
                                                     &mi_cond_end);

            memset(&mi_load_reg_mem, 0, sizeof(mi_load_reg_mem));
            mi_load_reg_mem.mmio_offset = status_buffer->mmio_image_ctrl_offset;
            mi_load_reg_mem.bo = status_buffer->gpe_res.bo;
            mi_load_reg_mem.offset = status_buffer->status_image_ctrl_offset;
            gen8_gpe_mi_load_register_mem(ctx, batch, &mi_load_reg_mem);
        }

        gen10_hevc_pak_picture_level(ctx, encode_state, encoder_context);
        gen10_hevc_pak_slice_level(ctx, encode_state, encoder_context);
        gen10_hevc_read_mfc_status(ctx, encoder_context);
    }

    intel_batchbuffer_end_atomic(batch);
    intel_batchbuffer_flush(batch);

    if (hevc_state->sao_2nd_needed) {
        if (i965->intel.has_bsd2)
            intel_batchbuffer_start_atomic_bcs_override(batch, 0x1000, BSD_RING0);
        else
            intel_batchbuffer_start_atomic_bcs(batch, 0x1000);

        intel_batchbuffer_emit_mi_flush(batch);

        BEGIN_BCS_BATCH(batch, 64);
        for (i = 0; i < 64; i++)
            OUT_BCS_BATCH(batch, MI_NOOP);

        ADVANCE_BCS_BATCH(batch);
        gen10_hevc_pak_picture_level(ctx, encode_state, encoder_context);
        gen10_hevc_pak_slice_level(ctx, encode_state, encoder_context);
        gen10_hevc_read_mfc_status(ctx, encoder_context);
        intel_batchbuffer_end_atomic(batch);
        intel_batchbuffer_flush(batch);
    }

    hevc_state->curr_pak_stat_index ^= 1;

    hevc_state->frame_number++;

    return VA_STATUS_SUCCESS;
}

static void
gen10_hevc_vme_context_destroy(void *context)
{
    struct gen10_hevc_enc_context *vme_context = context;
    int i;

    if (!vme_context)
        return;

    gen10_hevc_free_enc_resources(context);

    gen10_hevc_enc_free_common_resource(&vme_context->common_res);

    gen8_gpe_context_destroy(&vme_context->scaling_context.gpe_context);

    gen8_gpe_context_destroy(&vme_context->me_context.gpe_context);

    for (i = 0; i < GEN10_HEVC_BRC_NUM; i++)
        gen8_gpe_context_destroy(&vme_context->brc_context.gpe_contexts[i]);

    for (i = 0; i < GEN10_HEVC_MBENC_NUM; i++)
        gen8_gpe_context_destroy(&vme_context->mbenc_context.gpe_contexts[i]);

    if (vme_context->enc_priv_state)
        free(vme_context->enc_priv_state);

    free(vme_context);
}

Bool
gen10_hevc_vme_context_init(VADriverContextP ctx, struct intel_encoder_context *encoder_context)
{
    struct gen10_hevc_enc_context *vme_context = NULL;
    struct gen10_hevc_enc_state *hevc_state = NULL;

    vme_context = calloc(1, sizeof(struct gen10_hevc_enc_context));
    hevc_state = calloc(1, sizeof(struct gen10_hevc_enc_state));

    if (!vme_context || !hevc_state) {
        if (vme_context)
            free(vme_context);

        if (hevc_state)
            free(hevc_state);

        return false;
    }

    vme_context->enc_priv_state = hevc_state;

    gen10_hevc_vme_init_kernels_context(ctx, encoder_context, vme_context);

    hevc_state->use_hw_scoreboard = 1;
    hevc_state->use_hw_non_stalling_scoreboard = 0;
    hevc_state->num_regions_in_slice = 1;
    hevc_state->rdoq_enabled = 1;

    encoder_context->vme_context = vme_context;
    encoder_context->vme_pipeline = gen10_hevc_vme_pipeline;
    encoder_context->vme_context_destroy = gen10_hevc_vme_context_destroy;

    return true;
}

static VAStatus
gen10_hevc_get_coded_status(VADriverContextP ctx,
                            struct intel_encoder_context *encoder_context,
                            struct i965_coded_buffer_segment *coded_buf_seg)
{
    struct gen10_hevc_enc_status *enc_status;

    if (!encoder_context || !coded_buf_seg)
        return VA_STATUS_ERROR_INVALID_BUFFER;

    enc_status = (struct gen10_hevc_enc_status *)coded_buf_seg->codec_private_data;
    coded_buf_seg->base.size = enc_status->bytes_per_frame;

    return VA_STATUS_SUCCESS;
}

Bool
gen10_hevc_pak_context_init(VADriverContextP ctx, struct intel_encoder_context *encoder_context)
{
    struct gen10_hevc_enc_context *pak_context = encoder_context->vme_context;

    if (!pak_context)
        return false;

    encoder_context->mfc_context = pak_context;
    encoder_context->mfc_context_destroy = gen10_hevc_pak_context_destroy;
    encoder_context->mfc_pipeline = gen10_hevc_pak_pipeline;
    encoder_context->mfc_brc_prepare = gen10_hevc_pak_brc_prepare;
    encoder_context->get_status = gen10_hevc_get_coded_status;

    return true;
}
