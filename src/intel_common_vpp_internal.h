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

#ifndef _INTEL_COMMON_VPP_INTERNAL_H_
#define _INTEL_COMMON_VPP_INTERNAL_H_

struct object_surface;

/* the below is defined for YUV420 format scaling */
#define SRC_MSB         0x0001
#define DST_MSB         0x0002
#define SRC_PACKED      0x0004
#define DST_PACKED      0x0008
#define PACKED_MASK     0x000C

#define BTI_SCALING_INPUT_Y     0
#define BTI_SCALING_OUTPUT_Y    8

#define SRC_FORMAT_I420         0
#define SRC_FORMAT_YV12         1
#define SRC_FORMAT_NV12         2
#define SRC_FORMAT_P010         3
#define SRC_FORMAT_I010         4
#define SRC_FORMAT_YUY2         5
#define SRC_FORMAT_UYVY         6
#define SRC_FORMAT_RGBA         7
#define SRC_FORMAT_RGBX         8
#define SRC_FORMAT_BGRA         9
#define SRC_FORMAT_BGRX         10

#define DST_FORMAT_I420         0
#define DST_FORMAT_YV12         1
#define DST_FORMAT_NV12         2
#define DST_FORMAT_P010         3
#define DST_FORMAT_I010         4
#define DST_FORMAT_YUY2         5
#define DST_FORMAT_UYVY         6
#define DST_FORMAT_RGBA         7
#define DST_FORMAT_RGBX         8
#define DST_FORMAT_BGRA         9
#define DST_FORMAT_BGRX         10

/*
 *  32 DWs or 4 GRFs
 */
struct scaling_input_parameter {
    float inv_width;
    float inv_height;

    struct {
        unsigned int src_msb : 1;
        unsigned int dst_msb : 1;
        unsigned int src_packed : 1;    /* packed UV */
        unsigned int dst_packed : 1;    /* packed UV */
        unsigned int reserved : 12;
        unsigned int src_format : 8;
        unsigned int dst_format : 8;
    } dw2;

    int x_dst;
    int y_dst;
    float    x_factor; // src_rect_width / dst_rect_width / Surface_width
    float    y_factor; // src_rect_height / dst_rect_height / Surface_height
    float    x_orig;
    float    y_orig;
    unsigned int bti_input;
    unsigned int bti_output;
    unsigned int reserved0;
    float coef_ry;
    float coef_ru;
    float coef_rv;
    float coef_yd;
    float coef_gy;
    float coef_gu;
    float coef_gv;
    float coef_ud;
    float coef_by;
    float coef_bu;
    float coef_bv;
    float coef_vd;
    unsigned int reserved[8];
};

/* 4 Registers or 32 DWs */
struct clear_input_parameter {
    unsigned int color;     /* ayvu */
    unsigned int reserved[31];
};

VAStatus
gen9_yuv420p8_scaling_post_processing(
    VADriverContextP   ctx,
    struct i965_post_processing_context *pp_context,
    struct i965_surface *src_surface,
    VARectangle *src_rect,
    struct i965_surface *dst_surface,
    VARectangle *dst_rect);

VAStatus
gen8_yuv420p8_scaling_post_processing(
    VADriverContextP   ctx,
    struct i965_post_processing_context *pp_context,
    struct i965_surface *src_surface,
    VARectangle *src_rect,
    struct i965_surface *dst_surface,
    VARectangle *dst_rect);

VAStatus
gen9_10bit_8bit_scaling_post_processing(VADriverContextP   ctx,
                                        struct i965_post_processing_context *pp_context,
                                        struct i965_surface *src_surface,
                                        VARectangle *src_rect,
                                        struct i965_surface *dst_surface,
                                        VARectangle *dst_rect);

VAStatus
gen8_8bit_420_rgb32_scaling_post_processing(VADriverContextP   ctx,
                                            struct i965_post_processing_context *pp_context,
                                            struct i965_surface *src_surface,
                                            VARectangle *src_rect,
                                            struct i965_surface *dst_surface,
                                            VARectangle *dst_rect);

VAStatus
gen9_8bit_420_rgb32_scaling_post_processing(VADriverContextP   ctx,
                                            struct i965_post_processing_context *pp_context,
                                            struct i965_surface *src_surface,
                                            VARectangle *src_rect,
                                            struct i965_surface *dst_surface,
                                            VARectangle *dst_rect);

VAStatus
gen9_p010_scaling_post_processing(VADriverContextP   ctx,
                                  struct i965_post_processing_context *pp_context,
                                  struct i965_surface *src_surface,
                                  VARectangle *src_rect,
                                  struct i965_surface *dst_surface,
                                  VARectangle *dst_rect);

void
gen8_clear_surface(VADriverContextP ctx,
                   struct i965_post_processing_context *pp_context,
                   const struct object_surface *obj_surface,
                   unsigned int color);

void
gen9_clear_surface(VADriverContextP ctx,
                   struct i965_post_processing_context *pp_context,
                   const struct object_surface *obj_surface,
                   unsigned int color);

#endif  // _INTEL_COMMON_VPP_INTERNAL_H_
