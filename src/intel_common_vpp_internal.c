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

static VAStatus
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

static VAStatus
intel_10bit_8bit_scaling_post_processing(VADriverContextP   ctx,
                                         struct i965_post_processing_context *pp_context,
                                         struct i965_surface *src_surface,
                                         VARectangle *src_rect,
                                         struct i965_surface *dst_surface,
                                         VARectangle *dst_rect)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    VAStatus va_status = VA_STATUS_ERROR_UNIMPLEMENTED;

    if (IS_GEN9(i965->intel.device_info))
        va_status = gen9_10bit_8bit_scaling_post_processing(ctx, pp_context,
                                                            src_surface,
                                                            src_rect,
                                                            dst_surface,
                                                            dst_rect);

    return va_status;
}

VAStatus
intel_common_scaling_post_processing(VADriverContextP ctx,
                                     struct i965_post_processing_context *pp_context,
                                     const struct i965_surface *src_surface,
                                     const VARectangle *src_rect,
                                     struct i965_surface *dst_surface,
                                     const VARectangle *dst_rect)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    VAStatus status = VA_STATUS_ERROR_UNIMPLEMENTED;
    VARectangle aligned_dst_rect;
    int src_fourcc = pp_get_surface_fourcc(ctx, src_surface);
    int dst_fourcc = pp_get_surface_fourcc(ctx, dst_surface);
    unsigned int scale_flag = 0;
    unsigned int tmp_width, tmp_x;

    /* The Bit 2 is used to indicate that it is 10bit or 8bit.
     * The Bit 0/1 is used to indicate the 420/422/444 format
     */
#define SRC_8BIT_420     (1 << 0)
#define SRC_8BIT_422     (2 << 0)
#define SRC_8BIT_444     (3 << 0)
#define SRC_10BIT_420    (5 << 0)
#define SRC_10BIT_422    (6 << 0)
#define SRC_10BIT_444    (7 << 0)

    /* The Bit 6 is used to indicate that it is 10bit or 8bit.
     * The Bit 5/4 is used to indicate the 420/422/444 format
     */
#define DST_8BIT_420     (1 << 4)
#define DST_8BIT_422     (2 << 0)
#define DST_8BIT_444     (3 << 0)
#define DST_10BIT_420    (5 << 4)
#define DST_10BIT_422    (6 << 4)
#define DST_10BIT_444    (7 << 4)

#define SRC_YUV_PACKED   (1 << 3)
#define DST_YUV_PACKED   (1 << 7)

#define MASK_CSC         (0xFF)
#define SCALE_10BIT_10BIT_420   (SRC_10BIT_420 | DST_10BIT_420)
#define SCALE_8BIT_8BIT_420     (SRC_8BIT_420 | DST_8BIT_420)
#define SCALE_10BIT420_8BIT422  (SRC_10BIT_420 | DST_8BIT_422 | DST_YUV_PACKED)
#define SCALE_10BIT420_8BIT420  (SRC_10BIT_420 | DST_8BIT_420)

    if (src_fourcc == VA_FOURCC_P010 ||
        src_fourcc == VA_FOURCC_I010)
        scale_flag |= SRC_10BIT_420;

    if (src_fourcc == VA_FOURCC_NV12 ||
        src_fourcc == VA_FOURCC_I420)
        scale_flag |= SRC_8BIT_420;

    if (src_fourcc == VA_FOURCC_YUY2 ||
        src_fourcc == VA_FOURCC_UYVY)
        scale_flag |= (SRC_8BIT_422 | SRC_YUV_PACKED);

    if (dst_fourcc == VA_FOURCC_P010 ||
        dst_fourcc == VA_FOURCC_I010)
        scale_flag |= DST_10BIT_420;

    if (dst_fourcc == VA_FOURCC_NV12 ||
        dst_fourcc == VA_FOURCC_I420)
        scale_flag |= DST_8BIT_420;

    if (dst_fourcc == VA_FOURCC_YUY2 ||
        dst_fourcc == VA_FOURCC_UYVY)
        scale_flag |= (DST_8BIT_422 | DST_YUV_PACKED);

    if (dst_fourcc == VA_FOURCC_YUY2 ||
        dst_fourcc == VA_FOURCC_UYVY)
        scale_flag |= (DST_8BIT_422 | DST_YUV_PACKED);

    /* If P010 is converted without resolution change,
     * fall back to VEBOX
     */
    if (i965->intel.has_vebox &&
        (src_fourcc == VA_FOURCC_P010) &&
        (dst_fourcc == VA_FOURCC_P010 || dst_fourcc == VA_FOURCC_NV12) &&
        (src_rect->width == dst_rect->width) &&
        (src_rect->height == dst_rect->height))
        scale_flag = 0;

    if (((scale_flag & MASK_CSC) == SCALE_10BIT_10BIT_420) &&
        (pp_context->scaling_gpe_context_initialized & VPPGPE_10BIT_10BIT)) {
        unsigned int tmp_width, tmp_x;

        tmp_x = ALIGN_FLOOR(dst_rect->x, 2);
        tmp_width = dst_rect->x + dst_rect->width - tmp_x;
        aligned_dst_rect.x = tmp_x;
        aligned_dst_rect.width = tmp_width;
        aligned_dst_rect.y = dst_rect->y;
        aligned_dst_rect.height = dst_rect->height;

        status = gen9_p010_scaling_post_processing(ctx, pp_context,
                                                   (struct i965_surface *)src_surface, (VARectangle *)src_rect,
                                                   dst_surface, &aligned_dst_rect);
    }

    if (((scale_flag & MASK_CSC) == SCALE_8BIT_8BIT_420) &&
        (pp_context->scaling_gpe_context_initialized & VPPGPE_8BIT_8BIT)) {

        tmp_x = ALIGN_FLOOR(dst_rect->x, 4);
        tmp_width = dst_rect->x + dst_rect->width - tmp_x;
        aligned_dst_rect.x = tmp_x;
        aligned_dst_rect.width = tmp_width;
        aligned_dst_rect.y = dst_rect->y;
        aligned_dst_rect.height = dst_rect->height;

        status = intel_yuv420p8_scaling_post_processing(ctx, pp_context,
                                                        (struct i965_surface *)src_surface, (VARectangle *)src_rect,
                                                        dst_surface, &aligned_dst_rect);
    }

    if (((scale_flag & MASK_CSC) == SCALE_10BIT420_8BIT420 ||
         (scale_flag & MASK_CSC) == SCALE_10BIT420_8BIT422) &&
        (pp_context->scaling_gpe_context_initialized & VPPGPE_10BIT_8BIT)) {

        tmp_x = ALIGN_FLOOR(dst_rect->x, 4);
        tmp_width = dst_rect->x + dst_rect->width - tmp_x;
        aligned_dst_rect.x = tmp_x;
        aligned_dst_rect.width = tmp_width;
        aligned_dst_rect.y = dst_rect->y;
        aligned_dst_rect.height = dst_rect->height;

        status = intel_10bit_8bit_scaling_post_processing(ctx, pp_context,
                                                          (struct i965_surface *)src_surface, (VARectangle *)src_rect,
                                                          dst_surface, &aligned_dst_rect);
    }

    return status;
}
