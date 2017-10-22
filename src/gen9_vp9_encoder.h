/*
 * Copyright © 2016 Intel Corporation
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
 * SOFTWAR
 *
 * Authors:
 *    Zhao, Yakui <yakui.zhao@intel.com>
 *
 */

#ifndef GEN9_VP9_ENCODER_H
#define GEN9_VP9_ENCODER_H

#include <drm.h>
#include <i915_drm.h>
#include <intel_bufmgr.h>

#include <va/va.h>
#include "i965_gpe_utils.h"

#include "vp9_probs.h"

struct encode_state;
struct intel_encoder_context;

#define KEY_FRAME        0
#define INTER_FRAME      1

#define INTEL_BRC_NONE   0
#define INTEL_BRC_CBR    1
#define INTEL_BRC_VBR    2
#define INTEL_BRC_CQP    3

typedef enum _VP9_MEDIA_STATE_TYPE {
    VP9_MEDIA_STATE_ENC_I_FRAME_DIST        = 0,
    VP9_MEDIA_STATE_32X_SCALING                ,
    VP9_MEDIA_STATE_16X_SCALING                ,
    VP9_MEDIA_STATE_4X_SCALING                 ,
    VP9_MEDIA_STATE_32X_ME                     ,
    VP9_MEDIA_STATE_16X_ME                     ,
    VP9_MEDIA_STATE_4X_ME                      ,
    VP9_MEDIA_STATE_BRC_INIT_RESET             ,
    VP9_MEDIA_STATE_BRC_UPDATE                 ,
    VP9_MEDIA_STATE_MBENC_I_32x32            ,
    VP9_MEDIA_STATE_MBENC_I_16x16            ,
    VP9_MEDIA_STATE_MBENC_P                  ,
    VP9_MEDIA_STATE_MBENC_TX                 ,
    VP9_MEDIA_STATE_DYS                    ,
    VP9_NUM_MEDIA_STATES
} VP9_MEDIA_STATE_TYPE;


enum vp9_walker_degree {
    VP9_NO_DEGREE = 0,
    VP9_26_DEGREE,
    VP9_45Z_DEGREE
};

struct vp9_encoder_kernel_parameter {
    unsigned int                curbe_size;
    unsigned int                inline_data_size;
    unsigned int                sampler_size;
};

struct vp9_encoder_scoreboard_parameter {
    unsigned int                mask;
    unsigned int                type;
    unsigned int                enable;
    unsigned int                walkpat_flag;
};

typedef enum _INTEL_VP9_ENC_OPERATION {
    INTEL_VP9_ENC_SCALING4X = 0,
    INTEL_VP9_ENC_SCALING2X,
    INTEL_VP9_ENC_ME,
    INTEL_VP9_ENC_BRC,
    INTEL_VP9_ENC_MBENC,
    INTEL_VP9_ENC_DYS
} INTEL_VP9_ENC_OPERATION;

struct gen9_surface_vp9 {
    VADriverContextP ctx;
    VASurfaceID scaled_4x_surface_id;
    struct object_surface *scaled_4x_surface_obj;
    VASurfaceID scaled_16x_surface_id;
    struct object_surface *scaled_16x_surface_obj;

    VASurfaceID dys_surface_id;
    struct object_surface *dys_surface_obj;
    VASurfaceID dys_4x_surface_id;
    struct object_surface *dys_4x_surface_obj;
    VASurfaceID dys_16x_surface_id;
    struct object_surface *dys_16x_surface_obj;
    int dys_frame_width;
    int dys_frame_height;
    int frame_width;
    int frame_height;
    unsigned int qp_value;
    uint8_t dys_hme_flag;
};

/* The definition for Scaling */
enum vp9_binding_table_offset_scaling {
    VP9_BTI_SCALING_FRAME_SRC_Y                 = 0,
    VP9_BTI_SCALING_FRAME_DST_Y                 = 1,
    VP9_BTI_SCALING_FRAME_MBVPROCSTATS_DST_CM   = 6,
    VP9_BTI_SCALING_NUM_SURFACES                = 8
};


typedef struct _vp9_scaling4x_curbe_data_cm {
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
        uint32_t reserved;
    } dw3;

    struct {
        uint32_t reserved;
    } dw4;

    struct {
        uint32_t reserved;
    } dw5;

    struct {
        uint32_t   reserved0                              : 1;
        uint32_t   enable_mb_variance_output              : 1;
        uint32_t   enable_mb_pixel_average_output         : 1;
        uint32_t   enable_blk8x8_stat_output            : 1;
        uint32_t   reserved1                             : 28;
    } dw6;

    struct {
        uint32_t reserved;
    } dw7;

    struct {
        uint32_t reserved;
    } dw8;

    struct {
        uint32_t reserved;
    } dw9;

    struct {
        uint32_t mbv_proc_stat_bti;
    } dw10;

    struct {
        uint32_t reserved;
    } dw11;
} vp9_scaling4x_curbe_data_cm;

typedef struct _vp9_scaling2x_curbe_data_cm {
    struct {
        uint32_t   input_picture_width  : 16;
        uint32_t   input_picture_height : 16;
    } dw0;

    /* dw1-dw7 */
    uint32_t reserved1[7];

    struct {
        uint32_t input_y_bti;
    } dw8;

    struct {
        uint32_t output_y_bti;
    } dw9;

    uint32_t reserved2[2];
} vp9_scaling2x_curbe_data_cm;

typedef struct _vp9_scaling4x_inline_data_cm {
    struct {
        uint32_t       dstblk_hori_origin : 16;
        uint32_t       dstblk_vert_origin : 16;
    } dw0;

    struct {
        uint32_t       horiblk_compmask_layer0 : 16;
        uint32_t       vertblk_compmask_layer0 : 16;
    } dw1;

    struct {
        uint32_t       horiblk_compmask_layer1 : 16;
        uint32_t       vertblk_compmask_layer1 : 16;
    } dw2;

    struct {
        uint32_t       horiblk_compmask_layer2 : 16;
        uint32_t       vertblk_compmask_layer2 : 16;
    } dw3;

    struct {
        float       video_xscaling_step;
    } dw4;

    struct {
        float       video_step_delta;
    } dw5;


    struct {
        uint32_t       vert_blk_num                : 17;
        uint32_t       area_interest              : 1;
        uint32_t       reserved                   : 14;
    } dw6;

    struct {
        uint32_t       grp_id_num;
    } dw7;

    struct {
        uint32_t       horiblk_compmask_layer3 : 16;
        uint32_t       vertblk_compmask_layer3 : 16;
    } dw8;

    struct {
        uint32_t       horiblk_compmask_layer4 : 16;
        uint32_t       vertblk_compmask_layer4 : 16;
    } dw9;

    struct {
        uint32_t       horiblk_compmask_layer5 : 16;
        uint32_t       vertblk_compmask_layer5 : 16;
    } dw10;

    struct {
        uint32_t       horiblk_compmask_layer6 : 16;
        uint32_t       vertblk_compmask_layer6 : 16;
    } dw11;

    struct {
        uint32_t       horiblk_compmask_layer7 : 16;
        uint32_t       vertblk_compmask_layer7 : 16;
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
} vp9_scaling4x_inline_data_cm;

#define VP9_SCALING_4X                  0
#define VP9_SCALING_2X                  1
#define NUM_VP9_SCALING                 2

typedef struct _vp9_bti_scaling_offset {
    uint32_t   scaling_frame_src_y;
    uint32_t   scaling_frame_dst_y;
    uint32_t   reserved;
    uint32_t   scaling_frame_mbv_proc_stat_dst;
} vp9_bti_scaling_offset;

struct vp9_scaling_context {
    struct i965_gpe_context gpe_contexts[NUM_VP9_SCALING];
    vp9_bti_scaling_offset scaling_4x_bti;
    vp9_bti_scaling_offset scaling_2x_bti;
};

struct gen9_search_path_delta {
    char search_path_delta_x: 4;
    char search_path_delta_y: 4;
};

struct vp9_binding_table_me {
    uint32_t   memv_data_surface_offset;
    uint32_t   memv16x_data_surface_offset;
    uint32_t   me_dist_offset;
    uint32_t   me_brc_dist_offset;
    uint32_t   me_curr_picl0_offset;
    uint32_t   me_curr_picl1_offset;
};

enum vp9_binding_table_offset_me {
    VP9_BTI_ME_MV_DATA_SURFACE              = 0,
    VP9_BTI_16XME_MV_DATA_SURFACE           = 1,
    VP9_BTI_ME_DISTORTION_SURFACE           = 2,
    VP9_BTI_ME_BRC_DISTORTION_SURFACE       = 3,
    VP9_BTI_ME_CURR_PIC_L0                  = 4,
    VP9_BTI_ME_CURR_PIC_L1                  = VP9_BTI_ME_CURR_PIC_L0 + 17,
    VP9_BTI_ME_NUM_SURFACES                 = VP9_BTI_ME_CURR_PIC_L1 + 5
};

enum VP9_ENC_ME_MODES {
    VP9_ENC_ME16X_BEFORE_ME4X       = 0,
    VP9_ENC_ME16X_ONLY              = 1,
    VP9_ENC_ME4X_ONLY               = 2,
    VP9_ENC_ME4X_AFTER_ME16X        = 3
};

typedef struct _vp9_me_curbe_data {
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
        uint32_t l0_ref_pic_polarity_bits: 8;
        uint32_t l1_ref_pic_polarity_bits: 2;
        uint32_t reserved: 22;
    } dw14;

    struct {
        uint32_t prev_mv_read_pos_factor : 8;
        uint32_t mv_shift_factor : 8;
        uint32_t reserved: 16;
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
} vp9_me_curbe_data;

struct vp9_me_context {
    struct i965_gpe_context gpe_context;
    struct vp9_binding_table_me vp9_me_bti;
};


enum vp9_binding_table_offset_mbenc {
    VP9_BTI_MBENC_CURR_Y_G9                    = 0,
    VP9_BTI_MBENC_CURR_UV_G9                   = 1,
    VP9_BTI_MBENC_CURR_NV12_G9                 = 2,
    VP9_BTI_MBENC_LAST_NV12_G9                 = 3,
    VP9_BTI_MBENC_GOLD_NV12_G9                 = 5,
    VP9_BTI_MBENC_ALTREF_NV12_G9               = 7,
    VP9_BTI_MBENC_SEGMENTATION_MAP_G9          = 8,
    VP9_BTI_MBENC_TX_CURBE_G9                  = 9,
    VP9_BTI_MBENC_HME_MV_DATA_G9               = 10,
    VP9_BTI_MBENC_HME_DISTORTION_G9            = 11,
    VP9_BTI_MBENC_MODE_DECISION_PREV_G9        = 12,
    VP9_BTI_MBENC_MODE_DECISION_G9             = 13,
    VP9_BTI_MBENC_OUT_16x16_INTER_MODES_G9     = 14,
    VP9_BTI_MBENC_CU_RECORDS_G9                = 15,
    VP9_BTI_MBENC_PAK_DATA_G9                  = 16,
    VP9_BTI_MBENC_NUM_SURFACES_G9              = 17,
};

struct vp9_binding_table_mbenc_i32 {
    uint32_t   mbenc_curr_y;
    uint32_t   mbenc_curr_uv;
    uint32_t   mbenc_segmentation_map;
    uint32_t   mbenc_mode_decision;
};

struct vp9_binding_table_mbenc_i16 {
    uint32_t   mbenc_curr_y;
    uint32_t   mbenc_curr_uv;
    uint32_t   mbenc_curr_nv12;
    uint32_t   mbenc_segmentation_map;
    uint32_t   mbenc_tx_curbe;
    uint32_t   mbenc_mode_decision;
};

struct vp9_binding_table_mbenc_p {
    uint32_t   mbenc_curr_y;
    uint32_t   mbenc_curr_uv;
    uint32_t   mbenc_curr_nv12;
    uint32_t   mbenc_lastref_pic;
    uint32_t   mbenc_goldref_pic;
    uint32_t   mbenc_altref_pic;
    uint32_t   mbenc_hme_mvdata;
    uint32_t   mbenc_hme_distortion;
    uint32_t   mbenc_segmentation_map;
    uint32_t   mbenc_tx_curbe;
    uint32_t   mbenc_mode_decision_prev;
    uint32_t   mbenc_mode_decision;
    uint32_t   mbenc_output_intermodes16x16;
};

struct vp9_binding_table_mbenc_tx {
    uint32_t   mbenc_curr_y;
    uint32_t   mbenc_curr_uv;
    uint32_t   mbenc_segmentation_map;
    uint32_t   mbenc_mode_decision;
    uint32_t   mbenc_cu_records;
    uint32_t   mbenc_pak_data;
};

typedef struct _vp9_mbenc_curbe_data {
    struct {
        uint32_t frame_width: 16;
        uint32_t frame_height: 16;
    } dw0;

    struct {
        uint32_t frame_type          : 8;
        uint32_t segmentation_enable : 8;
        uint32_t ref_frame_flags     : 8;
        uint32_t min_16for32_check   : 8;
    } dw1;

    struct {
        uint32_t multi_pred : 8;
        uint32_t len_sp     : 8;
        uint32_t search_x   : 8;
        uint32_t search_y   : 8;
    } dw2;

    struct {
        uint32_t hme_enabled : 8;
        uint32_t multi_ref_qp_check : 8;
        uint32_t disable_temp_pred : 8;
        uint32_t min_ref_for32_check : 8;
    } dw3;

    struct {
        uint32_t skip16_threshold : 16;
        uint32_t disable_mr_threshold : 16;
    } dw4;

    struct {
        uint32_t enable_mbrc : 8;
        uint32_t inter_round : 8;
        uint32_t intra_round : 8;
        uint32_t frame_qpindex : 8;
    } dw5;

    struct {
        uint32_t reserved;
    } dw6;

    struct {
        uint32_t reserved;
    } dw7;

    struct {
        uint32_t last_ref_qp : 16;
        uint32_t golden_ref_qp : 16;
    } dw8;

    struct {
        uint32_t alt_ref_qp : 16;
        uint32_t reserved : 16;
    } dw9;

    struct {
        uint32_t sum_intra_dist;
    } dw10;

    struct {
        uint32_t sum_inter_dist;
    } dw11;

    struct {
        uint32_t num_intra;
    } dw12;

    struct {
        uint32_t num_lastref;
    } dw13;

    struct {
        uint32_t num_goldref;
    } dw14;

    struct {
        uint32_t num_altref;
    } dw15;

    struct {
        uint32_t ime_search_path_delta03;
    } dw16;

    struct {
        uint32_t ime_search_path_delta47;
    } dw17;

    struct {
        uint32_t ime_search_path_delta811;
    } dw18;

    struct {
        uint32_t ime_search_path_delta1215;
    } dw19;

    struct {
        uint32_t ime_search_path_delta1619;
    } dw20;

    struct {
        uint32_t ime_search_path_delta2023;
    } dw21;

    struct {
        uint32_t ime_search_path_delta2427;
    } dw22;

    struct {
        uint32_t ime_search_path_delta2831;
    } dw23;

    struct {
        uint32_t ime_search_path_delta3235;
    } dw24;

    struct {
        uint32_t ime_search_path_delta3639;
    } dw25;

    struct {
        uint32_t ime_search_path_delta4043;
    } dw26;

    struct {
        uint32_t ime_search_path_delta4447;
    } dw27;

    struct {
        uint32_t ime_search_path_delta4851;
    } dw28;

    struct {
        uint32_t ime_search_path_delta5255;
    } dw29;

    struct {
        uint32_t reserved;
    } dw30;

    struct {
        uint32_t reserved;
    } dw31;

    /* DW 32 */
    struct {
        struct {
            uint32_t segment_qpindex : 8;
            uint32_t intra_non_dcpenalty_16x16 : 8;
            uint32_t intra_non_dcpenalty_8x8   : 8;
            uint32_t intra_non_dcpenalty_4x4   : 8;
        } dw32;

        struct {
            uint32_t intra_non_dcpenalty_32x32 : 16;
            uint32_t reserved   : 16;
        } dw33;


        struct {
            uint32_t zero_cost : 16;
            uint32_t near_cost : 16;
        } dw34;

        struct {
            uint32_t nearest_cost : 16;
            uint32_t refid_cost : 16;
        } dw35;

        struct {
            uint32_t mv_cost0 : 16;
            uint32_t mv_cost1 : 16;
        } dw36;

        struct {
            uint32_t mv_cost2 : 16;
            uint32_t mv_cost3 : 16;
        } dw37;

        struct {
            uint32_t mv_cost4 : 16;
            uint32_t mv_cost5 : 16;
        } dw38;

        struct {
            uint32_t mv_cost6 : 16;
            uint32_t mv_cost7 : 16;
        } dw39;

        struct {
            uint32_t mv_cost0 : 8;
            uint32_t mv_cost1 : 8;
            uint32_t mv_cost2 : 8;
            uint32_t mv_cost3 : 8;
        } dw40;

        struct {
            uint32_t mv_cost4 : 8;
            uint32_t mv_cost5 : 8;
            uint32_t mv_cost6 : 8;
            uint32_t mv_cost7 : 8;
        } dw41;

        struct {
            uint32_t mode_cost0 : 8;
            uint32_t mode_cost1 : 8;
            uint32_t mode_cost2 : 8;
            uint32_t mode_cost3 : 8;
        } dw42;

        struct {
            uint32_t mode_cost4 : 8;
            uint32_t mode_cost5 : 8;
            uint32_t mode_cost6 : 8;
            uint32_t mode_cost7 : 8;
        } dw43;

        struct {
            uint32_t mode_cost8 : 8;
            uint32_t mode_cost9 : 8;
            uint32_t refid_cost : 8;
            uint32_t reserved : 8;
        } dw44;

        struct {
            uint32_t mode_cost_intra32x32 : 16;
            uint32_t mode_cost_inter32x32 : 16;
        } dw45;

        struct {
            uint32_t mode_cost_intra32x16 : 16;
            uint32_t reserved : 16;
        } dw46;

        struct {
            uint32_t lambda_intra : 16;
            uint32_t lambda_inter : 16;
        } dw47;
    } segments[8];

    /*
    Segment 0: dw32 - dw47
    Segment 1 : dw48 - dw63
    Segment 2 : dw64 - dw79
    Segment 3 : dw80 - dw95
    Segment 4 : dw96 - dw111
    Segment 5 : dw112 - dw127
    Segment 6 : dw128 - dw143
    Segment 7 : dw144 - dw159
    */

    // dw160
    struct {
        uint32_t enc_curr_y_surf_bti;
    } dw160;

    struct {
        uint32_t reserved;
    } dw161;

    struct {
        uint32_t enc_curr_nv12_surf_bti;
    } dw162;

    struct {
        uint32_t reserved;
    } dw163;

    struct {
        uint32_t reserved;
    } dw164;

    struct {
        uint32_t reserved;
    } dw165;

    struct {
        uint32_t segmentation_map_bti;
    } dw166;

    struct {
        uint32_t tx_curbe_bti;
    } dw167;

    struct {
        uint32_t hme_mvdata_bti;
    } dw168;

    struct {
        uint32_t hme_distortion_bti;
    } dw169;

    struct {
        uint32_t reserved;
    } dw170;

    struct {
        uint32_t mode_decision_prev_bti;
    } dw171;

    struct {
        uint32_t mode_decision_bti;
    } dw172;

    struct {
        uint32_t output_16x16_inter_modes_bti;
    } dw173;

    struct {
        uint32_t cu_record_bti;
    } dw174;

    struct {
        uint32_t pak_data_bti;
    } dw175;
} vp9_mbenc_curbe_data;



#define    VP9_MBENC_IDX_KEY_32x32         0
#define    VP9_MBENC_IDX_KEY_16x16         1
#define    VP9_MBENC_IDX_INTER               2
#define    VP9_MBENC_IDX_TX                  3
#define    NUM_VP9_MBENC                     4

struct vp9_mbenc_context {
    struct i965_gpe_context gpe_contexts[NUM_VP9_MBENC];
    struct vp9_binding_table_mbenc_tx vp9_mbenc_tx_bti;
    struct vp9_binding_table_mbenc_i32 vp9_mbenc_i32_bti;
    struct vp9_binding_table_mbenc_i16 vp9_mbenc_i16_bti;
    struct vp9_binding_table_mbenc_p vp9_mbenc_p_bti;
    dri_bo *mbenc_bo_dys;
    int mbenc_bo_size;
};

enum vp9_binding_table_offset_dys {
    VP9_BTI_DYS_INPUT_NV12                  = 0,
    VP9_BTI_DYS_OUTPUT_Y                    = 1,
    VP9_BTI_DYS_OUTPUT_UV                   = 2,
    VP9_BTI_DYS_NUM_SURFACES                = 3
};

struct vp9_binding_table_dys {
    uint32_t   dys_input_frame_nv12;
    uint32_t   dys_output_frame_y;
    uint32_t   dys_output_frame_uv;
};

typedef struct _vp9_dys_curbe_data {
    struct {
        uint32_t input_frame_width : 16;
        uint32_t input_frame_height: 16;
    } dw0;

    struct {
        uint32_t output_frame_width : 16;
        uint32_t output_frame_height: 16;
    } dw1;

    struct {
        float delta_u;
    } dw2;

    struct {
        float delta_v;
    } dw3;

    /* DW4-15 */
    uint32_t reserved[12];

    struct {
        uint32_t input_frame_nv12_bti;
    } dw16;

    struct {
        uint32_t output_frame_y_bti;
    } dw17;

    struct {
        uint32_t avs_sample_idx;
    } dw18;
} vp9_dys_curbe_data;

// DYS kernel parameters
typedef struct _gen9_vp9_dys_kernel_param {
    uint32_t               input_width;
    uint32_t               input_height;
    uint32_t               output_width;
    uint32_t               output_height;
    struct object_surface  *input_surface;
    struct object_surface  *output_surface;
} gen9_vp9_dys_kernel_param;

struct vp9_dys_context {
    struct i965_gpe_context gpe_context;
    struct vp9_binding_table_dys vp9_dys_bti;
};

enum vp9_binding_table_offset_brc {
    VP9_BTI_BRC_SRCY4X_G9                          = 0,
    VP9_BTI_BRC_VME_COARSE_INTRA_G9                = 1,
    VP9_BTI_BRC_HISTORY_G9                         = 2,
    VP9_BTI_BRC_CONSTANT_DATA_G9                   = 3,
    VP9_BTI_BRC_DISTORTION_G9                      = 4,
    VP9_BTI_BRC_MMDK_PAK_OUTPUT_G9                 = 5,
    VP9_BTI_BRC_MBENC_CURBE_INPUT_G9               = 6,
    VP9_BTI_BRC_MBENC_CURBE_OUTPUT_G9              = 7,
    VP9_BTI_BRC_PIC_STATE_INPUT_G9                 = 8,
    VP9_BTI_BRC_PIC_STATE_OUTPUT_G9                = 9,
    VP9_BTI_BRC_SEGMENT_STATE_INPUT_G9             = 10,
    VP9_BTI_BRC_SEGMENT_STATE_OUTPUT_G9            = 11,
    VP9_BTI_BRC_BITSTREAM_SIZE_G9                  = 12,
    VP9_BTI_BRC_HFW_DATA_G9                        = 13,
    VP9_BTI_BRC_NUM_SURFACES_G9                    = 14,
};

struct vp9_binding_table_brc_intra_dist {
    uint32_t   intra_dist_src_y4xsurface;
    uint32_t   brc_intra_dist_vme_coarse_intra;
    uint32_t   brc_intra_dist_distortion_buffer;
};

struct vp9_binding_table_brc_init {
    uint32_t   brc_history_buffer;
    uint32_t   brc_distortion_buffer;
};


typedef struct _vp9_brc_curbe_data {
    struct {
        uint32_t frame_width : 16;
        uint32_t frame_height : 16;
    } dw0;

    struct {
        uint32_t frame_type : 8;
        uint32_t segmentation_enable : 8;
        uint32_t ref_frame_flags : 8;
        uint32_t num_tlevels     : 8;
    } dw1;

    struct {
        uint32_t reserved : 16;
        uint32_t intra_mode_disable : 8;
        uint32_t loop_filter_type : 8;
    } dw2;

    struct {
        uint32_t max_level_ratiot0 : 8;
        uint32_t max_level_ratiot1 : 8;
        uint32_t max_level_ratiot2 : 8;
        uint32_t max_level_ratiot3 : 8;
    } dw3;

    struct {
        uint32_t profile_level_max_frame;
    } dw4;

    struct {
        uint32_t init_buf_fullness;
    } dw5;

    struct {
        uint32_t buf_size;
    } dw6;

    struct {
        uint32_t target_bit_rate;
    } dw7;

    struct {
        uint32_t max_bit_rate;
    } dw8;

    struct {
        uint32_t min_bit_rate;
    } dw9;

    struct {
        uint32_t frame_ratem;
    } dw10;

    struct {
        uint32_t frame_rated;
    } dw11;

    struct {
        uint32_t brc_flag : 16;
        uint32_t gopp     : 16;
    } dw12;

    struct {
        uint32_t init_frame_width : 16;
        uint32_t init_frame_height : 16;
    } dw13;

    struct {
        uint32_t avbr_accuracy : 16;
        uint32_t avbr_convergence : 16;
    } dw14;

    struct {
        uint32_t min_qp : 16;
        uint32_t max_qp : 16;
    } dw15;

    struct {
        uint32_t cq_level : 16;
        uint32_t reserved : 16;
    } dw16;

    struct {
        uint32_t enable_dynamic_scaling : 16;
        uint32_t brc_overshoot_cbr_pct  : 16;
    } dw17;

    struct {
        uint32_t pframe_deviation_threshold0 : 8;
        uint32_t pframe_deviation_threshold1 : 8;
        uint32_t pframe_deviation_threshold2 : 8;
        uint32_t pframe_deviation_threshold3 : 8;
    } dw18;

    struct {
        uint32_t pframe_deviation_threshold4 : 8;
        uint32_t pframe_deviation_threshold5 : 8;
        uint32_t pframe_deviation_threshold6 : 8;
        uint32_t pframe_deviation_threshold7 : 8;
    } dw19;

    struct {
        uint32_t vbr_deviation_threshold0 : 8;
        uint32_t vbr_deviation_threshold1 : 8;
        uint32_t vbr_deviation_threshold2 : 8;
        uint32_t vbr_deviation_threshold3 : 8;
    } dw20;

    struct {
        uint32_t vbr_deviation_threshold4 : 8;
        uint32_t vbr_deviation_threshold5 : 8;
        uint32_t vbr_deviation_threshold6 : 8;
        uint32_t vbr_deviation_threshold7 : 8;
    } dw21;

    struct {
        uint32_t kframe_deviation_threshold0 : 8;
        uint32_t kframe_deviation_threshold1 : 8;
        uint32_t kframe_deviation_threshold2 : 8;
        uint32_t kframe_deviation_threshold3 : 8;
    } dw22;

    struct {
        uint32_t kframe_deviation_threshold4 : 8;
        uint32_t kframe_deviation_threshold5 : 8;
        uint32_t kframe_deviation_threshold6 : 8;
        uint32_t kframe_deviation_threshold7 : 8;
    } dw23;

    struct {
        uint32_t target_size;
    } dw24;

    struct {
        uint32_t frame_number;
    } dw25;

    struct {
        uint32_t reserved;
    } dw26;

    struct {
        uint32_t hrd_buffer_fullness_upper_limit;
    } dw27;

    struct {
        uint32_t hrd_buffer_fullness_lower_limit;
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
        uint32_t seg_delta_qp0 : 8;
        uint32_t seg_delta_qp1 : 8;
        uint32_t seg_delta_qp2 : 8;
        uint32_t seg_delta_qp3 : 8;
    } dw32;

    struct {
        uint32_t seg_delta_qp4 : 8;
        uint32_t seg_delta_qp5 : 8;
        uint32_t seg_delta_qp6 : 8;
        uint32_t seg_delta_qp7 : 8;
    } dw33;

    struct {
        uint32_t temporal_id : 8;
        uint32_t multi_ref_qp_check : 8;
        uint32_t reserved  : 16;
    } dw34;

    struct {
        uint32_t max_num_pak_passes : 8;
        uint32_t sync_async : 8;
        uint32_t overflow   : 8;
        uint32_t mbrc       : 8;
    } dw35;

    struct {
        uint32_t reserved : 16;
        uint32_t segmentation : 8;
        uint32_t reserved1   : 8;
    } dw36;

    struct {
        uint32_t cur_qpindex : 8;
        uint32_t lastref_qpindex  : 8;
        uint32_t goldref_qpindex  : 8;
        uint32_t altref_qpindex   : 8;
    } dw37;

    struct {
        uint32_t qdelta_ydc : 8;
        uint32_t qdelta_uvac  : 8;
        uint32_t qdelta_uvdc  : 8;
        uint32_t reserved   : 8;
    } dw38;

    struct {
        uint32_t reserved;
    } dw39;

    struct {
        uint32_t reserved;
    } dw40;

    struct {
        uint32_t reserved;
    } dw41;

    struct {
        uint32_t reserved;
    } dw42;

    struct {
        uint32_t reserved;
    } dw43;

    struct {
        uint32_t reserved;
    } dw44;

    struct {
        uint32_t reserved;
    } dw45;

    struct {
        uint32_t reserved;
    } dw46;

    struct {
        uint32_t reserved;
    } dw47;

    struct {
        uint32_t brc_y4x_input_bti;
    } dw48;

    struct {
        uint32_t brc_vme_coarse_intra_input_bti;
    } dw49;

    struct {
        uint32_t brc_history_buffer_bti;
    } dw50;

    struct {
        uint32_t brc_const_data_input_bti;
    } dw51;

    struct {
        uint32_t brc_distortion_bti;
    } dw52;

    struct {
        uint32_t brc_mmdk_pak_output_bti;
    } dw53;

    struct {
        uint32_t brc_enccurbe_input_bti;
    } dw54;

    struct {
        uint32_t brc_enccurbe_output_bti;
    } dw55;

    struct {
        uint32_t brc_pic_state_input_bti;
    } dw56;

    struct {
        uint32_t brc_pic_state_output_bti;
    } dw57;

    struct {
        uint32_t brc_seg_state_input_bti;
    } dw58;

    struct {
        uint32_t brc_seg_state_output_bti;
    } dw59;

    struct {
        uint32_t brc_bitstream_size_data_bti;
    } dw60;

    struct {
        uint32_t brc_hfw_data_output_bti;
    } dw61;

    struct {
        uint32_t reserved;
    } dw62;

    struct {
        uint32_t reserved;
    } dw63;
} vp9_brc_curbe_data;


#define    VP9_BRC_INTRA_DIST        0
#define    VP9_BRC_INIT              1
#define    VP9_BRC_RESET             2
#define    VP9_BRC_UPDATE            3
#define    NUM_VP9_BRC                   4

struct vp9_brc_context {
    struct i965_gpe_context gpe_contexts[NUM_VP9_BRC];
};

struct gen9_vp9_scaling_curbe_param {
    uint32_t                input_picture_width;
    uint32_t                input_picture_height;
    bool                    use_16x_scaling;
    bool                    use_32x_scaling;
    bool                    mb_variance_output_enabled;
    bool                    mb_pixel_average_output_enabled;
    bool                    blk8x8_stat_enabled;
};

struct gen9_vp9_me_curbe_param {
    VAEncSequenceParameterBufferVP9           *pseq_param;
    VAEncPictureParameterBufferVP9            *ppic_param;
    uint32_t                                   frame_width;
    uint32_t                                   frame_height;
    uint32_t                                   ref_frame_flag;
    bool                                       use_16x_me;
    bool                                       b16xme_enabled;
};

struct gen9_vp9_mbenc_curbe_param {
    VAEncSequenceParameterBufferVP9           *pseq_param;
    VAEncPictureParameterBufferVP9            *ppic_param;
    VAEncMiscParameterTypeVP9PerSegmantParam  *psegment_param;

    uint16_t                                    frame_width_in_mb;
    uint16_t                                    frame_height_in_mb;
    uint16_t                                    frame_type;
    bool                                        hme_enabled;
    uint8_t                                     ref_frame_flag;
    VP9_MEDIA_STATE_TYPE                        media_state_type;
    struct object_surface                       *curr_obj;
    struct object_surface                       *last_ref_obj;
    struct object_surface                       *golden_ref_obj;
    struct object_surface                       *alt_ref_obj;
    int                                         picture_coding_type;
    bool                                        mbenc_curbe_set_in_brc_update;
    bool                                        multi_ref_qp_check;
};

struct gen9_vp9_dys_curbe_param {
    uint32_t                                   input_width;
    uint32_t                                   input_height;
    uint32_t                                   output_width;
    uint32_t                                   output_height;
};

struct gen9_vp9_brc_curbe_param {
    VAEncSequenceParameterBufferVP9           *pseq_param;
    VAEncPictureParameterBufferVP9            *ppic_param;
    VAEncMiscParameterTypeVP9PerSegmantParam  *psegment_param;

    VASurfaceID                                curr_frame;
    uint32_t                                   picture_coding_type;
    /* the unit is in bits */
    double    *pbrc_init_current_target_buf_full_in_bits;
    double    *pbrc_init_reset_input_bits_per_frame;
    uint32_t  *pbrc_init_reset_buf_size_in_bits;
    uint32_t  frame_width;
    uint32_t  frame_height;
    uint32_t  frame_width_in_mb;
    uint32_t  frame_height_in_mb;
    uint32_t  ref_frame_flag;
    bool      hme_enabled;
    bool      initbrc;
    bool      mbbrc_enabled;
    bool      b_used_ref;
    int32_t   brc_num_pak_passes;
    bool      multi_ref_qp_check;
    int16_t   frame_number;
    VP9_MEDIA_STATE_TYPE                       media_state_type;
};

struct gen9_vp9_scaling_surface_param {
    VASurfaceID                         curr_pic;
    void                                *p_scaling_bti;
    struct object_surface               *input_surface;
    struct object_surface               *output_surface;
    uint32_t                            input_frame_width;
    uint32_t                            input_frame_height;
    uint32_t                            output_frame_width;
    uint32_t                            output_frame_height;
    uint32_t                            vert_line_stride;
    uint32_t                            vert_line_stride_offset;
    bool                                scaling_out_use_16unorm_surf_fmt;
    bool                                scaling_out_use_32unorm_surf_fmt;
    bool                                mbv_proc_stat_enabled;
    struct i965_gpe_resource            *pres_mbv_proc_stat_buffer;
};

struct gen9_vp9_brc_init_constant_buffer_param {
    struct i965_gpe_resource               *pres_brc_const_data_buffer;
    uint16_t                               picture_coding_type;
};

struct gen9_vp9_dys_surface_param {
    struct object_surface                  *input_frame;
    struct object_surface                  *output_frame;
    uint32_t                               vert_line_stride;
    uint32_t                               vert_line_stride_offset;
};

struct gen9_vp9_me_surface_param {
    VASurfaceID                             curr_pic;
    struct object_surface                   *last_ref_pic;
    struct object_surface                   *golden_ref_pic;
    struct object_surface                   *alt_ref_pic;

    struct i965_gpe_resource                *pres_4x_memv_data_buffer;
    struct i965_gpe_resource                *pres_16x_memv_data_buffer;
    struct i965_gpe_resource                *pres_me_distortion_buffer;
    struct i965_gpe_resource                *pres_me_brc_distortion_buffer;
    uint32_t                                downscaled_width_in_mb;
    uint32_t                                downscaled_height_in_mb;
    uint32_t                                frame_width;
    uint32_t                                frame_height;
    bool                                    use_16x_me;
    bool                                    b16xme_enabled;
    bool                                    dys_enabled;
};

struct gen9_vp9_mbenc_surface_param {
    int                                 media_state_type;
    struct object_surface               *last_ref_obj;
    struct object_surface               *golden_ref_obj;
    struct object_surface               *alt_ref_obj;
    struct object_surface               *curr_frame_obj;
    unsigned short                      picture_coding_type;
    unsigned int                        curr_surface_offset;
    struct i965_gpe_resource            *ps4x_memv_data_buffer;
    struct i965_gpe_resource            *ps4x_memv_distortion_buffer;
    struct i965_gpe_resource            *ps_me_brc_distortion_buffer;
    uint32_t                            frame_width;
    uint32_t                            frame_height;
    uint32_t                            frame_width_in_mb;
    uint32_t                            frame_height_in_mb;
    bool                                hme_enabled;
    bool                                segmentation_enabled;
    uint32_t                            mb_data_offset;
    struct i965_gpe_resource            *pres_mb_code_surface;
    struct i965_gpe_resource            *pres_segmentation_map;
    struct i965_gpe_resource            *pres_mode_decision_prev;
    struct i965_gpe_resource            *pres_mode_decision;
    struct i965_gpe_resource            *pres_mbenc_curbe_buffer;
    struct i965_gpe_resource            *pres_output_16x16_inter_modes;
    struct i965_gpe_resource            *pres_mode_decision_i32;

    struct i965_gpe_context              *gpe_context_tx;
};

typedef struct _vp9_frame_status_ {
    uint16_t frame_width;
    uint16_t frame_height;
    uint8_t frame_type;
    uint8_t show_frame;
    uint8_t refresh_frame_context;
    uint8_t frame_context_idx;
    uint8_t intra_only;
} vp9_frame_status;

struct gen9_hcpe_pipe_mode_select_param {
    uint32_t                    codec_mode;
    uint32_t                    stream_out;
};

typedef struct _hcp_surface_state {
    struct {
        uint32_t surface_pitch : 17;
        uint32_t reserved      : 11;
        uint32_t surface_id    : 4;
    } dw1;

    struct {
        uint32_t y_cb_offset    : 15;
        uint32_t reserved       : 12;
        uint32_t surface_format : 5;
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
} hcp_surface_state;

struct gen9_encoder_context_vp9 {
    struct vp9_scaling_context scaling_context;
    struct vp9_me_context me_context;
    struct vp9_mbenc_context mbenc_context;
    struct vp9_brc_context brc_context;
    struct vp9_dys_context   dys_context;
    void *enc_priv_state;

    struct i965_gpe_resource            res_brc_history_buffer;
    struct i965_gpe_resource            res_brc_const_data_buffer;
    struct i965_gpe_resource            res_brc_mmdk_pak_buffer;
    struct i965_gpe_resource            res_brc_mbenc_curbe_write_buffer;
    struct i965_gpe_resource            res_pic_state_brc_read_buffer;
    struct i965_gpe_resource            res_pic_state_brc_write_hfw_read_buffer;
    struct i965_gpe_resource            res_pic_state_hfw_write_buffer;
    struct i965_gpe_resource            res_seg_state_brc_read_buffer;
    struct i965_gpe_resource            res_seg_state_brc_write_buffer;
    struct i965_gpe_resource            res_brc_bitstream_size_buffer;
    struct i965_gpe_resource            res_brc_hfw_data_buffer;

    struct i965_gpe_resource            s4x_memv_distortion_buffer;
    struct i965_gpe_resource            mb_segment_map_surface;
    struct i965_gpe_resource            s4x_memv_data_buffer;
    struct i965_gpe_resource            s16x_memv_data_buffer;
    struct i965_gpe_resource            res_mode_decision[2];
    struct i965_gpe_resource            res_output_16x16_inter_modes;
    struct i965_gpe_resource            res_mb_code_surface;

    /* PAK resource */
    struct i965_gpe_resource            res_hvd_line_buffer;
    struct i965_gpe_resource            res_hvd_tile_line_buffer;
    struct i965_gpe_resource            res_deblocking_filter_line_buffer;
    struct i965_gpe_resource            res_deblocking_filter_tile_line_buffer;
    struct i965_gpe_resource            res_deblocking_filter_tile_col_buffer;

    struct i965_gpe_resource            res_metadata_line_buffer;
    struct i965_gpe_resource            res_metadata_tile_line_buffer;
    struct i965_gpe_resource            res_metadata_tile_col_buffer;

    struct i965_gpe_resource            res_segmentid_buffer;
    struct i965_gpe_resource            res_prob_buffer;
    struct i965_gpe_resource            res_prob_delta_buffer;
    struct i965_gpe_resource            res_prob_counter_buffer;

    struct i965_gpe_resource            res_compressed_input_buffer;
    struct i965_gpe_resource            res_tile_record_streamout_buffer;
    struct i965_gpe_resource            res_cu_stat_streamout_buffer;
    struct i965_gpe_resource            res_mv_temporal_buffer[2];
    struct i965_gpe_resource            res_pak_uncompressed_input_buffer;

    char *frame_header_data;

    unsigned int use_hw_scoreboard;
    unsigned int use_hw_non_stalling_scoreboard;
    unsigned int mb_stats_supported;
    unsigned int hme_supported;
    unsigned int b32xme_supported;

    void (*pfn_set_sample_state_dys)(
        VADriverContextP ctx,
        struct encode_state *encode_state,
        struct i965_gpe_context *gpe_context,
        struct intel_encoder_context *encoder_context);

    void (*pfn_set_curbe_mbenc)(
        VADriverContextP ctx,
        struct encode_state *encode_state,
        struct i965_gpe_context *gpe_context,
        struct intel_encoder_context *encoder_context,
        struct gen9_vp9_mbenc_curbe_param *param);

    void (*pfn_set_curbe_me)(
        VADriverContextP ctx,
        struct encode_state *encode_state,
        struct i965_gpe_context *gpe_context,
        struct intel_encoder_context *encoder_context,
        struct gen9_vp9_me_curbe_param *param);

    void (*pfn_set_curbe_scaling)(
        VADriverContextP ctx,
        struct encode_state *encode_state,
        struct i965_gpe_context *gpe_context,
        struct intel_encoder_context *encoder_context,
        struct gen9_vp9_scaling_curbe_param *param);

    void (*pfn_set_curbe_dys)(
        VADriverContextP ctx,
        struct encode_state *encode_state,
        struct i965_gpe_context *gpe_context,
        struct intel_encoder_context *encoder_context,
        struct gen9_vp9_dys_curbe_param *param);

    void (*pfn_set_curbe_brc)(
        VADriverContextP ctx,
        struct encode_state *encode_state,
        struct i965_gpe_context *gpe_context,
        struct intel_encoder_context *encoder_context,
        struct gen9_vp9_brc_curbe_param *param);

    void (*pfn_send_me_surface)(
        VADriverContextP ctx,
        struct encode_state *encode_state,
        struct i965_gpe_context *gpe_context,
        struct intel_encoder_context *encoder_context,
        struct gen9_vp9_me_surface_param *param);

    void (*pfn_send_mbenc_surface)(
        VADriverContextP ctx,
        struct encode_state *encode_state,
        struct i965_gpe_context *gpe_context,
        struct intel_encoder_context *encoder_context,
        struct gen9_vp9_mbenc_surface_param *mbenc_param);

    void (*pfn_send_scaling_surface)(
        VADriverContextP ctx,
        struct encode_state *encode_state,
        struct i965_gpe_context *gpe_context,
        struct intel_encoder_context *encoder_context,
        struct gen9_vp9_scaling_surface_param *param);

    void (*pfn_send_dys_surface)(
        VADriverContextP ctx,
        struct encode_state *encode_state,
        struct i965_gpe_context *gpe_context,
        struct intel_encoder_context *encoder_context,
        struct gen9_vp9_dys_surface_param *param);

    struct {
        dri_bo *bo;
    } reconstructed_object;

    struct {
        dri_bo *bo;
    } uncompressed_picture_source;

    struct {
        dri_bo *bo;
        int offset;
        int end_offset;
        int status_offset;
    } indirect_pak_bse_object;

    struct {
        dri_bo *bo;
    } reference_surfaces[8];
};

enum INTEL_ENC_VP9_TU_MODE {
    INTEL_ENC_VP9_TU_QUALITY      = 0,
    INTEL_ENC_VP9_TU_NORMAL,
    INTEL_ENC_VP9_TU_PERFORMANCE
};

#define VP9_LAST_REF           0x01
#define VP9_GOLDEN_REF         0x02
#define VP9_ALT_REF            0x04

struct vp9_encode_status {
    uint32_t bs_byte_count;
    uint32_t image_status_mask;
    uint32_t image_status_ctrl;
    uint32_t media_index;
};

struct vp9_encode_status_buffer_internal {
    uint32_t bs_byte_count_offset;
    uint32_t reserved[15];

    uint32_t image_status_mask_offset;
    uint32_t image_status_ctrl_offset;

    uint32_t vp9_image_mask_reg_offset;
    uint32_t vp9_image_ctrl_reg_offset;
    uint32_t vp9_bs_frame_reg_offset;
    dri_bo *bo;

    uint32_t media_index_offset;
};


struct gen9_vp9_state {
    unsigned int brc_inited;
    unsigned int brc_reset;
    unsigned int brc_enabled;
    unsigned int use_hw_scoreboard;
    unsigned int use_hw_non_stalling_scoreborad;
    unsigned int hme_supported;
    unsigned int b16xme_supported;
    unsigned int hme_enabled;
    unsigned int b16xme_enabled;

    unsigned int frame_width;
    unsigned int frame_height;
    unsigned int frame_width_in_mb;
    unsigned int frame_height_in_mb;
    unsigned int frame_width_4x;
    unsigned int frame_height_4x;
    unsigned int frame_width_16x;
    unsigned int frame_height_16x;
    unsigned int downscaled_width_4x_in_mb;
    unsigned int downscaled_height_4x_in_mb;
    unsigned int downscaled_width_16x_in_mb;
    unsigned int downscaled_height_16x_in_mb;

    unsigned int res_width;
    unsigned int res_height;
    int          brc_allocated;
    VASurfaceID  curr_frame;
    VASurfaceID  last_ref_pic;
    VASurfaceID  alt_ref_pic;
    VASurfaceID  golden_ref_pic;

    struct object_surface                       *input_surface_obj;
    struct object_surface                       *last_ref_obj;
    struct object_surface                       *golden_ref_obj;
    struct object_surface                       *alt_ref_obj;

    VAEncSequenceParameterBufferVP9 *seq_param;
    VAEncSequenceParameterBufferVP9 bogus_seq_param;
    VAEncPictureParameterBufferVP9  *pic_param;
    VAEncMiscParameterTypeVP9PerSegmantParam *segment_param;
    double   brc_init_current_target_buf_full_in_bits;
    double   brc_init_reset_input_bits_per_frame;
    uint32_t brc_init_reset_buf_size_in_bits;
    unsigned int gop_size;
    unsigned int target_bit_rate;
    unsigned int max_bit_rate;
    unsigned int min_bit_rate;
    unsigned long init_vbv_buffer_fullness_in_bit;
    unsigned long vbv_buffer_size_in_bit;
    int      frame_number;
    struct intel_fraction framerate;
    uint8_t  ref_frame_flag;
    uint8_t  dys_ref_frame_flag;
    uint8_t  picture_coding_type;
    unsigned int adaptive_transform_decision_enabled;
    int      curr_mode_decision_index;
    int      target_usage;
    unsigned int mb_data_offset;
    int      curr_pak_pass;
    bool     first_frame;
    bool     dys_enabled;
    bool     dys_in_use;
    bool     mbenc_curbe_set_in_brc_update;
    bool     multi_ref_qp_check;
    bool     brc_distortion_buffer_supported;
    bool     brc_constant_buffer_supported;
    bool     mbenc_keyframe_dist_enabled;
    unsigned int curr_mv_temporal_index;
    int      tx_mode;
    int      num_pak_passes;
    char     *alias_insert_data;
    int      header_length;
    vp9_header_bitoffset frame_header;

    struct vp9_encode_status_buffer_internal status_buffer;

    /* the frame context related with VP9 encoding */
    FRAME_CONTEXT vp9_frame_ctx[FRAME_CONTEXTS];
    FRAME_CONTEXT vp9_current_fc;
    int frame_ctx_idx;

    vp9_frame_status vp9_last_frame;
};

struct vp9_compressed_element {
    uint8_t a_valid          : 1;
    uint8_t a_probdiff_select: 1;
    uint8_t a_prob_select    : 1;
    uint8_t a_bin            : 1;
    uint8_t b_valid          : 1;
    uint8_t b_probdiff_select: 1;
    uint8_t b_prob_select    : 1;
    uint8_t b_bin            : 1;
};

#define VP9_BRC_HISTORY_BUFFER_SIZE             768
#define VP9_BRC_CONSTANTSURFACE_SIZE            17792
#define VP9_BRC_BITSTREAM_SIZE_BUFFER_SIZE      16
#define VP9_BRC_MMDK_PAK_BUFFER_SIZE            64
#define VP9_SEGMENT_STATE_BUFFER_SIZE           256
#define VP9_HFW_BRC_DATA_BUFFER_SIZE            32

#endif /* GEN9_VP9_ENCODER_H */
