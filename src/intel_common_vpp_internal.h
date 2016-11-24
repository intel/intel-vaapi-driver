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

/* the below is defined for YUV420 format scaling */
#define SRC_MSB         0x0001
#define DST_MSB         0x0002
#define SRC_PACKED      0x0004
#define DST_PACKED      0x0008
#define PACKED_MASK     0x000C

#define BTI_SCALING_INPUT_Y     0
#define BTI_SCALING_OUTPUT_Y    8

struct scaling_input_parameter {
    unsigned int input_data[5];

    float inv_width;
    float inv_height;

    struct {
        unsigned int src_msb : 1;
        unsigned int dst_msb : 1;
        unsigned int src_packed : 1;
        unsigned int dst_packed : 1;
        unsigned int reserved : 28;
    } dw7;

    int x_dst;
    int y_dst;
    float    x_factor; // src_rect_width / dst_rect_width / Surface_width
    float    y_factor; // src_rect_height / dst_rect_height / Surface_height
    float    x_orig;
    float    y_orig;
    unsigned int bti_input;
    unsigned int bti_output;
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

#endif  // _INTEL_COMMON_VPP_INTERNAL_H_
