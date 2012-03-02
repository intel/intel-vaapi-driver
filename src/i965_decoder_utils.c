/*
 * Copyright (C) 2006-2012 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <alloca.h>
#include "intel_batchbuffer.h"
#include "i965_decoder_utils.h"
#include "i965_defines.h"

/* Generate flat scaling matrices for H.264 decoding */
void
avc_gen_default_iq_matrix(VAIQMatrixBufferH264 *iq_matrix)
{
    /* Flat_4x4_16 */
    memset(&iq_matrix->ScalingList4x4, 16, sizeof(iq_matrix->ScalingList4x4));

    /* Flat_8x8_16 */
    memset(&iq_matrix->ScalingList8x8, 16, sizeof(iq_matrix->ScalingList8x8));
}

/* Get first macroblock bit offset for BSD, minus EPB count (AVC) */
/* XXX: slice_data_bit_offset does not account for EPB */
unsigned int
avc_get_first_mb_bit_offset(
    dri_bo                     *slice_data_bo,
    VASliceParameterBufferH264 *slice_param,
    unsigned int                mode_flag
)
{
    unsigned int slice_data_bit_offset = slice_param->slice_data_bit_offset;

    if (mode_flag == ENTROPY_CABAC)
        slice_data_bit_offset = ALIGN(slice_data_bit_offset, 0x8);
    return slice_data_bit_offset;
}

/* Get first macroblock bit offset for BSD, with EPB count (AVC) */
/* XXX: slice_data_bit_offset does not account for EPB */
unsigned int
avc_get_first_mb_bit_offset_with_epb(
    dri_bo                     *slice_data_bo,
    VASliceParameterBufferH264 *slice_param,
    unsigned int                mode_flag
)
{
    unsigned int in_slice_data_bit_offset = slice_param->slice_data_bit_offset;
    unsigned int out_slice_data_bit_offset;
    unsigned int i, j, buf_size, data_size, header_size;
    uint8_t *buf;
    int ret;

    header_size = slice_param->slice_data_bit_offset / 8;
    data_size   = slice_param->slice_data_size - slice_param->slice_data_offset;
    buf_size    = (header_size * 4 + 2) / 3; /* max possible header size */
    if (buf_size > data_size)
        buf_size = data_size;

    buf = alloca(buf_size);
    ret = dri_bo_get_subdata(
        slice_data_bo, slice_param->slice_data_offset,
        buf_size, buf
    );
    assert(ret == 0);

    for (i = 2, j = 2; i < buf_size && j < header_size; i++, j++) {
        if (buf[i] == 0x03 && buf[i - 1] == 0x00 && buf[i - 2] == 0x00)
            i += 2, j++;
    }
    out_slice_data_bit_offset = in_slice_data_bit_offset % 8 + i * 8;

    if (mode_flag == ENTROPY_CABAC)
        out_slice_data_bit_offset = ALIGN(out_slice_data_bit_offset, 0x8);
    return out_slice_data_bit_offset;
}

static inline uint8_t
get_ref_idx_state_1(const VAPictureH264 *va_pic, unsigned int frame_store_id)
{
    const unsigned int is_long_term =
        !!(va_pic->flags & VA_PICTURE_H264_LONG_TERM_REFERENCE);
    const unsigned int is_top_field =
        !!(va_pic->flags & VA_PICTURE_H264_TOP_FIELD);
    const unsigned int is_bottom_field =
        !!(va_pic->flags & VA_PICTURE_H264_BOTTOM_FIELD);

    return ((is_long_term                         << 6) |
            ((is_top_field ^ is_bottom_field ^ 1) << 5) |
            (frame_store_id                       << 1) |
            ((is_top_field ^ 1) & is_bottom_field));
}

/* Fill in Reference List Entries (Gen5+: ILK, SNB, IVB) */
void
gen5_fill_avc_ref_idx_state(
    uint8_t             state[32],
    const VAPictureH264 ref_list[32],
    unsigned int        ref_list_count,
    const GenFrameStore frame_store[MAX_GEN_REFERENCE_FRAMES]
)
{
    unsigned int i, n, frame_idx;

    for (i = 0, n = 0; i < ref_list_count; i++) {
        const VAPictureH264 * const va_pic = &ref_list[i];

        if (va_pic->flags & VA_PICTURE_H264_INVALID)
            continue;

        for (frame_idx = 0; frame_idx < MAX_GEN_REFERENCE_FRAMES; frame_idx++) {
            const GenFrameStore * const fs = &frame_store[frame_idx];
            if (fs->surface_id != VA_INVALID_ID &&
                fs->surface_id == va_pic->picture_id) {
                assert(frame_idx == fs->frame_store_id);
                break;
            }
        }
        assert(frame_idx < MAX_GEN_REFERENCE_FRAMES);
        state[n++] = get_ref_idx_state_1(va_pic, frame_idx);
    }

    for (; n < 32; n++)
        state[n] = 0xff;
}

/* Emit Reference List Entries (Gen6+: SNB, IVB) */
static void
gen6_send_avc_ref_idx_state_1(
    struct intel_batchbuffer         *batch,
    unsigned int                      list,
    const VAPictureH264              *ref_list,
    unsigned int                      ref_list_count,
    const GenFrameStore               frame_store[MAX_GEN_REFERENCE_FRAMES]
)
{
    uint8_t ref_idx_state[32];

    BEGIN_BCS_BATCH(batch, 10);
    OUT_BCS_BATCH(batch, MFX_AVC_REF_IDX_STATE | (10 - 2));
    OUT_BCS_BATCH(batch, list);
    gen5_fill_avc_ref_idx_state(
        ref_idx_state,
        ref_list, ref_list_count,
        frame_store
    );
    intel_batchbuffer_data(batch, ref_idx_state, sizeof(ref_idx_state));
    ADVANCE_BCS_BATCH(batch);
}

void
gen6_send_avc_ref_idx_state(
    struct intel_batchbuffer         *batch,
    const VASliceParameterBufferH264 *slice_param,
    const GenFrameStore               frame_store[MAX_GEN_REFERENCE_FRAMES]
)
{
    if (slice_param->slice_type == SLICE_TYPE_I ||
        slice_param->slice_type == SLICE_TYPE_SI)
        return;

    /* RefPicList0 */
    gen6_send_avc_ref_idx_state_1(
        batch, 0,
        slice_param->RefPicList0, slice_param->num_ref_idx_l0_active_minus1 + 1,
        frame_store
    );

    if (slice_param->slice_type != SLICE_TYPE_B)
        return;

    /* RefPicList1 */
    gen6_send_avc_ref_idx_state_1(
        batch, 1,
        slice_param->RefPicList1, slice_param->num_ref_idx_l1_active_minus1 + 1,
        frame_store
    );
}
