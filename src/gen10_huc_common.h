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

#ifndef GEN10_HUC_COMMON_H
#define GEN10_HUC_COMMON_H

struct gen10_huc_pipe_mode_select_parameter {
    uint32_t    huc_stream_object_enabled;
    uint32_t    indirect_stream_out_enabled;
    uint32_t    media_soft_reset_counter;
};

struct gen10_huc_imem_state_parameter {
    uint32_t    huc_firmware_descriptor;
};

struct gen10_huc_dmem_state_parameter {
    struct i965_gpe_resource *huc_data_source_res;
    uint32_t    huc_data_destination_base_address;
    uint32_t    huc_data_length;
};

struct gen10_huc_virtual_addr_parameter {
    struct {
        struct i965_gpe_resource *huc_surface_res;
        uint32_t offset;
        uint32_t is_target;
    } regions[16];
};

struct gen10_huc_ind_obj_base_addr_parameter {
    struct i965_gpe_resource *huc_indirect_stream_in_object_res;
    struct i965_gpe_resource *huc_indirect_stream_out_object_res;
};

struct gen10_huc_stream_object_parameter {
    uint32_t indirect_stream_in_data_length;
    uint32_t indirect_stream_in_start_address;
    uint32_t indirect_stream_out_start_address;
    uint32_t huc_bitstream_enabled;
    uint32_t length_mode;
    uint32_t stream_out;
    uint32_t emulation_prevention_byte_removal;
    uint32_t start_code_search_engine;
    uint8_t start_code_byte2;
    uint8_t start_code_byte1;
    uint8_t start_code_byte0;
};

struct gen10_huc_start_parameter {
    uint32_t last_stream_object;
};

void
gen10_huc_pipe_mode_select(VADriverContextP ctx,
                           struct intel_batchbuffer *batch,
                           struct gen10_huc_pipe_mode_select_parameter *params);

void
gen10_huc_imem_state(VADriverContextP ctx,
                     struct intel_batchbuffer *batch,
                     struct gen10_huc_imem_state_parameter *params);

void
gen10_huc_dmem_state(VADriverContextP ctx,
                     struct intel_batchbuffer *batch,
                     struct gen10_huc_dmem_state_parameter *params);

void
gen10_huc_virtual_addr_state(VADriverContextP ctx,
                             struct intel_batchbuffer *batch,
                             struct gen10_huc_virtual_addr_parameter *params);

void
gen10_huc_ind_obj_base_addr_state(VADriverContextP ctx,
                                  struct intel_batchbuffer *batch,
                                  struct gen10_huc_ind_obj_base_addr_parameter *params);

void
gen10_huc_stream_object(VADriverContextP ctx,
                        struct intel_batchbuffer *batch,
                        struct gen10_huc_stream_object_parameter *params);

void
gen10_huc_start(VADriverContextP ctx,
                struct intel_batchbuffer *batch,
                struct gen10_huc_start_parameter *params);

#endif /* GEN10_HUC_COMMON_H */
