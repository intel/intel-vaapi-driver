/*
 * Copyright © 2015 Intel Corporation
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

#ifndef GEN9_VDENC_H
#define GEN9_VDENC_H

#include <drm.h>
#include <i915_drm.h>
#include <intel_bufmgr.h>

#include "i965_gpe_utils.h"
#include "i965_encoder.h"

struct encode_state;

#define HUC_BRC_INIT_RESET                      4
#define HUC_BRC_UPDATE                          5

#define HUC_DMEM_DATA_OFFSET                    0x2000

#define NUM_OF_BRC_PAK_PASSES                   2

#define HUC_BRC_HISTORY_BUFFER_SIZE             832
#define HUC_BRC_STREAM_INOUT_BUFFER_SIZE        4096
#define VDENC_STATISTICS_SIZE                   128
#define PAK_STATISTICS_SIZE                     256
#define VDENC_AVC_IMAGE_STATE_SIZE              (sizeof(struct gen9_mfx_avc_img_state) + sizeof(struct gen9_vdenc_img_state) + 2 * sizeof(int))
#define HME_DETECTION_SUMMARY_BUFFER_SIZE       256
#define BRC_CONSTANT_DATA_SIZE                  4096
#define BRC_DEBUG_OUTPUT_SIZE                   4096

#define HUC_STATUS_MMIO_OFFSET                  0x0D000

#define SCALE_FACTOR_4X                         4

#define VDENC_FRAME_I                           0
#define VDENC_FRAME_P                           1

#define VDENC_LUTMODE_INTRA_NONPRED             0x00
#define VDENC_LUTMODE_INTRA                     0x01
#define VDENC_LUTMODE_INTRA_16x16               0x01
#define VDENC_LUTMODE_INTRA_8x8                 0x02
#define VDENC_LUTMODE_INTRA_4x4                 0x03
#define VDENC_LUTMODE_INTER_16x8                0x04
#define VDENC_LUTMODE_INTER_8x16                0x04
#define VDENC_LUTMODE_INTER_8X8Q                0x05
#define VDENC_LUTMODE_INTER_8X4Q                0x06
#define VDENC_LUTMODE_INTER_4X8Q                0x06
#define VDENC_LUTMODE_INTER_16x8_FIELD          0x06
#define VDENC_LUTMODE_INTER_4X4Q                0x07
#define VDENC_LUTMODE_INTER_8x8_FIELD           0x07
#define VDENC_LUTMODE_INTER                     0x08
#define VDENC_LUTMODE_INTER_16x16               0x08
#define VDENC_LUTMODE_INTER_BWD                 0x09
#define VDENC_LUTMODE_REF_ID                    0x0A
#define VDENC_LUTMODE_INTRA_CHROMA              0x0B

struct gen9_mfx_avc_img_state {
    union {
        struct {
            uint32_t dword_length: 16;
            uint32_t sub_opcode_b: 5;
            uint32_t sub_opcode_a: 3;
            uint32_t command_opcode: 3;
            uint32_t pipeline: 2;
            uint32_t command_type: 3;
        };

        uint32_t value;
    } dw0;

    struct {
        uint32_t frame_size_in_mbs_minus1: 16;
        uint32_t pad0: 16;
    } dw1;

    struct {
        uint32_t frame_width_in_mbs_minus1: 8;
        uint32_t pad0: 8;
        uint32_t frame_height_in_mbs_minus1: 8;
        uint32_t pad1: 8;
    } dw2;

    struct {
        uint32_t pad0: 8;
        uint32_t image_structure: 2;
        uint32_t weighted_bipred_idc: 2;
        uint32_t weighted_pred_flag: 1;
        uint32_t brc_domain_rate_control_enable: 1;
        uint32_t pad1: 2;
        uint32_t chroma_qp_offset: 5;
        uint32_t pad2: 3;
        uint32_t second_chroma_qp_offset: 5;
        uint32_t pad3: 3;
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
        uint32_t inter_mb_zero_cbp_flag: 1;
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
        uint32_t pad0;
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
        uint32_t pad0: 18;
        uint32_t vad_error_logic: 1;
        uint32_t pad1: 13;
    } dw12;

    struct {
        uint32_t pic_qp_init_minus26: 8;
        uint32_t pic_num_ref_idx_l0_active_minus1: 6;
        uint32_t pad0: 2;
        uint32_t pic_num_ref_idx_l1_active_minus1: 6;
        uint32_t pad1: 2;
        uint32_t num_ref_frames: 5;
        uint32_t is_curr_pic_has_mmco5: 1;
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

    struct {
        uint32_t fqp: 3;                        // Must be zero for SKL
        uint32_t fqp_offset: 3;                 // Must be zero for SKL
        uint32_t pad0: 2;
        uint32_t ext_brc_dm_stat_en: 1;         // Must be zero for SKL
        uint32_t pad1: 7;
        uint32_t brc_dm_avg_mb_qp: 6;           // Must be zero for SKL
        uint32_t pad2: 10;
    } dw17;

    struct {
        uint32_t brc_domain_target_frame_size;
    } dw18;

    struct {
        uint32_t threshold_size_in_bytes;
    } dw19;

    struct {
        uint32_t target_slice_size_in_bytes;
    } dw20;
};

struct gen9_image_state_cost {
    struct {
        uint32_t mv0_cost: 8;
        uint32_t mv1_cost: 8;
        uint32_t mv2_cost: 8;
        uint32_t mv3_cost: 8;
    } dw0;

    struct {
        uint32_t mv4_cost: 8;
        uint32_t mv5_cost: 8;
        uint32_t mv6_cost: 8;
        uint32_t mv7_cost: 8;
    } dw1;
};

struct gen9_vdenc_img_state {
    union {
        struct {
            uint32_t dword_length: 12;
            uint32_t pad0: 4;
            uint32_t sub_opcode_b: 5;
            uint32_t sub_opcode_a: 2;
            uint32_t command_opcode: 4;
            uint32_t pipeline: 2;
            uint32_t command_type: 3;
        };

        uint32_t value;
    } dw0;

    struct {
        uint32_t pad0: 2;
        uint32_t bidirectional_mix_disable: 1;
        uint32_t pad1: 1;
        uint32_t time_budget_overflow_check: 1;
        uint32_t pad2: 1;
        uint32_t extended_pak_obj_cmd_enable: 1;
        uint32_t transform_8x8_flag: 1;
        uint32_t vdenc_l1_cache_priority: 2;
        uint32_t pad3: 22;
    } dw1;

    struct {
        uint32_t pad0: 16;
        uint32_t bidirectional_weight: 6;
        uint32_t pad1: 6;
        uint32_t unidirection_mix_disable: 1;
        uint32_t pad2: 3;
    } dw2;

    struct {
        uint32_t pad0: 16;
        uint32_t picture_width: 16;
    } dw3;

    struct {
        uint32_t pad0: 12;
        uint32_t subpel_mode: 2;
        uint32_t pad1: 3;
        uint32_t forward_transform_skip_check_enable: 1;
        uint32_t bme_disable_for_fbr_message: 1;
        uint32_t block_based_skip_enabled: 1;
        uint32_t inter_sad_measure_adjustment: 2;
        uint32_t intra_sad_measure_adjustment: 2;
        uint32_t sub_macroblock_sub_partition_mask: 7;
        uint32_t block_based_skip_type: 1;
    } dw4;

    struct {
        uint32_t picture_height_minus1: 16;
        uint32_t cre_prefetch_enable: 1;
        uint32_t hme_ref1_disable: 1;
        uint32_t mb_slice_threshold_value: 4;
        uint32_t pad0: 4;
        uint32_t constrained_intra_prediction_flag: 1;
        uint32_t pad1: 2;
        uint32_t picture_type: 2;
        uint32_t pad2: 1;
    } dw5;

    struct {
        uint32_t slice_macroblock_height_minus1: 16;
        uint32_t pad0: 16;
    } dw6;

    struct {
        uint32_t pad0;
    } dw7;

    struct {
        uint32_t luma_intra_partition_mask: 5;
        uint32_t non_skip_zero_mv_const_added: 1;
        uint32_t non_skip_mb_mode_const_added: 1;
        uint32_t pad0: 9;
        uint32_t mv_cost_scaling_factor: 2;
        uint32_t bilinear_filter_enable: 1;
        uint32_t pad1: 3;
        uint32_t ref_id_cost_mode_select: 1;
        uint32_t pad2: 9;
    } dw8;

    struct {
        uint32_t mode0_cost: 8;
        uint32_t mode1_cost: 8;
        uint32_t mode2_cost: 8;
        uint32_t mode3_cost: 8;
    } dw9;

    struct {
        uint32_t mode4_cost: 8;
        uint32_t mode5_cost: 8;
        uint32_t mode6_cost: 8;
        uint32_t mode7_cost: 8;
    } dw10;

    struct {
        uint32_t mode8_cost: 8;
        uint32_t mode9_cost: 8;
        uint32_t ref_id_cost: 8;
        uint32_t chroma_intra_mode_cost: 8;
    } dw11;

    struct {
        struct gen9_image_state_cost mv_cost;
    } dw12_13;

    struct {
        uint32_t qp_prime_y: 8;
        uint32_t pad0: 16;
        uint32_t target_size_in_word: 8;
    } dw14;

    struct {
        uint32_t pad0;
    } dw15;

    struct {
        uint32_t pad0;
    } dw16;

    struct {
        uint32_t avc_intra_4x4_mode_mask: 9;
        uint32_t pad0: 7;
        uint32_t avc_intra_8x8_mode_mask: 9;
        uint32_t pad1: 7;
    } dw17;

    struct {
        uint32_t avc_intra_16x16_mode_mask: 4;
        uint32_t avc_intra_chroma_mode_mask: 4;
        uint32_t intra_compute_type_intra_compute_type: 2;
        uint32_t pad0: 22;
    } dw18;

    struct {
        uint32_t pad0;
    } dw19;

    struct {
        uint32_t penalty_for_intra_16x16_non_dc_prediction: 8;
        uint32_t penalty_for_intra_8x8_non_dc_prediction: 8;
        uint32_t penalty_for_intra_4x4_non_dc_prediction: 8;
        uint32_t pad0: 8;
    } dw20;

    struct {
        uint32_t pad0;
    } dw21;

    struct {
        uint32_t panic_mode_mb_threadhold: 16;
        uint32_t small_mb_size_in_word: 8;
        uint32_t large_mb_size_in_word: 8;
    } dw22;

    struct {
        uint32_t l0_number_of_reference_minus1: 8;
        uint32_t pad0: 8;
        uint32_t l1_number_of_reference_minus1: 8;
        uint32_t pad1: 8;
    } dw23;

    struct {
        uint32_t pad0;
    } dw24;

    struct {
        uint32_t pad0;
    } dw25;

    struct {
        uint32_t pad0: 8;
        uint32_t hme_ref_windows_combining_threshold: 8;
        uint32_t pad1: 16;
    } dw26;

    struct {
        uint32_t max_hmv_r: 16;
        uint32_t max_vmv_r: 16;
    } dw27;

    struct {
        struct gen9_image_state_cost hme_mv_cost;
    } dw28_29;

    struct {
        uint32_t roi_qp_adjustment_for_zone0: 4;
        uint32_t roi_qp_adjustment_for_zone1: 4;
        uint32_t roi_qp_adjustment_for_zone2: 4;
        uint32_t roi_qp_adjustment_for_zone3: 4;
        uint32_t qp_adjustment_for_shape_best_intra_4x4_winner: 4;
        uint32_t qp_adjustment_for_shape_best_intra_8x8_winner: 4;
        uint32_t qp_adjustment_for_shape_best_intra_16x16_winner: 4;
        uint32_t pad0: 4;
    } dw30;

    struct {
        uint32_t best_distortion_qp_adjustment_for_zone0: 4;
        uint32_t best_distortion_qp_adjustment_for_zone1: 4;
        uint32_t best_distortion_qp_adjustment_for_zone2: 4;
        uint32_t best_distortion_qp_adjustment_for_zone3: 4;
        uint32_t offset0_for_zone0_neg_zone1_boundary: 16;
    } dw31;

    struct {
        uint32_t offset1_for_zone1_neg_zone2_boundary: 16;
        uint32_t offset2_for_zone2_neg_zone3_boundary: 16;
    } dw32;

    struct {
        uint32_t qp_range_check_upper_bound: 8;
        uint32_t qp_range_check_lower_bound: 8;
        uint32_t pad0: 8;
        uint32_t qp_range_check_value: 4;
        uint32_t pad1: 4;
    } dw33;

    struct {
        uint32_t roi_enable: 1;
        uint32_t fwd_predictor0_mv_enable: 1;
        uint32_t bdw_predictor1_mv_enable: 1;
        uint32_t mb_level_qp_enable: 1;
        uint32_t target_size_in_words_mb_max_size_in_words_mb_enable: 1;
        uint32_t pad0: 3;
        uint32_t ppmv_disable: 1;
        uint32_t coefficient_clamp_enable: 1;
        uint32_t long_term_reference_frame_bwd_ref0_indicator: 1;
        uint32_t long_term_reference_frame_fwd_ref2_indicator: 1;
        uint32_t long_term_reference_frame_fwd_ref1_indicator: 1;
        uint32_t long_term_reference_frame_fwd_ref0_indicator: 1;
        uint32_t image_state_qp_override: 1;
        uint32_t pad1: 1;
        uint32_t midpoint_distortion: 16;
    } dw34;
};

struct gen9_vdenc_streamin_state {
    struct {
        uint32_t roi_selection: 8;
        uint32_t force_intra: 1;
        uint32_t force_skip: 1;
        uint32_t pad0: 22;
    } dw0;

    struct {
        uint32_t qp_prime_y: 8;
        uint32_t target_size_in_word: 8;
        uint32_t max_size_in_word: 8;
        uint32_t pad0: 8;
    } dw1;

    struct {
        uint32_t fwd_predictor_x: 16;
        uint32_t fwd_predictor_y: 16;
    } dw2;

    struct {
        uint32_t bwd_predictore_x: 16;
        uint32_t bwd_predictore_y: 16;
    } dw3;

    struct {
        uint32_t fwd_ref_id0: 4;
        uint32_t bdw_ref_id0: 4;
        uint32_t pad0: 24;
    } dw4;

    struct {
        uint32_t pad0[11];
    } dw5_15;
};

struct huc_brc_update_constant_data {
    uint8_t global_rate_qp_adj_tab_i[64];
    uint8_t global_rate_qp_adj_tab_p[64];
    uint8_t global_rate_qp_adj_tab_b[64];
    uint8_t dist_threshld_i[10];
    uint8_t dist_threshld_p[10];
    uint8_t dist_threshld_b[10];
    uint8_t dist_qp_adj_tab_i[81];
    uint8_t dist_qp_adj_tab_p[81];
    uint8_t dist_qp_adj_tab_b[81];
    int8_t  buf_rate_adj_tab_i[72];
    int8_t  buf_rate_adj_tab_p[72];
    int8_t  buf_rate_adj_tab_b[72];
    uint8_t frame_size_min_tab_p[9];
    uint8_t frame_size_min_tab_b[9];
    uint8_t frame_size_min_tab_i[9];
    uint8_t frame_size_max_tab_p[9];
    uint8_t frame_size_max_tab_b[9];
    uint8_t frame_size_max_tab_i[9];
    uint8_t frame_size_scg_tab_p[9];
    uint8_t frame_size_scg_tab_b[9];
    uint8_t frame_size_scg_tab_i[9];
    /* cost table 14*42 = 588 bytes */
    uint8_t i_intra_non_pred[42];
    uint8_t i_intra_16x16[42];
    uint8_t i_intra_8x8[42];
    uint8_t i_intra_4x4[42];
    uint8_t i_intra_chroma[42];
    uint8_t p_intra_non_pred[42];
    uint8_t p_intra_16x16[42];
    uint8_t p_intra_8x8[42];
    uint8_t p_intra_4x4[42];
    uint8_t p_intra_chroma[42];
    uint8_t p_inter_16x8[42];
    uint8_t p_inter_8x8[42];
    uint8_t p_inter_16x16[42];
    uint8_t p_ref_id[42];
    uint8_t hme_mv_cost[8][42];
    uint8_t pad0[42];
};

struct huc_brc_init_dmem {
    uint8_t     brc_func;                       // 0: Init; 2: Reset
    uint8_t     os_enabled;                     // Always 1
    uint8_t     pad0[2];
    uint16_t    brc_flag;                       // ICQ or CQP with slice size control: 0x00 CBR: 0x10; VBR: 0x20; VCM: 0x40; LOWDELAY: 0x80.
    uint16_t    pad1;
    uint16_t    frame_width;                    // Luma width in bytes
    uint16_t    frame_height;                   // Luma height in bytes
    uint32_t    target_bitrate;                 // target bitrate, set by application
    uint32_t    min_rate;                       // 0
    uint32_t    max_rate;                       // Maximum bit rate in bits per second (bps).
    uint32_t    buffer_size;                    // buffer size in bits
    uint32_t    init_buffer_fullness;           // initial buffer fullness in bits
    uint32_t    profile_level_max_frame;        // user defined. refer to AVC BRC HLD for conformance check and correction
    uint32_t    frame_rate_m;                   // FrameRateM is the number of frames in FrameRateD
    uint32_t    frame_rate_d;                   // If driver gets this FrameRateD from VUI, it is the num_units_in_tick field (32 bits UINT).
    uint16_t    num_p_in_gop;                   // number of P frames in a GOP
    uint16_t    num_b_in_gop;                   // number of B frames in a GOP
    uint16_t    min_qp;                         // 10
    uint16_t    max_qp;                         // 51
    int8_t      dev_thresh_pb0[8];              // lowdelay ? (-45, -33, -23, -15, -8, 0, 15, 25) : (-46, -38, -30, -23, 23, 30, 40, 46)
    int8_t      dev_thresh_vbr0[8];             // lowdelay ? (-45, -35, -25, -15, -8, 0, 20, 40) : (-46, -40, -32, -23, 56, 64, 83, 93)
    int8_t      dev_thresh_i0[8];               // lowdelay ? (-40, -30, -17, -10, -5, 0, 10, 20) : (-43, -36, -25, -18, 18, 28, 38, 46)
    uint8_t     init_qp_ip;                     // Initial QP for I and P

    uint8_t     pad2;                           // Reserved
    uint8_t     init_qp_b;                      // Initial QP for B
    uint8_t     mb_qp_ctrl;                     // Enable MB level QP control (global)
    uint8_t     slice_size_ctrl_en;             // Enable slice size control
    int8_t      intra_qp_delta[3];              // set to zero for all by default
    int8_t      skip_qp_delta;                  // Reserved
    int8_t      dist_qp_delta[4];               // lowdelay ? (-5, -2, 2, 5) : (0, 0, 0, 0)
    uint8_t     oscillation_qp_delta;           // BRCFLAG_ISVCM ? 16 : 0
    uint8_t     first_iframe_no_hrd_check;      // BRCFLAG_ISVCM ? 1 : 0
    uint8_t     skip_frame_enable_flag;
    uint8_t     top_qp_delta_thr_for_2nd_pass;  // =1. QP Delta threshold for second pass.
    uint8_t     top_frame_size_threshold_for_2nd_pass;          // lowdelay ? 10 : 50. Top frame size threshold for second pass
    uint8_t     bottom_frame_size_threshold_for_2nd_pass;       // lowdelay ? 10 : 200. Bottom frame size threshold for second pass
    uint8_t     qp_select_for_first_pass;       // lowdelay ? 0 : 1. =0 to use previous frame final QP; or =1 to use (targetQP + previousQP) / 2.
    uint8_t     mb_header_compensation;         // Reserved
    uint8_t     over_shoot_carry_flag;          // set to zero by default
    uint8_t     over_shoot_skip_frame_pct;      // set to zero by default
    uint8_t     estrate_thresh_p0[7];           // 4, 8, 12, 16, 20, 24, 28
    uint8_t     estrate_thresh_b0[7];           // 4, 8, 12, 16, 20, 24, 28
    uint8_t     estrate_thresh_i0[7];           // 4, 8, 12, 16, 20, 24, 28
    uint8_t     fqp_enable;                     // ExtendedBrcDomainEn
    uint8_t     scenario_info;                  // 0: UNKNOWN, 1: DISPLAYREMOTING, 2: VIDEOCONFERENCE, 3: ARCHIVE, 4: LIVESTREAMING.
    uint8_t     static_Region_streamin;         // should be programmed from par file
    uint8_t     delta_qp_adaptation;            // =1,
    uint8_t     max_crf_quality_factor;         // =52,
    uint8_t     crf_quality_factor;             // =25,
    uint8_t     bottom_qp_delta_thr_for_2nd_pass;// =1. QP Delta threshold for second pass.
    uint8_t     sliding_window_size;            // =30, the window size (in frames) used to compute bit rate
    uint8_t     sliding_widow_rc_enable;        // =0, sliding window based rate control (SWRC) disabled, 1: enabled
    uint8_t     sliding_window_max_rate_ratio;  // =120, ratio between the max rate within the window and average target bitrate
    uint8_t     low_delay_golden_frame_boost;   // only for lowdelay mode, 0 (default): no boost for I and scene change frames, 1: boost
    uint8_t     pad3[61];                       // Must be zero
};

struct huc_brc_update_dmem {
    uint8_t     brc_func;                       // =1 for Update, other values are reserved for future use
    uint8_t     pad0[3];
    uint32_t    target_size;                    // refer to AVC BRC HLD for calculation
    uint32_t    frame_number;                   // frame number
    uint32_t    peak_tx_bits_per_frame;         // current global target bits - previous global target bits (global target bits += input bits per frame)
    uint32_t    frame_budget;                   // target time counter
    uint32_t    frame_byte_count;               // PAK output via MMIO
    uint32_t    timing_budget_overflow;         // PAK output via MMIO
    uint32_t    slice_size_violation;           // PAK output via MMIO
    uint32_t    ipcm_non_conformant;            // PAK output via MMIO

    uint16_t    start_global_adjust_frame[4];   // 10, 50, 100, 150
    uint16_t    mb_budget[52];                  // MB bugdet for QP 0 - 51.
    uint16_t    target_slice_size;              // target slice size
    uint16_t    slcsz_thr_deltai[42];           // slice size threshold delta for I frame
    uint16_t    slcsz_thr_deltap[42];           // slice size threshold delta for P frame
    uint16_t    num_of_frames_skipped;          // Recording how many frames have been skipped.
    uint16_t    skip_frame_size;                // Recording the skip frame size for one frame. =NumMBs * 1, assuming one bit per mb for skip frame.
    uint16_t    static_region_pct;              // One entry, recording the percentage of static region
    uint8_t     global_rate_ratio_threshold[7]; // 80,95,99,101,105,125,160
    uint8_t     current_frame_type;             // I frame: 2; P frame: 0; B frame: 1.
    uint8_t     start_global_adjust_mult[5];    // 1, 1, 3, 2, 1
    uint8_t     start_global_adjust_div[5];     // 40, 5, 5, 3, 1
    uint8_t     global_rate_ratio_threshold_qp[8];      // 253,254,255,0,1,1,2,3
    uint8_t     current_pak_pass;               // current pak pass number
    uint8_t     max_num_passes;                 // 2
    uint8_t     scene_change_width[2];          // set both to MIN((NumP + 1) / 5, 6)
    uint8_t     scene_change_detect_enable;                     // Enable scene change detection
    uint8_t     scene_change_prev_intra_percent_threshold;      // =96. scene change previous intra percentage threshold
    uint8_t     scene_change_cur_intra_perent_threshold;        // =192. scene change current intra percentage threshold
    uint8_t     ip_average_coeff;               // lowdelay ? 0 : 128
    uint8_t     min_qp_adjustment;              // Minimum QP increase step
    uint8_t     timing_budget_check;            // Flag indicating if kernel will check timing budget.
    int8_t      roi_qp_delta_i8[4];             // Application specified ROI QP Adjustment for Zone0, Zone1, Zone2 and Zone3.
    uint8_t     cqp_qp_value;                   // Application specified target QP in BRC_ICQ mode
    uint8_t     cqp_fqp;                        // Application specified fine position in BRC_ICQ mode
    uint8_t     hme_detection_enable;           // 0: default, 1: HuC BRC kernel requires information from HME detection kernel output
    uint8_t     hme_cost_enable;                // 0: default, 1: driver provides HME cost table
    uint8_t     disable_pframe_8x8_transform;
    uint8_t     skl_cabac_wa_enable;
    uint8_t     roi_source;                     // =0: disable, 1: ROIMap from HME Static Region or from App dirty rectangle, 2: ROIMap from App
    uint8_t     slice_size_consertative_threshold;      // =0, 0: do not set conservative threshold (suggested for video conference) 1: set conservative threshold for non-video conference
    uint16_t    max_target_slice_size;          // default: 1498, max target slice size from app DDI
    uint16_t    max_num_slice_allowed;          // computed by driver based on level idc
    uint16_t    second_level_batchbuffer_size;  // second level batch buffer (SLBB) size in bytes, the input buffer will contain two SLBBs A and B, A followed by B, A and B have the same structure.
    uint16_t    second_level_batchbuffer_b_offset;      // offset in bytes from the beginning of the input buffer, it points to the start of SLBB B, set by driver for skip frame support
    uint16_t    avc_img_state_offset;           // offset in bytes from the beginning of SLBB A

    /* HME distortion based QP adjustment */
    uint16_t    ave_hme_dist;
    uint8_t     hme_dist_available;             // 0: disabled, 1: enabled

    uint8_t     pad1[63];
};

struct gen9_vdenc_status {
    uint32_t    bytes_per_frame;
};

struct gen9_vdenc_context {
    uint32_t    frame_width_in_mbs;
    uint32_t    frame_height_in_mbs;
    uint32_t    frame_width;                    // frame_width_in_mbs * 16
    uint32_t    frame_height;                   // frame_height_in_mbs * 16
    uint32_t    down_scaled_width_in_mb4x;
    uint32_t    down_scaled_height_in_mb4x;
    uint32_t    down_scaled_width_4x;           // down_scaled_width_in_mb4x * 16
    uint32_t    down_scaled_height_4x;          // down_scaled_height_in_mbs * 16

    uint32_t    target_bit_rate;        /* in kbps */
    uint32_t    max_bit_rate;           /* in kbps */
    uint32_t    min_bit_rate;           /* in kbps */
    uint64_t    init_vbv_buffer_fullness_in_bit;
    uint64_t    vbv_buffer_size_in_bit;
    struct intel_fraction framerate;
    uint32_t    gop_size;
    uint32_t    ref_dist;
    double      brc_target_size;
    double      brc_init_current_target_buf_full_in_bits;
    double      brc_init_reset_input_bits_per_frame;
    uint32_t    brc_init_reset_buf_size_in_bits;
    uint32_t    brc_init_previous_target_buf_full_in_bits;

    uint8_t     mode_cost[12];
    uint8_t     mv_cost[8];
    uint8_t     hme_mv_cost[8];

    uint32_t    num_roi;
    uint32_t    max_delta_qp;
    uint32_t    min_delta_qp;
    struct intel_roi roi[3];

    uint32_t    brc_initted: 1;
    uint32_t    brc_need_reset: 1;
    uint32_t    is_low_delay: 1;
    uint32_t    brc_enabled: 1;
    uint32_t    internal_rate_mode: 4;
    uint32_t    current_pass: 4;
    uint32_t    num_passes: 4;
    uint32_t    is_first_pass: 1;
    uint32_t    is_last_pass: 1;

    uint32_t    vdenc_streamin_enable: 1;
    uint32_t    vdenc_pak_threshold_check_enable: 1;
    uint32_t    pad1: 1;
    uint32_t    transform_8x8_mode_enable: 1;
    uint32_t    frame_type: 2;

    uint32_t    mb_brc_enabled: 1;
    uint32_t    is_frame_level_vdenc: 1;
    uint32_t    use_extended_pak_obj_cmd: 1;
    uint32_t    pad0: 29;

    struct i965_gpe_resource brc_init_reset_dmem_res;
    struct i965_gpe_resource brc_history_buffer_res;
    struct i965_gpe_resource brc_stream_in_res;
    struct i965_gpe_resource brc_stream_out_res;
    struct i965_gpe_resource huc_dummy_res;

    struct i965_gpe_resource brc_update_dmem_res[NUM_OF_BRC_PAK_PASSES];
    struct i965_gpe_resource vdenc_statistics_res;
    struct i965_gpe_resource pak_statistics_res;
    struct i965_gpe_resource vdenc_avc_image_state_res;
    struct i965_gpe_resource hme_detection_summary_buffer_res;
    struct i965_gpe_resource brc_constant_data_res;
    struct i965_gpe_resource second_level_batch_res;

    struct i965_gpe_resource huc_status_res;
    struct i965_gpe_resource huc_status2_res;

    struct i965_gpe_resource recon_surface_res;
    struct i965_gpe_resource scaled_4x_recon_surface_res;
    struct i965_gpe_resource post_deblocking_output_res;
    struct i965_gpe_resource pre_deblocking_output_res;
    struct i965_gpe_resource list_reference_res[16];
    struct i965_gpe_resource list_scaled_4x_reference_res[16];
    struct i965_gpe_resource uncompressed_input_surface_res;                    // Input

    struct {
        struct i965_gpe_resource res;                                           // Output
        uint32_t start_offset;
        uint32_t end_offset;
    } compressed_bitstream;

    struct i965_gpe_resource mfx_intra_row_store_scratch_res;                   // MFX internal buffer
    struct i965_gpe_resource mfx_deblocking_filter_row_store_scratch_res;       // MFX internal buffer
    struct i965_gpe_resource mfx_bsd_mpc_row_store_scratch_res;                 // MFX internal buffer
    struct i965_gpe_resource vdenc_row_store_scratch_res;                       // VDENC internal buffer

    struct i965_gpe_resource vdenc_streamin_res;

    uint32_t    num_refs[2];
    uint32_t    list_ref_idx[2][32];

    struct {
        struct i965_gpe_resource res;
        uint32_t base_offset;
        uint32_t size;
        uint32_t bytes_per_frame_offset;
    } status_bffuer;
};

struct huc_pipe_mode_select_parameter {
    uint32_t    huc_stream_object_enable;
    uint32_t    indirect_stream_out_enable;
    uint32_t    media_soft_reset_counter;
};

struct huc_imem_state_parameter {
    uint32_t    huc_firmware_descriptor;
};

struct huc_dmem_state_parameter {
    struct i965_gpe_resource *huc_data_source_res;
    uint32_t    huc_data_destination_base_address;
    uint32_t    huc_data_length;
};

struct huc_cfg_state_parameter {
    uint32_t    force_reset;
};


struct huc_virtual_addr_parameter {
    struct {
        struct i965_gpe_resource *huc_surface_res;
        uint32_t is_target;
    } regions[16];
};

struct huc_ind_obj_base_addr_parameter {
    struct i965_gpe_resource *huc_indirect_stream_in_object_res;
    struct i965_gpe_resource *huc_indirect_stream_out_object_res;
};

struct huc_stream_object_parameter {
    uint32_t indirect_stream_in_data_length;
    uint32_t indirect_stream_in_start_address;
    uint32_t indirect_stream_out_start_address;
    uint32_t huc_bitstream_enable;
    uint32_t length_mode;
    uint32_t stream_out;
    uint32_t emulation_prevention_byte_removal;
    uint32_t start_code_search_engine;
    uint8_t start_code_byte2;
    uint8_t start_code_byte1;
    uint8_t start_code_byte0;
};

struct huc_start_parameter {
    uint32_t last_stream_object;
};

struct vd_pipeline_flush_parameter {
    uint32_t hevc_pipeline_done;
    uint32_t vdenc_pipeline_done;
    uint32_t mfl_pipeline_done;
    uint32_t mfx_pipeline_done;
    uint32_t vd_command_message_parser_done;
    uint32_t hevc_pipeline_command_flush;
    uint32_t vdenc_pipeline_command_flush;
    uint32_t mfl_pipeline_command_flush;
    uint32_t mfx_pipeline_command_flush;
};

extern Bool
gen9_vdenc_context_init(VADriverContextP ctx, struct intel_encoder_context *encoder_context);

#endif  /* GEN9_VDENC_H */
