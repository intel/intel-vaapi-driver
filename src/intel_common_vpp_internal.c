/*
 * Copyright © 2016 Intel Corporation
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "intel_batchbuffer.h"
#include "intel_driver.h"
#include "i965_drv_video.h"
#include "i965_post_processing.h"
#include "gen75_picture_process.h"

#include "intel_gen_vppapi.h"
#include "intel_common_vpp_internal.h"

int
intel_vpp_support_yuv420p8_scaling(struct intel_video_process_context *proc_ctx)
{
    struct i965_proc_context *gpe_proc_ctx;

    if (!proc_ctx || !proc_ctx->vpp_fmt_cvt_ctx)
        return 0;

    gpe_proc_ctx = (struct i965_proc_context *)proc_ctx->vpp_fmt_cvt_ctx;

    if (gpe_proc_ctx->pp_context.scaling_8bit_initialized & VPPGPE_8BIT_420)
        return 1;
    else
        return 0;
}

VAStatus
intel_yuv420p8_scaling_post_processing(
    VADriverContextP   ctx,
    struct i965_post_processing_context *pp_context,
    struct i965_surface *src_surface,
    VARectangle *src_rect,
    struct i965_surface *dst_surface,
    VARectangle *dst_rect)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    VAStatus va_status;

    if (IS_GEN8(i965->intel.device_info))
        va_status = gen8_yuv420p8_scaling_post_processing(ctx, pp_context,
                                                          src_surface,
                                                          src_rect,
                                                          dst_surface,
                                                          dst_rect);
    else
        va_status = gen9_yuv420p8_scaling_post_processing(ctx, pp_context,
                                                          src_surface,
                                                          src_rect,
                                                          dst_surface,
                                                          dst_rect);

    return va_status;
}
