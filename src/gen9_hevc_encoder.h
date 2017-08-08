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

#ifndef GEN9_HEVC_ENCODER_H
#define GEN9_HEVC_ENCODER_H

#include <drm.h>
#include <i915_drm.h>
#include <intel_bufmgr.h>

#include <va/va.h>
#include "i965_gpe_utils.h"
#include "gen9_hevc_enc_kernels.h"

// VME parameters
struct hevc_enc_kernel_walker_parameter {
    unsigned int walker_degree;
    unsigned int use_scoreboard;
    unsigned int scoreboard_mask;
    unsigned int no_dependency;
    unsigned int resolution_x;
    unsigned int resolution_y;
};

typedef enum _GEN9_HEVC_ENC_MEDIA_STATE {
    HEVC_ENC_MEDIA_STATE_OLP                                = 0,
    HEVC_ENC_MEDIA_STATE_ENC_NORMAL                         = 1,
    HEVC_ENC_MEDIA_STATE_ENC_PERFORMANCE                    = 2,
    HEVC_ENC_MEDIA_STATE_ENC_QUALITY                        = 3,
    HEVC_ENC_MEDIA_STATE_ENC_I_FRAME_DIST                   = 4,
    HEVC_ENC_MEDIA_STATE_32X_SCALING                        = 5,
    HEVC_ENC_MEDIA_STATE_16X_SCALING                        = 6,
    HEVC_ENC_MEDIA_STATE_4X_SCALING                         = 7,
    HEVC_ENC_MEDIA_STATE_32X_ME                             = 8,
    HEVC_ENC_MEDIA_STATE_16X_ME                             = 9,
    HEVC_ENC_MEDIA_STATE_4X_ME                              = 10,
    HEVC_ENC_MEDIA_STATE_BRC_INIT_RESET                     = 11,
    HEVC_ENC_MEDIA_STATE_BRC_UPDATE                         = 12,
    HEVC_ENC_MEDIA_STATE_BRC_BLOCK_COPY                     = 13,
    HEVC_ENC_MEDIA_STATE_HYBRID_PAK_P1                      = 14,
    HEVC_ENC_MEDIA_STATE_HYBRID_PAK_P2                      = 15,
    HEVC_ENC_MEDIA_STATE_ENC_I_FRAME_CHROMA                 = 16,
    HEVC_ENC_MEDIA_STATE_ENC_I_FRAME_LUMA                   = 17,
    HEVC_ENC_MEDIA_STATE_MPU_FHB                            = 18,
    HEVC_ENC_MEDIA_STATE_TPU_FHB                            = 19,
    HEVC_ENC_MEDIA_STATE_PA_COPY                            = 20,
    HEVC_ENC_MEDIA_STATE_PL2_COPY                           = 21,
    HEVC_ENC_MEDIA_STATE_ENC_WIDI                           = 22,
    HEVC_ENC_MEDIA_STATE_2X_SCALING                         = 23,
    HEVC_ENC_MEDIA_STATE_32x32_PU_MODE_DECISION             = 24,
    HEVC_ENC_MEDIA_STATE_16x16_PU_SAD                       = 25,
    HEVC_ENC_MEDIA_STATE_16x16_PU_MODE_DECISION             = 26,
    HEVC_ENC_MEDIA_STATE_8x8_PU                             = 27,
    HEVC_ENC_MEDIA_STATE_8x8_PU_FMODE                       = 28,
    HEVC_ENC_MEDIA_STATE_32x32_B_INTRA_CHECK                = 29,
    HEVC_ENC_MEDIA_STATE_HEVC_B_MBENC                       = 30,
    HEVC_ENC_MEDIA_STATE_RESET_VLINE_STRIDE                 = 31,
    HEVC_ENC_MEDIA_STATE_HEVC_B_PAK                         = 32,
    HEVC_ENC_MEDIA_STATE_HEVC_BRC_LCU_UPDATE                = 33,
    HEVC_ENC_MEDIA_STATE_ME_VDENC_STREAMIN                  = 34,
    HEVC_ENC_MEDIA_STATE_PREPROC                            = 51,
    HEVC_ENC_MEDIA_STATE_ENC_WP                             = 52,
    HEVC_ENC_MEDIA_STATE_HEVC_I_MBENC                       = 53,
    HEVC_ENC_MEDIA_STATE_CSC_DS_COPY                        = 54,
    HEVC_ENC_MEDIA_STATE_2X_4X_SCALING                      = 55,
    HEVC_ENC_MEDIA_STATE_HEVC_LCU64_B_MBENC                 = 56,
    HEVC_ENC_MEDIA_STATE_MB_BRC_UPDATE                      = 57,
    HEVC_ENC_MEDIA_STATE_STATIC_FRAME_DETECTION             = 58,
    HEVC_ENC_MEDIA_STATE_HEVC_ROI                           = 59,
    HEVC_ENC_MEDIA_STATE_SW_SCOREBOARD_INIT                 = 60,

    HEVC_ENC_MEDIA_STATE_DS_COMBINED                        = 70,
    HEVC_ENC_NUM_MEDIA_STATES                               = 71
} GEN9_HEVC_ENC_MEDIA_STATE;

// bti table id for MBEnc/BRC kernels
typedef enum _GEN9_HEVC_ENC_SURFACE_TYPE {
    HEVC_ENC_SURFACE_RAW_Y = 0,
    HEVC_ENC_SURFACE_RAW_Y_UV,
    HEVC_ENC_SURFACE_Y_2X,
    HEVC_ENC_SURFACE_32x32_PU_OUTPUT,
    HEVC_ENC_SURFACE_SLICE_MAP,
    HEVC_ENC_SURFACE_Y_2X_VME,
    HEVC_ENC_SURFACE_BRC_INPUT,
    HEVC_ENC_SURFACE_LCU_QP,
    HEVC_ENC_SURFACE_ROI,
    HEVC_ENC_SURFACE_BRC_DATA,
    HEVC_ENC_SURFACE_KERNEL_DEBUG,
    HEVC_ENC_SURFACE_SIMPLIFIED_INTRA,
    HEVC_ENC_SURFACE_HME_MVP,
    HEVC_ENC_SURFACE_HME_DIST,
    HEVC_ENC_SURFACE_16x16PU_SAD,
    HEVC_ENC_SURFACE_RAW_VME,
    HEVC_ENC_SURFACE_VME_8x8,
    HEVC_ENC_SURFACE_CU_RECORD,
    HEVC_ENC_SURFACE_INTRA_MODE,
    HEVC_ENC_SURFACE_HCP_PAK,
    HEVC_ENC_SURFACE_INTRA_DIST,
    HEVC_ENC_SURFACE_MIN_DIST,
    HEVC_ENC_SURFACE_VME_UNI_SIC_DATA,
    HEVC_ENC_SURFACE_COL_MB_MV,
    HEVC_ENC_SURFACE_CONCURRENT_THREAD,
    HEVC_ENC_SURFACE_MB_MV_INDEX,
    HEVC_ENC_SURFACE_MVP_INDEX,
    HEVC_ENC_SURFACE_REF_FRAME_VME,
    HEVC_ENC_SURFACE_Y_4X,
    HEVC_ENC_SURFACE_Y_4X_VME,
    HEVC_ENC_SURFACE_BRC_HISTORY,
    HEVC_ENC_SURFACE_BRC_ME_DIST,
    HEVC_ENC_SURFACE_BRC_PAST_PAK_INFO,
    HEVC_ENC_SURFACE_BRC_HCP_PIC_STATE,
    HEVC_ENC_SURFACE_RAW_10bit_Y,
    HEVC_ENC_SURFACE_RAW_10bit_Y_UV,
    HEVC_ENC_SURFACE_RAW_FC_8bit_Y,
    HEVC_ENC_SURFACE_RAW_FC_8bit_Y_UV,
    HEVC_ENC_SURFACE_RAW_MBSTAT,
    HEVC_ENC_SURFACE_TYPE_NUM
} GEN9_HEVC_ENC_SURFACE_TYPE;

struct gen9_hevc_surface_parameter {
    struct i965_gpe_resource    *gpe_resource;
    struct object_surface       *surface_object;
    dri_bo *bo;
};

#define GEN9_HEVC_ENC_MIN_LCU_SIZE      16
#define GEN9_HEVC_ENC_MAX_LCU_SIZE      32

#define MAX_HEVC_KERNELS_ENCODER_SURFACES        64
#define MAX_HEVC_KERNELS_URB_SIZE                4096

#define MAX_HEVC_MAX_NUM_ROI            16

// target usage mode
enum HEVC_TU_MODE {
    HEVC_TU_UNKNOWN         = 0,

    HEVC_TU_BEST_QUALITY    = 1,
    HEVC_TU_HI_QUALITY      = 2,
    HEVC_TU_OPT_QUALITY     = 3,
    HEVC_TU_OK_QUALITY      = 5,

    HEVC_TU_NO_SPEED        = 1,
    HEVC_TU_OPT_SPEED       = 3,
    HEVC_TU_RT_SPEED        = 4,
    HEVC_TU_HI_SPEED        = 6,
    HEVC_TU_BEST_SPEED      = 7,

    NUM_TU_MODES
};

enum HEVC_BRC_METHOD {
    HEVC_BRC_CBR,
    HEVC_BRC_VBR,
    HEVC_BRC_CQP,
    HEVC_BRC_AVBR,
    HEVC_BRC_ICQ,
    HEVC_BRC_VCM,
    //HEVC_BRC_QVBR,
    //HEVC_BRC_IWD_VBR
};

struct gen9_hevc_scaling_parameter {
    VASurfaceID              curr_pic;
    void                     *p_scaling_bti;
    struct object_surface    *input_surface;
    struct object_surface    *output_surface;
    unsigned int             input_frame_width;
    unsigned int             input_frame_height;
    unsigned int             output_frame_width;
    unsigned int             output_frame_height;
    unsigned int             vert_line_stride;
    unsigned int             vert_line_stride_offset;
    bool                     scaling_out_use_16unorm_surf_fmt;
    bool                     scaling_out_use_32unorm_surf_fmt;
    bool                     mbv_proc_stat_enabled;
    bool                     enable_mb_flatness_check;
    bool                     enable_mb_variance_output;
    bool                     enable_mb_pixel_average_output;
    bool                     use_4x_scaling;
    bool                     use_16x_scaling;
    bool                     use_32x_scaling;
    bool                     blk8x8_stat_enabled;
    struct i965_gpe_resource *pres_mbv_proc_stat_buffer;
};

enum HEVC_HME_TYPE {
    HEVC_HME_4X,
    HEVC_HME_16X,
    HEVC_HME_32X
};

#define GEN9_HEVC_HME_SUPPORTED     1
#define GEN9_HEVC_16XME_SUPPORTED   1
#define GEN9_HEVC_32XME_SUPPORTED   0

#define GEN9_HEVC_VME_MIN_ALLOWED_SIZE       48

#define HEVC_ENC_SCALING_4X                  0
#define HEVC_ENC_SCALING_16X                 1
#define HEVC_ENC_SCALING_32X                 2
#define NUM_HEVC_ENC_SCALING                 3

struct gen9_hevc_scaling_context {
    struct i965_gpe_context gpe_contexts[NUM_HEVC_ENC_SCALING];
};

#define HEVC_ENC_ME_B                    0
#define HEVC_ENC_ME_P                    1
#define NUM_HEVC_ENC_ME                  2
#define NUM_HEVC_ENC_ME_TYPES            3

struct gen9_hevc_me_context {
    struct i965_gpe_context gpe_context[NUM_HEVC_ENC_ME_TYPES][NUM_HEVC_ENC_ME];
};

#define HEVC_ENC_INTRA_TRANS_REGULAR       0
#define HEVC_ENC_INTRA_TRANS_RESERVED      1
#define HEVC_ENC_INTRA_TRANS_HAAR          2
#define HEVC_ENC_INTRA_TRANS_HADAMARD      3

#define MBENC_IDX(krn) (krn - GEN9_HEVC_ENC_MBENC_2xSCALING)

#define HEVC_MBENC_2xSCALING_IDX            MBENC_IDX(GEN9_HEVC_ENC_MBENC_2xSCALING)
#define HEVC_MBENC_32x32MD_IDX              MBENC_IDX(GEN9_HEVC_ENC_MBENC_32x32MD)
#define HEVC_MBENC_16x16SAD_IDX             MBENC_IDX(GEN9_HEVC_ENC_MBENC_16x16SAD)
#define HEVC_MBENC_16x16MD_IDX              MBENC_IDX(GEN9_HEVC_ENC_MBENC_16x16MD)
#define HEVC_MBENC_8x8PU_IDX                MBENC_IDX(GEN9_HEVC_ENC_MBENC_8x8PU)
#define HEVC_MBENC_8x8FMODE_IDX             MBENC_IDX(GEN9_HEVC_ENC_MBENC_8x8FMODE)
#define HEVC_MBENC_32x32INTRACHECK_IDX      MBENC_IDX(GEN9_HEVC_ENC_MBENC_32x32INTRACHECK)
#define HEVC_MBENC_BENC_IDX                 MBENC_IDX(GEN9_HEVC_ENC_MBENC_BENC)
#define HEVC_MBENC_BPAK_IDX                 MBENC_IDX(GEN9_HEVC_ENC_MBENC_BPAK)
#define HEVC_MBENC_MBENC_WIDI_IDX           MBENC_IDX(GEN9_HEVC_ENC_MBENC_WIDI)
#define HEVC_MBENC_DS_COMBINED_IDX          MBENC_IDX(GEN9_HEVC_ENC_MBENC_DS_COMBINED)
#define HEVC_MBENC_PENC_IDX                 MBENC_IDX(GEN9_HEVC_MBENC_PENC)
#define HEVC_MBENC_P_WIDI_IDX               MBENC_IDX(GEN9_HEVC_MBENC_P_WIDI)

static const int hevc_mbenc_curbe_size[GEN8_HEVC_ENC_MBENC_TOTAL_NUM] = {
    sizeof(gen9_hevc_mbenc_downscaling2x_curbe_data),
    sizeof(gen9_hevc_mbenc_32x32_pu_mode_curbe_data),
    sizeof(gen9_hevc_mbenc_16x16_sad_curbe_data),
    sizeof(gen9_hevc_enc_16x16_pu_curbe_data),
    sizeof(gen9_hevc_mbenc_8x8_pu_curbe_data),
    sizeof(gen9_hevc_mbenc_8x8_pu_fmode_curbe_data),
    sizeof(gen9_hevc_mbenc_b_32x32_pu_intra_curbe_data),
    sizeof(gen9_hevc_mbenc_b_mb_enc_curbe_data),
    sizeof(gen9_hevc_mbenc_b_pak_curbe_data),
    sizeof(gen9_hevc_mbenc_b_mb_enc_curbe_data),
    sizeof(gen95_hevc_mbenc_ds_combined_curbe_data),
    sizeof(gen9_hevc_mbenc_b_mb_enc_curbe_data),
    sizeof(gen9_hevc_mbenc_b_mb_enc_curbe_data),
};

#define GEN9_HEVC_MEDIA_WALKER_MAX_COLORS        16

struct gen9_hevc_walking_pattern_parameter {
    struct gpe_media_object_walker_parameter gpe_param;

    int offset_y;
    int offset_delta;
    unsigned int num_region;
    unsigned int max_height_in_region;
    unsigned int num_units_in_region;
};

struct gen9_hevc_mbenc_context {
    struct i965_gpe_context gpe_contexts[GEN8_HEVC_ENC_MBENC_TOTAL_NUM];

    int kernel_num;
};

#define HEVC_BRC_KBPS                        1000
#define HEVC_BRC_MIN_TARGET_PERCENTAGE       50

#define BRC_IDX(krn) (krn - GEN9_HEVC_ENC_BRC_COARSE_INTRA)

#define HEVC_BRC_COARSE_INTRA_IDX           BRC_IDX(GEN9_HEVC_ENC_BRC_COARSE_INTRA)
#define HEVC_BRC_INIT_IDX                   BRC_IDX(GEN9_HEVC_ENC_BRC_INIT)
#define HEVC_BRC_RESET_IDX                  BRC_IDX(GEN9_HEVC_ENC_BRC_RESET)
#define HEVC_BRC_FRAME_UPDATE_IDX           BRC_IDX(GEN9_HEVC_ENC_BRC_FRAME_UPDATE)
#define HEVC_BRC_LCU_UPDATE_IDX             BRC_IDX(GEN9_HEVC_ENC_BRC_LCU_UPDATE)

static int hevc_brc_curbe_size[GEN9_HEVC_ENC_BRC_NUM] = {
    sizeof(gen9_hevc_brc_coarse_intra_curbe_data),
    sizeof(gen9_hevc_brc_initreset_curbe_data),
    sizeof(gen9_hevc_brc_initreset_curbe_data),
    sizeof(gen9_hevc_brc_udpate_curbe_data),
    sizeof(gen9_hevc_brc_udpate_curbe_data),
};

struct gen9_hevc_brc_context {
    struct i965_gpe_context gpe_contexts[GEN9_HEVC_ENC_BRC_NUM];
};

struct gen9_hevc_slice_map {
    unsigned char slice_id;
    unsigned char reserved[3];
};

// PAK paramerters
#define GEN9_HEVC_ENC_BRC_PIC_STATE_SIZE      (128)
#define GEN9_HEVC_ENC_BRC_PAK_STATISTCS_SIZE  (32)
#define GEN9_HEVC_ENC_BRC_HISTORY_BUFFER_SIZE (576)

#define GEN9_HEVC_ENC_BRC_CONSTANT_SURFACE_WIDTH          (64)
#define GEN9_HEVC_ENC_BRC_CONSTANT_SURFACE_HEIGHT         (53)

#define GEN9_HEVC_ENC_PAK_OBJ_SIZE      (3 + 1)
#define GEN95_HEVC_ENC_PAK_OBJ_SIZE     (5 + 3)

#define GEN9_HEVC_ENC_PAK_CU_RECORD_SIZE    (16)

#define GEN9_HEVC_ENC_PAK_SLICE_STATE_SIZE 4096

#define GEN9_MAX_REF_SURFACES                    8
#define GEN9_MAX_MV_TEMPORAL_BUFFERS             (GEN9_MAX_REF_SURFACES + 1)

#define GEN9_HEVC_NUM_MAX_REF_L0  3
#define GEN9_HEVC_NUM_MAX_REF_L1  1

enum GEN9_HEVC_ENC_SURFACE_TYPE {
    GEN9_HEVC_ENC_SURFACE_RECON = 0,
    GEN9_HEVC_ENC_SURFACE_SOURCE = 1
};

#define HEVC_SCALED_SURF_4X_ID       0
#define HEVC_SCALED_SURF_16X_ID      1
#define HEVC_SCALED_SURF_32X_ID      2
#define HEVC_SCALED_SURFS_NUM       (HEVC_SCALED_SURF_32X_ID + 1)

struct gen9_hevc_surface_priv {
    VADriverContextP ctx;

    dri_bo                  *motion_vector_temporal_bo;

    VASurfaceID             scaled_surface_id[HEVC_SCALED_SURFS_NUM];
    struct object_surface   *scaled_surface_obj[HEVC_SCALED_SURFS_NUM];

    VASurfaceID             surface_id_nv12;
    struct object_surface   *surface_obj_nv12;
    int surface_nv12_valid;

    struct object_surface   *surface_reff;

    int qp_value;
};

struct hevc_enc_image_status_ctrl {
    unsigned int lcu_max_size_violate: 1;
    unsigned int frame_bit_count_violate_overrun: 1;
    unsigned int frame_bit_count_violate_underrun: 1;
    unsigned int reserverd1: 5;
    unsigned int total_pass: 4;
    unsigned int reserverd2: 4;
    unsigned int cumulative_frame_delta_lf: 7;
    unsigned int reserverd3: 1;
    unsigned int cumulative_frame_delta_qp: 8;
};

struct hevc_encode_status {
    // MUST don't move two fields image_status_mask
    // and image_status_ctrl
    unsigned int image_status_mask;
    unsigned int image_status_ctrl;
    unsigned int image_status_ctrl_last_pass;
    unsigned int bs_byte_count;
    unsigned int pass_num;
    unsigned int media_state;
};

#define MMIO_HCP_ENC_BITSTREAM_BYTECOUNT_FRAME_OFFSET           0x1E9A0
#define MMIO_HCP_ENC_BITSTREAM_BYTECOUNT_FRAME_NO_HEADER_OFFSET 0x1E9A4
#define MMIO_HCP_ENC_IMAGE_STATUS_MASK_OFFSET                   0x1E9B8
#define MMIO_HCP_ENC_IMAGE_STATUS_CTRL_OFFSET                   0x1E9BC

struct hevc_encode_status_buffer {
    dri_bo *bo;

    unsigned int status_bs_byte_count_offset;
    unsigned int status_image_mask_offset;
    unsigned int status_image_ctrl_offset;
    unsigned int status_image_ctrl_last_pass_offset;

    unsigned int mmio_bs_frame_offset;
    unsigned int mmio_bs_frame_no_header_offset;
    unsigned int mmio_image_mask_offset;
    unsigned int mmio_image_ctrl_offset;

    unsigned int status_pass_num_offset;
    unsigned int status_media_state_offset;
};

struct gen9_hevc_encoder_state {
    int picture_width;
    int picture_height;
    unsigned int picture_coding_type;
    unsigned int bit_depth_luma_minus8;
    unsigned int bit_depth_chroma_minus8;

    int cu_size;
    int lcu_size;
    int width_in_lcu;
    int height_in_lcu;
    int width_in_cu;
    int height_in_cu;
    int width_in_mb;
    int height_in_mb;

    int mb_data_offset;
    int mb_code_size;
    int pak_obj_size;
    int cu_record_size;
    int pic_state_size;
    int slice_batch_offset[I965_MAX_NUM_SLICE];
    int slice_start_lcu[I965_MAX_NUM_SLICE];

    struct hevc_encode_status_buffer status_buffer;
    enum HEVC_TU_MODE tu_mode;

    //HME width&height
    unsigned int frame_width_in_max_lcu;
    unsigned int frame_height_in_max_lcu;
    unsigned int frame_width_4x;
    unsigned int frame_height_4x;
    unsigned int frame_width_16x;
    unsigned int frame_height_16x;
    unsigned int frame_width_32x;
    unsigned int frame_height_32x;
    unsigned int downscaled_width_4x_in_mb;
    unsigned int downscaled_height_4x_in_mb;
    unsigned int downscaled_width_16x_in_mb;
    unsigned int downscaled_height_16x_in_mb;
    unsigned int downscaled_width_32x_in_mb;
    unsigned int downscaled_height_32x_in_mb;

    unsigned int flatness_check_enable    : 1;
    unsigned int flatness_check_supported : 1;
    unsigned int res_bits                 : 30;

    // brc state
    enum HEVC_BRC_METHOD brc_method;
    unsigned int frames_per_100s;
    unsigned int user_max_frame_size;
    unsigned int init_vbv_buffer_fullness_in_bit;
    unsigned int vbv_buffer_size_in_bit;
    unsigned int target_bit_rate_in_kbs;
    unsigned int max_bit_rate_in_kbs;
    unsigned int min_bit_rate_in_kbs;
    unsigned int gop_size;
    unsigned int gop_ref_dist;
    unsigned int crf_quality_factor;
    unsigned int num_b_in_gop[3];
    double brc_init_current_target_buf_full_in_bits;
    double brc_init_reset_input_bits_per_frame;
    unsigned int brc_init_reset_buf_size_in_bits;
    unsigned int num_skip_frames;
    unsigned int size_skip_frames;
    unsigned int frame_number;
    unsigned int parallel_brc;

    unsigned int num_roi;
    unsigned int roi_value_is_qp_delta;
    unsigned int max_delta_qp;
    unsigned int min_delta_qp;
    struct intel_roi roi[MAX_HEVC_MAX_NUM_ROI];

    unsigned int video_surveillance_flag;

    unsigned int lcu_brc_enabled                : 1;
    unsigned int low_delay                      : 1;
    unsigned int arbitrary_num_mb_in_slice      : 1;
    unsigned int rolling_intra_refresh          : 1;
    unsigned int walking_pattern_26             : 1;
    unsigned int use_hw_scoreboard              : 1;
    unsigned int use_hw_non_stalling_scoreborad : 1;
    unsigned int power_saving                   : 1;
    unsigned int reserverd                      : 24;

    unsigned int fixed_point_lambda_for_luma;
    unsigned int fixed_point_lambda_for_chroma;

    int num_regions_in_slice;

    unsigned int widi_first_intra_refresh;
    unsigned int widi_frame_num_in_gob;
    unsigned int widi_frame_num_without_intra_refresh;
    unsigned int widi_intra_insertion_location;
    unsigned int widi_intra_insertion_size;
    unsigned int widi_intra_refresh_qp_delta;

    unsigned int ctu_max_bitsize_allowed;
};

struct gen9_hevc_encoder_context {
    struct gen9_hevc_scaling_context scaling_context;
    struct gen9_hevc_me_context me_context;
    struct gen9_hevc_mbenc_context mbenc_context;
    struct gen9_hevc_brc_context brc_context;
    VADriverContextP ctx;

    struct gen9_hevc_surface_parameter gpe_surfaces[HEVC_ENC_SURFACE_TYPE_NUM];

    double lambda_md_table[3][52];
    double lambda_me_table[3][52];
    unsigned int lambda_table_inited;
    unsigned int lambda_intra_trans_type;

    VASurfaceID scaled_2x_surface_id;
    struct object_surface *scaled_2x_surface_obj;

    unsigned int mocs;

    // PAK internal pipe buffers
    struct i965_gpe_resource deblocking_filter_line_buffer;
    struct i965_gpe_resource deblocking_filter_tile_line_buffer;
    struct i965_gpe_resource deblocking_filter_tile_column_buffer;
    struct i965_gpe_resource metadata_line_buffer;
    struct i965_gpe_resource metadata_tile_line_buffer;
    struct i965_gpe_resource metadata_tile_column_buffer;
    struct i965_gpe_resource sao_line_buffer;
    struct i965_gpe_resource sao_tile_line_buffer;
    struct i965_gpe_resource sao_tile_column_buffer;

    struct {
        dri_bo *bo;
        int offset;
        int end_offset;
        int status_offset;
    } indirect_pak_bse_object;

    struct {
        struct object_surface *obj_surface;
        VASurfaceID surface_id;
    } uncompressed_picture_source;

    struct {
        struct object_surface *obj_surface;
        VASurfaceID surface_id;
    } reconstructed_object;

    struct {
        struct object_surface *obj_surface;
        VASurfaceID surface_id;
    } reference_surfaces[GEN9_MAX_REF_SURFACES];

    struct {
        dri_bo *bo;
    } mv_temporal_buffer[GEN9_MAX_MV_TEMPORAL_BUFFERS];

    int res_inited;
    struct intel_batchbuffer *res_pak_slice_batch_buffer;
    struct i965_gpe_resource res_mb_code_surface;
    struct i965_gpe_resource res_brc_pic_states_write_buffer;
    struct i965_gpe_resource res_brc_pic_states_read_buffer;
    struct i965_gpe_resource res_brc_history_buffer;
    struct i965_gpe_resource res_brc_intra_dist_buffer;
    struct i965_gpe_resource res_brc_me_dist_buffer;
    struct i965_gpe_resource res_brc_input_buffer_for_enc_kernels;
    struct i965_gpe_resource res_brc_pak_statistic_buffer;
    struct i965_gpe_resource res_brc_constant_data_buffer;
    struct i965_gpe_resource res_brc_mb_qp_buffer;
    struct i965_gpe_resource res_flatness_check_surface;
    struct i965_gpe_resource s4x_memv_distortion_buffer;
    struct i965_gpe_resource s4x_memv_data_buffer;
    struct i965_gpe_resource s16x_memv_data_buffer;
    struct i965_gpe_resource s32x_memv_data_buffer;
    struct i965_gpe_resource res_32x32_pu_output_buffer;
    struct i965_gpe_resource res_slice_map_buffer;
    struct i965_gpe_resource res_simplest_intra_buffer;
    struct i965_gpe_resource res_kernel_debug;
    struct i965_gpe_resource res_sad_16x16_pu_buffer;
    struct i965_gpe_resource res_vme_8x8_mode_buffer;
    struct i965_gpe_resource res_intra_mode_buffer;
    struct i965_gpe_resource res_intra_distortion_buffer;
    struct i965_gpe_resource res_min_distortion_buffer;
    struct i965_gpe_resource res_vme_uni_sic_buffer;
    struct i965_gpe_resource res_con_corrent_thread_buffer;
    struct i965_gpe_resource res_mv_index_buffer;
    struct i965_gpe_resource res_mvp_index_buffer;
    struct i965_gpe_resource res_roi_buffer;
    struct i965_gpe_resource res_mb_statistics_buffer;
};

#endif
