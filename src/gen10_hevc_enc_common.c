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
 *    Peng Chen <peng.c.chen@intel.com>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "intel_driver.h"
#include "intel_batchbuffer.h"
#include "i965_defines.h"
#include "i965_drv_video.h"
#include "i965_encoder.h"
#include "i965_encoder_common.h"
#include "i965_encoder_utils.h"
#include "i965_encoder_api.h"
#include "gen10_hcp_common.h"
#include "gen10_hevc_enc_common.h"

static const unsigned char default_scaling16[16] = {
    16, 16, 16, 16,
    16, 16, 16, 16,
    16, 16, 16, 16,
    16, 16, 16, 16
};

static const unsigned char default_scaling_intra[64] = {
    16, 16, 16, 16, 17, 18, 21, 24,
    16, 16, 16, 16, 17, 19, 22, 25,
    16, 16, 17, 18, 20, 22, 25, 29,
    16, 16, 18, 21, 24, 27, 31, 36,
    17, 17, 20, 24, 30, 35, 41, 47,
    18, 19, 22, 27, 35, 44, 54, 65,
    21, 22, 25, 31, 41, 54, 70, 88,
    24, 25, 29, 36, 47, 65, 88, 115
};

static const unsigned char default_scaling_inter[64] = {
    16, 16, 16, 16, 17, 18, 20, 24,
    16, 16, 16, 17, 18, 20, 24, 25,
    16, 16, 17, 18, 20, 24, 25, 28,
    16, 17, 18, 20, 24, 25, 28, 33,
    17, 18, 20, 24, 25, 28, 33, 41,
    18, 20, 24, 25, 28, 33, 41, 54,
    20, 24, 25, 28, 33, 41, 54, 71,
    24, 25, 28, 33, 41, 54, 71, 91
};

static void
hevc_init_qm_matrix(struct gen10_hevc_enc_frame_info *frame_info,
                    VAQMatrixBufferHEVC *qm_matrix,
                    int matrix_flag)
{
    uint8_t *real_qm, real_dc_qm = 0;
    uint16_t *real_fqm, *real_dc_fqm;
    int comps, len;
    int i, j, m, n;

    if (matrix_flag == 0) {
        for (m = 0; m < 4; m++) {
            comps = (m == 3) ? 1 : 3;
            len = (m == 0) ? 16 : 64;

            for (i = 0; i < comps; i++) {
                for (j = 0; j < 2; j++) {
                    real_qm = frame_info->qm_matrix[m][i][j];

                    switch (m) {
                    case 0:
                        memcpy(real_qm, qm_matrix->scaling_lists_4x4[i][j], len);
                        break;
                    case 1:
                        memcpy(real_qm, qm_matrix->scaling_lists_8x8[i][j], len);
                        break;
                    case 2:
                        memcpy(real_qm, qm_matrix->scaling_lists_16x16[i][j], len);

                        real_dc_qm = qm_matrix->scaling_list_dc_16x16[i][j];
                        frame_info->qm_dc_matrix[0][i][j] = real_dc_qm;
                        break;
                    case 3:
                        memcpy(real_qm, qm_matrix->scaling_lists_32x32[j], len);

                        real_dc_qm = qm_matrix->scaling_list_dc_32x32[j];
                        frame_info->qm_dc_matrix[1][i][j] = real_dc_qm;
                        break;
                    default:
                        assert(0);
                    }

                    if (i == 0) {
                        real_fqm = frame_info->fqm_matrix[m][j];

                        for (n = 0; n < len; n++) {
                            uint32_t qm_value = *(real_qm + n);
                            uint32_t fqm_value = 0;

                            fqm_value = (qm_value < 2) ? 0xFFFF : 0xFFFF / qm_value;

                            *(real_fqm + n) = fqm_value;
                        }

                        if (m == 2 || m == 3) {
                            uint32_t dc = real_dc_qm;

                            real_dc_fqm = &frame_info->fqm_dc_matrix[m - 2][j];
                            dc = (dc < 2) ? 0xFFFF : 0xFFFF / dc;

                            *real_dc_fqm = dc;
                        }
                    }
                }
            }
        }
    } else if (matrix_flag == 1) {
        for (m = 0; m < 4; m++) {
            comps = (m == 3) ? 1 : 3;
            len = (m == 0) ? 16 : 64;

            for (i = 0; i < comps; i++) {
                for (j = 0; j < 2; j++) {
                    real_qm = frame_info->qm_matrix[m][i][j];

                    switch (m) {
                    case 0:
                        memcpy(real_qm, default_scaling16, len);
                        break;
                    case 1:
                    case 2:
                    case 3:
                        if (j == 0)
                            memcpy(real_qm, default_scaling_intra, len);
                        else
                            memcpy(real_qm, default_scaling_inter, len);

                        break;
                    default:
                        assert(0);
                    }

                    if (i == 0) {
                        real_fqm = frame_info->fqm_matrix[m][j];

                        for (n = 0; n < len; n++) {
                            uint32_t qm_value = *(real_qm + n);
                            uint32_t fqm_value = 0;

                            fqm_value = (qm_value < 2) ? 0xFFFF : 0xFFFF / qm_value;

                            *(real_fqm + n) = fqm_value;
                        }
                    }
                }
            }
        }

        memset(&frame_info->qm_dc_matrix, 16, sizeof(frame_info->qm_dc_matrix));

        for (i = 0; i < 2; i++) {
            for (j = 0; j < 2; j++)
                frame_info->fqm_dc_matrix[i][j] = 0x1000;
        }

    } else if (matrix_flag == 2) {
        memset(&frame_info->qm_matrix, 16, sizeof(frame_info->qm_matrix));
        memset(&frame_info->qm_dc_matrix, 16, sizeof(frame_info->qm_dc_matrix));

        for (m = 0; m < 4; m++) {
            for (j = 0; j < 2; j++) {
                for (n = 0; n < 64; n++)
                    frame_info->fqm_matrix[m][j][n] = 0x1000;
            }
        }

        for (i = 0; i < 2; i++) {
            for (j = 0; j < 2; j++)
                frame_info->fqm_dc_matrix[i][j] = 0x1000;
        }
    } else
        assert(0);
}

static int
hevc_enc_map_pic_index(VASurfaceID id,
                       VAPictureHEVC *pic_list,
                       int pic_list_count)
{
    int i;

    if (id != VA_INVALID_ID) {
        for (i = 0; i < pic_list_count; i++) {
            VAPictureHEVC * const va_pic = &pic_list[i];

            if (va_pic->picture_id == id &&
                !(va_pic->flags & VA_PICTURE_HEVC_INVALID))
                return i;
        }
    }

    return -1;
}

static int
gen10_hevc_enc_get_relocation_flag(VAEncSequenceParameterBufferHEVC *cur_seq_param,
                                   VAEncSequenceParameterBufferHEVC *last_seq_param)
{
    if ((cur_seq_param->seq_fields.bits.bit_depth_luma_minus8 !=
         last_seq_param->seq_fields.bits.bit_depth_luma_minus8) ||
        (cur_seq_param->seq_fields.bits.bit_depth_chroma_minus8 !=
         last_seq_param->seq_fields.bits.bit_depth_chroma_minus8) ||
        (cur_seq_param->log2_min_luma_coding_block_size_minus3 !=
         last_seq_param->log2_min_luma_coding_block_size_minus3) ||
        (cur_seq_param->log2_diff_max_min_luma_coding_block_size !=
         last_seq_param->log2_diff_max_min_luma_coding_block_size) ||
        (cur_seq_param->pic_width_in_luma_samples !=
         last_seq_param->pic_width_in_luma_samples) ||
        (cur_seq_param->pic_height_in_luma_samples !=
         last_seq_param->pic_height_in_luma_samples) ||
        (cur_seq_param->seq_fields.bits.bit_depth_chroma_minus8 !=
         last_seq_param->seq_fields.bits.bit_depth_chroma_minus8) ||
        (cur_seq_param->seq_fields.bits.bit_depth_chroma_minus8 !=
         last_seq_param->seq_fields.bits.bit_depth_chroma_minus8))
        return 1;

    return 0;
}

void
gen10_hevc_enc_init_frame_info(VADriverContextP ctx,
                               struct encode_state *encode_state,
                               struct intel_encoder_context *encoder_context,
                               struct gen10_hevc_enc_frame_info *frame_info)
{
    uint32_t log2_max_coding_block_size = 0, raw_ctu_bits = 0;
    VAEncPictureParameterBufferHEVC *pic_param = NULL;
    VAEncSequenceParameterBufferHEVC *seq_param = NULL;
    VAEncSliceParameterBufferHEVC *slice_param = NULL;
    int i, j;

    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;
    seq_param = (VAEncSequenceParameterBufferHEVC *)encode_state->seq_param_ext->buffer;
    slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[0]->buffer;

    frame_info->bit_depth_luma_minus8 = seq_param->seq_fields.bits.bit_depth_luma_minus8;
    frame_info->bit_depth_chroma_minus8 = seq_param->seq_fields.bits.bit_depth_chroma_minus8;
    frame_info->cu_size = 1 << (seq_param->log2_min_luma_coding_block_size_minus3 + 3);
    frame_info->lcu_size = 1 << (seq_param->log2_diff_max_min_luma_coding_block_size +
                                 seq_param->log2_min_luma_coding_block_size_minus3 + 3);
    frame_info->frame_width = (seq_param->pic_width_in_luma_samples / frame_info->cu_size) * frame_info->cu_size;
    frame_info->frame_height = (seq_param->pic_height_in_luma_samples / frame_info->cu_size) * frame_info->cu_size;
    frame_info->width_in_lcu  = ALIGN(frame_info->frame_width, frame_info->lcu_size) / frame_info->lcu_size;
    frame_info->height_in_lcu = ALIGN(frame_info->frame_height, frame_info->lcu_size) / frame_info->lcu_size;
    frame_info->width_in_cu  = ALIGN(frame_info->frame_width, frame_info->cu_size) / frame_info->cu_size;
    frame_info->height_in_cu = ALIGN(frame_info->frame_height, frame_info->cu_size) / frame_info->cu_size;
    frame_info->width_in_mb  = ALIGN(frame_info->frame_width, 16) / 16;
    frame_info->height_in_mb = ALIGN(frame_info->frame_height, 16) / 16;

    frame_info->picture_coding_type = slice_param->slice_type;

    frame_info->ctu_max_bitsize_allowed = pic_param->ctu_max_bitsize_allowed;

    log2_max_coding_block_size  = seq_param->log2_min_luma_coding_block_size_minus3 + 3 +
                                  seq_param->log2_diff_max_min_luma_coding_block_size;
    raw_ctu_bits = (1 << (2 * log2_max_coding_block_size + 3)) +
                   (1 << (2 * log2_max_coding_block_size + 2));
    raw_ctu_bits = (5 * raw_ctu_bits / 3);

    if (frame_info->ctu_max_bitsize_allowed == 0 ||
        frame_info->ctu_max_bitsize_allowed > raw_ctu_bits)
        frame_info->ctu_max_bitsize_allowed = raw_ctu_bits;

    frame_info->low_delay = 1;
    frame_info->arbitrary_num_mb_in_slice = 0;

    for (i = 0; i < encode_state->num_slice_params_ext; i++) {
        slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[i]->buffer;

        if (i == 0) {
            frame_info->is_same_ref_list = 1;
            if (slice_param->slice_type == HEVC_SLICE_B) {
                if (slice_param->num_ref_idx_l0_active_minus1 >=
                    slice_param->num_ref_idx_l1_active_minus1) {
                    for (j = 0; j < slice_param->num_ref_idx_l1_active_minus1 + 1; j++)
                        if (slice_param->ref_pic_list0[j].picture_id !=
                            slice_param->ref_pic_list1[j].picture_id) {
                            frame_info->is_same_ref_list = 0;
                            break;
                        }
                } else
                    frame_info->is_same_ref_list = 0;
            }
        }

        if (slice_param->slice_type == HEVC_SLICE_B && frame_info->low_delay) {
            for (j = 0; j <= slice_param->num_ref_idx_l0_active_minus1; j++) {
                if (pic_param->decoded_curr_pic.pic_order_cnt <
                    slice_param->ref_pic_list0[j].pic_order_cnt)
                    frame_info->low_delay = 0;
            }

            for (j = 0; j <= slice_param->num_ref_idx_l1_active_minus1; j++) {
                if (pic_param->decoded_curr_pic.pic_order_cnt <
                    slice_param->ref_pic_list1[j].pic_order_cnt)
                    frame_info->low_delay = 0;
            }
        }

        if (!frame_info->arbitrary_num_mb_in_slice &&
            (slice_param->num_ctu_in_slice % frame_info->width_in_lcu))
            frame_info->arbitrary_num_mb_in_slice = 1;
    }

    for (i = 0; i < 8; i++) {
        frame_info->mapped_ref_idx_list0[i] = -1;
        frame_info->mapped_ref_idx_list1[i] = -1;
    }

    if (slice_param->slice_type != HEVC_SLICE_I) {
        for (i = 0; i <= slice_param->num_ref_idx_l0_active_minus1; i++)
            frame_info->mapped_ref_idx_list0[i] = hevc_enc_map_pic_index(slice_param->ref_pic_list0[i].picture_id,
                                                                         pic_param->reference_frames, 8);

        if (slice_param->slice_type == HEVC_SLICE_B) {
            for (i = 0; i <= slice_param->num_ref_idx_l1_active_minus1; i++)
                frame_info->mapped_ref_idx_list1[i] = hevc_enc_map_pic_index(slice_param->ref_pic_list1[i].picture_id,
                                                                             pic_param->reference_frames, 8);
        }
    }

    frame_info->slice_qp = pic_param->pic_init_qp + slice_param->slice_qp_delta;

    if (encoder_context->is_new_sequence ||
        memcmp(&frame_info->last_seq_param, seq_param, sizeof(*seq_param))) {
        VAQMatrixBufferHEVC *input_matrix = NULL;
        int matrix_flag = 0;

        if (seq_param->seq_fields.bits.scaling_list_enabled_flag) {
            if (pic_param->pic_fields.bits.scaling_list_data_present_flag) {
                if (encode_state->q_matrix && encode_state->q_matrix->buffer)
                    input_matrix = (VAQMatrixBufferHEVC *)encode_state->q_matrix->buffer;
                else if (encode_state->iq_matrix && encode_state->iq_matrix->buffer)
                    input_matrix = (VAQMatrixBufferHEVC *)encode_state->iq_matrix->buffer;

                matrix_flag = 0;
            } else
                matrix_flag = 1;
        } else
            matrix_flag = 2;

        hevc_init_qm_matrix(frame_info, input_matrix, matrix_flag);

        frame_info->gop_size = seq_param->intra_period;
        frame_info->gop_ref_dist = seq_param->ip_period;
        frame_info->gop_num_p = encoder_context->brc.num_pframes_in_gop;
        frame_info->gop_num_b[0] = encoder_context->brc.num_bframes_in_gop;
        frame_info->gop_num_b[1] = 0;
        frame_info->gop_num_b[2] = 0;

        if (gen10_hevc_enc_get_relocation_flag(seq_param,
                                               &frame_info->last_seq_param))
            frame_info->reallocate_flag = 1;
        else
            frame_info->reallocate_flag = 0;

        memcpy(&frame_info->last_seq_param, seq_param, sizeof(*seq_param));
    } else
        frame_info->reallocate_flag = 0;
}

static int
hevc_find_skipemulcnt(uint8_t *buf, int bits_length)
{
    int skip_cnt = 0, i = 0;

    if ((bits_length >> 3) < 6)
        return 0;

    for (i = 0; i < 3; i++)
        if (buf[i] != 0)
            break;

    if (i > 1) {
        if (buf[i] == 1)
            skip_cnt = i + 3;
    }

    return skip_cnt;
}

static void
gen10_hevc_enc_insert_object(VADriverContextP ctx,
                             struct intel_batchbuffer *batch,
                             uint8_t *header_data,
                             int length_in_bits,
                             int end_of_slice_flag,
                             int last_header_flag,
                             int emulation_flag,
                             int skip_emulation_bytes)
{
    gen10_hcp_pak_insert_object_param insert_param;

    memset(&insert_param, 0, sizeof(insert_param));

    insert_param.dw1.bits.end_of_slice_flag = end_of_slice_flag;
    insert_param.dw1.bits.last_header_flag = last_header_flag;
    insert_param.dw1.bits.emulation_flag = emulation_flag;

    if (emulation_flag) {
        if (skip_emulation_bytes)
            insert_param.dw1.bits.skip_emulation_bytes = skip_emulation_bytes;
        else
            insert_param.dw1.bits.skip_emulation_bytes = hevc_find_skipemulcnt((uint8_t *)header_data,
                                                                               length_in_bits);
    }

    insert_param.dw1.bits.data_bits_in_last_dw = length_in_bits & 0x1f;
    if (insert_param.dw1.bits.data_bits_in_last_dw == 0)
        insert_param.dw1.bits.data_bits_in_last_dw = 32;

    insert_param.inline_payload_ptr = header_data;
    insert_param.inline_payload_bits = length_in_bits;

    gen10_hcp_pak_insert_object(ctx, batch, &insert_param);
}

void
gen10_hevc_enc_insert_packed_header(VADriverContextP ctx,
                                    struct encode_state *encode_state,
                                    struct intel_encoder_context *encoder_context,
                                    struct intel_batchbuffer *batch)
{
    VAEncPackedHeaderParameterBuffer *param = NULL;
    uint8_t *header_data = NULL;
    uint32_t length_in_bits = 0;
    int packed_type = 0;
    int idx = 0, idx_offset = 0;
    int i = 0;

    for (i = 0; i < 4; i++) {
        idx_offset = 0;
        switch (i) {
        case 0:
            packed_type = VAEncPackedHeaderHEVC_VPS;
            break;
        case 1:
            packed_type = VAEncPackedHeaderHEVC_VPS;
            idx_offset = 1;
            break;
        case 2:
            packed_type = VAEncPackedHeaderHEVC_PPS;
            break;
        case 3:
            packed_type = VAEncPackedHeaderHEVC_SEI;
            break;
        default:
            break;
        }

        idx = va_enc_packed_type_to_idx(packed_type) + idx_offset;
        if (encode_state->packed_header_data[idx]) {
            param = (VAEncPackedHeaderParameterBuffer *)encode_state->packed_header_param[idx]->buffer;
            header_data = (uint8_t *)encode_state->packed_header_data[idx]->buffer;
            length_in_bits = param->bit_length;

            gen10_hevc_enc_insert_object(ctx, batch, header_data, length_in_bits,
                                         0, 0, !param->has_emulation_bytes, 0);
        }
    }
}

void
gen10_hevc_enc_insert_slice_header(VADriverContextP ctx,
                                   struct encode_state *encode_state,
                                   struct intel_encoder_context *encoder_context,
                                   struct intel_batchbuffer *batch,
                                   int slice_index)
{
    VAEncPackedHeaderParameterBuffer *param = NULL;
    uint8_t *header_data = NULL;
    uint32_t length_in_bits = 0;
    int count = 0, start_index = -1;
    int i = 0;

    count = encode_state->slice_rawdata_count[slice_index];
    start_index = encode_state->slice_rawdata_index[slice_index] &
                  SLICE_PACKED_DATA_INDEX_MASK;

    for (i = 0; i < count; i++) {
        param = (VAEncPackedHeaderParameterBuffer *)
                (encode_state->packed_header_params_ext[start_index + i]->buffer);

        if (param->type == VAEncPackedHeaderSlice)
            continue;

        header_data = (uint8_t *)encode_state->packed_header_data_ext[start_index]->buffer;
        length_in_bits = param->bit_length;

        gen10_hevc_enc_insert_object(ctx, batch, header_data, length_in_bits,
                                     0, 0, !param->has_emulation_bytes, 0);
    }

    start_index = -1;
    if (encode_state->slice_header_index[slice_index] & SLICE_PACKED_DATA_INDEX_TYPE)
        start_index = encode_state->slice_header_index[slice_index] &
                      SLICE_PACKED_DATA_INDEX_MASK;

    if (start_index == -1) {
        VAEncSequenceParameterBufferHEVC *seq_param = (VAEncSequenceParameterBufferHEVC *)encode_state->seq_param_ext->buffer;
        VAEncPictureParameterBufferHEVC *pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;
        VAEncSliceParameterBufferHEVC *slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[slice_index]->buffer;
        unsigned char *slice_header = NULL;
        int slice_header_bits = 0;

        slice_header_bits = build_hevc_slice_header(seq_param,
                                                    pic_param,
                                                    slice_param,
                                                    &slice_header,
                                                    0);

        gen10_hevc_enc_insert_object(ctx, batch, slice_header, slice_header_bits,
                                     0, 1, 1, 5);

        free(slice_header);
    } else {
        param = (VAEncPackedHeaderParameterBuffer *)
                (encode_state->packed_header_params_ext[start_index]->buffer);
        header_data = (uint8_t *)encode_state->packed_header_data_ext[start_index]->buffer;
        length_in_bits = param->bit_length;

        gen10_hevc_enc_insert_object(ctx, batch, header_data, length_in_bits,
                                     0, 1, !param->has_emulation_bytes, 0);
    }
}


#define ALLOC_GPE_RESOURCE(RES, NAME, SIZE)                 \
    do{                                                     \
        i965_free_gpe_resource(&common_res->RES);           \
        if (!i965_allocate_gpe_resource(i965->intel.bufmgr, \
                                 &common_res->RES,          \
                                 SIZE,                      \
                                 NAME))                     \
            goto FAIL;                                      \
    } while(0);

#define ALLOC_GPE_2D_RESOURCE(RES, NAME, W, H, P)               \
    do{                                                         \
        i965_free_gpe_resource(&priv_ctx->RES);                 \
        if (!i965_gpe_allocate_2d_resource(i965->intel.bufmgr,  \
                                 &common_res->RES,              \
                                 (ALIGN(W, 64)), H,             \
                                 (ALIGN(P, 64)),                \
                                 NAME))                         \
            goto FAIL;                                          \
    } while(0);

int
gen10_hevc_enc_init_common_resource(VADriverContextP ctx,
                                    struct encode_state *encode_state,
                                    struct intel_encoder_context *encoder_context,
                                    struct gen10_hevc_enc_common_res *common_res,
                                    struct gen10_hevc_enc_frame_info *frame_info,
                                    int inter_enabled,
                                    int vdenc_enabled)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    VAEncPictureParameterBufferHEVC *pic_param = NULL;
    struct object_surface *obj_surface;
    struct object_buffer *obj_buffer;
    int res_size = 0, size_shift = 0;
    int width = 0, height = 0;
    int i = 0;

    pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;

    obj_buffer = encode_state->coded_buf_object;
    i965_free_gpe_resource(&common_res->compressed_bitstream.gpe_res);
    i965_dri_object_to_buffer_gpe_resource(&common_res->compressed_bitstream.gpe_res,
                                           obj_buffer->buffer_store->bo);
    common_res->compressed_bitstream.offset = I965_CODEDBUFFER_HEADER_SIZE;
    common_res->compressed_bitstream.end_offset = ALIGN(obj_buffer->size_element - 0x1000,
                                                        0x1000);

    i965_free_gpe_resource(&common_res->uncompressed_pic.gpe_res);
    i965_object_surface_to_2d_gpe_resource(&common_res->uncompressed_pic.gpe_res,
                                           encode_state->input_yuv_object);
    common_res->uncompressed_pic.obj_surface = encode_state->input_yuv_object;
    common_res->uncompressed_pic.surface_id = encoder_context->input_yuv_surface;

    i965_free_gpe_resource(&common_res->reconstructed_pic.gpe_res);
    i965_object_surface_to_2d_gpe_resource(&common_res->reconstructed_pic.gpe_res,
                                           encode_state->reconstructed_object);
    common_res->reconstructed_pic.obj_surface = encode_state->reconstructed_object;
    common_res->reconstructed_pic.surface_id = pic_param->decoded_curr_pic.picture_id;

    if (inter_enabled) {
        for (i = 0; i < 15; i++) {
            if (common_res->reference_pics[i].surface_id != VA_INVALID_ID)
                i965_free_gpe_resource(&common_res->reference_pics[i].gpe_res);

            obj_surface = encode_state->reference_objects[i];
            if (obj_surface && obj_surface->bo) {
                i965_object_surface_to_2d_gpe_resource(&common_res->reference_pics[i].gpe_res,
                                                       obj_surface);

                common_res->reference_pics[i].obj_surface = obj_surface;
                common_res->reference_pics[i].surface_id = pic_param->reference_frames[i].picture_id;
            } else {
                common_res->reference_pics[i].obj_surface = NULL;
                common_res->reference_pics[i].surface_id = VA_INVALID_ID;
            }
        }
    }

    if (frame_info->reallocate_flag) {
        width = frame_info->frame_width;
        height = frame_info->frame_height;
        size_shift = (frame_info->bit_depth_luma_minus8 ||
                      frame_info->bit_depth_chroma_minus8) ? 2 : 3;

        res_size = ALIGN(width, 32) << (6 - size_shift);
        ALLOC_GPE_RESOURCE(deblocking_filter_line_buffer,
                           "Deblocking filter line buffer",
                           res_size);
        ALLOC_GPE_RESOURCE(deblocking_filter_tile_line_buffer,
                           "Deblocking filter tile line buffer",
                           res_size);

        res_size = ALIGN(height +
                         frame_info->width_in_lcu * 6, 32) << (6 - size_shift);
        ALLOC_GPE_RESOURCE(deblocking_filter_tile_column_buffer,
                           "Deblocking filter tile column buffer",
                           res_size);

        res_size = (((width + 15) >> 4) * 188 +
                    frame_info->width_in_lcu * 9 + 1023) >> 3;
        ALLOC_GPE_RESOURCE(metadata_line_buffer,
                           "metadata line buffer",
                           res_size);

        res_size = (((width + 15) >> 4) * 172 +
                    frame_info->width_in_lcu * 9 + 1023) >> 3;
        ALLOC_GPE_RESOURCE(metadata_tile_line_buffer,
                           "metadata tile line buffer",
                           res_size);

        res_size = (((height + 15) >> 4) * 176 +
                    frame_info->height_in_lcu * 9 + 1023) >> 3;
        ALLOC_GPE_RESOURCE(metadata_tile_column_buffer,
                           "metadata tile column buffer",
                           res_size);

        res_size = ALIGN(((width >> 1) +
                          frame_info->width_in_lcu * 3), 16) << (6 - size_shift);
        ALLOC_GPE_RESOURCE(sao_line_buffer,
                           "sao line buffer",
                           res_size);

        res_size = ALIGN(((width >> 1) +
                          frame_info->width_in_lcu * 6), 16) << (6 - size_shift);
        ALLOC_GPE_RESOURCE(sao_tile_line_buffer,
                           "sao tile line buffer",
                           res_size);

        res_size = ALIGN(((height >> 1) +
                          frame_info->height_in_lcu * 6), 16) << (6 - size_shift);
        ALLOC_GPE_RESOURCE(sao_tile_column_buffer,
                           "sao tile column buffer",
                           res_size);

        if (vdenc_enabled) {
            res_size = 0x500000;
            ALLOC_GPE_RESOURCE(streamout_data_destination_buffer,
                               "streamout data destination buffer",
                               res_size);
        }

        res_size = I965_MAX_NUM_SLICE * 64;
        ALLOC_GPE_RESOURCE(picture_status_buffer,
                           "picture status buffer",
                           res_size);

        res_size = frame_info->width_in_lcu * frame_info->height_in_lcu * 256;
        ALLOC_GPE_RESOURCE(ildb_streamout_buffer,
                           "ildb streamout buffer",
                           res_size);

        //res_size = frame_info->width_in_lcu * frame_info->height_in_lcu * 16;
        width = ALIGN(frame_info->frame_width, 64) >> 3;
        height = ALIGN(frame_info->frame_height, 64) >> 3;
        res_size = width * height * 16 + 1024;
        ALLOC_GPE_RESOURCE(sao_streamout_data_destination_buffer,
                           "sao streamout date destination buffer",
                           res_size);

        res_size = ALIGN(8 * 64, 4096);
        ALLOC_GPE_RESOURCE(frame_statics_streamout_data_destination_buffer,
                           "frame statics streamout date destination buffer",
                           res_size);

        //res_size = (ALIGN(width, GEN10_HEVC_ENC_MAX_LCU_SIZE) + 2) * 64 * 8 * 2;
        res_size = ALIGN(frame_info->frame_width, 64) * 1024 + 2048;
        res_size = res_size << 1;
        ALLOC_GPE_RESOURCE(sse_source_pixel_rowstore_buffer,
                           "sse source pixel rowstore buffer",
                           res_size);
    }

    return 0;

FAIL:
    return -1;
}

void
gen10_hevc_enc_free_common_resource(struct gen10_hevc_enc_common_res *common_res)
{
    int i;

    i965_free_gpe_resource(&common_res->compressed_bitstream.gpe_res);
    i965_free_gpe_resource(&common_res->uncompressed_pic.gpe_res);
    i965_free_gpe_resource(&common_res->reconstructed_pic.gpe_res);

    for (i = 0; i < 16; i++)
        if (common_res->reference_pics[i].surface_id != VA_INVALID_ID)
            i965_free_gpe_resource(&common_res->reference_pics[i].gpe_res);

    i965_free_gpe_resource(&common_res->deblocking_filter_line_buffer);
    i965_free_gpe_resource(&common_res->deblocking_filter_tile_line_buffer);
    i965_free_gpe_resource(&common_res->deblocking_filter_tile_column_buffer);
    i965_free_gpe_resource(&common_res->metadata_line_buffer);
    i965_free_gpe_resource(&common_res->metadata_tile_line_buffer);
    i965_free_gpe_resource(&common_res->metadata_tile_column_buffer);
    i965_free_gpe_resource(&common_res->sao_line_buffer);
    i965_free_gpe_resource(&common_res->sao_tile_line_buffer);
    i965_free_gpe_resource(&common_res->sao_tile_column_buffer);
    i965_free_gpe_resource(&common_res->streamout_data_destination_buffer);
    i965_free_gpe_resource(&common_res->picture_status_buffer);
    i965_free_gpe_resource(&common_res->ildb_streamout_buffer);
    i965_free_gpe_resource(&common_res->sao_streamout_data_destination_buffer);
    i965_free_gpe_resource(&common_res->frame_statics_streamout_data_destination_buffer);
    i965_free_gpe_resource(&common_res->sse_source_pixel_rowstore_buffer);
}

void
gen10_hevc_enc_init_status_buffer(VADriverContextP ctx,
                                  struct encode_state *encode_state,
                                  struct intel_encoder_context *encoder_context,
                                  struct gen10_hevc_enc_status_buffer *status_buffer)
{
    struct i965_coded_buffer_segment *coded_buffer_segment;
    uint32_t base_offset;
    char *pbuffer;
    dri_bo *bo;

    bo = encode_state->coded_buf_object->buffer_store->bo;

    i965_free_gpe_resource(&status_buffer->gpe_res);
    i965_dri_object_to_buffer_gpe_resource(&status_buffer->gpe_res, bo);

    status_buffer->status_size = ALIGN(sizeof(struct gen10_hevc_enc_status), 64);

    status_buffer->mmio_bytes_per_frame_offset = GEN10_MMIO_HCP_ENC_BITSTREAM_BYTECOUNT_FRAME_OFFSET;
    status_buffer->mmio_bs_frame_no_header_offset = GEN10_MMIO_HCP_ENC_BITSTREAM_BYTECOUNT_FRAME_NO_HEADER_OFFSET;
    status_buffer->mmio_image_mask_offset = GEN10_MMIO_HCP_ENC_IMAGE_STATUS_MASK_OFFSET;
    status_buffer->mmio_image_ctrl_offset = GEn10_MMIO_HCP_ENC_IMAGE_STATUS_CTRL_OFFSET;
    status_buffer->mmio_qp_status_offset = GEN10_MMIO_HCP_ENC_QP_STATE_OFFSET;
    status_buffer->mmio_bs_se_bitcount_offset = GEN10_MMIO_HCP_ENC_BITSTREAM_SE_BITCOUNT_FRAME_OFFSET;

    base_offset = offsetof(struct i965_coded_buffer_segment, codec_private_data);
    status_buffer->status_image_mask_offset = base_offset +
                                              offsetof(struct gen10_hevc_enc_status, image_status_mask);
    status_buffer->status_image_ctrl_offset = base_offset +
                                              offsetof(struct gen10_hevc_enc_status, image_status_ctrl);
    status_buffer->status_bytes_per_frame_offset = base_offset +
                                                   offsetof(struct gen10_hevc_enc_status, bytes_per_frame);
    status_buffer->status_pass_num_offset = base_offset +
                                            offsetof(struct gen10_hevc_enc_status, pass_number);
    status_buffer->status_media_state_offset = base_offset +
                                               offsetof(struct gen10_hevc_enc_status, media_state);
    status_buffer->status_qp_status_offset = base_offset +
                                             offsetof(struct gen10_hevc_enc_status, qp_status);
    status_buffer->status_bs_se_bitcount_offset = base_offset +
                                                  offsetof(struct gen10_hevc_enc_status, bs_se_bitcount);

    dri_bo_map(bo, 1);

    coded_buffer_segment = (struct i965_coded_buffer_segment *)bo->virtual;
    coded_buffer_segment->mapped = 0;
    coded_buffer_segment->codec = encoder_context->codec;
    coded_buffer_segment->status_support = 1;

    pbuffer = bo->virtual + base_offset;
    memset(pbuffer, 0, status_buffer->status_size);

    dri_bo_unmap(bo);
}

void
gen10_hevc_enc_init_lambda_param(struct gen10_hevc_enc_lambda_param *param,
                                 int bit_depth_luma_minus8,
                                 int bit_depth_chroma_minus8)
{
    double qp_temp, lambda_double, qp_factor;
    int qp, qp_max[2], qp_offset[2], shift_qp = 12;
    uint32_t lambda = 0;
    int i;

    memset(param, 0, sizeof(*param));

    qp_offset[0] = 6 * bit_depth_luma_minus8;
    qp_offset[1] = 6 * bit_depth_chroma_minus8;
    qp_max[0] = 52 + qp_offset[0];
    qp_max[1] = 52 + qp_offset[1];

    qp_factor = 0.25 * 0.65;
    for (i = 0; i < 2; i++) {
        for (qp = 0; qp < qp_max[i]; qp++) {
            qp_temp = (double)qp - qp_offset[i] - shift_qp;
            lambda_double = qp_factor * pow(2.0, qp_temp / 3.0);
            lambda_double = lambda_double * 16 + 0.5;
            lambda_double = (lambda_double > 65535) ? 65535 : lambda_double;
            lambda = (uint32_t)floor(lambda_double);
            param->lambda_intra[i][qp] = (uint16_t)lambda;
        }
    }

    qp_factor = 0.55;
    for (i = 0; i < 2; i++) {
        for (qp = 0; qp < qp_max[i]; qp++) {
            qp_temp = (double)qp - qp_offset[i] - shift_qp;
            lambda_double = qp_factor * pow(2.0, qp_temp / 3.0);
            if (i == 0)
                lambda_double *= MAX(1.00, MIN(1.6, 1.0 + 0.6 / 12.0 * (qp_temp - 10.0)));
            else
                lambda_double *= MAX(0.95, MIN(1.2, 0.25 / 12.0 * (qp_temp - 10.0) + 0.95));
            lambda_double = lambda_double * 16 + 0.5;
            lambda = (uint32_t)floor(lambda_double);
            lambda_double = (lambda_double > 0xffff) ? 0xffff : lambda_double;
            lambda = MIN(0xffff, lambda);
            param->lambda_inter[i][qp] = (uint16_t)lambda;
        }
    }
}

static void
hevc_hcp_set_qm_state(VADriverContextP ctx,
                      struct intel_batchbuffer *batch,
                      int size_id,
                      int color_component,
                      int prediction_type,
                      int dc_coefficient,
                      uint8_t *qm_buf,
                      int qm_length)
{
    gen10_hcp_qm_state_param param;

    memset(&param, 0, sizeof(param));
    param.dw1.prediction_type = prediction_type;
    param.dw1.size_id = size_id;
    param.dw1.color_component = color_component;
    param.dw1.dc_coefficient = dc_coefficient;
    memcpy(param.quant_matrix, qm_buf, qm_length);
    gen10_hcp_qm_state(ctx, batch, &param);
}

static void
hevc_hcp_set_fqm_state(VADriverContextP ctx,
                       struct intel_batchbuffer *batch,
                       int size_id,
                       int color_component,
                       int prediction_type,
                       int forward_dc_coeff,
                       uint16_t *fqm_buf,
                       int fqm_length)
{
    gen10_hcp_fqm_state_param param;

    memset(&param, 0, sizeof(param));
    param.dw1.prediction_type = prediction_type;
    param.dw1.size_id = size_id;
    param.dw1.color_component = color_component;
    param.dw1.forward_dc_coeff = forward_dc_coeff;
    memcpy(param.forward_quant_matrix, fqm_buf, fqm_length * sizeof(uint16_t));
    gen10_hcp_fqm_state(ctx, batch, &param);
}

void
gen10_hevc_enc_hcp_set_qm_fqm_states(VADriverContextP ctx,
                                     struct intel_batchbuffer *batch,
                                     struct gen10_hevc_enc_frame_info *frame_info)
{
    int dc_coefficient, forward_dc_coeff;
    uint8_t *real_qm;
    uint16_t *real_fqm;
    int comps, len;
    int i, j, m;

    for (m = 0; m < 4; m++) {
        comps = (m == 3) ? 1 : 3;
        len = (m == 0) ? 16 : 64;

        for (i = 0; i < comps; i++) {
            for (j = 0; j < 2; j++) {
                real_qm = frame_info->qm_matrix[m][i][j];

                if (i == 0)
                    real_fqm = frame_info->fqm_matrix[m][j];

                if (m == 2 || m == 3) {
                    dc_coefficient = frame_info->qm_dc_matrix[m - 2][i][j];

                    if (i == 0)
                        forward_dc_coeff = frame_info->fqm_dc_matrix[m - 2][j];
                } else {
                    dc_coefficient = 0;
                    forward_dc_coeff = 0;
                }

                hevc_hcp_set_qm_state(ctx, batch, m, i, j, dc_coefficient,
                                      real_qm, len);

                if (i == 0)
                    hevc_hcp_set_fqm_state(ctx, batch, m, i, j, forward_dc_coeff,
                                           real_fqm, len);
            }
        }
    }
}

static void
gen10_hevc_enc_hcp_set_ref_idx_state(VADriverContextP ctx,
                                     struct intel_batchbuffer *batch,
                                     VAEncPictureParameterBufferHEVC *pic_param,
                                     VAEncSliceParameterBufferHEVC *slice_param,
                                     int list_index)
{
    gen10_hcp_ref_idx_state_param param;
    VAPictureHEVC *ref_pic, *cur_pic;
    int weighted_pred_flag;
    int i, j;

    assert(list_index < 2);

    memset(&param, 0, sizeof(param));

    param.dw1.ref_pic_list_num = list_index;
    param.dw1.num_ref_idx_active_minus1 = list_index == 0 ? slice_param->num_ref_idx_l0_active_minus1 :
                                          slice_param->num_ref_idx_l1_active_minus1;

    cur_pic = &pic_param->decoded_curr_pic;
    weighted_pred_flag = (pic_param->pic_fields.bits.weighted_pred_flag &&
                          slice_param->slice_type == HEVC_SLICE_P) ||
                         (pic_param->pic_fields.bits.weighted_bipred_flag &&
                          slice_param->slice_type == HEVC_SLICE_B);

    for (i = 0; i < 16; i++) {
        if (i < MIN(param.dw1.num_ref_idx_active_minus1 + 1, 15)) {
            if (list_index == 0)
                ref_pic = &slice_param->ref_pic_list0[i];
            else
                ref_pic = &slice_param->ref_pic_list1[i];

            j = hevc_enc_map_pic_index(ref_pic->picture_id,
                                       pic_param->reference_frames, 8);
            if (j >= 0) {
                param.ref_list_entry[i].ref_pic_tb_value = CLAMP(-128, 127,
                                                                 cur_pic->pic_order_cnt - ref_pic->pic_order_cnt) & 0xff;
                param.ref_list_entry[i].ref_pic_frame_id = j;
                param.ref_list_entry[i].chroma_weight_flag = weighted_pred_flag;
                param.ref_list_entry[i].luma_weight_flag = weighted_pred_flag;
                param.ref_list_entry[i].long_term_ref_flag = !!(ref_pic->flags &
                                                                VA_PICTURE_HEVC_LONG_TERM_REFERENCE);
            }
        }
    }

    gen10_hcp_ref_idx_state(ctx, batch, &param);
}

void
gen10_hevc_enc_hcp_set_ref_idx_lists(VADriverContextP ctx,
                                     struct intel_batchbuffer *batch,
                                     VAEncPictureParameterBufferHEVC *pic_param,
                                     VAEncSliceParameterBufferHEVC *slice_param)
{
    gen10_hevc_enc_hcp_set_ref_idx_state(ctx, batch, pic_param, slice_param, 0);
    if (slice_param->slice_type == HEVC_SLICE_B)
        gen10_hevc_enc_hcp_set_ref_idx_state(ctx, batch, pic_param, slice_param, 1);
}

void
gen10_hevc_enc_hcp_set_weight_offsets(VADriverContextP ctx,
                                      struct intel_batchbuffer *batch,
                                      VAEncPictureParameterBufferHEVC *pic_param,
                                      VAEncSliceParameterBufferHEVC *slice_param)
{
    gen10_hcp_weightoffset_state_param param;
    int enable;
    int i, j;

    for (i = 0; i < 2; i++) {
        if (i == 0 &&
            pic_param->pic_fields.bits.weighted_pred_flag &&
            slice_param->slice_type == HEVC_SLICE_P)
            enable = 1;
        else if (i == 1 &&
                 pic_param->pic_fields.bits.weighted_bipred_flag &&
                 slice_param->slice_type == HEVC_SLICE_B)
            enable = 1;
        else
            enable = 0;

        if (enable) {
            memset(&param, 0, sizeof(param));

            param.dw1.ref_pic_list_num = i;

            if (i == 0) {
                for (j = 0; j < 15; j++) {
                    param.luma_offset[j].delta_luma_weight = slice_param->delta_luma_weight_l0[j];
                    param.luma_offset[j].luma_offset = slice_param->luma_offset_l0[j];

                    param.chroma_offset[j].delta_chroma_weight_0 = slice_param->delta_chroma_weight_l0[j][0];
                    param.chroma_offset[j].chroma_offset_0 = slice_param->chroma_offset_l0[j][0];
                    param.chroma_offset[j].delta_chroma_weight_1 = slice_param->delta_chroma_weight_l0[j][1];
                    param.chroma_offset[j].chroma_offset_1 = slice_param->chroma_offset_l0[j][1];
                }
            } else {
                for (j = 0; j < 15; j++) {
                    param.luma_offset[j].delta_luma_weight = slice_param->delta_luma_weight_l1[j];
                    param.luma_offset[j].luma_offset = slice_param->luma_offset_l1[j];

                    param.chroma_offset[j].delta_chroma_weight_0 = slice_param->delta_chroma_weight_l1[j][0];
                    param.chroma_offset[j].chroma_offset_0 = slice_param->chroma_offset_l1[j][0];
                    param.chroma_offset[j].delta_chroma_weight_1 = slice_param->delta_chroma_weight_l1[j][1];
                    param.chroma_offset[j].chroma_offset_1 = slice_param->chroma_offset_l1[j][1];
                }
            }

            gen10_hcp_weightoffset_state(ctx, batch, &param);
        }
    }
}

VAStatus
gen10_hevc_enc_ensure_surface(VADriverContextP ctx,
                              struct object_surface *obj_surface,
                              int bit_depth_minus8,
                              int reallocate_flag)
{
    VAStatus va_status = VA_STATUS_SUCCESS;
    uint32_t fourcc = VA_FOURCC_NV12;
    int update = 0;

    if (!obj_surface) {
        va_status = VA_STATUS_ERROR_INVALID_PARAMETER;

        goto EXIT;
    }

    if (bit_depth_minus8 > 0) {
        if (obj_surface->fourcc != VA_FOURCC_P010) {
            update = 1;
            fourcc = VA_FOURCC_P010;
        }
    } else if (obj_surface->fourcc != VA_FOURCC_NV12) {
        update = 1;
        fourcc = VA_FOURCC_NV12;
    }

    if (!obj_surface->bo || update) {
        if (reallocate_flag) {
            struct i965_driver_data * const i965 = i965_driver_data(ctx);

            i965_destroy_surface_storage(obj_surface);

            va_status = i965_check_alloc_surface_bo(ctx,
                                                    obj_surface,
                                                    i965->codec_info->has_tiled_surface,
                                                    fourcc,
                                                    SUBSAMPLE_YUV420);
        } else
            va_status = VA_STATUS_ERROR_INVALID_PARAMETER;
    }

EXIT:
    return va_status;
}

static void
hevc_get_max_mbps(uint32_t level_idc,
                  uint32_t *max_bps,
                  uint64_t *max_bps_per_pic)
{
    switch (level_idc) {
    case 30:
        *max_bps = 552960;
        *max_bps_per_pic = 36864;
        break;
    case 60:
        *max_bps = 3686400;
        *max_bps_per_pic = 122880;
        break;
    case 63:
        *max_bps = 7372800;
        *max_bps_per_pic = 245760;
        break;
    case 90:
        *max_bps = 16588800;
        *max_bps_per_pic = 552760;
        break;
    case 93:
        *max_bps = 33177600;
        *max_bps_per_pic = 983040;
        break;
    case 120:
        *max_bps = 66846720;
        *max_bps_per_pic = 2228224;
        break;
    case 123:
        *max_bps = 133693440;
        *max_bps_per_pic = 2228224;
        break;
    case 150:
        *max_bps = 267386880;
        *max_bps_per_pic = 8912896;
        break;
    case 153:
        *max_bps = 534773760;
        *max_bps_per_pic = 8912896;
        break;
    case 156:
        *max_bps = 1069547520;
        *max_bps_per_pic = 8912896;
        break;
    case 180:
        *max_bps = 1069547520;
        *max_bps_per_pic = 35651584;
        break;
    case 183:
        *max_bps = 2139095040;
        *max_bps_per_pic = 35651584;
        break;
    case 186:
        *max_bps = 4278190080;
        *max_bps_per_pic = 35651584;
        break;
    default:
        *max_bps = 16588800;
        *max_bps_per_pic = 552760;
        break;
    }
}

uint32_t
gen10_hevc_enc_get_profile_level_max_frame(VAEncSequenceParameterBufferHEVC *seq_param,
                                           uint32_t user_max_frame_size,
                                           uint32_t frame_rate)
{
    int bit_depth_minus8 = seq_param->seq_fields.bits.bit_depth_luma_minus8;
    uint64_t max_byte_per_pic, max_byte_per_pic_not0;
    int level_idc = seq_param->general_level_idc;
    uint32_t profile_level_max_frame, max_mbps;
    double format_factor = 1.5;
    double min_cr_scale = 1.0;
    int min_cr;

    assert(seq_param->seq_fields.bits.chroma_format_idc == 1);

    if (level_idc == 186 || level_idc == 150)
        min_cr = 6;
    else if (level_idc > 150)
        min_cr = 8;
    else if (level_idc > 93)
        min_cr = 4;
    else
        min_cr = 2;

    format_factor = bit_depth_minus8 == 2 ? 1.875 :
                    bit_depth_minus8 == 4 ? 2.25 : 1.5;

    min_cr_scale *= min_cr;
    format_factor /= min_cr_scale;

    hevc_get_max_mbps(level_idc, &max_mbps, &max_byte_per_pic);

    max_byte_per_pic_not0 = (uint64_t)((((float_t)max_mbps * (float_t)100) / (float_t)frame_rate)  * format_factor);

    if (user_max_frame_size) {
        profile_level_max_frame = (uint32_t)MIN(user_max_frame_size, max_byte_per_pic);
        profile_level_max_frame = (uint32_t)MIN(max_byte_per_pic_not0, profile_level_max_frame);
    } else
        profile_level_max_frame = (uint32_t)MIN(max_byte_per_pic_not0, max_byte_per_pic);

    return MIN(profile_level_max_frame,
               seq_param->pic_width_in_luma_samples * seq_param->pic_height_in_luma_samples);
}

uint32_t
gen10_hevc_enc_get_max_num_slices(VAEncSequenceParameterBufferHEVC *seq_param)
{
    uint32_t max_num_slices = 0;

    switch (seq_param->general_level_idc) {
    case 30:
        max_num_slices = 16;
        break;
    case 60:
        max_num_slices = 16;
        break;
    case 63:
        max_num_slices = 20;
        break;
    case 90:
        max_num_slices = 30;
        break;
    case 93:
        max_num_slices = 40;
        break;
    case 120:
        max_num_slices = 75;
        break;
    case 123:
        max_num_slices = 75;
        break;
    case 150:
        max_num_slices = 200;
        break;
    case 153:
        max_num_slices = 200;
        break;
    case 156:
        max_num_slices = 200;
        break;
    case 180:
        max_num_slices = 600;
        break;
    case 183:
        max_num_slices = 600;
        break;
    case 186:
        max_num_slices = 600;
        break;
    default:
        max_num_slices = 0;
        break;
    }

    return max_num_slices;
}

static
uint32_t gen10_hevc_get_start_code_offset(unsigned char *ptr,
                                          uint32_t size)
{
    uint32_t count = 0;

    while (count < size && *ptr != 0x01) {
        if (*ptr != 0)
            break;

        count++;
        ptr++;
    }

    return count + 1;
}

static
uint32_t gen10_hevc_get_emulation_num(unsigned char *ptr,
                                      uint32_t size)
{
    uint32_t emulation_num = 0;
    uint32_t header_offset = 0;
    uint32_t zero_count = 0;
    int i = 0;

    header_offset = gen10_hevc_get_start_code_offset(ptr, size);
    ptr += header_offset;

    for (i = 0 ; i < (size - header_offset); i++, ptr++) {
        if (zero_count == 2 && !(*ptr & 0xFC)) {
            zero_count = 0;
            emulation_num++;
        }

        if (*ptr == 0x00)
            zero_count++;
        else
            zero_count = 0;
    }

    return emulation_num;
}

#define HEVC_ENC_START_CODE_NAL_OFFSET                  (2)

uint32_t
gen10_hevc_enc_get_pic_header_size(struct encode_state *encode_state)
{
    VAEncPackedHeaderParameterBuffer *param = NULL;
    uint32_t header_begin = 0;
    uint32_t accum_size = 0;
    unsigned char *header_data = NULL;
    uint32_t length_in_bytes = 0;
    int packed_type = 0;
    int idx = 0, count = 0, idx_offset = 0;
    int i = 0, slice_idx = 0, start_index = 0;

    for (i = 0; i < 4; i++) {
        idx_offset = 0;
        switch (i) {
        case 0:
            packed_type = VAEncPackedHeaderHEVC_VPS;
            break;
        case 1:
            packed_type = VAEncPackedHeaderHEVC_VPS;
            idx_offset = 1;
            break;
        case 2:
            packed_type = VAEncPackedHeaderHEVC_PPS;
            break;
        case 3:
            packed_type = VAEncPackedHeaderHEVC_SEI;
            break;
        default:
            break;
        }

        idx = va_enc_packed_type_to_idx(packed_type) + idx_offset;
        if (encode_state->packed_header_data[idx]) {
            param = (VAEncPackedHeaderParameterBuffer *)encode_state->packed_header_param[idx]->buffer;
            header_data = (unsigned char *)encode_state->packed_header_data[idx]->buffer;
            length_in_bytes = (param->bit_length + 7) / 8;

            header_begin = gen10_hevc_get_start_code_offset(header_data, length_in_bytes) +
                           HEVC_ENC_START_CODE_NAL_OFFSET;

            accum_size += length_in_bytes;
            if (!param->has_emulation_bytes)
                accum_size += gen10_hevc_get_emulation_num(header_data,
                                                           length_in_bytes);
        }
    }

    for (slice_idx = 0; slice_idx < encode_state->num_slice_params_ext; slice_idx++) {
        count = encode_state->slice_rawdata_count[slice_idx];
        start_index = encode_state->slice_rawdata_index[slice_idx] &
                      SLICE_PACKED_DATA_INDEX_MASK;

        if (start_index >= 5)
            break;

        for (i = 0; i < count; i++) {
            param = (VAEncPackedHeaderParameterBuffer *)
                    (encode_state->packed_header_params_ext[start_index + i]->buffer);

            if (param->type == VAEncPackedHeaderSlice)
                continue;

            header_data = (unsigned char *)encode_state->packed_header_data[start_index]->buffer;
            length_in_bytes = (param->bit_length + 7) / 8;

            accum_size += length_in_bytes;
            if (!param->has_emulation_bytes)
                accum_size += gen10_hevc_get_emulation_num(header_data,
                                                           length_in_bytes);
        }
    }

    header_begin = MIN(header_begin, accum_size);

    return ((accum_size - header_begin) * 8);
}
