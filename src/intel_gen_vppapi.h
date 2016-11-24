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
 * Authors:
 *     Zhao Yakui <yakui.zhao@intel.com>
 *
 */

#ifndef _INTEL_GEN_VPPAPI_H_
#define _INTEL_GEN_VPPAPI_H_

#include <va/va.h>
#include <va/va_backend.h>

/*
struct i965_surface;
struct i965_post_processing_context;
*/

extern VAStatus
gen9_p010_scaling_post_processing(
    VADriverContextP   ctx,
    struct i965_post_processing_context *pp_context,
    struct i965_surface *src_surface,
    VARectangle *src_rect,
    struct i965_surface *dst_surface,
    VARectangle *dst_rect);

extern int
intel_vpp_support_yuv420p8_scaling(struct intel_video_process_context *proc_ctx);

extern VAStatus
intel_yuv420p8_scaling_post_processing(
    VADriverContextP   ctx,
    struct i965_post_processing_context *pp_context,
    struct i965_surface *src_surface,
    VARectangle *src_rect,
    struct i965_surface *dst_surface,
    VARectangle *dst_rect);

#endif  // _INTE_GEN_VPPAPI_H_
