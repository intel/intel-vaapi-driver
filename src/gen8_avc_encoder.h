/*
 * Copyright @ 2017 Intel Corporation
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
 * SOFTWAR OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Tiantian Wang <tiantian.wang@intel.com>
 *
 */

#ifndef GEN8_AVC_ENCODER_H
#define GEN8_AVC_ENCODER_H

#include "i965_encoder_common.h"

typedef struct _gen8_avc_encoder_kernel_header {
    int nKernelCount;

    // Quality mode for Frame/Field
    kernel_header mbenc_quality_I;
    kernel_header mbenc_quality_P;
    kernel_header mbenc_quality_B;
    // Normal mode for Frame/Field
    kernel_header mbenc_normal_I;
    kernel_header mbenc_normal_P;
    kernel_header mbenc_normal_B;
    // Performance modes for Frame/Field
    kernel_header mbenc_performance_I;
    kernel_header mbenc_performance_P;
    kernel_header mbenc_performance_B;
    // WiDi modes for Frame/Field
    kernel_header mbenc_widi_I;
    kernel_header mbenc_widi_P;
    kernel_header mbenc_widi_B;

    // HME
    kernel_header me_p;
    kernel_header me_b;

    // DownScaling
    kernel_header ply_dscale_ply;
    kernel_header ply_dscale_2f_ply_2f;
    // BRC Init frame
    kernel_header frame_brc_init;

    // FrameBRC Update
    kernel_header frame_brc_update;

    // BRC Reset frame
    kernel_header frame_brc_reset;

    // BRC I Frame Distortion
    kernel_header frame_brc_i_dist;

    //BRC Block Copy
    kernel_header brc_block_copy;

    // 2x DownScaling
    kernel_header ply_2xdscale_ply;
    kernel_header ply_2xdscale_2f_ply_2f;

    // Static frame detection Kernel
    kernel_header static_detection;
} gen8_avc_encoder_kernel_header;

struct gen8_mfx_avc_img_state {
    union {
        struct {
            uint32_t dword_length: 16;
            uint32_t command_sub_opcode_b: 5;
            uint32_t command_sub_opcode_a: 3;
            uint32_t command_opcode: 3;
            uint32_t command_pipeline: 2;
            uint32_t command_type: 3;
        };

        uint32_t value;
    } dw0;

    struct {
        uint32_t frame_size_in_mbs: 16; //minus1
        uint32_t pad0: 16;
    } dw1;

    struct {
        uint32_t frame_width_in_mbs_minus1: 8; //minus1
        uint32_t pad0: 8;
        uint32_t frame_height_in_mbs_minus1: 8; //minus1
        uint32_t pad1: 8;
    } dw2;

    struct {
        uint32_t pad0: 8;
        uint32_t image_structure: 2;
        uint32_t weighted_bipred_idc: 2;
        uint32_t weighted_pred_flag: 1;
        uint32_t inter_mb_conf_flag: 1;
        uint32_t intra_mb_conf_flag: 1;
        uint32_t pad1: 1;
        uint32_t chroma_qp_offset: 5;
        uint32_t pad3: 3;
        uint32_t second_chroma_qp_offset: 5;
        uint32_t pad4: 3;
    } dw3;

    struct {
        uint32_t field_picture_flag: 1;
        uint32_t mbaff_mode_active: 1;
        uint32_t frame_mb_only_flag: 1;
        uint32_t transform_8x8_idct_mode_flag: 1;
        uint32_t direct_8x8_interface_flag: 1;
        uint32_t constrained_intra_prediction_flag: 1;
        uint32_t current_img_dispoable_flag: 1;
        uint32_t entropy_coding_flag: 1;
        uint32_t mb_mv_format_flag: 1;
        uint32_t pad0: 1;
        uint32_t chroma_format_idc: 2;
        uint32_t mv_unpacked_flag: 1;
        uint32_t insert_test_flag: 1;
        uint32_t load_slice_pointer_flag: 1;
        uint32_t macroblock_stat_enable: 1;
        uint32_t minimum_frame_size: 16;
    } dw4;

    struct {
        uint32_t intra_mb_max_bit_flag: 1;
        uint32_t inter_mb_max_bit_flag: 1;
        uint32_t frame_size_over_flag: 1;
        uint32_t frame_size_under_flag: 1;
        uint32_t pad0: 3;
        uint32_t intra_mb_ipcm_flag: 1;
        uint32_t pad1: 1;
        uint32_t mb_rate_ctrl_flag: 1;
        uint32_t min_frame_size_units: 2;
        uint32_t inter_mb_zero_cbp_flag: 1; //?change
        uint32_t pad2: 3;
        uint32_t non_first_pass_flag: 1;
        uint32_t pad3: 10;
        uint32_t aq_chroma_disable: 1;
        uint32_t aq_rounding: 3;
        uint32_t aq_enable: 1;
    } dw5;

    struct {
        uint32_t intra_mb_max_size: 12;
        uint32_t pad0: 4;
        uint32_t inter_mb_max_size: 12;
        uint32_t pad1: 4;
    } dw6;

    struct {
        uint32_t pad0: 32;
    } dw7;

    struct {
        uint32_t slice_delta_qp_max0: 8;
        uint32_t slice_delta_qp_max1: 8;
        uint32_t slice_delta_qp_max2: 8;
        uint32_t slice_delta_qp_max3: 8;
    } dw8;

    struct {
        uint32_t slice_delta_qp_min0: 8;
        uint32_t slice_delta_qp_min1: 8;
        uint32_t slice_delta_qp_min2: 8;
        uint32_t slice_delta_qp_min3: 8;
    } dw9;

    struct {
        uint32_t frame_bitrate_min: 14;
        uint32_t frame_bitrate_min_unit_mode: 1;
        uint32_t frame_bitrate_min_unit: 1;
        uint32_t frame_bitrate_max: 14;
        uint32_t frame_bitrate_max_unit_mode: 1;
        uint32_t frame_bitrate_max_unit: 1;
    } dw10;

    struct {
        uint32_t frame_bitrate_min_delta: 15;
        uint32_t pad0: 1;
        uint32_t frame_bitrate_max_delta: 15;
        uint32_t pad1: 1;
    } dw11;

    struct {
        uint32_t pad0: 32;
    } dw12;

    struct {
        uint32_t pic_qp_init_minus26: 8;
        uint32_t pic_num_ref_idx_l0_active_minus1: 6;
        uint32_t pad0: 2;
        uint32_t pic_num_ref_idx_l1_active_minus1: 6;
        uint32_t pad1: 2;
        uint32_t num_ref_frames: 5;
        uint32_t is_curr_pic_has_mmco5: 1;
        uint32_t pad2: 2;
    } dw13;

    struct {
        uint32_t pic_order_present_flag: 1;
        uint32_t delta_pic_order_always_zero_flag: 1;
        uint32_t pic_order_cnt_type: 2;
        uint32_t pad0: 4;
        uint32_t slice_group_map_type: 3;
        uint32_t redundant_pic_cnt_present_flag: 1;
        uint32_t num_slice_groups_minus1: 3;
        uint32_t deblock_filter_ctrl_present_flag: 1;
        uint32_t log2_max_frame_num_minus4: 8;
        uint32_t log2_max_pic_order_cnt_lsb_minus4: 8;
    } dw14;

    struct {
        uint32_t slice_group_change_rate: 16;
        uint32_t curr_pic_frame_num: 16;
    } dw15;

    struct {
        uint32_t current_frame_view_id: 10;
        uint32_t pad0: 2;
        uint32_t max_view_idx_l0: 4;
        uint32_t pad1: 2;
        uint32_t max_view_idx_l1: 4;
        uint32_t pad2: 9;
        uint32_t inter_view_order_disable: 1;
    } dw16;
};



typedef struct _gen8_avc_mbenc_curbe_data {
    struct {
        uint32_t skip_mode_enable: 1;
        uint32_t adaptive_enable: 1;
        uint32_t bi_mix_dis: 1;
        uint32_t reserved0: 2;
        uint32_t early_ime_success_enable: 1;
        uint32_t reserved1: 1;
        uint32_t t8x8_flag_for_inter_enable: 1;
        uint32_t reserved2: 16;
        uint32_t early_ime_stop: 8;
    } dw0;

    struct {
        uint32_t max_num_mvs: 6;
        uint32_t reserved0: 10;
        uint32_t bi_weight: 6;
        uint32_t reserved1: 6;
        uint32_t uni_mix_disable: 1;
        uint32_t reserved2: 3;
    } dw1;

    struct {
        uint32_t max_len_sp: 8;
        uint32_t max_num_su: 8;
        uint32_t pitch_width: 16;
    } dw2;

    struct {
        uint32_t src_size: 2;
        uint32_t reserved0: 2;
        uint32_t mb_type_remap: 2;
        uint32_t src_access: 1;
        uint32_t ref_access: 1;
        uint32_t search_ctrl: 3;
        uint32_t dual_search_path_option: 1;
        uint32_t sub_pel_mode: 2;
        uint32_t skip_type: 1;
        uint32_t disable_field_cache_allocation: 1;
        uint32_t inter_chroma_mode: 1;
        uint32_t ftq_enable: 1;
        uint32_t bme_disable_fbr: 1;
        uint32_t block_based_skip_enable: 1;
        uint32_t inter_sad: 2;
        uint32_t intra_sad: 2;
        uint32_t sub_mb_part_mask: 7;
        uint32_t reserved1: 1;
    } dw3;

    struct {
        uint32_t picture_height_minus1: 16;
        uint32_t mv_restriction_in_slice_enable: 1;
        uint32_t delta_mv_enable: 1;
        uint32_t true_distortion_enable: 1;
        uint32_t enable_wavefront_optimization: 1;
        uint32_t reserved0: 1;
        uint32_t enable_intra_cost_scaling_for_static_frame: 1;
        uint32_t enable_intra_refresh: 1;
        uint32_t enable_widi_wa_surf: 1;
        uint32_t enable_widi_dirty_rect: 1;
        uint32_t enable_cur_fld_idr: 1;
        uint32_t contrained_intra_pred_flag: 1;
        uint32_t field_parity_flag: 1;
        uint32_t hme_enable: 1;
        uint32_t picture_type: 2;
        uint32_t use_actual_ref_qp_value: 1;
    } dw4;

    struct {
        uint32_t slice_mb_height: 16;
        uint32_t ref_width: 8;
        uint32_t ref_height: 8;
    } dw5;

    struct {
        uint32_t batch_buffer_end;
    } dw6;

    struct {
        uint32_t intra_part_mask: 5;
        uint32_t non_skip_zmv_added: 1;
        uint32_t non_skip_mode_added: 1;
        uint32_t luma_intra_src_corner_swap: 1;
        uint32_t reserved0: 8;
        uint32_t mv_cost_scale_factor: 2;
        uint32_t bilinear_enable: 1;
        uint32_t src_field_polarity: 1;
        uint32_t weightedsad_harr: 1;
        uint32_t ac_only_haar: 1;
        uint32_t ref_id_cost_mode: 1;
        uint32_t reserved1: 1;
        uint32_t skip_center_mask: 8;
    } dw7;

    struct {
        uint32_t mode_0_cost: 8;
        uint32_t mode_1_cost: 8;
        uint32_t mode_2_cost: 8;
        uint32_t mode_3_cost: 8;
    } dw8;

    struct {
        uint32_t mode_4_cost: 8;
        uint32_t mode_5_cost: 8;
        uint32_t mode_6_cost: 8;
        uint32_t mode_7_cost: 8;
    } dw9;

    struct {
        uint32_t mode_8_cost: 8;
        uint32_t mode_9_cost: 8;
        uint32_t ref_id_cost: 8;
        uint32_t chroma_intra_mode_cost: 8;
    } dw10;

    union {
        struct {
            uint32_t mv_0_cost: 8;
            uint32_t mv_1_cost: 8;
            uint32_t mv_2_cost: 8;
            uint32_t mv_3_cost: 8;
        };
        uint32_t value;
    } dw11;

    struct {
        uint32_t mv_4_cost: 8;
        uint32_t mv_5_cost: 8;
        uint32_t mv_6_cost: 8;
        uint32_t mv_7_cost: 8;
    } dw12;

    struct {
        uint32_t qp_prime_y: 8;
        uint32_t qp_prime_cb: 8;
        uint32_t qp_prime_cr: 8;
        uint32_t target_size_in_word: 8;
    } dw13;

    struct {
        uint32_t sic_fwd_transcoeff_threshold_0: 16;
        uint32_t sic_fwd_transcoeff_threshold_1: 8;
        uint32_t sic_fwd_transcoeff_threshold_2: 8;
    } dw14;

    struct {
        uint32_t sic_fwd_transcoeff_threshold_3: 8;
        uint32_t sic_fwd_transcoeff_threshold_4: 8;
        uint32_t sic_fwd_transcoeff_threshold_5: 8;
        uint32_t sic_fwd_transcoeff_threshold_6: 8;
    } dw15;

    struct {
        struct generic_search_path_delta sp_delta_0;
        struct generic_search_path_delta sp_delta_1;
        struct generic_search_path_delta sp_delta_2;
        struct generic_search_path_delta sp_delta_3;
    } dw16;

    struct {
        struct generic_search_path_delta sp_delta_4;
        struct generic_search_path_delta sp_delta_5;
        struct generic_search_path_delta sp_delta_6;
        struct generic_search_path_delta sp_delta_7;
    } dw17;

    struct {
        struct generic_search_path_delta sp_delta_8;
        struct generic_search_path_delta sp_delta_9;
        struct generic_search_path_delta sp_delta_10;
        struct generic_search_path_delta sp_delta_11;
    } dw18;

    struct {
        struct generic_search_path_delta sp_delta_12;
        struct generic_search_path_delta sp_delta_13;
        struct generic_search_path_delta sp_delta_14;
        struct generic_search_path_delta sp_delta_15;
    } dw19;

    struct {
        struct generic_search_path_delta sp_delta_16;
        struct generic_search_path_delta sp_delta_17;
        struct generic_search_path_delta sp_delta_18;
        struct generic_search_path_delta sp_delta_19;
    } dw20;

    struct {
        struct generic_search_path_delta sp_delta_20;
        struct generic_search_path_delta sp_delta_21;
        struct generic_search_path_delta sp_delta_22;
        struct generic_search_path_delta sp_delta_23;
    } dw21;

    struct {
        struct generic_search_path_delta sp_delta_24;
        struct generic_search_path_delta sp_delta_25;
        struct generic_search_path_delta sp_delta_26;
        struct generic_search_path_delta sp_delta_27;
    } dw22;

    struct {
        struct generic_search_path_delta sp_delta_28;
        struct generic_search_path_delta sp_delta_29;
        struct generic_search_path_delta sp_delta_30;
        struct generic_search_path_delta sp_delta_31;
    } dw23;

    struct {
        struct generic_search_path_delta sp_delta_32;
        struct generic_search_path_delta sp_delta_33;
        struct generic_search_path_delta sp_delta_34;
        struct generic_search_path_delta sp_delta_35;
    } dw24;

    struct {
        struct generic_search_path_delta sp_delta_36;
        struct generic_search_path_delta sp_delta_37;
        struct generic_search_path_delta sp_delta_38;
        struct generic_search_path_delta sp_delta_39;
    } dw25;

    struct {
        struct generic_search_path_delta sp_delta_40;
        struct generic_search_path_delta sp_delta_41;
        struct generic_search_path_delta sp_delta_42;
        struct generic_search_path_delta sp_delta_43;
    } dw26;

    struct {
        struct generic_search_path_delta sp_delta_44;
        struct generic_search_path_delta sp_delta_45;
        struct generic_search_path_delta sp_delta_46;
        struct generic_search_path_delta sp_delta_47;
    } dw27;

    struct {
        struct generic_search_path_delta sp_delta_48;
        struct generic_search_path_delta sp_delta_49;
        struct generic_search_path_delta sp_delta_50;
        struct generic_search_path_delta sp_delta_51;
    } dw28;

    struct {
        struct generic_search_path_delta sp_delta_52;
        struct generic_search_path_delta sp_delta_53;
        struct generic_search_path_delta sp_delta_54;
        struct generic_search_path_delta sp_delta_55;
    } dw29;

    struct {
        uint32_t intra_4x4_mode_mask: 9;
        uint32_t reserved0: 7;
        uint32_t intra_8x8_mode_mask: 9;
        uint32_t reserved1: 7;
    } dw30;

    struct {
        uint32_t intra_16x16_mode_mask: 4;
        uint32_t intra_chroma_mode_mask: 4;
        uint32_t intra_compute_type: 2;
        uint32_t reserved0: 22;
    } dw31;

    struct {
        uint32_t skip_val: 16;
        uint32_t mult_pred_l0_disable: 8;
        uint32_t mult_pred_l1_disable: 8;
    } dw32;

    struct {
        uint32_t intra_16x16_nondc_penalty: 8;
        uint32_t intra_8x8_nondc_penalty: 8;
        uint32_t intra_4x4_nondc_penalty: 8;
        uint32_t reserved0: 8;
    } dw33;

    struct {
        uint32_t list0_ref_id0_field_parity: 1;
        uint32_t list0_ref_id1_field_parity: 1;
        uint32_t list0_ref_id2_field_parity: 1;
        uint32_t list0_ref_id3_field_parity: 1;
        uint32_t list0_ref_id4_field_parity: 1;
        uint32_t list0_ref_id5_field_parity: 1;
        uint32_t list0_ref_id6_field_parity: 1;
        uint32_t list0_ref_id7_field_parity: 1;
        uint32_t list1_ref_id0_frm_field_parity: 1;
        uint32_t list1_ref_id1_frm_field_parity: 1;
        uint32_t widi_intra_refresh_en: 2;
        uint32_t arbitray_num_mbs_per_slice: 1;
        uint32_t force_non_skip_check: 1;
        uint32_t disable_enc_skip_check: 1;
        uint32_t enable_direct_bias_adjustment: 1;
        uint32_t enable_global_motion_bias_adjustment: 1;
        uint32_t b_force_to_skip: 1;
        uint32_t reserved0: 6;
        uint32_t list1_ref_id0_field_parity: 1;
        uint32_t list1_ref_id1_field_parity: 1;
        uint32_t mad_enable_falg: 1;
        uint32_t roi_enable_flag: 1;
        uint32_t enable_mb_flatness_check_optimization: 1;
        uint32_t b_direct_mode: 1;
        uint32_t mb_brc_enable: 1;
        uint32_t b_original_bff: 1;
    } dw34;

    struct {
        uint32_t panic_mode_mb_threshold: 16;
        uint32_t small_mb_size_in_word: 8;
        uint32_t large_mb_size_in_word: 8;
    } dw35;

    struct {
        uint32_t num_ref_idx_l0_minus_one: 8;
        uint32_t hme_combined_extra_sus: 8;
        uint32_t num_ref_idx_l1_minus_one: 8;
        uint32_t enable_cabac_work_around: 1;
        uint32_t reserved0: 3;
        uint32_t is_fwd_frame_short_term_ref: 1;
        uint32_t check_all_fractional_enable: 1;
        uint32_t hme_combine_overlap: 2;
    } dw36;

    struct {
        uint32_t skip_mode_enable: 1;
        uint32_t adaptive_enable: 1;
        uint32_t bi_mix_dis: 1;
        uint32_t reserved0: 2;
        uint32_t early_ime_success_enable: 1;
        uint32_t reserved1: 1;
        uint32_t t8x8_flag_for_inter_enable: 1;
        uint32_t reserved2: 16;
        uint32_t early_ime_stop: 8;
    } dw37;

    /* reserved */
    struct {
        uint32_t max_len_sp: 8;
        uint32_t max_num_su: 8;
        uint32_t ref_threshold: 16;
    } dw38;

    struct {
        uint32_t reserved0: 8;
        uint32_t hme_ref_windows_comb_threshold: 8;
        uint32_t ref_width: 8;
        uint32_t ref_height: 8;
    } dw39;

    struct {
        uint32_t dist_scale_factor_ref_id0_list0: 16;
        uint32_t dist_scale_factor_ref_id1_list0: 16;
    } dw40;

    struct {
        uint32_t dist_scale_factor_ref_id2_list0: 16;
        uint32_t dist_scale_factor_ref_id3_list0: 16;
    } dw41;

    struct {
        uint32_t dist_scale_factor_ref_id4_list0: 16;
        uint32_t dist_scale_factor_ref_id5_list0: 16;
    } dw42;

    struct {
        uint32_t dist_scale_factor_ref_id6_list0: 16;
        uint32_t dist_scale_factor_ref_id7_list0: 16;
    } dw43;

    struct {
        uint32_t actual_qp_value_for_ref_id0_list0: 8;
        uint32_t actual_qp_value_for_ref_id1_list0: 8;
        uint32_t actual_qp_value_for_ref_id2_list0: 8;
        uint32_t actual_qp_value_for_ref_id3_list0: 8;
    } dw44;

    struct {
        uint32_t actual_qp_value_for_ref_id4_list0: 8;
        uint32_t actual_qp_value_for_ref_id5_list0: 8;
        uint32_t actual_qp_value_for_ref_id6_list0: 8;
        uint32_t actual_qp_value_for_ref_id7_list0: 8;
    } dw45;

    struct {
        uint32_t actual_qp_value_for_ref_id0_list1: 8;
        uint32_t actual_qp_value_for_ref_id1_list1: 8;
        uint32_t ref_cost: 16;
    } dw46;

    struct {
        uint32_t mb_qp_read_factor: 8;
        uint32_t intra_cost_sf: 8;
        uint32_t max_vmv_r: 16;
    } dw47;

    struct {
        uint32_t widi_intra_refresh_mbx: 16;
        uint32_t widi_intra_refresh_unit_in_mb_minus1: 8;
        uint32_t widi_intra_refresh_qp_delta: 8;
    } dw48;

    struct {
        uint32_t roi_1_x_left: 16;
        uint32_t roi_1_y_top: 16;
    } dw49;

    struct {
        uint32_t roi_1_x_right: 16;
        uint32_t roi_1_y_bottom: 16;
    } dw50;

    struct {
        uint32_t roi_2_x_left: 16;
        uint32_t roi_2_y_top: 16;
    } dw51;

    struct {
        uint32_t roi_2_x_right: 16;
        uint32_t roi_2_y_bottom: 16;
    } dw52;

    struct {
        uint32_t roi_3_x_left: 16;
        uint32_t roi_3_y_top: 16;
    } dw53;

    struct {
        uint32_t roi_3_x_right: 16;
        uint32_t roi_3_y_bottom: 16;
    } dw54;

    struct {
        uint32_t roi_4_x_left: 16;
        uint32_t roi_4_y_top: 16;
    } dw55;

    struct {
        uint32_t roi_4_x_right: 16;
        uint32_t roi_4_y_bottom: 16;
    } dw56;

    struct {
        uint32_t roi_1_dqp_prime_y: 8;
        uint32_t roi_2_dqp_prime_y: 8;
        uint32_t roi_3_dqp_prime_y: 8;
        uint32_t roi_4_dqp_prime_y: 8;
    } dw57;

    struct {
        uint32_t hme_mv_cost_scaling_factor: 8;
        int32_t reserved0: 8;
        int32_t widi_intra_refresh_mby: 16;
    } dw58;

    struct {
        uint32_t reserved;
    } dw59;

    struct {
        uint32_t cabac_wa_zone0_threshold: 16;
        uint32_t cabac_wa_zone1_threshold: 16;
    } dw60;

    struct {
        uint32_t cabac_wa_zone2_threshold: 16;
        uint32_t cabac_wa_zone3_threshold: 16;
    } dw61;

    struct {
        uint32_t cabac_wa_zone0_intra_min_qp: 8;
        uint32_t cabac_wa_zone1_intra_min_qp: 8;
        uint32_t cabac_wa_zone2_intra_min_qp: 8;
        uint32_t cabac_wa_zone3_intra_min_qp: 8;
    } dw62;

    struct {
        uint32_t reserved;
    } dw63;

    struct {
        uint32_t reserved;
    } dw64;

    struct {
        uint32_t mb_data_surf_index;
    } dw65;

    struct {
        uint32_t mv_data_surf_index;
    } dw66;

    struct {
        uint32_t i_dist_surf_index;
    } dw67;

    struct {
        uint32_t src_y_surf_index;
    } dw68;

    struct {
        uint32_t mb_specific_data_surf_index;
    } dw69;

    struct {
        uint32_t aux_vme_out_surf_index;
    } dw70;

    struct {
        uint32_t curr_ref_pic_sel_surf_index;
    } dw71;

    struct {
        uint32_t hme_mv_pred_fwd_bwd_surf_index;
    } dw72;

    struct {
        uint32_t hme_dist_surf_index;
    } dw73;

    struct {
        uint32_t slice_map_surf_index;
    } dw74;

    struct {
        uint32_t fwd_frm_mb_data_surf_index;
    } dw75;

    struct {
        uint32_t fwd_frm_mv_surf_index;
    } dw76;

    struct {
        uint32_t mb_qp_buffer;
    } dw77;

    struct {
        uint32_t mb_brc_lut;
    } dw78;

    struct {
        uint32_t vme_inter_prediction_surf_index;
    } dw79;

    struct {
        uint32_t vme_inter_prediction_mr_surf_index;
    } dw80;

    struct {
        uint32_t flatness_chk_surf_index;
    } dw81;

    struct {
        uint32_t mad_surf_index;
    } dw82;

    struct {
        uint32_t force_non_skip_mb_map_surface;
    } dw83;

    struct {
        uint32_t widi_wa_surf_index;
    } dw84;

    struct {
        uint32_t brc_curbe_surf_index;
    } dw85;

    struct {
        uint32_t static_detection_cost_table_index;
    } dw86;

    struct {
        uint32_t reserved0;
    } dw87;

} gen8_avc_mbenc_curbe_data;

typedef struct _gen8_avc_frame_brc_update_curbe_data {
    struct {
        uint32_t target_size;
    } dw0;

    struct {
        uint32_t frame_number;
    } dw1;

    struct {
        uint32_t size_of_pic_headers;
    } dw2;

    struct {
        uint32_t start_gadj_frame0: 16;
        uint32_t start_gadj_frame1: 16;
    } dw3;

    struct {
        uint32_t start_gadj_frame2: 16;
        uint32_t start_gadj_frame3: 16;
    } dw4;

    struct {
        uint32_t target_size_flag: 8;
        uint32_t brc_flag: 8;
        uint32_t max_num_paks: 8;
        uint32_t cur_frame_type: 8;
    } dw5;

    struct {
        uint32_t num_skip_frames: 8;
        uint32_t minimum_qp: 8;
        uint32_t maximum_qp: 8;
        uint32_t widi_intra_refresh_mode: 8;
    } dw6;

    struct {
        uint32_t size_skip_frames;
    } dw7;

    struct {
        uint32_t start_global_adjust_mult_0: 8;
        uint32_t start_global_adjust_mult_1: 8;
        uint32_t start_global_adjust_mult_2: 8;
        uint32_t start_global_adjust_mult_3: 8;
    } dw8;

    struct {
        uint32_t start_global_adjust_mult_4: 8;
        uint32_t start_global_adjust_div_0: 8;
        uint32_t start_global_adjust_div_1: 8;
        uint32_t start_global_adjust_div_2: 8;
    } dw9;

    struct {
        uint32_t start_global_adjust_div_3: 8;
        uint32_t start_global_adjust_div_4: 8;
        uint32_t qp_threshold_0: 8;
        uint32_t qp_threshold_1: 8;
    } dw10;

    struct {
        uint32_t qp_threshold_2: 8;
        uint32_t qp_threshold_3: 8;
        uint32_t g_rate_ratio_threshold_0: 8;
        uint32_t g_rate_ratio_threshold_1: 8;
    } dw11;

    struct {
        uint32_t g_rate_ratio_threshold_2: 8;
        uint32_t g_rate_ratio_threshold_3: 8;
        uint32_t g_rate_ratio_threshold_4: 8;
        uint32_t g_rate_ratio_threshold_5: 8;
    } dw12;

    struct {
        uint32_t g_rate_ratio_threshold_qp_0: 8;
        uint32_t g_rate_ratio_threshold_qp_1: 8;
        uint32_t g_rate_ratio_threshold_qp_2: 8;
        uint32_t g_rate_ratio_threshold_qp_3: 8;
    } dw13;

    struct {
        uint32_t g_rate_ratio_threshold_qp_4: 8;
        uint32_t g_rate_ratio_threshold_qp_5: 8;
        uint32_t g_rate_ratio_threshold_qp_6: 8;
        uint32_t qp_index_of_cur_pic: 8;
    } dw14;

    struct {
        uint32_t widi_qp_intra_resresh: 8;
        uint32_t reserved0: 8;
    } dw15;

    struct {
        uint32_t widi_intra_refresh_y_pos: 16;
        uint32_t widi_intra_refresh_x_pos: 16;
    } dw16;

    struct {
        uint32_t widi_intra_refresh_height: 16;
        uint32_t widi_intra_refresh_width: 16;
    } dw17;
} gen8_avc_frame_brc_update_curbe_data;

typedef struct _gen8_avc_scaling4x_curbe_data {
    struct {
        uint32_t   input_picture_width  : 16;
        uint32_t   input_picture_height : 16;
    } dw0;

    struct {
        uint32_t   input_y_bti;
    } dw1;

    struct {
        uint32_t   output_y_bti;
    } dw2;

    struct {
        uint32_t   input_y_bti_bottom_field;
    } dw3;

    struct {
        uint32_t   output_y_bti_bottom_field;
    } dw4;

    struct {
        uint32_t flatness_threshold;
    } dw5;

    struct {
        uint32_t enable_mb_flatness_check: 1;
        uint32_t enable_mb_variance_output: 1;
        uint32_t enable_mb_pixel_average_output: 1;
        uint32_t enable_block8x8_statistics_output: 1;
        uint32_t reserved0: 28;
    } dw6;

    struct {
        uint32_t reserved;
    } dw7;

    struct {
        uint32_t flatness_output_bti_top_field;
    } dw8;

    struct {
        uint32_t flatness_output_bti_bottom_field;
    } dw9;

    struct {
        uint32_t mbv_proc_states_bti_top_field;
    } dw10;

    struct {
        uint32_t mbv_proc_states_bti_bottom_field;
    } dw11;
} gen8_avc_scaling4x_curbe_data;

typedef enum _gen8_avc_binding_table_offset_mbenc {
    GEN8_AVC_MBENC_MFC_AVC_PAK_OBJ_CM                    =  0,
    GEN8_AVC_MBENC_IND_MV_DATA_CM                        =  1,
    GEN8_AVC_MBENC_BRC_DISTORTION_CM                     =  2,    // For BRC distortion for I
    GEN8_AVC_MBENC_CURR_Y_CM                             =  3,
    GEN8_AVC_MBENC_CURR_UV_CM                            =  4,
    GEN8_AVC_MBENC_MB_SPECIFIC_DATA_CM                   =  5,
    GEN8_AVC_MBENC_AUX_VME_OUT_CM                        =  6,
    GEN8_AVC_MBENC_REFPICSELECT_L0_CM                    =  7,
    GEN8_AVC_MBENC_MV_DATA_FROM_ME_CM                    =  8,
    GEN8_AVC_MBENC_4xME_DISTORTION_CM                    =  9,
    GEN8_AVC_MBENC_SLICEMAP_DATA_CM                      = 10,
    GEN8_AVC_MBENC_FWD_MB_DATA_CM                        = 11,
    GEN8_AVC_MBENC_FWD_MV_DATA_CM                        = 12,
    GEN8_AVC_MBENC_MBQP_CM                               = 13,
    GEN8_AVC_MBENC_MBBRC_CONST_DATA_CM                   = 14,
    GEN8_AVC_MBENC_VME_INTER_PRED_CURR_PIC_IDX_0_CM      = 15,
    GEN8_AVC_MBENC_VME_INTER_PRED_FWD_PIC_IDX0_CM        = 16,
    GEN8_AVC_MBENC_VME_INTER_PRED_BWD_PIC_IDX0_0_CM      = 17,
    GEN8_AVC_MBENC_VME_INTER_PRED_FWD_PIC_IDX1_CM        = 18,
    GEN8_AVC_MBENC_VME_INTER_PRED_BWD_PIC_IDX1_0_CM      = 19,
    GEN8_AVC_MBENC_VME_INTER_PRED_FWD_PIC_IDX2_CM        = 20,
    GEN8_AVC_MBENC_RESERVED0_CM                          = 21,
    GEN8_AVC_MBENC_VME_INTER_PRED_FWD_PIC_IDX3_CM        = 22,
    GEN8_AVC_MBENC_RESERVED1_CM                          = 23,
    GEN8_AVC_MBENC_VME_INTER_PRED_FWD_PIC_IDX4_CM        = 24,
    GEN8_AVC_MBENC_RESERVED2_CM                          = 25,
    GEN8_AVC_MBENC_VME_INTER_PRED_FWD_PIC_IDX5_CM        = 26,
    GEN8_AVC_MBENC_RESERVED3_CM                          = 27,
    GEN8_AVC_MBENC_VME_INTER_PRED_FWD_PIC_IDX6_CM        = 28,
    GEN8_AVC_MBENC_RESERVED4_CM                          = 29,
    GEN8_AVC_MBENC_VME_INTER_PRED_FWD_PIC_IDX7_CM        = 30,
    GEN8_AVC_MBENC_RESERVED5_CM                          = 31,
    GEN8_AVC_MBENC_VME_INTER_PRED_CURR_PIC_IDX_1_CM      = 32,
    GEN8_AVC_MBENC_VME_INTER_PRED_BWD_PIC_IDX0_1_CM      = 33,
    GEN8_AVC_MBENC_RESERVED6_CM                          = 34,
    GEN8_AVC_MBENC_VME_INTER_PRED_BWD_PIC_IDX1_1_CM      = 35,
    GEN8_AVC_MBENC_RESERVED7_CM                          = 36,
    GEN8_AVC_MBENC_FLATNESS_CHECK_CM                     = 37,
    GEN8_AVC_MBENC_MAD_DATA_CM                           = 38,
    GEN8_AVC_MBENC_FORCE_NONSKIP_MB_MAP_CM               = 39,
    GEN8_AVC_MBENC_WIDI_WA_DATA_CM                       = 40,
    GEN8_AVC_MBENC_BRC_CURBE_DATA_CM                     = 41,
    GEN8_AVC_MBENC_STATIC_FRAME_DETECTION_OUTPUT_CM      = 42,
    GEN8_AVC_MBENC_NUM_SURFACES_CM                       = 43
} gen8_avc_binding_table_offset_mbenc;

typedef enum _gen8_avc_binding_table_offset_scaling {
    GEN8_SCALING_FRAME_SRC_Y_CM                 = 0,
    GEN8_SCALING_FRAME_DST_Y_CM                 = 1,
    GEN8_SCALING_FIELD_TOP_SRC_Y_CM             = 0,
    GEN8_SCALING_FIELD_TOP_DST_Y_CM             = 1,
    GEN8_SCALING_FIELD_BOT_SRC_Y_CM             = 2,
    GEN8_SCALING_FIELD_BOT_DST_Y_CM             = 3,
    GEN8_SCALING_FRAME_FLATNESS_DST_CM          = 4,
    GEN8_SCALING_FIELD_TOP_FLATNESS_DST_CM      = 4,
    GEN8_SCALING_FIELD_BOT_FLATNESS_DST_CM      = 5,
    GEN8_SCALING_FRAME_MBVPROCSTATS_DST_CM      = 6,
    GEN8_SCALING_FIELD_TOP_MBVPROCSTATS_DST_CM  = 6,
    GEN8_SCALING_FIELD_BOT_MBVPROCSTATS_DST_CM  = 7,
    GEN8_SCALING_NUM_SURFACES_CM                = 8
} gen8_avc_binding_table_offset_scaling;

typedef struct _gen8_avc_me_curbe_data {
    struct {
        uint32_t skip_mode_enable: 1;
        uint32_t adaptive_enable: 1;
        uint32_t bi_mix_dis: 1;
        uint32_t reserved0: 2;
        uint32_t early_ime_success_enable: 1;
        uint32_t reserved1: 1;
        uint32_t t8x8_flag_for_inter_enable: 1;
        uint32_t reserved2: 16;
        uint32_t early_ime_stop: 8;
    } dw0;

    struct {
        uint32_t max_num_mvs: 6;
        uint32_t reserved0: 10;
        uint32_t bi_weight: 6;
        uint32_t reserved1: 6;
        uint32_t uni_mix_disable: 1;
        uint32_t reserved2: 3;
    } dw1;

    struct {
        uint32_t max_len_sp: 8;
        uint32_t max_num_su: 8;
        uint32_t reserved0: 16;
    } dw2;

    struct {
        uint32_t src_size: 2;
        uint32_t reserved0: 2;
        uint32_t mb_type_remap: 2;
        uint32_t src_access: 1;
        uint32_t ref_access: 1;
        uint32_t search_ctrl: 3;
        uint32_t dual_search_path_option: 1;
        uint32_t sub_pel_mode: 2;
        uint32_t skip_type: 1;
        uint32_t disable_field_cache_allocation: 1;
        uint32_t inter_chroma_mode: 1;
        uint32_t ft_enable: 1;
        uint32_t bme_disable_fbr: 1;
        uint32_t block_based_skip_enable: 1;
        uint32_t inter_sad: 2;
        uint32_t intra_sad: 2;
        uint32_t sub_mb_part_mask: 7;
        uint32_t reserved1: 1;
    } dw3;

    struct {
        uint32_t reserved0: 8;
        uint32_t picture_height_minus1: 8;
        uint32_t picture_width: 8;
        uint32_t reserved1: 8;
    } dw4;

    struct {
        uint32_t reserved0: 8;
        uint32_t qp_prime_y: 8;
        uint32_t ref_width: 8;
        uint32_t ref_height: 8;
    } dw5;

    struct {
        uint32_t reserved0: 3;
        uint32_t write_distortions: 1;
        uint32_t use_mv_from_prev_step: 1;
        uint32_t reserved1: 3;
        uint32_t super_combine_dist: 8;
        uint32_t max_vmvr: 16;
    } dw6;

    struct {
        uint32_t reserved0: 16;
        uint32_t mv_cost_scale_factor: 2;
        uint32_t bilinear_enable: 1;
        uint32_t src_field_polarity: 1;
        uint32_t weightedsad_harr: 1;
        uint32_t ac_only_haar: 1;
        uint32_t ref_id_cost_mode: 1;
        uint32_t reserved1: 1;
        uint32_t skip_center_mask: 8;
    } dw7;

    struct {
        uint32_t mode_0_cost: 8;
        uint32_t mode_1_cost: 8;
        uint32_t mode_2_cost: 8;
        uint32_t mode_3_cost: 8;
    } dw8;

    struct {
        uint32_t mode_4_cost: 8;
        uint32_t mode_5_cost: 8;
        uint32_t mode_6_cost: 8;
        uint32_t mode_7_cost: 8;
    } dw9;

    struct {
        uint32_t mode_8_cost: 8;
        uint32_t mode_9_cost: 8;
        uint32_t ref_id_cost: 8;
        uint32_t chroma_intra_mode_cost: 8;
    } dw10;

    struct {
        uint32_t mv_0_cost: 8;
        uint32_t mv_1_cost: 8;
        uint32_t mv_2_cost: 8;
        uint32_t mv_3_cost: 8;
    } dw11;

    struct {
        uint32_t mv_4_cost: 8;
        uint32_t mv_5_cost: 8;
        uint32_t mv_6_cost: 8;
        uint32_t mv_7_cost: 8;
    } dw12;

    struct {
        uint32_t num_ref_idx_l0_minus1: 8;
        uint32_t num_ref_idx_l1_minus1: 8;
        uint32_t actual_mb_width: 8;
        uint32_t actual_mb_height: 8;
    } dw13;

    struct {
        uint32_t l0_ref_id0_field_parity: 1;
        uint32_t l0_ref_id1_field_parity: 1;
        uint32_t l0_ref_id2_field_parity: 1;
        uint32_t l0_ref_id3_field_parity: 1;
        uint32_t l0_ref_id4_field_parity: 1;
        uint32_t l0_ref_id5_field_parity: 1;
        uint32_t l0_ref_id6_field_parity: 1;
        uint32_t l0_ref_id7_field_parity: 1;
        uint32_t l1_ref_id0_field_parity: 1;
        uint32_t l1_ref_id1_field_parity: 1;
        uint32_t reserved: 22;
    } dw14;

    struct {
        uint32_t prev_mv_read_pos_factor : 8;
        uint32_t mv_shift_factor : 8;
        uint32_t reserved: 16;
    } dw15;

    struct {
        struct generic_search_path_delta sp_delta_0;
        struct generic_search_path_delta sp_delta_1;
        struct generic_search_path_delta sp_delta_2;
        struct generic_search_path_delta sp_delta_3;
    } dw16;

    struct {
        struct generic_search_path_delta sp_delta_4;
        struct generic_search_path_delta sp_delta_5;
        struct generic_search_path_delta sp_delta_6;
        struct generic_search_path_delta sp_delta_7;
    } dw17;

    struct {
        struct generic_search_path_delta sp_delta_8;
        struct generic_search_path_delta sp_delta_9;
        struct generic_search_path_delta sp_delta_10;
        struct generic_search_path_delta sp_delta_11;
    } dw18;

    struct {
        struct generic_search_path_delta sp_delta_12;
        struct generic_search_path_delta sp_delta_13;
        struct generic_search_path_delta sp_delta_14;
        struct generic_search_path_delta sp_delta_15;
    } dw19;

    struct {
        struct generic_search_path_delta sp_delta_16;
        struct generic_search_path_delta sp_delta_17;
        struct generic_search_path_delta sp_delta_18;
        struct generic_search_path_delta sp_delta_19;
    } dw20;

    struct {
        struct generic_search_path_delta sp_delta_20;
        struct generic_search_path_delta sp_delta_21;
        struct generic_search_path_delta sp_delta_22;
        struct generic_search_path_delta sp_delta_23;
    } dw21;

    struct {
        struct generic_search_path_delta sp_delta_24;
        struct generic_search_path_delta sp_delta_25;
        struct generic_search_path_delta sp_delta_26;
        struct generic_search_path_delta sp_delta_27;
    } dw22;

    struct {
        struct generic_search_path_delta sp_delta_28;
        struct generic_search_path_delta sp_delta_29;
        struct generic_search_path_delta sp_delta_30;
        struct generic_search_path_delta sp_delta_31;
    } dw23;

    struct {
        struct generic_search_path_delta sp_delta_32;
        struct generic_search_path_delta sp_delta_33;
        struct generic_search_path_delta sp_delta_34;
        struct generic_search_path_delta sp_delta_35;
    } dw24;

    struct {
        struct generic_search_path_delta sp_delta_36;
        struct generic_search_path_delta sp_delta_37;
        struct generic_search_path_delta sp_delta_38;
        struct generic_search_path_delta sp_delta_39;
    } dw25;

    struct {
        struct generic_search_path_delta sp_delta_40;
        struct generic_search_path_delta sp_delta_41;
        struct generic_search_path_delta sp_delta_42;
        struct generic_search_path_delta sp_delta_43;
    } dw26;

    struct {
        struct generic_search_path_delta sp_delta_44;
        struct generic_search_path_delta sp_delta_45;
        struct generic_search_path_delta sp_delta_46;
        struct generic_search_path_delta sp_delta_47;
    } dw27;

    struct {
        struct generic_search_path_delta sp_delta_48;
        struct generic_search_path_delta sp_delta_49;
        struct generic_search_path_delta sp_delta_50;
        struct generic_search_path_delta sp_delta_51;
    } dw28;

    struct {
        struct generic_search_path_delta sp_delta_52;
        struct generic_search_path_delta sp_delta_53;
        struct generic_search_path_delta sp_delta_54;
        struct generic_search_path_delta sp_delta_55;
    } dw29;

    struct {
        uint32_t reserved0;
    } dw30;

    struct {
        uint32_t reserved0;
    } dw31;

    struct {
        uint32_t _4x_memv_output_data_surf_index;
    } dw32;

    struct {
        uint32_t _16x_32x_memv_input_data_surf_index;
    } dw33;

    struct {
        uint32_t _4x_me_output_dist_surf_index;
    } dw34;

    struct {
        uint32_t _4x_me_output_brc_dist_surf_index;
    } dw35;

    struct {
        uint32_t vme_fwd_inter_pred_surf_index;
    } dw36;

    struct {
        uint32_t vme_bdw_inter_pred_surf_index;
    } dw37;

    /* reserved */
    struct {
        uint32_t reserved;
    } dw38;
} gen8_avc_me_curbe_data;

typedef enum _gen8_avc_binding_table_offset_me {
    GEN8_AVC_ME_MV_DATA_SURFACE_CM       = 0,
    GEN8_AVC_16xME_MV_DATA_SURFACE_CM    = 1,
    GEN8_AVC_32xME_MV_DATA_SURFACE_CM    = 1,
    GEN8_AVC_ME_DISTORTION_SURFACE_CM    = 2,
    GEN8_AVC_ME_BRC_DISTORTION_CM        = 3,
    GEN8_AVC_ME_RESERVED0_CM             = 4,
    GEN8_AVC_ME_CURR_FOR_FWD_REF_CM      = 5,
    GEN8_AVC_ME_FWD_REF_IDX0_CM          = 6,
    GEN8_AVC_ME_RESERVED1_CM             = 7,
    GEN8_AVC_ME_FWD_REF_IDX1_CM          = 8,
    GEN8_AVC_ME_RESERVED2_CM             = 9,
    GEN8_AVC_ME_FWD_REF_IDX2_CM          = 10,
    GEN8_AVC_ME_RESERVED3_CM             = 11,
    GEN8_AVC_ME_FWD_REF_IDX3_CM          = 12,
    GEN8_AVC_ME_RESERVED4_CM             = 13,
    GEN8_AVC_ME_FWD_REF_IDX4_CM          = 14,
    GEN8_AVC_ME_RESERVED5_CM             = 15,
    GEN8_AVC_ME_FWD_REF_IDX5_CM          = 16,
    GEN8_AVC_ME_RESERVED6_CM             = 17,
    GEN8_AVC_ME_FWD_REF_IDX6_CM          = 18,
    GEN8_AVC_ME_RESERVED7_CM             = 19,
    GEN8_AVC_ME_FWD_REF_IDX7_CM          = 20,
    GEN8_AVC_ME_RESERVED8_CM             = 21,
    GEN8_AVC_ME_CURR_FOR_BWD_REF_CM      = 22,
    GEN8_AVC_ME_BWD_REF_IDX0_CM          = 23,
    GEN8_AVC_ME_RESERVED9_CM             = 24,
    GEN8_AVC_ME_BWD_REF_IDX1_CM          = 25,
    GEN8_AVC_ME_RESERVED10_CM            = 26,
    GEN8_AVC_ME_NUM_SURFACES_CM          = 27
} gen8_avc_binding_table_offset_me;
#endif /* GEN8_AVC_ENCODER_H */
