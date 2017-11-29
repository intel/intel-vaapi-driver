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

#ifndef GEN10_HEVC_ENC_UTILS_H
#define GEN10_HEVC_ENC_UTILS_H

#define GEN10_MAX_REF_SURFACES                     8
#define GEN10_HEVC_NUM_MAX_REF_L0                  3
#define GEN10_HEVC_NUM_MAX_REF_L1                  1

#define GEN10_MAX_COLLOCATED_REF_IDX               2

#define GEN10_HEVC_ENC_MIN_LCU_SIZE                16
#define GEN10_HEVC_ENC_MAX_LCU_SIZE                64

#define GEN10_HEVC_ENC_PAK_OBJ_SIZE                ((5 + 3) * 4)
#define GEN10_HEVC_ENC_PAK_CU_RECORD_SIZE          (8 * 4)

#define GEN10_HEVC_ENC_ROLLING_I_COLUMN           1
#define GEN10_HEVC_ENC_ROLLING_I_ROW              2
#define GEN10_HEVC_ENC_ROLLING_I_SQUARE           3

enum GEN10_HEVC_BRC_METHOD {
    GEN10_HEVC_BRC_CBR,
    GEN10_HEVC_BRC_VBR,
    GEN10_HEVC_BRC_CQP,
    GEN10_HEVC_BRC_AVBR,
    GEN10_HEVC_BRC_ICQ,
    GEN10_HEVC_BRC_VCM,
    GEN10_HEVC_BRC_QVBR
};

struct gen10_hevc_enc_frame_info {
    int32_t frame_width;
    int32_t frame_height;
    uint32_t picture_coding_type;
    uint32_t bit_depth_luma_minus8;
    uint32_t bit_depth_chroma_minus8;

    int32_t cu_size;
    int32_t lcu_size;
    int32_t width_in_lcu;
    int32_t height_in_lcu;
    int32_t width_in_cu;
    int32_t height_in_cu;
    int32_t width_in_mb;
    int32_t height_in_mb;

    uint32_t ctu_max_bitsize_allowed;
    int32_t low_delay;
    int32_t arbitrary_num_mb_in_slice;

    uint8_t qm_matrix[4][3][2][64];
    uint8_t qm_dc_matrix[2][3][2];

    uint16_t fqm_matrix[4][2][64];
    uint16_t fqm_dc_matrix[2][2];

    int slice_qp;

    int mapped_ref_idx_list0[8];
    int mapped_ref_idx_list1[8];

    uint32_t gop_size;
    uint32_t gop_ref_dist;
    uint32_t gop_num_p;
    uint32_t gop_num_b[3];

    uint32_t reallocate_flag   : 1;
    uint32_t is_same_ref_list  : 1;
    uint32_t reserved          : 31;

    VAEncSequenceParameterBufferHEVC last_seq_param;
};

struct gen10_hevc_enc_status {
    uint32_t image_status_mask;
    uint32_t image_status_ctrl;
    uint32_t bytes_per_frame;
    uint32_t pass_number;
    uint32_t media_state;
    uint32_t qp_status;
    uint32_t bs_se_bitcount;
};

struct gen10_hevc_enc_status_buffer {
    struct i965_gpe_resource gpe_res;
    uint32_t status_size;

    uint32_t status_bytes_per_frame_offset;
    uint32_t status_image_mask_offset;
    uint32_t status_image_ctrl_offset;
    uint32_t status_pass_num_offset;
    uint32_t status_media_state_offset;
    uint32_t status_qp_status_offset;
    uint32_t status_bs_se_bitcount_offset;

    uint32_t mmio_bytes_per_frame_offset;
    uint32_t mmio_bs_frame_no_header_offset;
    uint32_t mmio_image_mask_offset;
    uint32_t mmio_image_ctrl_offset;
    uint32_t mmio_qp_status_offset;
    uint32_t mmio_bs_se_bitcount_offset;
};

struct gen10_hevc_enc_common_res {
    struct {
        struct i965_gpe_resource gpe_res;
        int offset;
        int end_offset;
    } compressed_bitstream;

    struct {
        struct object_surface *obj_surface;
        VASurfaceID surface_id;
        struct i965_gpe_resource gpe_res;
    } uncompressed_pic;

    struct {
        struct object_surface *obj_surface;
        VASurfaceID surface_id;
        struct i965_gpe_resource gpe_res;
    } reconstructed_pic;

    struct {
        struct object_surface *obj_surface;
        VASurfaceID surface_id;
        struct i965_gpe_resource gpe_res;
    } reference_pics[16];

    struct i965_gpe_resource deblocking_filter_line_buffer;
    struct i965_gpe_resource deblocking_filter_tile_line_buffer;
    struct i965_gpe_resource deblocking_filter_tile_column_buffer;
    struct i965_gpe_resource metadata_line_buffer;
    struct i965_gpe_resource metadata_tile_line_buffer;
    struct i965_gpe_resource metadata_tile_column_buffer;
    struct i965_gpe_resource sao_line_buffer;
    struct i965_gpe_resource sao_tile_line_buffer;
    struct i965_gpe_resource sao_tile_column_buffer;
    struct i965_gpe_resource streamout_data_destination_buffer;
    struct i965_gpe_resource picture_status_buffer;
    struct i965_gpe_resource ildb_streamout_buffer;
    struct i965_gpe_resource sao_streamout_data_destination_buffer;
    struct i965_gpe_resource frame_statics_streamout_data_destination_buffer;
    struct i965_gpe_resource sse_source_pixel_rowstore_buffer;
};

struct gen10_hevc_enc_lambda_param {
    uint16_t lambda_intra[2][64];
    uint16_t lambda_inter[2][64];
};

void
gen10_hevc_enc_init_frame_info(VADriverContextP ctx,
                               struct encode_state *encode_state,
                               struct intel_encoder_context *encoder_context,
                               struct gen10_hevc_enc_frame_info *frame_info);

void
gen10_hevc_enc_insert_packed_header(VADriverContextP ctx,
                                    struct encode_state *encode_state,
                                    struct intel_encoder_context *encoder_context,
                                    struct intel_batchbuffer *batch);

void
gen10_hevc_enc_insert_slice_header(VADriverContextP ctx,
                                   struct encode_state *encode_state,
                                   struct intel_encoder_context *encoder_context,
                                   struct intel_batchbuffer *batch,
                                   int slice_index);

int
gen10_hevc_enc_init_common_resource(VADriverContextP ctx,
                                    struct encode_state *encode_state,
                                    struct intel_encoder_context *encoder_context,
                                    struct gen10_hevc_enc_common_res *common_res,
                                    struct gen10_hevc_enc_frame_info *frame_info,
                                    int inter_enabled,
                                    int vdenc_enabled);

void
gen10_hevc_enc_free_common_resource(struct gen10_hevc_enc_common_res *common_resource);

void
gen10_hevc_enc_init_status_buffer(VADriverContextP ctx,
                                  struct encode_state *encode_state,
                                  struct intel_encoder_context *encoder_context,
                                  struct gen10_hevc_enc_status_buffer *status_buffer);

void
gen10_hevc_enc_init_lambda_param(struct gen10_hevc_enc_lambda_param *param,
                                 int bit_depth_luma_minus8,
                                 int bit_depth_chroma_minus8);

void
gen10_hevc_enc_hcp_set_qm_fqm_states(VADriverContextP ctx,
                                     struct intel_batchbuffer *batch,
                                     struct gen10_hevc_enc_frame_info *frame_info);

void
gen10_hevc_enc_hcp_set_ref_idx_lists(VADriverContextP ctx,
                                     struct intel_batchbuffer *batch,
                                     VAEncPictureParameterBufferHEVC *pic_param,
                                     VAEncSliceParameterBufferHEVC *slice_param);

void
gen10_hevc_enc_hcp_set_weight_offsets(VADriverContextP ctx,
                                      struct intel_batchbuffer *batch,
                                      VAEncPictureParameterBufferHEVC *pic_param,
                                      VAEncSliceParameterBufferHEVC *slice_param);

VAStatus
gen10_hevc_enc_ensure_surface(VADriverContextP ctx,
                              struct object_surface *obj_surface,
                              int bit_depth_minus8,
                              int reallocate_flag);

uint32_t
gen10_hevc_enc_get_profile_level_max_frame(VAEncSequenceParameterBufferHEVC *seq_param,
                                           uint32_t user_max_frame_size,
                                           uint32_t frame_rate);

uint32_t
gen10_hevc_enc_get_max_num_slices(VAEncSequenceParameterBufferHEVC *seq_param);

uint32_t
gen10_hevc_enc_get_pic_header_size(struct encode_state *encode_state);

#endif
