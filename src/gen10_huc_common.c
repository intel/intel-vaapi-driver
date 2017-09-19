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
 *    Xiang Haihao <haihao.xiang@intel.com>
 *
 */

#include <assert.h>

#include "intel_batchbuffer.h"
#include "i965_defines.h"
#include "i965_drv_video.h"
#include "gen10_huc_common.h"

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
gen10_huc_pipe_mode_select(VADriverContextP ctx,
                           struct intel_batchbuffer *batch,
                           struct gen10_huc_pipe_mode_select_parameter *params)
{
    BEGIN_BCS_BATCH(batch, 3);

    OUT_BCS_BATCH(batch, HUC_PIPE_MODE_SELECT | (3 - 2));
    OUT_BCS_BATCH(batch,
                  (!!params->huc_stream_object_enabled << 10) |
                  (!!params->indirect_stream_out_enabled << 4));
    OUT_BCS_BATCH(batch,
                  params->media_soft_reset_counter);

    ADVANCE_BCS_BATCH(batch);
}

void
gen10_huc_imem_state(VADriverContextP ctx,
                     struct intel_batchbuffer *batch,
                     struct gen10_huc_imem_state_parameter *params)
{
    assert(params->huc_firmware_descriptor >= 1 &&
           params->huc_firmware_descriptor <= 255);

    BEGIN_BCS_BATCH(batch, 5);

    OUT_BCS_BATCH(batch, HUC_IMEM_STATE | (5 - 2));
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, params->huc_firmware_descriptor);

    ADVANCE_BCS_BATCH(batch);
}

void
gen10_huc_dmem_state(VADriverContextP ctx,
                     struct intel_batchbuffer *batch,
                     struct gen10_huc_dmem_state_parameter *params)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);

    BEGIN_BCS_BATCH(batch, 6);

    OUT_BCS_BATCH(batch, HUC_DMEM_STATE | (6 - 2));
    OUT_BUFFER_3DW(batch, params->huc_data_source_res->bo, 0, 0);
    OUT_BCS_BATCH(batch, params->huc_data_destination_base_address);
    OUT_BCS_BATCH(batch, params->huc_data_length);

    ADVANCE_BCS_BATCH(batch);
}

void
gen10_huc_virtual_addr_state(VADriverContextP ctx,
                             struct intel_batchbuffer *batch,
                             struct gen10_huc_virtual_addr_parameter *params)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    int i;

    BEGIN_BCS_BATCH(batch, 49);

    OUT_BCS_BATCH(batch, HUC_VIRTUAL_ADDR_STATE | (49 - 2));

    for (i = 0; i < 16; i++) {
        if (params->regions[i].huc_surface_res && params->regions[i].huc_surface_res->bo)
            OUT_BUFFER_3DW(batch,
                           params->regions[i].huc_surface_res->bo,
                           !!params->regions[i].is_target,
                           params->regions[i].offset);
        else
            OUT_BUFFER_3DW(batch, NULL, 0, 0);
    }

    ADVANCE_BCS_BATCH(batch);
}

void
gen10_huc_ind_obj_base_addr_state(VADriverContextP ctx,
                                  struct intel_batchbuffer *batch,
                                  struct gen10_huc_ind_obj_base_addr_parameter *params)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);

    BEGIN_BCS_BATCH(batch, 11);

    OUT_BCS_BATCH(batch, HUC_IND_OBJ_BASE_ADDR_STATE | (11 - 2));

    if (params->huc_indirect_stream_in_object_res)
        OUT_BUFFER_3DW(batch,
                       params->huc_indirect_stream_in_object_res->bo,
                       0,
                       0);
    else
        OUT_BUFFER_3DW(batch, NULL, 0, 0);

    OUT_BUFFER_2DW(batch, NULL, 0, 0); /* ignore access upper bound */

    if (params->huc_indirect_stream_out_object_res)
        OUT_BUFFER_3DW(batch,
                       params->huc_indirect_stream_out_object_res->bo,
                       1,
                       0);
    else
        OUT_BUFFER_3DW(batch, NULL, 0, 0);

    OUT_BUFFER_2DW(batch, NULL, 0, 0); /* ignore access upper bound */

    ADVANCE_BCS_BATCH(batch);
}

void
gen10_huc_stream_object(VADriverContextP ctx,
                        struct intel_batchbuffer *batch,
                        struct gen10_huc_stream_object_parameter *params)
{
    BEGIN_BCS_BATCH(batch, 5);

    OUT_BCS_BATCH(batch, HUC_STREAM_OBJECT | (5 - 2));
    OUT_BCS_BATCH(batch, params->indirect_stream_in_data_length);
    OUT_BCS_BATCH(batch,
                  (1 << 31) |   /* Must be 1 */
                  params->indirect_stream_in_start_address);
    OUT_BCS_BATCH(batch, params->indirect_stream_out_start_address);
    OUT_BCS_BATCH(batch,
                  (!!params->huc_bitstream_enabled << 29) |
                  (params->length_mode << 27) |
                  (!!params->stream_out << 26) |
                  (!!params->emulation_prevention_byte_removal << 25) |
                  (!!params->start_code_search_engine << 24) |
                  (params->start_code_byte2 << 16) |
                  (params->start_code_byte1 << 8) |
                  params->start_code_byte0);

    ADVANCE_BCS_BATCH(batch);
}

void
gen10_huc_start(VADriverContextP ctx,
                struct intel_batchbuffer *batch,
                struct gen10_huc_start_parameter *params)
{
    BEGIN_BCS_BATCH(batch, 2);

    OUT_BCS_BATCH(batch, HUC_START | (2 - 2));
    OUT_BCS_BATCH(batch, !!params->last_stream_object);

    ADVANCE_BCS_BATCH(batch);
}
