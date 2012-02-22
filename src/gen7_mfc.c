/*
 * Copyright © 2011 Intel Corporation
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
 *    Zhou Chang <chang.zhou@intel.com>
 *    Xiang, Haihao <haihao.xiang@intel.com>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "intel_batchbuffer.h"
#include "i965_defines.h"
#include "i965_structs.h"
#include "i965_drv_video.h"
#include "i965_encoder.h"
#include "i965_encoder_utils.h"
#include "gen7_mfc.h"
#include "gen6_vme.h"

static void
gen7_mfc_pipe_mode_select(VADriverContextP ctx,
                          int standard_select,
                          struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;

    assert(standard_select == MFX_FORMAT_MPEG2 ||
           standard_select == MFX_FORMAT_AVC);

    BEGIN_BCS_BATCH(batch, 5);

    OUT_BCS_BATCH(batch, MFX_PIPE_MODE_SELECT | (5 - 2));
    OUT_BCS_BATCH(batch,
                  (MFX_LONG_MODE << 17) | /* Must be long format for encoder */
                  (MFD_MODE_VLD << 15) | /* VLD mode */
                  (0 << 10) | /* disable Stream-Out */
                  (1 << 9)  | /* Post Deblocking Output */
                  (0 << 8)  | /* Pre Deblocking Output */
                  (0 << 5)  | /* not in stitch mode */
                  (1 << 4)  | /* encoding mode */
                  (standard_select << 0));  /* standard select: avc or mpeg2 */
    OUT_BCS_BATCH(batch,
                  (0 << 7)  | /* expand NOA bus flag */
                  (0 << 6)  | /* disable slice-level clock gating */
                  (0 << 5)  | /* disable clock gating for NOA */
                  (0 << 4)  | /* terminate if AVC motion and POC table error occurs */
                  (0 << 3)  | /* terminate if AVC mbdata error occurs */
                  (0 << 2)  | /* terminate if AVC CABAC/CAVLC decode error occurs */
                  (0 << 1)  |
                  (0 << 0));
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen7_mfc_surface_state(VADriverContextP ctx, struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct gen7_mfc_context *mfc_context = encoder_context->mfc_context;

    BEGIN_BCS_BATCH(batch, 6);

    OUT_BCS_BATCH(batch, MFX_SURFACE_STATE | (6 - 2));
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch,
                  ((mfc_context->surface_state.height - 1) << 18) |
                  ((mfc_context->surface_state.width - 1) << 4));
    OUT_BCS_BATCH(batch,
                  (MFX_SURFACE_PLANAR_420_8 << 28) | /* 420 planar YUV surface */
                  (1 << 27) | /* must be 1 for interleave U/V, hardware requirement */
                  (0 << 22) | /* surface object control state, FIXME??? */
                  ((mfc_context->surface_state.w_pitch - 1) << 3) | /* pitch */
                  (0 << 2)  | /* must be 0 for interleave U/V */
                  (1 << 1)  | /* must be tiled */
                  (I965_TILEWALK_YMAJOR << 0));  /* tile walk, TILEWALK_YMAJOR */
    OUT_BCS_BATCH(batch,
                  (0 << 16) | 								/* must be 0 for interleave U/V */
                  (mfc_context->surface_state.h_pitch)); 		/* y offset for U(cb) */
    OUT_BCS_BATCH(batch, 0);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen7_mfc_pipe_buf_addr_state(VADriverContextP ctx, struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct gen7_mfc_context *mfc_context = encoder_context->mfc_context;
    int i;

    BEGIN_BCS_BATCH(batch, 24);

    OUT_BCS_BATCH(batch, MFX_PIPE_BUF_ADDR_STATE | (24 - 2));
    OUT_BCS_BATCH(batch, 0);											/* pre output addr   */
    OUT_BCS_RELOC(batch, mfc_context->post_deblocking_output.bo,
                  I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
                  0);											/* post output addr  */	
    OUT_BCS_RELOC(batch, mfc_context->uncompressed_picture_source.bo,
                  I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
                  0);											/* uncompressed data */
    OUT_BCS_RELOC(batch, mfc_context->macroblock_status_buffer.bo,
                  I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
                  0);											/* StreamOut data*/
    OUT_BCS_RELOC(batch, mfc_context->intra_row_store_scratch_buffer.bo,
                  I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
                  0);	
    OUT_BCS_RELOC(batch, mfc_context->deblocking_filter_row_store_scratch_buffer.bo,
                  I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
                  0);

    /* 7..22 Reference pictures*/
    for (i = 0; i < ARRAY_ELEMS(mfc_context->reference_surfaces); i++) {
        if ( mfc_context->reference_surfaces[i].bo != NULL) {
            OUT_BCS_RELOC(batch, mfc_context->reference_surfaces[i].bo,
                          I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
                          0);			
        } else {
            OUT_BCS_BATCH(batch, 0);
        }
    }

    OUT_BCS_RELOC(batch, mfc_context->macroblock_status_buffer.bo,
                  I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
                  0);											/* Macroblock status buffer*/

    ADVANCE_BCS_BATCH(batch);
}

static void
gen7_mfc_ind_obj_base_addr_state(VADriverContextP ctx, struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct gen7_mfc_context *mfc_context = encoder_context->mfc_context;
    struct gen6_vme_context *vme_context = encoder_context->vme_context;

    BEGIN_BCS_BATCH(batch, 11);

    OUT_BCS_BATCH(batch, MFX_IND_OBJ_BASE_ADDR_STATE | (11 - 2));
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    /* MFX Indirect MV Object Base Address */
    OUT_BCS_RELOC(batch, vme_context->vme_output.bo, I915_GEM_DOMAIN_INSTRUCTION, 0, 0);
    OUT_BCS_BATCH(batch, 0x80000000); /* must set, up to 2G */
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    /*MFC Indirect PAK-BSE Object Base Address for Encoder*/	
    OUT_BCS_RELOC(batch,
                  mfc_context->mfc_indirect_pak_bse_object.bo,
                  I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
                  0);
    OUT_BCS_RELOC(batch,
                  mfc_context->mfc_indirect_pak_bse_object.bo,
                  I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
                  mfc_context->mfc_indirect_pak_bse_object.end_offset);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen7_mfc_bsp_buf_base_addr_state(VADriverContextP ctx, struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct gen7_mfc_context *mfc_context = encoder_context->mfc_context;

    BEGIN_BCS_BATCH(batch, 4);

    OUT_BCS_BATCH(batch, MFX_BSP_BUF_BASE_ADDR_STATE | (4 - 2));
    OUT_BCS_RELOC(batch, mfc_context->bsd_mpc_row_store_scratch_buffer.bo,
                  I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
                  0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen7_mfc_avc_img_state(VADriverContextP ctx, struct encode_state *encode_state,  
                       struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct gen7_mfc_context *mfc_context = encoder_context->mfc_context;
    VAEncPictureParameterBufferH264 *pPicParameter = (VAEncPictureParameterBufferH264 *)encode_state->pic_param_ext->buffer;

    int width_in_mbs = (mfc_context->surface_state.width + 15) / 16;
    int height_in_mbs = (mfc_context->surface_state.height + 15) / 16;

    BEGIN_BCS_BATCH(batch, 16);

    OUT_BCS_BATCH(batch, MFX_AVC_IMG_STATE | (16 - 2));
    OUT_BCS_BATCH(batch,
                  ((width_in_mbs * height_in_mbs) & 0xFFFF));
    OUT_BCS_BATCH(batch, 
                  ((height_in_mbs - 1) << 16) | 
                  ((width_in_mbs - 1) << 0));
    OUT_BCS_BATCH(batch, 
                  (0 << 24) |	/* Second Chroma QP Offset */
                  (0 << 16) |	/* Chroma QP Offset */
                  (0 << 14) |   /* Max-bit conformance Intra flag */
                  (0 << 13) |   /* Max Macroblock size conformance Inter flag */
                  (0 << 12) |   /* FIXME: Weighted_Pred_Flag */
                  (0 << 10) |   /* FIXME: Weighted_BiPred_Idc */
                  (0 << 8)  |   /* FIXME: Image Structure */
                  (0 << 0) );   /* Current Decoed Image Frame Store ID, reserved in Encode mode */
    OUT_BCS_BATCH(batch,
                  (0 << 16) |   /* Mininum Frame size */
                  (0 << 15) |   /* Disable reading of Macroblock Status Buffer */
                  (0 << 14) |   /* Load BitStream Pointer only once, 1 slic 1 frame */
                  (0 << 13) |   /* CABAC 0 word insertion test enable */
                  (1 << 12) |   /* MVUnpackedEnable,compliant to DXVA */
                  (1 << 10) |   /* Chroma Format IDC, 4:2:0 */
                  (0 << 9)  |   /* FIXME: MbMvFormatFlag */
                  (pPicParameter->pic_fields.bits.entropy_coding_mode_flag << 7)  |   /*0:CAVLC encoding mode,1:CABAC*/
                  (0 << 6)  |   /* Only valid for VLD decoding mode */
                  (0 << 5)  |   /* Constrained Intra Predition Flag, from PPS */
                  (0 << 4)  |   /* Direct 8x8 inference flag */
                  (pPicParameter->pic_fields.bits.transform_8x8_mode_flag << 3)  |   /*8x8 or 4x4 IDCT Transform Mode Flag*/
                  (1 << 2)  |   /* Frame MB only flag */
                  (0 << 1)  |   /* MBAFF mode is in active */
                  (0 << 0));    /* Field picture flag */
    OUT_BCS_BATCH(batch, 0);    /* Mainly about MB rate control and debug, just ignoring */
    OUT_BCS_BATCH(batch,        /* Inter and Intra Conformance Max size limit */
                  (0xBB8 << 16) |       /* InterMbMaxSz */
                  (0xEE8) );            /* IntraMbMaxSz */
    OUT_BCS_BATCH(batch, 0);            /* Reserved */
    OUT_BCS_BATCH(batch, 0);            /* Slice QP Delta for bitrate control */
    OUT_BCS_BATCH(batch, 0);            /* Slice QP Delta for bitrate control */	
    OUT_BCS_BATCH(batch, 0x8C000000);
    OUT_BCS_BATCH(batch, 0x00010000);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen7_mfc_avc_directmode_state(VADriverContextP ctx, struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct gen7_mfc_context *mfc_context = encoder_context->mfc_context;

    int i;

    BEGIN_BCS_BATCH(batch, 69);

    OUT_BCS_BATCH(batch, MFX_AVC_DIRECTMODE_STATE | (69 - 2));

    /* Reference frames and Current frames */
    for (i = 0; i < NUM_MFC_DMV_BUFFERS; i++) {
        if (mfc_context->direct_mv_buffers[i].bo != NULL) { 
            OUT_BCS_RELOC(batch, mfc_context->direct_mv_buffers[i].bo,
                          I915_GEM_DOMAIN_INSTRUCTION, 0,
                          0);
        } else {
            OUT_BCS_BATCH(batch, 0);
        }
    }

    /* POL list */
    for (i = 0; i < 32; i++) {
        OUT_BCS_BATCH(batch, i / 2);
    }

    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen7_mfc_avc_slice_state(VADriverContextP ctx,
                         int slice_type,
                         struct encode_state *encode_state,
                         struct intel_encoder_context *encoder_context,
                         int rate_control_enable,
                         int qp)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct gen7_mfc_context *mfc_context = encoder_context->mfc_context;
    VAEncSliceParameterBufferH264 *pSliceParameter = (VAEncSliceParameterBufferH264 *)encode_state->slice_params_ext[0]->buffer; /* TODO: multi slices support */
    int bit_rate_control_target, maxQpN, maxQpP;
    unsigned char correct[6], grow, shrink;
    int i;

    if (slice_type == SLICE_TYPE_I)
        bit_rate_control_target = 0;
    else
        bit_rate_control_target = 1;

    maxQpN = mfc_context->bit_rate_control_context[bit_rate_control_target].MaxQpNegModifier;
    maxQpP = mfc_context->bit_rate_control_context[bit_rate_control_target].MaxQpPosModifier;

    for (i = 0; i < 6; i++)
        correct[i] = mfc_context->bit_rate_control_context[bit_rate_control_target].Correct[i];

    grow = mfc_context->bit_rate_control_context[bit_rate_control_target].GrowInit + 
        (mfc_context->bit_rate_control_context[bit_rate_control_target].GrowResistance << 4);
    shrink = mfc_context->bit_rate_control_context[bit_rate_control_target].ShrinkInit + 
        (mfc_context->bit_rate_control_context[bit_rate_control_target].ShrinkResistance << 4);

    BEGIN_BCS_BATCH(batch, 11);;

    OUT_BCS_BATCH(batch, MFX_AVC_SLICE_STATE | (11 - 2) );
    OUT_BCS_BATCH(batch, slice_type);			/*Slice Type: I:P:B Slice*/

    if (slice_type == SLICE_TYPE_I) {
        OUT_BCS_BATCH(batch, 0);			/*no reference frames and pred_weight_table*/
    } else {
        OUT_BCS_BATCH(batch, 0x00010000); 	/*1 reference frame*/
    }

    OUT_BCS_BATCH(batch, 
                  (pSliceParameter->direct_spatial_mv_pred_flag << 29) |             /*Direct Prediction Type*/
                  (0 << 24) |                /*Enable deblocking operation*/
                  (qp << 16) | 			/*Slice Quantization Parameter*/
                  (0x0202 << 0));
    OUT_BCS_BATCH(batch, 0);			/*First MB X&Y , the postion of current slice*/
    OUT_BCS_BATCH(batch, (((mfc_context->surface_state.height+15)/16) << 16));
    OUT_BCS_BATCH(batch, 
                  (rate_control_enable << 31) |		/*in CBR mode RateControlCounterEnable = enable*/
                  (1 << 30) |		/*ResetRateControlCounter*/
                  (0 << 28) |		/*RC Triggle Mode = Always Rate Control*/
                  (4 << 24) |     /*RC Stable Tolerance, middle level*/
                  (rate_control_enable << 23) |     /*RC Panic Enable*/                 
                  (0 << 22) |     /*QP mode, don't modfiy CBP*/
                  (0 << 21) |     /*MB Type Direct Conversion Enabled*/ 
                  (0 << 20) |     /*MB Type Skip Conversion Enabled*/ 
                  (1 << 19) |     /*IsLastSlice*/
                  (0 << 18) | 	/*BitstreamOutputFlag Compressed BitStream Output Disable Flag 0:enable 1:disable*/
                  (1 << 17) |	    /*HeaderPresentFlag*/	
                  (1 << 16) |	    /*SliceData PresentFlag*/
                  (1 << 15) |	    /*TailPresentFlag*/
                  (1 << 13) |	    /*RBSP NAL TYPE*/	
                  (0 << 12) );    /*CabacZeroWordInsertionEnable*/
    OUT_BCS_BATCH(batch, mfc_context->mfc_indirect_pak_bse_object.offset);
    OUT_BCS_BATCH(batch,
                  (maxQpN << 24) |     /*Target QP - 24 is lowest QP*/ 
                  (maxQpP << 16) |     /*Target QP + 20 is highest QP*/
                  (shrink << 8)  |
                  (grow << 0));   
    OUT_BCS_BATCH(batch,
                  (correct[5] << 20) |
                  (correct[4] << 16) |
                  (correct[3] << 12) |
                  (correct[2] << 8) |
                  (correct[1] << 4) |
                  (correct[0] << 0));
    OUT_BCS_BATCH(batch, 0);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen7_mfc_qm_state(VADriverContextP ctx,
                  int qm_type,
                  unsigned int *qm,
                  int qm_length,
                  struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    unsigned int qm_buffer[16];

    assert(qm_length <= 16);
    assert(sizeof(*qm) == 4);
    memcpy(qm_buffer, qm, qm_length * 4);

    BEGIN_BCS_BATCH(batch, 18);
    OUT_BCS_BATCH(batch, MFX_QM_STATE | (18 - 2));
    OUT_BCS_BATCH(batch, qm_type << 0);
    intel_batchbuffer_data(batch, qm_buffer, 16 * 4);
    ADVANCE_BCS_BATCH(batch);
}

static void
gen7_mfc_avc_qm_state(VADriverContextP ctx, struct intel_encoder_context *encoder_context)
{
    unsigned int qm[16] = {
        0x10101010, 0x10101010, 0x10101010, 0x10101010,
        0x10101010, 0x10101010, 0x10101010, 0x10101010,
        0x10101010, 0x10101010, 0x10101010, 0x10101010,
        0x10101010, 0x10101010, 0x10101010, 0x10101010
    };

    gen7_mfc_qm_state(ctx, MFX_QM_AVC_4X4_INTRA_MATRIX, qm, 12, encoder_context);
    gen7_mfc_qm_state(ctx, MFX_QM_AVC_4X4_INTER_MATRIX, qm, 12, encoder_context);
    gen7_mfc_qm_state(ctx, MFX_QM_AVC_8x8_INTRA_MATRIX, qm, 16, encoder_context);
    gen7_mfc_qm_state(ctx, MFX_QM_AVC_8x8_INTER_MATRIX, qm, 16, encoder_context);
}

static void
gen7_mfc_fqm_state(VADriverContextP ctx,
                   int fqm_type,
                   unsigned int *fqm,
                   int fqm_length,
                   struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    unsigned int fqm_buffer[32];

    assert(fqm_length <= 32);
    assert(sizeof(*fqm) == 4);
    memcpy(fqm_buffer, fqm, fqm_length * 4);

    BEGIN_BCS_BATCH(batch, 34);
    OUT_BCS_BATCH(batch, MFX_FQM_STATE | (34 - 2));
    OUT_BCS_BATCH(batch, fqm_type << 0);
    intel_batchbuffer_data(batch, fqm_buffer, 32 * 4);
    ADVANCE_BCS_BATCH(batch);
}

static void
gen7_mfc_avc_fqm_state(VADriverContextP ctx, struct intel_encoder_context *encoder_context)
{
    unsigned int qm[32] = {
        0x10001000, 0x10001000, 0x10001000, 0x10001000,
        0x10001000, 0x10001000, 0x10001000, 0x10001000,
        0x10001000, 0x10001000, 0x10001000, 0x10001000,
        0x10001000, 0x10001000, 0x10001000, 0x10001000,
        0x10001000, 0x10001000, 0x10001000, 0x10001000,
        0x10001000, 0x10001000, 0x10001000, 0x10001000,
        0x10001000, 0x10001000, 0x10001000, 0x10001000,
        0x10001000, 0x10001000, 0x10001000, 0x10001000
    };

    gen7_mfc_fqm_state(ctx, MFX_QM_AVC_4X4_INTRA_MATRIX, qm, 24, encoder_context);
    gen7_mfc_fqm_state(ctx, MFX_QM_AVC_4X4_INTER_MATRIX, qm, 24, encoder_context);
    gen7_mfc_fqm_state(ctx, MFX_QM_AVC_8x8_INTRA_MATRIX, qm, 32, encoder_context);
    gen7_mfc_fqm_state(ctx, MFX_QM_AVC_8x8_INTER_MATRIX, qm, 32, encoder_context);
}

static void
gen7_mfc_avc_ref_idx_state(VADriverContextP ctx, struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    int i;

    BEGIN_BCS_BATCH(batch, 10);
    OUT_BCS_BATCH(batch, MFX_AVC_REF_IDX_STATE | 8); 
    OUT_BCS_BATCH(batch, 0);                  //Select L0
    OUT_BCS_BATCH(batch, 0x80808020);         //Only 1 reference

    for (i = 0; i < 7; i++) {
        OUT_BCS_BATCH(batch, 0x80808080);
    }   

    ADVANCE_BCS_BATCH(batch);

    BEGIN_BCS_BATCH(batch, 10);
    OUT_BCS_BATCH(batch, MFX_AVC_REF_IDX_STATE | 8); 
    OUT_BCS_BATCH(batch, 1);                  //Select L1
    OUT_BCS_BATCH(batch, 0x80808022);         //Only 1 reference
    for(i = 0; i < 7; i++) {
        OUT_BCS_BATCH(batch, 0x80808080);
    }   
    ADVANCE_BCS_BATCH(batch);
}
	
static void
gen7_mfc_avc_insert_object(VADriverContextP ctx, struct intel_encoder_context *encoder_context,
                           unsigned int *insert_data, int lenght_in_dws, int data_bits_in_last_dw,
                           int skip_emul_byte_count, int is_last_header, int is_end_of_slice, int emulation_flag)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;

    BEGIN_BCS_BATCH(batch, lenght_in_dws + 2);

    OUT_BCS_BATCH(batch, MFX_INSERT_OBJECT | (lenght_in_dws + 2 - 2));
    OUT_BCS_BATCH(batch,
                  (0 << 16) |   /* always start at offset 0 */
                  (data_bits_in_last_dw << 8) |
                  (skip_emul_byte_count << 4) |
                  (!!emulation_flag << 3) |
                  ((!!is_last_header) << 2) |
                  ((!!is_end_of_slice) << 1) |
                  (0 << 0));    /* FIXME: ??? */
    intel_batchbuffer_data(batch, insert_data, lenght_in_dws * 4);

    ADVANCE_BCS_BATCH(batch);
}

static int
gen7_mfc_avc_pak_object_intra(VADriverContextP ctx, int x, int y, int end_mb, int qp,unsigned int *msg,
                              struct intel_encoder_context *encoder_context,
                              unsigned char target_mb_size, unsigned char max_mb_size)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    int len_in_dwords = 11;

    BEGIN_BCS_BATCH(batch, len_in_dwords);

    OUT_BCS_BATCH(batch, MFC_AVC_PAK_OBJECT | (len_in_dwords - 2));
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 
                  (0 << 24) |		/* PackedMvNum, Debug*/
                  (0 << 20) | 		/* No motion vector */
                  (1 << 19) |		/* CbpDcY */
                  (1 << 18) |		/* CbpDcU */
                  (1 << 17) |		/* CbpDcV */
                  (msg[0] & 0xFFFF) );

    OUT_BCS_BATCH(batch, (0xFFFF << 16) | (y << 8) | x);		/* Code Block Pattern for Y*/
    OUT_BCS_BATCH(batch, 0x000F000F);							/* Code Block Pattern */		
    OUT_BCS_BATCH(batch, (0 << 27) | (end_mb << 26) | qp);	/* Last MB */

    /*Stuff for Intra MB*/
    OUT_BCS_BATCH(batch, msg[1]);			/* We using Intra16x16 no 4x4 predmode*/	
    OUT_BCS_BATCH(batch, msg[2]);	
    OUT_BCS_BATCH(batch, msg[3]&0xFC);		
    
    /*MaxSizeInWord and TargetSzieInWord*/
    OUT_BCS_BATCH(batch, (max_mb_size << 24) |
                  (target_mb_size << 16) );

    ADVANCE_BCS_BATCH(batch);

    return len_in_dwords;
}

static int
gen7_mfc_avc_pak_object_inter(VADriverContextP ctx, int x, int y, int end_mb, int qp, unsigned int offset,
                              struct intel_encoder_context *encoder_context,
                              unsigned char target_mb_size,unsigned char max_mb_size, int slice_type)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    int len_in_dwords = 11;

    BEGIN_BCS_BATCH(batch, len_in_dwords);

    OUT_BCS_BATCH(batch, MFC_AVC_PAK_OBJECT | (len_in_dwords - 2));

    OUT_BCS_BATCH(batch, 32);         /* 32 MV*/
    OUT_BCS_BATCH(batch, offset);

    OUT_BCS_BATCH(batch, 
                  (1 << 24) |     /* PackedMvNum, Debug*/
                  (4 << 20) |     /* 8 MV, SNB don't use it*/
                  (1 << 19) |     /* CbpDcY */
                  (1 << 18) |     /* CbpDcU */
                  (1 << 17) |     /* CbpDcV */
                  (0 << 15) |     /* Transform8x8Flag = 0*/
                  (0 << 14) |     /* Frame based*/
                  (0 << 13) |     /* Inter MB */
                  (1 << 8)  |     /* MbType = P_L0_16x16 */   
                  (0 << 7)  |     /* MBZ for frame */
                  (0 << 6)  |     /* MBZ */
                  (2 << 4)  |     /* MBZ for inter*/
                  (0 << 3)  |     /* MBZ */
                  (0 << 2)  |     /* SkipMbFlag */
                  (0 << 0));      /* InterMbMode */

    OUT_BCS_BATCH(batch, (0xFFFF<<16) | (y << 8) | x);        /* Code Block Pattern for Y*/
    OUT_BCS_BATCH(batch, 0x000F000F);                         /* Code Block Pattern */  
#if 0 
    if ( slice_type == SLICE_TYPE_B) {
        OUT_BCS_BATCH(batch, (0xF<<28) | (end_mb << 26) | qp);	/* Last MB */
    } else {
        OUT_BCS_BATCH(batch, (end_mb << 26) | qp);	/* Last MB */
    }
#else
    OUT_BCS_BATCH(batch, (end_mb << 26) | qp);	/* Last MB */
#endif


    /*Stuff for Inter MB*/
    OUT_BCS_BATCH(batch, 0x0);        
    OUT_BCS_BATCH(batch, 0x0);    
    OUT_BCS_BATCH(batch, 0x0);        

    /*MaxSizeInWord and TargetSzieInWord*/
    OUT_BCS_BATCH(batch, (max_mb_size << 24) |
                  (target_mb_size << 16) );

    ADVANCE_BCS_BATCH(batch);

    return len_in_dwords;
}

static void
gen7_mfc_init(VADriverContextP ctx, struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct gen7_mfc_context *mfc_context = encoder_context->mfc_context;
    dri_bo *bo;
    int i;

    /*Encode common setup for MFC*/
    dri_bo_unreference(mfc_context->post_deblocking_output.bo);
    mfc_context->post_deblocking_output.bo = NULL;

    dri_bo_unreference(mfc_context->pre_deblocking_output.bo);
    mfc_context->pre_deblocking_output.bo = NULL;

    dri_bo_unreference(mfc_context->uncompressed_picture_source.bo);
    mfc_context->uncompressed_picture_source.bo = NULL;

    dri_bo_unreference(mfc_context->mfc_indirect_pak_bse_object.bo); 
    mfc_context->mfc_indirect_pak_bse_object.bo = NULL;

    for (i = 0; i < NUM_MFC_DMV_BUFFERS; i++){
        if ( mfc_context->direct_mv_buffers[i].bo != NULL);
        dri_bo_unreference(mfc_context->direct_mv_buffers[i].bo);
        mfc_context->direct_mv_buffers[i].bo = NULL;
    }

    for (i = 0; i < MAX_MFC_REFERENCE_SURFACES; i++){
        if (mfc_context->reference_surfaces[i].bo != NULL)
            dri_bo_unreference(mfc_context->reference_surfaces[i].bo);
        mfc_context->reference_surfaces[i].bo = NULL;  
    }

    dri_bo_unreference(mfc_context->intra_row_store_scratch_buffer.bo);
    bo = dri_bo_alloc(i965->intel.bufmgr,
                      "Buffer",
                      128 * 64,
                      64);
    assert(bo);
    mfc_context->intra_row_store_scratch_buffer.bo = bo;

    dri_bo_unreference(mfc_context->macroblock_status_buffer.bo);
    bo = dri_bo_alloc(i965->intel.bufmgr,
                      "Buffer",
                      128*128*16,
                      64);
    assert(bo);
    mfc_context->macroblock_status_buffer.bo = bo;

    dri_bo_unreference(mfc_context->deblocking_filter_row_store_scratch_buffer.bo);
    bo = dri_bo_alloc(i965->intel.bufmgr,
                      "Buffer",
                      49152,  /* 6 * 128 * 64 */
                      64);
    assert(bo);
    mfc_context->deblocking_filter_row_store_scratch_buffer.bo = bo;

    dri_bo_unreference(mfc_context->bsd_mpc_row_store_scratch_buffer.bo);
    bo = dri_bo_alloc(i965->intel.bufmgr,
                      "Buffer",
                      12288, /* 1.5 * 128 * 64 */
                      0x1000);
    assert(bo);
    mfc_context->bsd_mpc_row_store_scratch_buffer.bo = bo;
}

static void
gen7_mfc_avc_pipeline_programing(VADriverContextP ctx,
                                 struct encode_state *encode_state,
                                 struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    struct gen7_mfc_context *mfc_context = encoder_context->mfc_context;
    struct gen6_vme_context *vme_context = encoder_context->vme_context;
    VAEncSequenceParameterBufferH264 *pSequenceParameter = (VAEncSequenceParameterBufferH264 *)encode_state->seq_param_ext->buffer;
    VAEncPictureParameterBufferH264 *pPicParameter = (VAEncPictureParameterBufferH264 *)encode_state->pic_param_ext->buffer;
    VAEncSliceParameterBufferH264 *pSliceParameter = (VAEncSliceParameterBufferH264 *)encode_state->slice_params_ext[0]->buffer; /* FIXME: multi slices */
    unsigned int *msg = NULL, offset = 0;
    int emit_new_state = 1, object_len_in_bytes;
    int is_intra = pSliceParameter->slice_type == SLICE_TYPE_I;
    int width_in_mbs = (mfc_context->surface_state.width + 15) / 16;
    int height_in_mbs = (mfc_context->surface_state.height + 15) / 16;
    int x,y;
    unsigned int rate_control_mode = encoder_context->rate_control_mode;
    unsigned char target_mb_size = mfc_context->bit_rate_control_context[1-is_intra].TargetSizeInWord;
    unsigned char max_mb_size = mfc_context->bit_rate_control_context[1-is_intra].MaxSizeInWord;
    int qp = pPicParameter->pic_init_qp + pSliceParameter->slice_qp_delta;
    unsigned char *slice_header = NULL;
    int slice_header_length_in_bits = 0;
    unsigned int tail_data[] = { 0x0 };

    slice_header_length_in_bits = build_avc_slice_header(pSequenceParameter, pPicParameter, pSliceParameter, &slice_header);

    if (rate_control_mode == VA_RC_CBR) {
        qp = mfc_context->bit_rate_control_context[1-is_intra].QpPrimeY;
    }

    intel_batchbuffer_start_atomic_bcs(batch, 0x1000); 
    
    if (is_intra) {
        dri_bo_map(vme_context->vme_output.bo , 1);
        msg = (unsigned int *)vme_context->vme_output.bo->virtual;
    }

    for (y = 0; y < height_in_mbs; y++) {
        for (x = 0; x < width_in_mbs; x++) { 
            int last_mb = (y == (height_in_mbs-1)) && ( x == (width_in_mbs-1) );
            
            if (emit_new_state) {
                intel_batchbuffer_emit_mi_flush(batch);
                
                gen7_mfc_pipe_mode_select(ctx, MFX_FORMAT_AVC, encoder_context);
                gen7_mfc_surface_state(ctx, encoder_context);
                gen7_mfc_ind_obj_base_addr_state(ctx, encoder_context);
                gen7_mfc_pipe_buf_addr_state(ctx, encoder_context);
                gen7_mfc_bsp_buf_base_addr_state(ctx, encoder_context);
                gen7_mfc_avc_img_state(ctx, encode_state, encoder_context);
                gen7_mfc_avc_qm_state(ctx, encoder_context);
                gen7_mfc_avc_fqm_state(ctx, encoder_context);
                gen7_mfc_avc_directmode_state(ctx, encoder_context); 
                gen7_mfc_avc_ref_idx_state(ctx, encoder_context);
                gen7_mfc_avc_slice_state(ctx, pSliceParameter->slice_type, 
                                         encode_state, encoder_context, 
                                         rate_control_mode == VA_RC_CBR, pPicParameter->pic_init_qp + pSliceParameter->slice_qp_delta);

                if (encode_state->packed_header_data[VAEncPackedHeaderH264_SPS]) {
                    VAEncPackedHeaderParameterBuffer *param = NULL;
                    unsigned int *header_data = (unsigned int *)encode_state->packed_header_data[VAEncPackedHeaderH264_SPS]->buffer;
                    unsigned int length_in_bits;

                    assert(encode_state->packed_header_param[VAEncPackedHeaderH264_SPS]);
                    param = (VAEncPackedHeaderParameterBuffer *)encode_state->packed_header_param[VAEncPackedHeaderH264_SPS]->buffer;
                    length_in_bits = param->bit_length;

                    gen7_mfc_avc_insert_object(ctx, 
                                               encoder_context,
                                               header_data,
                                               ALIGN(length_in_bits, 32) >> 5,
                                               length_in_bits & 0x1f,
                                               5, /* FIXME: check it */
                                               0,
                                               0,
                                               !param->has_emulation_bytes);
                }

                if (encode_state->packed_header_data[VAEncPackedHeaderH264_PPS]) {
                    VAEncPackedHeaderParameterBuffer *param = NULL;
                    unsigned int *header_data = (unsigned int *)encode_state->packed_header_data[VAEncPackedHeaderH264_PPS]->buffer;
                    unsigned int length_in_bits;

                    assert(encode_state->packed_header_param[VAEncPackedHeaderH264_PPS]);
                    param = (VAEncPackedHeaderParameterBuffer *)encode_state->packed_header_param[VAEncPackedHeaderH264_PPS]->buffer;
                    length_in_bits = param->bit_length;

                    gen7_mfc_avc_insert_object(ctx, 
                                               encoder_context,
                                               header_data,
                                               ALIGN(length_in_bits, 32) >> 5,
                                               length_in_bits & 0x1f,
                                               5, /* FIXME: check it */
                                               0,
                                               0,
                                               !param->has_emulation_bytes);
                }

                gen7_mfc_avc_insert_object(ctx, encoder_context,
                                           (unsigned int *)slice_header, ALIGN(slice_header_length_in_bits, 32) >> 5, slice_header_length_in_bits & 0x1f,
                                           5,  /* first 5 bytes are start code + nal unit type */
                                           1, 0, 1);
                emit_new_state = 0;
            }

            if (is_intra) {
                assert(msg);
                object_len_in_bytes = gen7_mfc_avc_pak_object_intra(ctx, x, y, last_mb, qp, msg, encoder_context,target_mb_size, max_mb_size);
                msg += 4;
            } else {
                object_len_in_bytes = gen7_mfc_avc_pak_object_inter(ctx, x, y, last_mb, qp, offset, encoder_context, target_mb_size, max_mb_size, pSliceParameter->slice_type);
                offset += 64;
            }

            if (intel_batchbuffer_check_free_space(batch, object_len_in_bytes) == 0) {
                assert(0);
                intel_batchbuffer_end_atomic(batch);
                intel_batchbuffer_flush(batch);
                emit_new_state = 1;
                intel_batchbuffer_start_atomic_bcs(batch, 0x1000);
            }
        }
    }

    gen7_mfc_avc_insert_object(ctx, encoder_context,
                               tail_data, sizeof(tail_data) >> 2, 32,
                               sizeof(tail_data), 1, 1, 1);

    if (is_intra)
        dri_bo_unmap(vme_context->vme_output.bo);

    free(slice_header);

    intel_batchbuffer_end_atomic(batch);
}

static void 
gen7_mfc_free_avc_surface(void **data)
{
    struct gen7_mfc_avc_surface_aux *avc_surface = *data;

    if (!avc_surface)
        return;

    dri_bo_unreference(avc_surface->dmv_top);
    avc_surface->dmv_top = NULL;
    dri_bo_unreference(avc_surface->dmv_bottom);
    avc_surface->dmv_bottom = NULL;

    free(avc_surface);
    *data = NULL;
}

static void
gen7_mfc_bit_rate_control_context_init(struct encode_state *encode_state, 
                                       struct gen7_mfc_context *mfc_context) 
{
    VAEncSequenceParameterBufferH264 *pSequenceParameter = (VAEncSequenceParameterBufferH264 *)encode_state->seq_param_ext->buffer;
    
    int width_in_mbs = (mfc_context->surface_state.width + 15) / 16;
    int height_in_mbs = (mfc_context->surface_state.height + 15) / 16;
    float fps =  pSequenceParameter->time_scale * 0.5 / pSequenceParameter->num_units_in_tick ;
    int inter_mb_size = pSequenceParameter->bits_per_second * 1.0 / (fps+4.0) / width_in_mbs / height_in_mbs;
    int intra_mb_size = inter_mb_size * 5.0;
    int i;
    
    mfc_context->bit_rate_control_context[0].target_mb_size = intra_mb_size;
    mfc_context->bit_rate_control_context[0].target_frame_size = intra_mb_size * width_in_mbs * height_in_mbs;
    mfc_context->bit_rate_control_context[1].target_mb_size = inter_mb_size;
    mfc_context->bit_rate_control_context[1].target_frame_size = inter_mb_size * width_in_mbs * height_in_mbs;

    for(i = 0 ; i < 2; i++) {
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
    
    mfc_context->bit_rate_control_context[0].TargetSizeInWord = (intra_mb_size + 16)/ 16;
    mfc_context->bit_rate_control_context[1].TargetSizeInWord = (inter_mb_size + 16)/ 16;

    mfc_context->bit_rate_control_context[0].MaxSizeInWord = mfc_context->bit_rate_control_context[0].TargetSizeInWord * 1.5;
    mfc_context->bit_rate_control_context[1].MaxSizeInWord = mfc_context->bit_rate_control_context[1].TargetSizeInWord * 1.5;
}

static int
gen7_mfc_bit_rate_control_context_update(struct encode_state *encode_state, 
                                         struct gen7_mfc_context *mfc_context,
                                         int current_frame_size) 
{
    VAEncSliceParameterBufferH264 *pSliceParameter = (VAEncSliceParameterBufferH264 *)encode_state->slice_params_ext[0]->buffer; 
    int control_index = 1 - (pSliceParameter->slice_type == SLICE_TYPE_I);
    int oldQp = mfc_context->bit_rate_control_context[control_index].QpPrimeY;

    if ( current_frame_size > mfc_context->bit_rate_control_context[control_index].target_frame_size * 4.0 ) {
        mfc_context->bit_rate_control_context[control_index].QpPrimeY += 4;
    } else if ( current_frame_size > mfc_context->bit_rate_control_context[control_index].target_frame_size * 2.0 ) {
        mfc_context->bit_rate_control_context[control_index].QpPrimeY += 3;
    } else if ( current_frame_size > mfc_context->bit_rate_control_context[control_index].target_frame_size * 1.50 ) {
        mfc_context->bit_rate_control_context[control_index].QpPrimeY += 2;
    } else if ( current_frame_size > mfc_context->bit_rate_control_context[control_index].target_frame_size * 1.20 ) {
        mfc_context->bit_rate_control_context[control_index].QpPrimeY ++;
    } else if (current_frame_size < mfc_context->bit_rate_control_context[control_index].target_frame_size * 0.30 )  {
        mfc_context->bit_rate_control_context[control_index].QpPrimeY -= 3;
    } else if (current_frame_size < mfc_context->bit_rate_control_context[control_index].target_frame_size * 0.50 )  {
        mfc_context->bit_rate_control_context[control_index].QpPrimeY -= 2;
    } else if (current_frame_size < mfc_context->bit_rate_control_context[control_index].target_frame_size * 0.80 )  {
        mfc_context->bit_rate_control_context[control_index].QpPrimeY --;
    }
    
    if ( mfc_context->bit_rate_control_context[control_index].QpPrimeY > 51)
        mfc_context->bit_rate_control_context[control_index].QpPrimeY = 51;
    if ( mfc_context->bit_rate_control_context[control_index].QpPrimeY < 1)
        mfc_context->bit_rate_control_context[control_index].QpPrimeY = 1;
 
    if ( mfc_context->bit_rate_control_context[control_index].QpPrimeY != oldQp)
        return 0;

    return 1;
}

static VAStatus
gen7_mfc_avc_prepare(VADriverContextP ctx, 
                     struct encode_state *encode_state,
                     struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct gen7_mfc_context *mfc_context = encoder_context->mfc_context;
    struct object_surface *obj_surface;	
    struct object_buffer *obj_buffer;
    struct gen7_mfc_avc_surface_aux* gen7_avc_surface;
    dri_bo *bo;
    VAEncPictureParameterBufferH264 *pPicParameter = (VAEncPictureParameterBufferH264 *)encode_state->pic_param_ext->buffer;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    int i;

    /*Setup all the input&output object*/

    /* Setup current frame and current direct mv buffer*/
    obj_surface = SURFACE(pPicParameter->CurrPic.picture_id);
    assert(obj_surface);
    i965_check_alloc_surface_bo(ctx, obj_surface, 1, VA_FOURCC('N','V','1','2'), SUBSAMPLE_YUV420);

    if ( obj_surface->private_data == NULL) {
        gen7_avc_surface = calloc(sizeof(struct gen7_mfc_avc_surface_aux), 1);
        gen7_avc_surface->dmv_top = 
            dri_bo_alloc(i965->intel.bufmgr,
                         "Buffer",
                         68*8192, 
                         64);
        gen7_avc_surface->dmv_bottom = 
            dri_bo_alloc(i965->intel.bufmgr,
                         "Buffer",
                         68*8192, 
                         64);
        assert(gen7_avc_surface->dmv_top);
        assert(gen7_avc_surface->dmv_bottom);
        obj_surface->private_data = (void *)gen7_avc_surface;
        obj_surface->free_private_data = (void *)gen7_mfc_free_avc_surface; 
    }

    gen7_avc_surface = (struct gen7_mfc_avc_surface_aux*) obj_surface->private_data;
    mfc_context->direct_mv_buffers[NUM_MFC_DMV_BUFFERS - 2].bo = gen7_avc_surface->dmv_top;
    mfc_context->direct_mv_buffers[NUM_MFC_DMV_BUFFERS - 1].bo = gen7_avc_surface->dmv_bottom;
    dri_bo_reference(gen7_avc_surface->dmv_top);
    dri_bo_reference(gen7_avc_surface->dmv_bottom);

    mfc_context->post_deblocking_output.bo = obj_surface->bo;
    dri_bo_reference(mfc_context->post_deblocking_output.bo);

    mfc_context->surface_state.width = obj_surface->orig_width;
    mfc_context->surface_state.height = obj_surface->orig_height;
    mfc_context->surface_state.w_pitch = obj_surface->width;
    mfc_context->surface_state.h_pitch = obj_surface->height;
    
    /* Setup reference frames and direct mv buffers*/
    for (i = 0; i < MAX_MFC_REFERENCE_SURFACES; i++) {
        if (pPicParameter->ReferenceFrames[i].picture_id != VA_INVALID_ID) { 
            obj_surface = SURFACE(pPicParameter->ReferenceFrames[i].picture_id);
            assert(obj_surface);
            if (obj_surface->bo != NULL) {
                mfc_context->reference_surfaces[i].bo = obj_surface->bo;
                dri_bo_reference(obj_surface->bo);
            }
            /* Check DMV buffer */
            if (obj_surface->private_data == NULL) {
                
                gen7_avc_surface = calloc(sizeof(struct gen7_mfc_avc_surface_aux), 1);
                gen7_avc_surface->dmv_top = 
                    dri_bo_alloc(i965->intel.bufmgr,
                                 "Buffer",
                                 68*8192, 
                                 64);
                gen7_avc_surface->dmv_bottom = 
                    dri_bo_alloc(i965->intel.bufmgr,
                                 "Buffer",
                                 68*8192, 
                                 64);
                assert(gen7_avc_surface->dmv_top);
                assert(gen7_avc_surface->dmv_bottom);
                obj_surface->private_data = gen7_avc_surface;
                obj_surface->free_private_data = gen7_mfc_free_avc_surface; 
            }
    
            gen7_avc_surface = (struct gen7_mfc_avc_surface_aux*) obj_surface->private_data;
            /* Setup DMV buffer */
            mfc_context->direct_mv_buffers[i*2].bo = gen7_avc_surface->dmv_top;
            mfc_context->direct_mv_buffers[i*2+1].bo = gen7_avc_surface->dmv_bottom; 
            dri_bo_reference(gen7_avc_surface->dmv_top);
            dri_bo_reference(gen7_avc_surface->dmv_bottom);
        } else {
            break;
        }
    }
	
    obj_surface = SURFACE(encoder_context->input_yuv_surface);
    assert(obj_surface && obj_surface->bo);
    mfc_context->uncompressed_picture_source.bo = obj_surface->bo;
    dri_bo_reference(mfc_context->uncompressed_picture_source.bo);

    obj_buffer = BUFFER (pPicParameter->coded_buf); /* FIXME: fix this later */
    bo = obj_buffer->buffer_store->bo;
    assert(bo);
    mfc_context->mfc_indirect_pak_bse_object.bo = bo;
    mfc_context->mfc_indirect_pak_bse_object.offset = ALIGN(sizeof(VACodedBufferSegment), 64);
    mfc_context->mfc_indirect_pak_bse_object.end_offset = ALIGN (obj_buffer->size_element - 0x1000, 0x1000);
    dri_bo_reference(mfc_context->mfc_indirect_pak_bse_object.bo);

    /*Programing bit rate control */
    if ( mfc_context->bit_rate_control_context[0].MaxSizeInWord == 0 )
        gen7_mfc_bit_rate_control_context_init(encode_state, mfc_context);

    /*Programing bcs pipeline*/
    gen7_mfc_avc_pipeline_programing(ctx, encode_state, encoder_context);	//filling the pipeline
	
    return vaStatus;
}

static VAStatus
gen7_mfc_run(VADriverContextP ctx, 
             struct encode_state *encode_state,
             struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;

    intel_batchbuffer_flush(batch);		//run the pipeline

    return VA_STATUS_SUCCESS;
}

static VAStatus
gen7_mfc_stop(VADriverContextP ctx, 
              struct encode_state *encode_state,
              struct intel_encoder_context *encoder_context,
              int *encoded_bits_size)
{
    struct gen7_mfc_context *mfc_context = encoder_context->mfc_context;
    unsigned int *status_mem;
    unsigned int buffer_size_bits = 0;
    int width_in_mbs = (mfc_context->surface_state.width + 15) / 16;
    int height_in_mbs = (mfc_context->surface_state.height + 15) / 16;
    int i;

    dri_bo_map(mfc_context->macroblock_status_buffer.bo, 1);
    status_mem = (unsigned int *)mfc_context->macroblock_status_buffer.bo->virtual;

    //Detecting encoder buffer size and bit rate control result
    for(i = 0; i < width_in_mbs * height_in_mbs; i++) {
        unsigned short current_mb = status_mem[1] >> 16;
        buffer_size_bits += current_mb;
        status_mem += 4;
    }    

    dri_bo_unmap(mfc_context->macroblock_status_buffer.bo);

    *encoded_bits_size = buffer_size_bits;

    return VA_STATUS_SUCCESS;
}

static VAStatus
gen7_mfc_avc_encode_picture(VADriverContextP ctx, 
                            struct encode_state *encode_state,
                            struct intel_encoder_context *encoder_context)
{
    VAEncSequenceParameterBufferH264 *pSequenceParameter = (VAEncSequenceParameterBufferH264 *)encode_state->seq_param_ext->buffer;
    struct gen7_mfc_context *mfc_context = encoder_context->mfc_context;
    unsigned int rate_control_mode = encoder_context->rate_control_mode;
    int MAX_CBR_INTERATE = 4;
    int current_frame_bits_size;
    int i;
 
    for(i = 0; i < MAX_CBR_INTERATE; i++) {
        gen7_mfc_init(ctx, encoder_context);
        gen7_mfc_avc_prepare(ctx, encode_state, encoder_context);
        gen7_mfc_run(ctx, encode_state, encoder_context);
        gen7_mfc_stop(ctx, encode_state, encoder_context, &current_frame_bits_size);

        if (rate_control_mode == VA_RC_CBR) {
            if (gen7_mfc_bit_rate_control_context_update( encode_state, mfc_context, current_frame_bits_size))
                break;
        } else {
            break;
        }
    }

    return VA_STATUS_SUCCESS;
}

static VAStatus
gen7_mfc_pipeline(VADriverContextP ctx,
                  VAProfile profile,
                  struct encode_state *encode_state,
                  struct intel_encoder_context *encoder_context)
{
    VAStatus vaStatus;

    switch (profile) {
    case VAProfileH264Baseline:
    case VAProfileH264Main:
    case VAProfileH264High:
        vaStatus = gen7_mfc_avc_encode_picture(ctx, encode_state, encoder_context);
        break;

        /* FIXME: add for other profile */
    default:
        vaStatus = VA_STATUS_ERROR_UNSUPPORTED_PROFILE;
        break;
    }

    return vaStatus;
}

static void
gen7_mfc_context_destroy(void *context)
{
    struct gen7_mfc_context *mfc_context = context;
    int i;

    dri_bo_unreference(mfc_context->post_deblocking_output.bo);
    mfc_context->post_deblocking_output.bo = NULL;

    dri_bo_unreference(mfc_context->pre_deblocking_output.bo);
    mfc_context->pre_deblocking_output.bo = NULL;

    dri_bo_unreference(mfc_context->uncompressed_picture_source.bo);
    mfc_context->uncompressed_picture_source.bo = NULL;

    dri_bo_unreference(mfc_context->mfc_indirect_pak_bse_object.bo); 
    mfc_context->mfc_indirect_pak_bse_object.bo = NULL;

    for (i = 0; i < NUM_MFC_DMV_BUFFERS; i++){
        dri_bo_unreference(mfc_context->direct_mv_buffers[i].bo);
        mfc_context->direct_mv_buffers[i].bo = NULL;
    }

    dri_bo_unreference(mfc_context->intra_row_store_scratch_buffer.bo);
    mfc_context->intra_row_store_scratch_buffer.bo = NULL;

    dri_bo_unreference(mfc_context->macroblock_status_buffer.bo);
    mfc_context->macroblock_status_buffer.bo = NULL;

    dri_bo_unreference(mfc_context->deblocking_filter_row_store_scratch_buffer.bo);
    mfc_context->deblocking_filter_row_store_scratch_buffer.bo = NULL;

    dri_bo_unreference(mfc_context->bsd_mpc_row_store_scratch_buffer.bo);
    mfc_context->bsd_mpc_row_store_scratch_buffer.bo = NULL;


    for (i = 0; i < MAX_MFC_REFERENCE_SURFACES; i++){
        dri_bo_unreference(mfc_context->reference_surfaces[i].bo);
        mfc_context->reference_surfaces[i].bo = NULL;  
    }

    free(mfc_context);
}

Bool
gen7_mfc_context_init(VADriverContextP ctx, struct intel_encoder_context *encoder_context)
{
    encoder_context->mfc_context = calloc(1, sizeof(struct gen7_mfc_context));
    encoder_context->mfc_context_destroy = gen7_mfc_context_destroy;
    encoder_context->mfc_pipeline = gen7_mfc_pipeline;

    return True;
}
