/*
 * Copyright © 2014 Intel Corporation
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
 *    Xiang Haihao <haihao.xiang@intel.com>
 *
 */

#include "sysdeps.h"

#include <va/va.h>
#include <va/va_dec_hevc.h>

#include "intel_batchbuffer.h"
#include "intel_driver.h"
#include "i965_defines.h"
#include "i965_drv_video.h"
#include "i965_decoder_utils.h"

#include "gen9_mfd.h"
#include "intel_media.h"

#define OUT_BUFFER(buf_bo, is_target, ma)  do {                         \
        if (buf_bo) {                                                   \
            OUT_BCS_RELOC(batch,                                        \
                          buf_bo,                                       \
                          I915_GEM_DOMAIN_RENDER,                       \
                          is_target ? I915_GEM_DOMAIN_RENDER : 0,       \
                          0);                                           \
        } else {                                                        \
            OUT_BCS_BATCH(batch, 0);                                    \
        }                                                               \
        OUT_BCS_BATCH(batch, 0);                                        \
        if (ma)                                                         \
            OUT_BCS_BATCH(batch, 0);                                    \
    } while (0)

#define OUT_BUFFER_MA_TARGET(buf_bo)       OUT_BUFFER(buf_bo, 1, 1)
#define OUT_BUFFER_MA_REFERENCE(buf_bo)    OUT_BUFFER(buf_bo, 0, 1)
#define OUT_BUFFER_NMA_TARGET(buf_bo)      OUT_BUFFER(buf_bo, 1, 0)
#define OUT_BUFFER_NMA_REFERENCE(buf_bo)   OUT_BUFFER(buf_bo, 0, 0)

static void
gen9_hcpd_init_hevc_surface(VADriverContextP ctx,
                            VAPictureParameterBufferHEVC *pic_param,
                            struct object_surface *obj_surface,
                            struct gen9_hcpd_context *gen9_hcpd_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    GenHevcSurface *gen9_hevc_surface;

    if (!obj_surface)
        return;

    obj_surface->free_private_data = gen_free_hevc_surface;
    gen9_hevc_surface = obj_surface->private_data;

    if (!gen9_hevc_surface) {
        gen9_hevc_surface = calloc(sizeof(GenHevcSurface), 1);
        assert(gen9_hevc_surface);
        gen9_hevc_surface->base.frame_store_id = -1;
        obj_surface->private_data = gen9_hevc_surface;
    }

    if (gen9_hevc_surface->motion_vector_temporal_bo == NULL) {
        uint32_t size;

        if (gen9_hcpd_context->ctb_size == 16)
            size = ((gen9_hcpd_context->picture_width_in_pixels + 63) >> 6) *
                ((gen9_hcpd_context->picture_height_in_pixels + 15) >> 4);
        else
            size = ((gen9_hcpd_context->picture_width_in_pixels + 31) >> 5) *
                ((gen9_hcpd_context->picture_height_in_pixels + 31) >> 5);

        size <<= 6; /* in unit of 64bytes */
        gen9_hevc_surface->motion_vector_temporal_bo = dri_bo_alloc(i965->intel.bufmgr,
                                                                    "motion vector temporal buffer",
                                                                    size,
                                                                    0x1000);
    }
}

static VAStatus
gen9_hcpd_hevc_decode_init(VADriverContextP ctx,
                           struct decode_state *decode_state,
                           struct gen9_hcpd_context *gen9_hcpd_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    VAPictureParameterBufferHEVC *pic_param;
    struct object_surface *obj_surface;
    uint32_t size;

    assert(decode_state->pic_param && decode_state->pic_param->buffer);
    pic_param = (VAPictureParameterBufferHEVC *)decode_state->pic_param->buffer;
    intel_update_hevc_frame_store_index(ctx,
                                        decode_state,
                                        pic_param,
                                        gen9_hcpd_context->reference_surfaces,
                                        &gen9_hcpd_context->fs_ctx);

    gen9_hcpd_context->picture_width_in_pixels = pic_param->pic_width_in_luma_samples;
    gen9_hcpd_context->picture_height_in_pixels = pic_param->pic_height_in_luma_samples;
    gen9_hcpd_context->ctb_size = (1 << (pic_param->log2_min_luma_coding_block_size_minus3 +
                                         3 +
                                         pic_param->log2_diff_max_min_luma_coding_block_size));
    gen9_hcpd_context->picture_width_in_ctbs = ALIGN(gen9_hcpd_context->picture_width_in_pixels, gen9_hcpd_context->ctb_size) / gen9_hcpd_context->ctb_size;
    gen9_hcpd_context->picture_height_in_ctbs = ALIGN(gen9_hcpd_context->picture_height_in_pixels, gen9_hcpd_context->ctb_size) / gen9_hcpd_context->ctb_size;
    gen9_hcpd_context->min_cb_size = (1 << (pic_param->log2_min_luma_coding_block_size_minus3 + 3));
    gen9_hcpd_context->picture_width_in_min_cb_minus1 = gen9_hcpd_context->picture_width_in_pixels / gen9_hcpd_context->min_cb_size - 1;
    gen9_hcpd_context->picture_height_in_min_cb_minus1 = gen9_hcpd_context->picture_height_in_pixels / gen9_hcpd_context->min_cb_size - 1;

    /* Current decoded picture */
    obj_surface = decode_state->render_object;
    hevc_ensure_surface_bo(ctx, decode_state, obj_surface, pic_param);
    gen9_hcpd_init_hevc_surface(ctx, pic_param, obj_surface, gen9_hcpd_context);

    size = ALIGN(gen9_hcpd_context->picture_width_in_pixels, 32) >> 3;
    size <<= 6;
    ALLOC_GEN_BUFFER((&gen9_hcpd_context->deblocking_filter_line_buffer), "line buffer", size);
    ALLOC_GEN_BUFFER((&gen9_hcpd_context->deblocking_filter_tile_line_buffer), "tile line buffer", size);

    size = ALIGN(gen9_hcpd_context->picture_height_in_pixels + 6 * gen9_hcpd_context->picture_height_in_ctbs, 32) >> 3;
    size <<= 6;
    ALLOC_GEN_BUFFER((&gen9_hcpd_context->deblocking_filter_tile_column_buffer), "tile column buffer", size);

    size = (((gen9_hcpd_context->picture_width_in_pixels + 15) >> 4) * 188 + 9 * gen9_hcpd_context->picture_width_in_ctbs + 1023) >> 9;
    size <<= 6;
    ALLOC_GEN_BUFFER((&gen9_hcpd_context->metadata_line_buffer), "metadata line buffer", size);

    size = (((gen9_hcpd_context->picture_width_in_pixels + 15) >> 4) * 172 + 9 * gen9_hcpd_context->picture_width_in_ctbs + 1023) >> 9;
    size <<= 6;
    ALLOC_GEN_BUFFER((&gen9_hcpd_context->metadata_tile_line_buffer), "metadata tile line buffer", size);

    if (IS_CHERRYVIEW(i965->intel.device_info))
        size = (((gen9_hcpd_context->picture_height_in_pixels + 15) >> 4) * 256 + 9 * gen9_hcpd_context->picture_height_in_ctbs + 1023) >> 9;
    else
        size = (((gen9_hcpd_context->picture_height_in_pixels + 15) >> 4) * 176 + 89 * gen9_hcpd_context->picture_height_in_ctbs + 1023) >> 9;
    size <<= 6;
    ALLOC_GEN_BUFFER((&gen9_hcpd_context->metadata_tile_column_buffer), "metadata tile column buffer", size);

    size = ALIGN(((gen9_hcpd_context->picture_width_in_pixels >> 1) + 3 * gen9_hcpd_context->picture_width_in_ctbs), 16) >> 3;
    size <<= 6;
    ALLOC_GEN_BUFFER((&gen9_hcpd_context->sao_line_buffer), "sao line buffer", size);

    size = ALIGN(((gen9_hcpd_context->picture_width_in_pixels >> 1) + 6 * gen9_hcpd_context->picture_width_in_ctbs), 16) >> 3;
    size <<= 6;
    ALLOC_GEN_BUFFER((&gen9_hcpd_context->sao_tile_line_buffer), "sao tile line buffer", size);

    size = ALIGN(((gen9_hcpd_context->picture_height_in_pixels >> 1) + 6 * gen9_hcpd_context->picture_height_in_ctbs), 16) >> 3;
    size <<= 6;
    ALLOC_GEN_BUFFER((&gen9_hcpd_context->sao_tile_column_buffer), "sao tile column buffer", size);

    gen9_hcpd_context->first_inter_slice_collocated_ref_idx = 0;
    gen9_hcpd_context->first_inter_slice_collocated_from_l0_flag = 0;
    gen9_hcpd_context->first_inter_slice_valid = 0;

    return VA_STATUS_SUCCESS;
}

static void
gen9_hcpd_pipe_mode_select(VADriverContextP ctx,
                           struct decode_state *decode_state,
                           int codec,
                           struct gen9_hcpd_context *gen9_hcpd_context)
{
    struct intel_batchbuffer *batch = gen9_hcpd_context->base.batch;

    assert(codec == HCP_CODEC_HEVC);

    BEGIN_BCS_BATCH(batch, 4);

    OUT_BCS_BATCH(batch, HCP_PIPE_MODE_SELECT | (4 - 2));
    OUT_BCS_BATCH(batch,
                  (codec << 5) |
                  (0 << 3) | /* disable Pic Status / Error Report */
                  HCP_CODEC_SELECT_DECODE);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_hcpd_surface_state(VADriverContextP ctx,
                        struct decode_state *decode_state,
                        struct gen9_hcpd_context *gen9_hcpd_context)
{
    struct intel_batchbuffer *batch = gen9_hcpd_context->base.batch;
    struct object_surface *obj_surface = decode_state->render_object;
    unsigned int y_cb_offset;

    assert(obj_surface);

    y_cb_offset = obj_surface->y_cb_offset;

    BEGIN_BCS_BATCH(batch, 3);

    OUT_BCS_BATCH(batch, HCP_SURFACE_STATE | (3 - 2));
    OUT_BCS_BATCH(batch,
                  (0 << 28) |                   /* surface id */
                  (obj_surface->width - 1));    /* pitch - 1 */
    OUT_BCS_BATCH(batch,
                  (SURFACE_FORMAT_PLANAR_420_8 << 28) |
                  y_cb_offset);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_hcpd_pipe_buf_addr_state(VADriverContextP ctx,
                              struct decode_state *decode_state,
                              struct gen9_hcpd_context *gen9_hcpd_context)
{
    struct intel_batchbuffer *batch = gen9_hcpd_context->base.batch;
    struct object_surface *obj_surface;
    GenHevcSurface *gen9_hevc_surface;
    int i;

    BEGIN_BCS_BATCH(batch, 95);

    OUT_BCS_BATCH(batch, HCP_PIPE_BUF_ADDR_STATE | (95 - 2));

    obj_surface = decode_state->render_object;
    assert(obj_surface && obj_surface->bo);
    gen9_hevc_surface = obj_surface->private_data;
    assert(gen9_hevc_surface && gen9_hevc_surface->motion_vector_temporal_bo);

    OUT_BUFFER_MA_TARGET(obj_surface->bo); /* DW 1..3 */
    OUT_BUFFER_MA_TARGET(gen9_hcpd_context->deblocking_filter_line_buffer.bo);/* DW 4..6 */
    OUT_BUFFER_MA_TARGET(gen9_hcpd_context->deblocking_filter_tile_line_buffer.bo); /* DW 7..9 */
    OUT_BUFFER_MA_TARGET(gen9_hcpd_context->deblocking_filter_tile_column_buffer.bo); /* DW 10..12 */
    OUT_BUFFER_MA_TARGET(gen9_hcpd_context->metadata_line_buffer.bo);         /* DW 13..15 */
    OUT_BUFFER_MA_TARGET(gen9_hcpd_context->metadata_tile_line_buffer.bo);    /* DW 16..18 */
    OUT_BUFFER_MA_TARGET(gen9_hcpd_context->metadata_tile_column_buffer.bo);  /* DW 19..21 */
    OUT_BUFFER_MA_TARGET(gen9_hcpd_context->sao_line_buffer.bo);              /* DW 22..24 */
    OUT_BUFFER_MA_TARGET(gen9_hcpd_context->sao_tile_line_buffer.bo);         /* DW 25..27 */
    OUT_BUFFER_MA_TARGET(gen9_hcpd_context->sao_tile_column_buffer.bo);       /* DW 28..30 */
    OUT_BUFFER_MA_TARGET(gen9_hevc_surface->motion_vector_temporal_bo); /* DW 31..33 */
    OUT_BUFFER_MA_TARGET(NULL); /* DW 34..36, reserved */

    for (i = 0; i < ARRAY_ELEMS(gen9_hcpd_context->reference_surfaces); i++) {
        obj_surface = gen9_hcpd_context->reference_surfaces[i].obj_surface;

        if (obj_surface)
            OUT_BUFFER_NMA_REFERENCE(obj_surface->bo);
        else
            OUT_BUFFER_NMA_REFERENCE(NULL);
    }
    OUT_BCS_BATCH(batch, 0);    /* DW 53, memory address attributes */

    OUT_BUFFER_MA_REFERENCE(NULL); /* DW 54..56, ignore for decoding mode */
    OUT_BUFFER_MA_TARGET(NULL);
    OUT_BUFFER_MA_TARGET(NULL);
    OUT_BUFFER_MA_TARGET(NULL);

    for (i = 0; i < ARRAY_ELEMS(gen9_hcpd_context->reference_surfaces); i++) {
        obj_surface = gen9_hcpd_context->reference_surfaces[i].obj_surface;
        gen9_hevc_surface = NULL;

        if (obj_surface && obj_surface->private_data)
            gen9_hevc_surface = obj_surface->private_data;

        if (gen9_hevc_surface)
            OUT_BUFFER_NMA_REFERENCE(gen9_hevc_surface->motion_vector_temporal_bo);
        else
            OUT_BUFFER_NMA_REFERENCE(NULL);
    }
    OUT_BCS_BATCH(batch, 0);    /* DW 82, memory address attributes */

    OUT_BUFFER_MA_TARGET(NULL);    /* DW 83..85, ignore for HEVC */
    OUT_BUFFER_MA_TARGET(NULL);    /* DW 86..88, ignore for HEVC */
    OUT_BUFFER_MA_TARGET(NULL);    /* DW 89..91, ignore for HEVC */
    OUT_BUFFER_MA_TARGET(NULL);    /* DW 92..94, ignore for HEVC */

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_hcpd_ind_obj_base_addr_state(VADriverContextP ctx,
                                  dri_bo *slice_data_bo,
                                  struct gen9_hcpd_context *gen9_hcpd_context)
{
    struct intel_batchbuffer *batch = gen9_hcpd_context->base.batch;

    BEGIN_BCS_BATCH(batch, 14);

    OUT_BCS_BATCH(batch, HCP_IND_OBJ_BASE_ADDR_STATE | (14 - 2));
    OUT_BUFFER_MA_REFERENCE(slice_data_bo);        /* DW 1..3 */
    OUT_BUFFER_NMA_REFERENCE(NULL);                /* DW 4..5, Upper Bound */
    OUT_BUFFER_MA_REFERENCE(NULL);                 /* DW 6..8, CU, ignored */
    OUT_BUFFER_MA_TARGET(NULL);                    /* DW 9..11, PAK-BSE, ignored */
    OUT_BUFFER_NMA_TARGET(NULL);                   /* DW 12..13, Upper Bound  */

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_hcpd_qm_state(VADriverContextP ctx,
                   int size_id,
                   int color_component,
                   int pred_type,
                   int dc,
                   unsigned char *qm,
                   int qm_length,
                   struct gen9_hcpd_context *gen9_hcpd_context)
{
    struct intel_batchbuffer *batch = gen9_hcpd_context->base.batch;
    unsigned char qm_buffer[64];

    assert(qm_length <= 64);
    memset(qm_buffer, 0, sizeof(qm_buffer));
    memcpy(qm_buffer, qm, qm_length);

    BEGIN_BCS_BATCH(batch, 18);

    OUT_BCS_BATCH(batch, HCP_QM_STATE | (18 - 2));
    OUT_BCS_BATCH(batch,
                  dc << 5 |
                  color_component << 3 |
                  size_id << 1 |
                  pred_type);
    intel_batchbuffer_data(batch, qm_buffer, 64);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_hcpd_hevc_qm_state(VADriverContextP ctx,
                        struct decode_state *decode_state,
                        struct gen9_hcpd_context *gen9_hcpd_context)
{
    VAIQMatrixBufferHEVC *iq_matrix;
    VAPictureParameterBufferHEVC *pic_param;
    int i;

    if (decode_state->iq_matrix && decode_state->iq_matrix->buffer)
        iq_matrix = (VAIQMatrixBufferHEVC *)decode_state->iq_matrix->buffer;
    else
        iq_matrix = &gen9_hcpd_context->iq_matrix_hevc;

    assert(decode_state->pic_param && decode_state->pic_param->buffer);
    pic_param = (VAPictureParameterBufferHEVC *)decode_state->pic_param->buffer;

    if (!pic_param->pic_fields.bits.scaling_list_enabled_flag)
        iq_matrix = &gen9_hcpd_context->iq_matrix_hevc;

    for (i = 0; i < 6; i++) {
        gen9_hcpd_qm_state(ctx,
                           0, i % 3, i / 3, 0,
                           iq_matrix->ScalingList4x4[i], 16,
                           gen9_hcpd_context);
    }

    for (i = 0; i < 6; i++) {
        gen9_hcpd_qm_state(ctx,
                           1, i % 3, i / 3, 0,
                           iq_matrix->ScalingList8x8[i], 64,
                           gen9_hcpd_context);
    }

    for (i = 0; i < 6; i++) {
        gen9_hcpd_qm_state(ctx,
                           2, i % 3, i / 3, iq_matrix->ScalingListDC16x16[i],
                           iq_matrix->ScalingList16x16[i], 64,
                           gen9_hcpd_context);
    }

    for (i = 0; i < 2; i++) {
        gen9_hcpd_qm_state(ctx,
                           3, 0, i % 2, iq_matrix->ScalingListDC32x32[i],
                           iq_matrix->ScalingList32x32[i], 64,
                           gen9_hcpd_context);
    }
}

static void
gen9_hcpd_pic_state(VADriverContextP ctx,
                    struct decode_state *decode_state,
                    struct gen9_hcpd_context *gen9_hcpd_context)
{
    struct intel_batchbuffer *batch = gen9_hcpd_context->base.batch;
    VAPictureParameterBufferHEVC *pic_param;
    int max_pcm_size_minus3 = 0, min_pcm_size_minus3 = 0;
    int pcm_sample_bit_depth_luma_minus1 = 7, pcm_sample_bit_depth_chroma_minus1 = 7;
    /*
     * 7.4.3.1
     *
     * When not present, the value of loop_filter_across_tiles_enabled_flag
     * is inferred to be equal to 1.
     */
    int loop_filter_across_tiles_enabled_flag = 1;

    assert(decode_state->pic_param && decode_state->pic_param->buffer);
    pic_param = (VAPictureParameterBufferHEVC *)decode_state->pic_param->buffer;

    if (pic_param->pic_fields.bits.pcm_enabled_flag) {
        max_pcm_size_minus3 = pic_param->log2_min_pcm_luma_coding_block_size_minus3 +
            pic_param->log2_diff_max_min_pcm_luma_coding_block_size;
        min_pcm_size_minus3 = pic_param->log2_min_pcm_luma_coding_block_size_minus3;
        pcm_sample_bit_depth_luma_minus1 = (pic_param->pcm_sample_bit_depth_luma_minus1 & 0x0f);
        pcm_sample_bit_depth_chroma_minus1 = (pic_param->pcm_sample_bit_depth_chroma_minus1 & 0x0f);
    } else {
        max_pcm_size_minus3 = MIN(pic_param->log2_min_luma_coding_block_size_minus3 + pic_param->log2_diff_max_min_luma_coding_block_size, 2);
    }

    if (pic_param->pic_fields.bits.tiles_enabled_flag)
        loop_filter_across_tiles_enabled_flag = pic_param->pic_fields.bits.loop_filter_across_tiles_enabled_flag;

    BEGIN_BCS_BATCH(batch, 19);

    OUT_BCS_BATCH(batch, HCP_PIC_STATE | (19 - 2));

    OUT_BCS_BATCH(batch,
                  gen9_hcpd_context->picture_height_in_min_cb_minus1 << 16 |
                  gen9_hcpd_context->picture_width_in_min_cb_minus1);
    OUT_BCS_BATCH(batch,
                  max_pcm_size_minus3 << 10 |
                  min_pcm_size_minus3 << 8 |
                  (pic_param->log2_min_transform_block_size_minus2 +
                   pic_param->log2_diff_max_min_transform_block_size) << 6 |
                  pic_param->log2_min_transform_block_size_minus2 << 4 |
                  (pic_param->log2_min_luma_coding_block_size_minus3 +
                   pic_param->log2_diff_max_min_luma_coding_block_size) << 2 |
                  pic_param->log2_min_luma_coding_block_size_minus3);
    OUT_BCS_BATCH(batch, 0); /* DW 3, ignored */
    OUT_BCS_BATCH(batch,
                  0 << 27 |
                  pic_param->pic_fields.bits.strong_intra_smoothing_enabled_flag << 26 |
                  pic_param->pic_fields.bits.transquant_bypass_enabled_flag << 25 |
                  pic_param->pic_fields.bits.amp_enabled_flag << 23 |
                  pic_param->pic_fields.bits.transform_skip_enabled_flag << 22 |
                  !(pic_param->CurrPic.flags & VA_PICTURE_HEVC_BOTTOM_FIELD) << 21 |
                  !!(pic_param->CurrPic.flags & VA_PICTURE_HEVC_FIELD_PIC) << 20 |
                  pic_param->pic_fields.bits.weighted_pred_flag << 19 |
                  pic_param->pic_fields.bits.weighted_bipred_flag << 18 |
                  pic_param->pic_fields.bits.tiles_enabled_flag << 17 |
                  pic_param->pic_fields.bits.entropy_coding_sync_enabled_flag << 16 |
                  loop_filter_across_tiles_enabled_flag << 15 |
                  pic_param->pic_fields.bits.sign_data_hiding_enabled_flag << 13 |
                  pic_param->log2_parallel_merge_level_minus2 << 10 |
                  pic_param->pic_fields.bits.constrained_intra_pred_flag << 9 |
                  pic_param->pic_fields.bits.pcm_loop_filter_disabled_flag << 8 |
                  (pic_param->diff_cu_qp_delta_depth & 0x03) << 6 |
                  pic_param->pic_fields.bits.cu_qp_delta_enabled_flag << 5 |
                  pic_param->pic_fields.bits.pcm_enabled_flag << 4 |
                  pic_param->slice_parsing_fields.bits.sample_adaptive_offset_enabled_flag << 3 |
                  0);
    OUT_BCS_BATCH(batch,
                  pcm_sample_bit_depth_luma_minus1 << 20 |
                  pcm_sample_bit_depth_chroma_minus1 << 16 |
                  pic_param->max_transform_hierarchy_depth_inter << 13 |
                  pic_param->max_transform_hierarchy_depth_intra << 10 |
                  (pic_param->pps_cr_qp_offset & 0x1f) << 5 |
                  (pic_param->pps_cb_qp_offset & 0x1f));
    OUT_BCS_BATCH(batch,
                  0 << 29 |
                  0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0); /* DW 10 */
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0); /* DW 15 */
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_hcpd_tile_state(VADriverContextP ctx,
                     struct decode_state *decode_state,
                     struct gen9_hcpd_context *gen9_hcpd_context)
{
    struct intel_batchbuffer *batch = gen9_hcpd_context->base.batch;
    VAPictureParameterBufferHEVC *pic_param;
    uint8_t pos_col[20], pos_row[24];
    int i;

    assert(decode_state->pic_param && decode_state->pic_param->buffer);
    pic_param = (VAPictureParameterBufferHEVC *)decode_state->pic_param->buffer;

    memset(pos_col, 0, sizeof(pos_col));
    memset(pos_row, 0, sizeof(pos_row));

    for (i = 0; i <= MIN(pic_param->num_tile_columns_minus1, 18); i++)
        pos_col[i + 1] = pos_col[i] + pic_param->column_width_minus1[i] + 1;

    for (i = 0; i <= MIN(pic_param->num_tile_rows_minus1, 20); i++)
        pos_row[i + 1] = pos_row[i] + pic_param->row_height_minus1[i] + 1;

    BEGIN_BCS_BATCH(batch, 13);

    OUT_BCS_BATCH(batch, HCP_TILE_STATE | (13 - 2));

    OUT_BCS_BATCH(batch,
                  pic_param->num_tile_columns_minus1 << 5 |
                  pic_param->num_tile_rows_minus1);
    intel_batchbuffer_data(batch, pos_col, 20);
    intel_batchbuffer_data(batch, pos_row, 24);

    ADVANCE_BCS_BATCH(batch);
}

static int
gen9_hcpd_get_reference_picture_frame_id(VAPictureHEVC *ref_pic,
                                         GenFrameStore frame_store[MAX_GEN_HCP_REFERENCE_FRAMES])
{
    int i;

    if (ref_pic->picture_id == VA_INVALID_ID ||
        (ref_pic->flags & VA_PICTURE_HEVC_INVALID))
        return 0;

    for (i = 0; i < MAX_GEN_HCP_REFERENCE_FRAMES; i++) {
        if (ref_pic->picture_id == frame_store[i].surface_id) {
            assert(frame_store[i].frame_store_id < MAX_GEN_HCP_REFERENCE_FRAMES);
            return frame_store[i].frame_store_id;
        }
    }

    /* Should never get here !!! */
    assert(0);
    return 0;
}

static void
gen9_hcpd_ref_idx_state_1(struct intel_batchbuffer *batch,
                          int list,
                          VAPictureParameterBufferHEVC *pic_param,
                          VASliceParameterBufferHEVC *slice_param,
                          GenFrameStore frame_store[MAX_GEN_HCP_REFERENCE_FRAMES])
{
    int i;
    uint8_t num_ref_minus1 = (list ? slice_param->num_ref_idx_l1_active_minus1 : slice_param->num_ref_idx_l0_active_minus1);
    uint8_t *ref_list = slice_param->RefPicList[list];

    BEGIN_BCS_BATCH(batch, 18);

    OUT_BCS_BATCH(batch, HCP_REF_IDX_STATE | (18 - 2));
    OUT_BCS_BATCH(batch,
                  num_ref_minus1 << 1 |
                  list);

    for (i = 0; i < 16; i++) {
        if (i < MIN((num_ref_minus1 + 1), 15)) {
            VAPictureHEVC *ref_pic = &pic_param->ReferenceFrames[ref_list[i]];
            VAPictureHEVC *curr_pic = &pic_param->CurrPic;

            OUT_BCS_BATCH(batch,
                          !(ref_pic->flags & VA_PICTURE_HEVC_BOTTOM_FIELD) << 15 |
                          !!(ref_pic->flags & VA_PICTURE_HEVC_FIELD_PIC) << 14 |
                          !!(ref_pic->flags & VA_PICTURE_HEVC_LONG_TERM_REFERENCE) << 13 |
                          0 << 12 |
                          0 << 11 |
                          gen9_hcpd_get_reference_picture_frame_id(ref_pic, frame_store) << 8 |
                          (CLAMP(-128, 127, curr_pic->pic_order_cnt - ref_pic->pic_order_cnt) & 0xff));
        } else {
            OUT_BCS_BATCH(batch, 0);
        }
    }

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_hcpd_ref_idx_state(VADriverContextP ctx,
                        VAPictureParameterBufferHEVC *pic_param,
                        VASliceParameterBufferHEVC *slice_param,
                        struct gen9_hcpd_context *gen9_hcpd_context)
{
    struct intel_batchbuffer *batch = gen9_hcpd_context->base.batch;

    if (slice_param->LongSliceFlags.fields.slice_type == HEVC_SLICE_I)
        return;

    gen9_hcpd_ref_idx_state_1(batch, 0, pic_param, slice_param, gen9_hcpd_context->reference_surfaces);

    if (slice_param->LongSliceFlags.fields.slice_type == HEVC_SLICE_P)
        return;

    gen9_hcpd_ref_idx_state_1(batch, 1, pic_param, slice_param, gen9_hcpd_context->reference_surfaces);
}

static void
gen9_hcpd_weightoffset_state_1(struct intel_batchbuffer *batch,
                               int list,
                               VASliceParameterBufferHEVC *slice_param)
{
    int i;
    uint8_t num_ref_minus1 = (list == 1) ? slice_param->num_ref_idx_l1_active_minus1 : slice_param->num_ref_idx_l0_active_minus1;
    int8_t *luma_offset = (list == 1) ? slice_param->luma_offset_l1 : slice_param->luma_offset_l0;
    int8_t *delta_luma_weight = (list == 1) ? slice_param->delta_luma_weight_l1 : slice_param->delta_luma_weight_l0;
    int8_t (* chroma_offset)[2] = (list == 1) ? slice_param->ChromaOffsetL1 : slice_param->ChromaOffsetL0;
    int8_t (* delta_chroma_weight)[2] = (list == 1) ? slice_param->delta_chroma_weight_l1 : slice_param->delta_chroma_weight_l0;

    BEGIN_BCS_BATCH(batch, 34);

    OUT_BCS_BATCH(batch, HCP_WEIGHTOFFSET | (34 - 2));
    OUT_BCS_BATCH(batch, list);

    for (i = 0; i < 16; i++) {
        if (i < MIN((num_ref_minus1 + 1), 15)) {
            OUT_BCS_BATCH(batch,
                          (luma_offset[i] & 0xff) << 8 |
                          (delta_luma_weight[i] & 0xff));
        } else {
            OUT_BCS_BATCH(batch, 0);
        }
    }
    for (i = 0; i < 16; i++) {
        if (i < MIN((num_ref_minus1 + 1), 15)) {
            OUT_BCS_BATCH(batch,
                          (chroma_offset[i][1] & 0xff) << 24 |
                          (delta_chroma_weight[i][1] & 0xff) << 16 |
                          (chroma_offset[i][0] & 0xff) << 8 |
                          (delta_chroma_weight[i][0] & 0xff));
        } else {
            OUT_BCS_BATCH(batch, 0);
        }
    }

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_hcpd_weightoffset_state(VADriverContextP ctx,
                             VAPictureParameterBufferHEVC *pic_param,
                             VASliceParameterBufferHEVC *slice_param,
                             struct gen9_hcpd_context *gen9_hcpd_context)
{
    struct intel_batchbuffer *batch = gen9_hcpd_context->base.batch;

    if (slice_param->LongSliceFlags.fields.slice_type == HEVC_SLICE_I)
        return;

    if ((slice_param->LongSliceFlags.fields.slice_type == HEVC_SLICE_P &&
         !pic_param->pic_fields.bits.weighted_pred_flag) ||
        (slice_param->LongSliceFlags.fields.slice_type == HEVC_SLICE_B &&
         !pic_param->pic_fields.bits.weighted_bipred_flag))
        return;

    gen9_hcpd_weightoffset_state_1(batch, 0, slice_param);

    if (slice_param->LongSliceFlags.fields.slice_type == HEVC_SLICE_P)
        return;

    gen9_hcpd_weightoffset_state_1(batch, 1, slice_param);
}

static int
gen9_hcpd_get_collocated_ref_idx(VADriverContextP ctx,
                                 VAPictureParameterBufferHEVC *pic_param,
                                 VASliceParameterBufferHEVC *slice_param,
                                 struct gen9_hcpd_context *gen9_hcpd_context)
{
    uint8_t *ref_list;
    VAPictureHEVC *ref_pic;

    if (slice_param->collocated_ref_idx > 14)
        return 0;

    if (!slice_param->LongSliceFlags.fields.slice_temporal_mvp_enabled_flag)
        return 0;

    if (slice_param->LongSliceFlags.fields.slice_type == HEVC_SLICE_I)
        return 0;

    if (slice_param->LongSliceFlags.fields.slice_type == HEVC_SLICE_P ||
        (slice_param->LongSliceFlags.fields.slice_type == HEVC_SLICE_B &&
         slice_param->LongSliceFlags.fields.collocated_from_l0_flag))
        ref_list = slice_param->RefPicList[0];
    else {
        assert(slice_param->LongSliceFlags.fields.slice_type == HEVC_SLICE_B);
        ref_list = slice_param->RefPicList[1];
    }

    ref_pic = &pic_param->ReferenceFrames[ref_list[slice_param->collocated_ref_idx]];

    return gen9_hcpd_get_reference_picture_frame_id(ref_pic, gen9_hcpd_context->reference_surfaces);
}

static int
gen9_hcpd_is_list_low_delay(uint8_t ref_list_count,
                            uint8_t ref_list[15],
                            VAPictureHEVC *curr_pic,
                            VAPictureHEVC ref_surfaces[15])
{
    int i;

    for (i = 0; i < MIN(ref_list_count, 15); i++) {
        VAPictureHEVC *ref_pic;

        if (ref_list[i] > 14)
            continue;

        ref_pic = &ref_surfaces[ref_list[i]];

        if (ref_pic->pic_order_cnt > curr_pic->pic_order_cnt)
            return 0;
    }

    return 1;
}

static int
gen9_hcpd_is_low_delay(VADriverContextP ctx,
                       VAPictureParameterBufferHEVC *pic_param,
                       VASliceParameterBufferHEVC *slice_param)
{
    if (slice_param->LongSliceFlags.fields.slice_type == HEVC_SLICE_I)
        return 0;
    else if (slice_param->LongSliceFlags.fields.slice_type == HEVC_SLICE_P)
        return gen9_hcpd_is_list_low_delay(slice_param->num_ref_idx_l0_active_minus1 + 1,
                                           slice_param->RefPicList[0],
                                           &pic_param->CurrPic,
                                           pic_param->ReferenceFrames);
    else
        return gen9_hcpd_is_list_low_delay(slice_param->num_ref_idx_l0_active_minus1 + 1,
                                           slice_param->RefPicList[0],
                                           &pic_param->CurrPic,
                                           pic_param->ReferenceFrames) &&
            gen9_hcpd_is_list_low_delay(slice_param->num_ref_idx_l1_active_minus1 + 1,
                                        slice_param->RefPicList[1],
                                        &pic_param->CurrPic,
                                        pic_param->ReferenceFrames);
}

static void
gen9_hcpd_slice_state(VADriverContextP ctx,
                      VAPictureParameterBufferHEVC *pic_param,
                      VASliceParameterBufferHEVC *slice_param,
                      VASliceParameterBufferHEVC *next_slice_param,
                      struct gen9_hcpd_context *gen9_hcpd_context)
{
    struct intel_batchbuffer *batch = gen9_hcpd_context->base.batch;
    int slice_hor_pos, slice_ver_pos, next_slice_hor_pos, next_slice_ver_pos;
    unsigned short collocated_ref_idx, collocated_from_l0_flag;

    slice_hor_pos = slice_param->slice_segment_address % gen9_hcpd_context->picture_width_in_ctbs;
    slice_ver_pos = slice_param->slice_segment_address / gen9_hcpd_context->picture_width_in_ctbs;

    if (next_slice_param) {
        next_slice_hor_pos = next_slice_param->slice_segment_address % gen9_hcpd_context->picture_width_in_ctbs;
        next_slice_ver_pos = next_slice_param->slice_segment_address / gen9_hcpd_context->picture_width_in_ctbs;
    } else {
        next_slice_hor_pos = 0;
        next_slice_ver_pos = 0;
    }

    collocated_ref_idx = gen9_hcpd_get_collocated_ref_idx(ctx, pic_param, slice_param, gen9_hcpd_context);
    collocated_from_l0_flag = slice_param->LongSliceFlags.fields.collocated_from_l0_flag;

    if ((!gen9_hcpd_context->first_inter_slice_valid) &&
        (slice_param->LongSliceFlags.fields.slice_type != HEVC_SLICE_I) &&
        slice_param->LongSliceFlags.fields.slice_temporal_mvp_enabled_flag) {
        gen9_hcpd_context->first_inter_slice_collocated_ref_idx = collocated_ref_idx;
        gen9_hcpd_context->first_inter_slice_collocated_from_l0_flag = collocated_from_l0_flag;
        gen9_hcpd_context->first_inter_slice_valid = 1;
    }

    /* HW requirement */
    if (gen9_hcpd_context->first_inter_slice_valid &&
        ((slice_param->LongSliceFlags.fields.slice_type == HEVC_SLICE_I) ||
         (!slice_param->LongSliceFlags.fields.slice_temporal_mvp_enabled_flag))) {
        collocated_ref_idx = gen9_hcpd_context->first_inter_slice_collocated_ref_idx;
        collocated_from_l0_flag = gen9_hcpd_context->first_inter_slice_collocated_from_l0_flag;
    }

    BEGIN_BCS_BATCH(batch, 9);

    OUT_BCS_BATCH(batch, HCP_SLICE_STATE | (9 - 2));

    OUT_BCS_BATCH(batch,
                  slice_ver_pos << 16 |
                  slice_hor_pos);
    OUT_BCS_BATCH(batch,
                  next_slice_ver_pos << 16 |
                  next_slice_hor_pos);
    OUT_BCS_BATCH(batch,
                  (slice_param->slice_cr_qp_offset & 0x1f) << 17 |
                  (slice_param->slice_cb_qp_offset & 0x1f) << 12 |
                  (pic_param->init_qp_minus26 + 26 + slice_param->slice_qp_delta) << 6 |
                  slice_param->LongSliceFlags.fields.slice_temporal_mvp_enabled_flag << 5 |
                  slice_param->LongSliceFlags.fields.dependent_slice_segment_flag << 4 |
                  !next_slice_param << 2 |
                  slice_param->LongSliceFlags.fields.slice_type);
    OUT_BCS_BATCH(batch,
                  collocated_ref_idx << 26 |
                  (5 - slice_param->five_minus_max_num_merge_cand - 1) << 23 |
                  slice_param->LongSliceFlags.fields.cabac_init_flag << 22 |
                  slice_param->luma_log2_weight_denom << 19 |
                  ((slice_param->luma_log2_weight_denom + slice_param->delta_chroma_log2_weight_denom) & 0x7) << 16 |
                  collocated_from_l0_flag << 15 |
                  gen9_hcpd_is_low_delay(ctx, pic_param, slice_param) << 14 |
                  slice_param->LongSliceFlags.fields.mvd_l1_zero_flag << 13 |
                  slice_param->LongSliceFlags.fields.slice_sao_luma_flag << 12 |
                  slice_param->LongSliceFlags.fields.slice_sao_chroma_flag << 11 |
                  slice_param->LongSliceFlags.fields.slice_loop_filter_across_slices_enabled_flag << 10 |
                  (slice_param->slice_beta_offset_div2 & 0xf) << 5 |
                  (slice_param->slice_tc_offset_div2 & 0xf) << 1 |
                  slice_param->LongSliceFlags.fields.slice_deblocking_filter_disabled_flag);
    OUT_BCS_BATCH(batch,
                  slice_param->slice_data_byte_offset); /* DW 5 */
    OUT_BCS_BATCH(batch,
                  0 << 26 |
                  0 << 20 |
                  0);
    OUT_BCS_BATCH(batch, 0);    /* Ignored for decoding */
    OUT_BCS_BATCH(batch, 0);    /* Ignored for decoding */

    ADVANCE_BCS_BATCH(batch);
}

static void
gen9_hcpd_bsd_object(VADriverContextP ctx,
                     VASliceParameterBufferHEVC *slice_param,
                     struct gen9_hcpd_context *gen9_hcpd_context)
{
    struct intel_batchbuffer *batch = gen9_hcpd_context->base.batch;

    BEGIN_BCS_BATCH(batch, 3);

    OUT_BCS_BATCH(batch, HCP_BSD_OBJECT | (3 - 2));

    OUT_BCS_BATCH(batch, slice_param->slice_data_size);
    OUT_BCS_BATCH(batch, slice_param->slice_data_offset);

    ADVANCE_BCS_BATCH(batch);
}

static VAStatus
gen9_hcpd_hevc_decode_picture(VADriverContextP ctx,
                              struct decode_state *decode_state,
                              struct gen9_hcpd_context *gen9_hcpd_context)
{
    VAStatus vaStatus;
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct intel_batchbuffer *batch = gen9_hcpd_context->base.batch;
    VAPictureParameterBufferHEVC *pic_param;
    VASliceParameterBufferHEVC *slice_param, *next_slice_param, *next_slice_group_param;
    dri_bo *slice_data_bo;
    int i, j;

    vaStatus = gen9_hcpd_hevc_decode_init(ctx, decode_state, gen9_hcpd_context);

    if (vaStatus != VA_STATUS_SUCCESS)
        goto out;

    assert(decode_state->pic_param && decode_state->pic_param->buffer);
    pic_param = (VAPictureParameterBufferHEVC *)decode_state->pic_param->buffer;

    if (i965->intel.has_bsd2)
        intel_batchbuffer_start_atomic_bcs_override(batch, 0x1000, BSD_RING0);
    else
        intel_batchbuffer_start_atomic_bcs(batch, 0x1000);
    intel_batchbuffer_emit_mi_flush(batch);

    gen9_hcpd_pipe_mode_select(ctx, decode_state, HCP_CODEC_HEVC, gen9_hcpd_context);
    gen9_hcpd_surface_state(ctx, decode_state, gen9_hcpd_context);
    gen9_hcpd_pipe_buf_addr_state(ctx, decode_state, gen9_hcpd_context);
    gen9_hcpd_hevc_qm_state(ctx, decode_state, gen9_hcpd_context);
    gen9_hcpd_pic_state(ctx, decode_state, gen9_hcpd_context);

    if (pic_param->pic_fields.bits.tiles_enabled_flag)
        gen9_hcpd_tile_state(ctx, decode_state, gen9_hcpd_context);

    /* Need to double it works or not if the two slice groups have differenct slice data buffers */
    for (j = 0; j < decode_state->num_slice_params; j++) {
        assert(decode_state->slice_params && decode_state->slice_params[j]->buffer);
        slice_param = (VASliceParameterBufferHEVC *)decode_state->slice_params[j]->buffer;
        slice_data_bo = decode_state->slice_datas[j]->bo;

        gen9_hcpd_ind_obj_base_addr_state(ctx, slice_data_bo, gen9_hcpd_context);

        if (j == decode_state->num_slice_params - 1)
            next_slice_group_param = NULL;
        else
            next_slice_group_param = (VASliceParameterBufferHEVC *)decode_state->slice_params[j + 1]->buffer;

        for (i = 0; i < decode_state->slice_params[j]->num_elements; i++) {
            if (i < decode_state->slice_params[j]->num_elements - 1)
                next_slice_param = slice_param + 1;
            else
                next_slice_param = next_slice_group_param;

            gen9_hcpd_slice_state(ctx, pic_param, slice_param, next_slice_param, gen9_hcpd_context);
            gen9_hcpd_ref_idx_state(ctx, pic_param, slice_param, gen9_hcpd_context);
            gen9_hcpd_weightoffset_state(ctx, pic_param, slice_param, gen9_hcpd_context);
            gen9_hcpd_bsd_object(ctx, slice_param, gen9_hcpd_context);
            slice_param++;
        }
    }

    intel_batchbuffer_end_atomic(batch);
    intel_batchbuffer_flush(batch);

out:
    return vaStatus;
}

static VAStatus
gen9_hcpd_decode_picture(VADriverContextP ctx,
                         VAProfile profile,
                         union codec_state *codec_state,
                         struct hw_context *hw_context)
{
    struct gen9_hcpd_context *gen9_hcpd_context = (struct gen9_hcpd_context *)hw_context;
    struct decode_state *decode_state = &codec_state->decode;
    VAStatus vaStatus;

    assert(gen9_hcpd_context);

    vaStatus = intel_decoder_sanity_check_input(ctx, profile, decode_state);

    if (vaStatus != VA_STATUS_SUCCESS)
        goto out;

    switch (profile) {
    case VAProfileHEVCMain:
    case VAProfileHEVCMain10:
        vaStatus = gen9_hcpd_hevc_decode_picture(ctx, decode_state, gen9_hcpd_context);
        break;

    default:
        /* should never get here 1!! */
        assert(0);
        break;
    }

out:
    return vaStatus;
}

static void
gen9_hcpd_context_destroy(void *hw_context)
{
    struct gen9_hcpd_context *gen9_hcpd_context = (struct gen9_hcpd_context *)hw_context;

    FREE_GEN_BUFFER((&gen9_hcpd_context->deblocking_filter_line_buffer));
    FREE_GEN_BUFFER((&gen9_hcpd_context->deblocking_filter_tile_line_buffer));
    FREE_GEN_BUFFER((&gen9_hcpd_context->deblocking_filter_tile_column_buffer));
    FREE_GEN_BUFFER((&gen9_hcpd_context->metadata_line_buffer));
    FREE_GEN_BUFFER((&gen9_hcpd_context->metadata_tile_line_buffer));
    FREE_GEN_BUFFER((&gen9_hcpd_context->metadata_tile_column_buffer));
    FREE_GEN_BUFFER((&gen9_hcpd_context->sao_line_buffer));
    FREE_GEN_BUFFER((&gen9_hcpd_context->sao_tile_line_buffer));
    FREE_GEN_BUFFER((&gen9_hcpd_context->sao_tile_column_buffer));

    intel_batchbuffer_free(gen9_hcpd_context->base.batch);
    free(gen9_hcpd_context);
}

static void
gen9_hcpd_hevc_context_init(VADriverContextP ctx,
                            struct gen9_hcpd_context *gen9_hcpd_context)
{
    hevc_gen_default_iq_matrix(&gen9_hcpd_context->iq_matrix_hevc);
}

static struct hw_context *
gen9_hcpd_context_init(VADriverContextP ctx, struct object_config *object_config)
{
    struct intel_driver_data *intel = intel_driver_data(ctx);
    struct gen9_hcpd_context *gen9_hcpd_context = calloc(1, sizeof(struct gen9_hcpd_context));
    int i;

    if (!gen9_hcpd_context)
        return NULL;

    gen9_hcpd_context->base.destroy = gen9_hcpd_context_destroy;
    gen9_hcpd_context->base.run = gen9_hcpd_decode_picture;
    gen9_hcpd_context->base.batch = intel_batchbuffer_new(intel, I915_EXEC_VEBOX, 0);

    for (i = 0; i < ARRAY_ELEMS(gen9_hcpd_context->reference_surfaces); i++) {
        gen9_hcpd_context->reference_surfaces[i].surface_id = VA_INVALID_ID;
        gen9_hcpd_context->reference_surfaces[i].frame_store_id = -1;
        gen9_hcpd_context->reference_surfaces[i].obj_surface = NULL;
    }

    switch (object_config->profile) {
    case VAProfileHEVCMain:
    case VAProfileHEVCMain10:
        gen9_hcpd_hevc_context_init(ctx, gen9_hcpd_context);
        break;

    default:
        break;
    }

    return (struct hw_context *)gen9_hcpd_context;
}

struct hw_context *
gen9_dec_hw_context_init(VADriverContextP ctx, struct object_config *obj_config)
{
    if (obj_config->profile == VAProfileHEVCMain ||
        obj_config->profile == VAProfileHEVCMain10) {
        return gen9_hcpd_context_init(ctx, obj_config);
    } else {
        return gen8_dec_hw_context_init(ctx, obj_config);
    }
}
