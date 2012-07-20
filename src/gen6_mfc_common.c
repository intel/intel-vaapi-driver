/*
 * Copyright © 2012 Intel Corporation
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
 *    Zhao Yakui <yakui.zhao@intel.com>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "intel_batchbuffer.h"
#include "i965_defines.h"
#include "i965_structs.h"
#include "i965_drv_video.h"
#include "i965_encoder.h"
#include "i965_encoder_utils.h"
#include "gen6_mfc.h"
#include "gen6_vme.h"


#define BRC_CLIP(x, min, max) \
{ \
    x = ((x > (max)) ? (max) : ((x < (min)) ? (min) : x)); \
}

#define BRC_P_B_QP_DIFF 4
#define BRC_I_P_QP_DIFF 2
#define BRC_I_B_QP_DIFF (BRC_I_P_QP_DIFF + BRC_P_B_QP_DIFF)

#define BRC_PWEIGHT 0.6  /* weight if P slice with comparison to I slice */
#define BRC_BWEIGHT 0.25 /* weight if B slice with comparison to I slice */

#define BRC_QP_MAX_CHANGE 5 /* maximum qp modification */
#define BRC_CY 0.1 /* weight for */
#define BRC_CX_UNDERFLOW 5.
#define BRC_CX_OVERFLOW -4.

#define BRC_PI_0_5 1.5707963267948966192313216916398

static void
intel_mfc_bit_rate_control_context_init(struct encode_state *encode_state, 
                                       struct gen6_mfc_context *mfc_context)
{
    VAEncSequenceParameterBufferH264 *pSequenceParameter = (VAEncSequenceParameterBufferH264 *)encode_state->seq_param_ext->buffer;
    int width_in_mbs = (mfc_context->surface_state.width + 15) / 16;
    int height_in_mbs = (mfc_context->surface_state.height + 15) / 16;
    float fps =  pSequenceParameter->time_scale * 0.5 / pSequenceParameter->num_units_in_tick ;
    int inter_mb_size = pSequenceParameter->bits_per_second * 1.0 / (fps+4.0) / width_in_mbs / height_in_mbs;
    int intra_mb_size = inter_mb_size * 5.0;
    int i;

    mfc_context->bit_rate_control_context[SLICE_TYPE_I].target_mb_size = intra_mb_size;
    mfc_context->bit_rate_control_context[SLICE_TYPE_I].target_frame_size = intra_mb_size * width_in_mbs * height_in_mbs;
    mfc_context->bit_rate_control_context[SLICE_TYPE_P].target_mb_size = inter_mb_size;
    mfc_context->bit_rate_control_context[SLICE_TYPE_P].target_frame_size = inter_mb_size * width_in_mbs * height_in_mbs;
    mfc_context->bit_rate_control_context[SLICE_TYPE_B].target_mb_size = inter_mb_size;
    mfc_context->bit_rate_control_context[SLICE_TYPE_B].target_frame_size = inter_mb_size * width_in_mbs * height_in_mbs;

    for(i = 0 ; i < 3; i++) {
        mfc_context->bit_rate_control_context[i].QpPrimeY = 26;
        mfc_context->bit_rate_control_context[i].MaxQpNegModifier = 6;
        mfc_context->bit_rate_control_context[i].MaxQpPosModifier = 6;
        mfc_context->bit_rate_control_context[i].GrowInit = 6;
        mfc_context->bit_rate_control_context[i].GrowResistance = 4;
        mfc_context->bit_rate_control_context[i].ShrinkInit = 6;
        mfc_context->bit_rate_control_context[i].ShrinkResistance = 4;
        
        mfc_context->bit_rate_control_context[i].Correct[0] = 8;
        mfc_context->bit_rate_control_context[i].Correct[1] = 4;
        mfc_context->bit_rate_control_context[i].Correct[2] = 2;
        mfc_context->bit_rate_control_context[i].Correct[3] = 2;
        mfc_context->bit_rate_control_context[i].Correct[4] = 4;
        mfc_context->bit_rate_control_context[i].Correct[5] = 8;
    }
    
    mfc_context->bit_rate_control_context[SLICE_TYPE_I].TargetSizeInWord = (intra_mb_size + 16)/ 16;
    mfc_context->bit_rate_control_context[SLICE_TYPE_P].TargetSizeInWord = (inter_mb_size + 16)/ 16;
    mfc_context->bit_rate_control_context[SLICE_TYPE_B].TargetSizeInWord = (inter_mb_size + 16)/ 16;

    mfc_context->bit_rate_control_context[SLICE_TYPE_I].MaxSizeInWord = mfc_context->bit_rate_control_context[SLICE_TYPE_I].TargetSizeInWord * 1.5;
    mfc_context->bit_rate_control_context[SLICE_TYPE_P].MaxSizeInWord = mfc_context->bit_rate_control_context[SLICE_TYPE_P].TargetSizeInWord * 1.5;
    mfc_context->bit_rate_control_context[SLICE_TYPE_B].MaxSizeInWord = mfc_context->bit_rate_control_context[SLICE_TYPE_B].TargetSizeInWord * 1.5;
}

static void intel_mfc_brc_init(struct encode_state *encode_state,
                  struct intel_encoder_context* encoder_context)
{
    struct gen6_mfc_context *mfc_context = encoder_context->mfc_context;
    VAEncSequenceParameterBufferH264 *pSequenceParameter = (VAEncSequenceParameterBufferH264 *)encode_state->seq_param_ext->buffer;
    VAEncMiscParameterBuffer* pMiscParamHRD = (VAEncMiscParameterBuffer*)encode_state->misc_param[VAEncMiscParameterTypeHRD]->buffer;
    VAEncMiscParameterHRD* pParameterHRD = (VAEncMiscParameterHRD*)pMiscParamHRD->data;
    double bitrate = pSequenceParameter->bits_per_second;
    double framerate = (double)pSequenceParameter->time_scale /(2 * (double)pSequenceParameter->num_units_in_tick);
    int inum = 1, pnum = 0, bnum = 0; /* Gop structure: number of I, P, B frames in the Gop. */
    int intra_period = pSequenceParameter->intra_period;
    int ip_period = pSequenceParameter->ip_period;
    double qp1_size = 0.1 * 8 * 3 * (pSequenceParameter->picture_width_in_mbs<<4) * (pSequenceParameter->picture_height_in_mbs<<4)/2;
    double qp51_size = 0.001 * 8 * 3 * (pSequenceParameter->picture_width_in_mbs<<4) * (pSequenceParameter->picture_height_in_mbs<<4)/2;
    double bpf;

    if (pSequenceParameter->ip_period) {
        pnum = (intra_period + ip_period - 1)/ip_period - 1;
        bnum = intra_period - inum - pnum;
    }

    mfc_context->brc.mode = encoder_context->rate_control_mode;

    mfc_context->brc.target_frame_size[SLICE_TYPE_I] = (int)((double)((bitrate * intra_period)/framerate) /
                                                             (double)(inum + BRC_PWEIGHT * pnum + BRC_BWEIGHT * bnum));
    mfc_context->brc.target_frame_size[SLICE_TYPE_P] = BRC_PWEIGHT * mfc_context->brc.target_frame_size[SLICE_TYPE_I];
    mfc_context->brc.target_frame_size[SLICE_TYPE_B] = BRC_BWEIGHT * mfc_context->brc.target_frame_size[SLICE_TYPE_I];

    mfc_context->brc.gop_nums[SLICE_TYPE_I] = inum;
    mfc_context->brc.gop_nums[SLICE_TYPE_P] = pnum;
    mfc_context->brc.gop_nums[SLICE_TYPE_B] = bnum;

    bpf = mfc_context->brc.bits_per_frame = bitrate/framerate;

    mfc_context->hrd.buffer_size = (double)pParameterHRD->buffer_size;
    mfc_context->hrd.current_buffer_fullness =
        (double)(pParameterHRD->initial_buffer_fullness < mfc_context->hrd.buffer_size)?
            pParameterHRD->initial_buffer_fullness: mfc_context->hrd.buffer_size/2.;
    mfc_context->hrd.target_buffer_fullness = (double)mfc_context->hrd.buffer_size/2.;
    mfc_context->hrd.buffer_capacity = (double)mfc_context->hrd.buffer_size/qp1_size;
    mfc_context->hrd.violation_noted = 0;

    if ((bpf > qp51_size) && (bpf < qp1_size)) {
        mfc_context->bit_rate_control_context[SLICE_TYPE_P].QpPrimeY = 51 - 50*(bpf - qp51_size)/(qp1_size - qp51_size);
    }
    else if (bpf >= qp1_size)
        mfc_context->bit_rate_control_context[SLICE_TYPE_P].QpPrimeY = 1;
    else if (bpf <= qp51_size)
        mfc_context->bit_rate_control_context[SLICE_TYPE_P].QpPrimeY = 51;

    mfc_context->bit_rate_control_context[SLICE_TYPE_I].QpPrimeY = mfc_context->bit_rate_control_context[SLICE_TYPE_P].QpPrimeY;
    mfc_context->bit_rate_control_context[SLICE_TYPE_B].QpPrimeY = mfc_context->bit_rate_control_context[SLICE_TYPE_I].QpPrimeY;

    BRC_CLIP(mfc_context->bit_rate_control_context[SLICE_TYPE_I].QpPrimeY, 1, 51);
    BRC_CLIP(mfc_context->bit_rate_control_context[SLICE_TYPE_P].QpPrimeY, 1, 51);
    BRC_CLIP(mfc_context->bit_rate_control_context[SLICE_TYPE_B].QpPrimeY, 1, 51);
}

int intel_mfc_update_hrd(struct encode_state *encode_state,
                               struct gen6_mfc_context *mfc_context,
                               int frame_bits)
{
    double prev_bf = mfc_context->hrd.current_buffer_fullness;

    mfc_context->hrd.current_buffer_fullness -= frame_bits;

    if (mfc_context->hrd.buffer_size > 0 && mfc_context->hrd.current_buffer_fullness <= 0.) {
        mfc_context->hrd.current_buffer_fullness = prev_bf;
        return BRC_UNDERFLOW;
    }
    
    mfc_context->hrd.current_buffer_fullness += mfc_context->brc.bits_per_frame;
    if (mfc_context->hrd.buffer_size > 0 && mfc_context->hrd.current_buffer_fullness > mfc_context->hrd.buffer_size) {
        if (mfc_context->brc.mode == VA_RC_VBR)
            mfc_context->hrd.current_buffer_fullness = mfc_context->hrd.buffer_size;
        else {
            mfc_context->hrd.current_buffer_fullness = prev_bf;
            return BRC_OVERFLOW;
        }
    }
    return BRC_NO_HRD_VIOLATION;
}

int intel_mfc_brc_postpack(struct encode_state *encode_state,
                                 struct gen6_mfc_context *mfc_context,
                                 int frame_bits)
{
    gen6_brc_status sts = BRC_NO_HRD_VIOLATION;
    VAEncSliceParameterBufferH264 *pSliceParameter = (VAEncSliceParameterBufferH264 *)encode_state->slice_params_ext[0]->buffer; 
    int slicetype = pSliceParameter->slice_type;
    int qpi = mfc_context->bit_rate_control_context[SLICE_TYPE_I].QpPrimeY;
    int qpp = mfc_context->bit_rate_control_context[SLICE_TYPE_P].QpPrimeY;
    int qpb = mfc_context->bit_rate_control_context[SLICE_TYPE_B].QpPrimeY;
    int qp; // quantizer of previously encoded slice of current type
    int qpn; // predicted quantizer for next frame of current type in integer format
    double qpf; // predicted quantizer for next frame of current type in float format
    double delta_qp; // QP correction
    int target_frame_size, frame_size_next;
    /* Notes:
     *  x - how far we are from HRD buffer borders
     *  y - how far we are from target HRD buffer fullness
     */
    double x, y;
    double frame_size_alpha;

    if (slicetype == SLICE_TYPE_SP)
        slicetype = SLICE_TYPE_P;
    else if (slicetype == SLICE_TYPE_SI)
        slicetype = SLICE_TYPE_I;

    qp = mfc_context->bit_rate_control_context[slicetype].QpPrimeY;

    target_frame_size = mfc_context->brc.target_frame_size[slicetype];
    if (mfc_context->hrd.buffer_capacity < 5)
        frame_size_alpha = 0;
    else
        frame_size_alpha = (double)mfc_context->brc.gop_nums[slicetype];
    if (frame_size_alpha > 30) frame_size_alpha = 30;
    frame_size_next = target_frame_size + (double)(target_frame_size - frame_bits) /
                                          (double)(frame_size_alpha + 1.);

    /* frame_size_next: avoiding negative number and too small value */
    if ((double)frame_size_next < (double)(target_frame_size * 0.25))
        frame_size_next = (int)((double)target_frame_size * 0.25);

    qpf = (double)qp * target_frame_size / frame_size_next;
    qpn = (int)(qpf + 0.5);

    if (qpn == qp) {
        /* setting qpn we round qpf making mistakes: now we are trying to compensate this */
        mfc_context->brc.qpf_rounding_accumulator += qpf - qpn;
        if (mfc_context->brc.qpf_rounding_accumulator > 1.0) {
            qpn++;
            mfc_context->brc.qpf_rounding_accumulator = 0.;
        } else if (mfc_context->brc.qpf_rounding_accumulator < -1.0) {
            qpn--;
            mfc_context->brc.qpf_rounding_accumulator = 0.;
        }
    }
    /* making sure that QP is not changing too fast */
    if ((qpn - qp) > BRC_QP_MAX_CHANGE) qpn = qp + BRC_QP_MAX_CHANGE;
    else if ((qpn - qp) < -BRC_QP_MAX_CHANGE) qpn = qp - BRC_QP_MAX_CHANGE;
    /* making sure that with QP predictions we did do not leave QPs range */
    BRC_CLIP(qpn, 1, 51);

    /* checking wthether HRD compliance is still met */
    sts = intel_mfc_update_hrd(encode_state, mfc_context, frame_bits);

    /* calculating QP delta as some function*/
    x = mfc_context->hrd.target_buffer_fullness - mfc_context->hrd.current_buffer_fullness;
    if (x > 0) {
        x /= mfc_context->hrd.target_buffer_fullness;
        y = mfc_context->hrd.current_buffer_fullness;
    }
    else {
        x /= (mfc_context->hrd.buffer_size - mfc_context->hrd.target_buffer_fullness);
        y = mfc_context->hrd.buffer_size - mfc_context->hrd.current_buffer_fullness;
    }
    if (y < 0.01) y = 0.01;
    if (x > 1) x = 1;
    else if (x < -1) x = -1;

    delta_qp = BRC_QP_MAX_CHANGE*exp(-1/y)*sin(BRC_PI_0_5 * x);
    qpn = (int)(qpn + delta_qp + 0.5);

    /* making sure that with QP predictions we did do not leave QPs range */
    BRC_CLIP(qpn, 1, 51);

    if (sts == BRC_NO_HRD_VIOLATION) { // no HRD violation
        /* correcting QPs of slices of other types */
        if (slicetype == SLICE_TYPE_P) {
            if (abs(qpn + BRC_P_B_QP_DIFF - qpb) > 2)
                mfc_context->bit_rate_control_context[SLICE_TYPE_B].QpPrimeY += (qpn + BRC_P_B_QP_DIFF - qpb) >> 1;
            if (abs(qpn - BRC_I_P_QP_DIFF - qpi) > 2)
                mfc_context->bit_rate_control_context[SLICE_TYPE_I].QpPrimeY += (qpn - BRC_I_P_QP_DIFF - qpi) >> 1;
        } else if (slicetype == SLICE_TYPE_I) {
            if (abs(qpn + BRC_I_B_QP_DIFF - qpb) > 4)
                mfc_context->bit_rate_control_context[SLICE_TYPE_B].QpPrimeY += (qpn + BRC_I_B_QP_DIFF - qpb) >> 2;
            if (abs(qpn + BRC_I_P_QP_DIFF - qpp) > 2)
                mfc_context->bit_rate_control_context[SLICE_TYPE_P].QpPrimeY += (qpn + BRC_I_P_QP_DIFF - qpp) >> 2;
        } else { // SLICE_TYPE_B
            if (abs(qpn - BRC_P_B_QP_DIFF - qpp) > 2)
                mfc_context->bit_rate_control_context[SLICE_TYPE_P].QpPrimeY += (qpn - BRC_P_B_QP_DIFF - qpp) >> 1;
            if (abs(qpn - BRC_I_B_QP_DIFF - qpi) > 4)
                mfc_context->bit_rate_control_context[SLICE_TYPE_I].QpPrimeY += (qpn - BRC_I_B_QP_DIFF - qpi) >> 2;
        }
        BRC_CLIP(mfc_context->bit_rate_control_context[SLICE_TYPE_I].QpPrimeY, 1, 51);
        BRC_CLIP(mfc_context->bit_rate_control_context[SLICE_TYPE_P].QpPrimeY, 1, 51);
        BRC_CLIP(mfc_context->bit_rate_control_context[SLICE_TYPE_B].QpPrimeY, 1, 51);
    } else if (sts == BRC_UNDERFLOW) { // underflow
        if (qpn <= qp) qpn = qp + 1;
        if (qpn > 51) {
            qpn = 51;
            sts = BRC_UNDERFLOW_WITH_MAX_QP; //underflow with maxQP
        }
    } else if (sts == BRC_OVERFLOW) {
        if (qpn >= qp) qpn = qp - 1;
        if (qpn < 1) { // < 0 (?) overflow with minQP
            qpn = 1;
            sts = BRC_OVERFLOW_WITH_MIN_QP; // bit stuffing to be done
        }
    }

    mfc_context->bit_rate_control_context[slicetype].QpPrimeY = qpn;

    return sts;
}

static void intel_mfc_hrd_context_init(struct encode_state *encode_state,
                          struct intel_encoder_context *encoder_context)
{
    struct gen6_mfc_context *mfc_context = encoder_context->mfc_context;
    VAEncSequenceParameterBufferH264 *pSequenceParameter = (VAEncSequenceParameterBufferH264 *)encode_state->seq_param_ext->buffer;
    unsigned int rate_control_mode = encoder_context->rate_control_mode;
    int target_bit_rate = pSequenceParameter->bits_per_second;
    
    // current we only support CBR mode.
    if (rate_control_mode == VA_RC_CBR) {
        mfc_context->vui_hrd.i_bit_rate_value = target_bit_rate >> 10;
        mfc_context->vui_hrd.i_cpb_size_value = (target_bit_rate * 8) >> 10;
        mfc_context->vui_hrd.i_initial_cpb_removal_delay = mfc_context->vui_hrd.i_cpb_size_value * 0.5 * 1024 / target_bit_rate * 90000;
        mfc_context->vui_hrd.i_cpb_removal_delay = 2;
        mfc_context->vui_hrd.i_frame_number = 0;

        mfc_context->vui_hrd.i_initial_cpb_removal_delay_length = 24; 
        mfc_context->vui_hrd.i_cpb_removal_delay_length = 24;
        mfc_context->vui_hrd.i_dpb_output_delay_length = 24;
    }

}

void 
intel_mfc_hrd_context_update(struct encode_state *encode_state, 
                          struct gen6_mfc_context *mfc_context) 
{
    mfc_context->vui_hrd.i_frame_number++;
}

int intel_mfc_interlace_check(VADriverContextP ctx,
                   struct encode_state *encode_state,
                   struct intel_encoder_context *encoder_context) 
{
    struct gen6_mfc_context *mfc_context = encoder_context->mfc_context;
    VAEncSliceParameterBufferH264 *pSliceParameter;
    int i;
    int mbCount = 0;
    int width_in_mbs = (mfc_context->surface_state.width + 15) / 16;
    int height_in_mbs = (mfc_context->surface_state.height + 15) / 16;
  
    for (i = 0; i < encode_state->num_slice_params_ext; i++) {
        pSliceParameter = (VAEncSliceParameterBufferH264 *)encode_state->slice_params_ext[i]->buffer; 
        mbCount += pSliceParameter->num_macroblocks; 
    }
    
    if ( mbCount == ( width_in_mbs * height_in_mbs ) )
        return 0;

    return 1;
}

void intel_mfc_brc_prepare(struct encode_state *encode_state,
                          struct intel_encoder_context *encoder_context)
{
    unsigned int rate_control_mode = encoder_context->rate_control_mode;
    struct gen6_mfc_context *mfc_context = encoder_context->mfc_context;

    if (rate_control_mode == VA_RC_CBR) {
        /*Programing bit rate control */
        if ( mfc_context->bit_rate_control_context[SLICE_TYPE_I].MaxSizeInWord == 0 ) {
            intel_mfc_bit_rate_control_context_init(encode_state, mfc_context);
            intel_mfc_brc_init(encode_state, encoder_context);
        }

        /*Programing HRD control */
        if ( mfc_context->vui_hrd.i_cpb_size_value == 0 )
            intel_mfc_hrd_context_init(encode_state, encoder_context);    
    }
}

