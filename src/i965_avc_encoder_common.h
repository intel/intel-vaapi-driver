/*
 * Copyright ? 2016 Intel Corporation
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
 *     Pengfei Qu <Pengfei.Qu@intel.com>
 *
 */

#ifndef _I965_AVC_ENCODER_COMMON_H
#define _I965_AVC_ENCODER_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <assert.h>
#include "intel_driver.h"

// SubMbPartMask defined in CURBE for AVC ENC
#define INTEL_AVC_DISABLE_4X4_SUB_MB_PARTITION    0x40
#define INTEL_AVC_DISABLE_4X8_SUB_MB_PARTITION    0x20
#define INTEL_AVC_DISABLE_8X4_SUB_MB_PARTITION    0x10
#define INTEL_AVC_MAX_BWD_REF_NUM    2
#define INTEL_AVC_MAX_FWD_REF_NUM    8

#define MAX_MFC_AVC_REFERENCE_SURFACES      16
#define NUM_MFC_AVC_DMV_BUFFERS             34
#define MAX_HCP_REFERENCE_SURFACES      8
#define NUM_HCP_CURRENT_COLLOCATED_MV_TEMPORAL_BUFFERS             9

#define INTEL_AVC_IMAGE_STATE_CMD_SIZE    128
#define INTEL_AVC_MIN_QP    1
#define INTEL_AVC_MAX_QP    51

#define INTEL_AVC_WP_MODE_DEFAULT 0
#define INTEL_AVC_WP_MODE_EXPLICIT 1
#define INTEL_AVC_WP_MODE_IMPLICIT 2

struct avc_param {

    // original width/height
    uint32_t frame_width_in_pixel;
    uint32_t frame_height_in_pixel;
    uint32_t frame_width_in_mbs;
    uint32_t frame_height_in_mbs;
    uint32_t frames_per_100s;
    uint32_t vbv_buffer_size_in_bit;
    uint32_t target_bit_rate;
};

typedef enum
{
    INTEL_AVC_BASE_PROFILE               = 66,
    INTEL_AVC_MAIN_PROFILE               = 77,
    INTEL_AVC_EXTENDED_PROFILE           = 88,
    INTEL_AVC_HIGH_PROFILE               = 100,
    INTEL_AVC_HIGH10_PROFILE             = 110,
    INTEL_AVC_HIGH422_PROFILE            = 122,
    INTEL_AVC_HIGH444_PROFILE            = 244,
    INTEL_AVC_CAVLC444_INTRA_PROFILE     = 44,
    INTEL_AVC_SCALABLE_BASE_PROFILE      = 83,
    INTEL_AVC_SCALABLE_HIGH_PROFILE      = 86
} INTEL_AVC_PROFILE_IDC;

typedef enum
{
    INTEL_AVC_LEVEL_1                    = 10,
    INTEL_AVC_LEVEL_11                   = 11,
    INTEL_AVC_LEVEL_12                   = 12,
    INTEL_AVC_LEVEL_13                   = 13,
    INTEL_AVC_LEVEL_2                    = 20,
    INTEL_AVC_LEVEL_21                   = 21,
    INTEL_AVC_LEVEL_22                   = 22,
    INTEL_AVC_LEVEL_3                    = 30,
    INTEL_AVC_LEVEL_31                   = 31,
    INTEL_AVC_LEVEL_32                   = 32,
    INTEL_AVC_LEVEL_4                    = 40,
    INTEL_AVC_LEVEL_41                   = 41,
    INTEL_AVC_LEVEL_42                   = 42,
    INTEL_AVC_LEVEL_5                    = 50,
    INTEL_AVC_LEVEL_51                   = 51,
    INTEL_AVC_LEVEL_52                   = 52
} INTEL_AVC_LEVEL_IDC;

struct gen9_mfx_avc_img_state
{
    union {
        struct {
            uint32_t dword_length:12;
            uint32_t pad0:4;
            uint32_t sub_opcode_b:5;
            uint32_t sub_opcode_a:3;
            uint32_t command_opcode:3;
            uint32_t pipeline:2;
            uint32_t command_type:3;
        };

        uint32_t value;
    } dw0;

    struct {
        uint32_t frame_size_in_mbs:16;//minus1
        uint32_t pad0:16;
    } dw1;

    struct {
        uint32_t frame_width_in_mbs_minus1:8; //minus1
        uint32_t pad0:8;
        uint32_t frame_height_in_mbs_minus1:8;  //minus1
        uint32_t pad1:8;
    } dw2;

    struct {
        uint32_t pad0:8;
        uint32_t image_structure:2;
        uint32_t weighted_bipred_idc:2;
        uint32_t weighted_pred_flag:1;
        uint32_t brc_domain_rate_control_enable:1;
        uint32_t pad1:2;
        uint32_t chroma_qp_offset:5;
        uint32_t pad2:3;
        uint32_t second_chroma_qp_offset:5;
        uint32_t pad3:3;
    } dw3;

    struct {
        uint32_t field_picture_flag:1;
        uint32_t mbaff_mode_active:1;
        uint32_t frame_mb_only_flag:1;
        uint32_t transform_8x8_idct_mode_flag:1;
        uint32_t direct_8x8_interface_flag:1;
        uint32_t constrained_intra_prediction_flag:1;
        uint32_t current_img_dispoable_flag:1;
        uint32_t entropy_coding_flag:1;
        uint32_t mb_mv_format_flag:1;
        uint32_t pad0:1;
        uint32_t chroma_format_idc:2;
        uint32_t mv_unpacked_flag:1;
        uint32_t insert_test_flag:1;
        uint32_t load_slice_pointer_flag:1;
        uint32_t macroblock_stat_enable:1;
        uint32_t minimum_frame_size:16;
    } dw4;

    struct {
        uint32_t intra_mb_max_bit_flag:1;
        uint32_t inter_mb_max_bit_flag:1;
        uint32_t frame_size_over_flag:1;
        uint32_t frame_size_under_flag:1;
        uint32_t pad0:3;
        uint32_t intra_mb_ipcm_flag:1;
        uint32_t pad1:1;
        uint32_t mb_rate_ctrl_flag:1;
        uint32_t min_frame_size_units:2;
        uint32_t inter_mb_zero_cbp_flag:1; //?change
        uint32_t pad2:3;
        uint32_t non_first_pass_flag:1;
        uint32_t pad3:10;
        uint32_t aq_chroma_disable:1;
        uint32_t aq_rounding:3;
        uint32_t aq_enable:1;
    } dw5;

    struct {
        uint32_t intra_mb_max_size:12;
        uint32_t pad0:4;
        uint32_t inter_mb_max_size:12;
        uint32_t pad1:4;
    } dw6;

    struct {
        uint32_t vsl_top_mb_trans8x8_flag:1;
        uint32_t pad0:31;
    } dw7;

    struct {
        uint32_t slice_delta_qp_max0:8;
        uint32_t slice_delta_qp_max1:8;
        uint32_t slice_delta_qp_max2:8;
        uint32_t slice_delta_qp_max3:8;
    } dw8;

    struct {
        uint32_t slice_delta_qp_min0:8;
        uint32_t slice_delta_qp_min1:8;
        uint32_t slice_delta_qp_min2:8;
        uint32_t slice_delta_qp_min3:8;
    } dw9;

    struct {
        uint32_t frame_bitrate_min:14;
        uint32_t frame_bitrate_min_unit_mode:1;
        uint32_t frame_bitrate_min_unit:1;
        uint32_t frame_bitrate_max:14;
        uint32_t frame_bitrate_max_unit_mode:1;
        uint32_t frame_bitrate_max_unit:1;
    } dw10;

    struct {
        uint32_t frame_bitrate_min_delta:15;
        uint32_t pad0:1;
        uint32_t frame_bitrate_max_delta:15;
        uint32_t slice_tsats_streamout_enable:1;
    } dw11;

    struct {
        uint32_t pad0:16;
        uint32_t mpeg2_old_mode_select:1;
        uint32_t vad_noa_mux_select:1;
        uint32_t vad_error_logic:1;
        uint32_t pad1:1;
        uint32_t vmd_error_logic:1;
        uint32_t pad2:11;
    } dw12;

    struct {
        uint32_t pic_qp_init_minus26:8;
        uint32_t pic_num_ref_idx_l0_active_minus1:6;
        uint32_t pad0:2;
        uint32_t pic_num_ref_idx_l1_active_minus1:6;
        uint32_t pad1:2;
        uint32_t num_ref_frames:5;
        uint32_t is_curr_pic_has_mmco5:1;
        uint32_t pad2:2;
    } dw13;

    struct {
        uint32_t pic_order_present_flag:1;
        uint32_t delta_pic_order_always_zero_flag:1;
        uint32_t pic_order_cnt_type:2;
        uint32_t pad0:4;
        uint32_t slice_group_map_type:3;
        uint32_t redundant_pic_cnt_present_flag:1;
        uint32_t num_slice_groups_minus1:3;
        uint32_t deblock_filter_ctrl_present_flag:1;
        uint32_t log2_max_frame_num_minus4:8;
        uint32_t log2_max_pic_order_cnt_lsb_minus4:8;
    } dw14;

    struct {
        uint32_t slice_group_change_rate:16;
        uint32_t curr_pic_frame_num:16;
    } dw15;

    struct {
        uint32_t current_frame_view_id:10;
        uint32_t pad0:2;
        uint32_t max_view_idx_l0:4;
        uint32_t pad1:2;
        uint32_t max_view_idx_l1:4;
        uint32_t pad2:9;
        uint32_t inter_view_order_disable:1;
    } dw16;

    struct {
        uint32_t fqp:3;                         // Must be zero for SKL
        uint32_t fqp_offset:3;                  // Must be zero for SKL
        uint32_t pad0:2;
        uint32_t ext_brc_dm_stat_en:1;          // Must be zero for SKL
        uint32_t pad1:7;
        uint32_t brc_dm_avg_mb_qp:6;            // Must be zero for SKL
        uint32_t pad2:10;
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

extern int i965_avc_get_max_mbps(int level_idc);
extern int i965_avc_calculate_initial_qp(struct avc_param * param);
extern unsigned int i965_avc_get_profile_level_max_frame(struct avc_param * param,int level_idc);
extern int i965_avc_get_max_v_mv_r(int level_idc);
extern int i965_avc_get_max_mv_len(int level_idc);
extern int i965_avc_get_max_mv_per_2mb(int level_idc);
extern unsigned short i965_avc_calc_skip_value(unsigned int enc_block_based_sip_en, unsigned int transform_8x8_flag, unsigned short skip_value);
#endif // _I965_AVC_ENCODER_COMMON_H