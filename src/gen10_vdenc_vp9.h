/*
 * Copyright © 2018 Intel Corporation
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

#ifndef GEN10_VDENC_VP9_H
#define GEN10_VDENC_VP9_H

#include <drm.h>
#include <i915_drm.h>
#include <intel_bufmgr.h>

#include "i965_gpe_utils.h"
#include "i965_encoder.h"
#include "vp9_probs.h"

struct encode_state;

#define VP9_FRAME_I                             0
#define VP9_FRAME_P                             1

#define VP9_REF_NONE                            0x00
#define VP9_REF_LAST                            0x01
#define VP9_REF_GOLDEN                          0x02
#define VP9_REF_ALT                             0x04

#define VP9_SUPER_BLOCK_WIDTH                   64
#define VP9_SUPER_BLOCK_HEIGHT                  64

#define VP9_MAX_SEGMENTS                        8

#define VDENC_VP9_HUC_BRC_INIT_RESET            11
#define VDENC_VP9_HUC_BRC_UPDATE                12
#define VDENC_VP9_HUC_PROB                      13
#define VDENC_VP9_HUC_INITIALIZER               14

#define VDENC_VP9_HUC_DMEM_DATA_OFFSET          0x2000
#define VDENC_VP9_HUC_SUPERFRAME_PASS           2

#define VDENC_VP9_BRC_STATS_BUF_SIZE                    (48 * sizeof(unsigned int))
#define VDENC_VP9_BRC_PAK_STATS_BUF_SIZE                (64 * sizeof(unsigned int))
#define VDENC_VP9_BRC_HISTORY_BUFFER_SIZE               1152

#define VDENC_VP9_BRC_MAX_NUM_OF_PASSES                 3
#define VDENC_VP9_BRC_CONSTANT_DATA_SIZE                1664

#define VDENC_VP9_BRC_BITSTREAM_SIZE_BUFFER_SIZE        64
#define VDENC_VP9_BRC_BITSTREAM_BYTE_COUNT_OFFSET       0

#define VDENC_VP9_BRC_HUC_DATA_BUFFER_SIZE              64

#define VDENC_VP9_HUC_DATA_EXTENSION_SIZE               32

#define VDENC_HCP_VP9_PIC_STATE_SIZE                    168
#define VDENC_HCP_VP9_SEGMENT_STATE_SIZE                32
#define VDENC_VDENC_CMD0_STATE_SIZE                     120
#define VDENC_VDENC_CMD1_STATE_SIZE                     148
#define VDENC_BATCHBUFFER_END_SIZE                      4       /* END */

#define VDENC_HCP_VP9_PIC_STATE_SIZE_IN_DWS             (VDENC_HCP_VP9_PIC_STATE_SIZE >> 2)
#define VDENC_HCP_VP9_SEGMENT_STATE_SIZE_IN_DWS         (VDENC_HCP_VP9_SEGMENT_STATE_SIZE >> 2)
#define VDENC_VDENC_CMD0_STATE_SIZE_IN_DWS              (VDENC_VDENC_CMD0_STATE_SIZE >> 2)
#define VDENC_VDENC_CMD1_STATE_SIZE_IN_DWS              (VDENC_VDENC_CMD1_STATE_SIZE >> 2)

enum gen10_vdenc_vp9_walker_degree {
    VP9_NO_DEGREE = 0,
    VP9_26_DEGREE,
    VP9_45Z_DEGREE
};

#define NUM_KERNELS_PER_GPE_CONTEXT             1
#define MAX_VP9_VDENC_SURFACES                  128

struct gen10_vdenc_vp9_kernel_parameters {
    unsigned int curbe_size;
    unsigned int inline_data_size;
    unsigned int external_data_size;
    unsigned int sampler_size;
};

struct gen10_vdenc_vp9_kernel_scoreboard_parameters {
    unsigned int mask;
    unsigned int type;
    unsigned int enable;
    unsigned int walk_pattern_flag;
};

struct gen10_vdenc_vp9_dys_curbe_data {
    struct {
        uint32_t input_frame_width: 16;
        uint32_t input_frame_height: 16;
    } dw0;

    struct {
        uint32_t output_frame_width: 16;
        uint32_t output_frame_height: 16;
    } dw1;

    struct {
        float delta_u;
    } dw2;

    struct {
        float delta_v;
    } dw3;

    uint32_t reserved[12];

    struct {
        uint32_t input_frame_nv12_bti;
    } dw16;

    struct {
        uint32_t output_frame_y_bti;
    } dw17;

    struct {
        uint32_t avs_sample_bti;
    } dw18;
};

struct gen10_vdenc_vp9_dys_curbe_parameters {
    uint32_t input_width;
    uint32_t input_height;
    uint32_t output_width;
    uint32_t output_height;
};

struct gen10_vdenc_vp9_dys_surface_parameters {
    struct object_surface *input_frame;
    struct object_surface *output_frame;
    uint32_t vert_line_stride;
    uint32_t vert_line_stride_offset;
};

struct gen10_vdenc_vp9_dys_kernel_parameters {
    uint32_t input_width;
    uint32_t input_height;
    uint32_t output_width;
    uint32_t output_height;
    struct object_surface *input_surface;
    struct object_surface *output_surface;
};

enum gen10_vdenc_vp9_dys_binding_table_offset {
    VP9_BTI_DYS_INPUT_NV12 = 0,
    VP9_BTI_DYS_OUTPUT_Y = 1,
    VP9_BTI_DYS_OUTPUT_UV = 2,
    VP9_BTI_DYS_NUM_SURFACES = 3
};

struct gen10_vdenc_vp9_dys_context {
    struct i965_gpe_context gpe_context;
};

struct gen10_vdenc_vp9_search_path_delta {
    uint8_t x: 4;
    uint8_t y: 4;
};

struct gen10_vdenc_vp9_streamin_curbe_data {
    struct {
        uint32_t skip_mode_enabled: 1;
        uint32_t adaptive_enabled: 1;
        uint32_t bi_mix_dis: 1;
        uint32_t reserved0: 2;
        uint32_t early_ime_success_enabled: 1;
        uint32_t reserved1: 1;
        uint32_t t8x8_flag_for_inter_enabled: 1;
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
        uint32_t reserved: 16;
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
        uint32_t disable_field_cache_alloc: 1;
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
        uint32_t reserved: 8;
        uint32_t qp_prime_y: 8;
        uint32_t ref_width: 8;
        uint32_t ref_height: 8;
    } dw5;

    struct {
        uint32_t reserved0: 1;
        uint32_t input_streamin_enabled: 1;
        uint32_t lcu_size: 1;
        uint32_t write_distortions: 1;
        uint32_t use_mv_from_prev_step: 1;
        uint32_t reserved1: 3;
        uint32_t super_combine_dist: 8;
        uint32_t max_vmv_r: 16;
    } dw6;

    struct {
        uint32_t reserved0: 16;
        uint32_t mv_ct_scale_factor: 2;
        uint32_t bilinear_enable: 1;
        uint32_t src_field_polarity: 1;
        uint32_t weighted_sad_haar: 1;
        uint32_t ac_only_haar: 1;
        uint32_t ref_id_ct_mode: 1;
        uint32_t reserved1: 1;
        uint32_t skip_center_mask: 8;
    } dw7;

    struct {
        uint32_t mode0_ct: 8;
        uint32_t mode1_ct: 8;
        uint32_t mode2_ct: 8;
        uint32_t mode3_ct: 8;
    } dw8;

    struct {
        uint32_t mode4_ct: 8;
        uint32_t mode5_ct: 8;
        uint32_t mode6_ct: 8;
        uint32_t mode7_ct: 8;
    } dw9;

    struct {
        uint32_t mode8_ct: 8;
        uint32_t mode9_ct: 8;
        uint32_t ref_id_ct: 8;
        uint32_t chroma_intra_mode_ct: 8;
    } dw10;

    struct {
        uint32_t mv0_ct: 8;
        uint32_t mv1_ct: 8;
        uint32_t mv2_ct: 8;
        uint32_t mv3_ct: 8;
    } dw11;

    struct {
        uint32_t mv4_ct: 8;
        uint32_t mv5_ct: 8;
        uint32_t mv6_ct: 8;
        uint32_t mv7_ct: 8;
    } dw12;

    struct {
        uint32_t num_ref_idx_l0_minus_one: 8;
        uint32_t num_ref_idx_l1_minus_one: 8;
        uint32_t ref_streamin_ct: 8;
        uint32_t roi_enable: 3;
        uint32_t reserved: 5;
    } dw13;

    struct {
        uint32_t list0_ref_id0_field_parity: 1;
        uint32_t list0_ref_id1_field_parity: 1;
        uint32_t list0_ref_id2_field_parity: 1;
        uint32_t list0_ref_id3_field_parity: 1;
        uint32_t list0_ref_id4_field_parity: 1;
        uint32_t list0_ref_id5_field_parity: 1;
        uint32_t list0_ref_id6_field_parity: 1;
        uint32_t list0_ref_id7_field_parity: 1;
        uint32_t list1_ref_id0_field_parity: 1;
        uint32_t list1_ref_id1_field_parity: 1;
        uint32_t reserved: 22;
    } dw14;

    struct {
        uint32_t prev_mv_read_pos_factor: 8;
        uint32_t mv_shift_factor: 8;
        uint32_t reserved: 16;
    } dw15;

    struct {
        struct gen10_vdenc_vp9_search_path_delta sp_delta_0;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_1;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_2;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_3;
    } dw16;

    struct {
        struct gen10_vdenc_vp9_search_path_delta sp_delta_4;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_5;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_6;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_7;
    } dw17;

    struct {
        struct gen10_vdenc_vp9_search_path_delta sp_delta_8;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_9;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_10;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_11;
    } dw18;

    struct {
        struct gen10_vdenc_vp9_search_path_delta sp_delta_12;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_13;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_14;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_15;
    } dw19;

    struct {
        struct gen10_vdenc_vp9_search_path_delta sp_delta_16;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_17;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_18;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_19;
    } dw20;

    struct {
        struct gen10_vdenc_vp9_search_path_delta sp_delta_20;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_21;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_22;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_23;
    } dw21;

    struct {
        struct gen10_vdenc_vp9_search_path_delta sp_delta_24;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_25;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_26;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_27;
    } dw22;

    struct {
        struct gen10_vdenc_vp9_search_path_delta sp_delta_28;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_29;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_30;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_31;
    } dw23;

    struct {
        struct gen10_vdenc_vp9_search_path_delta sp_delta_32;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_33;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_34;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_35;
    } dw24;

    struct {
        struct gen10_vdenc_vp9_search_path_delta sp_delta_36;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_37;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_38;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_39;
    } dw25;

    struct {
        struct gen10_vdenc_vp9_search_path_delta sp_delta_40;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_41;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_42;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_43;
    } dw26;

    struct {
        struct gen10_vdenc_vp9_search_path_delta sp_delta_44;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_45;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_46;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_47;
    } dw27;

    struct {
        struct gen10_vdenc_vp9_search_path_delta sp_delta_48;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_49;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_50;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_51;
    } dw28;

    struct {
        struct gen10_vdenc_vp9_search_path_delta sp_delta_52;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_53;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_54;
        struct gen10_vdenc_vp9_search_path_delta sp_delta_55;
    } dw29;

    struct {
        uint32_t actual_mb_width: 16;
        uint32_t actual_mb_height: 16;
    } dw30;

    struct {
        uint32_t roi_ctrl: 8;
        uint32_t max_tu_size: 2;
        uint32_t max_cu_size: 2;
        uint32_t num_ime_predictors: 4;
        uint32_t reserved: 8;
        uint32_t pu_type_ctrl: 8;
    } dw31;

    struct {
        uint32_t force_mvx0: 16;
        uint32_t force_mvy0: 16;
    } dw32;

    struct {
        uint32_t force_mvx1: 16;
        uint32_t force_mvy1: 16;
    } dw33;

    struct {
        uint32_t force_mvx2: 16;
        uint32_t force_mvy2: 16;
    } dw34;

    struct {
        uint32_t force_mvx3: 16;
        uint32_t force_mvy3: 16;
    } dw35;

    struct {
        uint32_t force_ref_idx0: 4;
        uint32_t force_ref_idx1: 4;
        uint32_t force_ref_idx2: 4;
        uint32_t force_ref_idx3: 4;
        uint32_t num_merge_cand_cu8x8: 4;
        uint32_t num_merge_cand_cu16x16: 4;
        uint32_t num_merge_cand_cu32x32: 4;
        uint32_t num_merge_cand_cu64x64: 4;
    } dw36;

    struct {
        uint32_t seg_id: 16;
        uint32_t qp_enable: 4;
        uint32_t seg_id_enable: 1;
        uint32_t reserved0: 2;
        uint32_t force_ref_id_enable: 1;
        uint32_t reserved1: 8;
    } dw37;

    struct {
        uint32_t force_qp0: 8;
        uint32_t force_qp1: 8;
        uint32_t force_qp2: 8;
        uint32_t force_qp3: 8;
    } dw38;

    struct {
        uint32_t reserved;
    } dw39;

    struct {
        uint32_t surf_index_4x_me_mv_output_data;
    } dw40;

    struct {
        uint32_t surf_index_16x_or_32x_me_mv_input_data;
    } dw41;

    struct {
        uint32_t surf_index_4x_me_output_dist;
    } dw42;

    struct {
        uint32_t surf_index_4x_me_output_brc_dist;
    } dw43;

    struct {
        uint32_t surf_index_vme_fwd_inter_prediction;
    } dw44;

    struct {
        uint32_t surf_index_vme_bwd_inter_prediction;
    } dw45;

    struct {
        uint32_t surf_index_vdenc_streamin_output;
    } dw46;

    struct {
        uint32_t surf_index_vdenc_streamin_input;
    } dw47;
};

struct gen10_vdenc_vp9_streamin_curbe_parameters {
    uint32_t input_width;
    uint32_t input_height;
    uint32_t output_width;
    uint32_t output_height;
};

struct gen10_vdenc_vp9_streamin_surface_parameters {
    struct object_surface *input_frame;
    struct object_surface *output_frame;
    uint32_t vert_line_stride;
    uint32_t vert_line_stride_offset;
};

struct gen10_vdenc_vp9_streamin_kernel_parameters {
    uint32_t input_width;
    uint32_t input_height;
    uint32_t output_width;
    uint32_t output_height;
    struct object_surface *input_surface;
    struct object_surface *output_surface;
};

enum gen10_vdenc_vp9_streamin_binding_table_offset {
    VP9_BTI_STREAMIN_INPUT_NV12 = 0,
    VP9_BTI_STREAMIN_OUTPUT_Y = 1,
    VP9_BTI_STREAMIN_OUTPUT_UV = 2,
    VP9_BTI_STREAMIN_NUM_SURFACES = 3
};

struct gen10_vdenc_vp9_streamin_context {
    struct i965_gpe_context gpe_context;
};

struct gen10_is_ct {
    struct {
        uint32_t mv0_ct: 8;
        uint32_t mv1_ct: 8;
        uint32_t mv2_ct: 8;
        uint32_t mv3_ct: 8;
    } dw0;

    struct {
        uint32_t mv4_ct: 8;
        uint32_t mv5_ct: 8;
        uint32_t mv6_ct: 8;
        uint32_t mv7_ct: 8;
    } dw1;
};

struct gen10_vdenc_vp9_igs {
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
        uint32_t mv_ct_scaling_factor: 2;
        uint32_t bilinear_filter_enable: 1;
        uint32_t pad1: 3;
        uint32_t ref_id_ct_mode_select: 1;
        uint32_t pad2: 9;
    } dw8;

    struct {
        uint32_t mode0_ct: 8;
        uint32_t mode1_ct: 8;
        uint32_t mode2_ct: 8;
        uint32_t mode3_ct: 8;
    } dw9;

    struct {
        uint32_t mode4_ct: 8;
        uint32_t mode5_ct: 8;
        uint32_t mode6_ct: 8;
        uint32_t mode7_ct: 8;
    } dw10;

    struct {
        uint32_t mode8_ct: 8;
        uint32_t mode9_ct: 8;
        uint32_t ref_id_ct: 8;
        uint32_t chroma_intra_mode_ct: 8;
    } dw11;

    struct {
        struct gen10_is_ct mv_ct;
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
        struct gen10_is_ct hme_mv_ct;
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
        uint32_t is_qp_override: 1;
        uint32_t pad1: 1;
        uint32_t midpoint_distortion: 16;
    } dw34;
};

struct gen10_vdenc_vp9_streamin_state {
    struct {
        uint32_t roi_32x32_0_16x16_03: 8;
        uint32_t max_tu_size: 2;
        uint32_t max_cu_size: 2;
        uint32_t num_ime_predictors: 4;
        uint32_t pad0: 8;
        uint32_t pu_type_32x32_0_16x16_03: 8;
    } dw0;

    struct {
        uint32_t force_mv_x_32x32_0_16x16_0: 16;
        uint32_t force_mv_y_32x32_0_16x16_0: 16;
    } dw1;

    struct {
        uint32_t force_mv_x_32x32_0_16x16_1: 16;
        uint32_t force_mv_y_32x32_0_16x16_1: 16;
    } dw2;

    struct {
        uint32_t force_mv_x_32x32_0_16x16_2: 16;
        uint32_t force_mv_y_32x32_0_16x16_2: 16;
    } dw3;

    union {
        uint32_t force_mv_x_32x32_0_16x16_3: 16;
        uint32_t force_mv_y_32x32_0_16x16_3: 16;
    } dw4;

    struct {
        uint32_t pad0;
    } dw5;

    struct {
        uint32_t force_mv_ref_idx_32x32_0_16x16_0: 4;
        uint32_t force_mv_ref_idx_32x32_0_16x16_1: 4;
        uint32_t force_mv_ref_idx_32x32_0_16x16_2: 4;
        uint32_t force_mv_ref_idx_32x32_0_16x16_3: 4;
        uint32_t num_merge_candidate_cu_8x8: 4;
        uint32_t num_merge_candidate_cu_16x16: 4;
        uint32_t num_merge_candidate_cu_32x32: 4;
        uint32_t num_merge_candidate_cu_64x64: 4;
    } dw6;

    struct {
        uint32_t segid_32x32_0_16x16_03_vp9_only: 16;
        uint32_t qp_en_32x32_0_16x16_03: 4;
        uint32_t segid_enable: 1;
        uint32_t pad0: 2;
        uint32_t force_refid_enable_32x32_0: 4;
        uint32_t ime_predictor_refid_select_03_32x32_0: 8;
    } dw7;

    struct {
        uint32_t ime_predictor_0_x_32x32_0: 16;
        uint32_t ime_predictor_0_y_32x32_0: 16;
    } dw8;

    struct {
        uint32_t ime_predictor_0_x_32x32_1: 16;
        uint32_t ime_predictor_0_y_32x32_1: 16;
    } dw9;

    struct {
        uint32_t ime_predictor_0_x_32x32_2: 16;
        uint32_t ime_predictor_0_y_32x32_2: 16;
    } dw10;

    struct {
        uint32_t ime_predictor_0_x_32x32_3: 16;
        uint32_t ime_predictor_0_y_32x32_3: 16;
    } dw11;

    struct {
        uint32_t ime_predictor_0_refidx32x32_0: 4;
        uint32_t ime_predictor_1_refidx32x32_1: 4;
        uint32_t ime_predictor_2_refidx32x32_2: 4;
        uint32_t ime_predictor_3_refidx32x32_3: 4;
        uint32_t pad0: 16;
    } dw12;

    struct {
        uint32_t panic_model_cu_threshold: 16;
        uint32_t pad0: 16;
    } dw13;

    struct {
        uint32_t force_qp_value_16x16_0: 8;
        uint32_t force_qp_value_16x16_1: 8;
        uint32_t force_qp_value_16x16_2: 8;
        uint32_t force_qp_value_16x16_3: 8;
    } dw14;

    struct {
        uint32_t pad0;
    } dw15;
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
    int8_t buf_rate_adj_tab_i[72];
    int8_t buf_rate_adj_tab_p[72];
    int8_t buf_rate_adj_tab_b[72];
    uint8_t frame_size_min_tab_p[9];
    uint8_t frame_size_min_tab_b[9];
    uint8_t frame_size_min_tab_i[9];
    uint8_t frame_size_max_tab_p[9];
    uint8_t frame_size_max_tab_b[9];
    uint8_t frame_size_max_tab_i[9];
    uint8_t frame_size_scg_tab_p[9];
    uint8_t frame_size_scg_tab_b[9];
    uint8_t frame_size_scg_tab_i[9];

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
    uint8_t hme_mv_ct[8][42];
    uint8_t pad0[42];
};

struct vdenc_vp9_huc_brc_init_dmem {
    uint32_t brc_func;
    uint32_t profile_level_max_frame;
    uint32_t init_buffer_fullness;
    uint32_t buffer_size;
    uint32_t target_bitrate;
    uint32_t max_rate;
    uint32_t min_rate;
    uint32_t frame_rate_m;
    uint32_t frame_rate_d;
    uint32_t pad0[4];

    uint16_t brc_flag;
    uint16_t num_p_in_gop;
    uint16_t pad1;
    uint16_t frame_width;
    uint16_t frame_height;
    uint16_t min_qp;
    uint16_t max_qp;
    uint16_t level_qp;
    uint16_t golden_frame_interval;
    uint16_t enable_scaling;
    uint16_t overshoot_cbr;
    uint16_t pad2[5];

    int8_t inst_rate_thresh_p0[4];
    int8_t pad3[4];
    int8_t inst_rate_thresh_i0[4];
    int8_t dev_thresh_pb0[8];
    int8_t dev_thresh_vbr0[8];
    int8_t dev_thresh_i0[8];

    uint8_t init_qp_p;
    uint8_t init_qp_i;
    uint8_t pad4;
    uint8_t total_level;
    uint8_t max_level_ratio[16];
    uint8_t sliding_window_enable;
    uint8_t sliding_window_size;
    uint8_t pad5[47];
};

struct vdenc_vp9_huc_brc_update_dmem {
    int32_t target_buf_fullness;
    uint32_t frame_num;
    int32_t hrd_buffer_fullness_upper;
    int32_t hrd_buffer_fullness_lower;
    uint32_t pad0[7];

    uint16_t start_global_adjust_frame[4] ;
    uint16_t cur_width;
    uint16_t cur_height;
    uint16_t asyn;
    uint16_t vdenc_igs_offset;
    uint16_t second_level_batchbuffer_size;
    uint16_t pic_state_offset;
    uint16_t pad1[6];

    uint8_t overflow_flag;
    uint8_t brc_flag;
    uint8_t max_num_paks;
    int8_t current_frame_type;
    uint8_t qp_threshold[4];
    uint8_t global_rate_ratio_threshold[6];
    int8_t start_global_adjust_mult[5];
    int8_t start_global_adjust_div[5];
    int8_t global_rate_ratio_threshold_qp[7];
    uint8_t distion_threshld_i[9];
    uint8_t distion_threshld_p[9];
    uint8_t distion_threshld_b[9];
    int8_t max_frame_thresh_i[5];
    int8_t max_frame_thresh_p[5];
    int8_t max_frame_thresh_b[5];
    uint8_t current_pak_pass;
    uint8_t acq_qp;
    int8_t delta_qp_for_sad_zone0;
    int8_t delta_qp_for_sad_zone1;
    int8_t delta_qp_for_sad_zone2;
    int8_t delta_qp_for_sad_zone3;
    int8_t delta_qp_for_mv_zero;
    int8_t delta_qp_for_mv_zone0;
    int8_t delta_qp_for_mv_zone1;
    int8_t delta_qp_for_mv_zone2;
    uint8_t temporal_level;
    uint8_t segment_map_generating;

    uint8_t pad2[95];
};

struct gen10_vdenc_vp9_status {
    uint32_t bytes_per_frame;
};

struct vdenc_vp9_huc_frame_ctrl {
    uint32_t frame_type;
    uint32_t show_frame;
    uint32_t error_resilient_mode;
    uint32_t intra_only;
    uint32_t context_reset;
    uint32_t last_ref_frame_bias;
    uint32_t golden_ref_frame_bias;
    uint32_t alt_ref_frame_bias;
    uint32_t allow_high_precision_mv;
    uint32_t mcomp_filter_mode;
    uint32_t tx_mode;
    uint32_t refresh_frame_context;
    uint32_t frame_parallel_decode;
    uint32_t comp_pred_mode;
    uint32_t frame_context_idx;
    uint32_t sharpness_level;
    uint32_t seg_on;
    uint32_t seg_map_update;
    uint32_t seg_update_data;
    uint8_t pad0[13];
    uint8_t log2tile_cols;
    uint8_t log2tile_rows;
    uint8_t pad1[5];
};

struct vdenc_vp9_huc_prev_frame_info {
    uint32_t intra_only;
    uint32_t frame_width;
    uint32_t frame_height;
    uint32_t key_frame;
    uint32_t show_frame;
};

struct vdenc_vp9_huc_prob_dmem {
    uint32_t huc_pass_num;
    uint32_t frame_width;
    uint32_t frame_height;
    uint32_t pad0[6];
    char segment_ref[VP9_MAX_SEGMENTS];
    uint8_t segment_skip[VP9_MAX_SEGMENTS];
    uint8_t seg_code_abs;
    uint8_t seg_temporal_update;
    uint8_t last_ref_index;
    uint8_t golden_ref_index;
    uint8_t alt_ref_index;
    uint8_t refresh_frame_flags;
    uint8_t ref_frame_flags;
    uint8_t context_frame_types;
    struct vdenc_vp9_huc_frame_ctrl frame_ctrl;
    struct vdenc_vp9_huc_prev_frame_info prev_frame_info;
    uint8_t pad1[2];
    uint8_t frame_to_show;
    uint8_t load_key_framede_fault_probs;
    uint32_t frame_size;
    uint32_t pad2;
    uint32_t repak;
    uint16_t loop_filter_level_bit_offset;
    uint16_t qindex_bit_offset;
    uint16_t seg_bit_offset;
    uint16_t seg_length_in_bits;
    uint16_t uncomp_hdr_total_length_in_bits;
    uint16_t seg_update_disable;
    int32_t repak_threshold[256];
    uint16_t pic_state_offset;
    uint16_t slb_block_size;
    uint8_t streamin_enable;
    uint8_t streamin_segenable;
    uint8_t disable_dma;
    uint8_t ivf_header_size;
    uint8_t pad3[44];
};

struct vp9_huc_prob_dmem {
    uint32_t huc_pass_num;                              // dw0
    uint32_t frame_width;                               // dw1
    uint32_t frame_height;                              // dw2
    uint32_t max_num_pak_passes;                        // dw3
    int32_t repak_saving_thr;                           // dw4
    uint8_t frame_qp[VP9_MAX_SEGMENTS];                 // dw5,6
    uint8_t loop_filter_level[VP9_MAX_SEGMENTS];        // dw7,8
    uint8_t segment_ref[VP9_MAX_SEGMENTS];              // dw9,10
    uint8_t segment_skip[VP9_MAX_SEGMENTS];             // dw11,12
    uint8_t seg_code_abs;                               // dw13
    uint8_t seg_temporal_update;
    uint8_t last_ref_index;
    uint8_t golden_ref_index;
    uint8_t alt_ref_index;                              // dw14
    uint8_t refresh_frame_flags;
    uint8_t ref_frame_flags;
    uint8_t context_frame_types;
    struct vdenc_vp9_huc_frame_ctrl frame_ctrl;                // dw15 - 38
    struct vdenc_vp9_huc_prev_frame_info prev_frame_info;      // dw39 - 43
    uint8_t brc_enable;                                 // dw44
    uint8_t ts_enable;
    uint8_t frame_to_show;
    uint8_t load_key_frame_default_probs;
    uint32_t frame_size;                                // dw45
    uint32_t hcp_is_control;                            // dw46
    uint32_t repak;                                     // dw47
    uint16_t loop_filter_level_bit_offset;              // dw48
    uint16_t qindex_bit_offset;
    uint16_t seg_bit_offset;                            // dw49
    uint16_t seg_length_in_bits;
    uint16_t uncomp_hdr_total_length_in_bits;           // dw50
    uint16_t seg_update_disable;
    uint32_t repak_threshold[256];                      // dw51 - 306
    uint8_t pad0[52];                                   // dw307 - 319
};

struct vdenc_vp9_cu_data {
    struct {
        uint32_t cu_size: 2;
        uint32_t pad0: 2;
        uint32_t cu_part_mode: 2;
        uint32_t pad1: 2;
        uint32_t intra_chroma_mode0: 4;
        uint32_t pad2: 4;
        uint32_t intra_chroma_mode1: 4;
        uint32_t cu_pred_mode0: 1;
        uint32_t cu_pred_mode1: 1;
        uint32_t pad3: 2;
        uint32_t interpred_comp0: 1;
        uint32_t interpred_comp1: 1;
        uint32_t pad4: 6;
    } dw0;

    struct {
        uint32_t intra_mode0: 4;
        uint32_t pad0: 4;
        uint32_t intra_mode1: 4;
        uint32_t pad1: 4;
        uint32_t intra_mode2: 4;
        uint32_t pad2: 4;
        uint32_t intra_mode3: 4;
        uint32_t pad3: 4;
    } dw1;

    struct {
        int16_t mvx: 16;
        int16_t mvy: 16;
    } dw2;

    struct {
        int16_t mvx: 16;
        int16_t mvy: 16;
    } dw3;

    struct {
        int16_t mvx: 16;
        int16_t mvy: 16;
    } dw4;

    struct {
        int16_t mvx: 16;
        int16_t mvy: 16;
    } dw5;

    struct {
        int16_t mvx: 16;
        int16_t mvy: 16;
    } dw6;

    struct {
        int16_t mvx: 16;
        int16_t mvy: 16;
    } dw7;

    struct {
        int16_t mvx: 16;
        int16_t mvy: 16;
    } dw8;

    struct {
        int16_t mvx: 16;
        int16_t mvy: 16;
    } dw9;

    struct {
        uint32_t refframe_part0_l0: 2;    // 0=intra,1=last,2=golden,3=altref
        uint32_t pad0: 2;
        uint32_t refframe_part1_l0: 2;    // 0=intra,1=last,2=golden,3=altref
        uint32_t pad1: 2;
        uint32_t refframe_part0_l1: 2;    // 0=intra,1=last,2=golden,3=altref
        uint32_t pad2: 2;
        uint32_t refframe_part1_l1: 2;    // 0=intra,1=last,2=golden,3=altref
        uint32_t pad3: 2;
        uint32_t round_part0: 3;
        uint32_t pad4: 1;
        uint32_t round_part1: 3;
        uint32_t pad5: 9;
    } dw10;

    struct {
        uint32_t tu_size0: 2;
        uint32_t tu_size1: 2;
        uint32_t pad0: 10;
        uint32_t segidx_pred0: 1;
        uint32_t segidx_pred1: 1;
        uint32_t segidx_part0: 3;
        uint32_t segidx_part1: 3;
        uint32_t mc_filtertype_part0: 2;
        uint32_t mc_filtertype_part1: 2;
        uint32_t pad1: 6;
    } dw11;

    uint32_t dw12;

    uint32_t dw13;

    uint32_t dw14;

    uint32_t dw15;
};

struct vdenc_vp9_last_frame_status {
    uint16_t frame_width;
    uint16_t frame_height;
    uint8_t is_key_frame;
    uint8_t show_frame;
    uint8_t refresh_frame_context;
    uint8_t frame_context_idx;
    uint8_t intra_only;
    uint8_t segment_enabled;
};

struct huc_initializer_dmem {
    uint32_t output_size;
    uint32_t total_output_commands;
    uint8_t target_usage;
    uint8_t codec;
    uint8_t frame_type;
    uint8_t reserved[37];
    struct {
        uint16_t start_in_bytes;
        uint8_t id;
        uint8_t type;
        uint32_t batch_buffer_end;
    } output_command[50];
};

struct huc_initializer_data {
    uint32_t total_commands;
    struct {
        uint16_t id;
        uint16_t size_of_data;
        uint32_t data[40];
    } input_command[50];
};

struct huc_initializer_input_command1 {
    uint32_t frame_width_in_min_cb_minus1;
    uint32_t frame_height_in_min_cb_minus1;
    uint32_t log2_min_coding_block_size_minus3;
    uint8_t vdenc_streamin_enabled;
    uint8_t pak_only_multi_pass_enabled;
    uint16_t num_ref_idx_l0_active_minus1;
    uint16_t sad_qp_lambda;
    uint16_t rd_qp_lambda;

    uint16_t num_ref_idx_l1_active_minus1;
    uint8_t reserved0;
    uint8_t roi_streamin_enabled;
    int8_t roi_delta_qp[8];
    uint8_t fwd_poc_num_for_ref_id0_in_l0;
    uint8_t fwd_poc_num_for_ref_id0_in_l1;
    uint8_t fwd_poc_num_for_ref_id1_in_l0;
    uint8_t fwd_poc_num_for_ref_id1_in_l1;
    uint8_t fwd_poc_num_for_ref_id2_in_l0;
    uint8_t fwd_poc_num_for_ref_id2_in_l1;
    uint8_t fwd_poc_num_for_ref_id3_in_l0;
    uint8_t fwd_poc_num_for_ref_id3_in_l1;
    uint8_t enable_rolling_intra_refresh;
    int8_t qp_delta_for_inserted_intra;
    uint16_t intra_insertion_size;
    uint16_t intra_insertion_location;
    int8_t qp_y;
    uint8_t rounding_enabled;
    uint8_t use_default_qp_deltas;
    uint8_t panic_enabled;
    uint8_t reserved1[2];

    uint16_t dst_frame_width_minus1;
    uint16_t dst_frame_height_minus1;
    uint8_t segment_enabled;
    uint8_t prev_frame_segment_enabled;
    uint8_t segment_map_streamin_enabled;
    uint8_t luma_ac_qindex;
    int8_t luma_dc_qindex_delta;
    uint8_t reserved2[3];
    int16_t segment_qindex_delta[8];
};

struct gen10_vdenc_vp9_context {
    struct i965_gpe_table *gpe_table;

    struct gen10_vdenc_vp9_dys_context dys_context;
    struct gen10_vdenc_vp9_streamin_context streamin_context;

    struct intel_fraction framerate;

    uint32_t res_width;
    uint32_t res_height;
    uint32_t frame_width;
    uint32_t frame_height;
    uint32_t max_frame_width;
    uint32_t max_frame_height;
    uint32_t frame_width_in_mbs;
    uint32_t frame_height_in_mbs;
    uint32_t frame_width_in_mi_units;
    uint32_t frame_height_in_mi_units;
    uint32_t frame_width_in_sbs;        /* in super blocks */
    uint32_t frame_height_in_sbs;       /* in super blocks */
    uint32_t down_scaled_width_in_mb4x;
    uint32_t down_scaled_height_in_mb4x;
    uint32_t down_scaled_width_4x;
    uint32_t down_scaled_height_4x;
    uint32_t down_scaled_width_in_mb16x;
    uint32_t down_scaled_height_in_mb16x;
    uint32_t down_scaled_width_16x;
    uint32_t down_scaled_height_16x;
    uint32_t target_bit_rate;        /* in kbps */
    uint32_t max_bit_rate;           /* in kbps */
    uint32_t min_bit_rate;           /* in kbps */
    uint64_t init_vbv_buffer_fullness_in_bit;
    uint64_t vbv_buffer_size_in_bit;
    uint16_t sad_qp_lambda;
    uint16_t rd_qp_lambda;
    uint8_t ref_frame_flag;
    uint8_t num_ref_frames;
    uint8_t dys_ref_frame_flag;
    double current_target_buf_full_in_bits;
    double input_bits_per_frame;

    uint32_t brc_initted: 1;
    uint32_t brc_need_reset: 1;
    uint32_t brc_enabled: 1;
    uint32_t internal_rate_mode: 4;
    uint32_t current_pass: 4;
    uint32_t num_passes: 4;
    uint32_t is_first_pass: 1;
    uint32_t is_last_pass: 1;

    uint32_t vdenc_pak_threshold_check_enable: 1;
    uint32_t is_key_frame: 1;
    uint32_t frame_intra_only: 1;
    uint32_t vp9_frame_type: 2; // 0: key frame or intra only, 1: others

    uint32_t dys_enabled: 1;
    uint32_t dys_in_use: 1;
    uint32_t is_first_frame: 1;
    uint32_t has_hme: 1;
    uint32_t need_hme: 1;
    uint32_t hme_enabled: 1;
    uint32_t has_hme_16x: 1;
    uint32_t hme_16x_enabled: 1;
    uint32_t allocate_once_done: 1;
    uint32_t is_8bit: 1;
    uint32_t multiple_pass_brc_enabled: 1;
    uint32_t dys_multiple_pass_enbaled: 1;
    uint32_t pak_only_pass_enabled: 1;
    uint32_t is_super_frame_huc_pass: 1;
    uint32_t has_adaptive_repak: 1;
    uint32_t vdenc_pak_object_streamout_enable: 1;
    uint32_t use_huc: 1;
    uint32_t use_hw_scoreboard: 1;
    uint32_t use_hw_non_stalling_scoreborad: 1;
    uint32_t submit_batchbuffer: 1;

    VAEncSequenceParameterBufferVP9 *seq_param;
    VAEncSequenceParameterBufferVP9 bogus_seq_param;
    VAEncPictureParameterBufferVP9 *pic_param;
    VAEncMiscParameterTypeVP9PerSegmantParam *segment_param;

    struct i965_gpe_resource brc_history_buffer_res;
    struct i965_gpe_resource brc_constant_data_buffer_res;
    struct i965_gpe_resource brc_bitstream_size_buffer_res;
    struct i965_gpe_resource brc_huc_data_buffer_res;

    struct i965_gpe_resource s4x_memv_data_buffer_res;
    struct i965_gpe_resource s4x_memv_distortion_buffer_res;
    struct i965_gpe_resource s16x_memv_data_buffer_res;
    struct i965_gpe_resource output_16x16_inter_modes_buffer_res;
    struct i965_gpe_resource mode_decision_buffer_res[2];
    struct i965_gpe_resource mv_temporal_buffer_res[2];
    struct i965_gpe_resource mb_code_buffer_res;
    struct i965_gpe_resource mb_segment_map_buffer_res;

    /* PAK resource */
    struct i965_gpe_resource hvd_line_buffer_res;
    struct i965_gpe_resource hvd_tile_line_buffer_res;
    struct i965_gpe_resource deblocking_filter_line_buffer_res;
    struct i965_gpe_resource deblocking_filter_tile_line_buffer_res;
    struct i965_gpe_resource deblocking_filter_tile_col_buffer_res;

    struct i965_gpe_resource metadata_line_buffer_res;
    struct i965_gpe_resource metadata_tile_line_buffer_res;
    struct i965_gpe_resource metadata_tile_col_buffer_res;

    struct i965_gpe_resource segmentid_buffer_res;
    struct i965_gpe_resource prob_buffer_res[4];
    struct i965_gpe_resource prob_delta_buffer_res;
    struct i965_gpe_resource prob_counter_buffer_res;

    struct i965_gpe_resource compressed_header_buffer_res;
    struct i965_gpe_resource tile_record_streamout_buffer_res;
    struct i965_gpe_resource cu_stat_streamout_buffer_res;

    /* HUC */
    struct i965_gpe_resource huc_prob_dmem_buffer_res[2];
    struct i965_gpe_resource huc_default_prob_buffer_res;
    struct i965_gpe_resource huc_prob_output_buffer_res;
    struct i965_gpe_resource huc_pak_insert_uncompressed_header_input_2nd_batchbuffer_res;
    struct i965_gpe_resource huc_pak_insert_uncompressed_header_output_2nd_batchbuffer_res;

    /* VDEnc */
    struct i965_gpe_resource vdenc_row_store_scratch_res;
    struct i965_gpe_resource vdenc_brc_stat_buffer_res;
    struct i965_gpe_resource vdenc_pic_state_input_2nd_batchbuffer_res[4];
    struct i965_gpe_resource vdenc_pic_state_output_2nd_batchbuffer_res[4];
    struct i965_gpe_resource vdenc_dys_pic_state_2nd_batchbuffer_res;
    struct i965_gpe_resource vdenc_brc_init_reset_dmem_buffer_res;
    struct i965_gpe_resource vdenc_brc_update_dmem_buffer_res[VDENC_VP9_BRC_MAX_NUM_OF_PASSES];
    struct i965_gpe_resource vdenc_segment_map_stream_out_buffer_res;
    struct i965_gpe_resource vdenc_brc_pak_stat_buffer_res;
    struct i965_gpe_resource vdenc_sse_src_pixel_row_store_buffer_res;
    struct i965_gpe_resource vdenc_data_extension_buffer_res;
    struct i965_gpe_resource vdenc_streamin_buffer_res;
    struct i965_gpe_resource huc_status2_buffer_res;
    struct i965_gpe_resource huc_status_buffer_res;

    /* Reconstructed picture */
    struct i965_gpe_resource recon_surface_res;
    struct i965_gpe_resource scaled_4x_recon_surface_res;
    struct i965_gpe_resource scaled_8x_recon_surface_res;
    struct i965_gpe_resource scaled_16x_recon_surface_res;

    /* HuC CMD initializer */
    struct i965_gpe_resource huc_initializer_dmem_buffer_res[VDENC_VP9_BRC_MAX_NUM_OF_PASSES];
    struct i965_gpe_resource huc_initializer_data_buffer_res[VDENC_VP9_BRC_MAX_NUM_OF_PASSES];
    struct i965_gpe_resource huc_initializer_dys_dmem_buffer_res;
    struct i965_gpe_resource huc_initializer_dys_data_buffer_res;

    /* Reference */
    struct object_surface *last_ref_obj;
    struct object_surface *golden_ref_obj;
    struct object_surface *alt_ref_obj;

    struct i965_gpe_resource last_ref_res;
    struct i965_gpe_resource golden_ref_res;
    struct i965_gpe_resource alt_ref_res;

    /* Input YUV */
    struct i965_gpe_resource uncompressed_input_yuv_surface_res;                    // Input

    struct {
        struct i965_gpe_resource res;                                           // Output
        uint32_t start_offset;
        uint32_t end_offset;
    } compressed_bitstream;

    struct {
        struct i965_gpe_resource res;
        uint32_t base_offset;
        uint32_t size;
        uint32_t bytes_per_frame_offset;
    } status_bffuer;

    uint32_t coding_unit_offset;
    uint32_t mb_code_buffer_size;
    char *alias_insert_data;
    char *frame_header_data;
    int32_t frame_header_length;
    vp9_header_bitoffset frame_header;
    int32_t vdenc_pic_state_2nd_batchbuffer_index;

    struct vdenc_vp9_last_frame_status last_frame_status;
    uint32_t frame_number;
    uint32_t curr_mv_temporal_index;
    uint32_t tx_mode;
    uint32_t huc_2nd_batchbuffer_size;
    uint32_t cmd1_state_offset_in_2nd_batchbuffer;
    uint32_t pic_state_offset_in_2nd_batchbuffer;
    uint8_t context_frame_types[4];
};

#endif  /* GEN10_VDENC_VP9_H */
