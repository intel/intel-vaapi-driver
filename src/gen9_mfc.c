/*
 * Copyright © 2014 Intel Corporation
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
 *    Zhao Yakui <yakui.zhao@intel.com>
 *    Xiang Haihao <haihao.xiang@intel.com>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "intel_batchbuffer.h"
#include "i965_defines.h"
#include "i965_structs.h"
#include "i965_drv_video.h"
#include "i965_encoder.h"
#include "gen6_mfc.h"
#include "gen9_mfc.h"
#include "gen9_vdenc.h"
#include "gen9_vp9_encapi.h"
#include "i965_encoder_api.h"

extern Bool i965_encoder_vp8_pak_context_init(VADriverContextP ctx, struct intel_encoder_context *encoder_context);

Bool gen9_mfc_context_init(VADriverContextP ctx, struct intel_encoder_context *encoder_context)
{
    switch (encoder_context->codec) {
    case CODEC_VP8:
        return i965_encoder_vp8_pak_context_init(ctx, encoder_context);

    case CODEC_MPEG2:
    case CODEC_JPEG:
        return gen8_mfc_context_init(ctx, encoder_context);

    case CODEC_H264:
    case CODEC_H264_MVC:
        if (encoder_context->low_power_mode)
            return gen9_vdenc_context_init(ctx, encoder_context);
        else
            return gen9_avc_pak_context_init(ctx, encoder_context);

    case CODEC_HEVC:
        return gen9_hevc_pak_context_init(ctx, encoder_context);

    case CODEC_VP9:
        return gen9_vp9_pak_context_init(ctx, encoder_context);
    }

    /* Other profile/entrypoint pairs never get here, see gen9_enc_hw_context_init() */
    assert(0);
    return False;
}
