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

#ifndef GEN10_VDENC_COMMON_H
#define GEN10_VDENC_COMMON_H

typedef struct _gen10_vdenc_vd_pipeline_flush_param {
    struct {
        uint32_t hevc_pipeline_done                     : 1;
        uint32_t vdenc_pipeline_done                    : 1;
        uint32_t mfl_pipeline_done                      : 1;
        uint32_t mfx_pipeline_done                      : 1;
        uint32_t vd_cmd_msg_parser_done                 : 1;
        uint32_t reserved0                              : 11;
        uint32_t hevc_pipeline_flush                    : 1;
        uint32_t vdenc_pipeline_flush                   : 1;
        uint32_t mfl_pipeline_flush                     : 1;
        uint32_t mfx_pipeline_flush                     : 1;
        uint32_t reserved1                              : 12;
    } dw1;
} gen10_vdenc_vd_pipeline_flush_param;

#define GEN10_VDENC_HEVC_CODEC      0
#define GEN10_VDENC_VP9_CODEC       1
#define GEN10_VDENC_AVC_CODEC       2

#define GEN10_VDENC_CHROMA_420      1
#define GEN10_VDENC_CHROMA_422      2
#define GEN10_VDENC_CHROMA_444      3

typedef struct _gen10_vdenc_pipe_mode_select_param {
    struct {
        uint32_t codec_type                             : 4;
        uint32_t reserved0                              : 1;
        uint32_t frame_statics_streamout_enabled        : 1;
        uint32_t vdenc_pak_obj_cmd_streamout_enabled    : 1;
        uint32_t tlb_prefetch_enabled                   : 1;
        uint32_t pak_threshold_check_enabled            : 1;
        uint32_t vdenc_streamin_enabled                 : 1;
        uint32_t downscaled_8x_write_disable            : 1;
        uint32_t downscaled_4x_write_disable            : 1;
        uint32_t bit_depth                              : 3;
        uint32_t pak_chroma_subsampling_type            : 2;
        uint32_t reserved1                              : 14;
        uint32_t speed_mode_fetch_optimation_enabled    : 1;
    } dw1;
} gen10_vdenc_pipe_mode_select_param;


enum GEN10_VDENC_SURFACE_TYPE {
    GEN10_VDENC_SRC_SURFACE = 0,
    GEN10_VDENC_REF_SURFACE = 1,
    GEN10_VDENC_DS_REF_SURFACE = 2
};

#define GEN10_VDENC_COLOR_SPACE_BT601 0
#define GEN10_VDENC_COLOR_SPACE_BT709 1

#define GEN10_VDENC_TILEWALK_XMAJOR   0
#define GEN10_VDENC_TILEWALK_YMAJOR   1

typedef struct _gen10_vdenc_surface_state_param {
    struct {
        uint32_t crcb_offset_vdirection                 : 2;
        uint32_t surface_format_byte_swizzle            : 1;
        uint32_t color_space_conversion                 : 1;
        uint32_t width                                  : 14;
        uint32_t height                                 : 14;
    } dw0;

    struct {
        uint32_t tile_walk                              : 1;
        uint32_t tiled_surface_enabled                  : 1;
        uint32_t half_pitch_for_chroma_enabled          : 1;
        uint32_t pitch_in_bytes                         : 17;
        uint32_t chroma_downsample_filter_control       : 3;
        uint32_t reserved                               : 4;
        uint32_t interleave_chroma                      : 1;
        uint32_t format                                 : 4;
    } dw1;

    struct {
        uint32_t y_offset_for_cb                        : 15;
        uint32_t reserved0                              : 1;
        uint32_t x_offset_for_cb                        : 15;
        uint32_t reserved1                              : 1;
    } dw2;

    struct {
        uint32_t y_offset_for_cr                        : 16;
        uint32_t x_offset_for_cr                        : 13;
        uint32_t reserved1                              : 3;
    } dw3;
} gen10_vdenc_surface_state_param;

typedef struct _gen10_vdenc_pipe_buf_addr_state_param {
    struct i965_gpe_resource *downscaled_fwd_ref[2];
    struct i965_gpe_resource *downscaled_bwd_ref[1];
    struct i965_gpe_resource *uncompressed_picture;
    struct i965_gpe_resource *stream_data_picture;
    struct i965_gpe_resource *row_store_scratch_buf;
    struct i965_gpe_resource *collocated_mv_buf;
    struct i965_gpe_resource *fwd_ref[3];
    struct i965_gpe_resource *bwd_ref[1];
    struct i965_gpe_resource *statictics_streamout_buf;
    struct i965_gpe_resource *downscaled_fwd_ref_4x[2];
    struct i965_gpe_resource *lcu_pak_obj_cmd_buf;
    struct i965_gpe_resource *scaled_ref_8x;
    struct i965_gpe_resource *scaled_ref_4x;
    struct i965_gpe_resource *vp9_segmentation_map_streamin_buf;
    struct i965_gpe_resource *vp9_segmentation_map_streamout_buf;

    struct {
        uint32_t weights_histogram_streamout_offset;
    } dw61;
} gen10_vdenc_pipe_buf_addr_state_param;

typedef struct _gen10_vdenc_costs_state_param {
    struct {
        uint32_t hme_mvcost0                            : 8;
        uint32_t hme_mvcost1                            : 8;
        uint32_t hme_mvcost2                            : 8;
        uint32_t hme_mvcost3                            : 8;
    } dw1;

    struct {
        uint32_t hme_mvcost4                            : 8;
        uint32_t hme_mvcost5                            : 8;
        uint32_t hme_mvcost6                            : 8;
        uint32_t hme_mvcost7                            : 8;
    } dw2;

    struct {
        uint32_t sad_mvcost0                            : 8;
        uint32_t sad_mvcost1                            : 8;
        uint32_t sad_mvcost2                            : 8;
        uint32_t sad_mvcost3                            : 8;
    } dw3;

    struct {
        uint32_t sad_mvcost4                            : 8;
        uint32_t sad_mvcost5                            : 8;
        uint32_t sad_mvcost6                            : 8;
        uint32_t sad_mvcost7                            : 8;
    } dw4;

    struct {
        uint32_t sad_mvcost8                            : 8;
        uint32_t sad_mvcost9                            : 8;
        uint32_t sad_mvcost10                           : 8;
        uint32_t sad_mvcost11                           : 8;
    } dw5;

    struct {
        uint32_t rd_mvcost0                            : 8;
        uint32_t rd_mvcost1                            : 8;
        uint32_t rd_mvcost2                            : 8;
        uint32_t rd_mvcost3                            : 8;
    } dw6;

    struct {
        uint32_t rd_mvcost4                            : 8;
        uint32_t rd_mvcost5                            : 8;
        uint32_t rd_mvcost6                            : 8;
        uint32_t rd_mvcost7                            : 8;
    } dw7;

    struct {
        uint32_t rd_mvcost8                            : 8;
        uint32_t rd_mvcost9                            : 8;
        uint32_t rd_mvcost10                           : 8;
        uint32_t rd_mvcost11                           : 8;
    } dw8;

    struct {
        uint32_t vp9_near_mv_cost                      : 8;
        uint32_t vp9_nearest_mv_cost                   : 8;
        uint32_t vp9_zero_mv_cost                      : 8;
        uint32_t reserved                              : 8;
    } dw9;

    struct {
        uint32_t skip_8x8_cost                         : 8;
        uint32_t merge_8x8_cost                        : 8;
        uint32_t skip_16x16_cost                       : 8;
        uint32_t merge_16x16_cost                      : 8;
    } dw10;

    struct {
        uint32_t skip_32x32_cost                       : 8;
        uint32_t merge_32x32_cost                      : 8;
        uint32_t skip_64x64_cost                       : 8;
        uint32_t merge_64x64_cost                      : 8;
    } dw11;

    struct {
        uint32_t mode_inter_32x16                      : 8;
        uint32_t mode_inter_16x16                      : 8;
        uint32_t mode_inter_16x8                       : 8;
        uint32_t mode_inter_8x8                        : 8;
    } dw12;

    struct {
        uint32_t mode_inter_32x32                      : 8;
        uint32_t mode_inter_bidir                      : 8;
        uint32_t ref_id_cost                           : 8;
        uint32_t chroma_intra_mode_cost                : 8;
    } dw13;

    struct {
        uint32_t sad_penalty_for_intra_dc_32x32_pred_mode      : 8;
        uint32_t sad_penalty_for_intra_dc_8x8_pred_mode        : 8;
        uint32_t sad_penalty_for_intra_nondc_32x32_pred_mode   : 8;
        uint32_t sad_penalty_for_intra_nondc_8x8_pred_mode     : 8;
    } dw14;

    struct {
        uint32_t rd_penalty_for_intra_dc_32x32_pred_mode       : 8;
        uint32_t rd_penalty_for_intra_dc_8x8_pred_mode         : 8;
        uint32_t rd_penalty_for_intra_nondc_32x32_pred_mode    : 8;
        uint32_t rd_penalty_for_intra_nondc_8x8_pred_mode      : 8;
    } dw15;

    struct {
        uint32_t sad_penalty_for_intra_left_boundary_cu        : 8;
        uint32_t sad_penalty_for_intra_top_boundary_cu         : 8;
        uint32_t sad_intra_nonpred_mode_cost                   : 8;
        uint32_t ibc_ref_id_cost                               : 8;
    } dw16;

    struct {
        uint32_t rd_mode_intra_nonpred                         : 8;
        uint32_t mode_intra_32x32                              : 8;
        uint32_t mode_intra_16x16                              : 8;
        uint32_t mode_intra_8x8                                : 8;
    } dw17;

    struct {
        uint32_t intra_NxN_cost                                : 8;
        uint32_t intra_64x64_cost                              : 8;
        uint32_t mode_intra_2NxN_32x32CU                       : 8;
        uint32_t mode_intra_2NxN_16x16CU                       : 8;
    } dw18;

    struct {
        uint32_t reserved                                      : 8;
        uint32_t tu_depth_cost0                                : 8;
        uint32_t tu_depth_cost1                                : 8;
        uint32_t tu_depth_cost2                                : 8;
    } dw19;

    struct {
        uint32_t intra_tu_4x4_cbf_cost                         : 8;
        uint32_t intra_tu_8x8_cbf_cost                         : 8;
        uint32_t intra_tu_16x16_cbf_cost                       : 8;
        uint32_t intra_tu_32x32_cbf_cost                       : 8;
    } dw20;

    struct {
        uint32_t inter_tu_4x4_cbf_cost                         : 8;
        uint32_t inter_tu_8x8_cbf_cost                         : 8;
        uint32_t inter_tu_16x16_cbf_cost                       : 8;
        uint32_t inter_tu_32x32_cbf_cost                       : 8;
    } dw21;

    struct {
        uint32_t intra_tu_4x4_nzc                              : 8;
        uint32_t intra_tu_8x8_nzc                              : 8;
        uint32_t intra_tu_16x16_nzc                            : 8;
        uint32_t intra_tu_32x32_nzc                            : 8;
    } dw22;

    struct {
        uint32_t intra_tu_4x4_nsigc                            : 8;
        uint32_t intra_tu_8x8_nsigc                            : 8;
        uint32_t intra_tu_16x16_nsigc                          : 8;
        uint32_t intra_tu_32x32_nsigc                          : 8;
    } dw23;

    struct {
        uint32_t intra_tu_4x4_nsubsetc                         : 8;
        uint32_t intra_tu_8x8_nsubsetc                         : 8;
        uint32_t intra_tu_16x16_nsubsetc                       : 8;
        uint32_t intra_tu_32x32_nsubsetc                       : 8;
    } dw24;

    struct {
        uint32_t intra_tu_4x4_nlevelc                          : 8;
        uint32_t intra_tu_8x8_nlevelc                          : 8;
        uint32_t intra_tu_16x16_nlevelc                        : 8;
        uint32_t intra_tu_32x32_nlevelc                        : 8;
    } dw25;

    struct {
        uint32_t inter_tu_4x4_nzc                              : 8;
        uint32_t inter_tu_8x8_nzc                              : 8;
        uint32_t inter_tu_16x16_nzc                            : 8;
        uint32_t inter_tu_32x32_nzc                            : 8;
    } dw26;

    struct {
        uint32_t inter_tu_4x4_nsigc                            : 8;
        uint32_t inter_tu_8x8_nsigc                            : 8;
        uint32_t inter_tu_16x16_nsigc                          : 8;
        uint32_t inter_tu_32x32_nsigc                          : 8;
    } dw27;

    struct {
        uint32_t inter_tu_4x4_nsubsetc                         : 8;
        uint32_t inter_tu_8x8_nsubsetc                         : 8;
        uint32_t inter_tu_16x16_nsubsetc                       : 8;
        uint32_t inter_tu_32x32_nsubsetc                       : 8;
    } dw28;

    struct {
        uint32_t inter_tu_4x4_nlevelc                          : 8;
        uint32_t inter_tu_8x8_nlevelc                          : 8;
        uint32_t inter_tu_16x16_nlevelc                        : 8;
        uint32_t inter_tu_32x32_nlevelc                        : 8;
    } dw29;
} gen10_vdenc_costs_state_param;

#define GEN10_PICTURE_TYPE_I        0
#define GEN10_PICTURE_TYPE_P        1
#define GEN10_PICTURE_TYPE_B        2
#define GEN10_PICTURE_TYPE_GPB      3

typedef struct _gen10_vdenc_hevc_vp9_img_state_param {
    struct {
        uint32_t frame_width_in_pixels_minus_one               : 16;
        uint32_t frame_height_in_pixels_minus_one              : 16;
    } dw1;

    struct {
        uint32_t max_cu_size                                   : 2;
        uint32_t min_cu_size                                   : 2;
        uint32_t max_tu_size_cu64x64_inter                     : 2;
        uint32_t max_tu_size_cu64x64_intra                     : 2;
        uint32_t max_tu_size_cu32x32_inter                     : 2;
        uint32_t max_tu_size_cu32x32_intra                     : 2;
        uint32_t max_tu_size_cu16x16_inter                     : 2;
        uint32_t max_tu_size_cu16x16_intra                     : 2;
        uint32_t max_tu_size_cu8x8_inter                       : 2;
        uint32_t max_tu_size_cu8x8_intra                       : 2;
        uint32_t picture_type                                  : 2;
        uint32_t temporal_mvp_enabled                          : 1;
        uint32_t collocated_from_l0_flag                       : 1;
        uint32_t long_term_reference_flag_l0                   : 3;
        uint32_t long_term_reference_flag_l1                   : 1;
        uint32_t lcu_size_control                              : 1;
        uint32_t tu_4x4_disable                                : 1;
        uint32_t transform_skip                                : 1;
        uint32_t costrained_intra_pred_flag                    : 1;
    } dw2;

    struct {
        uint32_t poc_number_for_ref_id0_in_l0                  : 8;
        uint32_t poc_number_for_ref_id0_in_l1                  : 8;
        uint32_t poc_number_for_ref_id1_in_l0                  : 8;
        uint32_t poc_number_for_ref_id1_in_l1                  : 8;
    } dw3;

    struct {
        uint32_t poc_number_for_ref_id2_in_l0                  : 8;
        uint32_t poc_number_for_ref_id2_in_l1                  : 8;
        uint32_t poc_number_for_ref_id3_in_l0                  : 8;
        uint32_t poc_number_for_ref_id3_in_l1                  : 8;
    } dw4;

    struct {
        uint32_t intra_chroma_sad_wt                           : 3;
        uint32_t intra_chroma_mode_mask                        : 5;
        uint32_t streamin_roi_enabled                          : 1;
        uint32_t streamin_panic_enabled                        : 1;
        uint32_t sub_pel_mode                                  : 2;
        uint32_t intra_sad_measure_adjustment                  : 2;
        uint32_t inter_sad_measure_adjustment                  : 2;
        uint32_t bme_disable_for_fbr                           : 1;
        uint32_t bilinear_filter_enabled                       : 1;
        uint32_t luma_intra_partition_mask                     : 5;
        uint32_t ref_id_cost_mode_select                       : 1;
        uint32_t num_refidx_l0_minus1                          : 4;
        uint32_t num_refidx_l1_minus1                          : 4;
    } dw5;

    struct {
        uint32_t intra_8x8_mode_mask                           : 10;
        uint32_t intra_16x16_mode_mask                         : 10;
        uint32_t intra_32x32_mode_mask                         : 10;
        uint32_t vp9_intra_2NxN_NX2n_partition_masks           : 2;
    } dw6;

    struct {
        uint32_t max_dqp_depth                                 : 4;
        uint32_t segmentation_enabled                          : 1;
        uint32_t segmentation_map_temporal_prediction_enabled  : 1;
        uint32_t reserved                                      : 1;
        uint32_t tiling_enabled                                : 1;
        uint32_t speed_mode                                    : 1;
        uint32_t vdenc_streamin_enabled                        : 1;
        uint32_t use_left_recon_pix_rollingI                   : 1;
        uint32_t use_left_recon_pix                            : 1;
        uint32_t stage3_mv_threshold                           : 4;
        uint32_t pak_only_multi_pass_enabled                   : 1;
        uint32_t pak_prefetch_enabled                          : 1;
        uint32_t cre_prefetch_enabled                          : 1;
        uint32_t hme_ref1_disable                              : 1;
        uint32_t time_budget_overflow_check_enabled            : 1;
        uint32_t stage1_dual_list_winner_en                    : 1;
        uint32_t stage2_hme_disable                            : 1;
        uint32_t stage1_hme_disable                            : 1;
        uint32_t inter_shape_mask                              : 8;
    } dw7;

    struct {
        uint32_t stage3_stream0_predictor_list_priority        : 2;
        uint32_t stage3_stream1_predictor_list_priority        : 2;
        uint32_t stage3_stream2_predictor_list_priority        : 2;
        uint32_t stage3_stream3_predictor_list_priority        : 2;
        uint32_t stage3_zmv_l0_predictor_list_priority         : 2;
        uint32_t stage3_temp_mv_predictor_list_priority        : 2;
        uint32_t stage3_hme0_predictor_list_priority           : 2;
        uint32_t stage3_hme1_predictor_list_priority           : 2;
        uint32_t stage3_cc0123_list0_predictor_list_priority   : 8;
        uint32_t stage3_cc0123_list1_predictor_list_priority   : 8;
    } dw8;

    struct {
        uint32_t stage3_shmd0123_predictor_list_priorty        : 8;
        uint32_t stage3_zmv_l1_predictor_list_priority         : 2;
        uint32_t reserved                                      : 6;
        uint32_t num_beta_predictors                           : 4;
        uint32_t num_ime_predictors                            : 4;
        uint32_t num_merge_candidate_cu_32x32                  : 4;
        uint32_t num_merge_candidate_cu_64x64                  : 4;
    } dw9;

    struct {
        uint32_t hme0_x_offset                                 : 8;
        uint32_t hme0_y_offset                                 : 8;
        uint32_t hme1_x_offset                                 : 8;
        uint32_t hme1_y_offset                                 : 8;
    } dw10;

    struct {
        uint32_t vdenc_cache_priority;
    } dw11;

    struct {
        uint32_t lcu_budget                                    : 16;
        uint32_t initial_time                                  : 16;
    } dw12;

    struct {
        uint32_t roi_qp_adjustment_for_zone0                   : 4;
        uint32_t roi_qp_adjustment_for_zone1                   : 4;
        uint32_t roi_qp_adjustment_for_zone2                   : 4;
        uint32_t roi_qp_adjustment_for_zone3                   : 4;
        uint32_t qp_adjustment_for_current_cu_with_tu_4x4      : 4;
        uint32_t qp_adjustment_for_current_cu_with_tu_8x8      : 4;
        uint32_t qp_adjustment_for_current_cu_with_tu_16x16    : 4;
        uint32_t qp_adjustment_for_current_cu_with_tu_32x32    : 4;
    } dw13;

    struct {
        uint32_t best_distortion_qp_adjustment_for_zone0       : 4;
        uint32_t best_distortion_qp_adjustment_for_zone1       : 4;
        uint32_t best_distortion_qp_adjustment_for_zone2       : 4;
        uint32_t best_distortion_qp_adjustment_for_zone3       : 4;
        uint32_t sad_har_threshold0                            : 16;
    } dw14;

    struct {
        uint32_t sad_har_threshold1                            : 16;
        uint32_t sad_har_threshold2                            : 16;
    } dw15;

    struct {
        uint32_t min_qp                                        : 8;
        uint32_t max_qp                                        : 8;
        uint32_t delta_qp_for_non_angintra                     : 4;
        uint32_t delta_qp_for_angintra                         : 4;
        uint32_t max_delta_qp                                  : 4;
        uint32_t mv_length_qp_adjustment_for_zero_mv           : 4;
    } dw16;

    struct {
        uint32_t mid_point_sad_haar                            : 20;
        uint32_t reserved                                      : 12;
    } dw17;

    struct {
        uint32_t mv_length_qp_adjustment_for_zone0             : 4;
        uint32_t mv_length_qp_adjustment_for_zone1             : 4;
        uint32_t mv_length_qp_adjustment_for_zone2             : 4;
        uint32_t reserved                                      : 4;
        uint32_t mv_length_threshold0                          : 16;
    } dw18;

    struct {
        uint32_t mv_length_threshold1                          : 16;
        uint32_t lcu_estimated_size_adjustment                 : 8;
        uint32_t num_bits_multiplier                           : 7;
        uint32_t reserved                                      : 1;
    } dw19;

    struct {
        uint32_t panic_initial_size_in_dw                      : 16;
        uint32_t panic_enabled                                 : 1;
        uint32_t reserved                                      : 15;
    } dw20;

    struct {
        uint32_t widi_intra_refresh_pos                        : 9;
        uint32_t reserved0                                     : 7;
        uint32_t widi_intra_refresh_mb_size_minus_one          : 8;
        uint32_t widi_intra_refresh_mode                       : 1;
        uint32_t widi_intra_refresh_eable                      : 1;
        uint32_t reserved1                                     : 2;
        uint32_t qp_ajustment_for_rolling_I                    : 4;
    } dw21;

    struct {
        uint32_t low_luma_intra_quarter_block_flatness_threshold_minus_one  : 5;
        uint32_t low_luma_intra_total_block_sad_threshold                   : 3;
        uint32_t high_luma_intra_quarter_block_flatness_threshold_minus_one : 5;
        uint32_t high_luma_intra_total_block_sad_threshold                  : 3;
        uint32_t low_inter_quarter_block_flatness_threshold_minus_one       : 5;
        uint32_t low_inter_total_block_sad_threshold                        : 3;
        uint32_t high_inter_quarter_block_flatness_threshold_minus_one      : 5;
        uint32_t high_inter_total_block_sad_threshold                       : 3;
    } dw22;

    struct {
        uint32_t low_chroma_intra_quarter_block_flatness_threshold_minus_one  : 5;
        uint32_t low_chroma_intra_total_block_sad_threshold                   : 3;
        uint32_t high_chroma_intra_quarter_block_flatness_threshold_minus_one : 5;
        uint32_t high_chroma_intra_total_block_sad_threshold                  : 3;
        uint32_t intra_cu_depth_control                                       : 8;
        uint32_t inter_cu_depth_control                                       : 8;
    } dw23;

    struct {
        uint32_t qp_for_segment0                                : 8;
        uint32_t qp_for_segment1                                : 8;
        uint32_t qp_for_segment2                                : 8;
        uint32_t qp_for_segment3                                : 8;
    } dw24;

    struct {
        uint32_t qp_for_segment4                                : 8;
        uint32_t qp_for_segment5                                : 8;
        uint32_t qp_for_segment6                                : 8;
        uint32_t qp_for_segment7                                : 8;
    } dw25;

    struct {
        uint32_t rd_qp_lambda                                   : 16;
        uint32_t sad_qp_lambda                                  : 9;
        uint32_t vp9_dynamic_slice_enabled                      : 1;
        uint32_t reserved                                       : 2;
        uint32_t delata_qp_for_second_pass                      : 4;
    } dw26;

    struct {
        uint32_t qp_prime_Y_DC                                  : 8;
        uint32_t qp_prime_Y_AC                                  : 8;
        uint32_t panic_mode_lcu_threshold                       : 16;
    } dw27;

    struct {
        uint32_t rounding_threshold_intra_cu_32x32              : 16;
        uint32_t rounding_threshold_merge_cu_32x32              : 16;
    } dw28;

    struct {
        uint32_t rounding_threshold_inter_cu_32x32              : 16;
        uint32_t rounding_threshold_merge_cu_16x16              : 16;
    } dw29;

    struct {
        uint32_t rounding_threshold_intra_cu_16x16              : 16;
        uint32_t rounding_threshold_inter_cu_16x16              : 16;
    } dw30;

    struct {
        uint32_t rounding_threshold_merge_cu_8x8                : 16;
        uint32_t rounding_threshold_intra_cu_8x8                : 16;
    } dw31;

    struct {
        uint32_t rounding_threshold_inter_cu_8x8                : 16;
        uint32_t rounding_select0_merge_cu_32x32                : 4;
        uint32_t rounding_select1_merge_cu_32x32                : 4;
        uint32_t rounding_select0_intra_cu_32x32                : 4;
        uint32_t rounding_select1_intra_cu_32x32                : 4;
    } dw32;

    struct {
        uint32_t rounding_select0_inter_cu_32x32                : 4;
        uint32_t rounding_select1_inter_cu_32x32                : 4;
        uint32_t rounding_select0_merge_cu_16x16                : 4;
        uint32_t rounding_select1_merge_cu_16x16                : 4;
        uint32_t rounding_select0_intra_cu_16x16                : 4;
        uint32_t rounding_select1_intra_cu_16x16                : 4;
        uint32_t rounding_select0_inter_cu_16x16                : 4;
        uint32_t rounding_select1_inter_cu_16x16                : 4;
    } dw33;

    struct {
        uint32_t rounding_select0_merge_cu_8x8                  : 4;
        uint32_t rounding_select1_merge_cu_8x8                  : 4;
        uint32_t rounding_select0_intra_cu_8x8                  : 4;
        uint32_t rounding_select1_intra_cu_8x8                  : 4;
        uint32_t rounding_select0_inter_cu_8x8                  : 4;
        uint32_t rounding_select1_inter_cu_8x8                  : 4;
        uint32_t num_merge_candidate_cu_8x8                     : 4;
        uint32_t num_merge_candidate_cu_16x16                   : 4;
    } dw34;

    struct {
        uint32_t delta_lcu_byte_cost                            : 8;
        uint32_t num_32x32_in_flight                            : 4;
        uint32_t performance_stats_streamout_enabled            : 1;
        uint32_t perf_opt_I4x4                                  : 3;
        uint32_t time_budget_check_disable_lcu_num              : 16;
    } dw35;

    struct {
        uint32_t widi_intra_refresh_boundary_ref0               : 9;
        uint32_t reserved0                                      : 1;
        uint32_t widi_intra_refresh_boundary_ref1               : 9;
        uint32_t reserved1                                      : 1;
        uint32_t widi_intra_refresh_boundary_ref2               : 9;
        uint32_t reserved2                                      : 3;
    } dw36;
} gen10_vdenc_hevc_vp9_img_state_param;

typedef struct _gen10_vdenc_walker_state_param {
    struct {
        uint32_t lcu_start_y_position                           : 9;
        uint32_t reserved0                                      : 7;
        uint32_t lcu_start_x_position                           : 9;
        uint32_t reserved1                                      : 3;
        uint32_t first_super_slice                              : 1;
        uint32_t reserved2                                      : 3;
    } dw1;

    struct {
        uint32_t next_slice_lcu_start_y_position                : 10;
        uint32_t reserved0                                      : 6;
        uint32_t next_slice_lcu_start_x_position                : 10;
        uint32_t reserved1                                      : 6;
    } dw2;

    struct {
        uint32_t log2_weight_denom_luma                         : 3;
        uint32_t reserved                                       : 29;
    } dw3;

    struct {
        uint32_t tile_start_ctu_y                               : 16;
        uint32_t tile_start_ctu_x                               : 16;
    } dw4;

    struct {
        uint32_t tile_width                                     : 16;
        uint32_t tile_height                                    : 16;
    } dw5;
} gen10_vdenc_walker_state_param;

typedef struct _gen10_vdenc_weightsoffsets_state_param {
    struct {
        uint32_t weights_forward_reference0                     : 8;
        uint32_t offset_forward_reference0                      : 8;
        uint32_t weights_forward_reference1                     : 8;
        uint32_t offset_forward_reference1                      : 8;
    } dw1;

    struct {
        uint32_t weights_forward_reference2                     : 8;
        uint32_t offset_forward_reference2                      : 8;
        uint32_t reserved                                       : 16;
    } dw2;

    struct {
        uint32_t hevc_vp9_weights_forward_reference0            : 8;
        uint32_t hevc_vp9_offset_forward_reference0             : 8;
        uint32_t hevc_vp9_weights_forward_reference1            : 8;
        uint32_t hevc_vp9_offset_forward_reference1             : 8;
    } dw3;

    struct {
        uint32_t hevc_vp9_weights_backward_reference2           : 8;
        uint32_t hevc_vp9_offset_backward_reference2            : 8;
        uint32_t hevc_vp9_weights_backward_reference0           : 8;
        uint32_t hevc_vp9_offset_backward_reference0            : 8;
    } dw4;
} gen10_vdenc_weightsoffsets_state_param;

void
gen10_vdenc_vd_pipeline_flush(VADriverContextP ctx,
                              struct intel_batchbuffer *batch,
                              gen10_vdenc_vd_pipeline_flush_param *param);
void
gen10_vdenc_pipe_mode_select(VADriverContextP ctx,
                             struct intel_batchbuffer *batch,
                             gen10_vdenc_pipe_mode_select_param *param);

void
gen10_vdenc_surface_state(VADriverContextP ctx,
                          struct intel_batchbuffer *batch,
                          enum GEN10_VDENC_SURFACE_TYPE type,
                          gen10_vdenc_surface_state_param *surface0,
                          gen10_vdenc_surface_state_param *surface1);

void
gen10_vdenc_walker_state(VADriverContextP ctx,
                         struct intel_batchbuffer *batch,
                         gen10_vdenc_walker_state_param *param);
void
gen10_vdenc_weightsoffsets_state(VADriverContextP ctx,
                                 struct intel_batchbuffer *batch,
                                 gen10_vdenc_weightsoffsets_state_param *param);
void
gen10_vdenc_pipe_buf_addr_state(VADriverContextP ctx,
                                struct intel_batchbuffer *batch,
                                gen10_vdenc_pipe_buf_addr_state_param *param);
#endif
