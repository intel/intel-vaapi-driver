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

#include "intel_batchbuffer.h"
#include "intel_driver.h"

#include "i965_defines.h"
#include "i965_drv_video.h"
#include "i965_encoder.h"
#include "gen6_vme.h"

#define SURFACE_STATE_PADDED_SIZE_0_GEN7        ALIGN(sizeof(struct gen7_surface_state), 32)
#define SURFACE_STATE_PADDED_SIZE_1_GEN7        ALIGN(sizeof(struct gen7_surface_state2), 32)
#define SURFACE_STATE_PADDED_SIZE_GEN7          MAX(SURFACE_STATE_PADDED_SIZE_0_GEN7, SURFACE_STATE_PADDED_SIZE_1_GEN7)

#define SURFACE_STATE_PADDED_SIZE_0_GEN6        ALIGN(sizeof(struct i965_surface_state), 32)
#define SURFACE_STATE_PADDED_SIZE_1_GEN6        ALIGN(sizeof(struct i965_surface_state2), 32)
#define SURFACE_STATE_PADDED_SIZE_GEN6          MAX(SURFACE_STATE_PADDED_SIZE_0_GEN6, SURFACE_STATE_PADDED_SIZE_1_GEN6)

#define SURFACE_STATE_PADDED_SIZE               MAX(SURFACE_STATE_PADDED_SIZE_GEN6, SURFACE_STATE_PADDED_SIZE_GEN7)
#define SURFACE_STATE_OFFSET(index)             (SURFACE_STATE_PADDED_SIZE * index)
#define BINDING_TABLE_OFFSET(index)             (SURFACE_STATE_OFFSET(MAX_MEDIA_SURFACES_GEN6) + sizeof(unsigned int) * index)

#define VME_INTRA_SHADER        0	
#define VME_INTER_SHADER        1
#define VME_BATCHBUFFER         2

#define CURBE_ALLOCATION_SIZE   37              /* in 256-bit */
#define CURBE_TOTAL_DATA_LENGTH (4 * 32)        /* in byte, it should be less than or equal to CURBE_ALLOCATION_SIZE * 32 */
#define CURBE_URB_ENTRY_LENGTH  4               /* in 256-bit, it should be less than or equal to CURBE_TOTAL_DATA_LENGTH / 32 */
  
static const uint32_t gen6_vme_intra_frame[][4] = {
#include "shaders/vme/intra_frame.g6b"
};

static const uint32_t gen6_vme_inter_frame[][4] = {
#include "shaders/vme/inter_frame.g6b"
};

static const uint32_t gen6_vme_batchbuffer[][4] = {
#include "shaders/vme/batchbuffer.g6b"
};

static struct i965_kernel gen6_vme_kernels[] = {
    {
        "VME Intra Frame",
        VME_INTRA_SHADER,										/*index*/
        gen6_vme_intra_frame, 			
        sizeof(gen6_vme_intra_frame),		
        NULL
    },
    {
        "VME inter Frame",
        VME_INTER_SHADER,
        gen6_vme_inter_frame,
        sizeof(gen6_vme_inter_frame),
        NULL
    },
    {
        "VME BATCHBUFFER",
        VME_BATCHBUFFER,
        gen6_vme_batchbuffer,
        sizeof(gen6_vme_batchbuffer),
        NULL
    },
};

static const uint32_t gen7_vme_intra_frame[][4] = {
#include "shaders/vme/intra_frame.g7b"
};

static const uint32_t gen7_vme_inter_frame[][4] = {
#include "shaders/vme/inter_frame.g7b"
};

static const uint32_t gen7_vme_batchbuffer[][4] = {
#include "shaders/vme/batchbuffer.g7b"
};

static struct i965_kernel gen7_vme_kernels[] = {
    {
        "VME Intra Frame",
        VME_INTRA_SHADER,										/*index*/
        gen7_vme_intra_frame, 			
        sizeof(gen7_vme_intra_frame),		
        NULL
    },
    {
        "VME inter Frame",
        VME_INTER_SHADER,
        gen7_vme_inter_frame,
        sizeof(gen7_vme_inter_frame),
        NULL
    },
    {
        "VME BATCHBUFFER",
        VME_BATCHBUFFER,
        gen7_vme_batchbuffer,
        sizeof(gen7_vme_batchbuffer),
        NULL
    },
};

/* only used for VME source surface state */
static void 
gen6_vme_source_surface_state(VADriverContextP ctx,
                              int index,
                              struct object_surface *obj_surface,
                              struct intel_encoder_context *encoder_context)
{
    struct gen6_vme_context *vme_context = encoder_context->vme_context;

    vme_context->vme_surface2_setup(ctx,
                                    &vme_context->gpe_context,
                                    obj_surface,
                                    BINDING_TABLE_OFFSET(index),
                                    SURFACE_STATE_OFFSET(index));
}

static void
gen6_vme_media_source_surface_state(VADriverContextP ctx,
                                    int index,
                                    struct object_surface *obj_surface,
                                    struct intel_encoder_context *encoder_context)
{
    struct gen6_vme_context *vme_context = encoder_context->vme_context;

    vme_context->vme_media_rw_surface_setup(ctx,
                                            &vme_context->gpe_context,
                                            obj_surface,
                                            BINDING_TABLE_OFFSET(index),
                                            SURFACE_STATE_OFFSET(index));
}

static void
gen6_vme_output_buffer_setup(VADriverContextP ctx,
                             struct encode_state *encode_state,
                             int index,
                             struct intel_encoder_context *encoder_context)

{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct gen6_vme_context *vme_context = encoder_context->vme_context;
    VAEncSequenceParameterBufferH264 *pSequenceParameter = (VAEncSequenceParameterBufferH264 *)encode_state->seq_param_ext->buffer;
    VAEncSliceParameterBufferH264 *pSliceParameter = (VAEncSliceParameterBufferH264 *)encode_state->slice_params_ext[0]->buffer;
    int is_intra = pSliceParameter->slice_type == SLICE_TYPE_I;
    int width_in_mbs = pSequenceParameter->picture_width_in_mbs;
    int height_in_mbs = pSequenceParameter->picture_height_in_mbs;

    vme_context->vme_output.num_blocks = width_in_mbs * height_in_mbs;
    vme_context->vme_output.pitch = 16; /* in bytes, always 16 */

    if (is_intra)
        vme_context->vme_output.size_block = 16; /* in bytes */
    else
        vme_context->vme_output.size_block = 64; /* in bytes */

    vme_context->vme_output.bo = dri_bo_alloc(i965->intel.bufmgr, 
                                              "VME output buffer",
                                              vme_context->vme_output.num_blocks * vme_context->vme_output.size_block,
                                              0x1000);
    assert(vme_context->vme_output.bo);
    vme_context->vme_buffer_suface_setup(ctx,
                                         &vme_context->gpe_context,
                                         &vme_context->vme_output,
                                         BINDING_TABLE_OFFSET(index),
                                         SURFACE_STATE_OFFSET(index));
}

static void
gen6_vme_output_vme_batchbuffer_setup(VADriverContextP ctx,
                                      struct encode_state *encode_state,
                                      int index,
                                      struct intel_encoder_context *encoder_context)

{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct gen6_vme_context *vme_context = encoder_context->vme_context;
    VAEncSequenceParameterBufferH264 *pSequenceParameter = (VAEncSequenceParameterBufferH264 *)encode_state->seq_param_ext->buffer;
    int width_in_mbs = pSequenceParameter->picture_width_in_mbs;
    int height_in_mbs = pSequenceParameter->picture_height_in_mbs;

    vme_context->vme_batchbuffer.num_blocks = width_in_mbs * height_in_mbs + 1;
    vme_context->vme_batchbuffer.size_block = 32; /* 2 OWORDs */
    vme_context->vme_batchbuffer.pitch = 16;
    vme_context->vme_batchbuffer.bo = dri_bo_alloc(i965->intel.bufmgr, 
                                                   "VME batchbuffer",
                                                   vme_context->vme_batchbuffer.num_blocks * vme_context->vme_batchbuffer.size_block,
                                                   0x1000);
    vme_context->vme_buffer_suface_setup(ctx,
                                         &vme_context->gpe_context,
                                         &vme_context->vme_batchbuffer,
                                         BINDING_TABLE_OFFSET(index),
                                         SURFACE_STATE_OFFSET(index));
}

static VAStatus
gen6_vme_surface_setup(VADriverContextP ctx, 
                       struct encode_state *encode_state,
                       int is_intra,
                       struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct object_surface *obj_surface;
    VAEncPictureParameterBufferH264 *pPicParameter = (VAEncPictureParameterBufferH264 *)encode_state->pic_param_ext->buffer;

    /*Setup surfaces state*/
    /* current picture for encoding */
    obj_surface = SURFACE(encoder_context->input_yuv_surface);
    assert(obj_surface);
    gen6_vme_source_surface_state(ctx, 0, obj_surface, encoder_context);
    gen6_vme_media_source_surface_state(ctx, 4, obj_surface, encoder_context);

    if (!is_intra) {
        /* reference 0 */
        obj_surface = SURFACE(pPicParameter->ReferenceFrames[0].picture_id);
        assert(obj_surface);
        if ( obj_surface->bo != NULL)
            gen6_vme_source_surface_state(ctx, 1, obj_surface, encoder_context);

        /* reference 1 */
        obj_surface = SURFACE(pPicParameter->ReferenceFrames[1].picture_id);
        assert(obj_surface);
        if ( obj_surface->bo != NULL ) 
            gen6_vme_source_surface_state(ctx, 2, obj_surface, encoder_context);
    }

    /* VME output */
    gen6_vme_output_buffer_setup(ctx, encode_state, 3, encoder_context);
    gen6_vme_output_vme_batchbuffer_setup(ctx, encode_state, 5, encoder_context);

    return VA_STATUS_SUCCESS;
}

static VAStatus gen6_vme_interface_setup(VADriverContextP ctx, 
                                         struct encode_state *encode_state,
                                         struct intel_encoder_context *encoder_context)
{
    struct gen6_vme_context *vme_context = encoder_context->vme_context;
    struct gen6_interface_descriptor_data *desc;   
    int i;
    dri_bo *bo;

    bo = vme_context->gpe_context.idrt.bo;
    dri_bo_map(bo, 1);
    assert(bo->virtual);
    desc = bo->virtual;

    for (i = 0; i < GEN6_VME_KERNEL_NUMBER; i++) {
        struct i965_kernel *kernel;
        kernel = &vme_context->gpe_context.kernels[i];
        assert(sizeof(*desc) == 32);
        /*Setup the descritor table*/
        memset(desc, 0, sizeof(*desc));
        desc->desc0.kernel_start_pointer = (kernel->bo->offset >> 6);
        desc->desc2.sampler_count = 1; /* FIXME: */
        desc->desc2.sampler_state_pointer = (vme_context->vme_state.bo->offset >> 5);
        desc->desc3.binding_table_entry_count = 1; /* FIXME: */
        desc->desc3.binding_table_pointer = (BINDING_TABLE_OFFSET(0) >> 5);
        desc->desc4.constant_urb_entry_read_offset = 0;
        desc->desc4.constant_urb_entry_read_length = CURBE_URB_ENTRY_LENGTH;
 		
        /*kernel start*/
        dri_bo_emit_reloc(bo,	
                          I915_GEM_DOMAIN_INSTRUCTION, 0,
                          0,
                          i * sizeof(*desc) + offsetof(struct gen6_interface_descriptor_data, desc0),
                          kernel->bo);
        /*Sampler State(VME state pointer)*/
        dri_bo_emit_reloc(bo,
                          I915_GEM_DOMAIN_INSTRUCTION, 0,
                          (1 << 2),									//
                          i * sizeof(*desc) + offsetof(struct gen6_interface_descriptor_data, desc2),
                          vme_context->vme_state.bo);
        desc++;
    }
    dri_bo_unmap(bo);

    return VA_STATUS_SUCCESS;
}

static VAStatus gen6_vme_constant_setup(VADriverContextP ctx, 
                                        struct encode_state *encode_state,
                                        struct intel_encoder_context *encoder_context)
{
    struct gen6_vme_context *vme_context = encoder_context->vme_context;
    // unsigned char *constant_buffer;

    dri_bo_map(vme_context->gpe_context.curbe.bo, 1);
    assert(vme_context->gpe_context.curbe.bo->virtual);
    // constant_buffer = vme_context->curbe.bo->virtual;
	
    /*TODO copy buffer into CURB*/

    dri_bo_unmap( vme_context->gpe_context.curbe.bo);

    return VA_STATUS_SUCCESS;
}

static VAStatus gen6_vme_vme_state_setup(VADriverContextP ctx,
                                         struct encode_state *encode_state,
                                         int is_intra,
                                         struct intel_encoder_context *encoder_context)
{
    struct gen6_vme_context *vme_context = encoder_context->vme_context;
    unsigned int *vme_state_message;
    int i;
	
    //building VME state message
    dri_bo_map(vme_context->vme_state.bo, 1);
    assert(vme_context->vme_state.bo->virtual);
    vme_state_message = (unsigned int *)vme_context->vme_state.bo->virtual;
	
	vme_state_message[0] = 0x01010101;
	vme_state_message[1] = 0x10010101;
	vme_state_message[2] = 0x0F0F0F0F;
	vme_state_message[3] = 0x100F0F0F;
	vme_state_message[4] = 0x01010101;
	vme_state_message[5] = 0x00010101;
	vme_state_message[5] = 0x01010101;
	vme_state_message[7] = 0x10010101;
	vme_state_message[8] = 0x0F0F0F0F;
	vme_state_message[9] = 0x100F0F0F;
	vme_state_message[10] = 0x01010101;
	vme_state_message[11] = 0x00010101;
        vme_state_message[12] = 0x00;
        vme_state_message[13] = 0x00;

        vme_state_message[14] = 0x4a4a;
        vme_state_message[15] = 0x0;
        vme_state_message[16] = 0x4a4a4a4a;
        vme_state_message[17] = 0x4a4a4a4a;

        for(i = 18; i < 32; i++) {
            vme_state_message[i] = 0;
        }
    //vme_state_message[16] = 0x42424242;			//cost function LUT set 0 for Intra

    dri_bo_unmap( vme_context->vme_state.bo);
    return VA_STATUS_SUCCESS;
}

static void
gen6_vme_fill_vme_batchbuffer(VADriverContextP ctx, 
                              struct encode_state *encode_state,
                              int mb_width, int mb_height,
                              int kernel,
                              int transform_8x8_mode_flag,
                              struct intel_encoder_context *encoder_context)
{
    struct gen6_vme_context *vme_context = encoder_context->vme_context;
    int total_mbs = mb_width * mb_height;
    int number_mb_cmds = 128;
    int mb_x = 0, mb_y = 0;
    int i, count = 0;
    unsigned int *command_ptr;

    dri_bo_map(vme_context->vme_batchbuffer.bo, 1);
    command_ptr = vme_context->vme_batchbuffer.bo->virtual;

    for (i = 0; i < total_mbs / number_mb_cmds; i++) {
        mb_x = count % mb_width;
        mb_y = count / mb_width;

        *command_ptr++ = (CMD_MEDIA_OBJECT | (8 - 2));
        *command_ptr++ = kernel;
        *command_ptr++ = 0;
        *command_ptr++ = 0;
        *command_ptr++ = 0;
        *command_ptr++ = 0;
   
        /*inline data */
        *command_ptr++ = (mb_width << 16 | mb_y << 8 | mb_x);
        *command_ptr++ = (number_mb_cmds << 16 | transform_8x8_mode_flag);

        count += number_mb_cmds;
    }

    number_mb_cmds = total_mbs - count;
    
    if (number_mb_cmds) {
        mb_x = count % mb_width;
        mb_y = count / mb_width;

        *command_ptr++ = (CMD_MEDIA_OBJECT | (8 - 2));
        *command_ptr++ = kernel;
        *command_ptr++ = 0;
        *command_ptr++ = 0;
        *command_ptr++ = 0;
        *command_ptr++ = 0;
   
        /*inline data */
        *command_ptr++ = (mb_width << 16 | mb_y << 8 | mb_x);
        *command_ptr++ = (number_mb_cmds << 16 | transform_8x8_mode_flag);
    }

    *command_ptr++ = 0;
    *command_ptr++ = MI_BATCH_BUFFER_END;

    dri_bo_unmap(vme_context->vme_batchbuffer.bo);
}

static void gen6_vme_media_init(VADriverContextP ctx, struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct gen6_vme_context *vme_context = encoder_context->vme_context;
    dri_bo *bo;

    i965_gpe_context_init(ctx, &vme_context->gpe_context);

    /* VME output buffer */
    dri_bo_unreference(vme_context->vme_output.bo);
    vme_context->vme_output.bo = NULL;

    dri_bo_unreference(vme_context->vme_batchbuffer.bo);
    vme_context->vme_batchbuffer.bo = NULL;

    /* VME state */
    dri_bo_unreference(vme_context->vme_state.bo);
    bo = dri_bo_alloc(i965->intel.bufmgr,
                      "Buffer",
                      1024*16, 64);
    assert(bo);
    vme_context->vme_state.bo = bo;
}

static void gen6_vme_pipeline_programing(VADriverContextP ctx, 
                                         struct encode_state *encode_state,
                                         struct intel_encoder_context *encoder_context)
{
    struct gen6_vme_context *vme_context = encoder_context->vme_context;
    struct intel_batchbuffer *batch = encoder_context->base.batch;
    VAEncPictureParameterBufferH264 *pPicParameter = (VAEncPictureParameterBufferH264 *)encode_state->pic_param_ext->buffer;
    VAEncSliceParameterBufferH264 *pSliceParameter = (VAEncSliceParameterBufferH264 *)encode_state->slice_params_ext[0]->buffer;
    VAEncSequenceParameterBufferH264 *pSequenceParameter = (VAEncSequenceParameterBufferH264 *)encode_state->seq_param_ext->buffer;
    int is_intra = pSliceParameter->slice_type == SLICE_TYPE_I;
    int width_in_mbs = pSequenceParameter->picture_width_in_mbs;
    int height_in_mbs = pSequenceParameter->picture_height_in_mbs;

    gen6_vme_fill_vme_batchbuffer(ctx, 
                                  encode_state,
                                  width_in_mbs, height_in_mbs,
                                  is_intra ? VME_INTRA_SHADER : VME_INTER_SHADER,
                                  pPicParameter->pic_fields.bits.transform_8x8_mode_flag,
                                  encoder_context);

    intel_batchbuffer_start_atomic(batch, 0x1000);
    gen6_gpe_pipeline_setup(ctx, &vme_context->gpe_context, batch);
    BEGIN_BATCH(batch, 2);
    OUT_BATCH(batch, MI_BATCH_BUFFER_START | (2 << 6));
    OUT_RELOC(batch,
              vme_context->vme_batchbuffer.bo,
              I915_GEM_DOMAIN_COMMAND, 0, 
              0);
    ADVANCE_BATCH(batch);

    intel_batchbuffer_end_atomic(batch);	
}

static VAStatus gen6_vme_prepare(VADriverContextP ctx, 
                                 struct encode_state *encode_state,
                                 struct intel_encoder_context *encoder_context)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    VAEncSliceParameterBufferH264 *pSliceParameter = (VAEncSliceParameterBufferH264 *)encode_state->slice_params_ext[0]->buffer;
    int is_intra = pSliceParameter->slice_type == SLICE_TYPE_I;
	
    /*Setup all the memory object*/
    gen6_vme_surface_setup(ctx, encode_state, is_intra, encoder_context);
    gen6_vme_interface_setup(ctx, encode_state, encoder_context);
    gen6_vme_constant_setup(ctx, encode_state, encoder_context);
    gen6_vme_vme_state_setup(ctx, encode_state, is_intra, encoder_context);

    /*Programing media pipeline*/
    gen6_vme_pipeline_programing(ctx, encode_state, encoder_context);

    return vaStatus;
}

static VAStatus gen6_vme_run(VADriverContextP ctx, 
                             struct encode_state *encode_state,
                             struct intel_encoder_context *encoder_context)
{
    struct intel_batchbuffer *batch = encoder_context->base.batch;

    intel_batchbuffer_flush(batch);

    return VA_STATUS_SUCCESS;
}

static VAStatus gen6_vme_stop(VADriverContextP ctx, 
                              struct encode_state *encode_state,
                              struct intel_encoder_context *encoder_context)
{
    return VA_STATUS_SUCCESS;
}

static VAStatus
gen6_vme_pipeline(VADriverContextP ctx,
                  VAProfile profile,
                  struct encode_state *encode_state,
                  struct intel_encoder_context *encoder_context)
{
    gen6_vme_media_init(ctx, encoder_context);
    gen6_vme_prepare(ctx, encode_state, encoder_context);
    gen6_vme_run(ctx, encode_state, encoder_context);
    gen6_vme_stop(ctx, encode_state, encoder_context);

    return VA_STATUS_SUCCESS;
}

static void
gen6_vme_context_destroy(void *context)
{
    struct gen6_vme_context *vme_context = context;

    i965_gpe_context_destroy(&vme_context->gpe_context);

    dri_bo_unreference(vme_context->vme_output.bo);
    vme_context->vme_output.bo = NULL;

    dri_bo_unreference(vme_context->vme_state.bo);
    vme_context->vme_state.bo = NULL;

    dri_bo_unreference(vme_context->vme_batchbuffer.bo);
    vme_context->vme_batchbuffer.bo = NULL;

    free(vme_context);
}

Bool gen6_vme_context_init(VADriverContextP ctx, struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct gen6_vme_context *vme_context = calloc(1, sizeof(struct gen6_vme_context));

    vme_context->gpe_context.surface_state_binding_table.length = (SURFACE_STATE_PADDED_SIZE + sizeof(unsigned int)) * MAX_MEDIA_SURFACES_GEN6;

    vme_context->gpe_context.idrt.max_entries = MAX_INTERFACE_DESC_GEN6;
    vme_context->gpe_context.idrt.entry_size = sizeof(struct gen6_interface_descriptor_data);

    vme_context->gpe_context.curbe.length = CURBE_TOTAL_DATA_LENGTH;

    vme_context->gpe_context.vfe_state.max_num_threads = 60 - 1;
    vme_context->gpe_context.vfe_state.num_urb_entries = 16;
    vme_context->gpe_context.vfe_state.gpgpu_mode = 0;
    vme_context->gpe_context.vfe_state.urb_entry_size = 59 - 1;
    vme_context->gpe_context.vfe_state.curbe_allocation_size = CURBE_ALLOCATION_SIZE - 1;

    if (IS_GEN7(i965->intel.device_id)) {
        i965_gpe_load_kernels(ctx,
                              &vme_context->gpe_context,
                              gen7_vme_kernels,
                              GEN6_VME_KERNEL_NUMBER);
        vme_context->vme_surface2_setup = gen7_gpe_surface2_setup;
        vme_context->vme_media_rw_surface_setup = gen7_gpe_media_rw_surface_setup;
        vme_context->vme_buffer_suface_setup = gen7_gpe_buffer_suface_setup;
    } else {
        i965_gpe_load_kernels(ctx,
                              &vme_context->gpe_context,
                              gen6_vme_kernels,
                              GEN6_VME_KERNEL_NUMBER);
        vme_context->vme_surface2_setup = i965_gpe_surface2_setup;
        vme_context->vme_media_rw_surface_setup = i965_gpe_media_rw_surface_setup;
        vme_context->vme_buffer_suface_setup = i965_gpe_buffer_suface_setup;
    }

    encoder_context->vme_context = vme_context;
    encoder_context->vme_context_destroy = gen6_vme_context_destroy;
    encoder_context->vme_pipeline = gen6_vme_pipeline;

    return True;
}
