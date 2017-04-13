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
#include "gen9_avc_encoder.h"

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

#define AVC_NAL_DELIMITER           9

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

typedef enum {
    INTEL_AVC_BASE_PROFILE               = 66,
    INTEL_AVC_MAIN_PROFILE               = 77,
    INTEL_AVC_EXTENDED_PROFILE           = 88,
    INTEL_AVC_HIGH_PROFILE               = 100,
    INTEL_AVC_HIGH10_PROFILE             = 110
} INTEL_AVC_PROFILE_IDC;

typedef enum {
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

/*
common structure and define
*/
struct i965_avc_encoder_context {

    VADriverContextP ctx;

    /* VME resource */
    //mbbrc/brc:init/reset/update
    struct i965_gpe_resource res_brc_history_buffer;
    struct i965_gpe_resource res_brc_dist_data_surface;
    //brc:update
    struct i965_gpe_resource res_brc_pre_pak_statistics_output_buffer;
    struct i965_gpe_resource res_brc_image_state_read_buffer;
    struct i965_gpe_resource res_brc_image_state_write_buffer;
    struct i965_gpe_resource res_brc_mbenc_curbe_read_buffer;
    struct i965_gpe_resource res_brc_mbenc_curbe_write_buffer;
    struct i965_gpe_resource res_brc_const_data_buffer;
    //brc and mbbrc
    struct i965_gpe_resource res_mb_status_buffer;
    //mbbrc
    struct i965_gpe_resource res_mbbrc_mb_qp_data_surface;
    struct i965_gpe_resource res_mbbrc_roi_surface;
    struct i965_gpe_resource res_mbbrc_const_data_buffer;

    //mbenc
    struct i965_gpe_resource res_mbenc_slice_map_surface;
    struct i965_gpe_resource res_mbenc_brc_buffer;//gen95

    //scaling flatness check surface
    struct i965_gpe_resource res_flatness_check_surface;
    //me
    struct i965_gpe_resource s4x_memv_min_distortion_brc_buffer;
    struct i965_gpe_resource s4x_memv_distortion_buffer;
    struct i965_gpe_resource s4x_memv_data_buffer;
    struct i965_gpe_resource s16x_memv_data_buffer;
    struct i965_gpe_resource s32x_memv_data_buffer;


    struct i965_gpe_resource res_image_state_batch_buffer_2nd_level;
    struct intel_batchbuffer *pres_slice_batch_buffer_2nd_level;
    // mb code/data or indrirect mv data, define in private avc surface

    //sfd
    struct i965_gpe_resource res_sfd_output_buffer;
    struct i965_gpe_resource res_sfd_cost_table_p_frame_buffer;
    struct i965_gpe_resource res_sfd_cost_table_b_frame_buffer;

    //external mb qp data,application input
    struct i965_gpe_resource res_mb_qp_data_surface;

    struct i965_gpe_resource res_mad_data_buffer;

    //wp
    VASurfaceID wp_output_pic_select_surface_id[2];
    struct object_surface *wp_output_pic_select_surface_obj[2];
    struct i965_gpe_resource res_wp_output_pic_select_surface_list[2];

    //mb disable skip
    struct i965_gpe_resource res_mb_disable_skip_map_surface;

    /* PAK resource */
    //internal
    struct i965_gpe_resource res_intra_row_store_scratch_buffer;
    struct i965_gpe_resource res_deblocking_filter_row_store_scratch_buffer;
    struct i965_gpe_resource res_deblocking_filter_tile_col_buffer;
    struct i965_gpe_resource res_bsd_mpc_row_store_scratch_buffer;
    struct i965_gpe_resource res_mfc_indirect_bse_object;
    struct i965_gpe_resource res_pak_mb_status_buffer;
    struct i965_gpe_resource res_direct_mv_buffersr[NUM_MFC_AVC_DMV_BUFFERS];//INTERNAL: 0-31 as input,32 and 33 as output

    //output
    struct i965_gpe_resource res_post_deblocking_output;
    struct i965_gpe_resource res_pre_deblocking_output;

    //ref list
    struct i965_gpe_resource list_reference_res[MAX_MFC_AVC_REFERENCE_SURFACES];

    // kernel context
    struct gen_avc_scaling_context  context_scaling;
    struct gen_avc_me_context  context_me;
    struct gen_avc_brc_context  context_brc;
    struct gen_avc_mbenc_context  context_mbenc;
    struct gen_avc_wp_context  context_wp;
    struct gen_avc_sfd_context  context_sfd;

    struct encoder_status_buffer_internal status_buffer;

};

#define MAX_AVC_SLICE_NUM 256
struct avc_enc_state {

    VAEncSequenceParameterBufferH264 *seq_param;
    VAEncPictureParameterBufferH264  *pic_param;
    VAEncSliceParameterBufferH264    *slice_param[MAX_AVC_SLICE_NUM];
    VAEncMacroblockParameterBufferH264 *mb_param;

    uint32_t mad_enable: 1;
    //mb skip
    uint32_t mb_disable_skip_map_enable: 1;
    //static frame detection
    uint32_t sfd_enable: 1;
    uint32_t sfd_mb_enable: 1;
    uint32_t adaptive_search_window_enable: 1;
    //external mb qp
    uint32_t mb_qp_data_enable: 1;
    //rolling intra refresh
    uint32_t intra_refresh_i_enable: 1;
    uint32_t min_max_qp_enable: 1;
    uint32_t skip_bias_adjustment_enable: 1;

    uint32_t non_ftq_skip_threshold_lut_input_enable: 1;
    uint32_t ftq_skip_threshold_lut_input_enable: 1;
    uint32_t ftq_override: 1;
    uint32_t direct_bias_adjustment_enable: 1;
    uint32_t global_motion_bias_adjustment_enable: 1;
    uint32_t disable_sub_mb_partion: 1;
    uint32_t arbitrary_num_mbs_in_slice: 1;
    uint32_t adaptive_transform_decision_enable: 1;
    uint32_t skip_check_disable: 1;
    uint32_t tq_enable: 1;
    uint32_t enable_avc_ildb: 1;
    uint32_t suppress_recon_enable: 1;
    uint32_t flatness_check_supported: 1;
    uint32_t transform_8x8_mode_enable: 1;
    uint32_t caf_supported: 1;
    uint32_t mb_status_enable: 1;
    uint32_t mbaff_flag: 1;
    uint32_t enable_force_skip: 1;
    uint32_t rc_panic_enable: 1;
    uint32_t reserved0: 7;

    //generic begin
    uint32_t ref_pic_select_list_supported: 1;
    uint32_t mb_brc_supported: 1;
    uint32_t multi_pre_enable: 1;
    uint32_t ftq_enable: 1;
    uint32_t caf_enable: 1;
    uint32_t caf_disable_hd: 1;
    uint32_t skip_bias_adjustment_supported: 1;

    uint32_t adaptive_intra_scaling_enable: 1;
    uint32_t old_mode_cost_enable: 1;
    uint32_t multi_ref_qp_enable: 1;
    uint32_t weighted_ref_l0_enable: 1;
    uint32_t weighted_ref_l1_enable: 1;
    uint32_t weighted_prediction_supported: 1;
    uint32_t brc_split_enable: 1;
    uint32_t slice_level_report_supported: 1;

    uint32_t fbr_bypass_enable: 1;
    //mb status output in scaling kernel
    uint32_t field_scaling_output_interleaved: 1;
    uint32_t mb_variance_output_enable: 1;
    uint32_t mb_pixel_average_output_enable: 1;
    uint32_t rolling_intra_refresh_enable: 1;
    uint32_t mbenc_curbe_set_in_brc_update: 1;
    //rounding
    uint32_t rounding_inter_enable: 1;
    uint32_t adaptive_rounding_inter_enable: 1;

    uint32_t mbenc_i_frame_dist_in_use: 1;
    uint32_t mb_status_supported: 1;
    uint32_t mb_vproc_stats_enable: 1;
    uint32_t flatness_check_enable: 1;
    uint32_t block_based_skip_enable: 1;
    uint32_t use_widi_mbenc_kernel: 1;
    uint32_t kernel_trellis_enable: 1;
    uint32_t generic_reserved: 1;
    //generic end

    //rounding
    uint32_t rounding_value;
    uint32_t rounding_inter_p;
    uint32_t rounding_inter_b;
    uint32_t rounding_inter_b_ref;

    //min,max qp
    uint8_t  min_qp_i;
    uint8_t  max_qp_i;
    uint8_t  min_qp_p;
    uint8_t  max_qp_p;
    uint8_t  min_qp_b;
    uint8_t  max_qp_b;

    uint8_t  non_ftq_skip_threshold_lut[52];
    uint8_t  ftq_skip_threshold_lut[52];
    uint32_t  lamda_value_lut[52][2];


    uint32_t intra_refresh_qp_threshold;
    uint32_t trellis_flag;
    uint32_t hme_mv_cost_scaling_factor;
    uint32_t slice_height;//default 1
    uint32_t slice_num;//default 1
    uint32_t dist_scale_factor_list0[32];
    uint32_t bi_weight;
    uint32_t brc_const_data_surface_width;
    uint32_t brc_const_data_surface_height;

    uint32_t num_refs[2];
    uint32_t list_ref_idx[2][32];
    int32_t top_field_poc[NUM_MFC_AVC_DMV_BUFFERS];

    uint32_t tq_rounding;

    uint32_t zero_mv_threshold; //sfd

    uint32_t slice_second_levle_batch_buffer_in_use;
    uint32_t slice_batch_offset[MAX_AVC_SLICE_NUM];

    //gen95
    uint32_t decouple_mbenc_curbe_from_brc_enable : 1;
    uint32_t extended_mv_cost_range_enable : 1;
    uint32_t lambda_table_enable : 1;
    uint32_t reserved_g95 : 30;
    uint32_t mbenc_brc_buffer_size;

};

extern int i965_avc_get_max_mbps(int level_idc);
extern int i965_avc_calculate_initial_qp(struct avc_param * param);
extern unsigned int i965_avc_get_profile_level_max_frame(struct avc_param * param, int level_idc);
extern int i965_avc_get_max_v_mv_r(int level_idc);
extern int i965_avc_get_max_mv_len(int level_idc);
extern int i965_avc_get_max_mv_per_2mb(int level_idc);
extern unsigned short i965_avc_calc_skip_value(unsigned int enc_block_based_sip_en, unsigned int transform_8x8_flag, unsigned short skip_value);
#endif // _I965_AVC_ENCODER_COMMON_H
