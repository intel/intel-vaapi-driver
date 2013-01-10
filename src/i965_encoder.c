/*
 * Copyright © 2010 Intel Corporation
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
#include "gen6_mfc.h"

extern Bool gen6_mfc_context_init(VADriverContextP ctx, struct intel_encoder_context *encoder_context);
extern Bool gen6_vme_context_init(VADriverContextP ctx, struct intel_encoder_context *encoder_context);
extern Bool gen7_mfc_context_init(VADriverContextP ctx, struct intel_encoder_context *encoder_context);

VAStatus 
i965_DestroySurfaces(VADriverContextP ctx,
                     VASurfaceID *surface_list,
                     int num_surfaces);
VAStatus 
i965_CreateSurfaces(VADriverContextP ctx,
                    int width,
                    int height,
                    int format,
                    int num_surfaces,
                    VASurfaceID *surfaces);

static void
intel_encoder_check_yuv_surface(VADriverContextP ctx,
                                VAProfile profile,
                                struct encode_state *encode_state,
                                struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct i965_surface src_surface, dst_surface;
    struct object_surface *obj_surface;
    VAStatus status;
    VARectangle rect;

    /* releae the temporary surface */
    if (encoder_context->is_tmp_id) {
        i965_DestroySurfaces(ctx, &encoder_context->input_yuv_surface, 1);
    }

    encoder_context->is_tmp_id = 0;
    obj_surface = SURFACE(encode_state->current_render_target);
    assert(obj_surface && obj_surface->bo);

    if (obj_surface->fourcc == VA_FOURCC('N', 'V', '1', '2')) {
        unsigned int tiling = 0, swizzle = 0;

        dri_bo_get_tiling(obj_surface->bo, &tiling, &swizzle);

        if (tiling == I915_TILING_Y) {
            encoder_context->input_yuv_surface = encode_state->current_render_target;
            return;
        }
    }

    rect.x = 0;
    rect.y = 0;
    rect.width = obj_surface->orig_width;
    rect.height = obj_surface->orig_height;
    
    src_surface.id = encode_state->current_render_target;
    src_surface.type = I965_SURFACE_TYPE_SURFACE;
    src_surface.flags = I965_SURFACE_FLAG_FRAME;
    
    status = i965_CreateSurfaces(ctx,
                                 obj_surface->orig_width,
                                 obj_surface->orig_height,
                                 VA_RT_FORMAT_YUV420,
                                 1,
                                 &encoder_context->input_yuv_surface);
    assert(status == VA_STATUS_SUCCESS);
    obj_surface = SURFACE(encoder_context->input_yuv_surface);
    i965_check_alloc_surface_bo(ctx, obj_surface, 1, VA_FOURCC('N', 'V', '1', '2'), SUBSAMPLE_YUV420);
    
    dst_surface.id = encoder_context->input_yuv_surface;
    dst_surface.type = I965_SURFACE_TYPE_SURFACE;
    dst_surface.flags = I965_SURFACE_FLAG_FRAME;

    status = i965_image_processing(ctx,
                                   &src_surface,
                                   &rect,
                                   &dst_surface,
                                   &rect);
    assert(status == VA_STATUS_SUCCESS);

    encoder_context->is_tmp_id = 1;
}

static void 
intel_encoder_end_picture(VADriverContextP ctx, 
                          VAProfile profile, 
                          union codec_state *codec_state,
                          struct hw_context *hw_context)
{
    struct intel_encoder_context *encoder_context = (struct intel_encoder_context *)hw_context;
    struct encode_state *encode_state = &codec_state->encode;
    VAStatus vaStatus;

    intel_encoder_check_yuv_surface(ctx, profile, encode_state, encoder_context);

    encoder_context->mfc_brc_prepare(encode_state, encoder_context);

    vaStatus = encoder_context->vme_pipeline(ctx, profile, encode_state, encoder_context);

    if (vaStatus == VA_STATUS_SUCCESS)
        encoder_context->mfc_pipeline(ctx, profile, encode_state, encoder_context);
}

static void
intel_encoder_context_destroy(void *hw_context)
{
    struct intel_encoder_context *encoder_context = (struct intel_encoder_context *)hw_context;

    encoder_context->mfc_context_destroy(encoder_context->mfc_context);
    encoder_context->vme_context_destroy(encoder_context->vme_context);
    intel_batchbuffer_free(encoder_context->base.batch);
    free(encoder_context);
}

struct hw_context *
gen6_enc_hw_context_init(VADriverContextP ctx, struct object_config *obj_config)
{
    struct intel_driver_data *intel = intel_driver_data(ctx);
    struct intel_encoder_context *encoder_context = calloc(1, sizeof(struct intel_encoder_context));
    int i;

    encoder_context->base.destroy = intel_encoder_context_destroy;
    encoder_context->base.run = intel_encoder_end_picture;
    encoder_context->base.batch = intel_batchbuffer_new(intel, I915_EXEC_RENDER, 0);
    encoder_context->rate_control_mode = VA_RC_NONE;
    encoder_context->profile = obj_config->profile;

    for (i = 0; i < obj_config->num_attribs; i++) {
        if (obj_config->attrib_list[i].type == VAConfigAttribRateControl) {
            encoder_context->rate_control_mode = obj_config->attrib_list[i].value;
            break;
        }
    }

    gen6_vme_context_init(ctx, encoder_context);
    assert(encoder_context->vme_context);
    assert(encoder_context->vme_context_destroy);
    assert(encoder_context->vme_pipeline);

    gen6_mfc_context_init(ctx, encoder_context);
    assert(encoder_context->mfc_context);
    assert(encoder_context->mfc_context_destroy);
    assert(encoder_context->mfc_pipeline);

    return (struct hw_context *)encoder_context;
}

struct hw_context *
gen7_enc_hw_context_init(VADriverContextP ctx, struct object_config *obj_config)
{
    struct intel_driver_data *intel = intel_driver_data(ctx);
    struct intel_encoder_context *encoder_context = calloc(1, sizeof(struct intel_encoder_context));
    int i;

    encoder_context->base.destroy = intel_encoder_context_destroy;
    encoder_context->base.run = intel_encoder_end_picture;
    encoder_context->base.batch = intel_batchbuffer_new(intel, I915_EXEC_RENDER, 0);
    encoder_context->input_yuv_surface = VA_INVALID_SURFACE;
    encoder_context->is_tmp_id = 0;
    encoder_context->rate_control_mode = VA_RC_NONE;
    encoder_context->profile = obj_config->profile;

    for (i = 0; i < obj_config->num_attribs; i++) {
        if (obj_config->attrib_list[i].type == VAConfigAttribRateControl) {
            encoder_context->rate_control_mode = obj_config->attrib_list[i].value;
            break;
        }
    }

    gen7_vme_context_init(ctx, encoder_context);
    assert(encoder_context->vme_context);
    assert(encoder_context->vme_context_destroy);
    assert(encoder_context->vme_pipeline);

    gen7_mfc_context_init(ctx, encoder_context);
    assert(encoder_context->mfc_context);
    assert(encoder_context->mfc_context_destroy);
    assert(encoder_context->mfc_pipeline);

    return (struct hw_context *)encoder_context;
}

struct hw_context *
gen75_enc_hw_context_init(VADriverContextP ctx, struct object_config *obj_config)
{
    struct intel_driver_data *intel = intel_driver_data(ctx);
    struct intel_encoder_context *encoder_context = calloc(1, sizeof(struct intel_encoder_context));
    int i;

    encoder_context->base.destroy = intel_encoder_context_destroy;
    encoder_context->base.run = intel_encoder_end_picture;
    encoder_context->base.batch = intel_batchbuffer_new(intel, I915_EXEC_RENDER, 0);
    encoder_context->input_yuv_surface = VA_INVALID_SURFACE;
    encoder_context->is_tmp_id = 0;
    encoder_context->rate_control_mode = VA_RC_NONE;
    encoder_context->profile = obj_config->profile;

    for (i = 0; i < obj_config->num_attribs; i++) {
        if (obj_config->attrib_list[i].type == VAConfigAttribRateControl) {
            encoder_context->rate_control_mode = obj_config->attrib_list[i].value;
            break;
        }
    }

    gen75_vme_context_init(ctx, encoder_context);
    assert(encoder_context->vme_context);
    assert(encoder_context->vme_context_destroy);
    assert(encoder_context->vme_pipeline);

    gen75_mfc_context_init(ctx, encoder_context);
    assert(encoder_context->mfc_context);
    assert(encoder_context->mfc_context_destroy);
    assert(encoder_context->mfc_pipeline);

    return (struct hw_context *)encoder_context;
}
