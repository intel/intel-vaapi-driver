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
 * SOFTWAR OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Chen, Peng <chen.c.peng@intel.com>
 *
 */

#ifndef GEN9_HEVC_ENCODER_KERNELS_H
#define GEN9_HEVC_ENCODER_KERNELS_H

// Scaling kernel parameters
typedef enum _gen9_hevc_binding_table_offset_scaling {
    GEN9_HEVC_SCALING_FRAME_SRC_Y_INDEX                      = 0,
    GEN9_HEVC_SCALING_FRAME_DST_Y_INDEX                      = 1,
    GEN9_HEVC_SCALING_FRAME_MBVPROCSTATS_DST_INDEX           = 4,
    GEN9_HEVC_SCALING_NUM_SURFACES                           = 6
} gen9_hevc_binding_table_offset_scaling;

typedef struct _gen9_hevc_scaling4x_curbe_data {
    struct {
        unsigned int   input_picture_width  : 16;
        unsigned int   input_picture_height : 16;
    } dw0;

    struct {
        unsigned int   input_y_bti;
    } dw1;

    struct {
        unsigned int   output_y_bti;
    } dw2;

    struct {
        unsigned int reserved;
    } dw3;

    struct {
        unsigned int reserved;
    } dw4;

    struct {
        unsigned int flatness_threshold;
    } dw5;

    struct {
        unsigned int enable_mb_flatness_check;
    } dw6;

    struct {
        unsigned int enable_mb_variance_output;
    } dw7;

    struct {
        unsigned int enable_mb_pixel_average_output;
    } dw8;

    struct {
        unsigned int reserved;
    } dw9;

    struct {
        unsigned int mbv_proc_stat_bti;
    } dw10;

    struct {
        unsigned int reserved;
    } dw11;
} gen9_hevc_scaling4x_curbe_data;

typedef struct _gen9_hevc_scaling2x_curbe_data {
    struct {
        unsigned int   input_picture_width  : 16;
        unsigned int   input_picture_height : 16;
    } dw0;

    /* dw1-dw7 */
    unsigned int reserved1[7];

    struct {
        unsigned int input_y_bti;
    } dw8;

    struct {
        unsigned int output_y_bti;
    } dw9;
} gen9_hevc_scaling2x_curbe_data;

// ME kernel parameters
typedef enum _gen9_hevc_binding_table_offset_me {
    GEN9_HEVC_ME_MV_DATA_SURFACE_INDEX       = 0,
    GEN9_HEVC_ME_16X_MV_DATA_SURFACE_INDEX   = 1,
    GEN9_HEVC_ME_32X_MV_DATA_SURFACE_INDEX   = 1,
    GEN9_HEVC_ME_DISTORTION_SURFACE_INDEX    = 2,
    GEN9_HEVC_ME_BRC_DISTORTION_INDEX        = 3,
    GEN9_HEVC_ME_RESERVED0_INDEX             = 4,
    GEN9_HEVC_ME_CURR_FOR_FWD_REF_INDEX      = 5,
    GEN9_HEVC_ME_FWD_REF_IDX0_INDEX          = 6,
    GEN9_HEVC_ME_RESERVED1_INDEX             = 7,
    GEN9_HEVC_ME_FWD_REF_IDX1_INDEX          = 8,
    GEN9_HEVC_ME_RESERVED2_INDEX             = 9,
    GEN9_HEVC_ME_FWD_REF_IDX2_INDEX          = 10,
    GEN9_HEVC_ME_RESERVED3_INDEX             = 11,
    GEN9_HEVC_ME_FWD_REF_IDX3_INDEX          = 12,
    GEN9_HEVC_ME_RESERVED4_INDEX             = 13,
    GEN9_HEVC_ME_FWD_REF_IDX4_INDEX          = 14,
    GEN9_HEVC_ME_RESERVED5_INDEX             = 15,
    GEN9_HEVC_ME_FWD_REF_IDX5_INDEX          = 16,
    GEN9_HEVC_ME_RESERVED6_INDEX             = 17,
    GEN9_HEVC_ME_FWD_REF_IDX6_INDEX          = 18,
    GEN9_HEVC_ME_RESERVED7_INDEX             = 19,
    GEN9_HEVC_ME_FWD_REF_IDX7_INDEX          = 20,
    GEN9_HEVC_ME_RESERVED8_INDEX             = 21,
    GEN9_HEVC_ME_CURR_FOR_BWD_REF_INDEX      = 22,
    GEN9_HEVC_ME_BWD_REF_IDX0_INDEX          = 23,
    GEN9_HEVC_ME_RESERVED9_INDEX             = 24,
    GEN9_HEVC_ME_BWD_REF_IDX1_INDEX          = 25,
    GEN9_HEVC_ME_VDENC_STREAMIN_INDEX        = 26,
    GEN9_HEVC_ME_NUM_SURFACES_INDEX          = 27
} gen9_hevc_binding_table_offset_me;

struct gen9_search_path_delta {
    char gen9_search_path_delta_x: 4;
    char gen9_search_path_delta_y: 4;
};

typedef struct _gen9_hevc_me_curbe_data {
    struct {
        unsigned int skip_mode_enable: 1;
        unsigned int adaptive_enable: 1;
        unsigned int bi_mix_dis: 1;
        unsigned int reserved0: 2;
        unsigned int early_ime_success_enable: 1;
        unsigned int reserved1: 1;
        unsigned int t8x8_flag_for_inter_enable: 1;
        unsigned int reserved2: 16;
        unsigned int early_ime_stop: 8;
    } dw0;

    struct {
        unsigned int max_num_mvs: 6;
        unsigned int reserved0: 10;
        unsigned int bi_weight: 6;
        unsigned int reserved1: 6;
        unsigned int uni_mix_disable: 1;
        unsigned int reserved2: 3;
    } dw1;

    struct {
        unsigned int max_len_sp: 8;
        unsigned int max_num_su: 8;
        unsigned int reserved0: 16;
    } dw2;

    struct {
        unsigned int src_size: 2;
        unsigned int reserved0: 2;
        unsigned int mb_type_remap: 2;
        unsigned int src_access: 1;
        unsigned int ref_access: 1;
        unsigned int search_ctrl: 3;
        unsigned int dual_search_path_option: 1;
        unsigned int sub_pel_mode: 2;
        unsigned int skip_type: 1;
        unsigned int disable_field_cache_allocation: 1;
        unsigned int inter_chroma_mode: 1;
        unsigned int ft_enable: 1;
        unsigned int bme_disable_fbr: 1;
        unsigned int block_based_skip_enable: 1;
        unsigned int inter_sad: 2;
        unsigned int intra_sad: 2;
        unsigned int sub_mb_part_mask: 7;
        unsigned int reserved1: 1;
    } dw3;

    struct {
        unsigned int reserved0: 8;
        unsigned int picture_height_minus1: 8;
        unsigned int picture_width: 8;
        unsigned int reserved1: 8;
    } dw4;

    struct {
        unsigned int reserved0: 8;
        unsigned int qp_prime_y: 8;
        unsigned int ref_width: 8;
        unsigned int ref_height: 8;
    } dw5;

    struct {
        unsigned int reserved0: 3;
        unsigned int write_distortions: 1;
        unsigned int use_mv_from_prev_step: 1;
        unsigned int reserved1: 3;
        unsigned int super_combine_dist: 8;
        unsigned int max_vmvr: 16;
    } dw6;

    struct {
        unsigned int reserved0: 16;
        unsigned int mv_cost_scale_factor: 2;
        unsigned int bilinear_enable: 1;
        unsigned int src_field_polarity: 1;
        unsigned int weightedsad_harr: 1;
        unsigned int ac_only_haar: 1;
        unsigned int ref_id_cost_mode: 1;
        unsigned int reserved1: 1;
        unsigned int skip_center_mask: 8;
    } dw7;

    struct {
        unsigned int mode_0_cost: 8;
        unsigned int mode_1_cost: 8;
        unsigned int mode_2_cost: 8;
        unsigned int mode_3_cost: 8;
    } dw8;

    struct {
        unsigned int mode_4_cost: 8;
        unsigned int mode_5_cost: 8;
        unsigned int mode_6_cost: 8;
        unsigned int mode_7_cost: 8;
    } dw9;

    struct {
        unsigned int mode_8_cost: 8;
        unsigned int mode_9_cost: 8;
        unsigned int ref_id_cost: 8;
        unsigned int chroma_intra_mode_cost: 8;
    } dw10;

    struct {
        unsigned int mv_0_cost: 8;
        unsigned int mv_1_cost: 8;
        unsigned int mv_2_cost: 8;
        unsigned int mv_3_cost: 8;
    } dw11;

    struct {
        unsigned int mv_4_cost: 8;
        unsigned int mv_5_cost: 8;
        unsigned int mv_6_cost: 8;
        unsigned int mv_7_cost: 8;
    } dw12;

    struct {
        unsigned int num_ref_idx_l0_minus1: 8;
        unsigned int num_ref_idx_l1_minus1: 8;
        unsigned int ref_streamin_cost: 8;
        unsigned int roi_enable: 3;
        unsigned int reserved0: 5;
    } dw13;

    struct {
        unsigned int l0_ref_pic_polarity_bits: 8;
        unsigned int l1_ref_pic_polarity_bits: 2;
        unsigned int reserved: 22;
    } dw14;

    struct {
        unsigned int prev_mv_read_pos_factor : 8;
        unsigned int mv_shift_factor : 8;
        unsigned int reserved: 16;
    } dw15;

    struct {
        struct gen9_search_path_delta sp_delta_0;
        struct gen9_search_path_delta sp_delta_1;
        struct gen9_search_path_delta sp_delta_2;
        struct gen9_search_path_delta sp_delta_3;
    } dw16;

    struct {
        struct gen9_search_path_delta sp_delta_4;
        struct gen9_search_path_delta sp_delta_5;
        struct gen9_search_path_delta sp_delta_6;
        struct gen9_search_path_delta sp_delta_7;
    } dw17;

    struct {
        struct gen9_search_path_delta sp_delta_8;
        struct gen9_search_path_delta sp_delta_9;
        struct gen9_search_path_delta sp_delta_10;
        struct gen9_search_path_delta sp_delta_11;
    } dw18;

    struct {
        struct gen9_search_path_delta sp_delta_12;
        struct gen9_search_path_delta sp_delta_13;
        struct gen9_search_path_delta sp_delta_14;
        struct gen9_search_path_delta sp_delta_15;
    } dw19;

    struct {
        struct gen9_search_path_delta sp_delta_16;
        struct gen9_search_path_delta sp_delta_17;
        struct gen9_search_path_delta sp_delta_18;
        struct gen9_search_path_delta sp_delta_19;
    } dw20;

    struct {
        struct gen9_search_path_delta sp_delta_20;
        struct gen9_search_path_delta sp_delta_21;
        struct gen9_search_path_delta sp_delta_22;
        struct gen9_search_path_delta sp_delta_23;
    } dw21;

    struct {
        struct gen9_search_path_delta sp_delta_24;
        struct gen9_search_path_delta sp_delta_25;
        struct gen9_search_path_delta sp_delta_26;
        struct gen9_search_path_delta sp_delta_27;
    } dw22;

    struct {
        struct gen9_search_path_delta sp_delta_28;
        struct gen9_search_path_delta sp_delta_29;
        struct gen9_search_path_delta sp_delta_30;
        struct gen9_search_path_delta sp_delta_31;
    } dw23;

    struct {
        struct gen9_search_path_delta sp_delta_32;
        struct gen9_search_path_delta sp_delta_33;
        struct gen9_search_path_delta sp_delta_34;
        struct gen9_search_path_delta sp_delta_35;
    } dw24;

    struct {
        struct gen9_search_path_delta sp_delta_36;
        struct gen9_search_path_delta sp_delta_37;
        struct gen9_search_path_delta sp_delta_38;
        struct gen9_search_path_delta sp_delta_39;
    } dw25;

    struct {
        struct gen9_search_path_delta sp_delta_40;
        struct gen9_search_path_delta sp_delta_41;
        struct gen9_search_path_delta sp_delta_42;
        struct gen9_search_path_delta sp_delta_43;
    } dw26;

    struct {
        struct gen9_search_path_delta sp_delta_44;
        struct gen9_search_path_delta sp_delta_45;
        struct gen9_search_path_delta sp_delta_46;
        struct gen9_search_path_delta sp_delta_47;
    } dw27;

    struct {
        struct gen9_search_path_delta sp_delta_48;
        struct gen9_search_path_delta sp_delta_49;
        struct gen9_search_path_delta sp_delta_50;
        struct gen9_search_path_delta sp_delta_51;
    } dw28;

    struct {
        struct gen9_search_path_delta sp_delta_52;
        struct gen9_search_path_delta sp_delta_53;
        struct gen9_search_path_delta sp_delta_54;
        struct gen9_search_path_delta sp_delta_55;
    } dw29;

    struct {
        unsigned int actual_mb_width: 16;
        unsigned int actual_mb_height: 16;
    } dw30;

    struct {
        unsigned int reserved0;
    } dw31;

    struct {
        unsigned int _4x_memv_output_data_surf_index;
    } dw32;

    struct {
        unsigned int _16x_32x_memv_input_data_surf_index;
    } dw33;

    struct {
        unsigned int _4x_me_output_dist_surf_index;
    } dw34;

    struct {
        unsigned int _4x_me_output_brc_dist_surf_index;
    } dw35;

    struct {
        unsigned int vme_fwd_inter_pred_surf_index;
    } dw36;

    struct {
        unsigned int vme_bdw_inter_pred_surf_index;
    } dw37;

    struct {
        unsigned int vdenc_stream_in_surf_index;
    } dw38;
} gen9_hevc_me_curbe_data;

// MBENC kernel paramerters

typedef struct _gen9_hevc_mbenc_control_region {
    unsigned short reserved0[2];
    unsigned short start_y_current_slice;
    unsigned short start_y_next_slice;
    unsigned short x_offset;
    unsigned short reserved1[2];
    unsigned short y_offset;
    unsigned int reserverd2[4];
    unsigned int alignment[8];
} gen9_hevc_mbenc_control_region;

#define GEN9_HEVC_ENC_REGION_START_Y_OFFSET                  (32)
#define GEN9_HEVC_ENC_CONCURRENT_SURFACE_HEIGHT              (32)

typedef struct _gen9_hevc_mbenc_downscaling2x_curbe_data {
    struct {
        unsigned int pic_width: 16;
        unsigned int pic_height: 16;
    } dw0;

    struct {
        unsigned int reserved;
    } dw1;

    struct {
        unsigned int reserved;
    } dw2;

    struct {
        unsigned int reserved;
    } dw3;

    struct {
        unsigned int  reserved;
    } dw4;

    struct {
        unsigned int reserved;
    } dw5;

    struct {
        unsigned int reserved;
    } dw6;

    struct {
        unsigned int reserved;
    } dw7;

    struct {
        unsigned int bti_src_y;
    } dw8;

    struct {
        unsigned int bit_dst_y;
    } dw9;
} gen9_hevc_mbenc_downscaling2x_curbe_data;

typedef struct _gen9_hevc_mbenc_32x32_pu_mode_curbe_data {
    struct {
        unsigned int frame_width: 16;
        unsigned int frame_height: 16;
    } dw0;

    struct {
        unsigned int slice_type: 2;
        unsigned int pu_type: 2;
        unsigned int reserved0: 1;
        unsigned int lcu_type: 1;
        unsigned int reserverd1: 18;
        unsigned int brc_enable: 1;
        unsigned int lcu_brc_enable: 1;
        unsigned int roi_enable: 1;
        unsigned int fast_surveillance_flag: 1;
        unsigned int reserverd2: 3;
        unsigned int enable_debug_dump: 1;
    } dw1;

    struct {
        unsigned int lambda;
    } dw2;

    struct {
        unsigned int mode_cost_32x32;
    } dw3;

    struct {
        unsigned int early_exit;
    } dw4;

    struct {
        unsigned int reserved;
    } dw5;

    struct {
        unsigned int reserved;
    } dw6;

    struct {
        unsigned int reserved;
    } dw7;

    struct {
        unsigned int bti_32x32_pu_output;
    } dw8;

    struct {
        unsigned int bti_src_y;
    } dw9;

    struct {
        unsigned int bti_src_y2x;
    } dw10;

    struct {
        unsigned int bti_slice_map;
    } dw11;

    struct {
        unsigned int bti_src_y2x_vme;
    } dw12;

    struct {
        unsigned int bti_brc_input;
    } dw13;

    struct {
        unsigned int bti_lcu_qp_surface;
    } dw14;

    struct {
        unsigned int bti_brc_data;
    } dw15;

    struct {
        unsigned int bti_kernel_debug;
    } dw16;
} gen9_hevc_mbenc_32x32_pu_mode_curbe_data;

typedef struct _gen9_hevc_mbenc_16x16_sad_curbe_data {
    struct {
        unsigned int frame_width: 16;
        unsigned int frame_height: 16;
    } dw0;

    struct {
        unsigned int log2_max_cu_size: 8;
        unsigned int log2_min_cu_size: 8;
        unsigned int log2_min_tu_size: 8;
        unsigned int enable_intra_early_exit: 1;
        unsigned int reserved: 7;
    } dw1;

    struct {
        unsigned int slice_type: 2;
        unsigned int sim_flag_for_inter: 1;
        unsigned int fast_surveillance_flag: 1;
        unsigned int reserved: 28;
    } dw2;

    struct {
        unsigned int reserved;
    } dw3;

    struct {
        unsigned int reserved;
    } dw4;

    struct {
        unsigned int reserved;
    } dw5;

    struct {
        unsigned int reserved;
    } dw6;

    struct {
        unsigned int reserved;
    } dw7;

    struct {
        unsigned int bti_src_y;
    } dw8;

    struct {
        unsigned int bti_sad_16x16_pu_output;
    } dw9;

    struct {
        unsigned int bti_32x32_pu_mode_decision;
    } dw10;

    struct {
        unsigned int bti_slice_map;
    } dw11;

    struct {
        unsigned int bti_simplest_intra;
    } dw12;

    struct {
        unsigned int bti_debug;
    } dw13;
} gen9_hevc_mbenc_16x16_sad_curbe_data;

typedef struct _gen9_hevc_enc_16x16_pu_curbe_data {
    struct {
        unsigned int frame_width: 16;
        unsigned int frame_height: 16;
    } dw0;

    struct {
        unsigned int log2_max_cu_size: 8;
        unsigned int log2_min_cu_size: 8;
        unsigned int log2_min_tu_size: 8;
        unsigned int slice_qp: 8;
    } dw1;

    struct {
        unsigned int fixed_point_lambda_pred_mode;
    } dw2;

    struct {
        unsigned int lambda_scaling_factor: 8;
        unsigned int slice_type: 2;
        unsigned int reserved0: 6;
        unsigned int widi_intra_refresh_en: 2;
        unsigned int enable_rolling_intra: 1;
        unsigned int half_update_mixed_lcu: 1;
        unsigned int reserved1: 4;
        unsigned int enable_intra_early_exit: 1;
        unsigned int brc_enable: 1;
        unsigned int lcu_brc_enable: 1;
        unsigned int roi_enable: 1;
        unsigned int fast_surveillance_flag: 1;
        unsigned int reserved2: 3;
    } dw3;

    struct {
        unsigned int penalty_for_intra_8x8_non_dc_pred_mode: 8;
        unsigned int intra_compute_type: 8;
        unsigned int avc_intra_8x8_mask: 8;
        unsigned int intra_sad_adjust: 8;
    } dw4;

    struct {
        unsigned int fixed_point_lambda_cu_mode_for_cost_calculation;
    } dw5;

    struct {
        unsigned int screen_content_flag: 1;
        unsigned int reserved: 31;
    } dw6;

    struct {
        unsigned int mode_cost_intra_non_pred: 8;
        unsigned int mode_cost_intra_16x16: 8;
        unsigned int mode_cost_intra_8x8: 8;
        unsigned int mode_cost_intra_4x4: 8;
    } dw7;

    struct {
        unsigned int fixed_point_lambda_cu_mode_for_luma;
    } dw8;

    struct {
        unsigned int widi_intra_refresh_mb_num: 16;
        unsigned int widi_intra_refresh_unit_in_mb: 8;
        unsigned int widi_intra_refresh_qp_delta: 8;
    } dw9;

    struct {
        unsigned int haar_transform_mode: 2;
        unsigned int simplified_flag_for_inter: 1;
        unsigned int reserved: 29;
    } dw10;

    struct {
        unsigned int reserved;
    } dw11;

    struct {
        unsigned int reserved;
    } dw12;

    struct {
        unsigned int reserved;
    } dw13;

    struct {
        unsigned int reserved;
    } dw14;

    struct {
        unsigned int reserved;
    } dw15;

    struct {
        unsigned int bti_src_y;
    } dw16;

    struct {
        unsigned int bti_sad_16x16_pu;
    } dw17;

    struct {
        unsigned int bti_pak_object;
    } dw18;

    struct {
        unsigned int bti_sad_32x32_pu_mode;
    } dw19;

    struct {
        unsigned int bti_vme_mode_8x8;
    } dw20;

    struct {
        unsigned int bti_slice_map;
    } dw21;

    struct {
        unsigned int bti_vme_src;
    } dw22;

    struct {
        unsigned int bti_brc_input;
    } dw23;

    struct {
        unsigned int bti_simplest_intra;
    } dw24;

    struct {
        unsigned int bti_lcu_qp_surface;
    } dw25;


    struct {
        unsigned int bti_brc_data;
    } dw26;

    struct {
        unsigned int bti_debug;
    } dw27;
} gen9_hevc_enc_16x16_pu_curbe_data;

typedef struct _gen9_hevc_mbenc_8x8_pu_curbe_data {
    struct {
        unsigned int frame_width: 16;
        unsigned int frame_height: 16;
    } dw0;

    struct {
        unsigned int slice_type: 2;
        unsigned int pu_type: 2;
        unsigned int dc_filter_flag: 1;
        unsigned int angle_refine_flag: 1;
        unsigned int lcu_type: 1;
        unsigned int screen_content_flag: 1;
        unsigned int widi_intra_refresh_en: 2;
        unsigned int enable_rolling_intra: 1;
        unsigned int half_update_mixed_lcu: 1;
        unsigned int reserved0: 4;
        unsigned int qp_value: 8;
        unsigned int enable_intra_early_exit: 1;
        unsigned int brc_enable: 1;
        unsigned int lcu_brc_enable: 1;
        unsigned int roi_enable: 1;
        unsigned int fast_surveillance_flag: 1;
        unsigned int reserved1: 2;
        unsigned int enable_debug_dump: 1;
    } dw1;

    struct {
        unsigned int luma_lambda;
    } dw2;

    struct {
        unsigned int chroma_lambda;
    } dw3;

    struct {
        unsigned int harr_trans_form_flag: 2;
        unsigned int simplified_flag_for_inter: 1;
        unsigned int reserved: 29;
    } dw4;

    struct {
        unsigned int widi_intra_refresh_mb_num: 16;
        unsigned int widi_intra_refresh_unit_in_mb: 8;
        unsigned int widi_intra_refresh_qp_delta: 8;
    } dw5;

    struct {
        unsigned int reserved;
    } dw6;

    struct {
        unsigned int reserved;
    } dw7;

    struct {
        unsigned int bti_src_y;
    } dw8;

    struct {
        unsigned int bti_slice_map;
    } dw9;

    struct {
        unsigned int bti_vme_8x8_mode;
    } dw10;

    struct {
        unsigned int bti_intra_mode;
    } dw11;

    struct {
        unsigned int bti_brc_input;
    } dw12;

    struct {
        unsigned int bti_simplest_intra;
    } dw13;

    struct {
        unsigned int bti_lcu_qp_surface;
    } dw14;

    struct {
        unsigned int bti_brc_data;
    } dw15;

    struct {
        unsigned int bti_debug;
    } dw16;
} gen9_hevc_mbenc_8x8_pu_curbe_data;

typedef struct _gen9_hevc_mbenc_8x8_pu_fmode_curbe_data {
    struct {
        unsigned int frame_width: 16;
        unsigned int frame_height: 16;
    } dw0;

    struct {
        unsigned int slice_type: 2;
        unsigned int pu_type: 2;
        unsigned int pak_reording_flag: 1;
        unsigned int reserved0: 1;
        unsigned int lcu_type: 1;
        unsigned int screen_content_flag: 1;
        unsigned int widi_intra_refresh_en: 2;
        unsigned int enable_rolling_intra: 1;
        unsigned int half_update_mixed_lcu: 1;
        unsigned int reserved1: 12;
        unsigned int enable_intra_early_exit: 1;
        unsigned int brc_enable: 1;
        unsigned int lcu_brc_enable: 1;
        unsigned int roi_enable: 1;
        unsigned int fast_surveillance_flag: 1;
        unsigned int reserved2: 2;
        unsigned int enable_debug_dump: 1;
    } dw1;

    struct {
        unsigned int luma_lambda;
    } dw2;

    struct {
        unsigned int lambda_for_dist_calculation;
    } dw3;

    struct {
        unsigned int mode_cost_for_8x8_pu_tu8;
    } dw4;

    struct {
        unsigned int mode_cost_for_8x8_pu_tu4;
    } dw5;

    struct {
        unsigned int satd_16x16_pu_threshold: 16;
        unsigned int bias_factor_toward_8x8: 16;
    } dw6;

    struct {
        unsigned int qp: 16;
        unsigned int qp_for_inter: 16;
    } dw7;

    struct {
        unsigned int simplified_flag_for_inter: 1;
        unsigned int reserved0: 7;
        unsigned int kbl_control_flag: 1;
        unsigned int reserved1: 23;
    } dw8;

    struct {
        unsigned int widi_intra_refresh_mb_num: 16;
        unsigned int widi_intra_refresh_unit_in_mb: 8;
        unsigned int widi_intra_refresh_qp_delta: 8;
    } dw9;

    struct {
        unsigned int reserved;
    } dw10;

    struct {
        unsigned int reserved;
    } dw11;

    struct {
        unsigned int reserved;
    } dw12;

    struct {
        unsigned int reserved;
    } dw13;

    struct {
        unsigned int reserved;
    } dw14;

    struct {
        unsigned int reserved;
    } dw15;

    struct {
        unsigned int bti_pak_object;
    } dw16;

    struct {
        unsigned int bti_vme_8x8_mode;
    } dw17;

    struct {
        unsigned int bti_intra_mode;
    } dw18;

    struct {
        unsigned int bti_pak_command;
    } dw19;

    struct {
        unsigned int bti_slice_map;
    } dw20;

    struct {
        unsigned int bti_intra_dist;
    } dw21;

    struct {
        unsigned int bti_brc_input;
    } dw22;

    struct {
        unsigned int bti_simplest_intra;
    } dw23;

    struct {
        unsigned int bti_lcu_qp_surface;
    } dw24;

    struct {
        unsigned int bti_brc_data;
    } dw25;

    struct {
        unsigned int bti_debug;
    } dw26;
} gen9_hevc_mbenc_8x8_pu_fmode_curbe_data;

typedef struct _gen9_hevc_mbenc_b_32x32_pu_intra_curbe_data {
    struct {
        unsigned int frame_width: 16;
        unsigned int frame_height: 16;
    } dw0;

    struct {
        unsigned int slice_type: 2;
        unsigned int reserved0: 6;
        unsigned int log2_min_tu_size: 8;
        unsigned int flags: 8;
        unsigned int enable_intra_early_exit: 1;
        unsigned int hme_enable: 1;
        unsigned int fast_surveillance_flag: 1;
        unsigned int reserved1: 4;
        unsigned int enable_debug_dump: 1;
    } dw1;

    struct {
        unsigned int qp_value: 16;
        unsigned int qp_multiplier: 16;
    } dw2;

    struct {
        unsigned int reserved;
    } dw3;

    struct {
        unsigned int reserved;
    } dw4;

    struct {
        unsigned int reserved;
    } dw5;

    struct {
        unsigned int reserved;
    } dw6;

    struct {
        unsigned int reserved;
    } dw7;

    struct {
        unsigned int bti_per_32x32_pu_intra_checck;
    } dw8;

    struct {
        unsigned int bti_src_y;
    } dw9;

    struct {
        unsigned int bti_src_y2x;
    } dw10;

    struct {
        unsigned int bti_slice_map;
    } dw11;

    struct {
        unsigned int bti_vme_y2x;
    } dw12;

    struct {
        unsigned int bti_simplest_intra;
    } dw13;

    struct {
        unsigned int bti_hme_mv_pred;

    } dw14;

    struct {
        unsigned int bti_hme_dist;
    } dw15;

    struct {
        unsigned int bti_lcu_skip;
    } dw16;

    struct {
        unsigned int bti_debug;
    } dw17;
} gen9_hevc_mbenc_b_32x32_pu_intra_curbe_data;

typedef struct _gen9_hevc_mbenc_b_mb_enc_curbe_data {
    struct {
        unsigned int skip_mode_en: 1;
        unsigned int adaptive_en: 1;
        unsigned int bi_mix_dis: 1;
        unsigned int reserved0: 2;
        unsigned int early_ime_success_enable: 1;
        unsigned int reserved1: 1;
        unsigned int t_8x8_flag_for_inter_en: 1;
        unsigned int reserved2: 16;
        unsigned int early_ime_stop: 8;
    } dw0;

    struct {
        unsigned int max_num_mvs: 6;
        unsigned int reserved0: 10;
        unsigned int bi_weight: 6;
        unsigned int reserved1: 6;
        unsigned int uni_mix_disable: 1;
        unsigned int reserved2: 3;
    } dw1;

    struct {
        unsigned int len_sp: 8;
        unsigned int max_num_su: 8;
        unsigned int pic_width: 16;
    } dw2;

    struct {
        unsigned int src_size: 2;
        unsigned int reserved0: 2;
        unsigned int mb_type_remap: 2;
        unsigned int src_access: 1;
        unsigned int ref_access: 1;
        unsigned int search_ctrl: 3;
        unsigned int dual_search_path_option: 1;
        unsigned int sub_pel_mode: 2;
        unsigned int skip_type: 1;
        unsigned int disable_field_cache_alloc: 1;
        unsigned int inter_chroma_mode: 1;
        unsigned int ft_enable: 1;
        unsigned int bme_disable_fbr: 1;
        unsigned int block_based_skip_enable: 1;
        unsigned int inter_sad: 2;
        unsigned int intra_sad: 2;
        unsigned int sub_mb_part_mask: 7;
        unsigned int reserved1: 1;
    } dw3;

    struct {
        unsigned int pic_height_minus1: 16;
        unsigned int reserved0: 8;
        unsigned int enable_debug: 1;
        unsigned int reserved1: 3;
        unsigned int hme_enable: 1;
        unsigned int slice_type: 2;
        unsigned int use_actual_ref_qp_value: 1;
    } dw4;

    struct {
        unsigned int reserved: 16;
        unsigned int ref_width: 8;
        unsigned int ref_height: 8;
    } dw5;

    struct {
        unsigned int frame_width: 16;
        unsigned int frame_height: 16;
    } dw6;

    struct {
        unsigned int intra_part_mask: 5;
        unsigned int non_skip_zmv_added: 1;
        unsigned int non_skip_mode_added: 1;
        unsigned int luma_intra_src_corner_swap: 1;
        unsigned int reserved0: 8;
        unsigned int mv_cost_scale_factor: 2;
        unsigned int bilinear_enable: 1;
        unsigned int reserved1: 1;
        unsigned int weighted_sad_haar: 1;
        unsigned int aconly_haar: 1;
        unsigned int refid_cost_mode: 1;
        unsigned int reserved2: 1;
        unsigned int skip_center_mask: 8;
    } dw7;

    struct {
        unsigned int mode0_cost: 8;
        unsigned int mode1_cost: 8;
        unsigned int mode2_cost: 8;
        unsigned int mode3_cost: 8;
    } dw8;

    struct {
        unsigned int mode4_cost: 8;
        unsigned int mode5_cost: 8;
        unsigned int mode6_cost: 8;
        unsigned int mode7_cost: 8;
    } dw9;

    struct {
        unsigned int mode8_cost: 8;
        unsigned int mode9_cost: 8;
        unsigned int ref_id_cost: 8;
        unsigned int chroma_intra_mode_cost: 8;
    } dw10;

    struct {
        unsigned int mv0_cost: 8;
        unsigned int mv1_cost: 8;
        unsigned int mv2_cost: 8;
        unsigned int mv3_cost: 8;
    } dw11;

    struct {
        unsigned int mv4_cost: 8;
        unsigned int mv5_cost: 8;
        unsigned int mv6_cost: 8;
        unsigned int mv7_cost: 8;
    } dw12;

    struct {
        unsigned int qp_prime_y: 8;
        unsigned int qp_prime_cb: 8;
        unsigned int qp_prime_cr: 8;
        unsigned int target_size_in_word: 8;
    } dw13;

    struct {
        unsigned int sic_fwd_trans_coeff_thread_0: 16;
        unsigned int sic_fwd_trans_coeff_thread_1: 8;
        unsigned int sic_fwd_trans_coeff_thread_2: 8;
    } dw14;

    struct {
        unsigned int sic_fwd_trans_coeff_thread_3: 8;
        unsigned int sic_fwd_trans_coeff_thread_4: 8;
        unsigned int sic_fwd_trans_coeff_thread_5: 8;
        unsigned int sic_fwd_trans_coeff_thread_6: 8;
    } dw15;

    struct {
        struct gen9_search_path_delta sp_delta_0;
        struct gen9_search_path_delta sp_delta_1;
        struct gen9_search_path_delta sp_delta_2;
        struct gen9_search_path_delta sp_delta_3;
    } dw16;

    struct {
        struct gen9_search_path_delta sp_delta_4;
        struct gen9_search_path_delta sp_delta_5;
        struct gen9_search_path_delta sp_delta_6;
        struct gen9_search_path_delta sp_delta_7;
    } dw17;

    struct {
        struct gen9_search_path_delta sp_delta_8;
        struct gen9_search_path_delta sp_delta_9;
        struct gen9_search_path_delta sp_delta_10;
        struct gen9_search_path_delta sp_delta_11;
    } dw18;

    struct {
        struct gen9_search_path_delta sp_delta_12;
        struct gen9_search_path_delta sp_delta_13;
        struct gen9_search_path_delta sp_delta_14;
        struct gen9_search_path_delta sp_delta_15;
    } dw19;

    struct {
        struct gen9_search_path_delta sp_delta_16;
        struct gen9_search_path_delta sp_delta_17;
        struct gen9_search_path_delta sp_delta_18;
        struct gen9_search_path_delta sp_delta_19;
    } dw20;

    struct {
        struct gen9_search_path_delta sp_delta_20;
        struct gen9_search_path_delta sp_delta_21;
        struct gen9_search_path_delta sp_delta_22;
        struct gen9_search_path_delta sp_delta_23;
    } dw21;

    struct {
        struct gen9_search_path_delta sp_delta_24;
        struct gen9_search_path_delta sp_delta_25;
        struct gen9_search_path_delta sp_delta_26;
        struct gen9_search_path_delta sp_delta_27;
    } dw22;

    struct {
        struct gen9_search_path_delta sp_delta_28;
        struct gen9_search_path_delta sp_delta_29;
        struct gen9_search_path_delta sp_delta_30;
        struct gen9_search_path_delta sp_delta_31;
    } dw23;

    struct {
        struct gen9_search_path_delta sp_delta_32;
        struct gen9_search_path_delta sp_delta_33;
        struct gen9_search_path_delta sp_delta_34;
        struct gen9_search_path_delta sp_delta_35;
    } dw24;

    struct {
        struct gen9_search_path_delta sp_delta_36;
        struct gen9_search_path_delta sp_delta_37;
        struct gen9_search_path_delta sp_delta_38;
        struct gen9_search_path_delta sp_delta_39;
    } dw25;

    struct {
        struct gen9_search_path_delta sp_delta_40;
        struct gen9_search_path_delta sp_delta_41;
        struct gen9_search_path_delta sp_delta_42;
        struct gen9_search_path_delta sp_delta_43;
    } dw26;

    struct {
        struct gen9_search_path_delta sp_delta_44;
        struct gen9_search_path_delta sp_delta_45;
        struct gen9_search_path_delta sp_delta_46;
        struct gen9_search_path_delta sp_delta_47;
    } dw27;

    struct {
        struct gen9_search_path_delta sp_delta_48;
        struct gen9_search_path_delta sp_delta_49;
        struct gen9_search_path_delta sp_delta_50;
        struct gen9_search_path_delta sp_delta_51;
    } dw28;

    struct {
        struct gen9_search_path_delta sp_delta_52;
        struct gen9_search_path_delta sp_delta_53;
        struct gen9_search_path_delta sp_delta_54;
        struct gen9_search_path_delta sp_delta_55;
    } dw29;

    struct {
        unsigned int intra_4x4_mode_mask: 9;
        unsigned int reserved0: 7;
        unsigned int intra_8x8_mode_mask: 9;
        unsigned int reserved1: 7;
    } dw30;

    struct {
        unsigned int intra_16x16_mode_mask: 4;
        unsigned int intra_chroma_mode_mask: 4;
        unsigned int intra_chmpute_type: 2;
        unsigned int reserved: 22;
    } dw31;

    struct {
        unsigned int skip_val: 16;
        unsigned int multi_pred_l0_disable: 8;
        unsigned int multi_pred_l1_disable: 8;
    } dw32;

    struct {
        unsigned int intra_16x16_nondc_pred_penalty: 8;
        unsigned int intra_8x8_nondc_pred_penalty: 8;
        unsigned int intra_4x4_nondc_pred_penalty: 8;
        unsigned int reserved: 8;
    } dw33;

    struct {
        float lambda_me;
    } dw34;

    struct {
        unsigned int simp_intra_inter_threashold: 16;
        unsigned int mode_cost_sp: 8;
        unsigned int widi_intra_refresh_en: 2;
        unsigned int widi_first_intra_refresh: 1;
        unsigned int enable_rolling_intra: 1;
        unsigned int half_update_mixed_lcu: 1;
        unsigned int reserved: 3;
    } dw35;

    struct {
        unsigned int num_refidx_l0_minus_one: 8;
        unsigned int hme_combined_extra_sus: 8;
        unsigned int num_refidx_l1_minus_one: 8;
        unsigned int power_saving: 1;
        unsigned int brc_enable: 1;
        unsigned int lcu_brc_enable: 1;
        unsigned int roi_enable: 1;
        unsigned int fast_surveillance_flag: 1;
        unsigned int check_all_fractional_enable: 1;
        unsigned int hme_combined_over_lap: 2;
    } dw36;

    struct {
        unsigned int actual_qp_refid0_list0: 8;
        unsigned int actual_qp_refid1_list0: 8;
        unsigned int actual_qp_refid2_list0: 8;
        unsigned int actual_qp_refid3_list0: 8;
    } dw37;

    struct {
        unsigned int widi_num_intra_refresh_off_frames: 16;
        unsigned int widi_num_frame_in_gob: 16;
    } dw38;

    struct {
        unsigned int actual_qp_refid0_list1: 8;
        unsigned int actual_qp_refid1_list1: 8;
        unsigned int ref_cost: 16;
    } dw39;

    struct {
        unsigned int reserved;
    } dw40;

    struct {
        unsigned int reserved;
    } dw41;

    struct {
        unsigned int reserved;
    } dw42;

    struct {
        unsigned int reserved;
    } dw43;

    struct {
        unsigned int max_num_merge_candidates: 4;
        unsigned int max_num_ref_list0: 4;
        unsigned int max_num_ref_list1: 4;
        unsigned int reserved: 4;
        unsigned int max_vmvr: 16;
    } dw44;

    struct {
        unsigned int temporal_mvp_enable_flag: 1;
        unsigned int reserved: 7;
        unsigned int log2_parallel_merge_level: 8;
        unsigned int hme_combine_len_pslice: 8;
        unsigned int hme_combine_len_bslice: 8;
    } dw45;

    struct {
        unsigned int log2_min_tu_size: 8;
        unsigned int log2_max_tu_size: 8;
        unsigned int log2_min_cu_size: 8;
        unsigned int log2_max_cu_size: 8;
    } dw46;

    struct {
        unsigned int num_regions_in_slice: 8;
        unsigned int type_of_walking_pattern: 4;
        unsigned int chroma_flatness_check_flag: 1;
        unsigned int enable_intra_early_exit: 1;
        unsigned int skip_intra_krn_flag: 1;
        unsigned int screen_content_flag: 1;
        unsigned int is_low_delay: 1;
        unsigned int collocated_from_l0_flag: 1;
        unsigned int arbitary_slice_flag: 1;
        unsigned int multi_slice_flag: 1;
        unsigned int reserved: 4;
        unsigned int is_curr_ref_l0_long_term: 1;
        unsigned int is_curr_ref_l1_long_term: 1;
        unsigned int num_region_minus1: 6;
    } dw47;

    struct {
        unsigned int current_td_l0_0: 16;
        unsigned int current_td_l0_1: 16;
    } dw48;

    struct {
        unsigned int current_td_l0_2: 16;
        unsigned int current_td_l0_3: 16;
    } dw49;

    struct {
        unsigned int current_td_l1_0: 16;
        unsigned int current_td_l1_1: 16;
    } dw50;

    struct {
        unsigned int widi_intra_refresh_mb_num: 16;
        unsigned int widi_intra_refresh_unit_in_mb: 8;
        unsigned int widi_intra_refresh_qp_delta: 8;
    } dw51;

    struct {
        unsigned int num_of_units_in_region: 16;
        unsigned int max_height_in_region: 16;
    } dw52;

    struct {
        unsigned int widi_intra_refresh_ref_width: 8;
        unsigned int widi_intra_refresh_ref_height: 8;
        unsigned int reserved: 16;
    } dw53;

    struct {
        unsigned int reserved;
    } dw54;

    struct {
        unsigned int reserved;
    } dw55;

    struct {
        unsigned int bti_cu_record;
    } dw56;

    struct {
        unsigned int bti_pak_cmd;
    } dw57;

    struct {
        unsigned int bti_src_y;
    } dw58;

    struct {
        unsigned int bti_intra_dist;
    } dw59;

    struct {
        unsigned int bti_min_dist;
    } dw60;

    struct {
        unsigned int bti_hme_mv_pred_fwd_bwd_surf_index;
    } dw61;

    struct {
        unsigned int bti_hme_dist_surf_index;
    } dw62;

    struct {
        unsigned int bti_slice_map;
    } dw63;

    struct {
        unsigned int bti_vme_saved_uni_sic;
    } dw64;

    struct {
        unsigned int bti_simplest_intra;
    } dw65;

    struct {
        unsigned int bti_collocated_refframe;
    } dw66;

    struct {
        unsigned int bti_reserved;
    } dw67;

    struct {
        unsigned int bti_brc_input;
    } dw68;

    struct {
        unsigned int bti_lcu_qp;
    } dw69;

    struct {
        unsigned int bti_brc_data;
    } dw70;

    struct {
        unsigned int bti_vme_inter_prediction_surf_index;
    } dw71;

    union {
        unsigned int bti_vme_inter_prediction_b_surf_index;

        unsigned int bti_concurrent_thread_map;
    } dw72;

    union {
        unsigned int bti_concurrent_thread_map;

        unsigned int bti_mb_data_cur_frame;
    } dw73;

    union {
        unsigned int bti_mb_data_cur_frame;

        unsigned int bti_mvp_cur_frame;
    } dw74;

    union {
        unsigned int bti_mvp_cur_frame;

        unsigned int bti_debug;
    } dw75;

    struct {
        unsigned int bti_debug;
    } dw76;
} gen9_hevc_mbenc_b_mb_enc_curbe_data;

typedef struct _gen9_hevc_mbenc_b_pak_curbe_data {
    struct {
        unsigned int frame_width: 16;
        unsigned int frame_height: 16;
    } dw0;

    struct {
        unsigned int qp: 8;
        unsigned int reserved: 8;
        unsigned int max_vmvr: 16;
    } dw1;

    struct {
        unsigned int slice_type: 2;
        unsigned int reserved0: 6;
        unsigned int simplest_intra_enable: 1;
        unsigned int brc_enable: 1;
        unsigned int lcu_brc_enable: 1;
        unsigned int roi_enable: 1;
        unsigned int fast_surveillance_flag: 1;
        unsigned int enable_rolling_intra: 1;
        unsigned int reserved1: 2;
        unsigned int kbl_control_flag: 1;
        unsigned int reserved2: 14;
        unsigned int screen_content: 1;
    } dw2;

    struct {
        unsigned int widi_intra_refresh_mb_num: 16;
        unsigned int widi_intra_refresh_unit_in_mb: 8;
        unsigned int widi_intra_refresh_qp_delta: 8;
    } dw3;

    struct {
        unsigned int reserved;
    } dw4_15[12];

    struct {
        unsigned int bti_cu_record;
    } dw16;

    struct {
        unsigned int bti_pak_obj;
    } dw17;

    struct {
        unsigned int bti_slice_map;
    } dw18;

    struct {
        unsigned int bti_brc_input;
    } dw19;

    struct {
        unsigned int bti_lcu_qp;
    } dw20;

    struct {
        unsigned int bti_brc_data;
    } dw21;

    struct {
        unsigned int bti_mb_data;
    } dw22;

    struct {
        unsigned int bti_mvp_surface;
    } dw23;

    struct {
        unsigned int bti_debug;
    } dw24;
} gen9_hevc_mbenc_b_pak_curbe_data;

typedef enum _GEN9_HEVC_DOWNSCALE_STAGE {
    HEVC_ENC_DS_DISABLED                         = 0,
    HEVC_ENC_2xDS_STAGE                          = 1,
    HEVC_ENC_4xDS_STAGE                          = 2,
    HEVC_ENC16xDS_STAGE                          = 3,
    HEVC_ENC_2xDS_4xDS_STAGE                     = 4,
    HEVC_ENC_32xDS_STAGE                         = 5
} GEN9_HEVC_DOWNSCALE_STAGE;

typedef struct _gen95_hevc_mbenc_ds_combined_curbe_data {
    struct {
        unsigned int pak_bitdepth_chroma: 8;
        unsigned int pak_bitdepth_luma: 8;
        unsigned int enc_bitdepth_chroma: 8;
        unsigned int enc_bitdepth_luma: 7;
        unsigned int rounding_value: 1;
    } dw0;

    struct {
        unsigned int pic_format: 8;
        unsigned int pic_convert_flag: 1;
        unsigned int pic_down_scale: 3;
        unsigned int pic_mb_stat_output_cntrl: 1;
        unsigned int mbz: 19;
    } dw1;

    struct {
        unsigned int orig_pic_width: 16;
        unsigned int orig_pic_height: 16;
    } dw2;

    struct {
        unsigned int bti_surface_p010;
    } dw3;

    struct {
        unsigned int bti_surface_nv12;
    } dw4;

    struct {
        unsigned int bti_src_y_4xdownscaled;
    } dw5;

    struct {
        unsigned int bti_surf_mbstate;
    } dw6;

    struct {
        unsigned int bit_src_y_2xdownscaled;
    } dw7;
} gen95_hevc_mbenc_ds_combined_curbe_data;

#define GEN9_HEVC_AVBR_ACCURACY      30
#define GEN9_HEVC_AVBR_CONVERGENCE   150

typedef enum _GEN9_HEVC_BRC_INIT_FLAGS {
    HEVC_BRCINIT_ISCBR                       = 0x0010,
    HEVC_BRCINIT_ISVBR                       = 0x0020,
    HEVC_BRCINIT_ISAVBR                      = 0x0040,
    HEVC_BRCINIT_ISQVBR                      = 0x0080,
    HEVC_BRCINIT_FIELD_PIC                   = 0x0100,
    HEVC_BRCINIT_ISICQ                       = 0x0200,
    HEVC_BRCINIT_ISVCM                       = 0x0400,
    HEVC_BRCINIT_IGNORE_PICTURE_HEADER_SIZE  = 0x2000,
    HEVC_BRCINIT_ISCQP                       = 0x4000,
    HEVC_BRCINIT_DISABLE_MBBRC               = 0x8000
} GEN9_HEVC_BRCINIT_FLAGS;

// BRC kernel paramerters
typedef struct _gen9_hevc_brc_init_reset_curbe_data {
    struct {
        unsigned int profile_level_max_frame;
    } dw0;

    struct {
        unsigned int init_buf_full;
    } dw1;

    struct {
        unsigned int buf_size;
    } dw2;

    struct {
        unsigned int targe_bit_rate;
    } dw3;

    struct {
        unsigned int maximum_bit_rate;
    } dw4;

    struct {
        unsigned int minimum_bit_rate;
    } dw5;

    struct {
        unsigned int frame_rate_m;
    } dw6;

    struct {
        unsigned int frame_rate_d;
    } dw7;

    struct {
        unsigned int brc_flag: 16;
        unsigned int brc_param_a: 16;
    } dw8;

    struct {
        unsigned int brc_param_b: 16;
        unsigned int frame_width: 16;
    } dw9;

    struct {
        unsigned int frame_height: 16;
        unsigned int avbr_accuracy: 16;
    } dw10;

    struct {
        unsigned int avbr_convergence: 16;
        unsigned int minimum_qp: 16;
    } dw11;

    struct {
        unsigned int maximum_qp: 16;
        unsigned int number_slice: 16;
    } dw12;

    struct {
        unsigned int reserved: 16;
        unsigned int brc_param_c: 16;
    } dw13;

    struct {
        unsigned int brc_param_d: 16;
        unsigned int max_brc_level: 16;
    } dw14;

    struct {
        unsigned int reserved;
    } dw15;

    struct {
        unsigned int instant_rate_threshold0_pframe: 8;
        unsigned int instant_rate_threshold1_pframe: 8;
        unsigned int instant_rate_threshold2_pframe: 8;
        unsigned int instant_rate_threshold3_pframe: 8;
    } dw16;

    struct {
        unsigned int instant_rate_threshold0_bframe: 8;
        unsigned int instant_rate_threshold1_bframe: 8;
        unsigned int instant_rate_threshold2_bframe: 8;
        unsigned int instant_rate_threshold3_bframe: 8;
    } dw17;

    struct {
        unsigned int instant_rate_threshold0_iframe: 8;
        unsigned int instant_rate_threshold1_iframe: 8;
        unsigned int instant_rate_threshold2_iframe: 8;
        unsigned int instant_rate_threshold3_iframe: 8;
    } dw18;

    struct {
        unsigned int deviation_threshold0_pbframe: 8;
        unsigned int deviation_threshold1_pbframe: 8;
        unsigned int deviation_threshold2_pbframe: 8;
        unsigned int deviation_threshold3_pbframe: 8;
    } dw19;

    struct {
        unsigned int deviation_threshold4_pbframe: 8;
        unsigned int deviation_threshold5_pbframe: 8;
        unsigned int deviation_threshold6_pbframe: 8;
        unsigned int deviation_threshold7_pbframe: 8;
    } dw20;

    struct {
        unsigned int deviation_threshold0_vbr_control: 8;
        unsigned int deviation_threshold1_vbr_control: 8;
        unsigned int deviation_threshold2_vbr_control: 8;
        unsigned int deviation_threshold3_vbr_control: 8;
    } dw21;

    struct {
        unsigned int deviation_threshold4_vbr_control: 8;
        unsigned int deviation_threshold5_vbr_control: 8;
        unsigned int deviation_threshold6_vbr_control: 8;
        unsigned int deviation_threshold7_vbr_control: 8;
    } dw22;

    struct {
        unsigned int deviation_threshold0_iframe: 8;
        unsigned int deviation_threshold1_iframe: 8;
        unsigned int deviation_threshold2_iframe: 8;
        unsigned int deviation_threshold3_iframe: 8;
    } dw23;

    struct {
        unsigned int deviation_threshold4_iframe: 8;
        unsigned int deviation_threshold5_iframe: 8;
        unsigned int deviation_threshold6_iframe: 8;
        unsigned int deviation_threshold7_iframe: 8;
    } dw24;

    struct {
        unsigned int acqp_buffer: 8;
        unsigned int intra_sad_transform: 8;
        unsigned int reserved: 16;
    } dw25;

    struct {
        unsigned int   reserved;
    } dw26;

    struct {
        unsigned int   reserved;
    } dw27;

    struct {
        unsigned int   reserved;
    } dw28;

    struct {
        unsigned int   reserved;
    } dw29;

    struct {
        unsigned int   reserved;
    } dw30;

    struct {
        unsigned int   reserved;
    } dw31;
} gen9_hevc_brc_initreset_curbe_data;

// BRC Flag in BRC Update Kernel
typedef enum _GEN9_HEVC_BRC_UPDATE_FLAG {
    HEVC_BRC_UPDATE_IS_FIELD                  = 0x01,
    HEVC_BRC_UPDATE_IS_MBAFF                  = (0x01 << 1),
    HEVC_BRC_UPDATE_IS_BOTTOM_FIELD           = (0x01 << 2),
    HEVC_BRC_UPDATE_IS_ACTUALQP               = (0x01 << 6),
    HEVC_BRC_UPDATE_IS_REFERENCE              = (0x01 << 7)
} GEN9_HEVC_BRC_UPDATE_FLAG;

typedef enum _GEN9_HEVC_BRC_UPDATE_FRAME_TYPE {
    HEVC_BRC_FTYPE_P_OR_LB = 0,
    HEVC_BRC_FTYPE_B       = 1,
    HEVC_BRC_FTYPE_I       = 2,
    HEVC_BRC_FTYPE_B1      = 3,
    HEVC_BRC_FTYPE_B2      = 4
} GEN9_HEVC_BRC_UPDATE_FRAME_TYPE;

typedef struct _GEN9_HEVC_PAK_STATES {
    unsigned int HEVC_ENC_BYTECOUNT_FRAME;
    unsigned int HEVC_ENC_BYTECOUNT_FRAME_NOHEADER;
    unsigned int HEVC_ENC_IMAGE_STATUS_CONTROL;
    unsigned int reserved0;
    unsigned int HEVC_ENC_IMAGE_STATUS_CONTROL_FOR_LAST_PASS;
    unsigned int reserved1[3];
} GEN9_HEVC_PAK_STATES;

typedef struct _gen9_hevc_brc_udpate_curbe_data {
    struct {
        unsigned int target_size;
    } dw0;

    struct {
        unsigned int frame_number;
    } dw1;

    struct {
        unsigned int picture_header_size;
    } dw2;

    struct {
        unsigned int start_gadj_frame0: 16;
        unsigned int start_gadj_frame1: 16;
    } dw3;

    struct {
        unsigned int start_gadj_frame2: 16;
        unsigned int start_gadj_frame3: 16;
    } dw4;

    struct {
        unsigned int target_size_flag: 8;
        unsigned int brc_flag: 8;
        unsigned int max_num_paks: 8;
        unsigned int curr_frame_type: 8;
    } dw5;

    struct {
        unsigned int num_skipped_frames: 8;
        unsigned int cqp_value: 8;
        unsigned int roi_flag: 8;
        unsigned int roi_ratio: 8;
    } dw6;

    struct {
        unsigned int frame_width_in_lcu: 8;
        unsigned int reserved: 24;
    } dw7;

    struct {
        unsigned int start_global_adjust_mult0: 8;
        unsigned int start_global_adjust_mult1: 8;
        unsigned int start_global_adjust_mult2: 8;
        unsigned int start_global_adjust_mult3: 8;
    } dw8;

    struct {
        unsigned int start_global_adjust_mult4: 8;
        unsigned int start_global_adjust_divd0: 8;
        unsigned int start_global_adjust_divd1: 8;
        unsigned int start_global_adjust_divd2: 8;
    } dw9;

    struct {
        unsigned int start_global_adjust_divd3: 8;
        unsigned int start_global_adjust_divd4: 8;
        unsigned int qp_threshold0: 8;
        unsigned int qp_threshold1: 8;
    } dw10;

    struct {
        unsigned int qp_threshold2: 8;
        unsigned int qp_threshold3: 8;
        unsigned int g_rate_ratio_threshold0: 8;
        unsigned int g_rate_ratio_threshold1: 8;
    } dw11;

    struct {
        unsigned int g_rate_ratio_threshold2: 8;
        unsigned int g_rate_ratio_threshold3: 8;
        unsigned int g_rate_ratio_threshold4: 8;
        unsigned int g_rate_ratio_threshold5: 8;
    } dw12;

    struct {
        unsigned int g_rate_ratio_threshold6: 8;
        unsigned int g_rate_ratio_threshold7: 8;
        unsigned int g_rate_ratio_threshold8: 8;
        unsigned int g_rate_ratio_threshold9: 8;
    } dw13;

    struct {
        unsigned int g_rate_ratio_threshold10: 8;
        unsigned int g_rate_ratio_threshold11: 8;
        unsigned int g_rate_ratio_threshold12: 8;
        unsigned int parallel_mode: 8;
    } dw14;

    struct {
        unsigned int size_of_skipped_frames;
    } dw15;
} gen9_hevc_brc_udpate_curbe_data;

typedef struct _gen9_hevc_brc_coarse_intra_curbe_data {
    struct {
        unsigned int picture_width_in_luma_samples: 16;
        unsigned int picture_height_in_luma_samples: 16;
    } dw0;

    struct {
        unsigned int src_size: 2;
        unsigned int reserved0: 12;
        unsigned int skip_type: 1;
        unsigned int reserved1: 1;
        unsigned int inter_chroma_mode: 1;
        unsigned int ft_enable: 1;
        unsigned int reserved2: 1;
        unsigned int blk_skip_enabled: 1;
        unsigned int inter_sad: 2;
        unsigned int intra_sad: 2;
        unsigned int reserved3: 8;
    } dw1;

    struct {
        unsigned int intra_park_mask: 5;
        unsigned int non_skip_zmv_added: 1;
        unsigned int non_skip_mode_added: 1;
        unsigned int intra_corner_swap: 1;
        unsigned int reserved0: 8;
        unsigned int mv_cost_scale_factor: 2;
        unsigned int bilinear_enable: 1;
        unsigned int reserved1: 1;
        unsigned int weighted_sad_haar: 1;
        unsigned int aconly_haar: 1;
        unsigned int refid_cost_mode: 1;
        unsigned int reserved2: 1;
        unsigned int skip_center_mask: 8;
    } dw2;

    struct {
        unsigned int reserved;
    } dw3;

    struct {
        unsigned int reserved;
    } dw4;

    struct {
        unsigned int reserved;
    } dw5;

    struct {
        unsigned int reserved;
    } dw6;

    struct {
        unsigned int reserved;
    } dw7;

    struct {
        unsigned int bti_src_y4;
    } dw8;

    struct {
        unsigned int bti_intra_dist;
    } dw9;

    struct {
        unsigned int bti_vme_intra;
    } dw10;
} gen9_hevc_brc_coarse_intra_curbe_data;

// gen9 kernel header strcutures
typedef struct _gen9_hevc_enc_kernel_header {
    unsigned int reserved: 6;
    unsigned int kernel_start_pointer: 26;
} gen9_hevc_enc_kernel_header;

typedef struct _gen9_hevc_enc_kernels_header {
    int kernel_count;
    gen9_hevc_enc_kernel_header HEVC_ENC_I_2xDownSampling_Kernel;
    gen9_hevc_enc_kernel_header HEVC_ENC_I_32x32_PU_ModeDecision_Kernel;
    gen9_hevc_enc_kernel_header HEVC_ENC_I_16x16_PU_SADComputation_Kernel;
    gen9_hevc_enc_kernel_header HEVC_ENC_I_16x16_PU_ModeDecision_Kernel;
    gen9_hevc_enc_kernel_header HEVC_ENC_I_8x8_PU_Kernel;
    gen9_hevc_enc_kernel_header HEVC_ENC_I_8x8_PU_FMode_Kernel;
    gen9_hevc_enc_kernel_header HEVC_ENC_PB_32x32_PU_IntraCheck;
    gen9_hevc_enc_kernel_header HEVC_ENC_PB_MB;
    gen9_hevc_enc_kernel_header HEVC_ENC_I_DS4HME;
    gen9_hevc_enc_kernel_header HEVC_ENC_P_HME;
    gen9_hevc_enc_kernel_header HEVC_ENC_B_HME;
    gen9_hevc_enc_kernel_header HEVC_ENC_I_COARSE;
    gen9_hevc_enc_kernel_header HEVC_ENC_BRC_Init;
    gen9_hevc_enc_kernel_header HEVC_ENC_BRC_Reset;
    gen9_hevc_enc_kernel_header HEVC_ENC_BRC_Update;
    gen9_hevc_enc_kernel_header HEVC_ENC_BRC_LCU_Update;
    gen9_hevc_enc_kernel_header HEVC_ENC_PB_Pak;
    gen9_hevc_enc_kernel_header HEVC_ENC_PB_Widi;
    gen9_hevc_enc_kernel_header HEVC_ENC_BRC_Blockcopy;
    gen9_hevc_enc_kernel_header HEVC_ENC_DS_Combined;
} gen9_hevc_enc_kernels_header;

typedef struct _gen9_hevc_enc_kernels_header_bxt {
    int kernel_count;
    gen9_hevc_enc_kernel_header HEVC_ENC_I_2xDownSampling_Kernel;
    gen9_hevc_enc_kernel_header HEVC_ENC_I_32x32_PU_ModeDecision_Kernel;
    gen9_hevc_enc_kernel_header HEVC_ENC_I_16x16_PU_SADComputation_Kernel;
    gen9_hevc_enc_kernel_header HEVC_ENC_I_16x16_PU_ModeDecision_Kernel;
    gen9_hevc_enc_kernel_header HEVC_ENC_I_8x8_PU_Kernel;
    gen9_hevc_enc_kernel_header HEVC_ENC_I_8x8_PU_FMode_Kernel;
    gen9_hevc_enc_kernel_header HEVC_ENC_PB_32x32_PU_IntraCheck;
    gen9_hevc_enc_kernel_header HEVC_ENC_PB_MB;
    gen9_hevc_enc_kernel_header HEVC_ENC_I_DS4HME;
    gen9_hevc_enc_kernel_header HEVC_ENC_P_HME;
    gen9_hevc_enc_kernel_header HEVC_ENC_B_HME;
    gen9_hevc_enc_kernel_header HEVC_ENC_I_COARSE;
    gen9_hevc_enc_kernel_header HEVC_ENC_BRC_Init;
    gen9_hevc_enc_kernel_header HEVC_ENC_BRC_Reset;
    gen9_hevc_enc_kernel_header HEVC_ENC_BRC_Update;
    gen9_hevc_enc_kernel_header HEVC_ENC_BRC_LCU_Update;
    gen9_hevc_enc_kernel_header HEVC_ENC_PB_Pak;
    gen9_hevc_enc_kernel_header HEVC_ENC_PB_Widi;
    gen9_hevc_enc_kernel_header HEVC_ENC_BRC_Blockcopy;
    gen9_hevc_enc_kernel_header HEVC_ENC_DS_Combined;
    gen9_hevc_enc_kernel_header HEVC_ENC_P_MB;
    gen9_hevc_enc_kernel_header HEVC_ENC_P_Widi;
} gen9_hevc_enc_kernels_header_bxt;

typedef enum _GEN9_HEVC_ENC_MBENC_KRNIDX {
    GEN9_HEVC_ENC_MBENC_2xSCALING = 0,
    GEN9_HEVC_ENC_MBENC_32x32MD,
    GEN9_HEVC_ENC_MBENC_16x16SAD,
    GEN9_HEVC_ENC_MBENC_16x16MD,
    GEN9_HEVC_ENC_MBENC_8x8PU,
    GEN9_HEVC_ENC_MBENC_8x8FMODE,
    GEN9_HEVC_ENC_MBENC_32x32INTRACHECK,
    GEN9_HEVC_ENC_MBENC_BENC,
    GEN9_HEVC_ENC_MBENC_BPAK,
    GEN9_HEVC_ENC_MBENC_WIDI,
    GEN9_HEVC_ENC_MBENC_NUM,
    GEN9_HEVC_ENC_MBENC_DS_COMBINED = GEN9_HEVC_ENC_MBENC_NUM,
    GEN95_HEVC_ENC_MBENC_NUM_KBL,
    GEN9_HEVC_MBENC_PENC = GEN95_HEVC_ENC_MBENC_NUM_KBL,
    GEN9_HEVC_MBENC_P_WIDI,
    GEN9_HEVC_MBENC_NUM_BXT,
    GEN8_HEVC_ENC_MBENC_TOTAL_NUM = GEN9_HEVC_MBENC_NUM_BXT
} GEN9_HEVC_ENC_MBENC_KRNIDX;

typedef enum _GEN9_HEVC_ENC_BRC_KRNIDX {
    GEN9_HEVC_ENC_BRC_COARSE_INTRA = 0,
    GEN9_HEVC_ENC_BRC_INIT,
    GEN9_HEVC_ENC_BRC_RESET,
    GEN9_HEVC_ENC_BRC_FRAME_UPDATE,
    GEN9_HEVC_ENC_BRC_LCU_UPDATE,
    GEN9_HEVC_ENC_BRC_NUM
} GEN9_HEVC_ENC_BRC_KRNIDX;

typedef enum _GEN9_ENC_ENC_OPERATION {
    GEN9_ENC_SCALING4X = 0,
    GEN9_ENC_SCALING2X,
    GEN9_ENC_ME,
    GEN9_ENC_BRC,
    GEN9_ENC_MBENC,
    GEN9_ENC_MBENC_WIDI,
    GEN9_ENC_RESETVLINESTRIDE,
    GEN9_ENC_MC,
    GEN9_ENC_MBPAK,
    GEN9_ENC_DEBLOCK,
    GEN9_ENC_PREPROC,
    GEN9_ENC_VDENC_ME,
    GEN9_ENC_WP,
    GEN9_ENC_SFD,
    GEN9_ENC_MBENC_I_LUMA,
    GEN9_ENC_MPU,
    GEN9_ENC_TPU,
    GEN9_ENC_SCALING_CONVERSION,
    GEN9_ENC_DYS
} GEN9_ENC_OPERATION;

#endif
