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


#ifndef GEN10_HCP_COMMON_H
#define GEN10_HCP_COMMON_H

#define GEN10_MMIO_HCP_ENC_BITSTREAM_BYTECOUNT_FRAME_OFFSET           0x1E9A0
#define GEN10_MMIO_HCP_ENC_BITSTREAM_BYTECOUNT_FRAME_NO_HEADER_OFFSET 0x1E9A4
#define GEN10_MMIO_HCP_ENC_BITSTREAM_SE_BITCOUNT_FRAME_OFFSET         0x1E9A8
#define GEN10_MMIO_HCP_ENC_IMAGE_STATUS_MASK_OFFSET                   0x1E9B8
#define GEn10_MMIO_HCP_ENC_IMAGE_STATUS_CTRL_OFFSET                   0x1E9BC
#define GEN10_MMIO_HCP_ENC_QP_STATE_OFFSET                            0x1E9C0

#define   GEN10_HCP_ENCODE              1
#define   GEN10_HCP_DECODE              0

#define   GEN10_HCP_HEVC_CODEC          0
#define   GEN10_HCP_VP9_CODEC           1

typedef struct _gen10_hcp_pipe_mode_select_param {
    struct {
        uint32_t codec_select             : 1;
        uint32_t deblocker_stream_enabled : 1;
        uint32_t pak_streamout_enabled    : 1;
        uint32_t pic_error_stat_enabled   : 1;
        uint32_t reserved0                : 1;
        uint32_t codec_standard_select    : 3;
        uint32_t sao_first_pass           : 1;
        uint32_t advanced_brc_enabled     : 1;
        uint32_t vdenc_mode               : 1;
        uint32_t rdoq_enabled             : 1;
        uint32_t pak_frame_level_streamout_enabled : 1;
        uint32_t reserved1                : 2;
        uint32_t pipe_work_mode           : 2;
        uint32_t reserved2                : 15;
    } dw1;

    uint32_t media_reset_counter;
    uint32_t pic_error_report_id;
    uint32_t reserved[2];
} gen10_hcp_pipe_mode_select_param;

#define GEN10_HCP_DECODE_SURFACE_ID     0
#define GEN10_HCP_INPUT_SURFACE_ID      1
#define GEN10_HCP_PREV_SURFACE_ID       2
#define GEN10_HCP_GOLD_SURFACE_ID       3
#define GEN10_HCP_ALT_SURFACE_ID        4
#define GEN10_HCP_REF_SURFACE_ID        5

typedef struct _gen10_hcp_surface_state_param {
    struct {
        uint32_t surface_pitch : 17;
        uint32_t reserved      : 11;
        uint32_t surface_id    : 4;
    } dw1;

    struct {
        uint32_t y_cb_offset    : 15;
        uint32_t reserved       : 13;
        uint32_t surface_format : 4;
    } dw2;

    struct {
        uint32_t default_alpha  : 16;
        uint32_t y_cr_offset    : 16;
    } dw3;

    struct {
        uint32_t auxilary_index : 11;
        uint32_t reserved0      : 1;
        uint32_t memory_compression : 1;
        uint32_t reserved1      : 18;
    } dw4;
} gen10_hcp_surface_state_param;

typedef struct _gen10_hcp_pipe_buf_addr_state_param {
    struct i965_gpe_resource *reconstructed;
    struct i965_gpe_resource *deblocking_filter_line;
    struct i965_gpe_resource *deblocking_filter_tile_line;
    struct i965_gpe_resource *deblocking_filter_tile_column;
    struct i965_gpe_resource *metadata_line;
    struct i965_gpe_resource *metadata_tile_line;
    struct i965_gpe_resource *metadata_tile_column;
    struct i965_gpe_resource *sao_line;
    struct i965_gpe_resource *sao_tile_line;
    struct i965_gpe_resource *sao_tile_column;
    struct i965_gpe_resource *current_motion_vector_temporal;
    struct i965_gpe_resource *reference_picture[8];
    struct i965_gpe_resource *uncompressed_picture;
    struct i965_gpe_resource *streamout_data_destination;
    struct i965_gpe_resource *picture_status;
    struct i965_gpe_resource *ildb_streamout;
    struct i965_gpe_resource *collocated_motion_vector_temporal[8];
    struct i965_gpe_resource *vp9_probability;
    struct i965_gpe_resource *vp9_segmentid;
    struct i965_gpe_resource *vp9_hvd_line_rowstore;
    struct i965_gpe_resource *vp9_hvd_time_rowstore;
    struct i965_gpe_resource *sao_streamout_data_destination;
    struct i965_gpe_resource *frame_statics_streamout_data_destination;
    struct i965_gpe_resource *sse_source_pixel_rowstore;
} gen10_hcp_pipe_buf_addr_state_param;

typedef struct _gen10_hcp_ind_obj_base_addr_state_param {
    struct i965_gpe_resource *ind_cu_obj_bse;

    uint32_t ind_cu_obj_bse_offset;

    struct i965_gpe_resource *ind_pak_bse;

    uint32_t ind_pak_bse_offset;
    uint32_t ind_pak_bse_upper;
} gen10_hcp_ind_obj_base_addr_state_param;

typedef struct _gen10_hcp_pic_state_param {
    struct {
        uint32_t frame_width_in_cu_minus1  : 11;
        uint32_t reserved0                 : 4;
        uint32_t pak_transform_skip        : 1;
        uint32_t frame_height_in_cu_minus1 : 11;
        uint32_t reserved1                 : 5;
    } dw1;

    struct {
        uint32_t min_cu_size               : 2;
        uint32_t lcu_size                  : 2;
        uint32_t min_tu_size               : 2;
        uint32_t max_tu_size               : 2;
        uint32_t min_pcm_size              : 2;
        uint32_t max_pcm_size              : 2;
        uint32_t reserved                  : 20;
    } dw2;

    struct {
        uint32_t col_pic_i_only            : 1;
        uint32_t curr_pic_i_only           : 1;
        uint32_t insert_test_flag          : 1;
        uint32_t reserved                  : 29;
    } dw3;

    struct {
        uint32_t reserved0                 : 3;
        uint32_t sao_enabled_flag          : 1;
        uint32_t pcm_enabled_flag          : 1;
        uint32_t cu_qp_delta_enabled_flag  : 1;
        uint32_t diff_cu_qp_delta_depth    : 2;

        uint32_t pcm_loop_filter_disable_flag      : 1;
        uint32_t constrained_intra_pred_flag       : 1;
        uint32_t log2_parallel_merge_level_minus2  : 3;
        uint32_t sign_data_hiding_flag             : 1;
        uint32_t reserved1                         : 1;
        uint32_t loop_filter_across_tiles_enabled_flag : 1;

        uint32_t entropy_coding_sync_enabled_flag  : 1;
        uint32_t tiles_enabled_flag               : 1;
        uint32_t weighted_bipred_flag             : 1;
        uint32_t weighted_pred_flag               : 1;
        uint32_t field_pic                        : 1;
        uint32_t bottom_field                     : 1;
        uint32_t transform_skip_enabled_flag      : 1;
        uint32_t amp_enabled_flag                 : 1;

        uint32_t reserved2                        : 1;
        uint32_t transquant_bypass_enabled_flag   : 1;
        uint32_t strong_intra_smoothing_enabled_flag : 1;
        uint32_t cu_pak_structure                 : 1;
        uint32_t reserved3                        : 4;
    } dw4;

    struct {
        uint32_t pic_cb_qp_offset                    : 5;
        uint32_t pic_cr_qp_offset                    : 5;
        uint32_t max_transform_hierarchy_depth_intra : 3;
        uint32_t max_transform_hierarchy_depth_inter : 3;

        uint32_t pcm_sample_bit_depth_chroma_minus1  : 4;
        uint32_t pcm_sample_bit_depth_luma_minus1    : 4;
        uint32_t bit_depth_chroma_minus8             : 3;
        uint32_t bit_depth_luma_minus8               : 3;
        uint32_t reserved                            : 2;
    } dw5;

    struct {
        uint32_t lcu_max_bits_allowed          : 16;
        uint32_t non_first_pass_flag           : 1;
        uint32_t reserved0                     : 7;
        uint32_t lcu_max_bits_stats_en         : 1;
        uint32_t frame_sz_over_status_en       : 1;
        uint32_t frame_sz_under_status_en      : 1;
        uint32_t reserved1                     : 2;
        uint32_t load_slice_ptr_flag           : 1;
        uint32_t reserved2                     : 2;
    } dw6;

    struct {
        uint32_t frame_bit_rate_max            : 14;
        uint32_t reserved                      : 17;
        uint32_t frame_bit_rate_unit           : 1;
    } dw7;

    struct {
        uint32_t frame_bit_rate_min            : 14;
        uint32_t reserved                      : 17;
        uint32_t frame_bit_rate_unit           : 1;
    } dw8;

    struct {
        uint32_t frame_bit_rate_delta_min      : 15;
        uint32_t reserved0                     : 1;
        uint32_t frame_bit_rate_delta_max      : 15;
        uint32_t reserved1                     : 1;
    } dw9;

    struct {
        uint32_t frame_max_delta_qp0           : 8;
        uint32_t frame_max_delta_qp1           : 8;
        uint32_t frame_max_delta_qp2           : 8;
        uint32_t frame_max_delta_qp3           : 8;
    } dw10;

    struct {
        uint32_t frame_max_delta_qp4           : 8;
        uint32_t frame_max_delta_qp5           : 8;
        uint32_t frame_max_delta_qp6           : 8;
        uint32_t frame_max_delta_qp7           : 8;
    } dw11;

    struct {
        uint32_t frame_min_delta_qp0           : 8;
        uint32_t frame_min_delta_qp1           : 8;
        uint32_t frame_min_delta_qp2           : 8;
        uint32_t frame_min_delta_qp3           : 8;
    } dw12;

    struct {
        uint32_t frame_min_delta_qp4           : 8;
        uint32_t frame_min_delta_qp5           : 8;
        uint32_t frame_min_delta_qp6           : 8;
        uint32_t frame_min_delta_qp7           : 8;
    } dw13;

    struct {
        uint32_t frame_max_range_delta_qp0     : 8;
        uint32_t frame_max_range_delta_qp1     : 8;
        uint32_t frame_max_range_delta_qp2     : 8;
        uint32_t frame_max_range_delta_qp3     : 8;
    } dw14;

    struct {
        uint32_t frame_max_range_delta_qp4     : 8;
        uint32_t frame_max_range_delta_qp5     : 8;
        uint32_t frame_max_range_delta_qp6     : 8;
        uint32_t frame_max_range_delta_qp7     : 8;
    } dw15;

    struct {
        uint32_t frame_min_range_delta_qp0     : 8;
        uint32_t frame_min_range_delta_qp1     : 8;
        uint32_t frame_min_range_delta_qp2     : 8;
        uint32_t frame_min_range_delta_qp3     : 8;
    } dw16;

    struct {
        uint32_t frame_min_range_delta_qp4     : 8;
        uint32_t frame_min_range_delta_qp5     : 8;
        uint32_t frame_min_range_delta_qp6     : 8;
        uint32_t frame_min_range_delta_qp7     : 8;
    } dw17;

    struct {
        uint32_t min_frame_size                : 16;
        uint32_t reserved                      : 14;
        uint32_t min_frame_size_unit           : 2;
    } dw18;

    struct {
        uint32_t fraction_qp_input             : 3;
        uint32_t fraction_qp_offset            : 3;
        uint32_t rho_domain_rc_enabled         : 1;
        uint32_t fraction_qp_adj_enabled       : 1;

        uint32_t rho_domain_frame_qp           : 6;
        uint32_t pak_dyna_slice_mode_enabled   : 1;
        uint32_t no_output_of_prior_pics_flag  : 1;

        uint32_t first_slice_segment_in_pic_flag : 1;
        uint32_t nal_unit_type_flag              : 1;
        uint32_t slice_pic_parameter_set_id      : 6;

        uint32_t sse_enabled                     : 1;
        uint32_t rhoq_enabled                    : 1;
        uint32_t lcu_num_in_ssc_mode             : 2;
        uint32_t reserved                        : 2;
        uint32_t partial_frame_update            : 1;
        uint32_t temporal_mv_disable             : 1;
    } dw19;

    struct {
        uint32_t reserved;
    } dw20;

    struct {
        uint32_t slice_size_thr_in_bytes;
    } dw21;

    struct {
        uint32_t target_slice_size_in_bytes;
    } dw22;

    struct {
        uint32_t class0_sse_threshold_0     : 16;
        uint32_t class0_sse_threshold_1     : 16;
    } dw23;

    struct {
        uint32_t class1_sse_threshold_0     : 16;
        uint32_t class1_sse_threshold_1     : 16;
    } dw24;

    struct {
        uint32_t class2_sse_threshold_0     : 16;
        uint32_t class2_sse_threshold_1     : 16;
    } dw25;

    struct {
        uint32_t class3_sse_threshold_0     : 16;
        uint32_t class3_sse_threshold_1     : 16;
    } dw26;

    struct {
        uint32_t class4_sse_threshold_0     : 16;
        uint32_t class4_sse_threshold_1     : 16;
    } dw27;

    struct {
        uint32_t class5_sse_threshold_0     : 16;
        uint32_t class5_sse_threshold_1     : 16;
    } dw28;

    struct {
        uint32_t class6_sse_threshold_0     : 16;
        uint32_t class6_sse_threshold_1     : 16;
    } dw29;

    struct {
        uint32_t class7_sse_threshold_0     : 16;
        uint32_t class7_sse_threshold_1     : 16;
    } dw30;
} gen10_hcp_pic_state_param;

typedef struct _gen10_hcp_vp9_pic_state_param {
    struct {
        uint32_t frame_width_in_pixels_minus1: 14;
        uint32_t reserved0: 2;
        uint32_t frame_height_in_pixels_minus1: 14;
        uint32_t reserved1: 2;
    } dw1;

    struct {
        uint32_t frame_type: 1;
        uint32_t adapt_probabilities_flag: 1;
        uint32_t intra_only_flag: 1;
        uint32_t allow_high_precision_mv: 1;
        uint32_t motion_comp_filter_type: 3;
        uint32_t ref_frame_sign_bias_last: 1;
        uint32_t ref_frame_sign_bias_golden: 1;
        uint32_t ref_frame_sign_bias_altref: 1;
        uint32_t use_prev_in_find_mv_references: 1;
        uint32_t hybrid_prediction_mode: 1;
        uint32_t selectable_tx_mode: 1;
        uint32_t last_frame_type: 1;
        uint32_t refresh_frame_context: 1;
        uint32_t error_resilient_mode: 1;
        uint32_t frame_parallel_decoding_mode: 1;
        uint32_t filter_level: 6;
        uint32_t sharpness_level: 3;
        uint32_t segmentation_enabled: 1;
        uint32_t segmentation_update_map: 1;
        uint32_t segmentation_temporal_update: 1;
        uint32_t lossless_mode: 1;
        uint32_t segment_id_streamout_enable: 1;
        uint32_t segment_id_streamin_enable: 1;
    } dw2;

    struct {
        uint32_t log2_tile_column: 4;
        uint32_t pad0: 4;
        uint32_t log2_tile_row: 2;
        uint32_t pad1: 11;
        uint32_t sse_enable: 1;
        uint32_t chroma_sampling_format: 2;
        uint32_t bit_depth_minus8: 4;
        uint32_t profile_level: 4;
    } dw3;

    struct {
        uint32_t vertical_scale_factor_for_last: 16;
        uint32_t horizontal_scale_factor_for_last: 16;
    } dw4;

    struct {
        uint32_t vertical_scale_factor_for_golden: 16;
        uint32_t horizontal_scale_factor_for_golden: 16;
    } dw5;

    struct {
        uint32_t vertical_scale_factor_for_altref: 16;
        uint32_t horizontal_scale_factor_for_altref: 16;
    } dw6;

    struct {
        uint32_t last_frame_width_in_pixels_minus1: 14;
        uint32_t pad0: 2;
        uint32_t last_frame_height_in_pixels_minus1: 14;
        uint32_t pad1: 2;
    } dw7;

    struct {
        uint32_t golden_frame_width_in_pixels_minus1: 14;
        uint32_t pad0: 2;
        uint32_t golden_frame_height_in_pixels_minus1: 14;
        uint32_t pad1: 2;
    } dw8;

    struct {
        uint32_t altref_frame_width_in_pixels_minus1: 14;
        uint32_t pad0: 2;
        uint32_t altref_frame_height_in_pixels_minus1: 14;
        uint32_t pad1: 2;
    } dw9;

    struct {
        uint32_t uncompressed_header_length_in_bytes: 8;
        uint32_t pad0: 8;
        uint32_t first_partition_size_in_bytes: 16;
    } dw10;

    struct {
        uint32_t pad0: 1;
        uint32_t motion_comp_scaling: 1;
        uint32_t mv_clamp_disable: 1;
        uint32_t chroma_fractional_calculation_modified: 1;
        uint32_t pad1: 28;
    } dw11;

    struct {
        uint32_t pad0;
    } dw12;

    struct {
        uint32_t compressed_header_buffer_bin_count: 16;
        uint32_t base_qindex: 8;
        uint32_t tail_insertion_enable: 1;
        uint32_t header_insertion_enable: 1;
        uint32_t pad0: 6;
    } dw13;

    struct {
        uint32_t chroma_ac_qindex_delta: 5;
        uint32_t pad0: 3;
        uint32_t chroma_dc_qindex_delta: 5;
        uint32_t pad1: 3;
        uint32_t luma_dc_qindex_delta: 5;
        uint32_t pad2: 11;
    } dw14;

    struct {
        uint32_t lf_ref_delta0: 7;
        uint32_t pad0: 1;
        uint32_t lf_ref_delta1: 7;
        uint32_t pad1: 1;
        uint32_t lf_ref_delta2: 7;
        uint32_t pad2: 1;
        uint32_t lf_ref_delta3: 7;
        uint32_t pad3: 1;
    } dw15;

    struct {
        uint32_t lf_mode_delta0: 7;
        uint32_t pad0: 1;
        uint32_t lf_mode_delta1: 7;
        uint32_t pad1: 17;
    } dw16;

    struct {
        uint32_t bit_offset_for_lf_ref_delta: 16;
        uint32_t bit_offset_for_mode_delta: 16;
    } dw17;

    struct {
        uint32_t bit_offset_for_qindex: 16;
        uint32_t bit_offset_for_lf_level: 16;
    } dw18;

    struct {
        uint32_t pad0: 16;
        uint32_t non_first_pass_flag: 1;
        uint32_t vdenc_pak_only_pass: 1;
        uint32_t pad1: 7;
        uint32_t frame_bitrate_max_report_mask: 1;
        uint32_t frame_bitrate_min_report_mask: 1;
        uint32_t pad2: 5;
    } dw19;

    struct {
        uint32_t frame_bitrate_max: 14;
        uint32_t pad0: 17;
        uint32_t frame_bitrate_max_unit: 1;
    } dw20;

    struct {
        uint32_t frame_bitrate_min: 14;
        uint32_t pad0: 17;
        uint32_t frame_bitrate_min_unit: 1;
    } dw21;

    struct {
        uint32_t frame_delta_qindex_max_low;
        uint32_t frame_delta_qindex_max_high;
    } dw2223;

    struct {
        uint32_t frame_delta_qindex_min;
    } dw24;

    struct {
        uint32_t frame_delta_lf_max_low;
        uint32_t frame_delta_lf_max_high;
    } dw2526;

    struct {
        uint32_t frame_delta_lf_min;
    } dw27;

    struct {
        uint32_t frame_delta_qindex_lf_max_range_low;
        uint32_t frame_delta_qindex_lf_max_range_high;
    } dw2829;

    struct {
        uint32_t frame_delta_qindex_lf_min_range;
    } dw30;

    struct {
        uint32_t min_fram_size: 16;
        uint32_t pad0: 14;
        uint32_t min_frame_size_units: 2;
    } dw31;

    struct {
        uint32_t bit_offset_for_first_partition_size: 16;
        uint32_t pad0: 16;
    } dw32;

    struct {
        uint32_t class0_sse_threshold_0: 16;
        uint32_t class0_sse_threshold_1: 16;
    } dw33;

    struct {
        uint32_t class1_sse_threshold_0: 16;
        uint32_t class1_sse_threshold_1: 16;
    } dw34;

    struct {
        uint32_t class2_sse_threshold_0: 16;
        uint32_t class2_sse_threshold_1: 16;
    } dw35;

    struct {
        uint32_t class3_sse_threshold_0: 16;
        uint32_t class3_sse_threshold_1: 16;
    } dw36;

    struct {
        uint32_t class4_sse_threshold_0: 16;
        uint32_t class4_sse_threshold_1: 16;
    } dw37;

    struct {
        uint32_t class5_sse_threshold_0: 16;
        uint32_t class5_sse_threshold_1: 16;
    } dw38;

    struct {
        uint32_t class6_sse_threshold_0: 16;
        uint32_t class6_sse_threshold_1: 16;
    } dw39;

    struct {
        uint32_t class7_sse_threshold_0: 16;
        uint32_t class7_sse_threshold_1: 16;
    } dw40;

    struct {
        uint32_t class8_sse_threshold_0: 16;
        uint32_t class8_sse_threshold_1: 16;
    } dw41;
} gen10_hcp_vp9_pic_state_param;

typedef struct _gen10_hcp_qm_state_param {
    struct {
        uint32_t prediction_type            : 1;
        uint32_t size_id                    : 2;
        uint32_t color_component            : 2;
        uint32_t dc_coefficient             : 8;
        uint32_t reserved                   : 19;
    } dw1;

    uint32_t quant_matrix[16];
} gen10_hcp_qm_state_param;

typedef struct _gen10_hcp_fqm_state_param {
    struct {
        uint32_t prediction_type            : 1;
        uint32_t size_id                    : 2;
        uint32_t color_component            : 2;
        uint32_t reserved                   : 11;
        uint32_t forward_dc_coeff           : 16;
    } dw1;

    uint32_t forward_quant_matrix[32];
} gen10_hcp_fqm_state_param;

typedef struct _gen10_hcp_rdoq_state_param {
    struct {
        uint32_t reserved         : 30;
        uint32_t disable_htq_fix1 : 1;
        uint32_t disable_htq_fix0 : 1;
    } dw1;

    uint16_t lambda_intra_luma[64];
    uint16_t lambda_intra_chroma[64];
    uint16_t lambda_inter_luma[64];
    uint16_t lambda_inter_chroma[64];
} gen10_hcp_rdoq_state_param;

typedef struct _gen10_hcp_weightoffset_state_param {
    struct {
        uint32_t ref_pic_list_num   : 1;
        uint32_t reserved           : 31;
    } dw1;

    struct {
        uint32_t delta_luma_weight  : 8;
        uint32_t luma_offset        : 8;
        uint32_t reserved           : 16;
    } luma_offset[16];

    struct {
        uint32_t delta_chroma_weight_0  : 8;
        uint32_t chroma_offset_0         : 8;
        uint32_t delta_chroma_weight_1  : 8;
        uint32_t chroma_offset_1         : 8;
    } chroma_offset[16];
} gen10_hcp_weightoffset_state_param;

#define GEN10_HCP_B_SLICE     0
#define GEN10_HCP_P_SLICE     1
#define GEN10_HCP_I_SLICE     2

typedef struct _gen10_hcp_slice_state_param {
    struct {
        uint32_t slice_start_ctu_x            : 10;
        uint32_t reserved0                    : 6;
        uint32_t slice_start_ctu_y            : 10;
        uint32_t reserved1                    : 6;
    } dw1;

    struct {
        uint32_t next_slice_start_ctu_x       : 10;
        uint32_t reserved0                    : 6;
        uint32_t next_slice_start_ctu_y       : 10;
        uint32_t reserved1                    : 6;
    } dw2;

    struct {
        uint32_t slice_type                   : 2;
        uint32_t last_slice_flag              : 1;
        uint32_t slice_qp_sign_flag           : 1;
        uint32_t reserved0                    : 1;
        uint32_t slice_temporal_mvp_enabled   : 1;
        uint32_t slice_qp                     : 6;
        uint32_t slice_cb_qp_offset           : 5;
        uint32_t slice_cr_qp_offset           : 5;
        uint32_t intra_ref_fetch_disable      : 1;
        uint32_t cu_chroma_qu_offset_enabled  : 1;
        uint32_t reserved1                    : 8;
    } dw3;

    struct {
        uint32_t deblocking_filter_disable    : 1;
        uint32_t tc_offset_div2               : 4;
        uint32_t beta_offset_div2             : 4;
        uint32_t reserved0                    : 1;
        uint32_t loop_filter_across_slices_enabled : 1;
        uint32_t sao_chroma_flag              : 1;
        uint32_t sao_luma_flag                : 1;
        uint32_t mvd_l1_zero_flag             : 1;
        uint32_t is_low_delay                 : 1;
        uint32_t collocated_from_l0_flag      : 1;
        uint32_t chroma_log2_weight_denom     : 3;
        uint32_t luma_log2_weight_denom       : 3;
        uint32_t cabac_init_flag              : 1;
        uint32_t max_merge_idx                : 3;
        uint32_t collocated_ref_idx           : 3;
        uint32_t reserved1                    : 3;
    } dw4;

    struct {
        uint32_t reserved;
    } dw5;

    struct {
        uint32_t reserved0                    : 20;
        uint32_t round_intra                  : 4;
        uint32_t reserved1                    : 2;
        uint32_t round_inter                  : 4;
        uint32_t reserved2                    : 2;
    } dw6;

    struct {
        uint32_t reserved0                    : 1;
        uint32_t cabac_zero_word_insertion_enabled : 1;
        uint32_t emulation_byte_insert_enabled : 1;
        uint32_t reserved1                    : 5;
        uint32_t tail_insertion_enabled       : 1;
        uint32_t slice_data_enabled           : 1;
        uint32_t header_insertion_enabled     : 1;
        uint32_t reserved2                    : 21;
    } dw7;

    struct {
        uint32_t reserved;
    } dw8;

    struct {
        uint32_t transform_skip_lambda        : 16;
        uint32_t reserved                     : 16;
    } dw9;

    struct {
        uint32_t transform_skip_zero_factor0      : 8;
        uint32_t transform_skip_nonezero_factor0  : 8;
        uint32_t transform_skip_zero_factor1      : 8;
        uint32_t transform_skip_nonezero_factor1  : 8;
    } dw10;
} gen10_hcp_slice_state_param;

typedef struct _gen10_hcp_ref_idx_state_param {
    struct {
        uint32_t ref_pic_list_num          : 1;
        uint32_t num_ref_idx_active_minus1 : 4;
        uint32_t reserved                  : 27;
    } dw1;

    struct {
        uint32_t ref_pic_tb_value          : 8;
        uint32_t ref_pic_frame_id          : 3;
        uint32_t chroma_weight_flag        : 1;
        uint32_t luma_weight_flag          : 1;
        uint32_t long_term_ref_flag        : 1;
        uint32_t field_pic_flag            : 1;
        uint32_t bottom_field_flag         : 1;
        uint32_t reserved                  : 16;
    } ref_list_entry[16];
} gen10_hcp_ref_idx_state_param;

typedef struct _gen10_hcp_pak_insert_object_param {
    union {
        struct {
            uint32_t reserved0                : 1;
            uint32_t end_of_slice_flag        : 1;
            uint32_t last_header_flag         : 1;
            uint32_t emulation_flag           : 1;
            uint32_t skip_emulation_bytes     : 4;
            uint32_t data_bits_in_last_dw     : 6;
            uint32_t reserved1                : 18;
        } bits;
        uint32_t value;
    } dw1;

    uint8_t *inline_payload_ptr;
    uint32_t inline_payload_bits;
} gen10_hcp_pak_insert_object_param;

typedef struct gen10_hcp_vp9_segment_state_param {
    struct {
        uint32_t segment_id: 3;
        uint32_t pad0: 29;
    } dw1;

    struct {
        uint32_t segment_skipped: 1;
        uint32_t segment_reference: 2;
        uint32_t segment_reference_enabled: 1;
        uint32_t pad0: 28;
    } dw2;

    struct {
        uint32_t pad0; /* decoder only */
    } dw3;

    struct {
        uint32_t pad0; /* decoder only */
    } dw4;

    struct {
        uint32_t pad0; /* decoder only */
    } dw5;

    struct {
        uint32_t pad0; /* decoder only */
    } dw6;

    struct {
        uint32_t segment_qindex_delta: 9;
        uint32_t pad0: 7;
        uint32_t segment_lf_level_delta: 7;
        uint32_t pad1: 9;
    } dw7;
} gen10_hcp_vp9_segment_state_param;

void
gen10_hcp_pipe_mode_select(VADriverContextP ctx,
                           struct intel_batchbuffer *batch,
                           gen10_hcp_pipe_mode_select_param *param);
void
gen10_hcp_surface_state(VADriverContextP ctx,
                        struct intel_batchbuffer *batch,
                        gen10_hcp_surface_state_param *param);
void
gen10_hcp_pipe_buf_addr_state(VADriverContextP ctx,
                              struct intel_batchbuffer *batch,
                              gen10_hcp_pipe_buf_addr_state_param *param);
void
gen10_hcp_ind_obj_base_addr_state(VADriverContextP ctx,
                                  struct intel_batchbuffer *batch,
                                  gen10_hcp_ind_obj_base_addr_state_param *param);
void
gen10_hcp_pic_state(VADriverContextP ctx,
                    struct intel_batchbuffer *batch,
                    gen10_hcp_pic_state_param *param);

void
gen10_hcp_vp9_pic_state(VADriverContextP ctx,
                        struct intel_batchbuffer *batch,
                        gen10_hcp_vp9_pic_state_param *param);

void
gen10_hcp_qm_state(VADriverContextP ctx,
                   struct intel_batchbuffer *batch,
                   gen10_hcp_qm_state_param *param);
void
gen10_hcp_fqm_state(VADriverContextP ctx,
                    struct intel_batchbuffer *batch,
                    gen10_hcp_fqm_state_param *param);
void
gen10_hcp_rdoq_state(VADriverContextP ctx,
                     struct intel_batchbuffer *batch,
                     gen10_hcp_rdoq_state_param *param);
void
gen10_hcp_weightoffset_state(VADriverContextP ctx,
                             struct intel_batchbuffer *batch,
                             gen10_hcp_weightoffset_state_param *param);
void
gen10_hcp_slice_state(VADriverContextP ctx,
                      struct intel_batchbuffer *batch,
                      gen10_hcp_slice_state_param *param);
void
gen10_hcp_ref_idx_state(VADriverContextP ctx,
                        struct intel_batchbuffer *batch,
                        gen10_hcp_ref_idx_state_param *param);
void
gen10_hcp_pak_insert_object(VADriverContextP ctx,
                            struct intel_batchbuffer *batch,
                            gen10_hcp_pak_insert_object_param *param);

void
gen10_hcp_vp9_segment_state(VADriverContextP ctx,
                            struct intel_batchbuffer *batch,
                            gen10_hcp_vp9_segment_state_param *param);

#endif
