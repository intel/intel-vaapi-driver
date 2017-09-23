/*
 * Copyright © 2018 Intel Corporation
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
#include "gen10_vdenc_common.h"

#define VDENC_WRITE_COMMANDS(command_flag, batch, param)        \
    {                                                           \
        int cmd_size = sizeof(*param) / sizeof(uint32_t);       \
        BEGIN_BCS_BATCH(batch, cmd_size + 1);                   \
        OUT_BCS_BATCH(batch, (command_flag) | (cmd_size - 1));  \
        intel_batchbuffer_data(batch, param, sizeof(*param));   \
        ADVANCE_BCS_BATCH(batch);                               \
    }

void
gen10_vdenc_vd_pipeline_flush(VADriverContextP ctx,
                              struct intel_batchbuffer *batch,
                              gen10_vdenc_vd_pipeline_flush_param *param)
{
    VDENC_WRITE_COMMANDS(VD_PIPELINE_FLUSH, batch, param);
}

void
gen10_vdenc_pipe_mode_select(VADriverContextP ctx,
                             struct intel_batchbuffer *batch,
                             gen10_vdenc_pipe_mode_select_param *param)
{
    VDENC_WRITE_COMMANDS(VDENC_PIPE_MODE_SELECT, batch, param);
}

void
gen10_vdenc_surface_state(VADriverContextP ctx,
                          struct intel_batchbuffer *batch,
                          enum GEN10_VDENC_SURFACE_TYPE type,
                          gen10_vdenc_surface_state_param *surface0,
                          gen10_vdenc_surface_state_param *surface1)
{
    uint32_t dw0 = 0;
    int cmd_size = 0;

    cmd_size = 1 + sizeof(gen10_vdenc_surface_state_param) / sizeof(uint32_t);
    if (type == GEN10_VDENC_DS_REF_SURFACE) {
        cmd_size *= 2;

        dw0 = VDENC_DS_REF_SURFACE_STATE;
    } else if (type == GEN10_VDENC_REF_SURFACE)
        dw0 = VDENC_REF_SURFACE_STATE;
    else
        dw0 = VDENC_SRC_SURFACE_STATE;

    dw0 |= (cmd_size - 1);

    BEGIN_BCS_BATCH(batch, cmd_size + 1);

    OUT_BCS_BATCH(batch, dw0);

    OUT_BCS_BATCH(batch, 0);

    intel_batchbuffer_data(batch, surface0, sizeof(*surface0));
    if (type == GEN10_VDENC_DS_REF_SURFACE)
        intel_batchbuffer_data(batch, surface1, sizeof(*surface1));

    ADVANCE_BCS_BATCH(batch);
}

void
gen10_vdenc_walker_state(VADriverContextP ctx,
                         struct intel_batchbuffer *batch,
                         gen10_vdenc_walker_state_param *param)
{
    VDENC_WRITE_COMMANDS(VDENC_WALKER_STATE, batch, param);
}

void
gen10_vdenc_weightsoffsets_state(VADriverContextP ctx,
                                 struct intel_batchbuffer *batch,
                                 gen10_vdenc_weightsoffsets_state_param *param)
{
    VDENC_WRITE_COMMANDS(VDENC_WEIGHTSOFFSETS_STATE, batch, param);
}

#define OUT_BUFFER_2DW(batch, bo, is_target, delta)  do {               \
        if (bo) {                                                       \
            OUT_BCS_RELOC64(batch,                                      \
                            bo,                                         \
                            I915_GEM_DOMAIN_RENDER,                     \
                            is_target ? I915_GEM_DOMAIN_RENDER : 0,     \
                            delta);                                     \
        } else {                                                        \
            OUT_BCS_BATCH(batch, 0);                                    \
            OUT_BCS_BATCH(batch, 0);                                    \
        }                                                               \
    } while (0)

#define OUT_BUFFER_3DW(batch, bo, is_target, delta)        do { \
        OUT_BUFFER_2DW(batch, bo, is_target, delta);            \
        if (bo)                                                 \
            OUT_BCS_BATCH(batch, i965->intel.mocs_state);       \
        else                                                    \
            OUT_BCS_BATCH(batch, 0);                            \
    } while (0)

void
gen10_vdenc_pipe_buf_addr_state(VADriverContextP ctx,
                                struct intel_batchbuffer *batch,
                                gen10_vdenc_pipe_buf_addr_state_param *param)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    int i;

    BEGIN_BCS_BATCH(batch, 62);

    OUT_BCS_BATCH(batch, VDENC_PIPE_BUF_ADDR_STATE | (62 - 2));

    /* DW1..9 */
    for (i = 0; i < 2; i++)
        OUT_BUFFER_3DW(batch, param->downscaled_fwd_ref[i]->bo,
                       0, 0);

    OUT_BUFFER_3DW(batch, param->downscaled_bwd_ref[0]->bo,
                   0, 0);

    /* DW10..12 */
    OUT_BUFFER_3DW(batch, param->uncompressed_picture->bo,
                   0, 0);

    /* DW13..15 */
    OUT_BUFFER_3DW(batch, param->stream_data_picture->bo,
                   0, 0);

    /* DW16..18 */
    OUT_BUFFER_3DW(batch, param->row_store_scratch_buf->bo,
                   1, 0);

    /* DW19..21 */
    OUT_BUFFER_3DW(batch, param->collocated_mv_buf->bo,
                   1, 0);

    /* DW22..33 */
    for (i = 0; i < 3; i++)
        OUT_BUFFER_3DW(batch, param->fwd_ref[i]->bo,
                       0, 0);

    OUT_BUFFER_3DW(batch, param->bwd_ref[0]->bo,
                   0, 0);

    /* DW34..36 */
    OUT_BUFFER_3DW(batch, param->statictics_streamout_buf->bo,
                   1, 0);

    /* DW37..42 */
    for (i = 0; i < 2; i++)
        OUT_BUFFER_3DW(batch, param->downscaled_fwd_ref_4x[i]->bo,
                       0, 0);

    /* DW43..45 */
    OUT_BUFFER_3DW(batch, NULL, 0, 0);

    /* DW46..48 */
    OUT_BUFFER_3DW(batch, param->lcu_pak_obj_cmd_buf->bo,
                   1, 0);

    /* DW49..51 */
    OUT_BUFFER_3DW(batch, param->scaled_ref_8x->bo,
                   1, 0);

    /* DW52..54 */
    OUT_BUFFER_3DW(batch, param->scaled_ref_4x->bo,
                   1, 0);

    /* DW55..60 */
    OUT_BUFFER_3DW(batch, param->vp9_segmentation_map_streamin_buf->bo,
                   1, 0);
    OUT_BUFFER_3DW(batch, param->vp9_segmentation_map_streamout_buf->bo,
                   1, 0);

    /* DW61 */
    OUT_BCS_BATCH(batch, param->dw61.weights_histogram_streamout_offset);

    ADVANCE_BCS_BATCH(batch);
}
