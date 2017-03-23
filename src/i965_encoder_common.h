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

#ifndef _I965_COMMON_ENCODER_H
#define _I965_COMMON_ENCODER_H

#include <drm.h>
#include <i915_drm.h>
#include <intel_bufmgr.h>

#include <va/va.h>
#include "i965_encoder.h"
#include "i965_gpe_utils.h"

struct encode_state;
struct intel_encoder_context;

/*
   this file define the common structure for encoder, such as H264/H265/VP8/VP9
*/
#define INTEL_BRC_NONE   0x00000001
#define INTEL_BRC_CBR    0x00000002
#define INTEL_BRC_VBR    0x00000004
#define INTEL_BRC_CQP    0x00000010
#define INTEL_BRC_AVBR   0x00000040

#define INTEL_BRC_INIT_FLAG_CBR                        0x0010,
#define INTEL_BRC_INIT_FLAG_VBR                        0x0020,
#define INTEL_BRC_INIT_FLAG_AVBR                       0x0040,
#define INTEL_BRC_INIT_FLAG_CQL                        0x0080,
#define INTEL_BRC_INIT_FLAG_FIELD_PIC                  0x0100,
#define INTEL_BRC_INIT_FLAG_ICQ                        0x0200,
#define INTEL_BRC_INIT_FLAG_VCM                        0x0400,
#define INTEL_BRC_INIT_FLAG_IGNORE_PICTURE_HEADER_SIZE 0x2000,
#define INTEL_BRC_INIT_FLAG_QVBR                       0x4000,
#define INTEL_BRC_INIT_FLAG_DISABLE_MBBRC              0x8000


#define INTEL_BRC_UPDATE_FLAG_FIELD                  0x01,
#define INTEL_BRC_UPDATE_FLAG_MBAFF                  (0x01 << 1),
#define INTEL_BRC_UPDATE_FLAG_BOTTOM_FIELD           (0x01 << 2),
#define INTEL_BRC_UPDATE_FLAG_ACTUALQP               (0x01 << 6),
#define INTEL_BRC_UPDATE_FLAG_REFERENCE              (0x01 << 7)

#define INTEL_VME_MIN_ALLOWED_WIDTH_HEIGHT           48

#define INTEL_ROI_NUM                    4

extern const unsigned int table_enc_search_path[2][8][16];

// BRC Flag in BRC Init Kernel
typedef enum _INTEL_ENCODE_BRCINIT_FLAG {
    INTEL_ENCODE_BRCINIT_ISCBR                       = 0x0010,
    INTEL_ENCODE_BRCINIT_ISVBR                       = 0x0020,
    INTEL_ENCODE_BRCINIT_ISAVBR                      = 0x0040,
    INTEL_ENCODE_BRCINIT_ISCQL                       = 0x0080,
    INTEL_ENCODE_BRCINIT_FIELD_PIC                   = 0x0100,
    INTEL_ENCODE_BRCINIT_ISICQ                       = 0x0200,
    INTEL_ENCODE_BRCINIT_ISVCM                       = 0x0400,
    INTEL_ENCODE_BRCINIT_IGNORE_PICTURE_HEADER_SIZE  = 0x2000,
    INTEL_ENCODE_BRCINIT_ISQVBR                      = 0x4000,
    INTEL_ENCODE_BRCINIT_DISABLE_MBBRC               = 0x8000
} INTEL_ENCODE_BRCINIT_FLAG;

// BRC Flag in BRC Update Kernel
typedef enum _INTEL_ENCODE_BRCUPDATE_FLAG {
    INTEL_ENCODE_BRCUPDATE_IS_FIELD                  = 0x01,
    INTEL_ENCODE_BRCUPDATE_IS_MBAFF                  = (0x01 << 1),
    INTEL_ENCODE_BRCUPDATE_IS_BOTTOM_FIELD           = (0x01 << 2),
    INTEL_ENCODE_BRCUPDATE_IS_ACTUALQP               = (0x01 << 6),
    INTEL_ENCODE_BRCUPDATE_IS_REFERENCE              = (0x01 << 7)
} INTEL_ENCODE_BRCUPDATE_FLAG;

/*
kernel operation related defines
*/
typedef enum _INTEL_GENERIC_ENC_OPERATION {
    INTEL_GENERIC_ENC_SCALING4X = 0,
    INTEL_GENERIC_ENC_SCALING2X,
    INTEL_GENERIC_ENC_ME,
    INTEL_GENERIC_ENC_BRC,
    INTEL_GENERIC_ENC_MBENC,
    INTEL_GENERIC_ENC_MBENC_WIDI,
    INTEL_GENERIC_ENC_WP,
    INTEL_GENERIC_ENC_SFD,                   // Static frame detection
    INTEL_GENERIC_ENC_DYS
} INTEL_GENERIC_ENC_OPERATION;

typedef enum _INTEL_MEDIA_STATE_TYPE {
    INTEL_MEDIA_STATE_OLP                                = 0,
    INTEL_MEDIA_STATE_ENC_NORMAL                         = 1,
    INTEL_MEDIA_STATE_ENC_PERFORMANCE                    = 2,
    INTEL_MEDIA_STATE_ENC_QUALITY                        = 3,
    INTEL_MEDIA_STATE_ENC_I_FRAME_DIST                   = 4,
    INTEL_MEDIA_STATE_32X_SCALING                        = 5,
    INTEL_MEDIA_STATE_16X_SCALING                        = 6,
    INTEL_MEDIA_STATE_4X_SCALING                         = 7,
    INTEL_MEDIA_STATE_32X_ME                             = 8,
    INTEL_MEDIA_STATE_16X_ME                             = 9,
    INTEL_MEDIA_STATE_4X_ME                              = 10,
    INTEL_MEDIA_STATE_BRC_INIT_RESET                     = 11,
    INTEL_MEDIA_STATE_BRC_UPDATE                         = 12,
    INTEL_MEDIA_STATE_BRC_BLOCK_COPY                     = 13,
    INTEL_MEDIA_STATE_PA_COPY                            = 20,
    INTEL_MEDIA_STATE_PL2_COPY                           = 21,
    INTEL_MEDIA_STATE_ENC_WIDI                           = 22,
    INTEL_MEDIA_STATE_2X_SCALING                         = 23,
    INTEL_MEDIA_STATE_32x32_PU_MODE_DECISION             = 24,
    INTEL_MEDIA_STATE_16x16_PU_SAD                       = 25,
    INTEL_MEDIA_STATE_16x16_PU_MODE_DECISION             = 26,
    INTEL_MEDIA_STATE_8x8_PU                             = 27,
    INTEL_MEDIA_STATE_8x8_PU_FMODE                       = 28,
    INTEL_MEDIA_STATE_32x32_B_INTRA_CHECK                = 29,
    INTEL_MEDIA_STATE_HEVC_B_MBENC                       = 30,
    INTEL_MEDIA_STATE_HEVC_B_PAK                         = 32,
    INTEL_MEDIA_STATE_HEVC_BRC_LCU_UPDATE                = 33,
    INTEL_MEDIA_STATE_VP9_ENC_I_32x32                    = 35,
    INTEL_MEDIA_STATE_VP9_ENC_I_16x16                    = 36,
    INTEL_MEDIA_STATE_VP9_ENC_P                          = 37,
    INTEL_MEDIA_STATE_VP9_ENC_TX                         = 38,
    INTEL_MEDIA_STATE_VP9_DYS                            = 39,
    INTEL_MEDIA_STATE_PREPROC                            = 51,
    INTEL_MEDIA_STATE_ENC_WP                             = 52,
    INTEL_MEDIA_STATE_HEVC_I_MBENC                       = 53,
    INTEL_MEDIA_STATE_CSC_DS_COPY                        = 54,
    INTEL_MEDIA_STATE_2X_4X_SCALING                      = 55,
    INTEL_MEDIA_STATE_HEVC_LCU64_B_MBENC                 = 56,
    INTEL_MEDIA_STATE_MB_BRC_UPDATE                      = 57,
    INTEL_MEDIA_STATE_STATIC_FRAME_DETECTION             = 58,
    INTEL_MEDIA_STATE_HEVC_ROI                           = 59,
    INTEL_MEDIA_STATE_SW_SCOREBOARD_INIT                 = 60,
    INTEL_NUM_MEDIA_STATES                               = 61
} INTEL_MEDIA_STATE_TYPE;

typedef enum {
    INTEL_ROLLING_I_DISABLED  = 0,
    INTEL_ROLLING_I_COLUMN    = 1,
    INTEL_ROLLING_I_ROW       = 2,
    INTEL_ROLLING_I_SQUARE    = 3
} INTEL_ROLLING_I_SETTING;

struct encoder_kernel_parameter {
    unsigned int curbe_size;
    unsigned int inline_data_size;
    unsigned int sampler_size;
};

struct encoder_scoreboard_parameter {
    unsigned int mask;
    unsigned int type;
    unsigned int enable;
    unsigned int walkpat_flag;
};


/*
ME related defines
*/
#define INTEL_ENC_HME_4x    0
#define INTEL_ENC_HME_16x   1
#define INTEL_ENC_HME_32x   2

/*
   the definition for rate control
*/
#define GENERIC_BRC_SEQ         0x01
#define GENERIC_BRC_HRD         0x02
#define GENERIC_BRC_RC          0x04
#define GENERIC_BRC_FR          0x08
#define GENERIC_BRC_FAILURE     (1 << 31)

enum INTEL_ENC_KERNAL_MODE {
    INTEL_ENC_KERNEL_QUALITY      = 0,
    INTEL_ENC_KERNEL_NORMAL,
    INTEL_ENC_KERNEL_PERFORMANCE
};

enum INTEL_ENC_PRESET_MODE {
    INTEL_PRESET_UNKNOWN         = 0,
    INTEL_PRESET_BEST_QUALITY    = 1,
    INTEL_PRESET_HI_QUALITY      = 2,
    INTEL_PRESET_OPT_QUALITY     = 3,
    INTEL_PRESET_OK_QUALITY      = 5,
    INTEL_PRESET_NO_SPEED        = 1,
    INTEL_PRESET_OPT_SPEED       = 3,
    INTEL_PRESET_RT_SPEED        = 4,
    INTEL_PRESET_HI_SPEED        = 6,
    INTEL_PRESET_BEST_SPEED      = 7,
    INTEL_PRESET_LOW_LATENCY     = 0x10,
    INTEL_PRESET_MULTIPASS       = 0x20
};

/*
   the definition for encoder VME/PAK context
*/


struct generic_encoder_context {
    // kernel pointer
    void * enc_kernel_ptr;
    uint32_t enc_kernel_size;
    //scoreboard
    uint32_t use_hw_scoreboard;
    uint32_t use_hw_non_stalling_scoreboard;
    //input surface
    struct i965_gpe_resource res_uncompressed_input_surface;
    //reconstructed surface
    struct i965_gpe_resource res_reconstructed_surface;
    //output bitstream
    struct {
        struct i965_gpe_resource res;
        uint32_t start_offset;
        uint32_t end_offset;
    } compressed_bitstream;

    //curbe set function pointer
    void (*pfn_set_curbe_scaling2x)(
        VADriverContextP ctx,
        struct encode_state *encode_state,
        struct i965_gpe_context *gpe_context,
        struct intel_encoder_context *encoder_context,
        void *param);

    void (*pfn_set_curbe_scaling4x)(
        VADriverContextP ctx,
        struct encode_state *encode_state,
        struct i965_gpe_context *gpe_context,
        struct intel_encoder_context *encoder_context,
        void *param);

    void (*pfn_set_curbe_me)(
        VADriverContextP ctx,
        struct encode_state *encode_state,
        struct i965_gpe_context *gpe_context,
        struct intel_encoder_context *encoder_context,
        void *param);

    void (*pfn_set_curbe_mbenc)(
        VADriverContextP ctx,
        struct encode_state *encode_state,
        struct i965_gpe_context *gpe_context,
        struct intel_encoder_context *encoder_context,
        void *param);

    void (*pfn_set_curbe_brc_init_reset)(
        VADriverContextP ctx,
        struct encode_state *encode_state,
        struct i965_gpe_context *gpe_context,
        struct intel_encoder_context *encoder_context,
        void *param);

    void (*pfn_set_curbe_brc_frame_update)(
        VADriverContextP ctx,
        struct encode_state *encode_state,
        struct i965_gpe_context *gpe_context,
        struct intel_encoder_context *encoder_context,
        void *param);

    void (*pfn_set_curbe_brc_mb_update)(
        VADriverContextP ctx,
        struct encode_state *encode_state,
        struct i965_gpe_context *gpe_context,
        struct intel_encoder_context *encoder_context,
        void *param);

    void (*pfn_set_curbe_sfd)(
        VADriverContextP ctx,
        struct encode_state *encode_state,
        struct i965_gpe_context *gpe_context,
        struct intel_encoder_context *encoder_context,
        void *param);

    void (*pfn_set_curbe_wp)(
        VADriverContextP ctx,
        struct encode_state *encode_state,
        struct i965_gpe_context *gpe_context,
        struct intel_encoder_context *encoder_context,
        void *param);

    //surface set function pointer
    void (*pfn_send_scaling_surface)(
        VADriverContextP ctx,
        struct encode_state *encode_state,
        struct i965_gpe_context *gpe_context,
        struct intel_encoder_context *encoder_context,
        void *param);

    void (*pfn_send_me_surface)(
        VADriverContextP ctx,
        struct encode_state *encode_state,
        struct i965_gpe_context *gpe_context,
        struct intel_encoder_context *encoder_context,
        void *param);

    void (*pfn_send_mbenc_surface)(
        VADriverContextP ctx,
        struct encode_state *encode_state,
        struct i965_gpe_context *gpe_context,
        struct intel_encoder_context *encoder_context,
        void *param);

    void (*pfn_send_brc_init_reset_surface)(
        VADriverContextP ctx,
        struct encode_state *encode_state,
        struct i965_gpe_context *gpe_context,
        struct intel_encoder_context *encoder_context,
        void *param);

    void (*pfn_send_brc_frame_update_surface)(
        VADriverContextP ctx,
        struct encode_state *encode_state,
        struct i965_gpe_context *gpe_context,
        struct intel_encoder_context *encoder_context,
        void *param);

    void (*pfn_send_brc_mb_update_surface)(
        VADriverContextP ctx,
        struct encode_state *encode_state,
        struct i965_gpe_context *gpe_context,
        struct intel_encoder_context *encoder_context,
        void *param);

    void (*pfn_send_sfd_surface)(
        VADriverContextP ctx,
        struct encode_state *encode_state,
        struct i965_gpe_context *gpe_context,
        struct intel_encoder_context *encoder_context,
        void *param);

    void (*pfn_send_wp_surface)(
        VADriverContextP ctx,
        struct encode_state *encode_state,
        struct i965_gpe_context *gpe_context,
        struct intel_encoder_context *encoder_context,
        void *param);
};
/*
   the definition for encoder codec state
*/

struct generic_enc_codec_state {

    //generic related
    int32_t  kernel_mode;
    int32_t  preset;
    int32_t  seq_frame_number;
    int32_t  total_frame_number;
    int32_t  herder_bytes_inserted;
    uint8_t  frame_type;
    bool     first_frame;

    // original width/height
    uint32_t frame_width_in_pixel;
    uint32_t frame_height_in_pixel;
    uint32_t frame_width_in_mbs;
    uint32_t frame_height_in_mbs;

    //scaling related
    uint32_t frame_width_2x;
    uint32_t frame_height_2x;
    uint32_t downscaled_width_2x_in_mb;
    uint32_t downscaled_height_2x_in_mb;
    uint32_t frame_width_4x;
    uint32_t frame_height_4x;
    uint32_t frame_width_16x;
    uint32_t frame_height_16x;
    uint32_t frame_width_32x;
    uint32_t frame_height_32x;
    uint32_t downscaled_width_4x_in_mb;
    uint32_t downscaled_height_4x_in_mb;
    uint32_t downscaled_width_16x_in_mb;
    uint32_t downscaled_height_16x_in_mb;
    uint32_t downscaled_width_32x_in_mb;
    uint32_t downscaled_height_32x_in_mb;

    // ME related
    uint32_t hme_supported: 1;
    uint32_t b16xme_supported: 1;
    uint32_t b32xme_supported: 1;
    uint32_t hme_enabled: 1;
    uint32_t b16xme_enabled: 1;
    uint32_t b32xme_enabled: 1;
    uint32_t brc_distortion_buffer_supported: 1;
    uint32_t brc_constant_buffer_supported: 1;
    uint32_t hme_reserved: 24;

    //BRC related
    uint32_t frame_rate;
    uint32_t internal_rate_mode;

    uint32_t brc_allocated: 1;
    uint32_t brc_inited: 1;
    uint32_t brc_need_reset: 1;
    uint32_t is_low_delay: 1;
    uint32_t brc_enabled: 1;
    uint32_t curr_pak_pass: 4;
    uint32_t num_pak_passes: 4;
    uint32_t is_first_pass: 1;
    uint32_t is_last_pass: 1;
    uint32_t mb_brc_enabled: 1;
    uint32_t brc_roi_enable: 1;
    uint32_t brc_dirty_roi_enable: 1;
    uint32_t skip_frame_enbale: 1;
    uint32_t brc_reserved: 13;

    uint32_t target_bit_rate;
    uint32_t max_bit_rate;
    uint32_t min_bit_rate;
    uint64_t init_vbv_buffer_fullness_in_bit;
    uint64_t vbv_buffer_size_in_bit;
    uint32_t frames_per_100s;
    uint32_t gop_size;
    uint32_t gop_ref_distance;
    uint32_t brc_target_size;
    uint32_t brc_mode;
    double   brc_init_current_target_buf_full_in_bits;
    double   brc_init_reset_input_bits_per_frame;
    uint32_t brc_init_reset_buf_size_in_bits;
    uint32_t brc_init_previous_target_buf_full_in_bits;
    int32_t  frames_per_window_size;
    int32_t  target_percentage;
    uint16_t avbr_curracy;
    uint16_t avbr_convergence;

    //skip frame enbale
    uint32_t num_skip_frames;
    uint32_t size_skip_frames;

    // ROI related
    uint32_t dirty_num_roi;
    uint32_t num_roi;
    uint32_t max_delta_qp;
    uint32_t min_delta_qp;
    struct intel_roi roi[INTEL_ROI_NUM];

};

/*
 by now VME and PAK use the same context. it will bind the ctx according to the codec and platform, also vdenc and non-vdenc
*/
struct encoder_vme_mfc_context {
    int32_t codec_id;
    void * generic_enc_ctx;
    void * private_enc_ctx; //pointer to the specific enc_ctx
    void * generic_enc_state;
    void * private_enc_state; //pointer to the specific enc_state
};

#endif /* _I965_COMMON_ENCODER_H */
