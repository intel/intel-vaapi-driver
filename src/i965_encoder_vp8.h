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
 *    Xiang, Haihao <haihao.xiang@intel.com>
 *
 */

#ifndef I965_ENCODER_VP8_H
#define I965_ENCODER_VP8_H

#include <drm.h>
#include <i915_drm.h>
#include <intel_bufmgr.h>

#include "i965_gpe_utils.h"

#define VP8_ME_MV_DATA_SIZE_MULTIPLIER     3
#define VP8_HISTOGRAM_SIZE                 (136 * sizeof(unsigned int))
#define VP8_FRAME_HEADER_SIZE              4096
#define VP8_MODE_PROPABILITIES_SIZE        96

#define VP8_NUM_COEFF_PLANES               4
#define VP8_NUM_COEFF_BANDS                8
#define VP8_NUM_LOCAL_COMPLEXITIES         3
#define VP8_NUM_COEFF_NODES                11
#define VP8_COEFFS_PROPABILITIES_SIZE      (VP8_NUM_COEFF_PLANES * VP8_NUM_COEFF_BANDS * VP8_NUM_LOCAL_COMPLEXITIES * VP8_NUM_COEFF_NODES)

#define VP8_TOKEN_BITS_DATA_SIZE           (16 * sizeof(unsigned int))

#define VP8_HEADER_METADATA_SIZE           (32 * sizeof(unsigned int))
#define VP8_PICTURE_STATE_CMD_SIZE         (37 * sizeof(unsigned int))
#define VP8_PICTURE_STATE_SIZE             (VP8_PICTURE_STATE_CMD_SIZE + VP8_HEADER_METADATA_SIZE + (16 * sizeof(unsigned int)))

#define VP8_MPU_BITSTREAM_SIZE             128
#define VP8_TPU_BITSTREAM_SIZE             1344
#define VP8_ENTROPY_COST_TABLE_SIZE        (256 * sizeof(unsigned int))

#define VP8_TOKEN_STATISTICS_SIZE          (304 * sizeof(unsigned int))

#define VP8_INTERMEDIATE_PARTITION0_SIZE   (64 * 1024)

#define VP8_REPAK_DECISION_BUF_SIZE        (4 * sizeof(unsigned int))

#define VP8_MAX_SEGMENTS                   4

#define VP8_ENCODE_STATUS_NUM                  512

#define VP8_HEADER_METADATA_SIZE           (32  * sizeof(unsigned int))
#define VP8_PICTURE_STATE_CMD_SIZE         (37  * sizeof(unsigned int))
#define VP8_PICTURE_STATE_SIZE             (VP8_PICTURE_STATE_CMD_SIZE + VP8_HEADER_METADATA_SIZE + (16 * sizeof(unsigned int)))
#define VP8_HEADER_METADATA_OFFSET         (VP8_PICTURE_STATE_CMD_SIZE + (3 * sizeof(unsigned int)))

#define VDBOX0_MMIO_BASE                                        0x12000
#define VDBOX1_MMIO_BASE                                        0x1c000

#define VP8_MFC_IMAGE_STATUS_MASK_REG_OFFSET                    0x900
#define VP8_MFC_IMAGE_STATUS_CTRL_REG_OFFSET                    0x904
#define VP8_MFC_BITSTREAM_BYTECOUNT_FRAME_REG_OFFSET            0x908

#define VP8_MFX_BRC_DQ_INDEX_REG_OFFSET                         0x910
#define VP8_MFX_BRC_D_LOOP_FILTER_REG_OFFSET                    0x914
#define VP8_MFX_BRC_CUMULATIVE_DQ_INDEX01_REG_OFFSET            0x918
#define VP8_MFX_BRC_CUMULATIVE_DQ_INDEX23_REG_OFFSET            0x91C
#define VP8_MFX_BRC_CUMULATIVE_D_LOOP_FILTER01_REG_OFFSET       0x920
#define VP8_MFX_BRC_CUMULATIVE_D_LOOP_FILTER23_REG_OFFSET       0x924
#define VP8_MFX_BRC_CONVERGENCE_STATUS_REG_OFFSET               0x928

struct encode_state;
struct intel_encoder_context;

struct i965_encoder_vp8_surface {
    VADriverContextP ctx;
    VASurfaceID scaled_4x_surface_id;
    struct object_surface *scaled_4x_surface_obj;
    VASurfaceID scaled_16x_surface_id;
    struct object_surface *scaled_16x_surface_obj;
    unsigned int qp_index;
};

enum vp8_binding_table_offset_vp8_brc_init_reset {
    VP8_BTI_BRC_INIT_RESET_HISTORY = 0,
    VP8_BTI_BRC_INIT_RESET_DISTORTION,
    VP8_BTI_BRC_INIT_RESET_NUM_SURFACES
};

struct vp8_brc_init_reset_curbe_data {
    struct {
        uint32_t profile_level_max_frame;
    } dw0;

    struct {
        uint32_t init_buf_full_in_bits;
    } dw1;

    struct {
        uint32_t buf_size_in_bits;
    } dw2;

    struct {
        uint32_t average_bitrate;
    } dw3;

    struct {
        uint32_t max_bitrate;
    } dw4;

    struct {
        uint32_t min_bitrate;
    } dw5;

    struct {
        uint32_t frame_rate_m;
    } dw6;

    struct {
        uint32_t frame_rate_d;
    } dw7;

    struct {
        uint32_t brc_flag: 16;
        uint32_t gop_minus1: 16;
    } dw8;

    struct {
        uint32_t reserved: 16;
        uint32_t frame_width_in_bytes: 16;
    } dw9;

    struct {
        uint32_t frame_height_in_bytes: 16;
        uint32_t avbr_accuracy: 16;
    } dw10;

    struct {
        uint32_t avbr_convergence: 16;
        uint32_t min_qp: 16;
    } dw11;

    struct {
        uint32_t max_qp: 16;
        uint32_t level_qp: 16;
    } dw12;

    struct {
        uint32_t max_section_pct: 16;
        uint32_t under_shoot_cbr_pct: 16;
    } dw13;

    struct {
        uint32_t vbr_bias_pct: 16;
        uint32_t min_section_pct: 16;
    } dw14;

    struct {
        uint32_t instant_rate_threshold_0_for_p: 8;
        uint32_t instant_rate_threshold_1_for_p: 8;
        uint32_t instant_rate_threshold_2_for_p: 8;
        uint32_t instant_rate_threshold_3_for_p: 8;
    } dw15;

    struct {
        uint32_t reserved;
    } dw16;

    struct {
        uint32_t instant_rate_threshold_0_for_i: 8;
        uint32_t instant_rate_threshold_1_for_i: 8;
        uint32_t instant_rate_threshold_2_for_i: 8;
        uint32_t instant_rate_threshold_3_for_i: 8;
    } dw17;

    struct {
        uint32_t deviation_threshold_0_for_p: 8; // Signed byte
        uint32_t deviation_threshold_1_for_p: 8; // Signed byte
        uint32_t deviation_threshold_2_for_p: 8; // Signed byte
        uint32_t deviation_threshold_3_for_p: 8; // Signed byte
    } dw18;

    struct {
        uint32_t deviation_threshold_4_for_p: 8; // Signed byte
        uint32_t deviation_threshold_5_for_p: 8; // Signed byte
        uint32_t deviation_threshold_6_for_p: 8; // Signed byte
        uint32_t deviation_threshold_7_for_p: 8; // Signed byte
    } dw19;

    struct {
        uint32_t deviation_threshold_0_for_vbr: 8; // Signed byte
        uint32_t deviation_threshold_1_for_vbr: 8; // Signed byte
        uint32_t deviation_threshold_2_for_vbr: 8; // Signed byte
        uint32_t deviation_threshold_3_for_vbr: 8; // Signed byte
    } dw20;

    struct {
        uint32_t deviation_threshold_4_for_vbr: 8; // Signed byte
        uint32_t deviation_threshold_5_for_vbr: 8; // Signed byte
        uint32_t deviation_threshold_6_for_vbr: 8; // Signed byte
        uint32_t deviation_threshold_7_for_vbr: 8; // Signed byte
    } dw21;

    struct {
        uint32_t deviation_threshold_0_for_i: 8; // Signed byte
        uint32_t deviation_threshold_1_for_i: 8; // Signed byte
        uint32_t deviation_threshold_2_for_i: 8; // Signed byte
        uint32_t deviation_threshold_3_for_i: 8; // Signed byte
    } dw22;

    struct {
        uint32_t deviation_threshold_4_for_i: 8; // Signed byte
        uint32_t deviation_threshold_5_for_i: 8; // Signed byte
        uint32_t deviation_threshold_6_for_i: 8; // Signed byte
        uint32_t deviation_threshold_7_for_i: 8; // Signed byte
    } dw23;

    struct {
        uint32_t num_t_levels: 8;
        uint32_t reserved: 24;
    } dw24;

    struct {
        uint32_t reserved;
    } dw25;

    struct {
        uint32_t history_buffer_bti;
    } dw26;

    struct {
        uint32_t distortion_buffer_bti;
    } dw27;
};

#define VP8_BRC_INIT                    0
#define VP8_BRC_RESET                   1
#define NUM_VP8_BRC_RESET               2

struct i965_encoder_vp8_brc_init_reset_context {
    struct i965_gpe_context gpe_contexts[NUM_VP8_BRC_RESET];
};

struct scaling_curbe_parameters {
    unsigned int input_picture_width;
    unsigned int input_picture_height;
    char is_field_picture;
    char flatness_check_enabled;
    char mb_variance_output_enabled;
    char mb_pixel_average_output_enabled;
};

struct scaling_surface_parameters {
    struct object_surface *input_obj_surface;
    struct object_surface *output_obj_surface;
    unsigned int input_width;
    unsigned int input_height;
    unsigned int output_width;
    unsigned int output_height;
};

enum vp8_binding_table_offset_scaling {
    VP8_BTI_SCALING_FRAME_SRC_Y                 = 0,
    VP8_BTI_SCALING_FRAME_DST_Y                 = 1,
    VP8_BTI_SCALING_FIELD_TOP_SRC_Y             = 0,
    VP8_BTI_SCALING_FIELD_TOP_DST_Y             = 1,
    VP8_BTI_SCALING_FIELD_BOT_SRC_Y             = 2,
    VP8_BTI_SCALING_FIELD_BOT_DST_Y             = 3,
    VP8_BTI_SCALING_FRAME_FLATNESS_DST          = 4,
    VP8_BTI_SCALING_FIELD_TOP_FLATNESS_DST      = 4,
    VP8_BTI_SCALING_FIELD_BOT_FLATNESS_DST      = 5,
    VP8_BTI_SCALING_FRAME_MBVPROCSTATS_DST      = 6,
    VP8_BTI_SCALING_FIELD_TOP_MBVPROCSTATS_DST  = 6,
    VP8_BTI_SCALING_FIELD_BOT_MBVPROCSTATS_DST  = 7,
    VP8_BTI_SCALING_NUM_SURFACES                = 8
};

struct vp8_scaling_curbe_data {
    struct {
        uint32_t input_picture_width: 16;
        uint32_t input_picture_height: 16;
    } dw0;

    union {
        uint32_t input_y_bti_frame;
        uint32_t input_y_bti_top_field;
    } dw1;

    union {
        uint32_t output_y_bti_frame;
        uint32_t output_y_bti_top_field;
    } dw2;

    struct {
        uint32_t input_y_bti_bottom_field;
    } dw3;

    struct {
        uint32_t output_y_bti_bottom_field;
    } dw4;

    struct {
        uint32_t flatness_threshold;
    } dw5;

    struct {
        uint32_t enable_mb_flatness_check: 1;
        uint32_t enable_mb_variance_output: 1;
        uint32_t enable_mb_pixel_average_output: 1;
        uint32_t reserved: 29;
    } dw6;

    struct {
        uint32_t reserved;
    } dw7;

    union {
        uint32_t flatness_output_bti_frame;
        uint32_t flatness_output_bti_top_field;
    } dw8;

    struct {
        uint32_t flatness_output_bti_bottom_field;
    } dw9;

    union {
        uint32_t mbv_proc_stats_bti_frame;
        uint32_t mbv_proc_stats_bti_top_field;
    } dw10;

    struct {
        uint32_t mbv_proc_stats_bti_bottom_field;
    } dw11;
};

#define VP8_SCALING_4X                  0
#define VP8_SCALING_16X                 1
#define NUM_VP8_SCALING                 2

struct i965_encoder_vp8_scaling_context {
    struct i965_gpe_context gpe_contexts[NUM_VP8_SCALING];
};

enum VP8_ME_MODES {
    VP8_ME_MODE_ME16X_BEFORE_ME4X       = 0,
    VP8_ME_MODE_ME16X_ONLY              = 1,
    VP8_ME_MODE_ME4X_ONLY               = 2,
    VP8_ME_MODE_ME4X_AFTER_ME16X        = 3
};

struct me_curbe_parameters {
    unsigned int down_scaled_width_in_mbs;
    unsigned int down_scaled_height_in_mbs;
    int use_16x_me;
};

struct me_surface_parameters {
    int use_16x_me;
};

struct vp8_search_path_delta {
    char search_path_delta_x: 4;
    char search_path_delta_y: 4;
};

enum vp8_binding_table_offset_me {
    VP8_BTI_ME_MV_DATA                  = 0,
    VP8_BTI_16X_ME_MV_DATA              = 2,
    VP8_BTI_ME_DISTORTION               = 3,
    VP8_BTI_ME_MIN_DIST_BRC_DATA        = 4,
    VP8_BTI_VME_INTER_PRED              = 5,
    VP8_BTI_ME_REF1_PIC                 = 6,
    VP8_BTI_ME_REF2_PIC                 = 8,
    VP8_BTI_ME_REF3_PIC                 = 10,
    VP8_BTI_ME_NUM_SURFACES             = 11
};

struct vp8_me_curbe_data {
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
        uint32_t src_Access: 1;
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
        uint32_t me_mode: 2;
        uint32_t reserved1: 3;
        uint32_t super_combine_dist: 8;
        uint32_t max_vmv_range: 16;
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
        uint32_t l0_ref_pic_polarity_bits: 8;
        uint32_t l1_ref_pic_polarity_bits: 2;
        uint32_t reserved: 12;
    } dw14;

    struct {
        uint32_t reserved;
    } dw15;

    struct {
        struct vp8_search_path_delta sp_delta_0;
        struct vp8_search_path_delta sp_delta_1;
        struct vp8_search_path_delta sp_delta_2;
        struct vp8_search_path_delta sp_delta_3;
    } dw16;

    struct {
        struct vp8_search_path_delta sp_delta_4;
        struct vp8_search_path_delta sp_delta_5;
        struct vp8_search_path_delta sp_delta_6;
        struct vp8_search_path_delta sp_delta_7;
    } dw17;

    struct {
        struct vp8_search_path_delta sp_delta_8;
        struct vp8_search_path_delta sp_delta_9;
        struct vp8_search_path_delta sp_delta_10;
        struct vp8_search_path_delta sp_delta_11;
    } dw18;

    struct {
        struct vp8_search_path_delta sp_delta_12;
        struct vp8_search_path_delta sp_delta_13;
        struct vp8_search_path_delta sp_delta_14;
        struct vp8_search_path_delta sp_delta_15;
    } dw19;

    struct {
        struct vp8_search_path_delta sp_delta_16;
        struct vp8_search_path_delta sp_delta_17;
        struct vp8_search_path_delta sp_delta_18;
        struct vp8_search_path_delta sp_delta_19;
    } dw20;

    struct {
        struct vp8_search_path_delta sp_delta_20;
        struct vp8_search_path_delta sp_delta_21;
        struct vp8_search_path_delta sp_delta_22;
        struct vp8_search_path_delta sp_delta_23;
    } dw21;

    struct {
        struct vp8_search_path_delta sp_delta_24;
        struct vp8_search_path_delta sp_delta_25;
        struct vp8_search_path_delta sp_delta_26;
        struct vp8_search_path_delta sp_delta_27;
    } dw22;

    struct {
        struct vp8_search_path_delta sp_delta_28;
        struct vp8_search_path_delta sp_delta_29;
        struct vp8_search_path_delta sp_delta_30;
        struct vp8_search_path_delta sp_delta_31;
    } dw23;

    struct {
        struct vp8_search_path_delta sp_delta_32;
        struct vp8_search_path_delta sp_delta_33;
        struct vp8_search_path_delta sp_delta_34;
        struct vp8_search_path_delta sp_delta_35;
    } dw24;

    struct {
        struct vp8_search_path_delta sp_delta_36;
        struct vp8_search_path_delta sp_delta_37;
        struct vp8_search_path_delta sp_delta_38;
        struct vp8_search_path_delta sp_delta_39;
    } dw25;

    struct {
        struct vp8_search_path_delta sp_delta_40;
        struct vp8_search_path_delta sp_delta_41;
        struct vp8_search_path_delta sp_delta_42;
        struct vp8_search_path_delta sp_delta_43;
    } dw26;

    struct {
        struct vp8_search_path_delta sp_delta_44;
        struct vp8_search_path_delta sp_delta_45;
        struct vp8_search_path_delta sp_delta_46;
        struct vp8_search_path_delta sp_delta_47;
    } dw27;

    struct {
        struct vp8_search_path_delta sp_delta_48;
        struct vp8_search_path_delta sp_delta_49;
        struct vp8_search_path_delta sp_delta_50;
        struct vp8_search_path_delta sp_delta_51;
    } dw28;

    struct {
        struct vp8_search_path_delta sp_delta_52;
        struct vp8_search_path_delta sp_delta_53;
        struct vp8_search_path_delta sp_delta_54;
        struct vp8_search_path_delta sp_delta_55;
    } dw29;

    struct {
        uint32_t reserved0;
    } dw30;

    struct {
        uint32_t reserved0;
    } dw31;

    struct {
        uint32_t vp8_me_mv_output_data_bti;
    } dw32;

    struct {
        uint32_t vp8_me_mv_input_data_bti;
    } dw33;

    struct {
        uint32_t vp8_me_distorion_bti;
    } dw34;

    struct {
        uint32_t vp8_me_min_dist_brc_bti;
    } dw35;

    struct {
        uint32_t vp8_me_forward_ref_bti;
    } dw36;

    struct {
        uint32_t vp8_me_backward_ref_bti;
    } dw37;
};

#define VP8_ME_4X                       0
#define VP8_ME_16X                      1
#define NUM_VP8_ME                      2

struct i965_encoder_vp8_me_context {
    struct i965_gpe_context gpe_contexts[NUM_VP8_ME];
};

#define MAX_QP_VP8                      127
#define NUM_QP_VP8                      (MAX_QP_VP8 + 1)

#define DC_BIAS_SEGMENT_DEFAULT_VAL_VP8 1500

struct mbenc_surface_parameters {
    int i_frame_dist_in_use;
    struct i965_gpe_resource *me_brc_distortion_buffer;
};

enum vp8_binding_table_offset_mbenc {
    VP8_BTI_MBENC_PER_MB_OUT            = 0,
    VP8_BTI_MBENC_CURR_Y                = 1,
    VP8_BTI_MBENC_CURR_UV               = 2,

    VP8_BTI_MBENC_MB_MODE_COST_LUMA     = 3,
    VP8_BTI_MBENC_BLOCK_MODE_COST       = 4,
    VP8_BTI_MBENC_CHROMA_RECON          = 5,
    VP8_BTI_MBENC_SEGMENTATION_MAP      = 6,
    VP8_BTI_MBENC_HISTOGRAM             = 7,
    VP8_BTI_MBENC_I_VME_DEBUG_STREAMOUT = 8,
    VP8_BTI_MBENC_VME                   = 9,
    VP8_BTI_MBENC_IDIST                 = 10,
    VP8_BTI_MBENC_CURR_Y_DOWNSCALED     = 11,
    VP8_BTI_MBENC_VME_COARSE_INTRA      = 12,

    VP8_BTI_MBENC_MV_DATA_FROM_ME       = 3,
    VP8_BTI_MBENC_IND_MV_DATA           = 4,
    VP8_BTI_MBENC_REF_MB_COUNT          = 5,
    VP8_BTI_MBENC_INTER_PRED            = 8,
    VP8_BTI_MBENC_REF1_PIC              = 9,
    VP8_BTI_MBENC_REF2_PIC              = 11,
    VP8_BTI_MBENC_REF3_PIC              = 13,
    VP8_BTI_MBENC_P_PER_MB_QUANT        = 14,
    VP8_BTI_MBENC_INTER_PRED_DISTORTION = 15,
    VP8_BTI_MBENC_PRED_MV_DATA          = 16,
    VP8_BTI_MBENC_MODE_COST_UPDATE      = 17,
    VP8_BTI_MBENC_P_VME_DEBUG_STREAMOUT = 18,
    VP8_BTI_MBENC_NUM_SURFACES          = 19
};

struct vp8_mbenc_i_frame_curbe_data {
    struct {
        uint32_t frame_width: 16;
        uint32_t frame_height: 16;
    } dw0;

    struct {
        uint32_t frame_type: 1;
        uint32_t enable_segmentation: 1;
        uint32_t enable_hw_intra_prediction: 1;
        uint32_t enable_debug_dumps: 1;
        uint32_t enable_coeff_clamp: 1;
        uint32_t enable_chroma_ip_enhancement: 1;
        uint32_t enable_mpu_histogram_update: 1;
        uint32_t reserved0: 1;
        uint32_t vme_enable_tm_check: 1;
        uint32_t vme_distortion_measure: 2;
        uint32_t reserved1: 21;
    } dw1;

    struct {
        uint32_t lambda_seg_0: 16;
        uint32_t lambda_seg_1: 16;
    } dw2;

    struct {
        uint32_t lambda_seg_2: 16;
        uint32_t lambda_seg_3: 16;
    } dw3;

    struct {
        uint32_t all_dc_bias_segment_0: 16;
        uint32_t all_dc_bias_segment_1: 16;
    } dw4;

    struct {
        uint32_t all_dc_bias_segment_2: 16;
        uint32_t all_dc_bias_segment_3: 16;
    } dw5;

    struct {
        uint32_t chroma_dc_de_quant_segment_0: 16;
        uint32_t chroma_dc_de_quant_segment_1: 16;
    } dw6;

    struct {
        uint32_t chroma_dc_de_quant_segment_2: 16;
        uint32_t chroma_dc_de_quant_segment_3: 16;
    } dw7;

    struct {
        uint32_t chroma_ac_de_quant_segment0: 16;
        uint32_t chroma_ac_de_quant_segment1: 16;
    } dw8;

    struct {
        uint32_t chroma_ac_de_quant_segment2: 16;
        uint32_t chroma_ac_de_quant_segment3: 16;
    } dw9;

    struct {
        uint32_t chroma_ac0_threshold0_segment0: 16;
        uint32_t chroma_ac0_threshold1_segment0: 16;
    } dw10;

    struct {
        uint32_t chroma_ac0_threshold0_segment1: 16;
        uint32_t chroma_ac0_threshold1_segment1: 16;
    } dw11;

    struct {
        uint32_t chroma_ac0_threshold0_segment2: 16;
        uint32_t chroma_ac0_threshold1_segment2: 16;
    } dw12;

    struct {
        uint32_t chroma_ac0_threshold0_segment3: 16;
        uint32_t chroma_ac0_threshold1_segment3: 16;
    } dw13;

    struct {
        uint32_t chroma_dc_threshold0_segment0: 16;
        uint32_t chroma_dc_threshold1_segment0: 16;
    } dw14;

    struct {
        uint32_t chroma_dc_threshold2_segment0: 16;
        uint32_t chroma_dc_threshold3_segment0: 16;
    } dw15;

    struct {
        uint32_t chroma_dc_threshold0_segment1: 16;
        uint32_t chroma_dc_threshold1_segment1: 16;
    } dw16;

    struct {
        uint32_t chroma_dc_threshold2_segment1: 16;
        uint32_t chroma_dc_threshold3_segment1: 16;
    } dw17;

    struct {
        uint32_t chroma_dc_threshold0_segment2: 16;
        uint32_t chroma_dc_threshold1_segment2: 16;
    } dw18;

    struct {
        uint32_t chroma_dc_threshold2_segment2: 16;
        uint32_t chroma_dc_threshold3_segment2: 16;
    } dw19;

    struct {
        uint32_t chroma_dc_threshold0_segment3: 16;
        uint32_t chroma_dc_threshold1_segment3: 16;
    } dw20;

    struct {
        uint32_t chroma_dc_threshold2_segment3: 16;
        uint32_t chroma_dc_threshold3_segment3: 16;
    } dw21;

    struct {
        uint32_t chroma_ac1_threshold_segment0: 16;
        uint32_t chroma_ac1_threshold_segment1: 16;
    } dw22;

    struct {
        uint32_t chroma_ac1_threshold_segment2: 16;
        uint32_t chroma_ac1_threshold_segment3: 16;
    } dw23;

    struct {
        uint32_t vme_16x16_cost_segment0: 8;
        uint32_t vme_16x16_cost_segment1: 8;
        uint32_t vme_16x16_cost_segment2: 8;
        uint32_t vme_16x16_cost_segment3: 8;
    } dw24;

    struct {
        uint32_t vme_4x4_cost_segment0: 8;
        uint32_t vme_4x4_cost_segment1: 8;
        uint32_t vme_4x4_cost_segment2: 8;
        uint32_t vme_4x4_cost_segment3: 8;
    } dw25;

    struct {
        uint32_t vme_16x16_non_dc_penalty_segment0: 8;
        uint32_t vme_16x16_non_dc_penalty_segment1: 8;
        uint32_t vme_16x16_non_dc_penalty_segment2: 8;
        uint32_t vme_16x16_non_dc_penalty_segment3: 8;
    } dw26;

    struct {
        uint32_t vme_4x4_non_dc_penalty_segment0: 8;
        uint32_t vme_4x4_non_dc_penalty_segment1: 8;
        uint32_t vme_4x4_non_dc_penalty_segment2: 8;
        uint32_t vme_4x4_non_dc_penalty_segment3: 8;
    } dw27;

    struct {
        uint32_t reserved;
    } dw28;

    struct {
        uint32_t reserved;
    } dw29;

    struct {
        uint32_t reserved;
    } dw30;

    struct {
        uint32_t reserved;
    } dw31;

    struct {
        uint32_t mb_enc_per_mb_out_data_surf_bti;
    } dw32;

    struct {
        uint32_t mb_enc_curr_y_bti;
    } dw33;

    struct {
        uint32_t mb_enc_curr_uv_bti;
    } dw34;

    struct {
        uint32_t mb_mode_cost_luma_bti;
    } dw35;

    struct {
        uint32_t mb_enc_block_mode_cost_bti;
    } dw36;

    struct {
        uint32_t chroma_recon_surf_bti;
    } dw37;

    struct {
        uint32_t segmentation_map_bti;
    } dw38;

    struct {
        uint32_t histogram_bti;
    } dw39;

    struct {
        uint32_t mb_enc_vme_debug_stream_out_bti;
    } dw40;

    struct {
        uint32_t vme_bti;
    } dw41;

    struct {
        uint32_t idist_surface_bti;
    } dw42;

    struct {
        uint32_t curr_y_down_scaled_bti;
    } dw43;

    struct {
        uint32_t vme_coarse_intra_bti;
    } dw44;
};

struct vp8_mbenc_p_frame_curbe_data {
    struct {
        uint32_t frame_width: 16;
        uint32_t frame_height: 16;
    } dw0;

    struct {
        uint32_t frame_type: 1;
        uint32_t multiple_pred: 2;
        uint32_t hme_enable: 1;
        uint32_t hme_combine_overlap: 2;
        uint32_t all_fractional: 1;
        uint32_t enable_temporal_scalability: 1;
        uint32_t hme_combined_extra_su: 8;
        uint32_t ref_frame_flags: 4;
        uint32_t enable_segmentation: 1;
        uint32_t enable_segmentation_info_update: 1;
        uint32_t enable_coeff_clamp: 1;
        uint32_t multi_reference_qp_check: 1;
        uint32_t mode_cost_enable_flag: 1;
        uint32_t main_ref: 6;
        uint32_t enable_debug_dumps: 1;
    } dw1;

    struct {
        uint32_t lambda_intra_segment0: 16;
        uint32_t lambda_inter_segment0: 16;
    } dw2;

    struct {
        uint32_t lambda_intra_segment1: 16;
        uint32_t lambda_inter_segment1: 16;
    } dw3;

    struct {
        uint32_t lambda_intra_segment2: 16;
        uint32_t lambda_inter_segment2: 16;
    } dw4;

    struct {
        uint32_t lambda_intra_segment3: 16;
        uint32_t lambda_inter_segment3: 16;
    } dw5;

    struct {
        uint32_t reference_frame_sign_bias_0: 8;
        uint32_t reference_frame_sign_bias_1: 8;
        uint32_t reference_frame_sign_bias_2: 8;
        uint32_t reference_frame_sign_bias_3: 8;
    } dw6;

    struct {
        uint32_t raw_dist_threshold: 16;
        uint32_t temporal_layer_id: 8;
        uint32_t reserved_mbz: 8;
    } dw7;

    struct {
        uint32_t skip_mode_enable: 1;
        uint32_t adaptive_search_enable: 1;
        uint32_t bidirectional_mix_disbale: 1;
        uint32_t reserved_mbz1: 2;
        uint32_t early_ime_success_enable: 1;
        uint32_t reserved_mbz2: 1;
        uint32_t transform8x8_flag_for_inter_enable: 1;
        uint32_t reserved_mbz3: 16;
        uint32_t early_ime_successful_stop_threshold: 8;
    } dw8;

    struct {
        uint32_t max_num_of_motion_vectors: 6;
        uint32_t reserved_mbz1: 2;
        uint32_t ref_id_polarity_bits: 8;
        uint32_t bidirectional_weight: 6;
        uint32_t reserved_mbz2: 6;
        uint32_t unidirection_mix_enable: 1;
        uint32_t ref_pixel_bias_enable: 1;
        uint32_t reserved_mbz3: 2;
    } dw9;

    struct {
        uint32_t max_fixed_search_path_length: 8;
        uint32_t maximum_search_path_length: 8;
        uint32_t reserved_mbz: 16;
    } dw10;

    struct {
        uint32_t source_block_size: 2;
        uint32_t reserved_mbz1: 2;
        uint32_t inter_mb_type_road_map: 2;
        uint32_t source_access: 1;
        uint32_t reference_access: 1;
        uint32_t search_control: 3;
        uint32_t dual_search_path_option: 1;
        uint32_t sub_pel_mode: 2;
        uint32_t skip_mode_type: 1;
        uint32_t disable_field_cache_allocation: 1;
        uint32_t process_inter_chroma_pixels_mode: 1;
        uint32_t forward_trans_form_skip_check_enable: 1;
        uint32_t bme_disable_for_fbr_message: 1;
        uint32_t block_based_skip_enable: 1;
        uint32_t inter_sad_measure_adjustment: 2;
        uint32_t intra_sad_measure_adjustment: 2;
        uint32_t submacro_block_subPartition_mask: 6;
        uint32_t reserved_mbz2: 1;
    } dw11;

    struct {
        uint32_t reserved_mbz: 16;
        uint32_t reference_search_windows_width: 8;
        uint32_t reference_search_windows_height: 8;
    } dw12;

    struct {
        uint32_t mode_0_3_cost_seg0;
    } dw13;

    struct {
        uint32_t mode_4_7_cost_seg0;
    } dw14;

    struct {
        uint32_t mode_8_9_ref_id_chroma_cost_seg0;
    } dw15;

    struct {
        struct vp8_search_path_delta sp_delta_0;
        struct vp8_search_path_delta sp_delta_1;
        struct vp8_search_path_delta sp_delta_2;
        struct vp8_search_path_delta sp_delta_3;
    } dw16;

    struct {
        struct vp8_search_path_delta sp_delta_4;
        struct vp8_search_path_delta sp_delta_5;
        struct vp8_search_path_delta sp_delta_6;
        struct vp8_search_path_delta sp_delta_7;
    } dw17;

    struct {
        struct vp8_search_path_delta sp_delta_8;
        struct vp8_search_path_delta sp_delta_9;
        struct vp8_search_path_delta sp_delta_10;
        struct vp8_search_path_delta sp_delta_11;
    } dw18;

    struct {
        struct vp8_search_path_delta sp_delta_12;
        struct vp8_search_path_delta sp_delta_13;
        struct vp8_search_path_delta sp_delta_14;
        struct vp8_search_path_delta sp_delta_15;
    } dw19;

    struct {
        struct vp8_search_path_delta sp_delta_16;
        struct vp8_search_path_delta sp_delta_17;
        struct vp8_search_path_delta sp_delta_18;
        struct vp8_search_path_delta sp_delta_19;
    } dw20;

    struct {
        struct vp8_search_path_delta sp_delta_20;
        struct vp8_search_path_delta sp_delta_21;
        struct vp8_search_path_delta sp_delta_22;
        struct vp8_search_path_delta sp_delta_23;
    } dw21;

    struct {
        struct vp8_search_path_delta sp_delta_24;
        struct vp8_search_path_delta sp_delta_25;
        struct vp8_search_path_delta sp_delta_26;
        struct vp8_search_path_delta sp_delta_27;
    } dw22;

    struct {
        struct vp8_search_path_delta sp_delta_28;
        struct vp8_search_path_delta sp_delta_29;
        struct vp8_search_path_delta sp_delta_30;
        struct vp8_search_path_delta sp_delta_31;
    } dw23;

    struct {
        struct vp8_search_path_delta sp_delta_32;
        struct vp8_search_path_delta sp_delta_33;
        struct vp8_search_path_delta sp_delta_34;
        struct vp8_search_path_delta sp_delta_35;
    } dw24;

    struct {
        struct vp8_search_path_delta sp_delta_36;
        struct vp8_search_path_delta sp_delta_37;
        struct vp8_search_path_delta sp_delta_38;
        struct vp8_search_path_delta sp_delta_39;
    } dw25;

    struct {
        struct vp8_search_path_delta sp_delta_40;
        struct vp8_search_path_delta sp_delta_41;
        struct vp8_search_path_delta sp_delta_42;
        struct vp8_search_path_delta sp_delta_43;
    };

    struct {
        struct vp8_search_path_delta sp_delta_44;
        struct vp8_search_path_delta sp_delta_45;
        struct vp8_search_path_delta sp_delta_46;
        struct vp8_search_path_delta sp_delta_47;
    } dw27;

    struct {
        struct vp8_search_path_delta sp_delta_48;
        struct vp8_search_path_delta sp_delta_49;
        struct vp8_search_path_delta sp_delta_50;
        struct vp8_search_path_delta sp_delta_51;
    } dw28;

    struct {
        struct vp8_search_path_delta sp_delta_52;
        struct vp8_search_path_delta sp_delta_53;
        struct vp8_search_path_delta sp_delta_54;
        struct vp8_search_path_delta sp_delta_55;
    } dw29;

    struct {
        uint32_t mv_0_3_cost_seg0;
    } dw30;

    struct {
        uint32_t mv_4_7_cost_seg0;
    } dw31;

    struct {
        uint32_t intra_16x16_no_dc_penalty_segment0: 8;
        uint32_t intra_16x16_no_dc_penalty_segment1: 8;
        uint32_t reserved_mbz1: 7;
        uint32_t bilinear_enable: 1;
        uint32_t reserved_mbz2: 8;
    } dw32;

    struct {
        uint32_t hme_combine_len: 16;
        uint32_t intra_16x16_no_dc_penalty_segment2: 8;
        uint32_t intra_16x16_no_dc_penalty_segment3: 8;
    } dw33;

    struct {
        uint32_t mv_ref_cost_context_0_0_0: 16;
        uint32_t mv_ref_cost_context_0_0_1: 16;
    } dw34;

    struct {
        uint32_t mv_ref_cost_context_0_1_0: 16;
        uint32_t mv_ref_cost_context_0_1_1: 16;
    } dw35;

    struct {
        uint32_t mv_ref_cost_context_0_2_0: 16;
        uint32_t mv_ref_cost_context_0_2_1: 16;
    } dw36;

    struct {
        uint32_t mv_ref_cost_context_0_3_0: 16;
        uint32_t mv_ref_cost_context_0_3_1: 16;
    } dw37;

    struct {
        uint32_t mv_ref_cost_context_1_0_0: 16;
        uint32_t mv_ref_cost_context_1_0_1: 16;
    } dw38;

    struct {
        uint32_t mv_ref_cost_context_1_1_0: 16;
        uint32_t mv_ref_cost_context_1_1_1: 16;
    } dw39;

    struct {
        uint32_t mv_ref_cost_context_1_2_0: 16;
        uint32_t mv_ref_cost_context_1_2_1: 16;
    } dw40;

    struct {
        uint32_t mv_ref_cost_context_1_3_0: 16;
        uint32_t mv_ref_cost_context_1_3_1: 16;
    } dw41;

    struct {
        uint32_t mv_ref_cost_context_2_0_0: 16;
        uint32_t mv_ref_cost_context_2_0_1: 16;
    };

    struct {
        uint32_t mv_ref_cost_context_2_1_0: 16;
        uint32_t mv_ref_cost_context_2_1_1: 16;
    };

    struct {
        uint32_t mv_ref_cost_context_2_2_0: 16;
        uint32_t mv_ref_cost_context_2_2_1: 16;
    } dw44;

    struct {
        uint32_t mv_ref_cost_context_2_3_0: 16;
        uint32_t mv_ref_cost_context_2_3_1: 16;
    } dw45;

    struct {
        uint32_t mv_ref_cost_context_3_0_0: 16;
        uint32_t mv_ref_cost_context_3_0_1: 16;
    } dw46;

    struct {
        uint32_t mv_ref_cost_context_3_1_0: 16;
        uint32_t mv_ref_cost_context_3_1_1: 16;
    } dw47;

    struct {
        uint32_t mv_ref_cost_context_3_2_0: 16;
        uint32_t mv_ref_cost_context_3_2_1: 16;
    } dw48;

    struct {
        uint32_t mv_ref_cost_context_3_3_0: 16;
        uint32_t mv_ref_cost_context_3_3_1: 16;
    } dw49;

    struct {
        uint32_t mv_ref_cost_context_4_0_0: 16;
        uint32_t mv_ref_cost_context_4_0_1: 16;
    } dw50;

    struct {
        uint32_t mv_ref_cost_context_4_1_0: 16;
        uint32_t mv_ref_cost_context_4_1_1: 16;
    } dw51;

    struct {
        uint32_t mv_ref_cost_context_4_2_0: 16;
        uint32_t mv_ref_cost_context_4_2_1: 16;
    };

    struct {
        uint32_t mv_ref_cost_context_4_3_0: 16;
        uint32_t mv_ref_cost_context_4_3_1: 16;
    };

    struct {
        uint32_t mv_ref_cost_context_5_0_0: 16;
        uint32_t mv_ref_cost_context_5_0_1: 16;
    };

    struct {
        uint32_t mv_ref_cost_context_5_1_0: 16;
        uint32_t mv_ref_cost_context_5_1_1: 16;
    } dw55;

    struct {
        uint32_t mv_ref_cost_context_5_2_0: 16;
        uint32_t mv_ref_cost_context_5_2_1: 16;
    } dw56;

    struct {
        uint32_t mv_ref_cost_context_5_3_0: 16;
        uint32_t mv_ref_cost_context_5_3_1: 16;
    } dw57;

    struct {
        uint32_t enc_cost_16x16: 16;
        uint32_t enc_cost_16x8: 16;
    } dw58;

    struct {
        uint32_t enc_cost_8x8: 16;
        uint32_t enc_cost_4x4: 16;
    } dw59;

    struct {
        uint32_t frame_count_probability_ref_frame_cost_0: 16;
        uint32_t frame_count_probability_ref_frame_cost_1: 16;
    } dw60;

    struct {
        uint32_t frame_count_probability_ref_frame_cost_2: 16;
        uint32_t frame_count_probability_ref_frame_cost_3: 16;
    } dw61;

    struct {
        uint32_t average_qp_of_last_ref_frame: 8;
        uint32_t average_qp_of_gold_ref_frame: 8;
        uint32_t average_qp_of_alt_ref_frame: 8;
        uint32_t reserved_mbz: 8;
    } dw62;

    struct {
        uint32_t intra_4x4_no_dc_penalty_segment0: 8;
        uint32_t intra_4x4_no_dc_penalty_segment1: 8;
        uint32_t intra_4x4_no_dc_penalty_segment2: 8;
        uint32_t intra_4x4_no_dc_penalty_segment3: 8;
    } dw63;

    struct {
        uint32_t mode_0_3_cost_seg1;
    } dw64;

    struct {
        uint32_t mode_4_7_cost_seg1;
    } dw65;

    struct {
        uint32_t mode_8_9_ref_id_chroma_cost_seg1;
    } dw66;

    struct {
        uint32_t mv_0_3_cost_seg1;
    } dw67;

    struct {
        uint32_t mv_4_7_cost_seg1;
    } dw68;

    struct {
        uint32_t mode_0_3_cost_seg2;
    } dw69;

    struct {
        uint32_t mode_4_7_cost_seg2;
    } dw70;

    struct {
        uint32_t mode_8_9_ref_id_chroma_cost_seg2;
    } dw71;

    struct {
        uint32_t mv_0_3_cost_seg2;
    } dw72;

    struct {
        uint32_t mv_4_7_cost_seg2;
    } dw73;

    struct {
        uint32_t mode_0_3_cost_seg3;
    } dw74;

    struct {
        uint32_t mode_4_7_cost_seg3;
    } dw75;

    struct {
        uint32_t mode_8_9_ref_id_chroma_cost_seg3;
    } dw76;

    struct {
        uint32_t mv_0_3_cost_seg3;
    } dw77;

    struct {
        uint32_t mv_4_7_cost_seg3;
    } dw78;

    struct {
        uint32_t new_mv_skip_threshold_segment0: 16;
        uint32_t new_mv_skip_threshold_segment1: 16;
    } dw79;

    struct {
        uint32_t new_mv_skip_threshold_segment2: 16;
        uint32_t new_mv_skip_threshold_segment3: 16;
    } dw80;

    struct {
        uint32_t per_mb_output_data_surface_bti;
    } dw81;

    struct {
        uint32_t current_picture_y_surface_bti;
    } dw82;

    struct {
        uint32_t current_picture_interleaved_uv_surface_bti;
    } dw83;

    struct {
        uint32_t hme_mv_data_surface_bti;
    } dw84;

    struct {
        uint32_t mv_data_surface_bti;
    } dw85;

    struct {
        uint32_t mb_count_per_reference_frame_bti;
    } dw86;

    struct {
        uint32_t vme_inter_prediction_bti;
    } dw87;

    struct {
        uint32_t active_ref1_bti;
    } dw88;

    struct {
        uint32_t active_ref2_bti;
    } dw89;

    struct {
        uint32_t active_ref3_bti;
    } dw90;

    struct {
        uint32_t per_mb_quant_data_bti;
    } dw91;

    struct {
        uint32_t segment_map_bti;
    } dw92;

    struct {
        uint32_t inter_prediction_distortion_bti;
    } dw93;

    struct {
        uint32_t histogram_bti;
    } dw94;

    struct {
        uint32_t pred_mv_data_bti;
    } dw95;

    struct {
        uint32_t mode_cost_update_bti;
    } dw96;

    struct {
        uint32_t kernel_debug_dump_bti;
    } dw97;
};

#define VP8_MBENC_I_FRAME_DIST          0
#define VP8_MBENC_I_FRAME_LUMA          1
#define VP8_MBENC_I_FRAME_CHROMA        2
#define VP8_MBENC_P_FRAME               3
#define NUM_VP8_MBENC                   4

struct i965_encoder_vp8_mbenc_context {
    struct i965_gpe_context gpe_contexts[NUM_VP8_MBENC];
    dri_bo *luma_chroma_dynamic_buffer;
};

enum vp8_binding_table_offset_brc_update {
    VP8_BTI_BRC_UPDATE_HISTORY                  = 1,
    VP8_BTI_BRC_UPDATE_PAK_STATISTICS_OUTPUT    = 2,
    VP8_BTI_BRC_UPDATE_MFX_ENCODER_CFG_READ     = 3,
    VP8_BTI_BRC_UPDATE_MFX_ENCODER_CFG_WRITE    = 4,
    VP8_BTI_BRC_UPDATE_MBENC_CURBE_READ         = 5,
    VP8_BTI_BRC_UPDATE_MBENC_CURBE_WRITE        = 6,
    VP8_BTI_BRC_UPDATE_DISTORTION_SURFACE       = 7,
    VP8_BTI_BRC_UPDATE_CONSTANT_DATA            = 8,
    VP8_BTI_BRC_UPDATE_SEGMENT_MAP              = 9,
    VP8_BTI_BRC_UPDATE_MPU_CURBE_READ           = 10,
    VP8_BTI_BRC_UPDATE_MPU_CURBE_WRITE          = 11,
    VP8_BTI_BRC_UPDATE_TPU_CURBE_READ           = 12,
    VP8_BTI_BRC_UPDATE_TPU_CURBE_WRITE          = 13,
    VP8_BTI_BRC_UPDATE_NUM_SURFACES             = 14
};

struct brc_update_surface_parameters {
    struct i965_gpe_context *mbenc_gpe_context;
    struct i965_gpe_context *mpu_gpe_context;
    struct i965_gpe_context *tpu_gpe_context;
};

struct vp8_brc_update_curbe_data {
    struct {
        uint32_t target_size;
    } dw0;

    struct {
        uint32_t frame_number;
    } dw1;

    struct {
        uint32_t picture_header_size;
    } dw2;

    struct {
        uint32_t start_global_adjust_frame0: 16;
        uint32_t start_global_adjust_frame1: 16;
    } dw3;

    struct {
        uint32_t start_global_adjust_frame2: 16;
        uint32_t start_global_adjust_frame3: 16;
    } dw4;

    struct {
        uint32_t target_size_flag: 8;
        uint32_t brc_flag: 8;
        uint32_t max_num_paks: 8;
        uint32_t curr_frame_type: 8;
    } dw5;

    struct {
        uint32_t tid: 8;
        uint32_t num_t_levels: 8;
        uint32_t reserved0: 16;
    } dw6;

    struct {
        uint32_t reserved0;
    } dw7;

    struct {
        uint32_t start_global_adjust_mult0: 8;
        uint32_t start_global_adjust_mult1: 8;
        uint32_t start_global_adjust_mult2: 8;
        uint32_t start_global_adjust_mult3: 8;
    } dw8;

    struct {
        uint32_t start_global_adjust_mult4: 8;
        uint32_t start_global_adjust_div0: 8;
        uint32_t start_global_adjust_div1: 8;
        uint32_t start_global_adjust_div2: 8;
    } dw9;

    struct {
        uint32_t start_global_adjust_div3: 8;
        uint32_t start_global_adjust_div4: 8;
        uint32_t qp_threshold0: 8;
        uint32_t qp_threshold1: 8;
    } dw10;

    struct {
        uint32_t qp_threshold2: 8;
        uint32_t qp_threshold3: 8;
        uint32_t g_rate_ratio_threshold0: 8;
        uint32_t g_rate_ratio_threshold1: 8;
    } dw11;

    struct {
        uint32_t g_rate_ratio_threshold2: 8;
        uint32_t g_rate_ratio_threshold3: 8;
        uint32_t g_rate_ratio_threshold4: 8;
        uint32_t g_rate_ratio_threshold5: 8;
    } dw12;

    struct {
        uint32_t g_rate_ratio_threshold_qp0: 8;
        uint32_t g_rate_ratio_threshold_qp1: 8;
        uint32_t g_rate_ratio_threshold_qp2: 8;
        uint32_t g_rate_ratio_threshold_qp3: 8;
    } dw13;

    struct {
        uint32_t g_rate_ratio_threshold_qp4: 8;
        uint32_t g_rate_ratio_threshold_qp5: 8;
        uint32_t g_rate_ratio_threshold_qp6: 8;
        uint32_t index_of_previous_qp: 8;
    } dw14;

    struct {
        uint32_t frame_width_in_mb: 16;
        uint32_t frame_height_in_mb: 16;
    } dw15;

    struct {
        uint32_t p_frame_qp_seg0: 8;
        uint32_t p_frame_qp_seg1: 8;
        uint32_t p_frame_qp_seg2: 8;
        uint32_t p_frame_qp_seg3: 8;
    } dw16;

    struct {
        uint32_t key_frame_qp_seg0: 8;
        uint32_t key_frame_qp_seg1: 8;
        uint32_t key_frame_qp_seg2: 8;
        uint32_t key_frame_qp_seg3: 8;
    } dw17;

    struct {
        uint32_t qdelta_plane0: 8;
        uint32_t qdelta_plane1: 8;
        uint32_t qdelta_plane2: 8;
        uint32_t qdelta_plane3: 8;
    } dw18;

    struct {
        uint32_t qdelta_plane4: 8;
        uint32_t qindex: 8;
        uint32_t main_ref: 8;
        uint32_t ref_frame_flags: 8;
    } dw19;

    struct {
        uint32_t seg_on: 8;
        uint32_t mb_rc: 8;
        uint32_t brc_method: 8;
        uint32_t vme_intra_prediction: 8;
    } dw20;

    struct {
        uint32_t current_frame_qpindex: 8;
        uint32_t last_frame_qpindex: 8;
        uint32_t gold_frame_qpindex: 8;
        uint32_t alt_frame_qpindex: 8;
    } dw21;

    struct {
        uint32_t historyt_buffer_bti;
    } dw22;

    struct {
        uint32_t pak_statistics_bti;
    } dw23;

    struct {
        uint32_t mfx_vp8_encoder_cfg_read_bti;
    } dw24;

    struct {
        uint32_t mfx_vp8_encoder_cfg_write_bti;
    } dw25;

    struct {
        uint32_t mbenc_curbe_read_bti;
    } dw26;

    struct {
        uint32_t mbenc_curbe_write_bti;
    } dw27;

    struct {
        uint32_t distortion_bti;
    } dw28;

    struct {
        uint32_t constant_data_bti;
    } dw29;

    struct {
        uint32_t segment_map_bti;
    } dw30;

    struct {
        uint32_t mpu_curbe_read_bti;
    } dw31;

    struct {
        uint32_t mpu_curbe_write_bti;
    } dw32;

    struct {
        uint32_t tpu_curbe_read_bti;
    } dw33;

    struct {
        uint32_t tpu_curbe_write_bti;
    } dw34;
};

#define VP8_BRC_UPDATE                  0
#define NUM_VP8_BRC_UPDATE              1

struct i965_encoder_vp8_brc_update_context {
    struct i965_gpe_context gpe_contexts[NUM_VP8_BRC_UPDATE];
};

enum vp8_binding_table_offset_mpu {
    VP8_BTI_MPU_HISTOGRAM               = 0,
    VP8_BTI_MPU_REF_MODE_PROBABILITY    = 1,
    VP8_BTI_MPU_CURR_MODE_PROBABILITY   = 2,
    VP8_BTI_MPU_REF_TOKEN_PROBABILITY   = 3,
    VP8_BTI_MPU_CURR_TOKEN_PROBABILITY  = 4,
    VP8_BTI_MPU_HEADER_BITSTREAM        = 5,
    VP8_BTI_MPU_HEADER_METADATA         = 6,
    VP8_BTI_MPU_PICTURE_STATE           = 7,
    VP8_BTI_MPU_MPU_BITSTREAM           = 8,
    VP8_BTI_MPU_TOKEN_BITS_DATA_TABLE   = 9,
    VP8_BTI_MPU_VME_DEBUG_STREAMOUT     = 10,
    VP8_BTI_MPU_ENTROPY_COST_TABLE      = 11,
    VP8_BTI_MPU_MODE_COST_UPDATE        = 12,
    VP8_BTI_MPU_NUM_SURFACES            = 13
};

struct vp8_mpu_curbe_data {
    struct {
        uint32_t frame_width: 16;
        uint32_t frame_height: 16;
    } dw0;

    struct {
        uint32_t frame_type: 1;
        uint32_t version: 3;
        uint32_t show_frame: 1;
        uint32_t horizontal_scale_code: 2;
        uint32_t vertical_scale_code: 2;
        uint32_t color_space_type: 1;
        uint32_t clamp_type: 1;
        uint32_t partition_num_l2: 2;
        uint32_t enable_segmentation: 1;
        uint32_t seg_map_update: 1;
        uint32_t segmentation_feature_update: 1;
        uint32_t segmentation_feature_mode: 1;
        uint32_t loop_filter_type: 1;
        uint32_t sharpness_level: 3;
        uint32_t loop_filter_adjustment_on: 1;
        uint32_t mb_no_coeffiscient_skip: 1;
        uint32_t golden_reference_copy_flag: 2;
        uint32_t alternate_reference_copy_flag: 2;
        uint32_t last_frame_update: 1;
        uint32_t sign_bias_golden: 1;
        uint32_t sign_bias_alt_ref: 1;
        uint32_t refresh_entropy_p: 1;
        uint32_t forced_lf_update_for_key_frame: 1;
    } dw1;

    struct {
        uint32_t loop_filter_level: 6;
        uint32_t reserved0: 2;
        uint32_t qindex: 7;
        uint32_t reserved1: 1;
        uint32_t y1_dc_qindex: 8;
        uint32_t y2_dc_qindex: 8;
    } dw2;

    struct {
        uint32_t y2_ac_qindex: 8;
        uint32_t uv_dc_qindex: 8;
        uint32_t uv_ac_qindex: 8;
        uint32_t feature_data0_segment0: 8;
    } dw3;

    struct {
        uint32_t feature_data0_segment1: 8;
        uint32_t feature_data0_segment2: 8;
        uint32_t feature_data0_segment3: 8;
        uint32_t feature_data1_segment0: 8;
    } dw4;

    struct {
        uint32_t feature_data1_segment1: 8;
        uint32_t feature_data1_segment2: 8;
        uint32_t feature_data1_segment3: 8;
        uint32_t ref_lf_delta0: 8;
    } dw5;

    struct {
        uint32_t ref_lf_delta1: 8;
        uint32_t ref_lf_delta2: 8;
        uint32_t ref_lf_delta3: 8;
        uint32_t mode_lf_delta0: 8;
    } dw6;

    struct {
        uint32_t mode_lf_delta1: 8;
        uint32_t mode_lf_delta2: 8;
        uint32_t mode_lf_delta3: 8;
        uint32_t forced_token_surface_read: 1;
        uint32_t mode_cost_enable_flag: 1;
        uint32_t mc_filter_select: 1;
        uint32_t chroma_full_pixel_mc_filter_mode: 1;
        uint32_t max_num_pak_passes: 4;

    } dw7;

    struct {
        uint32_t temporal_layer_id: 8;
        uint32_t num_t_levels: 8;
        uint32_t reserved: 16;
    } dw8;

    struct {
        uint32_t reserved;
    } dw9;

    struct {
        uint32_t reserved;
    } dw10;

    struct {
        uint32_t reserved;
    } dw11;

    struct {
        uint32_t histogram_bti;
    } dw12;

    struct {
        uint32_t reference_mode_probability_bti;
    } dw13;

    struct {
        uint32_t mode_probability_bti;
    } dw14;

    struct {
        uint32_t reference_token_probability_bti;
    } dw15;

    struct {
        uint32_t token_probability_bti;
    } dw16;

    struct {
        uint32_t frame_header_bitstream_bti;
    } dw17;

    struct {
        uint32_t header_meta_data_bti;
    } dw18;

    struct {
        uint32_t picture_state_bti;
    } dw19;

    struct {
        uint32_t mpu_bitstream_bti;
    } dw20;

    struct {
        uint32_t token_bits_data_bti;
    } dw21;

    struct {
        uint32_t kernel_debug_dump_bti;
    } dw22;

    struct {
        uint32_t entropy_cost_bti;
    } dw23;

    struct {
        uint32_t mode_cost_update_bti;
    } dw24;
};

struct vp8_mfx_encoder_cfg_cmd {
    union {
        struct {
            uint32_t dword_length: 12;
            uint32_t reserved: 4;
            uint32_t sub_opcode_b: 5;
            uint32_t sub_opcode_a: 3;
            uint32_t media_command_opcode: 3;
            uint32_t pipeline: 2;
            uint32_t command_type: 3;
        };

        uint32_t value;
    } dw0;

    struct {
        uint32_t performance_counter_enable: 1;
        uint32_t final_bitstream_output_disable: 1;
        uint32_t token_statistics_output_enable: 1;
        uint32_t bitstream_statistics_output_enable: 1;
        uint32_t update_segment_feature_data_flag: 1;
        uint32_t skip_final_bitstream_when_over_under_flow: 1;
        uint32_t rate_control_initial_pass: 1;
        uint32_t per_segment_delta_qindex_loop_filter_disable: 1;
        uint32_t finer_brc_enable: 1;
        uint32_t compressed_bitstream_output_disable: 1;
        uint32_t clock_gating_disable: 1;
        uint32_t reserved: 21;
    } dw1;

    struct {
        uint32_t max_frame_bit_count_rate_control_enable_mask: 1;
        uint32_t min_frame_bit_count_rate_control_enable_mask: 1;
        uint32_t max_inter_mb_bit_count_check_enable_mask: 1;
        uint32_t max_intra_mb_bit_count_check_enable_mask: 1;
        uint32_t inter_mediate_bit_buffer_overrun_enable_mask: 1;
        uint32_t final_bistream_buffer_overrun_enable_mask: 1;
        uint32_t qindex_clamp_high_mask_for_underflow: 1;
        uint32_t qindex_clamp_high_mask_for_overflow: 1;
        uint32_t reserved: 24;
    } dw2;

    struct {
        uint32_t max_inter_mb_bit_count: 12;
        uint32_t reserved0: 4;
        uint32_t max_intra_mb_bit_count_limit: 12;
        uint32_t reserved1: 4;
    } dw3;

    struct {
        uint32_t frame_bit_rate_max: 14;
        uint32_t frame_bit_rate_max_unit: 1;
        uint32_t frame_bit_rate_max_unit_mode: 1;
        uint32_t frame_bit_rate_min: 14;
        uint32_t frame_bit_rate_min_unit: 1;
        uint32_t frame_bit_rate_min_unit_mode: 1;
    } dw4;

    struct {
        uint32_t frame_delta_qindex_max0: 8;
        uint32_t frame_delta_qindex_max1: 8;
        uint32_t frame_delta_qindex_max2: 8;
        uint32_t frame_delta_qindex_max3: 8;
    } dw5;

    struct {
        uint32_t frame_delta_qindex_min0: 8;
        uint32_t frame_delta_qindex_min1: 8;
        uint32_t frame_delta_qindex_min2: 8;
        uint32_t frame_delta_qindex_min3: 8;
    } dw6;

    struct {
        uint32_t per_segment_frame_delta_qindex_max1;
    } dw7;

    struct {
        uint32_t per_segment_frame_delta_qindex_min1;
    } dw8;

    struct {
        uint32_t per_segment_frame_delta_qindex_max2;
    } dw9;

    struct {
        uint32_t per_segment_frame_delta_qindex_min2;
    } dw10;

    struct {
        uint32_t per_segment_frame_delta_qindex_max3;
    } dw11;

    struct {
        uint32_t per_segment_frame_delta_qindex_min3;
    } dw12;

    struct {
        uint32_t frame_delta_loop_filter_max0: 8;
        uint32_t frame_delta_loop_filter_max1: 8;
        uint32_t frame_delta_loop_filter_max2: 8;
        uint32_t frame_delta_loop_filter_max3: 8;
    } dw13;

    struct {
        uint32_t frame_delta_loop_filter_min0: 8;
        uint32_t frame_delta_loop_filter_min1: 8;
        uint32_t frame_delta_loop_filter_min2: 8;
        uint32_t frame_delta_loop_filter_min3: 8;
    } dw14;

    struct {
        uint32_t per_segment_frame_delta_loop_filter_max1;
    } dw15;

    struct {
        uint32_t per_segment_frame_delta_loop_filter_min1;
    } dw16;

    struct {
        uint32_t per_segment_frame_delta_loop_filter_max2;
    } dw17;

    struct {
        uint32_t per_segment_frame_delta_loop_filter_min2;
    } dw18;

    struct {
        uint32_t per_segment_frame_delta_loop_filter_max3;
    } dw19;

    struct {
        uint32_t per_segment_frame_delta_loop_filter_min3;
    } dw20;

    struct {
        uint32_t frame_bit_rate_max_delta: 15;
        uint32_t reserved0: 1;
        uint32_t frame_bit_rate_min_delta: 15;
        uint32_t reserved1: 1;
    } dw21;

    struct {
        uint32_t min_frame_w_size: 16;
        uint32_t min_frame_w_size_unit: 2;
        uint32_t reserved0: 2;
        uint32_t bitstream_format_version: 3;
        uint32_t show_frame: 1;
        uint32_t reserved1: 8;
    } dw22;

    struct {
        uint32_t horizontal_size_code: 16;
        uint32_t vertical_size_code: 16;
    } dw23;

    struct {
        uint32_t frame_header_bit_count;
    } dw24;

    struct {
        uint32_t frame_header_bin_buffer_qindex_update_pointer;
    } dw25;

    struct {
        uint32_t frame_header_bin_buffer_loopfilter_update_pointer;
    } dw26;

    struct {
        uint32_t frame_header_bin_buffer_token_update_pointer;
    } dw27;

    struct {
        uint32_t frame_header_bin_buffer_MvupdatePointer;
    } dw28;

    struct {
        uint32_t cv0_neg_clamp_value0: 4;
        uint32_t cv1: 4;
        uint32_t cv2: 4;
        uint32_t cv3: 4;
        uint32_t cv4: 4;
        uint32_t cv5: 4;
        uint32_t cv6: 4;
        uint32_t clamp_values_cv7: 4;
    } dw29;
};

struct vp8_mpu_encoder_config_parameters {
    struct i965_gpe_resource *config_buffer;
    unsigned int is_first_pass;
    unsigned int command_offset;
    unsigned int buffer_size;
};

#define VP8_MPU                         0
#define NUM_VP8_MPU                     1

struct i965_encoder_vp8_mpu_context {
    struct i965_gpe_context gpe_contexts[NUM_VP8_MPU];
    dri_bo *dynamic_buffer;
};

enum vp8_binding_table_offset_tpu {
    VP8_BTI_TPU_PAK_TOKEN_STATISTICS            = 0,
    VP8_BTI_TPU_TOKEN_UPDATE_FLAGS              = 1,
    VP8_BTI_TPU_ENTROPY_COST_TABLE              = 2,
    VP8_BTI_TPU_HEADER_BITSTREAM                = 3,
    VP8_BTI_TPU_DEFAULT_TOKEN_PROBABILITY       = 4,
    VP8_BTI_TPU_PICTURE_STATE                   = 5,
    VP8_BTI_TPU_MPU_CURBE_DATA                  = 6,
    VP8_BTI_TPU_HEADER_METADATA                 = 7,
    VP8_BTI_TPU_TOKEN_PROBABILITY               = 8,
    VP8_BTI_TPU_PAK_HW_PASS1_PROBABILITY        = 9,
    VP8_BTI_TPU_KEY_TOKEN_PROBABILITY           = 10,
    VP8_BTI_TPU_UPDATED_TOKEN_PROBABILITY       = 11,
    VP8_BTI_TPU_PAK_HW_PASS2_PROBABILITY        = 12,
    VP8_BTI_TPU_VME_DEBUG_STREAMOUT             = 13,
    VP8_BTI_TPU_REPAK_DECISION                  = 14,
    VP8_BTI_TPU_NUM_SURFACES                    = 15
};

struct vp8_tpu_curbe_data {
    struct {
        uint32_t mbs_in_frame;
    } dw0;

    struct {
        uint32_t frame_type: 1;
        uint32_t enable_segmentation: 1;
        uint32_t rebinarization_frame_hdr: 1;
        uint32_t refresh_entropy_p: 1;
        uint32_t mb_no_coeffiscient_skip: 1;
        uint32_t reserved: 27;
    } dw1;

    struct {
        uint32_t token_probability_start_offset: 16;
        uint32_t token_probability_end_offset: 16;
    } dw2;

    struct {
        uint32_t frame_header_bit_count: 16;
        uint32_t max_qp: 8;
        uint32_t min_qp: 8;
    } dw3;

    struct {
        uint32_t loop_filter_level_segment0: 8;
        uint32_t loop_filter_level_segment1: 8;
        uint32_t loop_filter_level_segment2: 8;
        uint32_t loop_filter_level_segment3: 8;
    } dw4;

    struct {
        uint32_t quantization_index_segment0: 8;
        uint32_t quantization_index_segment1: 8;
        uint32_t quantization_index_segment2: 8;
        uint32_t quantization_index_segment3: 8;
    } dw5;

    struct {
        uint32_t pak_pass_num;
    } dw6;

    struct {
        uint32_t token_cost_delta_threshold: 16;
        uint32_t skip_cost_delta_threshold: 16;
    } dw7;

    struct {
        uint32_t cumulative_dqindex01;
    } dw8;

    struct {
        uint32_t cumulative_dqindex02;
    } dw9;

    struct {
        uint32_t cumulative_loop_filter01;
    } dw10;

    struct {
        uint32_t cumulative_loop_filter02;
    } dw11;

    struct {
        uint32_t pak_token_statistics_bti;
    } dw12;

    struct {
        uint32_t token_update_flags_bti;
    } dw13;

    struct {
        uint32_t entropy_cost_table_bti;
    } dw14;

    struct {
        uint32_t frame_header_bitstream_bti;
    } dw15;

    struct {
        uint32_t default_token_probability_bti;
    } dw16;

    struct {
        uint32_t picture_state_bti;
    } dw17;

    struct {
        uint32_t mpu_curbe_data_bti;
    } dw18;

    struct {
        uint32_t header_meta_data_bti;
    } dw19;

    struct {
        uint32_t token_probability_bti;
    } dw20;

    struct {
        uint32_t pak_hardware_token_probability_pass1_bti;
    } dw21;

    struct {
        uint32_t key_frame_token_probability_bti;
    } dw22;

    struct {
        uint32_t updated_token_probability_bti;
    } dw23;

    struct {
        uint32_t pak_hardware_token_probability_pass2_bti;
    } dw24;

    struct {
        uint32_t kernel_debug_dump_bti;
    } dw25;

    struct {
        uint32_t repak_decision_surface_bti;
    } dw26;
};

#define VP8_TPU                         0
#define NUM_VP8_TPU                     1

struct i965_encoder_vp8_tpu_context {
    struct i965_gpe_context gpe_contexts[NUM_VP8_TPU];
    dri_bo *dynamic_buffer;
};

struct vp8_encoder_kernel_parameters {
    unsigned int                curbe_size;
    unsigned int                inline_data_size;
    unsigned int                external_data_size;
};

enum VP8_ENCODER_WALKER_DEGREE {
    VP8_ENCODER_NO_DEGREE       = 0,
    VP8_ENCODER_45_DEGREE,
    VP8_ENCODER_26_DEGREE,
    VP8_ENCODER_46_DEGREE,
    VP8_ENCODER_45Z_DEGREE
};

struct vp8_encoder_kernel_walker_parameter {
    unsigned int                walker_degree;
    unsigned int                use_scoreboard;
    unsigned int                scoreboard_mask;
    unsigned int                no_dependency;
    unsigned int                resolution_x;
    unsigned int                resolution_y;
};

struct vp8_encoder_scoreboard_parameters {
    unsigned int                mask;
    unsigned int                type;
    unsigned int                enable;
};

#define VP8_BRC_HISTORY_BUFFER_SIZE     704

#define VP8_BRC_SINGLE_PASS             1 /* No IPCM case */
#define VP8_BRC_MINIMUM_NUM_PASSES      2 /* 2 to cover IPCM case */
#define VP8_BRC_DEFAULT_NUM_PASSES      4
#define VP8_BRC_MAXIMUM_NUM_PASSES      7

#define VP8_BRC_IMG_STATE_SIZE_PER_PASS 128

#define VP8_BRC_CONSTANT_DATA_SIZE      2880

struct vp8_brc_pak_statistics {
    // DWORD 0
    struct {
        uint32_t bitstream_byte_count_per_frame;
    } dw0;

    // DWORD 1
    struct {
        uint32_t bitstream_byte_count_frame_no_headers;
    } dw1;

    // DWORD 2
    struct {
        uint32_t num_of_pak_passes_executed: 16;
        uint32_t reserved: 16;
    } dw2;

    // DWORD 3
    struct {
        uint32_t previous_qp: 32;
    } dw3;

    // DWORD 4 - 1st pass IMAGE_STATUS_CONTROL_MMIO
    struct {
        uint32_t max_macroblock_conformance_flag: 1;
        uint32_t frame_bit_count_over_underflow: 1;
        uint32_t reserved0: 14;
        uint32_t suggested_slice_qp_delta: 8;
        uint32_t reserved1: 8;
    } dw4;

    // DWORD 5 - 2nd pass IMAGE_STATUS_CONTROL_MMIO
    struct {
        uint32_t max_macroblock_conformance_flag: 1;
        uint32_t frame_bit_count_over_underflow: 1;
        uint32_t reserved0: 14;
        uint32_t suggested_slice_qp_delta: 8;
        uint32_t reserved1: 8;
    } dw5;

    // DWORD 6 - 3rd pass IMAGE_STATUS_CONTROL_MMIO
    struct {
        uint32_t max_macroblock_conformance_flag: 1;
        uint32_t frame_bit_count_over_underflow: 1;
        uint32_t reserved0: 14;
        uint32_t suggested_slice_qp_delta: 8;
        uint32_t reserved1: 8;
    } dw6;

    // DWORD 7 - 4th pass IMAGE_STATUS_CONTROL_MMIO
    struct {
        uint32_t max_macroblock_conformance_flag: 1;
        uint32_t frame_bit_count_over_underflow: 1;
        uint32_t reserved0: 14;
        uint32_t suggested_slice_qp_delta: 8;
        uint32_t reserved1: 8;
    } dw7;

    // DWORD 8 - 5th pass IMAGE_STATUS_CONTROL_MMIO
    struct {
        uint32_t max_macroblock_conformance_flag: 1;
        uint32_t frame_bit_count_over_underflow: 1;
        uint32_t reserved0: 14;
        uint32_t suggested_slice_qp_delta: 8;
        uint32_t reserved1: 8;
    } dw8;

    // DWORD 9 - 6th pass IMAGE_STATUS_CONTROL_MMIO
    struct {
        uint32_t max_macroblock_conformance_flag: 1;
        uint32_t frame_bit_count_over_underflow: 1;
        uint32_t reserved0: 14;
        uint32_t suggested_slice_qp_delta: 8;
        uint32_t reserved1: 8;
    } dw9;

    // DWORD 10 - 7th pass IMAGE_STATUS_CONTROL_MMIO
    struct {
        uint32_t max_macroblock_conformance_flag: 1;
        uint32_t frame_bit_count_over_underflow: 1;
        uint32_t reserved0: 14;
        uint32_t suggested_slice_qp_delta: 8;
        uint32_t reserved1: 8;
    } dw10;

    // DWORD 11
    struct {
        uint32_t reserved;
    } dw11;

    struct {
        uint32_t reserved;
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
};

enum vp8_media_state_type {
    VP8_MEDIA_STATE_ENC_I_FRAME_CHROMA = 0,
    VP8_MEDIA_STATE_ENC_I_FRAME_LUMA,
    VP8_MEDIA_STATE_ENC_I_FRAME_DIST,
    VP8_MEDIA_STATE_ENC_P_FRAME,
    VP8_MEDIA_STATE_16X_SCALING,
    VP8_MEDIA_STATE_4X_SCALING,
    VP8_MEDIA_STATE_16X_ME,
    VP8_MEDIA_STATE_4X_ME,
    VP8_MEDIA_STATE_BRC_INIT_RESET,
    VP8_MEDIA_STATE_BRC_UPDATE,
    VP8_MEDIA_STATE_MPU,
    VP8_MEDIA_STATE_TPU,
    VP8_NUM_MEDIA_STATES
};

struct vp8_vdbox_image_status_control {
    uint32_t max_mb_conformance_flag: 1;
    uint32_t frame_bitcount_flag: 1;
    uint32_t panic: 1;
    uint32_t missing_huffman_code: 1;
    uint32_t reserved0: 4;
    uint32_t total_num_pass: 4;
    uint32_t reserved1: 1;
    uint32_t num_pass_polarity_change: 2;
    uint32_t cumulative_slice_qp_polarity_change: 1;
    uint32_t suggested_slice_qpdelta: 8;
    uint32_t cumulative_slice_delta_qp: 8;
};

struct vp8_encode_status {
    uint32_t bitstream_byte_count_per_frame;
    uint32_t pad0;
    uint32_t image_status_mask;
    uint32_t pad1;
    struct vp8_vdbox_image_status_control image_status_ctrl;
    uint32_t pad2;
};

struct i965_encoder_vp8_encode_status_buffer {
    dri_bo *bo;
    uint32_t base_offset;
    uint32_t size;

    uint32_t bitstream_byte_count_offset;
    uint32_t image_status_mask_offset;
    uint32_t image_status_ctrl_offset;
};

#ifndef MAX_MFX_REFERENCE_SURFACES
#define MAX_MFX_REFERENCE_SURFACES              16
#endif

struct i965_encoder_vp8_context {
    struct i965_gpe_table *gpe_table;

    struct i965_encoder_vp8_brc_init_reset_context brc_init_reset_context;
    struct i965_encoder_vp8_scaling_context scaling_context;
    struct i965_encoder_vp8_me_context me_context;
    struct i965_encoder_vp8_mbenc_context mbenc_context;
    struct i965_encoder_vp8_brc_update_context brc_update_context;
    struct i965_encoder_vp8_mpu_context mpu_context;

    struct i965_encoder_vp8_tpu_context tpu_context;

    struct i965_gpe_resource reference_frame_mb_count_buffer;
    struct i965_gpe_resource mb_mode_cost_luma_buffer;
    struct i965_gpe_resource block_mode_cost_buffer;
    struct i965_gpe_resource chroma_recon_buffer;
    struct i965_gpe_resource per_mb_quant_data_buffer;
    struct i965_gpe_resource pred_mv_data_buffer;
    struct i965_gpe_resource mode_cost_update_buffer;

    struct i965_gpe_resource brc_history_buffer;
    struct i965_gpe_resource brc_segment_map_buffer;
    struct i965_gpe_resource brc_distortion_buffer;
    struct i965_gpe_resource brc_pak_statistics_buffer;
    struct i965_gpe_resource brc_vp8_cfg_command_read_buffer;
    struct i965_gpe_resource brc_vp8_cfg_command_write_buffer;
    struct i965_gpe_resource brc_vp8_constant_data_buffer;
    struct i965_gpe_resource brc_pak_statistics_dump_buffer;

    struct i965_gpe_resource me_4x_mv_data_buffer;
    struct i965_gpe_resource me_4x_distortion_buffer;
    struct i965_gpe_resource me_16x_mv_data_buffer;

    struct i965_gpe_resource histogram_buffer;

    struct i965_gpe_resource pak_intra_row_store_scratch_buffer;
    struct i965_gpe_resource pak_deblocking_filter_row_store_scratch_buffer;
    struct i965_gpe_resource pak_mpc_row_store_scratch_buffer;
    struct i965_gpe_resource pak_stream_out_buffer;
    struct i965_gpe_resource pak_frame_header_buffer;
    struct i965_gpe_resource pak_intermediate_buffer;
    struct i965_gpe_resource pak_mpu_tpu_mode_probs_buffer;
    struct i965_gpe_resource pak_mpu_tpu_ref_mode_probs_buffer;
    struct i965_gpe_resource pak_mpu_tpu_coeff_probs_buffer;
    struct i965_gpe_resource pak_mpu_tpu_ref_coeff_probs_buffer;
    struct i965_gpe_resource pak_mpu_tpu_token_bits_data_buffer;
    struct i965_gpe_resource pak_mpu_tpu_picture_state_buffer;
    struct i965_gpe_resource pak_mpu_tpu_mpu_bitstream_buffer;
    struct i965_gpe_resource pak_mpu_tpu_tpu_bitstream_buffer;
    struct i965_gpe_resource pak_mpu_tpu_entropy_cost_table_buffer;
    struct i965_gpe_resource pak_mpu_tpu_pak_token_statistics_buffer;
    struct i965_gpe_resource pak_mpu_tpu_pak_token_update_flags_buffer;
    struct i965_gpe_resource pak_mpu_tpu_default_token_probability_buffer;
    struct i965_gpe_resource pak_mpu_tpu_key_frame_token_probability_buffer;
    struct i965_gpe_resource pak_mpu_tpu_updated_token_probability_buffer;
    struct i965_gpe_resource pak_mpu_tpu_hw_token_probability_pak_pass_2_buffer;
    struct i965_gpe_resource pak_mpu_tpu_repak_decision_buffer;

    struct i965_gpe_resource mb_coded_buffer;

    struct i965_encoder_vp8_encode_status_buffer encode_status_buffer;

    struct object_surface *ref_last_frame;
    struct object_surface *ref_gf_frame;
    struct object_surface *ref_arf_frame;

    unsigned long mv_offset;
    unsigned long mb_coded_buffer_size;

    unsigned int ref_frame_ctrl;
    unsigned int average_i_frame_qp;
    unsigned int average_p_frame_qp;

    /* TPU */
    unsigned int num_passes;
    unsigned int curr_pass;
    unsigned int repak_pass_iter_val;
    unsigned int min_pak_passes;

    unsigned int num_brc_pak_passes;

    unsigned int picture_width; /* in pixel */
    unsigned int picture_height;/* in pixel */
    unsigned int frame_width_in_mbs;
    unsigned int frame_height_in_mbs;
    unsigned int frame_width;   /* frame_width_in_mbs * 16 */
    unsigned int frame_height;  /* frame_height_in_mbs * 16 */
    unsigned int down_scaled_width_in_mb4x;
    unsigned int down_scaled_height_in_mb4x;
    unsigned int down_scaled_width_4x;
    unsigned int down_scaled_height_4x;
    unsigned int down_scaled_width_in_mb16x;
    unsigned int down_scaled_height_in_mb16x;
    unsigned int down_scaled_width_16x;
    unsigned int down_scaled_height_16x;
    unsigned int min_scaled_dimension;
    unsigned int min_scaled_dimension_in_mbs;

    unsigned int frame_type;
    unsigned int internal_rate_mode;
    unsigned short avbr_accuracy;
    unsigned short avbr_convergence;
    unsigned int frame_num;
    struct intel_fraction framerate;
    unsigned int gop_size;
    unsigned int brc_init_reset_buf_size_in_bits;
    unsigned int target_bit_rate;
    unsigned int max_bit_rate;
    unsigned int min_bit_rate;
    unsigned long init_vbv_buffer_fullness_in_bit;
    unsigned long vbv_buffer_size_in_bit;
    double brc_init_current_target_buf_full_in_bits;
    double brc_init_reset_input_bits_per_frame;

    unsigned int brc_initted: 1;
    unsigned int brc_need_reset: 1;
    unsigned int brc_mbenc_phase1_ignored: 1;
    unsigned int hme_supported: 1;
    unsigned int hme_16x_supported: 1;
    unsigned int hme_enabled            : 1;
    unsigned int hme_16x_enabled        : 1;
    unsigned int is_render_context: 1;
    unsigned int is_first_frame: 1;
    unsigned int is_first_two_frame: 1;
    unsigned int repak_supported: 1;
    unsigned int multiple_pass_brc_supported: 1;
    unsigned int use_hw_scoreboard: 1;
    unsigned int use_hw_non_stalling_scoreborad: 1;
    unsigned int ref_ctrl_optimization_done: 1;
    unsigned int brc_distortion_buffer_supported: 1;
    unsigned int brc_constant_buffer_supported: 1;
    unsigned int brc_distortion_buffer_need_reset: 1;
    unsigned int mbenc_curbe_updated_in_brc_update: 1;
    unsigned int mpu_curbe_updated_in_brc_update: 1;
    unsigned int mfx_encoder_config_command_initialized: 1;
    unsigned int tpu_curbe_updated_in_brc_update: 1;
    unsigned int tpu_required: 1;
    unsigned int submit_batchbuffer: 1;

    struct {
        dri_bo *bo;
    } post_deblocking_output;

    struct {
        dri_bo *bo;
    } pre_deblocking_output;

    struct {
        dri_bo *bo;
    } uncompressed_picture_source;

    struct {
        dri_bo *bo;
        int offset;
        int end_offset;
    } indirect_pak_bse_object;

    struct {
        dri_bo *bo;
    } deblocking_filter_row_store_scratch_buffer;

    struct {
        dri_bo *bo;
    } reference_surfaces[MAX_MFX_REFERENCE_SURFACES];

    unsigned int vdbox_idc;
    unsigned int vdbox_mmio_base;
    unsigned int idrt_entry_size;
    unsigned int mocs;
};

#endif /* I965_ENCODER_VP8_H */
