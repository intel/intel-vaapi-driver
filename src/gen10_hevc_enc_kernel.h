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

#ifndef _GEN10_HEVC_ENC_KERNEL_H
#define _GEN10_HEVC_ENC_KERNEL_H

typedef struct _gen10_hevc_concurrent_tg_data_ {
    uint16_t curr_tg_start_lcu_index;
    uint16_t curr_tg_end_luc_index;
    uint16_t curr_tg_index;
    uint16_t reserved0;
    uint16_t curr_wf_lcu_idx_x;
    uint16_t curr_wf_lcu_idx_y;
    uint16_t curr_wf_lcu_idx1_y;
    uint16_t next_wf_lcu_idx_x;
    uint16_t curr_wf_yoffset;
    uint16_t reserved1[23];
} gen10_hevc_concurrent_tg_data;

typedef struct _gen10_hevc_lcu_level_data_ {
    int16_t    slice_start_lcu_idx;
    int16_t    slice_end_lcu_idx;
    int16_t    slice_id;
    int16_t    slice_qp;
    int16_t    reserved[4];
} gen10_hevc_lcu_level_data;

typedef struct _gen10_hevc_pak_stats_info_ {
    uint32_t hcp_bs_frame;
    uint32_t hcp_bs_frame_noheader;
    uint32_t hcp_image_status_control;
    uint32_t reserved0;
    uint32_t hcp_image_status_ctl_last_pass;
    uint32_t reserved1[3];
} gen10_hevc_pak_stats_info;

typedef struct _gen10_hevc_brc_init_curbe_data {
    struct {
        uint32_t   profile_level_max_frame;
    } dw0;

    struct {
        uint32_t   init_buf_full;
    } dw1;

    struct {
        uint32_t   buf_size;
    } dw2;

    struct {
        uint32_t   target_bit_rate;
    } dw3;

    struct {
        uint32_t   maximum_bit_rate;
    } dw4;

    struct {
        uint32_t   minimum_bit_rate;
    } dw5;

    struct {
        uint32_t   frame_ratem;
    } dw6;

    struct {
        uint32_t   frame_rated;
    } dw7;

    struct {
        uint32_t   brc_flag     : 16;
        uint32_t   brc_gopp     : 16;
    } dw8;

    struct {
        uint32_t   brc_gopb     : 16;
        uint32_t   frame_width  : 16;
    } dw9;

    struct {
        uint32_t   frame_height  : 16;
        uint32_t   avbr_accuracy : 16;
    } dw10;

    struct {
        uint32_t   avbr_convergence : 16;
        uint32_t   minimum_qp       : 16;
    } dw11;

    struct {
        uint32_t   maximum_qp       : 16;
        uint32_t   number_slice     : 16;
    } dw12;

    struct {
        uint32_t   reserved         : 16;
        uint32_t   brc_gopb1        : 16;
    } dw13;

    struct {
        uint32_t   brc_gopb2        : 16;
        uint32_t   max_brc_level    : 16;
    } dw14;

    struct {
        uint32_t   long_term_interval  : 16;
        uint32_t   reserved            : 16;
    } dw15;

    struct {
        uint32_t   instant_rate_thr0_pframe : 8;
        uint32_t   instant_rate_thr1_pframe : 8;
        uint32_t   instant_rate_thr2_pframe : 8;
        uint32_t   instant_rate_thr3_pframe : 8;
    } dw16;

    struct {
        uint32_t   instant_rate_thr0_bframe : 8;
        uint32_t   instant_rate_thr1_bframe : 8;
        uint32_t   instant_rate_thr2_bframe : 8;
        uint32_t   instant_rate_thr3_bframe : 8;
    } dw17;

    struct {
        uint32_t   instant_rate_thr0_iframe : 8;
        uint32_t   instant_rate_thr1_iframe : 8;
        uint32_t   instant_rate_thr2_iframe : 8;
        uint32_t   instant_rate_thr3_iframe : 8;
    } dw18;

    struct {
        uint32_t   deviation_thr0_pbframe  : 8;
        uint32_t   deviation_thr1_pbframe  : 8;
        uint32_t   deviation_thr2_pbframe  : 8;
        uint32_t   deviation_thr3_pbframe  : 8;
    } dw19;

    struct {
        uint32_t   deviation_thr4_pbframe  : 8;
        uint32_t   deviation_thr5_pbframe  : 8;
        uint32_t   deviation_thr6_pbframe  : 8;
        uint32_t   deviation_thr7_pbframe  : 8;
    } dw20;

    struct {
        uint32_t   deviation_thr0_vbrctrl  : 8;
        uint32_t   deviation_thr1_vbrctrl  : 8;
        uint32_t   deviation_thr2_vbrctrl  : 8;
        uint32_t   deviation_thr3_vbrctrl  : 8;
    } dw21;

    struct {
        uint32_t   deviation_thr4_vbrctrl  : 8;
        uint32_t   deviation_thr5_vbrctrl  : 8;
        uint32_t   deviation_thr6_vbrctrl  : 8;
        uint32_t   deviation_thr7_vbrctrl  : 8;
    } dw22;

    struct {
        uint32_t   deviation_thr0_iframe   : 8;
        uint32_t   deviation_thr1_iframe   : 8;
        uint32_t   deviation_thr2_iframe   : 8;
        uint32_t   deviation_thr3_iframe   : 8;
    } dw23;

    struct {
        uint32_t   deviation_thr4_iframe   : 8;
        uint32_t   deviation_thr5_iframe   : 8;
        uint32_t   deviation_thr6_iframe   : 8;
        uint32_t   deviation_thr7_iframe   : 8;
    } dw24;

    struct {
        uint32_t   ac_qp_buffer            : 8;
        uint32_t   intra_sad_tr            : 8;
        uint32_t   log2_max_cu_size        : 8;
        uint32_t   sliding_wind_size       : 8;
    } dw25;

    struct {
        uint32_t   reserved;
    } dw26;

    struct {
        uint32_t   reserved;
    } dw27;

    struct {
        uint32_t   reserved;
    } dw28;

    struct {
        uint32_t   reserved;
    } dw29;

    struct {
        uint32_t   reserved;
    } dw30;

    struct {
        uint32_t   reserved;
    } dw31;
} gen10_hevc_brc_init_curbe_data;

typedef struct _gen10_hevc_brc_update_curbe_data {
    struct {
        uint32_t   target_size;
    } dw0;

    struct {
        uint32_t   frame_num;
    } dw1;

    struct {
        uint32_t   picture_header_size;
    } dw2;

    struct {
        uint32_t   start_gadj_frame0    : 16;
        uint32_t   start_gadj_frame1    : 16;
    } dw3;

    struct {
        uint32_t   start_gadj_frame2    : 16;
        uint32_t   start_gadj_frame3    : 16;
    } dw4;

    struct {
        uint32_t   target_size_flag    : 8;
        uint32_t   reserved            : 8;
        uint32_t   max_num_paks        : 8;
        uint32_t   curr_frame_brclevel : 8;
    } dw5;

    struct {
        uint32_t   num_skipped_frames  : 8;
        uint32_t   cqp_value           : 8;
        uint32_t   new_feature_flag    : 8;
        uint32_t   roi_ratio           : 8;
    } dw6;

    struct {
        uint32_t   reserved;
    } dw7;

    struct {
        uint32_t   start_gadj_mult0  : 8;
        uint32_t   start_gadj_mult1  : 8;
        uint32_t   start_gadj_mult2  : 8;
        uint32_t   start_gadj_mult3  : 8;
    } dw8;

    struct {
        uint32_t   start_gadj_mult4  : 8;
        uint32_t   start_gadj_divd0  : 8;
        uint32_t   start_gadj_divd1  : 8;
        uint32_t   start_gadj_divd2  : 8;
    } dw9;

    struct {
        uint32_t   start_gadj_divd3  : 8;
        uint32_t   start_gadj_divd4  : 8;
        uint32_t   qp_threshold0     : 8;
        uint32_t   qp_threshold1     : 8;
    } dw10;

    struct {
        uint32_t   qp_threshold2     : 8;
        uint32_t   qp_threshold3     : 8;
        uint32_t   grate_ratio_thr0  : 8;
        uint32_t   grate_ratio_thr1  : 8;
    } dw11;

    struct {
        uint32_t   grate_ratio_thr2  : 8;
        uint32_t   grate_ratio_thr3  : 8;
        uint32_t   grate_ratio_thr4  : 8;
        uint32_t   grate_ratio_thr5  : 8;
    } dw12;

    struct {
        uint32_t   grate_ratio_thr6  : 8;
        uint32_t   grate_ratio_thr7  : 8;
        uint32_t   grate_ratio_thr8  : 8;
        uint32_t   grate_ratio_thr9  : 8;
    } dw13;

    struct {
        uint32_t   grate_ratio_thr10  : 8;
        uint32_t   grate_ratio_thr11  : 8;
        uint32_t   grate_ratio_thr12  : 8;
        uint32_t   parallel_mode      : 8;
    } dw14;

    struct {
        uint32_t   size_of_skipped_frames;
    } dw15;
} gen10_hevc_brc_update_curbe_data;

typedef struct _gen10_hevc_scaling_curbe_data_ {
    struct {
        uint32_t    input_bit_depth_for_chroma : 8;
        uint32_t    input_bit_depth_for_luma   : 8;
        uint32_t    output_bit_depth_for_chroma: 8;
        uint32_t    output_bit_depth_for_luma  : 7;
        uint32_t    rounding_enabled           : 1;
    } dw0;

    struct {
        uint32_t    picture_format             : 8;
        uint32_t    convert_flag               : 1;
        uint32_t    downscale_stage            : 3;
        uint32_t    mb_statistics_dump_flag    : 1;
        uint32_t    reserved0                  : 2;
        uint32_t    lcu_size                   : 1;
        uint32_t    job_queue_size             : 16;
    } dw1;

    struct {
        uint32_t    orig_pic_width_in_pixel  : 16;
        uint32_t    orig_pic_height_in_pixel : 16;
    } dw2;

    struct {
        uint32_t    bti_input_conversion_surface;
    } dw3;

    union {
        uint32_t    bti_output_conversion_surface;
        uint32_t    bti_input_ds_surface;
    } dw4;

    struct {
        uint32_t    bti_4x_ds_surface;
    } dw5;

    struct {
        uint32_t    bti_mbstat_surface;
    } dw6;

    struct {
        uint32_t    bti_2x_ds_surface;
    } dw7;

    struct {
        uint32_t    bti_mb_split_surface;
    } dw8;

    struct {
        uint32_t    bti_lcu32_jobqueue_buffer_surface;
    } dw9;

    struct {
        uint32_t    bti_lcu64_lcu32_jobqueue_buffer_surface;
    } dw10;

    struct {
        uint32_t    bti_lcu64_cu32_distortion_surface;
    } dw11;
} gen10_hevc_scaling_curbe_data;

typedef struct _gen10_hevc_me_curbe_data_ {
    struct {
        uint32_t    rounded_frame_width_in_mv_for4x  : 16;
        uint32_t    rounded_frame_height_in_mv_for4x : 16;
    } dw0;

    struct {
        uint32_t    reserved0             : 16;
        uint32_t    mv_cost_scale_factor  : 2;
        uint32_t    reserved1             : 14;
    } dw1;

    struct {
        uint32_t    reserved0             : 16;
        uint32_t    sub_pel_mode          : 2;
        uint32_t    bme_disable_fbr       : 1;
        uint32_t    reserved1             : 1;
        uint32_t    inter_sad_adj         : 2;
        uint32_t    reserved2             : 10;
    } dw2;

    struct {
        uint32_t    reserved0             : 1;
        uint32_t    adaptive_search_en    : 1;
        uint32_t    reserved1             : 14;
        uint32_t    ime_ref_window_size   : 2;
        uint32_t    reserved2             : 14;
    } dw3;

    struct {
        uint32_t    reserved0              : 8;
        uint32_t    quarter_quad_tree_cand : 5;
        uint32_t    reserved1              : 3;
        uint32_t    bi_weight              : 6;
        uint32_t    reserved2              : 10;
    } dw4;

    struct {
        uint32_t    len_sp                  : 8;
        uint32_t    max_num_su             : 8;
        uint32_t    start_center0_x        : 4;
        uint32_t    start_center0_y        : 4;
        uint32_t    reserved               : 8;
    } dw5;

    struct {
        uint32_t    reserved0              : 1;
        uint32_t    slice_type             : 1;
        uint32_t    hme_stage              : 2;
        uint32_t    num_ref_l0             : 2;
        uint32_t    num_ref_l1             : 2;
        uint32_t    reserved1              : 24;
    } dw6;

    struct {
        uint32_t    rounded_frame_width_in_mv_for16x : 16;
        uint32_t    rounded_frame_height_in_mv_for16x : 16;
    } dw7;

    uint32_t    ime_search_path_03;
    uint32_t    ime_search_path_47;
    uint32_t    ime_search_path_811;
    uint32_t    ime_search_path_1215;
    uint32_t    ime_search_path_1619;
    uint32_t    ime_search_path_2023;
    uint32_t    ime_search_path_2427;
    uint32_t    ime_search_path_2831;
    uint32_t    ime_search_path_3235;
    uint32_t    ime_search_path_3639;
    uint32_t    ime_search_path_4043;
    uint32_t    ime_search_path_4447;
    uint32_t    ime_search_path_4851;
    uint32_t    ime_search_path_5255;
    uint32_t    ime_search_path_5659;
    uint32_t    ime_search_path_6063;

    struct {
        uint32_t    reserved0                  : 6;
        uint32_t    coding_unit_size           : 2;
        uint32_t    reserved1                  : 4;
        uint32_t    coding_unit_partition_mode : 3;
        uint32_t    coding_unit_prediction_mode: 1;
        uint32_t    reserved2           : 16;
    } dw24;

    struct {
        uint32_t    frame_width_in_pixel_cs      : 16;
        uint32_t    frame_height_in_pixel_cs     : 16;
    } dw25;

    struct {
        uint32_t    intra8x8_mode_mask           : 10;
        uint32_t    reserved0                    : 6;
        uint32_t    intra16x16_mode_mask         : 9;
        uint32_t    reserved1                    : 7;
    } dw26;

    struct {
        uint32_t    intra32x32_mode_mask         : 4;
        uint32_t    intra_chroma_mode_mask       : 5;
        uint32_t    intra_compute_type           : 2;
        uint32_t    reserved0                    : 21;
    } dw27;

    struct {
        uint32_t    reserved0                    : 8;
        uint32_t    penalty_intra32x32_nondc     : 8;
        uint32_t    penalty_intra16x16_nondc     : 8;
        uint32_t    penalty_intra8x8_nondc       : 8;
    } dw28;

    struct {
        uint32_t    mode0_cost                   : 8;
        uint32_t    mode1_cost                   : 8;
        uint32_t    mode2_cost                   : 8;
        uint32_t    mode3_cost                   : 8;
    } dw29;

    struct {
        uint32_t    mode4_cost                   : 8;
        uint32_t    mode5_cost                   : 8;
        uint32_t    mode6_cost                   : 8;
        uint32_t    mode7_cost                   : 8;
    } dw30;

    struct {
        uint32_t    mode8_cost                   : 8;
        uint32_t    mode9_cost                   : 8;
        uint32_t    reserved0                    : 8;
        uint32_t    chroma_intra_mode_cost       : 8;
    } dw31;

    struct {
        uint32_t    reserved0                    : 8;
        uint32_t    sicintra_neighbor_avail_flag : 6;
        uint32_t    reserved1                    : 6;

        uint32_t    sic_inter_sad_measure        : 2;
        uint32_t    sic_intra_sad_measure        : 2;
        uint32_t    reserved2                    : 8;
    } dw32;

    struct {
        uint32_t    sic_log2_min_cu_size         : 8;
        uint32_t    reserved0                    : 12;
        uint32_t    sic_only_harr                : 1;
        uint32_t    reserved1                    : 3;
        uint32_t    sic_hevc_quarter_quadtree    : 5;
        uint32_t    reserved2                    : 3;
    } dw33;

    struct {
        uint32_t    bti_hme_output_mv_data_surface;
    } dw34;

    struct {
        uint32_t    bti_16xinput_mv_data_surface;
    } dw35;

    struct {
        uint32_t    bti_4x_output_distortion_surface;
    } dw36;

    struct {
        uint32_t    bti_vme_input_surface;
    } dw37;

    struct {
        uint32_t    bti_4xds_surface;
    } dw38;

    struct {
        uint32_t    bti_brc_distortion_surface;
    } dw39;

    struct {
        uint32_t    bti_mv_and_distortion_sum_surface;
    } dw40;
} gen10_hevc_me_curbe_data;

typedef struct _gen10_hevc_mbenc_intra_curbe_data {
    struct {
        uint32_t  frame_width_in_pixel      : 16;
        uint32_t  frame_height_in_pixel     : 16;
    } dw0;

    struct {
        uint32_t  reserved0                 : 8;
        uint32_t  penalty_intra32x32_nondc_pred : 8;
        uint32_t  penalty_intra16x16_nondc_pred : 8;
        uint32_t  penalty_intra8x8_nondc_pred   : 8;
    } dw1;

    struct {
        uint32_t  reserved0                : 6;
        uint32_t  intra_sad_measure_adj    : 2;
        uint32_t  intra_prediction         : 3;
        uint32_t  reserved1                : 21;
    } dw2;

    struct {
        uint32_t  mode0_cost               : 8;
        uint32_t  mode1_cost               : 8;
        uint32_t  mode2_cost               : 8;
        uint32_t  mode3_cost               : 8;
    } dw3;

    struct {
        uint32_t  mode4_cost               : 8;
        uint32_t  mode5_cost               : 8;
        uint32_t  mode6_cost               : 8;
        uint32_t  mode7_cost               : 8;
    } dw4;

    struct {
        uint32_t  mode8_cost               : 8;
        uint32_t  mode9_cost               : 8;
        uint32_t  ref_id_cost              : 8;
        uint32_t  chroma_intra_mode_cost   : 8;
    } dw5;

    struct {
        uint32_t  log2_max_cu_size         : 4;
        uint32_t  log2_min_cu_size         : 4;
        uint32_t  log2_max_tu_size         : 4;
        uint32_t  log2_min_tu_size         : 4;
        uint32_t  max_tr_depth_intra       : 4;
        uint32_t  tu_split_flag            : 1;
        uint32_t  tu_based_cost_setting    : 3;
        uint32_t  reserved0                : 8;
    } dw6;

    struct {
        uint32_t  concurrent_group_num       : 8;
        uint32_t  enc_tu_decision_mode     : 2;
        uint32_t  reserved0                : 14;
        uint32_t  slice_qp                 : 8;
    } dw7;

    struct {
        uint32_t  lambda_rd;
    } dw8;

    struct {
        uint32_t  lambda_md                 : 16;
        uint32_t  reserved0                : 16;
    } dw9;

    struct {
        uint32_t  intra_tusad_thr;
    } dw10;

    struct {
        uint32_t  slice_type               : 2;
        uint32_t  qp_type                  : 2;
        uint32_t  check_pcm_mode_flag      : 1;
        uint32_t  enable_intra4x4_pu       : 1;
        uint32_t  enc_qt_decision_mode     : 1;
        uint32_t  reserved0                : 25;
    } dw11;

    struct {
        uint32_t  pcm_8x8_sad_threshold    : 16;
        uint32_t  reserved0                : 16;
    } dw12;

    struct {
        uint32_t reserved;
    } dw13;

    struct {
        uint32_t reserved;
    } dw14;

    struct {
        uint32_t reserved;
    } dw15;

    struct {
        uint32_t  bti_vme_intra_pred_surface;
    } dw16;

    struct {
        uint32_t  bti_curr_picture_y;
    } dw17;

    struct {
        uint32_t  bti_enc_curecord_surface;
    } dw18;

    struct {
        uint32_t  bti_pak_obj_cmd_surface;
    } dw19;

    struct {
        uint32_t  bti_cu_packet_for_pak_surface;
    } dw20;

    struct {
        uint32_t  bti_internal_scratch_surface;
    } dw21;

    struct {
        uint32_t  bti_cu_based_qp_surface;
    } dw22;

    struct {
        uint32_t  bti_const_data_lut_surface;
    } dw23;

    struct {
        uint32_t  bti_lcu_level_data_input_surface;
    } dw24;

    struct {
        uint32_t  bti_concurrent_tg_data_surface;
    } dw25;

    struct {
        uint32_t  bti_brc_combined_enc_param_surface;
    } dw26;

    struct {
        uint32_t  bti_cu_split_surface;
    } dw27;

    struct {
        uint32_t  bti_debug_surface;
    } dw28;
} gen10_hevc_mbenc_intra_curbe_data;

typedef struct _gen10_hevc_mbenc_inter_curbe_data {
    struct {
        uint32_t  frame_width_in_pixel      : 16;
        uint32_t  frame_height_in_pixel      : 16;
    } dw0;

    struct {
        uint32_t  log2_max_cu_size          : 4;
        uint32_t  log2_min_cu_size          : 4;
        uint32_t  log2_max_tu_size          : 4;
        uint32_t  log2_min_tu_size          : 4;
        uint32_t  max_tr_depth_inter        : 4;
        uint32_t  max_tr_depth_intra        : 4;
        uint32_t  log2_para_merge_level     : 4;
        uint32_t  max_num_ime_search_center : 4;
    } dw1;

    struct {
        uint32_t  transquant_bypass_enable      : 1;
        uint32_t  cu_qpdelta_enable             : 1;
        uint32_t  pcm_enable                    : 1;
        uint32_t  enable_cu64_check             : 1;
        uint32_t  enable_intra4x4_pu            : 1;
        uint32_t  chroma_skip_check             : 1;
        uint32_t  enc_trans_simplify            : 2;
        uint32_t  hme_flag                      : 2;
        uint32_t  hme_coarse_stage              : 2;
        uint32_t  hme_subpel_mode               : 2;
        uint32_t  super_hme_enable              : 1;
        uint32_t  regions_in_slice_splits_enable       : 1;
        uint32_t  enc_tu_dec_mode               : 2;
        uint32_t  enc_tu_dec_for_all_qt         : 1;
        uint32_t  coef_bit_est_mode             : 1;
        uint32_t  enc_skip_dec_mode             : 2;
        uint32_t  enc_qt_dec_mode               : 1;
        uint32_t  lcu32_enc_rd_dec_mode_for_all_qt : 1;
        uint32_t  qp_type                       : 2;
        uint32_t  lcu64_cu64_skip_check_only    : 1;
        uint32_t  sic_dys_run_path_mode         : 2;
        uint32_t  reserved0                     : 3;
    } dw2;

    struct {
        uint32_t  active_num_child_threads_cu64  : 4;
        uint32_t  active_num_child_threads_cu32_0  : 4;
        uint32_t  active_num_child_threads_cu32_1  : 4;
        uint32_t  active_num_child_threads_cu32_2  : 4;
        uint32_t  active_num_child_threads_cu32_3  : 4;
        uint32_t  reserved0                        : 4;
        uint32_t  slice_qp                         : 8;
    } dw3;

    struct {
        uint32_t  skip_mode_enable                : 1;
        uint32_t  adaptive_enable                 : 1;
        uint32_t  reserved0                       : 1;
        uint32_t  hevc_min_cu_ctrl                : 2;
        uint32_t  early_ime_succ_enable           : 1;
        uint32_t  reserved1                       : 1;
        uint32_t  ime_cost_center_sel             : 1;
        uint32_t  ref_pixel_offset                : 8;
        uint32_t  ime_ref_window_size             : 2;
        uint32_t  residual_pred_data_type_ctrl    : 1;
        uint32_t  residual_pred_inter_chroma_ctrl : 1;
        uint32_t  residual_pred16x16_sel_ctrl     : 2;
        uint32_t  reserved2                       : 2;
        uint32_t  early_ime_stop                  : 8;
    } dw4;

    struct {
        uint32_t  subpel_mode                     : 2;
        uint32_t  reserved0                       : 2;
        uint32_t  inter_sad_measure               : 2;
        uint32_t  intra_sad_measure               : 2;
        uint32_t  len_sp                          : 8;
        uint32_t  max_num_su                      : 8;
        uint32_t  intra_pred_mask                 : 3;
        uint32_t  refid_cost_mode                 : 1;
        uint32_t  disable_pintra                  : 1;
        uint32_t  tu_based_cost_setting           : 3;
    } dw5;

    struct {
        uint32_t  reserved0;
    } dw6;

    struct {
        uint32_t  slice_type               : 2;
        uint32_t  temporal_mvp_enable      : 1;
        uint32_t  mvp_collocated_from_l0   : 1;
        uint32_t  same_ref_list            : 1;
        uint32_t  is_low_delay             : 1;
        uint32_t  reserved0                : 2;
        uint32_t  max_num_merge_cand       : 8;
        uint32_t  num_ref_idx_l0           : 8;
        uint32_t  num_ref_idx_l1           : 8;
    } dw7;

    struct {
        uint32_t  fwd_poc_num_l0_mtb_0                 : 8;
        uint32_t  bwd_poc_num_l1_mtb_0                 : 8;
        uint32_t  fwd_poc_num_l0_mtb_1                 : 8;
        uint32_t  bwd_poc_num_l1_mtb_1                 : 8;
    } dw8;

    struct {
        uint32_t  fwd_poc_num_l0_mtb_2                 : 8;
        uint32_t  bwd_poc_num_l1_mtb_2                 : 8;
        uint32_t  fwd_poc_num_l0_mtb_3                 : 8;
        uint32_t  bwd_poc_num_l1_mtb_3                 : 8;
    } dw9;

    struct {
        uint32_t  fwd_poc_num_l0_mtb_4                 : 8;
        uint32_t  bwd_poc_num_l1_mtb_4                 : 8;
        uint32_t  fwd_poc_num_l0_mtb_5                 : 8;
        uint32_t  bwd_poc_num_l1_mtb_5                 : 8;
    } dw10;

    struct {
        uint32_t  fwd_poc_num_l0_mtb_6                 : 8;
        uint32_t  bwd_poc_num_l1_mtb_6                 : 8;
        uint32_t  fwd_poc_num_l0_mtb_7                 : 8;
        uint32_t  bwd_poc_num_l1_mtb_7                 : 8;
    } dw11;

    struct {
        uint32_t  long_term_ref_flags_l0               : 16;
        uint32_t  long_term_ref_flags_l1               : 16;
    } dw12;

    struct {
        uint32_t  ref_frame_hor_size                   : 16;
        uint32_t  ref_frame_ver_size                   : 16;
    } dw13;

    struct {
        uint32_t  kernel_debug;
    } dw14;

    struct {
        uint32_t  concurrent_gop_num                  : 8;
        uint32_t  total_thread_num_per_lcu            : 8;
        uint32_t  regions_in_slice_split_count               : 8;
        uint32_t  reserved0                           : 8;
    } dw15;

    struct {
        uint32_t  bti_curr_picture_y;
    } dw16;

    struct {
        uint32_t  bti_enc_curecord_surface;
    } dw17;

    union {
        uint32_t  bti_lcu32_pak_objcmd_surface;
        uint32_t  bti_lcu64_enc_curecord2_surface;
    } dw18;

    union {
        uint32_t  bti_lcu32_pak_curecord_surface;
        uint32_t  bti_lcu64_pak_objcmd_surface;
    } dw19;

    union {
        uint32_t  bti_lcu32_vme_intra_inter_pred_surface;
        uint32_t  bti_lcu64_pak_curecord_surface;
    } dw20;

    union {
        uint32_t  bti_lcu32_cu16_qpdata_input_surface;
        uint32_t  bti_lcu64_vme_intra_inter_pred_surface;
    } dw21;

    union {
        uint32_t  bti_lcu32_enc_const_table_surface;
        uint32_t  bti_lcu64_cu16_qpdata_input_surface;
    } dw22;

    union {
        uint32_t  bti_lcu32_colocated_mvdata_surface;
        uint32_t  bti_lcu64_cu32_enc_const_table_surface;
    } dw23;

    union {
        uint32_t  bti_lcu32_hme_pred_data_surface;
        uint32_t  bti_lcu64_colocated_mvdata_surface;
    } dw24;

    union {
        uint32_t  bti_lcu32_lculevel_data_input_surface;
        uint32_t  bti_lcu64_hme_pred_surface;
    } dw25;

    union {
        uint32_t  bti_lcu32_enc_scratch_surface;
        uint32_t  bti_lcu64_lculevel_data_input_surface;
    } dw26;

    union {
        uint32_t  bti_lcu32_concurrent_tg_data_surface;
        uint32_t  bti_lcu64_cu32_enc_scratch_surface;
    } dw27;

    union {
        uint32_t  bti_lcu32_brc_combined_enc_param_surface;
        uint32_t  bti_lcu64_64x64_dist_surface;
    } dw28;

    union {
        uint32_t  bti_lcu32_jbq_scratch_surface;
        uint32_t  bti_lcu64_concurrent_tg_data_surface;
    } dw29;

    union {
        uint32_t  bti_lcu32_cusplit_data_surface;
        uint32_t  bti_lcu64_brc_combined_enc_param_surface;
    } dw30;

    union {
        uint32_t  bti_lcu32_residual_scratch_surface;
        uint32_t  bti_lcu64_cu32_jbq1d_buf_surface;
    } dw31;

    union {
        uint32_t  bti_lcu32_debug_surface;
        uint32_t  bti_lcu64_cu32_jbq2d_buf_surface;
    } dw32;

    union {
        uint32_t  reserved0;
        uint32_t  bti_lcu64_cu32_residual_scratch_surface;
    } dw33;

    union {
        uint32_t  reserved0;
        uint32_t  bti_lcu64_cusplit_surface;
    } dw34;

    union {
        uint32_t  reserved0;
        uint32_t  bti_lcu64_curr_picture_y_2xds;
    } dw35;

    union {
        uint32_t  reserved0;
        uint32_t  bti_lcu64_intermediate_curecord_surface;
    } dw36;

    union {
        uint32_t  reserved0;
        uint32_t  bti_lcu64_const_data_lut_surface;
    } dw37;

    union {
        uint32_t  reserved0;
        uint32_t  bti_lcu64_lcu_storage_surface;
    } dw38;

    union {
        uint32_t  reserved0;
        uint32_t  bti_lcu64_vme_inter_pred_2xds_surface;
    } dw39;

    union {
        uint32_t  reserved0;
        uint32_t  bti_lcu64_cu64_jbq1d_surface;
    } dw40;

    union {
        uint32_t  reserved0;
        uint32_t  bti_lcu64_cu64_jbq2d_surface;
    } dw41;

    union {
        uint32_t  reserved0;
        uint32_t  bti_lcu64_cu64_residual_scratch_surface;
    } dw42;

    union {
        uint32_t  reserved0;
        uint32_t  bti_lcu64_debug_surface;
    } dw43;
} gen10_hevc_mbenc_inter_curbe_data;

typedef struct _gen10_intel_kernel_header_ {
    uint32_t reserved                        : 6;
    uint32_t kernel_start_pointer            : 26;
} gen10_intel_kernel_header;

typedef struct _gen10_hevc_kernel_header_ {
    int kernel_count;
    gen10_intel_kernel_header hevc_intra;
    gen10_intel_kernel_header hevc_enc;
    gen10_intel_kernel_header hevc_ds_convert;
    gen10_intel_kernel_header hevc_hme;
    gen10_intel_kernel_header hevc_enc_lcu64;
    gen10_intel_kernel_header hevc_brc_init;
    gen10_intel_kernel_header hevc_brc_lcuqp;
    gen10_intel_kernel_header hevc_brc_reset;
    gen10_intel_kernel_header hevc_brc_update;
    gen10_intel_kernel_header hevc_last;
} gen10_hevc_kernel_header;

typedef enum _GEN10_HEVC_ENC_OPERATION_ {
    GEN10_HEVC_ENC_SCALING_CONVERSION = 0,
    GEN10_HEVC_ENC_ME,
    GEN10_HEVC_ENC_BRC,
    GEN10_HEVC_ENC_MBENC,
} GEN10_HEVC_ENC_OPERATION;

#endif
