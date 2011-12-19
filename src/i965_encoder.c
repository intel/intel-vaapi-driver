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

#include <va/va_backend.h>

#include "intel_batchbuffer.h"
#include "intel_driver.h"

#include "i965_defines.h"
#include "i965_drv_video.h"
#include "i965_encoder.h"

extern Bool gen6_mfc_context_init(VADriverContextP ctx, struct intel_encoder_context *encoder_context);
extern Bool gen6_vme_context_init(VADriverContextP ctx, struct intel_encoder_context *encoder_context);
extern Bool gen7_mfc_context_init(VADriverContextP ctx, struct intel_encoder_context *encoder_context);

static void 
intel_encoder_end_picture(VADriverContextP ctx, 
                          VAProfile profile, 
                          union codec_state *codec_state,
                          struct hw_context *hw_context)
{
    struct intel_encoder_context *encoder_context = (struct intel_encoder_context *)hw_context;
    struct encode_state *encode_state = &codec_state->encode;
    VAStatus vaStatus;

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
gen6_enc_hw_context_init(VADriverContextP ctx, VAProfile profile)
{
    struct intel_driver_data *intel = intel_driver_data(ctx);
    struct intel_encoder_context *encoder_context = calloc(1, sizeof(struct intel_encoder_context));

    encoder_context->base.destroy = intel_encoder_context_destroy;
    encoder_context->base.run = intel_encoder_end_picture;
    encoder_context->base.batch = intel_batchbuffer_new(intel, I915_EXEC_RENDER);

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
gen7_enc_hw_context_init(VADriverContextP ctx, VAProfile profile)
{
    struct intel_driver_data *intel = intel_driver_data(ctx);
    struct intel_encoder_context *encoder_context = calloc(1, sizeof(struct intel_encoder_context));

    encoder_context->base.destroy = intel_encoder_context_destroy;
    encoder_context->base.run = intel_encoder_end_picture;
    encoder_context->base.batch = intel_batchbuffer_new(intel, I915_EXEC_RENDER);

    gen6_vme_context_init(ctx, encoder_context);
    assert(encoder_context->vme_context);
    assert(encoder_context->vme_context_destroy);
    assert(encoder_context->vme_pipeline);

    gen7_mfc_context_init(ctx, encoder_context);
    assert(encoder_context->mfc_context);
    assert(encoder_context->mfc_context_destroy);
    assert(encoder_context->mfc_pipeline);

    return (struct hw_context *)encoder_context;
}
