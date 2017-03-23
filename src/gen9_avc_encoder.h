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

#ifndef GEN9_AVC_ENCODER_H
#define GEN9_AVC_ENCODER_H

#include "i965_encoder_common.h"
/*
common structure and define
gen9_avc_surface structure
*/
#define MAX_AVC_ENCODER_SURFACES        64
#define MAX_AVC_PAK_PASS_NUM        4

#define ENCODER_AVC_CONST_SURFACE_WIDTH 64
#define ENCODER_AVC_CONST_SURFACE_HEIGHT 44
#define WIDTH_IN_MACROBLOCKS(width)             (ALIGN(width, 16) >> 4)

#define AVC_BRC_HISTORY_BUFFER_SIZE             864
#define AVC_BRC_CONSTANTSURFACE_SIZE            1664
#define AVC_ADAPTIVE_TX_DECISION_THRESHOLD           128
#define AVC_MB_TEXTURE_THRESHOLD                     1024
#define AVC_SFD_COST_TABLE_BUFFER_SIZ                52
#define AVC_INVALID_ROUNDING_VALUE                255

struct gen9_mfx_avc_img_state {
    union {
        struct {
            uint32_t dword_length: 12;
            uint32_t pad0: 4;
            uint32_t sub_opcode_b: 5;
            uint32_t sub_opcode_a: 3;
            uint32_t command_opcode: 3;
            uint32_t pipeline: 2;
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
        uint32_t vsl_top_mb_trans8x8_flag: 1;
        uint32_t pad0: 31;
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
        uint32_t slice_tsats_streamout_enable: 1;
    } dw11;

    struct {
        uint32_t pad0: 16;
        uint32_t mpeg2_old_mode_select: 1;
        uint32_t vad_noa_mux_select: 1;
        uint32_t vad_error_logic: 1;
        uint32_t pad1: 1;
        uint32_t vmd_error_logic: 1;
        uint32_t pad2: 11;
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

/*
   the definition for encoder status
*/
struct encoder_status {
    uint32_t image_status_mask;
    uint32_t image_status_ctrl;
    uint32_t bs_byte_count_frame;
    uint32_t bs_byte_count_frame_nh;
    uint32_t mfc_qp_status_count;
    uint32_t media_index;
};

struct encoder_status_buffer_internal {
    dri_bo *bo;
    uint32_t image_status_mask_offset;
    uint32_t image_status_ctrl_offset;
    uint32_t bs_byte_count_frame_offset;
    uint32_t bs_byte_count_frame_nh_offset;
    uint32_t mfc_qp_status_count_offset;
    uint32_t media_index_offset;

    uint32_t bs_byte_count_frame_reg_offset;
    uint32_t bs_byte_count_frame_nh_reg_offset;
    uint32_t image_status_mask_reg_offset;
    uint32_t image_status_ctrl_reg_offset;
    uint32_t mfc_qp_status_count_reg_offset;
    uint32_t status_buffer_size;
    uint32_t base_offset;
};

/* BRC define */
#define CLIP(x, min, max)                                             \
    {                                                                   \
        (x) = (((x) > (max)) ? (max) : (((x) < (min)) ? (min) : (x)));  \
    }

typedef struct _kernel_header_ {
    uint32_t       reserved                        : 6;
    uint32_t       kernel_start_pointer            : 26;
} kernel_header;

struct generic_search_path_delta {
    uint8_t search_path_delta_x: 4;
    uint8_t search_path_delta_y: 4;
};

struct scaling_param {
    VASurfaceID             curr_pic;
    void                    *p_scaling_bti;
    struct object_surface   *input_surface;
    struct object_surface   *output_surface;
    uint32_t                input_frame_width;
    uint32_t                input_frame_height;
    uint32_t                output_frame_width;
    uint32_t                output_frame_height;
    uint32_t                vert_line_stride;
    uint32_t                vert_line_stride_offset;
    bool                    scaling_out_use_16unorm_surf_fmt;
    bool                    scaling_out_use_32unorm_surf_fmt;
    bool                    mbv_proc_stat_enabled;
    bool                    enable_mb_flatness_check;
    bool                    enable_mb_variance_output;
    bool                    enable_mb_pixel_average_output;
    bool                    use_4x_scaling;
    bool                    use_16x_scaling;
    bool                    use_32x_scaling;
    bool                    blk8x8_stat_enabled;
    struct i965_gpe_resource            *pres_mbv_proc_stat_buffer;
    struct i965_gpe_resource            *pres_flatness_check_surface;
};

struct avc_surface_param {
    uint32_t frame_width;
    uint32_t frame_height;
};
struct me_param {
    uint32_t hme_type;
};
struct wp_param {
    uint32_t ref_list_idx;
};

struct brc_param {
    struct i965_gpe_context * gpe_context_brc_frame_update;
    struct i965_gpe_context * gpe_context_mbenc;
};

struct mbenc_param {
    uint32_t frame_width_in_mb;
    uint32_t frame_height_in_mb;
    uint32_t mbenc_i_frame_dist_in_use;
    uint32_t mad_enable;
    uint32_t roi_enabled;
    uint32_t brc_enabled;
    uint32_t slice_height;
    uint32_t mb_const_data_buffer_in_use;
    uint32_t mb_qp_buffer_in_use;
    uint32_t mb_vproc_stats_enable;
};

struct gen9_surface_avc {
    VADriverContextP ctx;
    VASurfaceID scaled_4x_surface_id;
    struct object_surface *scaled_4x_surface_obj;
    VASurfaceID scaled_16x_surface_id;
    struct object_surface *scaled_16x_surface_obj;
    VASurfaceID scaled_32x_surface_id;
    struct object_surface *scaled_32x_surface_obj;

    //mv code and mv data
    struct i965_gpe_resource res_mb_code_surface;
    struct i965_gpe_resource res_mv_data_surface;

    struct i965_gpe_resource res_ref_pic_select_surface;
    //dmv top/bottom
    dri_bo *dmv_top;
    dri_bo *dmv_bottom;

    int dmv_bottom_flag;
    int frame_store_id;
    int frame_idx;
    int is_as_ref;
    unsigned int qp_value;
    int top_field_order_cnt;
};

typedef struct _gen9_avc_encoder_kernel_header {
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

    // MbBRC Update
    kernel_header mb_brc_update;

    // 2x DownScaling
    kernel_header ply_2xdscale_ply;
    kernel_header ply_2xdscale_2f_ply_2f;

    //Weighted Prediction Kernel
    kernel_header wp;

    // Static frame detection Kernel
    kernel_header static_detection;
} gen9_avc_encoder_kernel_header;

/*
   The definition for Scaling
*/
typedef enum _gen9_avc_binding_table_offset_scaling {
    GEN9_AVC_SCALING_FRAME_SRC_Y_INDEX                      = 0,
    GEN9_AVC_SCALING_FRAME_DST_Y_INDEX                      = 1,
    GEN9_AVC_SCALING_FRAME_MBVPROCSTATS_DST_INDEX           = 4,
    GEN9_AVC_SCALING_NUM_SURFACES                           = 6
} gen9_avc_binding_table_offset_scaling;

typedef struct _gen9_avc_scaling4x_curbe_data {
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
        uint32_t flatness_threshold;
    } dw5;

    struct {
        uint32_t enable_mb_flatness_check;
    } dw6;

    struct {
        uint32_t enable_mb_variance_output;
    } dw7;

    struct {
        uint32_t enable_mb_pixel_average_output;
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
} gen9_avc_scaling4x_curbe_data;

typedef struct _gen9_avc_scaling2x_curbe_data {
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
} gen9_avc_scaling2x_curbe_data;

#define GEN9_AVC_KERNEL_SCALING_2X_IDX         0
#define GEN9_AVC_KERNEL_SCALING_4X_IDX         1
#define NUM_GEN9_AVC_KERNEL_SCALING            2

struct gen_avc_scaling_context {
    struct i965_gpe_context gpe_contexts[NUM_GEN9_AVC_KERNEL_SCALING];
};

/*
me structure and define
*/
typedef enum _gen9_avc_binding_table_offset_me {
    GEN9_AVC_ME_MV_DATA_SURFACE_INDEX       = 0,
    GEN9_AVC_16XME_MV_DATA_SURFACE_INDEX    = 1,
    GEN9_AVC_32XME_MV_DATA_SURFACE_INDEX    = 1,
    GEN9_AVC_ME_DISTORTION_SURFACE_INDEX    = 2,
    GEN9_AVC_ME_BRC_DISTORTION_INDEX        = 3,
    GEN9_AVC_ME_RESERVED0_INDEX             = 4,
    GEN9_AVC_ME_CURR_FOR_FWD_REF_INDEX      = 5,
    GEN9_AVC_ME_FWD_REF_IDX0_INDEX          = 6,
    GEN9_AVC_ME_RESERVED1_INDEX             = 7,
    GEN9_AVC_ME_FWD_REF_IDX1_INDEX          = 8,
    GEN9_AVC_ME_RESERVED2_INDEX             = 9,
    GEN9_AVC_ME_FWD_REF_IDX2_INDEX          = 10,
    GEN9_AVC_ME_RESERVED3_INDEX             = 11,
    GEN9_AVC_ME_FWD_REF_IDX3_INDEX          = 12,
    GEN9_AVC_ME_RESERVED4_INDEX             = 13,
    GEN9_AVC_ME_FWD_REF_IDX4_INDEX          = 14,
    GEN9_AVC_ME_RESERVED5_INDEX             = 15,
    GEN9_AVC_ME_FWD_REF_IDX5_INDEX          = 16,
    GEN9_AVC_ME_RESERVED6_INDEX             = 17,
    GEN9_AVC_ME_FWD_REF_IDX6_INDEX          = 18,
    GEN9_AVC_ME_RESERVED7_INDEX             = 19,
    GEN9_AVC_ME_FWD_REF_IDX7_INDEX          = 20,
    GEN9_AVC_ME_RESERVED8_INDEX             = 21,
    GEN9_AVC_ME_CURR_FOR_BWD_REF_INDEX      = 22,
    GEN9_AVC_ME_BWD_REF_IDX0_INDEX          = 23,
    GEN9_AVC_ME_RESERVED9_INDEX             = 24,
    GEN9_AVC_ME_BWD_REF_IDX1_INDEX          = 25,
    GEN9_AVC_ME_VDENC_STREAMIN_INDEX        = 26,
    GEN9_AVC_ME_NUM_SURFACES_INDEX          = 27
} gen9_avc_binding_table_offset_me;

typedef struct _gen9_avc_me_curbe_data {
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
        uint32_t ref_streamin_cost: 8;
        uint32_t roi_enable: 3;
        uint32_t reserved0: 5;
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
        uint32_t actual_mb_width: 16;
        uint32_t actual_mb_height: 16;
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
} gen9_avc_me_curbe_data;

#define GEN9_AVC_KERNEL_ME_P_IDX         0
#define GEN9_AVC_KERNEL_ME_B_IDX         1
#define NUM_GEN9_AVC_KERNEL_ME           2

struct gen_avc_me_context {
    struct i965_gpe_context gpe_contexts[NUM_GEN9_AVC_KERNEL_ME];
};

/*
frame/mb brc structure and define
*/
typedef enum _gen9_avc_binding_table_offset_brc_init_reset {
    GEN9_AVC_BRC_INIT_RESET_HISTORY_INDEX = 0,
    GEN9_AVC_BRC_INIT_RESET_DISTORTION_INDEX,
    GEN9_AVC_BRC_INIT_RESET_NUM_SURFACES
} gen9_avc_binding_table_offset_brc_init_reset;

typedef struct _gen9_avc_brc_init_reset_curbe_data {
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
        uint32_t average_bit_rate;
    } dw3;

    struct {
        uint32_t max_bit_rate;
    } dw4;

    struct {
        uint32_t min_bit_rate;
    } dw5;

    struct {
        uint32_t frame_rate_m;
    } dw6;

    struct {
        uint32_t frame_rate_d;
    } dw7;

    struct {
        uint32_t brc_flag: 16;
        uint32_t gop_p: 16;
    } dw8;

    struct {
        uint32_t gop_b: 16;
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
        uint32_t no_slices: 16;
    } dw12;

    struct {
        uint32_t instant_rate_threshold_0_p: 8;
        uint32_t instant_rate_threshold_1_p: 8;
        uint32_t instant_rate_threshold_2_p: 8;
        uint32_t instant_rate_threshold_3_p: 8;
    } dw13;

    struct {
        uint32_t instant_rate_threshold_0_b: 8;
        uint32_t instant_rate_threshold_1_b: 8;
        uint32_t instant_rate_threshold_2_b: 8;
        uint32_t instant_rate_threshold_3_b: 8;
    } dw14;

    struct {
        uint32_t instant_rate_threshold_0_i: 8;
        uint32_t instant_rate_threshold_1_i: 8;
        uint32_t instant_rate_threshold_2_i: 8;
        uint32_t instant_rate_threshold_3_i: 8;
    } dw15;

    struct {
        uint32_t deviation_threshold_0_pand_b: 8;
        uint32_t deviation_threshold_1_pand_b: 8;
        uint32_t deviation_threshold_2_pand_b: 8;
        uint32_t deviation_threshold_3_pand_b: 8;
    } dw16;

    struct {
        uint32_t deviation_threshold_4_pand_b: 8;
        uint32_t deviation_threshold_5_pand_b: 8;
        uint32_t deviation_threshold_6_pand_b: 8;
        uint32_t deviation_threshold_7_pand_b: 8;
    } dw17;

    struct {
        uint32_t deviation_threshold_0_vbr: 8;
        uint32_t deviation_threshold_1_vbr: 8;
        uint32_t deviation_threshold_2_vbr: 8;
        uint32_t deviation_threshold_3_vbr: 8;
    } dw18;

    struct {
        uint32_t deviation_threshold_4_vbr: 8;
        uint32_t deviation_threshold_5_vbr: 8;
        uint32_t deviation_threshold_6_vbr: 8;
        uint32_t deviation_threshold_7_vbr: 8;
    } dw19;

    struct {
        uint32_t deviation_threshold_0_i: 8;
        uint32_t deviation_threshold_1_i: 8;
        uint32_t deviation_threshold_2_i: 8;
        uint32_t deviation_threshold_3_i: 8;
    } dw20;

    struct {
        uint32_t deviation_threshold_4_i: 8;
        uint32_t deviation_threshold_5_i: 8;
        uint32_t deviation_threshold_6_i: 8;
        uint32_t deviation_threshold_7_i: 8;
    } dw21;

    struct {
        uint32_t initial_qp_i: 8;
        uint32_t initial_qp_p: 8;
        uint32_t initial_qp_b: 8;
        uint32_t sliding_window_size: 8;
    } dw22;

    struct {
        uint32_t acqp;
    } dw23;

} gen9_avc_brc_init_reset_curbe_data;

typedef enum _gen9_avc_binding_table_offset_frame_brc_update {
    GEN9_AVC_FRAME_BRC_UPDATE_HISTORY_INDEX                = 0,
    GEN9_AVC_FRAME_BRC_UPDATE_PAK_STATISTICS_OUTPUT_INDEX  = 1,
    GEN9_AVC_FRAME_BRC_UPDATE_IMAGE_STATE_READ_INDEX       = 2,
    GEN9_AVC_FRAME_BRC_UPDATE_IMAGE_STATE_WRITE_INDEX      = 3,
    GEN9_AVC_FRAME_BRC_UPDATE_MBENC_CURBE_READ_INDEX       = 4,
    GEN9_AVC_FRAME_BRC_UPDATE_MBENC_CURBE_WRITE_INDEX      = 5,
    GEN9_AVC_FRAME_BRC_UPDATE_DISTORTION_INDEX             = 6,
    GEN9_AVC_FRAME_BRC_UPDATE_CONSTANT_DATA_INDEX          = 7,
    GEN9_AVC_FRAME_BRC_UPDATE_MB_STATUS_INDEX              = 8,
    GEN9_AVC_FRAME_BRC_UPDATE_NUM_SURFACES_INDEX           = 9
} gen9_avc_binding_table_offset_frame_brc_update;

typedef struct _gen9_avc_frame_brc_update_curbe_data {
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
        uint32_t enable_force_skip: 1;
        uint32_t enable_sliding_window: 1;
        uint32_t reserved: 6;
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
        uint32_t reserved0: 8;
        uint32_t enable_roi: 8;
        uint32_t reserved1: 8;
        uint32_t reserved2: 8;
    } dw15;

    struct {
        uint32_t reserved;
    } dw16;

    struct {
        uint32_t reserved;
    } dw17;

    struct {
        uint32_t reserved;
    } dw18;

    struct {
        uint32_t user_max_frame;
    } dw19;

    struct {
        uint32_t reserved;
    } dw20;

    struct {
        uint32_t reserved;
    } dw21;

    struct {
        uint32_t reserved;
    } dw22;

    struct {
        uint32_t reserved;
    } dw23;

} gen9_avc_frame_brc_update_curbe_data;

typedef enum _gen9_avc_binding_table_offset_mb_brc_update {
    GEN9_AVC_MB_BRC_UPDATE_HISTORY_INDEX               = 0,
    GEN9_AVC_MB_BRC_UPDATE_MB_QP_INDEX                 = 1,
    GEN9_AVC_MB_BRC_UPDATE_ROI_INDEX                   = 2,
    GEN9_AVC_MB_BRC_UPDATE_MB_STATUS_INDEX             = 3,
    GEN9_AVC_MB_BRC_UPDATE_NUM_SURFACES_INDEX          = 4
} gen9_avc_binding_table_offset_mb_brc_update;

typedef struct _gen9_avc_mb_brc_curbe_data {
    struct {
        uint32_t cur_frame_type: 8;
        uint32_t enable_roi: 8;
        uint32_t roi_ratio: 8;
        uint32_t reserved0: 8;
    } dw0;

    struct {
        uint32_t reserved;
    } dw1;

    struct {
        uint32_t reserved;
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
        uint32_t reserved;
    } dw6;


} gen9_avc_mb_brc_curbe_data;

#define GEN9_AVC_KERNEL_BRC_INIT         0
#define GEN9_AVC_KERNEL_BRC_FRAME_UPDATE 1
#define GEN9_AVC_KERNEL_BRC_RESET        2
#define GEN9_AVC_KERNEL_BRC_I_FRAME_DIST 3
#define GEN9_AVC_KERNEL_BRC_BLOCK_COPY   4
#define GEN9_AVC_KERNEL_BRC_MB_UPDATE    5
#define NUM_GEN9_AVC_KERNEL_BRC          6

struct gen_avc_brc_context {
    struct i965_gpe_context gpe_contexts[NUM_GEN9_AVC_KERNEL_BRC];
};

/*
wp structure and define
*/
typedef enum _gen9_avc_binding_table_offset_wp {
    GEN9_AVC_WP_INPUT_REF_SURFACE_INDEX                 = 0,
    GEN9_AVC_WP_OUTPUT_SCALED_SURFACE_INDEX             = 1,
    GEN9_AVC_WP_NUM_SURFACES_INDEX                      = 2
} gen9_avc_binding_table_offset_wp;

typedef struct _gen9_avc_wp_curbe_data {
    struct {
        uint32_t default_weight: 16;
        uint32_t default_offset: 16;
    } dw0;

    struct {
        uint32_t roi_0_x_left: 16;
        uint32_t roi_0_y_top: 16;
    } dw1;

    struct {
        uint32_t roi_0_x_right: 16;
        uint32_t roi_0_y_bottom: 16;
    } dw2;

    struct {
        uint32_t roi_0_weight: 16;
        uint32_t roi_0_offset: 16;
    } dw3;

    struct {
        uint32_t roi_1_x_left: 16;
        uint32_t roi_1_y_top: 16;
    } dw4;

    struct {
        uint32_t roi_1_x_right: 16;
        uint32_t roi_1_y_bottom: 16;
    } dw5;

    struct {
        uint32_t roi_1_weight: 16;
        uint32_t roi_1_offset: 16;
    } dw6;

    struct {
        uint32_t roi_2_x_left: 16;
        uint32_t roi_2_y_top: 16;
    } dw7;

    struct {
        uint32_t roi_2_x_right: 16;
        uint32_t roi_2_y_bottom: 16;
    } dw8;

    struct {
        uint32_t roi_2_weight: 16;
        uint32_t roi_2_offset: 16;
    } dw9;

    struct {
        uint32_t roi_3_x_left: 16;
        uint32_t roi_3_y_top: 16;
    } dw10;

    struct {
        uint32_t roi_3_x_right: 16;
        uint32_t roi_3_y_bottom: 16;
    } dw11;

    struct {
        uint32_t roi_3_weight: 16;
        uint32_t roi_3_offset: 16;
    } dw12;

    struct {
        uint32_t roi_4_x_left: 16;
        uint32_t roi_4_y_top: 16;
    } dw13;

    struct {
        uint32_t roi_4_x_right: 16;
        uint32_t roi_4_y_bottom: 16;
    } dw14;

    struct {
        uint32_t roi_4_weight: 16;
        uint32_t roi_4_offset: 16;
    } dw15;

    struct {
        uint32_t roi_5_x_left: 16;
        uint32_t roi_5_y_top: 16;
    } dw16;

    struct {
        uint32_t roi_5_x_right: 16;
        uint32_t roi_5_y_bottom: 16;
    } dw17;

    struct {
        uint32_t roi_5_weight: 16;
        uint32_t roi_5_offset: 16;
    } dw18;

    struct {
        uint32_t roi_6_x_left: 16;
        uint32_t roi_6_y_top: 16;
    } dw19;

    struct {
        uint32_t roi_6_x_right: 16;
        uint32_t roi_6_y_bottom: 16;
    } dw20;

    struct {
        uint32_t roi_6_weight: 16;
        uint32_t roi_6_offset: 16;
    } dw21;

    struct {
        uint32_t roi_7_x_left: 16;
        uint32_t roi_7_y_top: 16;
    } dw22;

    struct {
        uint32_t roi_7_x_right: 16;
        uint32_t roi_7_y_bottom: 16;
    } dw23;

    struct {
        uint32_t roi_7_weight: 16;
        uint32_t roi_7_offset: 16;
    } dw24;

    struct {
        uint32_t roi_8_x_left: 16;
        uint32_t roi_8_y_top: 16;
    } dw25;

    struct {
        uint32_t roi_8_x_right: 16;
        uint32_t roi_8_y_bottom: 16;
    } dw26;

    struct {
        uint32_t roi_8_weight: 16;
        uint32_t roi_8_offset: 16;
    } dw27;

    struct {
        uint32_t roi_9_x_left: 16;
        uint32_t roi_9_y_top: 16;
    } dw28;

    struct {
        uint32_t roi_9_x_right: 16;
        uint32_t roi_9_y_bottom: 16;
    } dw29;

    struct {
        uint32_t roi_9_weight: 16;
        uint32_t roi_9_offset: 16;
    } dw30;

    struct {
        uint32_t roi_10_x_left: 16;
        uint32_t roi_10_y_top: 16;
    } dw31;

    struct {
        uint32_t roi_10_x_right: 16;
        uint32_t roi_10_y_bottom: 16;
    } dw32;

    struct {
        uint32_t roi_10_weight: 16;
        uint32_t roi_10_offset: 16;
    } dw33;

    struct {
        uint32_t roi_11_x_left: 16;
        uint32_t roi_11_y_top: 16;
    } dw34;

    struct {
        uint32_t roi_11_x_right: 16;
        uint32_t roi_11_y_bottom: 16;
    } dw35;

    struct {
        uint32_t roi_11_weight: 16;
        uint32_t roi_11_offset: 16;
    } dw36;

    struct {
        uint32_t roi_12_x_left: 16;
        uint32_t roi_12_y_top: 16;
    } dw37;

    struct {
        uint32_t roi_12_x_right: 16;
        uint32_t roi_12_y_bottom: 16;
    } dw38;

    struct {
        uint32_t roi_12_weight: 16;
        uint32_t roi_12_offset: 16;
    } dw39;

    struct {
        uint32_t roi_13_x_left: 16;
        uint32_t roi_13_y_top: 16;
    } dw40;

    struct {
        uint32_t roi_13_x_right: 16;
        uint32_t roi_13_y_bottom: 16;
    } dw41;

    struct {
        uint32_t roi_13_weight: 16;
        uint32_t roi_13_offset: 16;
    } dw42;

    struct {
        uint32_t roi_14_x_left: 16;
        uint32_t roi_14_y_top: 16;
    } dw43;

    struct {
        uint32_t roi_14_x_right: 16;
        uint32_t roi_14_y_bottom: 16;
    } dw44;

    struct {
        uint32_t roi_14_weight: 16;
        uint32_t roi_14_offset: 16;
    } dw45;

    struct {
        uint32_t roi_15_x_left: 16;
        uint32_t roi_15_y_top: 16;
    } dw46;

    struct {
        uint32_t roi_15_x_right: 16;
        uint32_t roi_15_y_bottom: 16;
    } dw47;

    struct {
        uint32_t roi_15_weight: 16;
        uint32_t roi_15_offset: 16;
    } dw48;

    struct {
        uint32_t input_surface;
    } dw49;

    struct {
        uint32_t output_surface;
    } dw50;



} gen9_avc_wp_curbe_data;

struct gen_avc_wp_context {
    struct i965_gpe_context gpe_contexts;
};

/*
mbenc structure and define
*/
typedef enum _gen9_avc_binding_table_offset_mbenc {
    GEN9_AVC_MBENC_MFC_AVC_PAK_OBJ_INDEX                    =  0,
    GEN9_AVC_MBENC_IND_MV_DATA_INDEX                        =  1,
    GEN9_AVC_MBENC_BRC_DISTORTION_INDEX                     =  2,    // FOR BRC DISTORTION FOR I
    GEN9_AVC_MBENC_CURR_Y_INDEX                             =  3,
    GEN9_AVC_MBENC_CURR_UV_INDEX                            =  4,
    GEN9_AVC_MBENC_MB_SPECIFIC_DATA_INDEX                   =  5,
    GEN9_AVC_MBENC_AUX_VME_OUT_INDEX                        =  6,
    GEN9_AVC_MBENC_REFPICSELECT_L0_INDEX                    =  7,
    GEN9_AVC_MBENC_MV_DATA_FROM_ME_INDEX                    =  8,
    GEN9_AVC_MBENC_4XME_DISTORTION_INDEX                    =  9,
    GEN9_AVC_MBENC_SLICEMAP_DATA_INDEX                      = 10,
    GEN9_AVC_MBENC_FWD_MB_DATA_INDEX                        = 11,
    GEN9_AVC_MBENC_FWD_MV_DATA_INDEX                        = 12,
    GEN9_AVC_MBENC_MBQP_INDEX                               = 13,
    GEN9_AVC_MBENC_MBBRC_CONST_DATA_INDEX                   = 14,
    GEN9_AVC_MBENC_VME_INTER_PRED_CURR_PIC_IDX_0_INDEX      = 15,
    GEN9_AVC_MBENC_VME_INTER_PRED_FWD_PIC_IDX0_INDEX        = 16,
    GEN9_AVC_MBENC_VME_INTER_PRED_BWD_PIC_IDX0_0_INDEX      = 17,
    GEN9_AVC_MBENC_VME_INTER_PRED_FWD_PIC_IDX1_INDEX        = 18,
    GEN9_AVC_MBENC_VME_INTER_PRED_BWD_PIC_IDX1_0_INDEX      = 19,
    GEN9_AVC_MBENC_VME_INTER_PRED_FWD_PIC_IDX2_INDEX        = 20,
    GEN9_AVC_MBENC_RESERVED0_INDEX                          = 21,
    GEN9_AVC_MBENC_VME_INTER_PRED_FWD_PIC_IDX3_INDEX        = 22,
    GEN9_AVC_MBENC_RESERVED1_INDEX                          = 23,
    GEN9_AVC_MBENC_VME_INTER_PRED_FWD_PIC_IDX4_INDEX        = 24,
    GEN9_AVC_MBENC_RESERVED2_INDEX                          = 25,
    GEN9_AVC_MBENC_VME_INTER_PRED_FWD_PIC_IDX5_INDEX        = 26,
    GEN9_AVC_MBENC_RESERVED3_INDEX                          = 27,
    GEN9_AVC_MBENC_VME_INTER_PRED_FWD_PIC_IDX6_INDEX        = 28,
    GEN9_AVC_MBENC_RESERVED4_INDEX                          = 29,
    GEN9_AVC_MBENC_VME_INTER_PRED_FWD_PIC_IDX7_INDEX        = 30,
    GEN9_AVC_MBENC_RESERVED5_INDEX                          = 31,
    GEN9_AVC_MBENC_VME_INTER_PRED_CURR_PIC_IDX_1_INDEX      = 32,
    GEN9_AVC_MBENC_VME_INTER_PRED_BWD_PIC_IDX0_1_INDEX      = 33,
    GEN9_AVC_MBENC_RESERVED6_INDEX                          = 34,
    GEN9_AVC_MBENC_VME_INTER_PRED_BWD_PIC_IDX1_1_INDEX      = 35,
    GEN9_AVC_MBENC_RESERVED7_INDEX                          = 36,
    GEN9_AVC_MBENC_MB_STATS_INDEX                           = 37,
    GEN9_AVC_MBENC_MAD_DATA_INDEX                           = 38,
    GEN9_AVC_MBENC_FORCE_NONSKIP_MB_MAP_INDEX               = 39,
    GEN9_AVC_MBENC_WIDI_WA_INDEX                            = 40,
    GEN9_AVC_MBENC_BRC_CURBE_DATA_INDEX                     = 41,
    GEN9_AVC_MBENC_SFD_COST_TABLE_INDEX                     = 42,
    GEN9_AVC_MBENC_MV_PREDICTOR_INDEX                       = 43,
    GEN9_AVC_MBENC_NUM_SURFACES_INDEX                       = 44
} gen9_avc_binding_table_offset_mbenc;

typedef struct _gen9_avc_mbenc_curbe_data {
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
        uint32_t enable_fbr_bypass: 1;
        uint32_t enable_intra_cost_scaling_for_static_frame: 1;
        uint32_t reserved0: 1;
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
        uint32_t enable_adaptive_tx_decision: 1;
        uint32_t force_non_skip_check: 1;
        uint32_t disable_enc_skip_check: 1;
        uint32_t enable_direct_bias_adjustment: 1;
        uint32_t b_force_to_skip: 1;
        uint32_t enable_global_motion_bias_adjustment: 1;
        uint32_t enable_adaptive_search_window_size: 1;
        uint32_t enable_per_mb_static_check: 1;
        uint32_t reserved0: 3;
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
        uint32_t reserved0: 4;
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
        uint32_t widi_intra_refresh_mb_num: 16;
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
        uint32_t mb_texture_threshold: 16;
        uint32_t tx_decision_threshold: 16;
    } dw58;

    struct {
        uint32_t hme_mv_cost_scaling_factor: 8;
        uint32_t reserved0: 24;
    } dw59;

    struct {
        uint32_t reserved;
    } dw60;

    struct {
        uint32_t reserved;
    } dw61;

    struct {
        uint32_t reserved;
    } dw62;

    struct {
        uint32_t reserved;
    } dw63;

    struct {
        uint32_t mb_data_surf_index;
    } dw64;

    struct {
        uint32_t mv_data_surf_index;
    } dw65;

    struct {
        uint32_t i_dist_surf_index;
    } dw66;

    struct {
        uint32_t src_y_surf_index;
    } dw67;

    struct {
        uint32_t mb_specific_data_surf_index;
    } dw68;

    struct {
        uint32_t aux_vme_out_surf_index;
    } dw69;

    struct {
        uint32_t curr_ref_pic_sel_surf_index;
    } dw70;

    struct {
        uint32_t hme_mv_pred_fwd_bwd_surf_index;
    } dw71;

    struct {
        uint32_t hme_dist_surf_index;
    } dw72;

    struct {
        uint32_t slice_map_surf_index;
    } dw73;

    struct {
        uint32_t fwd_frm_mb_data_surf_index;
    } dw74;

    struct {
        uint32_t fwd_frm_mv_surf_index;
    } dw75;

    struct {
        uint32_t mb_qp_buffer;
    } dw76;

    struct {
        uint32_t mb_brc_lut;
    } dw77;

    struct {
        uint32_t vme_inter_prediction_surf_index;
    } dw78;

    struct {
        uint32_t vme_inter_prediction_mr_surf_index;
    } dw79;

    struct {
        uint32_t mb_stats_surf_index;
    } dw80;

    struct {
        uint32_t mad_surf_index;
    } dw81;

    struct {
        uint32_t force_non_skip_mb_map_surface;
    } dw82;

    struct {
        uint32_t widi_wa_surf_index;
    } dw83;

    struct {
        uint32_t brc_curbe_surf_index;
    } dw84;

    struct {
        uint32_t static_detection_cost_table_index;
    } dw85;

    struct {
        uint32_t reserved0;
    } dw86;

    struct {
        uint32_t reserved0;
    } dw87;

} gen9_avc_mbenc_curbe_data;

#define GEN9_AVC_KERNEL_MBENC_QUALITY_I            0
#define GEN9_AVC_KERNEL_MBENC_QUALITY_P            1
#define GEN9_AVC_KERNEL_MBENC_QUALITY_B            2
#define GEN9_AVC_KERNEL_MBENC_NORMAL_I             3
#define GEN9_AVC_KERNEL_MBENC_NORMAL_P             4
#define GEN9_AVC_KERNEL_MBENC_NORMAL_B             5
#define GEN9_AVC_KERNEL_MBENC_PERFORMANCE_I        6
#define GEN9_AVC_KERNEL_MBENC_PERFORMANCE_P        7
#define GEN9_AVC_KERNEL_MBENC_PERFORMANCE_B        8
#define NUM_GEN9_AVC_KERNEL_MBENC                  9

struct gen_avc_mbenc_context {
    struct i965_gpe_context gpe_contexts[NUM_GEN9_AVC_KERNEL_MBENC];
};

/*
static frame detection structure and define
*/
typedef enum _gen9_avc_binding_table_offset_sfd {
    GEN9_AVC_SFD_VDENC_INPUT_IMAGE_STATE_INDEX                 =  0,
    GEN9_AVC_SFD_MV_DATA_SURFACE_INDEX                         =  1,
    GEN9_AVC_SFD_INTER_DISTORTION_SURFACE_INDEX                =  2,
    GEN9_AVC_SFD_OUTPUT_DATA_SURFACE_INDEX                     =  3,
    GEN9_AVC_SFD_VDENC_OUTPUT_IMAGE_STATE_INDEX                =  4,
    GEN9_AVC_SFD_NUM_SURFACES                                  =  5
} gen9_avc_binding_table_offset_sfd;

typedef struct _gen9_avc_sfd_curbe_data {
    struct {
        uint32_t vdenc_mode_disable: 1;
        uint32_t brc_mode_enable: 1;
        uint32_t slice_type: 2;
        uint32_t reserved0: 1;
        uint32_t stream_in_type: 4;
        uint32_t enable_adaptive_mv_stream_in: 1;
        uint32_t reserved1: 1;
        uint32_t enable_intra_cost_scaling_for_static_frame: 1;
        uint32_t reserved2: 20;
    } dw0;

    struct {
        uint32_t qp_value: 8;
        uint32_t num_of_refs: 8;
        uint32_t hme_stream_in_ref_cost: 8;
        uint32_t reserved0: 8;
    } dw1;

    struct {
        uint32_t frame_width_in_mbs: 16;
        uint32_t frame_height_in_mbs: 16;
    } dw2;

    struct {
        uint32_t large_mv_threshold;
    } dw3;

    struct {
        uint32_t total_large_mv_threshold;
    } dw4;

    struct {
        uint32_t zmv_threshold;
    } dw5;

    struct {
        uint32_t total_zmv_threshold;
    } dw6;

    struct {
        uint32_t min_dist_threshold;
    } dw7;

    char cost_table[52];
    struct {
        uint32_t actual_width_in_mb: 16;
        uint32_t actual_height_in_mb: 16;
    } dw21;

    struct {
        uint32_t reserved;
    } dw22;

    struct {
        uint32_t reserved;
    } dw23;

    struct {
        uint32_t vdenc_input_image_state_index;
    } dw24;

    struct {
        uint32_t reserved;
    } dw25;

    struct {
        uint32_t mv_data_surface_index;
    } dw26;

    struct {
        uint32_t inter_distortion_surface_index;
    } dw27;

    struct {
        uint32_t output_data_surface_index;
    } dw28;

    struct {
        uint32_t vdenc_output_image_state_index;
    } dw29;

} gen9_avc_sfd_curbe_data;

struct gen_avc_sfd_context {
    struct i965_gpe_context gpe_contexts;
};

/* Gen95 */

typedef struct _gen95_avc_scaling4x_curbe_data {
    struct {
        uint32_t   input_picture_width  : 16;
        uint32_t   input_picture_height : 16;
    } dw0;

    struct {
        uint32_t   input_y_bti_frame;
    } dw1;

    struct {
        uint32_t   output_y_bti_frame;
    } dw2;

    struct {
        uint32_t reserved;
    } dw3;

    struct {
        uint32_t reserved;
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
        uint32_t mbv_proc_stat_bti_frame;
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
} gen95_avc_scaling4x_curbe_data;

typedef enum _gen95_avc_binding_table_offset_frame_brc_update {
    GEN95_AVC_FRAME_BRC_UPDATE_HISTORY_INDEX                = 0,
    GEN95_AVC_FRAME_BRC_UPDATE_PAK_STATISTICS_OUTPUT_INDEX  = 1,
    GEN95_AVC_FRAME_BRC_UPDATE_IMAGE_STATE_READ_INDEX       = 2,
    GEN95_AVC_FRAME_BRC_UPDATE_IMAGE_STATE_WRITE_INDEX      = 3,
    GEN95_AVC_FRAME_BRC_UPDATE_MBENC_CURBE_WRITE_INDEX      = 4,
    GEN95_AVC_FRAME_BRC_UPDATE_DISTORTION_INDEX             = 5,
    GEN95_AVC_FRAME_BRC_UPDATE_CONSTANT_DATA_INDEX          = 6,
    GEN95_AVC_FRAME_BRC_UPDATE_MB_STATUS_INDEX              = 7,
    GEN95_AVC_FRAME_BRC_UPDATE_NUM_SURFACES_INDEX           = 8
} gen95_avc_binding_table_offset_frame_brc_update;

typedef enum _gen95_avc_binding_table_offset_mbenc {
    GEN95_AVC_MBENC_BRC_CURBE_DATA_INDEX                     = 39,
    GEN95_AVC_MBENC_FORCE_NONSKIP_MB_MAP_INDEX               = 40,
    GEN95_AVC_MBENC_WIDI_WA_INDEX                            = 41,
    GEN95_AVC_MBENC_SFD_COST_TABLE_INDEX                     = 42,
    GEN95_AVC_MBENC_NUM_SURFACES_INDEX                       = 43
} gen95_avc_binding_table_offset_mbenc;

typedef struct _gen95_avc_mbenc_curbe_data {
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
        uint32_t extended_mv_cost_range: 1;
        uint32_t reserved0: 9;
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
        uint32_t enable_fbr_bypass: 1;
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
        uint32_t tq_enable: 1;
        uint32_t force_non_skip_check: 1;
        uint32_t disable_enc_skip_check: 1;
        uint32_t enable_direct_bias_adjustment: 1;
        uint32_t b_force_to_skip: 1;
        uint32_t enable_global_motion_bias_adjustment: 1;
        uint32_t enable_adaptive_tx_decision: 1;
        uint32_t enable_per_mb_static_check: 1;
        uint32_t enable_adaptive_search_window_size: 1;
        uint32_t reserved0: 1;
        uint32_t cqp_flag: 1;
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
        uint32_t reserved0: 4;
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
        uint32_t widi_intra_refresh_mb_x: 16;
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

    union {
        struct {
            uint32_t lambda_8x8_inter: 16;
            uint32_t lambda_8x8_intra: 16;
        };
        uint32_t value;
    } dw58;

    union {
        struct {
            uint32_t lambda_inter: 16;
            uint32_t lambda_intra: 16;
        };
        uint32_t value;
    } dw59;

    struct {
        uint32_t mb_texture_threshold: 16;
        uint32_t tx_decision_threshold: 16;
    } dw60;

    struct {
        uint32_t hme_mv_cost_scaling_factor: 8;
        uint32_t reserved0: 8;
        uint32_t widi_intra_refresh_mb_y: 16;
    } dw61;

    struct {
        uint32_t reserved;
    } dw62;

    struct {
        uint32_t reserved;
    } dw63;

    struct {
        uint32_t reserved;
    } dw64;

    struct {
        uint32_t reserved;
    } dw65;

    struct {
        uint32_t mb_data_surf_index;
    } dw66;

    struct {
        uint32_t mv_data_surf_index;
    } dw67;

    struct {
        uint32_t i_dist_surf_index;
    } dw68;

    struct {
        uint32_t src_y_surf_index;
    } dw69;

    struct {
        uint32_t mb_specific_data_surf_index;
    } dw70;

    struct {
        uint32_t aux_vme_out_surf_index;
    } dw71;

    struct {
        uint32_t curr_ref_pic_sel_surf_index;
    } dw72;

    struct {
        uint32_t hme_mv_pred_fwd_bwd_surf_index;
    } dw73;

    struct {
        uint32_t hme_dist_surf_index;
    } dw74;

    struct {
        uint32_t slice_map_surf_index;
    } dw75;

    struct {
        uint32_t fwd_frm_mb_data_surf_index;
    } dw76;

    struct {
        uint32_t fwd_frm_mv_surf_index;
    } dw77;

    struct {
        uint32_t mb_qp_buffer;
    } dw78;

    struct {
        uint32_t mb_brc_lut;
    } dw79;

    struct {
        uint32_t vme_inter_prediction_surf_index;
    } dw80;

    struct {
        uint32_t vme_inter_prediction_mr_surf_index;
    } dw81;

    struct {
        uint32_t mb_stats_surf_index;
    } dw82;

    struct {
        uint32_t mad_surf_index;
    } dw83;

    struct {
        uint32_t brc_curbe_surf_index;
    } dw84;

    struct {
        uint32_t force_non_skip_mb_map_surface;
    } dw85;

    struct {
        uint32_t widi_wa_surf_index;
    } dw86;

    struct {
        uint32_t static_detection_cost_table_index;
    } dw87;

} gen95_avc_mbenc_curbe_data;

#endif /* GEN9_AVC_ENCODER_H */
