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

#ifndef GEN10_HEVC_ENCODER_H
#define GEN10_HEVC_ENCODER_H

#include <drm.h>
#include <i915_drm.h>
#include <intel_bufmgr.h>

#include <va/va.h>
#include "i965_gpe_utils.h"

#define GEN10_HEVC_REGION_START_Y_OFFSET    32

#define GEN10_HEVC_LOG2_MAX_HEVC_LCU        6
#define GEN10_HEVC_LOG2_MIN_HEVC_LCU        4

#define GEN10_MAX_REF_SURFACES              8

#define GEN10_HEVC_NUM_MAX_REF_L0           3
#define GEN10_HEVC_NUM_MAX_REF_L1           1

#define GEN10_HEVC_VME_REF_WIN              64

#define GEN10_HEVC_MAX_LCU_SIZE             64

#define BYTES2UINT32(a)      (a / sizeof(uint32_t))

enum GEN10_HEVC_MEDIA_STATE_TYPE {
    GEN10_HEVC_MEDIA_STATE_BRC_INIT_RESET = 0,
    GEN10_HEVC_MEDIA_STATE_BRC_UPDATE     = 1,
    GEN10_HEVC_MEDIA_STATE_BRC_LCU_UPDATE = 2,
    GEN10_HEVC_MEDIA_STATE_MBENC_INTRA    = 3,
    GEN10_HEVC_MEDIA_STATE_MBENC_LCU32    = 4,
    GEN10_HEVC_MEDIA_STATE_MBENC_LCU64    = 5,
    GEN10_HEVC_MEDIA_STATE_4XME           = 6,
    GEN10_HEVC_MEDIA_STATE_16XME          = 7,
    GEN10_HEVC_MEDIA_STATE_NO_SCALING     = 8,
    GEN10_HEVC_MEDIA_STATE_2X_SCALING     = 9,
    GEN10_HEVC_MEDIA_STATE_4X_SCALING     = 10,
    GEN10_HEVC_MEDIA_STATE_2X_4X_SCALING  = 11,
    GEN10_HEVC_MEDIA_STATE_16X_SCALING    = 12
};

struct gen10_hevc_enc_kernel_walker_parameter {
    unsigned int use_scoreboard;
    unsigned int no_dependency;
    unsigned int use_vertical_scan;
    unsigned int use_custom_walker;
    unsigned int walker_degree;
    unsigned int resolution_x;
    unsigned int resolution_y;
};

struct gen10_hevc_enc_kernel_parameter {
    uint32_t curbe_size;
    uint32_t inline_data_size;
    uint32_t sampler_size;
};

struct gen10_hevc_enc_scoreboard_parameter {
    uint32_t mask;
    uint32_t type;
    uint32_t enable;
    uint32_t no_dependency;
};

struct gen10_hevc_scaling_conversion_param {
    struct object_surface *input_surface;
    struct object_surface *converted_output_surface;
    struct object_surface *scaled_2x_surface;
    struct object_surface *scaled_4x_surface;
    uint32_t input_width;
    uint32_t input_height;
    uint32_t output_2x_width;
    uint32_t output_2x_height;
    uint32_t output_4x_width;
    uint32_t output_4x_height;
    struct {
        uint32_t  ds_type       : 4;
        uint32_t  reserved0     : 12;
        uint32_t  conv_enable   : 1;
        uint32_t  dump_enable   : 1;
        uint32_t  is_64lcu      : 1;
        uint32_t  reserved1     : 13;
    } scale_flag;
};

#define    GEN10_NONE_DS         0
#define    GEN10_2X_DS           1
#define    GEN10_4X_DS           2
#define    GEN10_16X_DS          3
#define    GEN10_2X_4X_DS        4
#define    GEN10_DS_MASK         0x000F

#define    GEN10_DEPTH_CONV_ENABLE    1
#define    GEN10_DEPTH_CONV_DISABLE   0

enum GEN10_HEVC_SCALING_BTI {
    GEN10_HEVC_SCALING_10BIT_Y = 0,
    GEN10_HEVC_SCALING_10BIT_UV,
    GEN10_HEVC_SCALING_8BIT_Y,
    GEN10_HEVC_SCALING_8BIT_UV,
    GEN10_HEVC_SCALING_4xDS,
    GEN10_HEVC_SCALING_MB_STATS,
    GEN10_HEVC_SCALING_2xDS,
    GEN10_HEVC_SCALING_MB_SPLIT_SURFACE,
    GEN10_HEVC_SCALING_LCU32_JOB_QUEUE_SCRATCH_SURFACE,
    GEN10_HEVC_SCALING_LCU64_JOB_QUEUE_SCRATCH_SURFACE,
    GEN10_HEVC_SCALING_LCU64_64x64_DISTORTION_SURFACE
};

enum GEN10_HEVC_HME_BTI {
    GEN10_HEVC_HME_OUTPUT_MV_DATA = 0,
    GEN10_HEVC_HME_16xINPUT_MV_DATA,
    GEN10_HEVC_HME_4xOUTPUT_DISTORTION,
    GEN10_HEVC_HME_VME_PRED_CURR_PIC_IDX0,
    GEN10_HEVC_HME_VME_PRED_FWD_PIC_IDX0,
    GEN10_HEVC_HME_VME_PRED_BWD_PIC_IDX0,
    GEN10_HEVC_HME_VME_PRED_FWD_PIC_IDX1,
    GEN10_HEVC_HME_VME_PRED_BWD_PIC_IDX1,
    GEN10_HEVC_HME_VME_PRED_FWD_PIC_IDX2,
    GEN10_HEVC_HME_VME_PRED_BWD_PIC_IDX2,
    GEN10_HEVC_HME_VME_PRED_FWD_PIC_IDX3,
    GEN10_HEVC_HME_VME_PRED_BWD_PIC_IDX3,
    GEN10_HEVC_HME_4xDS_INPUT,
    GEN10_HEVC_HME_BRC_DISTORTION,
    GEN10_HEVC_HME_MV_AND_DISTORTION_SUM
};

enum GEN10_HEVC_MBENC_INTRA_BTI {
    GEN10_HEVC_MBENC_INTRA_VME_PRED_CURR_PIC_IDX0 = 0,
    GEN10_HEVC_MBENC_INTRA_VME_PRED_FWD_PIC_IDX0,
    GEN10_HEVC_MBENC_INTRA_VME_PRED_BWD_PIC_IDX0,
    GEN10_HEVC_MBENC_INTRA_VME_PRED_FWD_PIC_IDX1,
    GEN10_HEVC_MBENC_INTRA_VME_PRED_BWD_PIC_IDX1,
    GEN10_HEVC_MBENC_INTRA_VME_PRED_FWD_PIC_IDX2,
    GEN10_HEVC_MBENC_INTRA_VME_PRED_BWD_PIC_IDX2,
    GEN10_HEVC_MBENC_INTRA_VME_PRED_FWD_PIC_IDX3,
    GEN10_HEVC_MBENC_INTRA_VME_PRED_BWD_PIC_IDX3,
    GEN10_HEVC_MBENC_INTRA_CURR_Y,
    GEN10_HEVC_MBENC_INTRA_CURR_UV,
    GEN10_HEVC_MBENC_INTRA_INTERMEDIATE_CU_RECORD,
    GEN10_HEVC_MBENC_INTRA_PAK_OBJ0,
    GEN10_HEVC_MBENC_INTRA_PAK_CU_RECORD,
    GEN10_HEVC_MBENC_INTRA_SCRATCH_SURFACE,
    GEN10_HEVC_MBENC_INTRA_CU_QP_DATA,
    GEN10_HEVC_MBENC_INTRA_CONST_DATA_LUT,
    GEN10_HEVC_MBENC_INTRA_LCU_LEVEL_DATA_INPUT,
    GEN10_HEVC_MBENC_INTRA_CONCURRENT_TG_DATA,
    GEN10_HEVC_MBENC_INTRA_BRC_COMBINED_ENC_PARAMETER_SURFACE,
    GEN10_HEVC_MBENC_INTRA_CU_SPLIT_SURFACE,
    GEN10_HEVC_MBENC_INTRA_DEBUG_DUMP
};

enum GEN10_HEVC_MBENC_INTER_LCU32_BTI {
    GEN10_HEVC_MBENC_INTER_LCU32_CURR_Y = 0,
    GEN10_HEVC_MBENC_INTER_LCU32_CURR_UV,
    GEN10_HEVC_MBENC_INTER_LCU32_ENC_CU_RECORD,
    GEN10_HEVC_MBENC_INTER_LCU32_PAK_OBJ0,
    GEN10_HEVC_MBENC_INTER_LCU32_PAK_CU_RECORD,
    GEN10_HEVC_MBENC_INTER_LCU32_VME_PRED_CURR_PIC_IDX0,
    GEN10_HEVC_MBENC_INTER_LCU32_VME_PRED_FWD_PIC_IDX0,
    GEN10_HEVC_MBENC_INTER_LCU32_VME_PRED_BWD_PIC_IDX0,
    GEN10_HEVC_MBENC_INTER_LCU32_VME_PRED_FWD_PIC_IDX1,
    GEN10_HEVC_MBENC_INTER_LCU32_VME_PRED_BWD_PIC_IDX1,
    GEN10_HEVC_MBENC_INTER_LCU32_VME_PRED_FWD_PIC_IDX2,
    GEN10_HEVC_MBENC_INTER_LCU32_VME_PRED_BWD_PIC_IDX2,
    GEN10_HEVC_MBENC_INTER_LCU32_VME_PRED_FWD_PIC_IDX3,
    GEN10_HEVC_MBENC_INTER_LCU32_VME_PRED_BWD_PIC_IDX3,
    GEN10_HEVC_MBENC_INTER_LCU32_CU16x16_QP_DATA,
    GEN10_HEVC_MBENC_INTER_LCU32_ENC_CONST_TABLE,
    GEN10_HEVC_MBENC_INTER_LCU32_COLOCATED_CU_MV_DATA,
    GEN10_HEVC_MBENC_INTER_LCU32_HME_MOTION_PREDICTOR_DATA,
    GEN10_HEVC_MBENC_INTER_LCU32_LCU_LEVEL_DATA_INPUT,
    GEN10_HEVC_MBENC_INTER_LCU32_LCU_ENC_SCRATCH_SURFACE,
    GEN10_HEVC_MBENC_INTER_LCU32_CONCURRENT_TG_DATA,
    GEN10_HEVC_MBENC_INTER_LCU32_BRC_COMBINED_ENC_PARAMETER_SURFACE,
    GEN10_HEVC_MBENC_INTER_LCU32_JOB_QUEUE_SCRATCH_SURFACE,
    GEN10_HEVC_MBENC_INTER_LCU32_CU_SPLIT_DATA_SURFACE,
    GEN10_HEVC_MBENC_INTER_LCU32_RESIDUAL_DATA_SCRATCH_SURFACE,
    GEN10_HEVC_MBENC_INTER_LCU32_DEBUG_SURFACE
};

enum GEN10_HEVC_MBENC_INTER_LCU64_BTI {
    GEN10_HEVC_MBENC_INTER_LCU64_CURR_Y = 0,
    GEN10_HEVC_MBENC_INTER_LCU64_CURR_UV,
    GEN10_HEVC_MBENC_INTER_LCU64_CU32_ENC_CU_RECORD,
    GEN10_HEVC_MBENC_INTER_LCU64_SECOND_CU32_ENC_CU_RECORD,
    GEN10_HEVC_MBENC_INTER_LCU64_PAK_OBJ0,
    GEN10_HEVC_MBENC_INTER_LCU64_PAK_CU_RECORD,
    GEN10_HEVC_MBENC_INTER_LCU64_VME_PRED_CURR_PIC_IDX0,
    GEN10_HEVC_MBENC_INTER_LCU64_VME_PRED_FWD_PIC_IDX0,
    GEN10_HEVC_MBENC_INTER_LCU64_VME_PRED_BWD_PIC_IDX0,
    GEN10_HEVC_MBENC_INTER_LCU64_VME_PRED_FWD_PIC_IDX1,
    GEN10_HEVC_MBENC_INTER_LCU64_VME_PRED_BWD_PIC_IDX1,
    GEN10_HEVC_MBENC_INTER_LCU64_VME_PRED_FWD_PIC_IDX2,
    GEN10_HEVC_MBENC_INTER_LCU64_VME_PRED_BWD_PIC_IDX2,
    GEN10_HEVC_MBENC_INTER_LCU64_VME_PRED_FWD_PIC_IDX3,
    GEN10_HEVC_MBENC_INTER_LCU64_VME_PRED_BWD_PIC_IDX3,
    GEN10_HEVC_MBENC_INTER_LCU64_CU16x16_QP_DATA,
    GEN10_HEVC_MBENC_INTER_LCU64_CU32_ENC_CONST_TABLE,
    GEN10_HEVC_MBENC_INTER_LCU64_COLOCATED_CU_MV_DATA,
    GEN10_HEVC_MBENC_INTER_LCU64_HME_MOTION_PREDICTOR_DATA,
    GEN10_HEVC_MBENC_INTER_LCU64_LCU_LEVEL_DATA_INPUT,
    GEN10_HEVC_MBENC_INTER_LCU64_CU32_LCU_ENC_SCRATCH_SURFACE,
    GEN10_HEVC_MBENC_INTER_LCU64_64X64_DISTORTION_SURFACE,
    GEN10_HEVC_MBENC_INTER_LCU64_CONCURRENT_TG_DATA,
    GEN10_HEVC_MBENC_INTER_LCU64_BRC_COMBINED_ENC_PARAMETER_SURFACE,
    GEN10_HEVC_MBENC_INTER_LCU64_CU32_JOB_QUEUE_1D_SURFACE,
    GEN10_HEVC_MBENC_INTER_LCU64_CU32_JOB_QUEUE_2D_SURFACE,
    GEN10_HEVC_MBENC_INTER_LCU64_CU32_RESIDUAL_DATA_SCRATCH_SURFACE,
    GEN10_HEVC_MBENC_INTER_LCU64_CU_SPLIT_DATA_SURFACE,
    GEN10_HEVC_MBENC_INTER_LCU64_CURR_Y_2xDS,
    GEN10_HEVC_MBENC_INTER_LCU64_INTERMEDIATE_CU_RECORD,
    GEN10_HEVC_MBENC_INTER_LCU64_CONST64_DATA_LUT,
    GEN10_HEVC_MBENC_INTER_LCU64_LCU_STORAGE_SURFACE,
    GEN10_HEVC_MBENC_INTER_LCU64_VME_PRED_CURR_PIC_2xDS_IDX0,
    GEN10_HEVC_MBENC_INTER_LCU64_VME_PRED_FWD_PIC_2xDS_IDX0,
    GEN10_HEVC_MBENC_INTER_LCU64_VME_PRED_BWD_PIC_2xDS_IDX0,
    GEN10_HEVC_MBENC_INTER_LCU64_VME_PRED_FWD_PIC_2xDS_IDX1,
    GEN10_HEVC_MBENC_INTER_LCU64_VME_PRED_BWD_PIC_2xDS_IDX1,
    GEN10_HEVC_MBENC_INTER_LCU64_VME_PRED_FWD_PIC_2xDS_IDX2,
    GEN10_HEVC_MBENC_INTER_LCU64_VME_PRED_BWD_PIC_2xDS_IDX2,
    GEN10_HEVC_MBENC_INTER_LCU64_VME_PRED_FWD_PIC_2xDS_IDX3,
    GEN10_HEVC_MBENC_INTER_LCU64_VME_PRED_BWD_PIC_2xDS_IDX3,
    GEN10_HEVC_MBENC_INTER_LCU64_JOB_QUEUE_1D_SURFACE,
    GEN10_HEVC_MBENC_INTER_LCU64_JOB_QUEUE_2D_SURFACE,
    GEN10_HEVC_MBENC_INTER_LCU64_RESIDUAL_DATA_SCRATCH_SURFACE,
    GEN10_HEVC_MBENC_INTER_LCU64_DEBUG_SURFACE
};

struct gen10_scaling_context {
    struct i965_gpe_context gpe_context;
};

#define GEN10_HEVC_ME_DIST_TYPE_INTER_BRC    2
#define GEN10_HEVC_ME_DIST_TYPE_INTRA        0
#define GEN10_HEVC_ME_DIST_TYPE_INTRA_BRC    1

#define GEN10_HEVC_HME_LEVEL_4X              1
#define GEN10_HEVC_HME_LEVEL_16X             2

struct gen10_me_context {
    struct i965_gpe_context gpe_context;
};

#define GEN10_HEVC_MBENC_INTRA              0
#define GEN10_HEVC_MBENC_INTER_LCU32        1
#define GEN10_HEVC_MBENC_INTER_LCU64        2

#define GEN10_HEVC_MBENC_I_KRNIDX_G10           0
#define GEN10_HEVC_MBENC_INTER_LCU32_KRNIDX_G10 1
#define GEN10_HEVC_MBENC_INTER_LCU64_KRNIDX_G10 2
#define GEN10_HEVC_MBENC_NUM                    3

#define GEN10_HEVC_MBENC_INTRA              0
#define GEN10_HEVC_MBENC_INTER_LCU32        1
#define GEN10_HEVC_MBENC_INTER_LCU64        2

struct gen10_mbenc_context {
    struct i965_gpe_context gpe_contexts[GEN10_HEVC_MBENC_NUM];
};

#define GEN10_HEVC_BRC_HISTORY_BUFFER_SIZE        832
#define GEN10_HEVC_BRC_IMG_STATE_SIZE_PER_PASS    128

#define GEN10_HEVC_BRC_CONST_SURFACE_WIDTH        64
#define GEN10_HEVC_BRC_CONST_SURFACE_HEIGHT       35

#define GEN10_HEVC_BRC_INIT         0
#define GEN10_HEVC_BRC_RESET        1
#define GEN10_HEVC_BRC_FRAME_UPDATE 2
#define GEN10_HEVC_BRC_LCU_UPDATE   3
#define GEN10_HEVC_BRC_NUM          4

struct gen10_brc_context {
    struct i965_gpe_context gpe_contexts[GEN10_HEVC_BRC_NUM];
};

struct gen10_hevc_surface_priv {
    VADriverContextP ctx;

    VASurfaceID scaled_4x_surface_id;
    struct object_surface *scaled_4x_surface;
    struct i965_gpe_resource gpe_scaled_4x_surface;

    VASurfaceID scaled_16x_surface_id;
    struct object_surface *scaled_16x_surface;
    struct i965_gpe_resource gpe_scaled_16x_surface;

    VASurfaceID scaled_2x_surface_id;
    struct object_surface *scaled_2x_surface;
    struct i965_gpe_resource gpe_scaled_2x_surface;

    VASurfaceID converted_surface_id;
    struct object_surface *converted_surface;
    struct i965_gpe_resource gpe_converted_surface;

    struct i965_gpe_resource motion_vector_temporal;

    int frame_width;
    int frame_height;

    int width_ctb;
    int height_ctb;

    uint32_t is_10bit                  : 1;
    uint32_t is_64lcu                  : 1;
    uint32_t hme_enabled               : 1;
    uint32_t conv_scaling_done         : 1;
    uint32_t reserved                  : 28;
};

struct gen10_hevc_gpe_scoreboard {
    struct {
        uint32_t mask: 8;
        uint32_t pad: 22;
        uint32_t type: 1;
        uint32_t enable: 1;
    } scoreboard0;

    union {
        uint32_t value;
        struct {
            int32_t delta_x0: 4;
            int32_t delta_y0: 4;
            int32_t delta_x1: 4;
            int32_t delta_y1: 4;
            int32_t delta_x2: 4;
            int32_t delta_y2: 4;
            int32_t delta_x3: 4;
            int32_t delta_y3: 4;
        } scoreboard1;
    } dw1;

    union {
        uint32_t value;
        struct {
            int32_t delta_x4: 4;
            int32_t delta_y4: 4;
            int32_t delta_x5: 4;
            int32_t delta_y5: 4;
            int32_t delta_x6: 4;
            int32_t delta_y6: 4;
            int32_t delta_x7: 4;
            int32_t delta_y7: 4;
        } scoreboard2;
    } dw2;
};

struct gen10_hevc_enc_state {
    uint32_t frame_width;
    uint32_t frame_height;
    uint32_t frame_width_2x;
    uint32_t frame_height_2x;
    uint32_t frame_width_4x;
    uint32_t frame_height_4x;
    uint32_t frame_width_16x;
    uint32_t frame_height_16x;

    uint32_t use_hw_scoreboard                        : 1;
    uint32_t use_hw_non_stalling_scoreboard           : 1;
    uint32_t hme_supported                            : 1;
    uint32_t b16xme_supported                         : 1;
    uint32_t hme_enabled                              : 1;
    uint32_t b16xme_enabled                           : 1;
    uint32_t is_10bit                                 : 1;
    uint32_t is_64lcu                                 : 1;
    uint32_t is_same_ref_list                         : 1;
    uint32_t is_col_mvp_enabled                       : 1;
    uint32_t sao_2nd_needed                           : 1;
    uint32_t sao_first_pass_flag                      : 1;
    uint32_t lambda_init                              : 1;
    uint32_t rdoq_enabled                             : 1;
    uint32_t low_delay                                : 1;
    uint32_t reserved                                 : 17;

    uint32_t curr_pak_stat_index;
    uint32_t frame_number;
    uint32_t cu_records_offset;

    int thread_num_per_ctb;
    int num_regions_in_slice;

    int num_pak_passes;
    int num_sao_passes;
    int curr_pak_idx;

    uint32_t profile_level_max_frame;

    struct {
        int offset_y;
        int offset_delta;
        uint32_t num_regions;
        uint32_t max_height_in_region;
        uint32_t num_unit_in_wf;
    } hevc_wf_param;

    struct {
        uint32_t target_usage;

        double brc_init_current_target_buf_full_in_bits;
        double brc_init_reset_input_bits_per_frame;
        double brc_init_reset_buf_size_in_bits;

        uint32_t init_vbv_buffer_fullness_in_bit;
        uint32_t vbv_buffer_size_in_bit;
        int window_size;

        uint32_t target_percentage;
        uint32_t target_bit_rate;
        uint32_t max_bit_rate;
        uint32_t min_bit_rate;

        int frame_rate_m;
        int frame_rate_d;

        uint32_t gop_size;
        uint32_t gop_p;
        uint32_t gop_b;

        uint32_t lcu_brc_enabled : 1;
        uint32_t brc_inited      : 1;
        uint32_t brc_reset       : 1;
        uint32_t brc_enabled     : 1;
        uint32_t brc_method      : 8;
        uint32_t reserved        : 20;
    } brc;
};

struct gen10_hevc_enc_context {
    void *enc_priv_state;

    struct gen10_scaling_context scaling_context;
    struct gen10_me_context me_context;
    struct gen10_mbenc_context mbenc_context;
    struct gen10_brc_context brc_context;

    struct i965_gpe_resource res_mb_code_surface;

    struct i965_gpe_resource res_16x16_qp_data_surface;
    struct i965_gpe_resource res_lculevel_input_data_buffer;
    struct i965_gpe_resource res_concurrent_tg_data;
    struct i965_gpe_resource res_cu_split_surface;
    struct i965_gpe_resource res_kernel_trace_data;
    struct i965_gpe_resource res_temp_curecord_lcu32_surface;
    struct i965_gpe_resource res_temp2_curecord_lcu32_surface;
    struct i965_gpe_resource res_temp_curecord_surface_lcu64;
    struct i965_gpe_resource res_enc_scratch_buffer;
    struct i965_gpe_resource res_enc_scratch_lcu64_buffer;
    struct i965_gpe_resource res_enc_const_table_inter;
    struct i965_gpe_resource res_enc_const_table_inter_lcu64;
    struct i965_gpe_resource res_enc_const_table_intra;
    struct i965_gpe_resource res_scratch_surface;
    struct i965_gpe_resource res_jbq_header_buffer;
    struct i965_gpe_resource res_jbq_header_lcu64_buffer;
    struct i965_gpe_resource res_jbq_data_lcu32_surface;
    struct i965_gpe_resource res_jbq_data_lcu64_surface;
    struct i965_gpe_resource res_residual_scratch_lcu32_surface;
    struct i965_gpe_resource res_residual_scratch_lcu64_surface;
    struct i965_gpe_resource res_64x64_dist_buffer;
    struct i965_gpe_resource res_mb_stat_surface;
    struct i965_gpe_resource res_mb_split_surface;

    struct i965_gpe_resource res_s4x_memv_data_surface;
    struct i965_gpe_resource res_s4x_me_dist_surface;
    struct i965_gpe_resource res_s16x_memv_data_surface;
    struct i965_gpe_resource res_mv_dist_sum_buffer;

    struct i965_gpe_resource res_brc_history_buffer;
    struct i965_gpe_resource res_brc_intra_dist_surface;
    struct i965_gpe_resource res_brc_pak_statistics_buffer[2];
    struct i965_gpe_resource res_brc_pic_image_state_write_buffer;
    struct i965_gpe_resource res_brc_pic_image_state_read_buffer;
    struct i965_gpe_resource res_brc_const_data_surface;
    struct i965_gpe_resource res_brc_lcu_const_data_buffer;
    struct i965_gpe_resource res_brc_mb_qp_surface;
    struct i965_gpe_resource res_brc_input_enc_kernel_buffer;
    struct i965_gpe_resource res_brc_me_dist_surface;

    struct gen10_hevc_enc_lambda_param lambda_param;
    struct gen10_hevc_enc_frame_info frame_info;
    struct gen10_hevc_enc_common_res common_res;
    struct gen10_hevc_enc_status_buffer status_buffer;
};

#endif
