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
 *    Peng Chen <peng.c.chen@intel.com>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "intel_batchbuffer.h"
#include "i965_defines.h"
#include "i965_drv_video.h"
#include "gen10_hcp_common.h"

#define HCP_WRITE_COMMANDS(command_flag)                        \
    {                                                           \
        int cmd_size = sizeof(*param) / sizeof(uint32_t);       \
        BEGIN_BCS_BATCH(batch, cmd_size + 1);                   \
        OUT_BCS_BATCH(batch, (command_flag) | (cmd_size - 1));  \
        intel_batchbuffer_data(batch, param, sizeof(*param));   \
        ADVANCE_BCS_BATCH(batch);                               \
    }

void
gen10_hcp_pipe_mode_select(VADriverContextP ctx,
                           struct intel_batchbuffer *batch,
                           gen10_hcp_pipe_mode_select_param *param)
{
    HCP_WRITE_COMMANDS(HCP_PIPE_MODE_SELECT);
}

void
gen10_hcp_surface_state(VADriverContextP ctx,
                        struct intel_batchbuffer *batch,
                        gen10_hcp_surface_state_param *param)
{
    HCP_WRITE_COMMANDS(HCP_SURFACE_STATE);
}

void
gen10_hcp_pic_state(VADriverContextP ctx,
                    struct intel_batchbuffer *batch,
                    gen10_hcp_pic_state_param *param)
{
    HCP_WRITE_COMMANDS(HCP_PIC_STATE);
}

void
gen10_hcp_vp9_pic_state(VADriverContextP ctx,
                        struct intel_batchbuffer *batch,
                        gen10_hcp_vp9_pic_state_param *param)
{
    HCP_WRITE_COMMANDS(HCP_VP9_PIC_STATE);
}

void
gen10_hcp_qm_state(VADriverContextP ctx,
                   struct intel_batchbuffer *batch,
                   gen10_hcp_qm_state_param *param)
{
    HCP_WRITE_COMMANDS(HCP_QM_STATE);
}


void
gen10_hcp_fqm_state(VADriverContextP ctx,
                    struct intel_batchbuffer *batch,
                    gen10_hcp_fqm_state_param *param)
{
    HCP_WRITE_COMMANDS(HCP_FQM_STATE);
}

void
gen10_hcp_rdoq_state(VADriverContextP ctx,
                     struct intel_batchbuffer *batch,
                     gen10_hcp_rdoq_state_param *param)
{
    HCP_WRITE_COMMANDS(HCP_RDOQ_STATE);
}

void
gen10_hcp_weightoffset_state(VADriverContextP ctx,
                             struct intel_batchbuffer *batch,
                             gen10_hcp_weightoffset_state_param *param)
{
    HCP_WRITE_COMMANDS(HCP_WEIGHTOFFSET);
}

void
gen10_hcp_slice_state(VADriverContextP ctx,
                      struct intel_batchbuffer *batch,
                      gen10_hcp_slice_state_param *param)
{
    HCP_WRITE_COMMANDS(HCP_SLICE_STATE);
}

void
gen10_hcp_ref_idx_state(VADriverContextP ctx,
                        struct intel_batchbuffer *batch,
                        gen10_hcp_ref_idx_state_param *param)
{
    HCP_WRITE_COMMANDS(HCP_REF_IDX_STATE);
}

void
gen10_hcp_vp9_segment_state(VADriverContextP ctx,
                            struct intel_batchbuffer *batch,
                            gen10_hcp_vp9_segment_state_param *param)
{
    HCP_WRITE_COMMANDS(HCP_VP9_SEGMENT_STATE);
}

void
gen10_hcp_pak_insert_object(VADriverContextP ctx,
                            struct intel_batchbuffer *batch,
                            gen10_hcp_pak_insert_object_param *param)
{
    int payload_bits = param->inline_payload_bits;
    int cmd_size_in_dw = ALIGN(payload_bits, 32) >> 5;

    BEGIN_BCS_BATCH(batch, cmd_size_in_dw + 2);

    OUT_BCS_BATCH(batch, HCP_INSERT_PAK_OBJECT | (cmd_size_in_dw));

    OUT_BCS_BATCH(batch, param->dw1.value);
    intel_batchbuffer_data(batch, param->inline_payload_ptr,
                           cmd_size_in_dw * 4);

    ADVANCE_BCS_BATCH(batch);
}

#define OUT_BUFFER_2DW(batch, gpe_res, is_target, delta)  do {              \
        if (gpe_res) {                                                      \
            struct i965_gpe_resource * res = gpe_res;                       \
            dri_bo *bo = res->bo;                                           \
            if (bo) {                                                       \
                OUT_BCS_RELOC64(batch,                                      \
                                bo,                                         \
                                I915_GEM_DOMAIN_RENDER,                     \
                                is_target ? I915_GEM_DOMAIN_RENDER : 0,     \
                                delta);                                     \
            }                                                               \
            else {                                                          \
              OUT_BCS_BATCH(batch, 0);                                      \
              OUT_BCS_BATCH(batch, 0);                                      \
            }                                                               \
        } else {                                                            \
            OUT_BCS_BATCH(batch, 0);                                        \
            OUT_BCS_BATCH(batch, 0);                                        \
        }                                                                   \
    } while (0)

#define OUT_BUFFER_3DW(batch, gpe_res, is_target, delta)        do { \
        OUT_BUFFER_2DW(batch, gpe_res, is_target, delta);            \
        if (gpe_res)                                                 \
            OUT_BCS_BATCH(batch, i965->intel.mocs_state);            \
        else                                                         \
            OUT_BCS_BATCH(batch, 0);                                 \
    } while (0)

void
gen10_hcp_pipe_buf_addr_state(VADriverContextP ctx,
                              struct intel_batchbuffer *batch,
                              gen10_hcp_pipe_buf_addr_state_param *param)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    int i = 0;

    BEGIN_BCS_BATCH(batch, 104);

    OUT_BCS_BATCH(batch, HCP_PIPE_BUF_ADDR_STATE | (104 - 2));

    /* DW1..3 */
    OUT_BUFFER_3DW(batch, param->reconstructed,
                   1, 0);

    /* DW4..6 */
    OUT_BUFFER_3DW(batch, param->deblocking_filter_line,
                   1, 0);

    /* DW7..9 */
    OUT_BUFFER_3DW(batch, param->deblocking_filter_tile_line,
                   1, 0);

    /* DW10..12 */
    OUT_BUFFER_3DW(batch, param->deblocking_filter_tile_column,
                   1, 0);

    /* DW13..15 */
    OUT_BUFFER_3DW(batch, param->metadata_line,
                   1, 0);

    /* DW16..18 */
    OUT_BUFFER_3DW(batch, param->metadata_tile_line,
                   1, 0);

    /* DW19..21 */
    OUT_BUFFER_3DW(batch, param->metadata_tile_column,
                   1, 0);

    /* DW 22..24 */
    OUT_BUFFER_3DW(batch, param->sao_line,
                   1, 0);

    /* DW 25..27 */
    OUT_BUFFER_3DW(batch, param->sao_tile_line,
                   1, 0);

    /* DW 28..30 */
    OUT_BUFFER_3DW(batch, param->sao_tile_column,
                   1, 0);

    /* DW 31..33 */
    OUT_BUFFER_3DW(batch, param->current_motion_vector_temporal,
                   1, 0);

    /* DW 34..36 */
    OUT_BUFFER_3DW(batch, NULL, 0, 0);

    /* DW 37..52 */
    for (i = 0; i < 8; i++)
        OUT_BUFFER_2DW(batch, param->reference_picture[i],
                       0, 0);

    OUT_BCS_BATCH(batch, i965->intel.mocs_state);

    /* DW 54..56 */
    OUT_BUFFER_3DW(batch, param->uncompressed_picture,
                   0, 0);

    /* DW 57..59 */
    OUT_BUFFER_3DW(batch, param->streamout_data_destination,
                   1, 0);

    /* DW 60..62 */
    OUT_BUFFER_3DW(batch, param->picture_status,
                   1, 0);

    /* DW 63..65 */
    OUT_BUFFER_3DW(batch, param->ildb_streamout,
                   1, 0);

    /* DW 66..81 */
    for (i = 0; i < 8; i++)
        OUT_BUFFER_2DW(batch, param->collocated_motion_vector_temporal[i],
                       0, 0);

    OUT_BCS_BATCH(batch, i965->intel.mocs_state);

    /* DW 83..85 */
    OUT_BUFFER_3DW(batch, param->vp9_probability,
                   1, 0);

    /* DW 86..88 */
    OUT_BUFFER_3DW(batch, param->vp9_segmentid,
                   1, 0);

    /* DW 89..91 */
    OUT_BUFFER_3DW(batch, param->vp9_hvd_line_rowstore,
                   1, 0);

    /* DW 92..94 */
    OUT_BUFFER_3DW(batch, param->vp9_hvd_time_rowstore,
                   1, 0);

    /* DW 95..97 */
    OUT_BUFFER_3DW(batch, param->sao_streamout_data_destination,
                   1, 0);

    /* DW 98..100 */
    OUT_BUFFER_3DW(batch, param->frame_statics_streamout_data_destination,
                   1, 0);

    /* DW 101..103. */
    OUT_BUFFER_3DW(batch, param->sse_source_pixel_rowstore,
                   1, 0);

    ADVANCE_BCS_BATCH(batch);
}

void
gen10_hcp_ind_obj_base_addr_state(VADriverContextP ctx,
                                  struct intel_batchbuffer *batch,
                                  gen10_hcp_ind_obj_base_addr_state_param *param)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);

    BEGIN_BCS_BATCH(batch, 29);

    OUT_BCS_BATCH(batch, HCP_IND_OBJ_BASE_ADDR_STATE | (29 - 2));

    /* DW 1..5 */
    OUT_BUFFER_3DW(batch, NULL, 0, 0);
    OUT_BUFFER_2DW(batch, NULL, 0, 0);

    /* DW 6..8 */
    OUT_BUFFER_3DW(batch,
                   param->ind_cu_obj_bse,
                   0,
                   param->ind_cu_obj_bse_offset);

    /* DW 9..13 */
    OUT_BUFFER_3DW(batch,
                   param->ind_pak_bse,
                   1,
                   param->ind_pak_bse_offset);

    OUT_BUFFER_2DW(batch,
                   param->ind_pak_bse,
                   1,
                   param->ind_pak_bse_upper);

    /* DW 14..16 */
    OUT_BUFFER_3DW(batch, NULL, 0, 0);

    /* DW 17..19 */
    OUT_BUFFER_3DW(batch, NULL, 0, 0);

    /* DW 20..22 */
    OUT_BUFFER_3DW(batch, NULL, 0, 0);

    /* DW 23..28 */
    OUT_BUFFER_3DW(batch, NULL, 0, 0);
    OUT_BUFFER_3DW(batch, NULL, 0, 0);

    ADVANCE_BCS_BATCH(batch);
}
