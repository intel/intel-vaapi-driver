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
 *    Pengfei Qu <Pengfei.qu@intel.com>
 *
 */

#ifndef GEN9_AVC_const_DEF_H
#define GEN9_AVC_const_DEF_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define GEN9_AVC_NUM_REF_CHECK_WIDTH      3840
#define GEN9_AVC_NUM_REF_CHECK_HEIGHT     2160
#define GEN9_AVC_MBENC_CURBE_SIZE    88
#define AVC_QP_MAX    52
#define PRESET_NUM    8
#define GEN95_AVC_MAX_LAMBDA              0xEFFF
#define GEN95_AVC_DEFAULT_TRELLIS_QUANT_INTRA_ROUNDING      5

extern const char gen9_avc_sfd_cost_table_p_frame[AVC_QP_MAX];
extern const char gen9_avc_sfd_cost_table_b_frame[AVC_QP_MAX];

extern const unsigned int gen9_avc_old_intra_mode_cost[AVC_QP_MAX];
extern const unsigned int gen9_avc_mv_cost_p_skip_adjustment[AVC_QP_MAX];
extern const unsigned short gen9_avc_skip_value_p[2][2][64];
extern const unsigned short gen9_avc_skip_value_b[2][2][64];

// QP is from 0 - 51, pad it to 64 since BRC needs array size to be 64 bytes
extern const unsigned char gen9_avc_adaptive_intra_scaling_factor[64];
extern const unsigned char gen9_avc_intra_scaling_factor[64];
// AVC MBEnc CURBE init data
extern const unsigned int gen9_avc_mbenc_curbe_normal_i_frame_init_data[GEN9_AVC_MBENC_CURBE_SIZE];
extern const unsigned int gen9_avc_mbenc_curbe_normal_p_frame_init_data[GEN9_AVC_MBENC_CURBE_SIZE];
extern const unsigned int gen9_avc_mbenc_curbe_normal_b_frame_init_data[GEN9_AVC_MBENC_CURBE_SIZE];
// AVC I_DIST CURBE init data
extern const unsigned int gen9_avc_mbenc_curbe_i_frame_dist_init_data[GEN9_AVC_MBENC_CURBE_SIZE];
// AVC ME CURBE init data
extern const unsigned int gen9_avc_me_curbe_init_data[39];
//extern const unsigned int gen9_avc_brc_init_reset_curbe_init_data[24];
//extern const unsigned int gen9_avc_frame_brc_update_curbe_init_data[16];
//extern const unsigned int gen9_avc_mb_brc_update_curbe_init_data[7];
extern const unsigned int gen75_avc_mode_mv_cost_table[3][52][8];
extern const unsigned int gen9_avc_mode_mv_cost_table[3][52][8];
extern const unsigned char gen75_avc_qp_adjustment_dist_threshold_max_frame_threshold_dist_qp_adjustment_ipb[576];
extern const unsigned char  gen9_avc_qp_adjustment_dist_threshold_max_frame_threshold_dist_qp_adjustment_ipb[576];
// SkipVal (DW offset 9) in the following table needs to be changed by Driver based on the BlockbasedSkip and Transform Flag.
// Kernel indexes this table based on the MB QP.
extern const unsigned int gen9_avc_mb_brc_const_data[3][AVC_QP_MAX][16];
extern const unsigned short gen9_avc_ref_cost[3][64];

//
extern const bool gen9_avc_mbbrc_enable[PRESET_NUM];
extern const unsigned int gen9_avc_super_hme[PRESET_NUM];
extern const unsigned int gen9_avc_ultra_hme[PRESET_NUM];

// 1 for P, 3 for P & B
extern const unsigned int gen9_avc_all_fractional[PRESET_NUM];
extern const unsigned char gen9_avc_max_ref_id0_progressive_4k[PRESET_NUM];
extern const unsigned char gen9_avc_max_ref_id0[PRESET_NUM];
extern const unsigned char gen9_avc_max_b_ref_id0[PRESET_NUM];
extern const unsigned char gen9_avc_max_ref_id1[PRESET_NUM];
extern const unsigned int gen9_avc_inter_rounding_p[PRESET_NUM];
extern const unsigned int gen9_avc_inter_rounding_b_ref[PRESET_NUM];
extern const unsigned int gen9_avc_inter_rounding_b[PRESET_NUM];
// This applies only for progressive pictures. For interlaced, CAF is currently not disabled.
extern const unsigned int gen9_avc_disable_all_fractional_check_for_high_res[PRESET_NUM];
extern const unsigned char gen9_avc_adaptive_inter_rounding_p[AVC_QP_MAX];
extern const unsigned char gen9_avc_adaptive_inter_rounding_b[AVC_QP_MAX];
extern const unsigned char gen9_avc_adaptive_inter_rounding_p_without_b[AVC_QP_MAX];
extern const unsigned int gen9_avc_trellis_quantization_enable[PRESET_NUM];
extern const unsigned int gen9_avc_trellis_quantization_rounding[PRESET_NUM];
extern const unsigned int gen9_avc_enable_adaptive_trellis_quantization[PRESET_NUM];

//new add
extern const unsigned int gen9_avc_super_combine_dist[PRESET_NUM + 1];
extern const unsigned char gen9_avc_p_me_method[PRESET_NUM + 1];
extern const unsigned char gen9_avc_b_me_method[PRESET_NUM + 1];
extern const unsigned int gen9_avc_enable_adaptive_search[PRESET_NUM];
extern const unsigned int gen9_avc_max_len_sp[PRESET_NUM];
extern const unsigned int gen9_avc_max_ftq_based_skip[PRESET_NUM];
extern const unsigned int gen9_avc_mr_disable_qp_check[PRESET_NUM];
extern const unsigned int gen9_avc_multi_pred[PRESET_NUM];
extern const unsigned int gen9_avc_hme_b_combine_len[PRESET_NUM];
extern const unsigned int gen9_avc_hme_combine_len[PRESET_NUM];

extern const unsigned int gen9_avc_search_x[PRESET_NUM];
extern const unsigned int gen9_avc_search_y[PRESET_NUM];
extern const unsigned int gen9_avc_b_search_x[PRESET_NUM];
extern const unsigned int gen9_avc_b_search_y[PRESET_NUM];
extern const unsigned char gen9_avc_enable_adaptive_tx_decision[PRESET_NUM];
extern const unsigned char gen9_avc_kernel_mode[PRESET_NUM];

/* Gen95  */
extern const unsigned int gen95_avc_trellis_quantization_rounding[PRESET_NUM];
extern const unsigned int gen95_avc_mbenc_curbe_normal_i_frame_init_data[GEN9_AVC_MBENC_CURBE_SIZE];
extern const unsigned int gen95_avc_mbenc_curbe_normal_p_frame_init_data[GEN9_AVC_MBENC_CURBE_SIZE];
extern const unsigned int gen95_avc_mbenc_curbe_normal_b_frame_init_data[GEN9_AVC_MBENC_CURBE_SIZE];
extern const unsigned int gen95_avc_mbenc_curbe_i_frame_dist_init_data[GEN9_AVC_MBENC_CURBE_SIZE];
extern const unsigned int gen95_avc_tq_lambda_i_frame[AVC_QP_MAX][2];
extern const unsigned int gen95_avc_tq_lambda_p_frame[AVC_QP_MAX][2];
extern const unsigned int gen95_avc_tq_lambda_b_frame[AVC_QP_MAX][2];
extern const unsigned short gen95_avc_lambda_data[256];
extern const unsigned char gen95_avc_ftq25[64];
#endif //GEN9_AVC_const_DEF_H
