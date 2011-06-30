/*
 * Copyright © 2010-2011 Intel Corporation
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
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "assert.h"
#include "intel_batchbuffer.h"
#include "i965_defines.h"
#include "i965_structs.h"
#include "i965_drv_video.h"
#include "i965_encoder.h"
#include "i965_encoder_utils.h"

static void
gen6_mfc_pipe_mode_select(VADriverContextP ctx, struct gen6_encoder_context *gen6_encoder_context)
{
    struct intel_batchbuffer *batch = gen6_encoder_context->base.batch;

    BEGIN_BCS_BATCH(batch, 4);

    OUT_BCS_BATCH(batch, MFX_PIPE_MODE_SELECT | (4 - 2));
    OUT_BCS_BATCH(batch,
                  (0 << 10) | /* disable Stream-Out , advanced QP/bitrate control need enable it*/
                  (1 << 9)  | /* Post Deblocking Output */
                  (0 << 8)  | /* Pre Deblocking Output */
                  (0 << 7)  | /* disable TLB prefectch */
                  (0 << 5)  | /* not in stitch mode */
                  (1 << 4)  | /* encoding mode */
                  (2 << 0));  /* Standard Select: AVC */
    OUT_BCS_BATCH(batch,
                  (0 << 20) | /* round flag in PB slice */
                  (0 << 19) | /* round flag in Intra8x8 */
                  (0 << 7)  | /* expand NOA bus flag */
                  (1 << 6)  | /* must be 1 */
                  (0 << 5)  | /* disable clock gating for NOA */
                  (0 << 4)  | /* terminate if AVC motion and POC table error occurs */
                  (0 << 3)  | /* terminate if AVC mbdata error occurs */
                  (0 << 2)  | /* terminate if AVC CABAC/CAVLC decode error occurs */
                  (0 << 1)  | /* AVC long field motion vector */
                  (0 << 0));  /* always calculate AVC ILDB boundary strength */
    OUT_BCS_BATCH(batch, 0);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen7_mfc_pipe_mode_select(VADriverContextP ctx,
                          int standard_select,
                          struct gen6_encoder_context *gen6_encoder_context)
{
    struct intel_batchbuffer *batch = gen6_encoder_context->base.batch;

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
gen6_mfc_surface_state(VADriverContextP ctx, struct gen6_encoder_context *gen6_encoder_context)
{
    struct intel_batchbuffer *batch = gen6_encoder_context->base.batch;
    struct gen6_mfc_context *mfc_context = &gen6_encoder_context->mfc_context;

    BEGIN_BCS_BATCH(batch, 6);

    OUT_BCS_BATCH(batch, MFX_SURFACE_STATE | (6 - 2));
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch,
                  ((mfc_context->surface_state.height - 1) << 19) |
                  ((mfc_context->surface_state.width - 1) << 6));
    OUT_BCS_BATCH(batch,
                  (MFX_SURFACE_PLANAR_420_8 << 28) | /* 420 planar YUV surface */
                  (1 << 27) | /* must be 1 for interleave U/V, hardware requirement */
                  (0 << 22) | /* surface object control state, FIXME??? */
                  ((mfc_context->surface_state.w_pitch - 1) << 3) | /* pitch */
                  (0 << 2)  | /* must be 0 for interleave U/V */
                  (1 << 1)  | /* must be y-tiled */
                  (I965_TILEWALK_YMAJOR << 0));  			/* tile walk, TILEWALK_YMAJOR */
    OUT_BCS_BATCH(batch,
                  (0 << 16) | 								/* must be 0 for interleave U/V */
                  (mfc_context->surface_state.h_pitch)); 		/* y offset for U(cb) */
    OUT_BCS_BATCH(batch, 0);
    ADVANCE_BCS_BATCH(batch);
}

static void
gen7_mfc_surface_state(VADriverContextP ctx, struct gen6_encoder_context *gen6_encoder_context)
{
    struct intel_batchbuffer *batch = gen6_encoder_context->base.batch;
    struct gen6_mfc_context *mfc_context = &gen6_encoder_context->mfc_context;

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
gen6_mfc_pipe_buf_addr_state(VADriverContextP ctx, struct gen6_encoder_context *gen6_encoder_context)
{
    struct intel_batchbuffer *batch = gen6_encoder_context->base.batch;
    struct gen6_mfc_context *mfc_context = &gen6_encoder_context->mfc_context;
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
gen6_mfc_ind_obj_base_addr_state(VADriverContextP ctx, struct gen6_encoder_context *gen6_encoder_context)
{
    struct intel_batchbuffer *batch = gen6_encoder_context->base.batch;
    struct gen6_mfc_context *mfc_context = &gen6_encoder_context->mfc_context;
    struct gen6_vme_context *vme_context = &gen6_encoder_context->vme_context;

    BEGIN_BCS_BATCH(batch, 11);

    OUT_BCS_BATCH(batch, MFX_IND_OBJ_BASE_ADDR_STATE | (11 - 2));
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);
    /* MFX Indirect MV Object Base Address */
    OUT_BCS_RELOC(batch, vme_context->vme_output.bo, I915_GEM_DOMAIN_INSTRUCTION, 0, 0);
    OUT_BCS_BATCH(batch, 0);	
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
gen7_mfc_ind_obj_base_addr_state(VADriverContextP ctx, struct gen6_encoder_context *gen6_encoder_context)
{
    struct intel_batchbuffer *batch = gen6_encoder_context->base.batch;
    struct gen6_mfc_context *mfc_context = &gen6_encoder_context->mfc_context;
    struct gen6_vme_context *vme_context = &gen6_encoder_context->vme_context;

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
gen6_mfc_bsp_buf_base_addr_state(VADriverContextP ctx, struct gen6_encoder_context *gen6_encoder_context)
{
    struct intel_batchbuffer *batch = gen6_encoder_context->base.batch;
    struct gen6_mfc_context *mfc_context = &gen6_encoder_context->mfc_context;

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
gen6_mfc_avc_img_state(VADriverContextP ctx,struct encode_state *encode_state,
                       struct gen6_encoder_context *gen6_encoder_context)
{
    struct intel_batchbuffer *batch = gen6_encoder_context->base.batch;
    struct gen6_mfc_context *mfc_context = &gen6_encoder_context->mfc_context;
    VAEncSequenceParameterBufferH264Ext *pSequenceParameter = (VAEncSequenceParameterBufferH264Ext *)encode_state->seq_param_ext->buffer;
    int width_in_mbs = (mfc_context->surface_state.width + 15) / 16;
    int height_in_mbs = (mfc_context->surface_state.height + 15) / 16;

    BEGIN_BCS_BATCH(batch, 13);
    OUT_BCS_BATCH(batch, MFX_AVC_IMG_STATE | (13 - 2));
    OUT_BCS_BATCH(batch, 
                  ((width_in_mbs * height_in_mbs) & 0xFFFF));
    OUT_BCS_BATCH(batch, 
                  (height_in_mbs << 16) | 
                  (width_in_mbs << 0));
    OUT_BCS_BATCH(batch, 
                  (0 << 24) |	  /*Second Chroma QP Offset*/
                  (0 << 16) |	  /*Chroma QP Offset*/
                  (0 << 14) |   /*Max-bit conformance Intra flag*/
                  (0 << 13) |   /*Max Macroblock size conformance Inter flag*/
                  (1 << 12) |   /*Should always be written as "1" */
                  (0 << 10) |   /*QM Preset FLag */
                  (0 << 8)  |   /*Image Structure*/
                  (0 << 0) );   /*Current Decoed Image Frame Store ID, reserved in Encode mode*/
    OUT_BCS_BATCH(batch,
                  (400 << 16) |   /*Mininum Frame size*/	
                  (0 << 15) |	/*Disable reading of Macroblock Status Buffer*/
                  (0 << 14) |   /*Load BitStream Pointer only once, 1 slic 1 frame*/
                  (0 << 13) |   /*CABAC 0 word insertion test enable*/
                  (1 << 12) |   /*MVUnpackedEnable,compliant to DXVA*/
                  (1 << 10) |   /*Chroma Format IDC, 4:2:0*/
                  (1 << 7)  |   /*0:CAVLC encoding mode,1:CABAC*/
                  (0 << 6)  |   /*Only valid for VLD decoding mode*/
                  (0 << 5)  |   /*Constrained Intra Predition Flag, from PPS*/
                  (pSequenceParameter->direct_8x8_inference_flag << 4)  |   /*Direct 8x8 inference flag*/
                  (0 << 3)  |   /*Only 8x8 IDCT Transform Mode Flag*/
                  (1 << 2)  |   /*Frame MB only flag*/
                  (0 << 1)  |   /*MBAFF mode is in active*/
                  (0 << 0) );   /*Field picture flag*/
    OUT_BCS_BATCH(batch, 
                  (1<<16)   |   /*Frame Size Rate Control Flag*/  
                  (1<<12)   |   
                  (1<<9)    |	/*MB level Rate Control Enabling Flag*/
                  (1 << 3)  |   /*FrameBitRateMinReportMask*/
                  (1 << 2)  |   /*FrameBitRateMaxReportMask*/
                  (1 << 1)  |   /*InterMBMaxSizeReportMask*/
                  (1 << 0) );   /*IntraMBMaxSizeReportMask*/
    OUT_BCS_BATCH(batch, 			/*Inter and Intra Conformance Max size limit*/
                  (0x0600 << 16) |		/*InterMbMaxSz 192 Byte*/
                  (0x0800) );			/*IntraMbMaxSz 256 Byte*/
    OUT_BCS_BATCH(batch, 0x00000000);   /*Reserved : MBZReserved*/
    OUT_BCS_BATCH(batch, 0x01020304);	/*Slice QP Delta for bitrate control*/   		
    OUT_BCS_BATCH(batch, 0xFEFDFCFB);   	
    OUT_BCS_BATCH(batch, 0x80601004);   /*MAX = 128KB, MIN = 64KB*/
    OUT_BCS_BATCH(batch, 0x00800001);   
    OUT_BCS_BATCH(batch, 0);

    ADVANCE_BCS_BATCH(batch);
}

static void
gen7_mfc_avc_img_state(VADriverContextP ctx, struct gen6_encoder_context *gen6_encoder_context)
{
    struct intel_batchbuffer *batch = gen6_encoder_context->base.batch;
    struct gen6_mfc_context *mfc_context = &gen6_encoder_context->mfc_context;

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
                  (1 << 7)  |   /* 0:CAVLC encoding mode,1:CABAC */
                  (0 << 6)  |   /* Only valid for VLD decoding mode */
                  (0 << 5)  |   /* Constrained Intra Predition Flag, from PPS */
                  (0 << 4)  |   /* Direct 8x8 inference flag */
                  (0 << 3)  |   /* Only 8x8 IDCT Transform Mode Flag */
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

static void gen6_mfc_avc_directmode_state(VADriverContextP ctx, struct gen6_encoder_context *gen6_encoder_context)
{
    struct intel_batchbuffer *batch = gen6_encoder_context->base.batch;
    struct gen6_mfc_context *mfc_context = &gen6_encoder_context->mfc_context;

    int i;

    BEGIN_BCS_BATCH(batch, 69);

    OUT_BCS_BATCH(batch, MFX_AVC_DIRECTMODE_STATE | (69 - 2));

    /* Reference frames and Current frames */
    for(i = 0; i < NUM_MFC_DMV_BUFFERS; i++) {
        if ( mfc_context->direct_mv_buffers[i].bo != NULL) { 
            OUT_BCS_RELOC(batch, mfc_context->direct_mv_buffers[i].bo,
                  I915_GEM_DOMAIN_INSTRUCTION, 0,
                  0);
         } else {
             OUT_BCS_BATCH(batch, 0);
         }
    }

    /* POL list */
    for(i = 0; i < 32; i++) {
        OUT_BCS_BATCH(batch, i/2);
    }
    OUT_BCS_BATCH(batch, 0);
    OUT_BCS_BATCH(batch, 0);

    ADVANCE_BCS_BATCH(batch);
}

static void gen6_mfc_avc_slice_state(VADriverContextP ctx,
                                     int slice_type,
                                     struct encode_state *encode_state,
                                     struct gen6_encoder_context *gen6_encoder_context,
                                     int rate_control_enable,
                                     int qp)
{
    struct intel_batchbuffer *batch = gen6_encoder_context->base.batch;
    struct gen6_mfc_context *mfc_context = &gen6_encoder_context->mfc_context;
    VAEncSliceParameterBufferH264Ext *pSliceParameter = (VAEncSliceParameterBufferH264Ext *)encode_state->slice_params_ext[0]->buffer; /* TODO: multi slices support */

    BEGIN_BCS_BATCH(batch, 11);;

    OUT_BCS_BATCH(batch, MFX_AVC_SLICE_STATE | (11 - 2) );

	OUT_BCS_BATCH(batch, slice_type);			/*Slice Type: I:P:B Slice*/

    if ( slice_type == SLICE_TYPE_I )
        OUT_BCS_BATCH(batch, 0);			/*no reference frames and pred_weight_table*/
    else 
        OUT_BCS_BATCH(batch, 0x00010000); 	/*1 reference frame*/

    OUT_BCS_BATCH(batch, 
                  (pSliceParameter->direct_spatial_mv_pred_flag<<29) |             /*Direct Prediction Type*/
                  (0<<24) |                /*Enable deblocking operation*/
                  (qp<<16) | 			/*Slice Quantization Parameter*/
                  0x0202 );
    OUT_BCS_BATCH(batch, 0);			/*First MB X&Y , the postion of current slice*/
    OUT_BCS_BATCH(batch, ( ((mfc_context->surface_state.height+15)/16) << 16) );

    OUT_BCS_BATCH(batch, 
                  (rate_control_enable<<31) |		/*in CBR mode RateControlCounterEnable = enable*/
                  (1<<30) |		/*ResetRateControlCounter*/
                  (0<<28) |		/*RC Triggle Mode = Always Rate Control*/
                  (8<<24) |     /*RC Stable Tolerance, middle level*/
                  (rate_control_enable<<23) |     /*RC Panic Enable*/                 
                  (0<<22) |     /*QP mode, don't modfiy CBP*/
                  (0<<21) |     /*MB Type Direct Conversion Disable*/ 
                  (0<<20) |     /*MB Type Skip Conversion Disable*/ 
                  (1<<19) |     /*IsLastSlice*/
                  (0<<18) | 	/*BitstreamOutputFlag Compressed BitStream Output Disable Flag 0:enable 1:disable*/
                  (1<<17) |	    /*HeaderPresentFlag*/	
                  (1<<16) |	    /*SliceData PresentFlag*/
                  (1<<15) |	    /*TailPresentFlag*/
                  (1<<13) |	    /*RBSP NAL TYPE*/	
                  (0<<12) );    /*CabacZeroWordInsertionEnable*/
	
    OUT_BCS_BATCH(batch, mfc_context->mfc_indirect_pak_bse_object.offset);

    OUT_BCS_BATCH(batch, (24<<24) |     /*Target QP - 24 is lowest QP*/ 
                         (20<<16) |     /*Target QP + 20 is highest QP*/
                         (8<<12)  |
                         (8<<8)   |
                         (8<<4)   |
                         (8<<0));
    OUT_BCS_BATCH(batch, 0x08888888);   
    OUT_BCS_BATCH(batch, 0);

    ADVANCE_BCS_BATCH(batch);
}
static void gen6_mfc_avc_qm_state(VADriverContextP ctx, struct gen6_encoder_context *gen6_encoder_context)
{
    struct intel_batchbuffer *batch = gen6_encoder_context->base.batch;
    int i;

    BEGIN_BCS_BATCH(batch, 58);

    OUT_BCS_BATCH(batch, MFX_AVC_QM_STATE | 56);
    OUT_BCS_BATCH(batch, 0xFF ) ; 
    for( i = 0; i < 56; i++) {
        OUT_BCS_BATCH(batch, 0x10101010); 
    }   

    ADVANCE_BCS_BATCH(batch);
}

static void gen6_mfc_avc_fqm_state(VADriverContextP ctx, struct gen6_encoder_context *gen6_encoder_context)
{
    struct intel_batchbuffer *batch = gen6_encoder_context->base.batch;
    int i;

    BEGIN_BCS_BATCH(batch, 113);
    OUT_BCS_BATCH(batch, MFC_AVC_FQM_STATE | (113 - 2));

    for(i = 0; i < 112;i++) {
        OUT_BCS_BATCH(batch, 0x10001000);
    }   

    ADVANCE_BCS_BATCH(batch);	
}

static void
gen7_mfc_qm_state(VADriverContextP ctx,
                  int qm_type,
                  unsigned int *qm,
                  int qm_length,
                  struct gen6_encoder_context *gen6_encoder_context)
{
    struct intel_batchbuffer *batch = gen6_encoder_context->base.batch;
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

static void gen7_mfc_avc_qm_state(VADriverContextP ctx, struct gen6_encoder_context *gen6_encoder_context)
{
    unsigned int qm[16] = {
        0x10101010, 0x10101010, 0x10101010, 0x10101010,
        0x10101010, 0x10101010, 0x10101010, 0x10101010,
        0x10101010, 0x10101010, 0x10101010, 0x10101010,
        0x10101010, 0x10101010, 0x10101010, 0x10101010
    };

    gen7_mfc_qm_state(ctx, MFX_QM_AVC_4X4_INTRA_MATRIX, qm, 12, gen6_encoder_context);
    gen7_mfc_qm_state(ctx, MFX_QM_AVC_4X4_INTER_MATRIX, qm, 12, gen6_encoder_context);
    gen7_mfc_qm_state(ctx, MFX_QM_AVC_8x8_INTRA_MATRIX, qm, 16, gen6_encoder_context);
    gen7_mfc_qm_state(ctx, MFX_QM_AVC_8x8_INTER_MATRIX, qm, 16, gen6_encoder_context);
}

static void
gen7_mfc_fqm_state(VADriverContextP ctx,
                   int fqm_type,
                   unsigned int *fqm,
                   int fqm_length,
                   struct gen6_encoder_context *gen6_encoder_context)
{
    struct intel_batchbuffer *batch = gen6_encoder_context->base.batch;
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

static void gen7_mfc_avc_fqm_state(VADriverContextP ctx, struct gen6_encoder_context *gen6_encoder_context)
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

    gen7_mfc_fqm_state(ctx, MFX_QM_AVC_4X4_INTRA_MATRIX, qm, 24, gen6_encoder_context);
    gen7_mfc_fqm_state(ctx, MFX_QM_AVC_4X4_INTER_MATRIX, qm, 24, gen6_encoder_context);
    gen7_mfc_fqm_state(ctx, MFX_QM_AVC_8x8_INTRA_MATRIX, qm, 32, gen6_encoder_context);
    gen7_mfc_fqm_state(ctx, MFX_QM_AVC_8x8_INTER_MATRIX, qm, 32, gen6_encoder_context);
}

static void gen6_mfc_avc_ref_idx_state(VADriverContextP ctx, struct gen6_encoder_context *gen6_encoder_context)
{
    struct intel_batchbuffer *batch = gen6_encoder_context->base.batch;
    int i;

	BEGIN_BCS_BATCH(batch, 10);
	OUT_BCS_BATCH(batch, MFX_AVC_REF_IDX_STATE | 8); 
	OUT_BCS_BATCH(batch, 0);                  //Select L0
	OUT_BCS_BATCH(batch, 0x80808020);         //Only 1 reference
	for(i = 0; i < 7; i++) {
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
gen6_mfc_avc_insert_object(VADriverContextP ctx, struct gen6_encoder_context *gen6_encoder_context,
                           unsigned int *insert_data, int lenght_in_dws, int data_bits_in_last_dw,
                           int skip_emul_byte_count, int is_last_header, int is_end_of_slice)
{
    struct intel_batchbuffer *batch = gen6_encoder_context->base.batch;

    BEGIN_BCS_BATCH(batch, lenght_in_dws + 2);

    OUT_BCS_BATCH(batch, MFC_AVC_INSERT_OBJECT | (lenght_in_dws + 2 - 2));
    OUT_BCS_BATCH(batch,
                  (0 << 16) |   /* always start at offset 0 */
                  (data_bits_in_last_dw << 8) |
                  (skip_emul_byte_count << 4) |
                  (1 << 3) |    /* FIXME: ??? */
                  ((!!is_last_header) << 2) |
                  ((!!is_end_of_slice) << 1) |
                  (0 << 0));    /* FIXME: ??? */

    intel_batchbuffer_data(batch, insert_data, lenght_in_dws * 4);
    ADVANCE_BCS_BATCH(batch);
}

static int
gen6_mfc_avc_pak_object_intra(VADriverContextP ctx, int x, int y, int end_mb, int qp,unsigned int *msg,
                              struct gen6_encoder_context *gen6_encoder_context,
                              int intra_mb_size_in_bits)
{
    struct intel_batchbuffer *batch = gen6_encoder_context->base.batch;
    int len_in_dwords = 11;
    unsigned char target_mb_size = intra_mb_size_in_bits / 16;     //In Words
    unsigned char max_mb_size = target_mb_size * 2 > 255? 255: target_mb_size * 2 ;

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

    OUT_BCS_BATCH(batch, (0xFFFF<<16) | (y << 8) | x);		/* Code Block Pattern for Y*/
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

static int gen6_mfc_avc_pak_object_inter(VADriverContextP ctx, int x, int y, int end_mb, int qp, unsigned int offset,
                                         struct gen6_encoder_context *gen6_encoder_context,
                                         int inter_mb_size_in_bits, int slice_type)
{
    struct intel_batchbuffer *batch = gen6_encoder_context->base.batch;
    int len_in_dwords = 11;
    unsigned char target_mb_size = inter_mb_size_in_bits / 16;     //In Words
    unsigned char max_mb_size = target_mb_size * 16 > 255? 255: target_mb_size * 16 ;

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
    if ( slice_type == SLICE_TYPE_B) {
        OUT_BCS_BATCH(batch, (0xF<<28) | (end_mb << 26) | qp);	/* Last MB */
    } else {
        OUT_BCS_BATCH(batch, (end_mb << 26) | qp);	/* Last MB */
    }


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

static void gen6_mfc_init(VADriverContextP ctx, struct gen6_encoder_context *gen6_encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct gen6_mfc_context *mfc_context = &gen6_encoder_context->mfc_context;
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
                      4*9600,
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

void gen6_mfc_avc_pipeline_programing(VADriverContextP ctx,
                                      struct encode_state *encode_state,
                                      struct gen6_encoder_context *gen6_encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct intel_batchbuffer *batch = gen6_encoder_context->base.batch;
    struct gen6_mfc_context *mfc_context = &gen6_encoder_context->mfc_context;
    struct gen6_vme_context *vme_context = &gen6_encoder_context->vme_context;
    VAEncSequenceParameterBufferH264Ext *pSequenceParameter = (VAEncSequenceParameterBufferH264Ext *)encode_state->seq_param_ext->buffer;
    VAEncPictureParameterBufferH264Ext *pPicParameter = (VAEncPictureParameterBufferH264Ext *)encode_state->pic_param_ext->buffer;
    VAEncSliceParameterBufferH264Ext *pSliceParameter = (VAEncSliceParameterBufferH264Ext *)encode_state->slice_params_ext[0]->buffer; /* FIXME: multi slices */
    VAEncH264DecRefPicMarkingBuffer *pDecRefPicMarking = NULL;
    unsigned int *msg = NULL, offset = 0;
    int emit_new_state = 1, object_len_in_bytes;
    int is_intra = pSliceParameter->slice_type == SLICE_TYPE_I;
    int width_in_mbs = (mfc_context->surface_state.width + 15) / 16;
    int height_in_mbs = (mfc_context->surface_state.height + 15) / 16;
    int x,y;
    int rate_control_mode = pSequenceParameter->rate_control_method; 
    float fps =  pSequenceParameter->time_scale * 0.5 / pSequenceParameter->num_units_in_tick ;
    int inter_mb_size = pSequenceParameter->bits_per_second * 1.0 / fps / width_in_mbs / height_in_mbs;
    int intra_mb_size = inter_mb_size * 5.0;
    int qp = pPicParameter->pic_init_qp;
    unsigned char *slice_header = NULL;
    int slice_header_length_in_bits = 0;
    unsigned int tail_data[] = { 0x0 };

    if (encode_state->dec_ref_pic_marking)
        pDecRefPicMarking = (VAEncH264DecRefPicMarkingBuffer *)encode_state->dec_ref_pic_marking->buffer;

    slice_header_length_in_bits = build_avc_slice_header(pSequenceParameter, pPicParameter, pSliceParameter, pDecRefPicMarking, &slice_header);

    if ( rate_control_mode != 2) {
        qp = 26;
        if ( intra_mb_size > 384*8)         //ONE MB raw data is 384 bytes
            intra_mb_size = 384*8;
        if ( inter_mb_size > 256*8)
            intra_mb_size = 256*8;
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
                
                if (IS_GEN7(i965->intel.device_id)) {
                    gen7_mfc_pipe_mode_select(ctx, MFX_FORMAT_AVC, gen6_encoder_context);
                    gen7_mfc_surface_state(ctx, gen6_encoder_context);
                    gen7_mfc_ind_obj_base_addr_state(ctx, gen6_encoder_context);
                } else {
                    gen6_mfc_pipe_mode_select(ctx, gen6_encoder_context);
                    gen6_mfc_surface_state(ctx, gen6_encoder_context);
                    gen6_mfc_ind_obj_base_addr_state(ctx, gen6_encoder_context);
                }

                gen6_mfc_pipe_buf_addr_state(ctx, gen6_encoder_context);
                gen6_mfc_bsp_buf_base_addr_state(ctx, gen6_encoder_context);

                if (IS_GEN7(i965->intel.device_id)) {
                    gen7_mfc_avc_img_state(ctx, gen6_encoder_context);
                    gen7_mfc_avc_qm_state(ctx, gen6_encoder_context);
                    gen7_mfc_avc_fqm_state(ctx, gen6_encoder_context);
                } else {
                    gen6_mfc_avc_img_state(ctx, encode_state,gen6_encoder_context);
                    gen6_mfc_avc_qm_state(ctx, gen6_encoder_context);
                    gen6_mfc_avc_fqm_state(ctx, gen6_encoder_context);
                }

                gen6_mfc_avc_directmode_state(ctx, gen6_encoder_context); 
                gen6_mfc_avc_ref_idx_state(ctx, gen6_encoder_context);
                gen6_mfc_avc_slice_state(ctx, pSliceParameter->slice_type, 
                                         encode_state, gen6_encoder_context, 
                                         rate_control_mode == 0, qp);
                gen6_mfc_avc_insert_object(ctx, gen6_encoder_context,
                                           (unsigned int *)slice_header, ALIGN(slice_header_length_in_bits, 32) >> 5, slice_header_length_in_bits & 0x1f,
                                           5, 1, 0); /* first 5 bytes are start code + nal unit type */
                emit_new_state = 0;
            }

            if (is_intra) {
                assert(msg);
                object_len_in_bytes = gen6_mfc_avc_pak_object_intra(ctx, x, y, last_mb, qp, msg, gen6_encoder_context, intra_mb_size);
                msg += 4;
            } else {
                object_len_in_bytes = gen6_mfc_avc_pak_object_inter(ctx, x, y, last_mb, qp, offset, gen6_encoder_context, inter_mb_size, pSliceParameter->slice_type);
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

    gen6_mfc_avc_insert_object(ctx, gen6_encoder_context,
                               tail_data, sizeof(tail_data) >> 2, 32,
                               sizeof(tail_data), 1, 1);

    if (is_intra)
        dri_bo_unmap(vme_context->vme_output.bo);

    free(slice_header);

    intel_batchbuffer_end_atomic(batch);
}

static void 
gen6_mfc_free_avc_surface(void **data)
{
    struct gen6_mfc_avc_surface_aux *avc_surface = *data;

    if (!avc_surface)
        return;

    dri_bo_unreference(avc_surface->dmv_top);
    avc_surface->dmv_top = NULL;
    dri_bo_unreference(avc_surface->dmv_bottom);
    avc_surface->dmv_bottom = NULL;

    free(avc_surface);
    *data = NULL;
}

static VAStatus gen6_mfc_avc_prepare(VADriverContextP ctx, 
                                     struct encode_state *encode_state,
                                     struct gen6_encoder_context *gen6_encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct gen6_mfc_context *mfc_context = &gen6_encoder_context->mfc_context;
    struct object_surface *obj_surface;	
    struct object_buffer *obj_buffer;
    struct gen6_mfc_avc_surface_aux* gen6_avc_surface;
    dri_bo *bo;
    VAEncPictureParameterBufferH264Ext *pPicParameter = (VAEncPictureParameterBufferH264Ext *)encode_state->pic_param_ext->buffer;
    VAStatus vaStatus = VA_STATUS_SUCCESS;
	int i;

    /*Setup all the input&output object*/

    /* Setup current frame and current direct mv buffer*/
    obj_surface = SURFACE(pPicParameter->CurrPic.picture_id);
    assert(obj_surface);
    i965_check_alloc_surface_bo(ctx, obj_surface, 1, VA_FOURCC('N','V','1','2'));
    if ( obj_surface->private_data == NULL) {
        gen6_avc_surface = calloc(sizeof(struct gen6_mfc_avc_surface_aux), 1);
        gen6_avc_surface->dmv_top = 
            dri_bo_alloc(i965->intel.bufmgr,
                    "Buffer",
                    68*8192, 
                    64);
        gen6_avc_surface->dmv_bottom = 
            dri_bo_alloc(i965->intel.bufmgr,
                            "Buffer",
                            68*8192, 
                            64);
        assert(gen6_avc_surface->dmv_top);
        assert(gen6_avc_surface->dmv_bottom);
        obj_surface->private_data = (void *)gen6_avc_surface;
        obj_surface->free_private_data = (void *)gen6_mfc_free_avc_surface; 
    }
    gen6_avc_surface = (struct gen6_mfc_avc_surface_aux*) obj_surface->private_data;
    mfc_context->direct_mv_buffers[NUM_MFC_DMV_BUFFERS - 2].bo = gen6_avc_surface->dmv_top;
    mfc_context->direct_mv_buffers[NUM_MFC_DMV_BUFFERS - 1].bo = gen6_avc_surface->dmv_bottom;
	dri_bo_reference(gen6_avc_surface->dmv_top);
	dri_bo_reference(gen6_avc_surface->dmv_bottom);

    mfc_context->post_deblocking_output.bo = obj_surface->bo;
    dri_bo_reference(mfc_context->post_deblocking_output.bo);

    mfc_context->surface_state.width = obj_surface->orig_width;
    mfc_context->surface_state.height = obj_surface->orig_height;
    mfc_context->surface_state.w_pitch = obj_surface->width;
    mfc_context->surface_state.h_pitch = obj_surface->height;
    
    /* Setup reference frames and direct mv buffers*/
    for(i = 0; i < MAX_MFC_REFERENCE_SURFACES; i++) {
		if ( pPicParameter->ReferenceFrames[i].picture_id != VA_INVALID_ID ) { 
			obj_surface = SURFACE(pPicParameter->ReferenceFrames[i].picture_id);
			assert(obj_surface);
			if (obj_surface->bo != NULL) {
				mfc_context->reference_surfaces[i].bo = obj_surface->bo;
				dri_bo_reference(obj_surface->bo);
			}
            /* Check DMV buffer */
            if ( obj_surface->private_data == NULL) {
                
                gen6_avc_surface = calloc(sizeof(struct gen6_mfc_avc_surface_aux), 1);
                gen6_avc_surface->dmv_top = 
                    dri_bo_alloc(i965->intel.bufmgr,
                            "Buffer",
                            68*8192, 
                            64);
                gen6_avc_surface->dmv_bottom = 
                    dri_bo_alloc(i965->intel.bufmgr,
                            "Buffer",
                            68*8192, 
                            64);
                assert(gen6_avc_surface->dmv_top);
                assert(gen6_avc_surface->dmv_bottom);
                obj_surface->private_data = gen6_avc_surface;
                obj_surface->free_private_data = gen6_mfc_free_avc_surface; 
            }
    
            gen6_avc_surface = (struct gen6_mfc_avc_surface_aux*) obj_surface->private_data;
            /* Setup DMV buffer */
            mfc_context->direct_mv_buffers[i*2].bo = gen6_avc_surface->dmv_top;
            mfc_context->direct_mv_buffers[i*2+1].bo = gen6_avc_surface->dmv_bottom; 
            dri_bo_reference(gen6_avc_surface->dmv_top);
            dri_bo_reference(gen6_avc_surface->dmv_bottom);
		} else {
			break;
		}
	}
	
    obj_surface = SURFACE(encode_state->current_render_target);
    assert(obj_surface && obj_surface->bo);
    mfc_context->uncompressed_picture_source.bo = obj_surface->bo;
    dri_bo_reference(mfc_context->uncompressed_picture_source.bo);

    obj_buffer = BUFFER (pPicParameter->CodedBuf); /* FIXME: fix this later */
    bo = obj_buffer->buffer_store->bo;
    assert(bo);
    mfc_context->mfc_indirect_pak_bse_object.bo = bo;
    mfc_context->mfc_indirect_pak_bse_object.offset = ALIGN(sizeof(VACodedBufferSegment), 64);
    mfc_context->mfc_indirect_pak_bse_object.end_offset = ALIGN (obj_buffer->size_element - 0x1000, 0x1000);
    dri_bo_reference(mfc_context->mfc_indirect_pak_bse_object.bo);

    /*Programing bcs pipeline*/
    gen6_mfc_avc_pipeline_programing(ctx, encode_state, gen6_encoder_context);	//filling the pipeline
	
    return vaStatus;
}

static VAStatus gen6_mfc_run(VADriverContextP ctx, 
                             struct encode_state *encode_state,
                             struct gen6_encoder_context *gen6_encoder_context)
{
    struct intel_batchbuffer *batch = gen6_encoder_context->base.batch;

    intel_batchbuffer_flush(batch);		//run the pipeline

    return VA_STATUS_SUCCESS;
}

static VAStatus gen6_mfc_stop(VADriverContextP ctx, 
                              struct encode_state *encode_state,
                              struct gen6_encoder_context *gen6_encoder_context)
{
#if 0
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct gen6_mfc_context *mfc_context = &gen6_encoder_context->mfc_context;
	
    VAEncPictureParameterBufferH264Ext *pPicParameter = (VAEncPictureParameterBufferH264Ext *)encode_state->pic_param_ext->buffer;
	
    struct object_surface *obj_surface = SURFACE(pPicParameter->reconstructed_picture);
    //struct object_surface *obj_surface = SURFACE(pPicParameter->reference_picture[0]);
    //struct object_surface *obj_surface = SURFACE(encode_state->current_render_target);
    my_debug(obj_surface);

#endif

    return VA_STATUS_SUCCESS;
}

static VAStatus
gen6_mfc_avc_encode_picture(VADriverContextP ctx, 
                            struct encode_state *encode_state,
                            struct gen6_encoder_context *gen6_encoder_context)
{
    gen6_mfc_init(ctx, gen6_encoder_context);
    gen6_mfc_avc_prepare(ctx, encode_state, gen6_encoder_context);
    gen6_mfc_run(ctx, encode_state, gen6_encoder_context);
    gen6_mfc_stop(ctx, encode_state, gen6_encoder_context);

    return VA_STATUS_SUCCESS;
}

VAStatus
gen6_mfc_pipeline(VADriverContextP ctx,
                  VAProfile profile,
                  struct encode_state *encode_state,
                  struct gen6_encoder_context *gen6_encoder_context)
{
    VAStatus vaStatus;

    switch (profile) {
    case VAProfileH264Baseline:
        vaStatus = gen6_mfc_avc_encode_picture(ctx, encode_state, gen6_encoder_context);
        break;

        /* FIXME: add for other profile */
    default:
        vaStatus = VA_STATUS_ERROR_UNSUPPORTED_PROFILE;
        break;
    }

    return vaStatus;
}

Bool gen6_mfc_context_init(VADriverContextP ctx, struct gen6_mfc_context *mfc_context)
{
    return True;
}

Bool gen6_mfc_context_destroy(struct gen6_mfc_context *mfc_context)
{
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

    return True;
}
