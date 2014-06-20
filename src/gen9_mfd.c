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
    VASliceParameterBufferHEVC *slice_param;
    struct object_surface *obj_surface;
    uint32_t size;
    int i, j, has_inter = 0;

    for (j = 0; j < decode_state->num_slice_params && !has_inter; j++) {
        assert(decode_state->slice_params && decode_state->slice_params[j]->buffer);
        slice_param = (VASliceParameterBufferHEVC *)decode_state->slice_params[j]->buffer;

        for (i = 0; i < decode_state->slice_params[j]->num_elements; i++) {
            if (slice_param->LongSliceFlags.fields.slice_type == HEVC_SLICE_B ||
                slice_param->LongSliceFlags.fields.slice_type == HEVC_SLICE_P) {
                has_inter = 1;
                break;
            }

            slice_param++;
        }
    }

    assert(decode_state->pic_param && decode_state->pic_param->buffer);
    pic_param = (VAPictureParameterBufferHEVC *)decode_state->pic_param->buffer;

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
    gen9_hcpd_init_hevc_surface(ctx, pic_param, obj_surface, gen9_hcpd_context);

    size = ALIGN(gen9_hcpd_context->picture_width_in_pixels, 32) >> 3;
    size <<= 6;
    ALLOC_GEN_BUFFER((&gen9_hcpd_context->deblocking_filter_line_buffer), "line buffer", size);
    ALLOC_GEN_BUFFER((&gen9_hcpd_context->deblocking_filter_tile_line_buffer), "tile line buffer", size);

    size = ALIGN(gen9_hcpd_context->picture_height_in_pixels + 6 * gen9_hcpd_context->picture_height_in_ctbs, 32) >> 3;
    size <<= 6;
    ALLOC_GEN_BUFFER((&gen9_hcpd_context->deblocking_filter_tile_column_buffer), "tile column buffer", size);

    if (has_inter) {
        size = (((gen9_hcpd_context->picture_width_in_pixels + 15) >> 4) * 188 + 9 * gen9_hcpd_context->picture_width_in_ctbs + 1023) >> 9;
        size <<= 6;
        ALLOC_GEN_BUFFER((&gen9_hcpd_context->metadata_line_buffer), "metadata line buffer", size);

        size = (((gen9_hcpd_context->picture_width_in_pixels + 15) >> 4) * 172 + 9 * gen9_hcpd_context->picture_width_in_ctbs + 1023) >> 9;
        size <<= 6;
        ALLOC_GEN_BUFFER((&gen9_hcpd_context->metadata_tile_line_buffer), "metadata tile line buffer", size);

        size = (((gen9_hcpd_context->picture_height_in_pixels + 15) >> 4) * 176 + 89 * gen9_hcpd_context->picture_width_in_ctbs + 1023) >> 9;
        size <<= 6;
        ALLOC_GEN_BUFFER((&gen9_hcpd_context->metadata_tile_column_buffer), "metadata tile column buffer", size);
    } else {
        size = (gen9_hcpd_context->picture_width_in_pixels + 8 * gen9_hcpd_context->picture_width_in_ctbs + 1023) >> 9;
        size <<= 6;
        ALLOC_GEN_BUFFER((&gen9_hcpd_context->metadata_line_buffer), "metadata line buffer", size);

        size = (gen9_hcpd_context->picture_width_in_pixels + 16 * gen9_hcpd_context->picture_width_in_ctbs + 1023) >> 9;
        size <<= 6;
        ALLOC_GEN_BUFFER((&gen9_hcpd_context->metadata_tile_line_buffer), "metadata tile line buffer", size);

        size = (gen9_hcpd_context->picture_height_in_pixels + 8 * gen9_hcpd_context->picture_height_in_ctbs + 1023) >> 9;
        size <<= 6;
        ALLOC_GEN_BUFFER((&gen9_hcpd_context->metadata_tile_column_buffer), "metadata tile column buffer", size);
    }

    size = ALIGN(((gen9_hcpd_context->picture_width_in_pixels >> 1) + 3 * gen9_hcpd_context->picture_width_in_ctbs), 16) >> 3;
    size <<= 6;
    ALLOC_GEN_BUFFER((&gen9_hcpd_context->sao_line_buffer), "sao line buffer", size);

    size = ALIGN(((gen9_hcpd_context->picture_width_in_pixels >> 1) + 6 * gen9_hcpd_context->picture_width_in_ctbs), 16) >> 3;
    size <<= 6;
    ALLOC_GEN_BUFFER((&gen9_hcpd_context->sao_tile_line_buffer), "sao tile line buffer", size);

    size = ALIGN(((gen9_hcpd_context->picture_height_in_pixels >> 1) + 6 * gen9_hcpd_context->picture_height_in_ctbs), 16) >> 3;
    size <<= 6;
    ALLOC_GEN_BUFFER((&gen9_hcpd_context->sao_tile_column_buffer), "sao tile column buffer", size);

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

static VAStatus
gen9_hcpd_hevc_decode_picture(VADriverContextP ctx,
                              struct decode_state *decode_state,
                              struct gen9_hcpd_context *gen9_hcpd_context)
{
    VAStatus vaStatus;
    struct intel_batchbuffer *batch = gen9_hcpd_context->base.batch;

    vaStatus = gen9_hcpd_hevc_decode_init(ctx, decode_state, gen9_hcpd_context);

    if (vaStatus != VA_STATUS_SUCCESS)
        goto out;

    intel_batchbuffer_start_atomic_bcs(batch, 0x1000);
    intel_batchbuffer_emit_mi_flush(batch);

    gen9_hcpd_pipe_mode_select(ctx, decode_state, HCP_CODEC_HEVC, gen9_hcpd_context);
    gen9_hcpd_surface_state(ctx, decode_state, gen9_hcpd_context);

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
