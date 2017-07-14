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

#include "i965_post_processing.h"
#include "i965_encoder_api.h"

static struct intel_fraction
reduce_fraction(struct intel_fraction f)
{
    unsigned int a = f.num, b = f.den, c;
    while ((c = a % b)) {
        a = b;
        b = c;
    }
    return (struct intel_fraction) {
        f.num / b, f.den / b
    };
}

static VAStatus
clear_border(struct object_surface *obj_surface)
{
    int width[3], height[3], hstride[3], vstride[3]; /* in byte */
    int planes;
    unsigned char* p;
    int i, j;

    if (obj_surface->border_cleared)
        return VA_STATUS_SUCCESS;

    if (obj_surface->fourcc == VA_FOURCC_NV12) {
        planes = 2;
        width[0] = width[1] = obj_surface->orig_width;
        height[0] = obj_surface->orig_height;
        height[1] = obj_surface->orig_height / 2;
        hstride[0] = hstride[1] = obj_surface->width;
        vstride[0] = obj_surface->height;
        vstride[1] = obj_surface->height / 2;

    } else {
        /* todo add P010 */
        return VA_STATUS_SUCCESS;
    }
    drm_intel_gem_bo_map_gtt(obj_surface->bo);

    p = (unsigned char*)obj_surface->bo->virtual;
    if (!p)
        return VA_STATUS_ERROR_INVALID_SURFACE;

    for (i = 0; i < planes; i++) {
        int w = width[i];
        int h = height[i];
        int hs = hstride[i];
        int vs = vstride[i];
        /* right */
        for (j = 0; j < h; j++) {
            memset(p + w, 0, hs - w);
            p += hs;
        }
        /* bottom */
        for (/* nothing */; j < vs; j++) {
            memset(p, 0, hs);
            p += hs;
        }

    }
    drm_intel_gem_bo_unmap_gtt(obj_surface->bo);
    obj_surface->border_cleared = true;
    return VA_STATUS_SUCCESS;
}

static VAStatus
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
    int format = VA_RT_FORMAT_YUV420;
    unsigned int fourcc = VA_FOURCC_NV12;

    /* releae the temporary surface */
    if (encoder_context->is_tmp_id) {
        i965_DestroySurfaces(ctx, &encoder_context->input_yuv_surface, 1);
        encode_state->input_yuv_object = NULL;
    }

    encoder_context->is_tmp_id = 0;
    obj_surface = SURFACE(encode_state->current_render_target);
    assert(obj_surface && obj_surface->bo);

    if (!obj_surface || !obj_surface->bo)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    if (VAProfileHEVCMain10 == profile &&
        obj_surface->fourcc != VA_FOURCC_P010)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    if (obj_surface->fourcc == VA_FOURCC_NV12 ||
        (VAProfileHEVCMain10 == profile &&
         obj_surface->fourcc == VA_FOURCC_P010)) {

        unsigned int tiling = 0, swizzle = 0;
        dri_bo_get_tiling(obj_surface->bo, &tiling, &swizzle);

        if (tiling == I915_TILING_Y) {
            encoder_context->input_yuv_surface = encode_state->current_render_target;
            encode_state->input_yuv_object = obj_surface;
            return clear_border(obj_surface);
        }
    }

    if (VAProfileHEVCMain10 == profile) {
        format = VA_RT_FORMAT_YUV420_10BPP;
        fourcc = VA_FOURCC_P010;
    }

    rect.x = 0;
    rect.y = 0;
    rect.width = obj_surface->orig_width;
    rect.height = obj_surface->orig_height;

    src_surface.base = (struct object_base *)obj_surface;
    src_surface.type = I965_SURFACE_TYPE_SURFACE;
    src_surface.flags = I965_SURFACE_FLAG_FRAME;

    status = i965_CreateSurfaces(ctx,
                                 obj_surface->orig_width,
                                 obj_surface->orig_height,
                                 format,
                                 1,
                                 &encoder_context->input_yuv_surface);
    ASSERT_RET(status == VA_STATUS_SUCCESS, status);

    obj_surface = SURFACE(encoder_context->input_yuv_surface);
    encode_state->input_yuv_object = obj_surface;
    assert(obj_surface);
    i965_check_alloc_surface_bo(ctx, obj_surface, 1, fourcc, SUBSAMPLE_YUV420);

    dst_surface.base = (struct object_base *)obj_surface;
    dst_surface.type = I965_SURFACE_TYPE_SURFACE;
    dst_surface.flags = I965_SURFACE_FLAG_FRAME;

    status = i965_image_processing(ctx,
                                   &src_surface,
                                   &rect,
                                   &dst_surface,
                                   &rect);
    assert(status == VA_STATUS_SUCCESS);

    encoder_context->is_tmp_id = 1;

    return clear_border(obj_surface);
}


static VAStatus
intel_encoder_check_jpeg_yuv_surface(VADriverContextP ctx,
                                     VAProfile profile,
                                     struct encode_state *encode_state,
                                     struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct i965_surface src_surface, dst_surface;
    struct object_surface *obj_surface;
    VAStatus status;
    VARectangle rect;
    int format = 0, fourcc = 0, subsample = 0;

    /* releae the temporary surface */
    if (encoder_context->is_tmp_id) {
        i965_DestroySurfaces(ctx, &encoder_context->input_yuv_surface, 1);
        encode_state->input_yuv_object = NULL;
    }

    encoder_context->is_tmp_id = 0;
    obj_surface = SURFACE(encode_state->current_render_target);
    assert(obj_surface && obj_surface->bo);

    if (!obj_surface || !obj_surface->bo)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    unsigned int tiling = 0, swizzle = 0;

    dri_bo_get_tiling(obj_surface->bo, &tiling, &swizzle);

    if (tiling == I915_TILING_Y) {
        if ((obj_surface->fourcc == VA_FOURCC_NV12)  || (obj_surface->fourcc == VA_FOURCC_UYVY) ||
            (obj_surface->fourcc == VA_FOURCC_YUY2)  || (obj_surface->fourcc == VA_FOURCC_Y800) ||
            (obj_surface->fourcc == VA_FOURCC_RGBA)  || (obj_surface->fourcc == VA_FOURCC_444P)) {
            encoder_context->input_yuv_surface = encode_state->current_render_target;
            encode_state->input_yuv_object = obj_surface;
            return VA_STATUS_SUCCESS;
        }
    }

    rect.x = 0;
    rect.y = 0;
    rect.width = obj_surface->orig_width;
    rect.height = obj_surface->orig_height;

    src_surface.base = (struct object_base *)obj_surface;
    src_surface.type = I965_SURFACE_TYPE_SURFACE;
    src_surface.flags = I965_SURFACE_FLAG_FRAME;

    switch (obj_surface->fourcc) {

    case VA_FOURCC_YUY2:
        fourcc = VA_FOURCC_YUY2;
        format = VA_RT_FORMAT_YUV422;
        subsample = SUBSAMPLE_YUV422H;
        break;

    case VA_FOURCC_UYVY:
        fourcc = VA_FOURCC_UYVY;
        format = VA_RT_FORMAT_YUV422;
        subsample = SUBSAMPLE_YUV422H;
        break;

    case VA_FOURCC_Y800:
        fourcc = VA_FOURCC_Y800;
        format = VA_RT_FORMAT_YUV400;
        subsample = SUBSAMPLE_YUV400;
        break;

    case VA_FOURCC_444P:
        fourcc = VA_FOURCC_444P;
        format = VA_RT_FORMAT_YUV444;
        subsample = SUBSAMPLE_YUV444;
        break;

    case VA_FOURCC_RGBA:
        fourcc = VA_FOURCC_RGBA;
        format = VA_RT_FORMAT_RGB32;
        subsample = SUBSAMPLE_RGBX;
        break;

    default: //All other scenarios will have NV12 format
        fourcc = VA_FOURCC_NV12;
        format = VA_RT_FORMAT_YUV420;
        subsample = SUBSAMPLE_YUV420;
        break;
    }

    status = i965_CreateSurfaces(ctx,
                                 obj_surface->orig_width,
                                 obj_surface->orig_height,
                                 format,
                                 1,
                                 &encoder_context->input_yuv_surface);
    assert(status == VA_STATUS_SUCCESS);

    if (status != VA_STATUS_SUCCESS)
        return status;

    obj_surface = SURFACE(encoder_context->input_yuv_surface);
    encode_state->input_yuv_object = obj_surface;
    assert(obj_surface);
    i965_check_alloc_surface_bo(ctx, obj_surface, 1, fourcc, subsample);

    dst_surface.base = (struct object_base *)obj_surface;
    dst_surface.type = I965_SURFACE_TYPE_SURFACE;
    dst_surface.flags = I965_SURFACE_FLAG_FRAME;

    //The Y800 format is expected to be tiled.
    //Linear Y800 is a corner case and needs code in the i965_image_processing.
    if (obj_surface->fourcc != VA_FOURCC_Y800) {
        status = i965_image_processing(ctx,
                                       &src_surface,
                                       &rect,
                                       &dst_surface,
                                       &rect);
        assert(status == VA_STATUS_SUCCESS);
    }

    encoder_context->is_tmp_id = 1;

    return VA_STATUS_SUCCESS;
}

static VAStatus
intel_encoder_check_brc_h264_sequence_parameter(VADriverContextP ctx,
                                                struct encode_state *encode_state,
                                                struct intel_encoder_context *encoder_context,
                                                unsigned int *seq_bits_per_second)
{
    VAEncSequenceParameterBufferH264 *seq_param = (VAEncSequenceParameterBufferH264 *)encode_state->seq_param_ext->buffer;
    struct intel_fraction framerate;
    unsigned short num_pframes_in_gop, num_bframes_in_gop;

    if (!encoder_context->is_new_sequence)
        return VA_STATUS_SUCCESS;

    assert(seq_param);

    if (!seq_param->num_units_in_tick || !seq_param->time_scale) {
        framerate = (struct intel_fraction) {
            30, 1
        };
    } else {
        // for the highest layer
        framerate = (struct intel_fraction) {
            seq_param->time_scale, 2 * seq_param->num_units_in_tick
        };
    }
    framerate = reduce_fraction(framerate);

    encoder_context->brc.num_iframes_in_gop = 1; // Always 1

    if (seq_param->intra_period == 0) { // E.g. IDRPP... / IDR(PBB)... (no IDR/I any more)
        if (seq_param->ip_period == 0)
            goto error;

        encoder_context->brc.gop_size = (framerate.num + framerate.den - 1) / framerate.den; // fake
        num_pframes_in_gop = (encoder_context->brc.gop_size +
                              seq_param->ip_period - 1) / seq_param->ip_period - 1;
    } else if (seq_param->intra_period == 1) { // E.g. IDRIII...
        encoder_context->brc.gop_size = 1;
        num_pframes_in_gop = 0;
    } else {
        if (seq_param->ip_period == 0)
            goto error;

        encoder_context->brc.gop_size = seq_param->intra_period;
        num_pframes_in_gop = (encoder_context->brc.gop_size +
                              seq_param->ip_period - 1) / seq_param->ip_period - 1;
    }

    num_bframes_in_gop = (encoder_context->brc.gop_size -
                          encoder_context->brc.num_iframes_in_gop - num_pframes_in_gop);

    if (num_pframes_in_gop != encoder_context->brc.num_pframes_in_gop ||
        num_bframes_in_gop != encoder_context->brc.num_bframes_in_gop ||
        framerate.num != encoder_context->brc.framerate[encoder_context->layer.num_layers - 1].num ||
        framerate.den != encoder_context->brc.framerate[encoder_context->layer.num_layers - 1].den) {
        encoder_context->brc.num_pframes_in_gop = num_pframes_in_gop;
        encoder_context->brc.num_bframes_in_gop = num_bframes_in_gop;
        encoder_context->brc.framerate[encoder_context->layer.num_layers - 1] = framerate;
        encoder_context->brc.need_reset = 1;
    }

    if (!encoder_context->brc.hrd_buffer_size ||
        !encoder_context->brc.hrd_initial_buffer_fullness) {
        encoder_context->brc.hrd_buffer_size = seq_param->bits_per_second << 1;
        encoder_context->brc.hrd_initial_buffer_fullness = seq_param->bits_per_second;
    }

    *seq_bits_per_second = seq_param->bits_per_second;

    return VA_STATUS_SUCCESS;

error:
    return VA_STATUS_ERROR_INVALID_PARAMETER;
}

static VAStatus
intel_encoder_check_brc_vp8_sequence_parameter(VADriverContextP ctx,
                                               struct encode_state *encode_state,
                                               struct intel_encoder_context *encoder_context,
                                               unsigned int *seq_bits_per_second)
{
    VAEncSequenceParameterBufferVP8 *seq_param = (VAEncSequenceParameterBufferVP8 *)encode_state->seq_param_ext->buffer;
    unsigned int num_pframes_in_gop;

    if (!encoder_context->is_new_sequence)
        return VA_STATUS_SUCCESS;

    assert(seq_param);

    encoder_context->brc.num_iframes_in_gop = 1;// Always 1
    encoder_context->brc.num_bframes_in_gop = 0;// No B frame

    if (seq_param->intra_period == 0) {         // E.g. IPPP... (only one I frame in the stream)
        encoder_context->brc.gop_size = 30;     // fake
    } else {
        encoder_context->brc.gop_size = seq_param->intra_period;
    }

    num_pframes_in_gop = encoder_context->brc.gop_size - 1;

    if (!encoder_context->brc.framerate[encoder_context->layer.num_layers - 1].num) {
        // for the highest layer
        encoder_context->brc.framerate[encoder_context->layer.num_layers - 1] = (struct intel_fraction) {
            30, 1
        };
        encoder_context->brc.need_reset = 1;
    }

    if (num_pframes_in_gop != encoder_context->brc.num_pframes_in_gop) {
        encoder_context->brc.num_pframes_in_gop = num_pframes_in_gop;
        encoder_context->brc.need_reset = 1;
    }

    if (!encoder_context->brc.hrd_buffer_size ||
        !encoder_context->brc.hrd_initial_buffer_fullness) {
        encoder_context->brc.hrd_buffer_size = seq_param->bits_per_second << 1;
        encoder_context->brc.hrd_initial_buffer_fullness = seq_param->bits_per_second;
        encoder_context->brc.need_reset = 1;
    }

    *seq_bits_per_second = seq_param->bits_per_second;

    return VA_STATUS_SUCCESS;
}

static VAStatus
intel_encoder_check_brc_hevc_sequence_parameter(VADriverContextP ctx,
                                                struct encode_state *encode_state,
                                                struct intel_encoder_context *encoder_context,
                                                unsigned int *seq_bits_per_second)
{
    VAEncSequenceParameterBufferHEVC *seq_param = (VAEncSequenceParameterBufferHEVC*)encode_state->seq_param_ext->buffer;
    struct intel_fraction framerate;
    unsigned int gop_size, num_iframes_in_gop, num_pframes_in_gop, num_bframes_in_gop;

    if (!encoder_context->is_new_sequence)
        return VA_STATUS_SUCCESS;
    if (!seq_param)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    if (!seq_param->vui_time_scale || !seq_param->vui_num_units_in_tick)
        framerate = (struct intel_fraction) {
        30, 1
    };
    else
        framerate = (struct intel_fraction) {
        seq_param->vui_time_scale, seq_param->vui_num_units_in_tick
    };
    framerate = reduce_fraction(framerate);

    num_iframes_in_gop = 1;
    if (seq_param->intra_period == 0) {
        gop_size = -1;
        num_pframes_in_gop = -1;
    } else if (seq_param->intra_period == 1) {
        gop_size = 1;
        num_pframes_in_gop = 0;
    } else {
        gop_size = seq_param->intra_period;
        num_pframes_in_gop = (seq_param->intra_period + seq_param->ip_period - 1) / seq_param->ip_period - 1;
    }
    num_bframes_in_gop = gop_size - num_iframes_in_gop - num_pframes_in_gop;

    if (encoder_context->brc.framerate[0].num != framerate.num ||
        encoder_context->brc.framerate[0].den != framerate.den) {
        encoder_context->brc.framerate[0] = framerate;
        encoder_context->brc.need_reset = 1;
    }

    if (encoder_context->brc.gop_size != gop_size ||
        encoder_context->brc.num_iframes_in_gop != num_iframes_in_gop ||
        encoder_context->brc.num_pframes_in_gop != num_pframes_in_gop ||
        encoder_context->brc.num_bframes_in_gop != num_bframes_in_gop) {
        encoder_context->brc.gop_size = gop_size;
        encoder_context->brc.num_iframes_in_gop = num_iframes_in_gop;
        encoder_context->brc.num_pframes_in_gop = num_pframes_in_gop;
        encoder_context->brc.num_bframes_in_gop = num_bframes_in_gop;
        encoder_context->brc.need_reset = 1;
    }

    *seq_bits_per_second = seq_param->bits_per_second;

    return VA_STATUS_SUCCESS;
}

static VAStatus
intel_encoder_check_brc_vp9_sequence_parameter(VADriverContextP ctx,
                                               struct encode_state *encode_state,
                                               struct intel_encoder_context *encoder_context,
                                               unsigned int *seq_bits_per_second)
{
    VAEncSequenceParameterBufferVP9 *seq_param = (VAEncSequenceParameterBufferVP9*)encode_state->seq_param_ext->buffer;
    unsigned int gop_size;

    if (!encoder_context->is_new_sequence)
        return VA_STATUS_SUCCESS;
    if (!seq_param)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    if (seq_param->intra_period == 0)
        gop_size = -1; // Dummy value (infinity).
    else
        gop_size = seq_param->intra_period;

    if (encoder_context->brc.gop_size != gop_size) {
        encoder_context->brc.gop_size = gop_size;
        encoder_context->brc.need_reset = 1;
    }

    *seq_bits_per_second = seq_param->bits_per_second;

    return VA_STATUS_SUCCESS;
}

static VAStatus
intel_encoder_check_brc_sequence_parameter(VADriverContextP ctx,
                                           struct encode_state *encode_state,
                                           struct intel_encoder_context *encoder_context,
                                           unsigned int *seq_bits_per_second)
{
    *seq_bits_per_second = 0;

    switch (encoder_context->codec) {
    case CODEC_H264:
    case CODEC_H264_MVC:
        return intel_encoder_check_brc_h264_sequence_parameter(ctx, encode_state, encoder_context, seq_bits_per_second);

    case CODEC_VP8:
        return intel_encoder_check_brc_vp8_sequence_parameter(ctx, encode_state, encoder_context, seq_bits_per_second);

    case CODEC_HEVC:
        return intel_encoder_check_brc_hevc_sequence_parameter(ctx, encode_state, encoder_context, seq_bits_per_second);

    case CODEC_VP9:
        return intel_encoder_check_brc_vp9_sequence_parameter(ctx, encode_state, encoder_context, seq_bits_per_second);

    default:
        // TODO: other codecs
        return VA_STATUS_SUCCESS;
    }
}

static void
intel_encoder_check_rate_control_parameter(VADriverContextP ctx,
                                           struct intel_encoder_context *encoder_context,
                                           VAEncMiscParameterRateControl *misc,
                                           int *hl_bitrate_updated)
{
    int temporal_id = 0;

    if (encoder_context->layer.num_layers >= 2)
        temporal_id = misc->rc_flags.bits.temporal_id;

    if (temporal_id >= encoder_context->layer.num_layers)
        return;

    if (misc->rc_flags.bits.reset)
        encoder_context->brc.need_reset = 1;

    if (encoder_context->brc.bits_per_second[temporal_id] != misc->bits_per_second) {
        encoder_context->brc.bits_per_second[temporal_id] = misc->bits_per_second;
        encoder_context->brc.need_reset = 1;
    }

    if (encoder_context->brc.mb_rate_control[temporal_id] != misc->rc_flags.bits.mb_rate_control) {
        encoder_context->brc.mb_rate_control[temporal_id] = misc->rc_flags.bits.mb_rate_control;
        encoder_context->brc.need_reset = 1;
    }

    if (encoder_context->brc.target_percentage[temporal_id] != misc->target_percentage) {
        encoder_context->brc.target_percentage[temporal_id] = misc->target_percentage;
        encoder_context->brc.need_reset = 1;
    }

    if (encoder_context->brc.window_size != misc->window_size ||
        encoder_context->brc.initial_qp  != misc->initial_qp ||
        encoder_context->brc.min_qp      != misc->min_qp) {
        encoder_context->brc.window_size = misc->window_size;
        encoder_context->brc.initial_qp  = misc->initial_qp;
        encoder_context->brc.min_qp      = misc->min_qp;
        encoder_context->brc.need_reset = 1;
    }

    if (temporal_id == encoder_context->layer.num_layers - 1)
        *hl_bitrate_updated = 1;
}

static void
intel_encoder_check_hrd_parameter(VADriverContextP ctx,
                                  struct intel_encoder_context *encoder_context,
                                  VAEncMiscParameterHRD *misc)
{
    if (encoder_context->brc.hrd_buffer_size != misc->buffer_size ||
        encoder_context->brc.hrd_initial_buffer_fullness != misc->initial_buffer_fullness) {
        encoder_context->brc.hrd_buffer_size = misc->buffer_size;
        encoder_context->brc.hrd_initial_buffer_fullness = misc->initial_buffer_fullness;
        encoder_context->brc.need_reset = 1;
    }
}

static void
intel_encoder_check_framerate_parameter(VADriverContextP ctx,
                                        struct intel_encoder_context *encoder_context,
                                        VAEncMiscParameterFrameRate *misc)
{
    struct intel_fraction framerate;
    int temporal_id = 0;

    if (encoder_context->layer.num_layers >= 2)
        temporal_id = misc->framerate_flags.bits.temporal_id;

    if (temporal_id >= encoder_context->layer.num_layers)
        return;

    if (misc->framerate & 0xffff0000)
        framerate = (struct intel_fraction) {
        misc->framerate & 0xffff, misc->framerate >> 16 & 0xffff
    };
    else
        framerate = (struct intel_fraction) {
        misc->framerate, 1
    };
    framerate = reduce_fraction(framerate);

    if (encoder_context->brc.framerate[temporal_id].num != framerate.num ||
        encoder_context->brc.framerate[temporal_id].den != framerate.den) {
        encoder_context->brc.framerate[temporal_id] = framerate;
        encoder_context->brc.need_reset = 1;
    }
}

static void
intel_encoder_check_roi_parameter(VADriverContextP ctx,
                                  struct intel_encoder_context *encoder_context,
                                  VAEncMiscParameterBufferROI *misc)
{
    int i = 0;

    encoder_context->brc.num_roi = MIN(misc->num_roi, I965_MAX_NUM_ROI_REGIONS);
    encoder_context->brc.roi_max_delta_qp = misc->max_delta_qp;
    encoder_context->brc.roi_min_delta_qp = misc->min_delta_qp;
    encoder_context->brc.roi_value_is_qp_delta = 0;

    if (encoder_context->rate_control_mode != VA_RC_CQP)
        encoder_context->brc.roi_value_is_qp_delta = misc->roi_flags.bits.roi_value_is_qp_delta;

    for (i = 0; i <  encoder_context->brc.num_roi; i++) {
        encoder_context->brc.roi[i].left = misc->roi->roi_rectangle.x;
        encoder_context->brc.roi[i].right = encoder_context->brc.roi[i].left + misc->roi->roi_rectangle.width;
        encoder_context->brc.roi[i].top = misc->roi->roi_rectangle.y;
        encoder_context->brc.roi[i].bottom = encoder_context->brc.roi[i].top + misc->roi->roi_rectangle.height;
        encoder_context->brc.roi[i].value = misc->roi->roi_value;
    }
}

static VAStatus
intel_encoder_check_brc_parameter(VADriverContextP ctx,
                                  struct encode_state *encode_state,
                                  struct intel_encoder_context *encoder_context)
{
    VAStatus ret;
    VAEncMiscParameterBuffer *misc_param;
    int i, j;
    int hl_bitrate_updated = 0; // Indicate whether the bitrate for the highest level is changed in misc parameters
    unsigned int seq_bits_per_second = 0;

    if (encoder_context->rate_control_mode & (VA_RC_CBR | VA_RC_VBR)) {
        ret = intel_encoder_check_brc_sequence_parameter(ctx, encode_state, encoder_context, &seq_bits_per_second);

        if (ret)
            return ret;
    }

    for (i = 0; i < ARRAY_ELEMS(encode_state->misc_param); i++) {
        for (j = 0; j < ARRAY_ELEMS(encode_state->misc_param[0]); j++) {
            if (!encode_state->misc_param[i][j] || !encode_state->misc_param[i][j]->buffer)
                continue;

            misc_param = (VAEncMiscParameterBuffer *)encode_state->misc_param[i][j]->buffer;

            if (!(encoder_context->rate_control_mode & (VA_RC_CBR | VA_RC_VBR))) {
                if (misc_param->type != VAEncMiscParameterTypeROI)
                    continue;
            }

            switch (misc_param->type) {
            case VAEncMiscParameterTypeFrameRate:
                intel_encoder_check_framerate_parameter(ctx,
                                                        encoder_context,
                                                        (VAEncMiscParameterFrameRate *)misc_param->data);
                break;

            case VAEncMiscParameterTypeRateControl:
                intel_encoder_check_rate_control_parameter(ctx,
                                                           encoder_context,
                                                           (VAEncMiscParameterRateControl *)misc_param->data,
                                                           &hl_bitrate_updated);
                break;

            case VAEncMiscParameterTypeHRD:
                intel_encoder_check_hrd_parameter(ctx,
                                                  encoder_context,
                                                  (VAEncMiscParameterHRD *)misc_param->data);
                break;

            case VAEncMiscParameterTypeROI:
                intel_encoder_check_roi_parameter(ctx,
                                                  encoder_context,
                                                  (VAEncMiscParameterBufferROI *)misc_param->data);
                break;

            default:
                break;
            }
        }
    }

    if (!hl_bitrate_updated && seq_bits_per_second &&
        encoder_context->brc.bits_per_second[encoder_context->layer.num_layers - 1] != seq_bits_per_second) {

        encoder_context->brc.bits_per_second[encoder_context->layer.num_layers - 1] = seq_bits_per_second;
        encoder_context->brc.need_reset = 1;

    }

    return VA_STATUS_SUCCESS;
}

static VAStatus
intel_encoder_check_temporal_layer_structure(VADriverContextP ctx,
                                             struct encode_state *encode_state,
                                             struct intel_encoder_context *encoder_context)
{
    VAEncMiscParameterBuffer* misc_param;
    VAEncMiscParameterTemporalLayerStructure *tls_paramter;
    unsigned int rate_control_mode = encoder_context->rate_control_mode;
    int i;

    if (!encoder_context->is_new_sequence) {
        if (encoder_context->layer.num_layers > 1)
            encoder_context->layer.curr_frame_layer_id = encoder_context->layer.frame_layer_ids[(encoder_context->num_frames_in_sequence - 1) % encoder_context->layer.size_frame_layer_ids];
        else
            encoder_context->layer.curr_frame_layer_id = 0;

        return VA_STATUS_SUCCESS;
    }

    if (!(rate_control_mode & (VA_RC_CBR | VA_RC_VBR)))
        return VA_STATUS_SUCCESS;

    if (!encode_state->misc_param[VAEncMiscParameterTypeTemporalLayerStructure][0] ||
        !encode_state->misc_param[VAEncMiscParameterTypeTemporalLayerStructure][0]->buffer)
        return VA_STATUS_SUCCESS;

    misc_param = (VAEncMiscParameterBuffer*)encode_state->misc_param[VAEncMiscParameterTypeTemporalLayerStructure][0]->buffer;
    tls_paramter = (VAEncMiscParameterTemporalLayerStructure *)misc_param->data;

    if (tls_paramter->number_of_layers <= 1)
        return VA_STATUS_SUCCESS;

    if (tls_paramter->number_of_layers > MAX_TEMPORAL_LAYERS)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    if (tls_paramter->periodicity > 32 || tls_paramter->periodicity <= 1)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    for (i = 0; i < tls_paramter->number_of_layers; i++) {
        if (!encode_state->misc_param[VAEncMiscParameterTypeRateControl][i] ||
            !encode_state->misc_param[VAEncMiscParameterTypeRateControl][i]->buffer ||
            !encode_state->misc_param[VAEncMiscParameterTypeFrameRate][i] ||
            !encode_state->misc_param[VAEncMiscParameterTypeFrameRate][i]->buffer) {

            return VA_STATUS_ERROR_INVALID_PARAMETER;
        }
    }

    encoder_context->layer.size_frame_layer_ids = tls_paramter->periodicity;
    encoder_context->layer.num_layers = tls_paramter->number_of_layers;

    for (i = 0; i < encoder_context->layer.size_frame_layer_ids; i++) {
        if (tls_paramter->layer_id[i] >= tls_paramter->number_of_layers)
            return VA_STATUS_ERROR_INVALID_PARAMETER;

        encoder_context->layer.frame_layer_ids[i] = tls_paramter->layer_id[i];
    }

    if (encoder_context->is_new_sequence)
        encoder_context->layer.curr_frame_layer_id = 0;
    else
        encoder_context->layer.curr_frame_layer_id = encoder_context->layer.frame_layer_ids[(encoder_context->num_frames_in_sequence - 1) % encoder_context->layer.size_frame_layer_ids];

    return VA_STATUS_SUCCESS;
}

static VAStatus
intel_encoder_check_misc_parameter(VADriverContextP ctx,
                                   VAProfile profile,
                                   struct encode_state *encode_state,
                                   struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    VAStatus ret = VA_STATUS_SUCCESS;

    if (encode_state->misc_param[VAEncMiscParameterTypeQualityLevel][0] &&
        encode_state->misc_param[VAEncMiscParameterTypeQualityLevel][0]->buffer) {
        VAEncMiscParameterBuffer* pMiscParam = (VAEncMiscParameterBuffer*)encode_state->misc_param[VAEncMiscParameterTypeQualityLevel][0]->buffer;
        VAEncMiscParameterBufferQualityLevel* param_quality_level = (VAEncMiscParameterBufferQualityLevel*)pMiscParam->data;
        encoder_context->quality_level = param_quality_level->quality_level;

        if (encoder_context->quality_level == 0) {
            switch (profile) {
            case VAProfileH264ConstrainedBaseline:
            case VAProfileH264Main:
            case VAProfileH264High:
            case VAProfileH264MultiviewHigh:
            case VAProfileH264StereoHigh:
                if (IS_SKL(i965->intel.device_info) ||
                    IS_BXT(i965->intel.device_info))
                    encoder_context->quality_level = ENCODER_DEFAULT_QUALITY_AVC;
                break;

            case VAProfileHEVCMain:
            case VAProfileHEVCMain10:
                encoder_context->quality_level = ENCODER_DEFAULT_QUALITY_HEVC;
                break;

            default:
                encoder_context->quality_level = ENCODER_DEFAULT_QUALITY;
                break;
            }
        } else if (encoder_context->quality_level > encoder_context->quality_range) {
            ret = VA_STATUS_ERROR_INVALID_PARAMETER;
            goto out;
        }
    }

    ret = intel_encoder_check_temporal_layer_structure(ctx, encode_state, encoder_context);

    if (ret)
        goto out;

    ret = intel_encoder_check_brc_parameter(ctx, encode_state, encoder_context);

out:
    return ret;
}

static VAStatus
intel_encoder_check_avc_parameter(VADriverContextP ctx,
                                  struct encode_state *encode_state,
                                  struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct object_surface *obj_surface;
    struct object_buffer *obj_buffer;
    VAEncPictureParameterBufferH264 *pic_param = (VAEncPictureParameterBufferH264 *)encode_state->pic_param_ext->buffer;
    VAEncSequenceParameterBufferH264 *seq_param = (VAEncSequenceParameterBufferH264 *)encode_state->seq_param_ext->buffer;
    int i;

    assert(!(pic_param->CurrPic.flags & VA_PICTURE_H264_INVALID));

    if (pic_param->CurrPic.flags & VA_PICTURE_H264_INVALID)
        goto error;

    obj_surface = SURFACE(pic_param->CurrPic.picture_id);
    assert(obj_surface); /* It is possible the store buffer isn't allocated yet */

    if (!obj_surface)
        goto error;

    encode_state->reconstructed_object = obj_surface;
    obj_buffer = BUFFER(pic_param->coded_buf);
    assert(obj_buffer && obj_buffer->buffer_store && obj_buffer->buffer_store->bo);

    if (!obj_buffer || !obj_buffer->buffer_store || !obj_buffer->buffer_store->bo)
        goto error;

    if (encode_state->num_slice_params_ext > encoder_context->max_slice_or_seg_num)
        goto error;

    encode_state->coded_buf_object = obj_buffer;

    for (i = 0; i < 16; i++) {
        if (pic_param->ReferenceFrames[i].flags & VA_PICTURE_H264_INVALID ||
            pic_param->ReferenceFrames[i].picture_id == VA_INVALID_SURFACE)
            break;
        else {
            obj_surface = SURFACE(pic_param->ReferenceFrames[i].picture_id);
            assert(obj_surface);

            if (!obj_surface)
                goto error;

            if (obj_surface->bo)
                encode_state->reference_objects[i] = obj_surface;
            else
                encode_state->reference_objects[i] = NULL; /* FIXME: Warning or Error ??? */
        }
    }

    for (; i < 16; i++)
        encode_state->reference_objects[i] = NULL;

    /*
     * A sequence consists of an IDR unit, followed by zero or more non-IDR unit, but not including any
     * subsequent IDR unit, so idr_pic_flag can indicate the current frame is the start of a new
     * sequnce
     */
    encoder_context->is_new_sequence = (pic_param->pic_fields.bits.idr_pic_flag && seq_param);

    if (encoder_context->is_new_sequence) {
        encoder_context->num_frames_in_sequence = 0;
        encoder_context->frame_width_in_pixel = seq_param->picture_width_in_mbs * 16;
        encoder_context->frame_height_in_pixel = seq_param->picture_height_in_mbs * 16;
    }

    return VA_STATUS_SUCCESS;

error:
    return VA_STATUS_ERROR_INVALID_PARAMETER;
}

static VAStatus
intel_encoder_check_mpeg2_parameter(VADriverContextP ctx,
                                    struct encode_state *encode_state,
                                    struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    VAEncPictureParameterBufferMPEG2 *pic_param = (VAEncPictureParameterBufferMPEG2 *)encode_state->pic_param_ext->buffer;
    struct object_surface *obj_surface;
    struct object_buffer *obj_buffer;
    int i = 0;

    obj_surface = SURFACE(pic_param->reconstructed_picture);
    assert(obj_surface); /* It is possible the store buffer isn't allocated yet */

    if (!obj_surface)
        goto error;

    encode_state->reconstructed_object = obj_surface;
    obj_buffer = BUFFER(pic_param->coded_buf);
    assert(obj_buffer && obj_buffer->buffer_store && obj_buffer->buffer_store->bo);

    if (!obj_buffer || !obj_buffer->buffer_store || !obj_buffer->buffer_store->bo)
        goto error;

    encode_state->coded_buf_object = obj_buffer;

    if (pic_param->picture_type == VAEncPictureTypeIntra) {
    } else if (pic_param->picture_type == VAEncPictureTypePredictive) {
        assert(pic_param->forward_reference_picture != VA_INVALID_SURFACE);
        obj_surface = SURFACE(pic_param->forward_reference_picture);
        assert(obj_surface && obj_surface->bo);

        if (!obj_surface || !obj_surface->bo)
            goto error;

        encode_state->reference_objects[i++] = obj_surface;
    } else if (pic_param->picture_type == VAEncPictureTypeBidirectional) {
        assert(pic_param->forward_reference_picture != VA_INVALID_SURFACE);
        obj_surface = SURFACE(pic_param->forward_reference_picture);
        assert(obj_surface && obj_surface->bo);

        if (!obj_surface || !obj_surface->bo)
            goto error;

        encode_state->reference_objects[i++] = obj_surface;

        assert(pic_param->backward_reference_picture != VA_INVALID_SURFACE);
        obj_surface = SURFACE(pic_param->backward_reference_picture);
        assert(obj_surface && obj_surface->bo);

        if (!obj_surface || !obj_surface->bo)
            goto error;

        encode_state->reference_objects[i++] = obj_surface;
    } else
        goto error;

    for (; i < 16; i++)
        encode_state->reference_objects[i] = NULL;

    return VA_STATUS_SUCCESS;

error:
    return VA_STATUS_ERROR_INVALID_PARAMETER;
}

static VAStatus
intel_encoder_check_jpeg_parameter(VADriverContextP ctx,
                                   struct encode_state *encode_state,
                                   struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct object_buffer *obj_buffer;
    VAEncPictureParameterBufferJPEG *pic_param = (VAEncPictureParameterBufferJPEG *)encode_state->pic_param_ext->buffer;


    assert(!(pic_param->pic_flags.bits.profile)); //Baseline profile is 0.

    obj_buffer = BUFFER(pic_param->coded_buf);
    assert(obj_buffer && obj_buffer->buffer_store && obj_buffer->buffer_store->bo);

    if (!obj_buffer || !obj_buffer->buffer_store || !obj_buffer->buffer_store->bo)
        goto error;

    encode_state->coded_buf_object = obj_buffer;

    return VA_STATUS_SUCCESS;

error:
    return VA_STATUS_ERROR_INVALID_PARAMETER;
}

static VAStatus
intel_encoder_check_vp8_parameter(VADriverContextP ctx,
                                  struct encode_state *encode_state,
                                  struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    VAEncPictureParameterBufferVP8 *pic_param = (VAEncPictureParameterBufferVP8 *)encode_state->pic_param_ext->buffer;
    VAEncSequenceParameterBufferVP8 *seq_param = (VAEncSequenceParameterBufferVP8 *)encode_state->seq_param_ext->buffer;
    struct object_surface *obj_surface;
    struct object_buffer *obj_buffer;
    int i = 0;
    int is_key_frame = !pic_param->pic_flags.bits.frame_type;

    obj_surface = SURFACE(pic_param->reconstructed_frame);
    assert(obj_surface); /* It is possible the store buffer isn't allocated yet */

    if (!obj_surface)
        goto error;

    encode_state->reconstructed_object = obj_surface;
    obj_buffer = BUFFER(pic_param->coded_buf);
    assert(obj_buffer && obj_buffer->buffer_store && obj_buffer->buffer_store->bo);

    if (!obj_buffer || !obj_buffer->buffer_store || !obj_buffer->buffer_store->bo)
        goto error;

    encode_state->coded_buf_object = obj_buffer;

    if (!is_key_frame) {
        assert(pic_param->ref_last_frame != VA_INVALID_SURFACE);
        obj_surface = SURFACE(pic_param->ref_last_frame);
        assert(obj_surface && obj_surface->bo);

        if (!obj_surface || !obj_surface->bo)
            goto error;

        encode_state->reference_objects[i++] = obj_surface;

        assert(pic_param->ref_gf_frame != VA_INVALID_SURFACE);
        obj_surface = SURFACE(pic_param->ref_gf_frame);
        assert(obj_surface && obj_surface->bo);

        if (!obj_surface || !obj_surface->bo)
            goto error;

        encode_state->reference_objects[i++] = obj_surface;

        assert(pic_param->ref_arf_frame != VA_INVALID_SURFACE);
        obj_surface = SURFACE(pic_param->ref_arf_frame);
        assert(obj_surface && obj_surface->bo);

        if (!obj_surface || !obj_surface->bo)
            goto error;

        encode_state->reference_objects[i++] = obj_surface;
    }

    for (; i < 16; i++)
        encode_state->reference_objects[i] = NULL;

    encoder_context->is_new_sequence = (is_key_frame && seq_param);

    if (encoder_context->is_new_sequence) {
        encoder_context->num_frames_in_sequence = 0;
        encoder_context->frame_width_in_pixel = seq_param->frame_width;
        encoder_context->frame_height_in_pixel = seq_param->frame_height;
    }

    return VA_STATUS_SUCCESS;

error:
    return VA_STATUS_ERROR_INVALID_PARAMETER;
}

static VAStatus
intel_encoder_check_hevc_parameter(VADriverContextP ctx,
                                   struct encode_state *encode_state,
                                   struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct object_surface *obj_surface;
    struct object_buffer *obj_buffer;
    VAEncPictureParameterBufferHEVC *pic_param = (VAEncPictureParameterBufferHEVC *)encode_state->pic_param_ext->buffer;
    VAEncSliceParameterBufferHEVC *slice_param;
    VAEncSequenceParameterBufferHEVC *seq_param;
    int i;

    seq_param = NULL;

    if (encode_state->seq_param_ext &&
        encode_state->seq_param_ext->buffer)
        seq_param = (VAEncSequenceParameterBufferHEVC *)(encode_state->seq_param_ext->buffer);

    assert(!(pic_param->decoded_curr_pic.flags & VA_PICTURE_HEVC_INVALID));

    if (pic_param->decoded_curr_pic.flags & VA_PICTURE_HEVC_INVALID)
        goto error;

    obj_surface = SURFACE(pic_param->decoded_curr_pic.picture_id);
    assert(obj_surface); /* It is possible the store buffer isn't allocated yet */

    if (!obj_surface)
        goto error;

    encode_state->reconstructed_object = obj_surface;
    obj_buffer = BUFFER(pic_param->coded_buf);
    assert(obj_buffer && obj_buffer->buffer_store && obj_buffer->buffer_store->bo);

    if (!obj_buffer || !obj_buffer->buffer_store || !obj_buffer->buffer_store->bo)
        goto error;

    if (encode_state->num_slice_params_ext > encoder_context->max_slice_or_seg_num)
        goto error;

    encode_state->coded_buf_object = obj_buffer;

    for (i = 0; i < 15; i++) {
        if (pic_param->reference_frames[i].flags & VA_PICTURE_HEVC_INVALID ||
            pic_param->reference_frames[i].picture_id == VA_INVALID_SURFACE)
            break;
        else {
            obj_surface = SURFACE(pic_param->reference_frames[i].picture_id);
            assert(obj_surface);

            if (!obj_surface)
                goto error;

            if (obj_surface->bo)
                encode_state->reference_objects[i] = obj_surface;
            else
                encode_state->reference_objects[i] = NULL; /* FIXME: Warning or Error ??? */
        }
    }

    for (; i < 15; i++)
        encode_state->reference_objects[i] = NULL;

    for (i = 0; i < encode_state->num_slice_params_ext; i++) {
        slice_param = (VAEncSliceParameterBufferHEVC *)encode_state->slice_params_ext[i]->buffer;

        if (slice_param->slice_type != HEVC_SLICE_I &&
            slice_param->slice_type != HEVC_SLICE_P &&
            slice_param->slice_type != HEVC_SLICE_B)
            goto error;

        /* TODO: add more check here */
    }

    encoder_context->is_new_sequence = (pic_param->pic_fields.bits.idr_pic_flag && seq_param);

    return VA_STATUS_SUCCESS;

error:
    return VA_STATUS_ERROR_INVALID_PARAMETER;
}

static VAStatus
intel_encoder_check_vp9_parameter(VADriverContextP ctx,
                                  struct encode_state *encode_state,
                                  struct intel_encoder_context *encoder_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    VAEncPictureParameterBufferVP9 *pic_param;
    VAEncSequenceParameterBufferVP9 *seq_param;
    struct object_surface *obj_surface;
    struct object_buffer *obj_buffer;
    int i = 0;
    int is_key_frame = 0;
    int index;

    if (encode_state->pic_param_ext == NULL ||
        encode_state->pic_param_ext->buffer == NULL)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    seq_param = NULL;
    if (encode_state->seq_param_ext &&
        encode_state->seq_param_ext->buffer)
        seq_param = (VAEncSequenceParameterBufferVP9 *)(encode_state->seq_param_ext->buffer);

    pic_param = (VAEncPictureParameterBufferVP9 *)encode_state->pic_param_ext->buffer;

    obj_surface = SURFACE(pic_param->reconstructed_frame);

    if (!obj_surface)
        goto error;

    encode_state->reconstructed_object = obj_surface;
    obj_buffer = BUFFER(pic_param->coded_buf);

    if (!obj_buffer || !obj_buffer->buffer_store || !obj_buffer->buffer_store->bo)
        goto error;

    encode_state->coded_buf_object = obj_buffer;

    is_key_frame = !pic_param->pic_flags.bits.frame_type;
    if (!is_key_frame && !pic_param->pic_flags.bits.intra_only) {
        /* slot 0 is for last reference frame */
        index = pic_param->ref_flags.bits.ref_last_idx;
        obj_surface = SURFACE(pic_param->reference_frames[index]);
        if (obj_surface && obj_surface->bo)
            encode_state->reference_objects[i++] = obj_surface;
        else
            encode_state->reference_objects[i++] = NULL;

        /* slot 1 is for golden reference frame */
        index = pic_param->ref_flags.bits.ref_gf_idx;
        obj_surface = SURFACE(pic_param->reference_frames[index]);
        if (obj_surface && obj_surface->bo)
            encode_state->reference_objects[i++] = obj_surface;
        else
            encode_state->reference_objects[i++] = NULL;

        /* slot 2 is alt reference frame */
        index = pic_param->ref_flags.bits.ref_arf_idx;
        obj_surface = SURFACE(pic_param->reference_frames[index]);
        if (obj_surface && obj_surface->bo)
            encode_state->reference_objects[i++] = obj_surface;
        else
            encode_state->reference_objects[i++] = NULL;
    }

    for (; i < 16; i++)
        encode_state->reference_objects[i] = NULL;

    encoder_context->is_new_sequence = (is_key_frame && seq_param);

    return VA_STATUS_SUCCESS;

error:
    return VA_STATUS_ERROR_INVALID_PARAMETER;
}


static VAStatus
intel_encoder_sanity_check_input(VADriverContextP ctx,
                                 VAProfile profile,
                                 struct encode_state *encode_state,
                                 struct intel_encoder_context *encoder_context)
{
    VAStatus vaStatus;

    switch (profile) {
    case VAProfileH264ConstrainedBaseline:
    case VAProfileH264Main:
    case VAProfileH264High:
    case VAProfileH264MultiviewHigh:
    case VAProfileH264StereoHigh: {
        vaStatus = intel_encoder_check_avc_parameter(ctx, encode_state, encoder_context);
        if (vaStatus != VA_STATUS_SUCCESS)
            goto out;
        vaStatus = intel_encoder_check_yuv_surface(ctx, profile, encode_state, encoder_context);
        break;
    }

    case VAProfileMPEG2Simple:
    case VAProfileMPEG2Main: {
        vaStatus = intel_encoder_check_mpeg2_parameter(ctx, encode_state, encoder_context);
        if (vaStatus != VA_STATUS_SUCCESS)
            goto out;
        vaStatus = intel_encoder_check_yuv_surface(ctx, profile, encode_state, encoder_context);
        break;
    }

    case VAProfileJPEGBaseline:  {
        vaStatus = intel_encoder_check_jpeg_parameter(ctx, encode_state, encoder_context);
        if (vaStatus != VA_STATUS_SUCCESS)
            goto out;
        vaStatus = intel_encoder_check_jpeg_yuv_surface(ctx, profile, encode_state, encoder_context);
        break;
    }

    case VAProfileVP8Version0_3: {
        vaStatus = intel_encoder_check_vp8_parameter(ctx, encode_state, encoder_context);
        if (vaStatus != VA_STATUS_SUCCESS)
            goto out;
        vaStatus = intel_encoder_check_yuv_surface(ctx, profile, encode_state, encoder_context);
        break;
    }

    case VAProfileHEVCMain:
    case VAProfileHEVCMain10:  {
        vaStatus = intel_encoder_check_hevc_parameter(ctx, encode_state, encoder_context);
        if (vaStatus != VA_STATUS_SUCCESS)
            goto out;
        vaStatus = intel_encoder_check_yuv_surface(ctx, profile, encode_state, encoder_context);
        break;
    }

    case VAProfileVP9Profile0: {
        vaStatus = intel_encoder_check_vp9_parameter(ctx, encode_state, encoder_context);
        if (vaStatus != VA_STATUS_SUCCESS)
            goto out;
        vaStatus = intel_encoder_check_yuv_surface(ctx, profile, encode_state, encoder_context);
        break;
    }
    default:
        vaStatus = VA_STATUS_ERROR_UNSUPPORTED_PROFILE;
        break;
    }

    if (vaStatus == VA_STATUS_SUCCESS)
        vaStatus = intel_encoder_check_misc_parameter(ctx, profile, encode_state, encoder_context);

out:
    return vaStatus;
}

static VAStatus
intel_encoder_end_picture(VADriverContextP ctx,
                          VAProfile profile,
                          union codec_state *codec_state,
                          struct hw_context *hw_context)
{
    struct intel_encoder_context *encoder_context = (struct intel_encoder_context *)hw_context;
    struct encode_state *encode_state = &codec_state->encode;
    VAStatus vaStatus;

    vaStatus = intel_encoder_sanity_check_input(ctx, profile, encode_state, encoder_context);

    if (vaStatus != VA_STATUS_SUCCESS)
        return vaStatus;

    encoder_context->mfc_brc_prepare(encode_state, encoder_context);

    if ((encoder_context->vme_context && encoder_context->vme_pipeline)) {
        vaStatus = encoder_context->vme_pipeline(ctx, profile, encode_state, encoder_context);
        if (vaStatus != VA_STATUS_SUCCESS)
            return vaStatus;
    }

    encoder_context->mfc_pipeline(ctx, profile, encode_state, encoder_context);
    encoder_context->num_frames_in_sequence++;
    encoder_context->brc.need_reset = 0;
    /*
     * ROI is only available for the current frame, see the comment
     * for VAEncROI in va.h
     */
    encoder_context->brc.num_roi = 0;

    return VA_STATUS_SUCCESS;
}

static void
intel_encoder_context_destroy(void *hw_context)
{
    struct intel_encoder_context *encoder_context = (struct intel_encoder_context *)hw_context;

    encoder_context->mfc_context_destroy(encoder_context->mfc_context);

    if (encoder_context->vme_context_destroy && encoder_context->vme_context)
        encoder_context->vme_context_destroy(encoder_context->vme_context);

    if (encoder_context->enc_priv_state) {
        free(encoder_context->enc_priv_state);
        encoder_context->enc_priv_state = NULL;
    }

    if (encoder_context->is_tmp_id) {
        assert(encoder_context->input_yuv_surface != VA_INVALID_SURFACE);
        i965_DestroySurfaces(encoder_context->ctx, &encoder_context->input_yuv_surface, 1);
        encoder_context->is_tmp_id = 0;
    }

    intel_batchbuffer_free(encoder_context->base.batch);
    free(encoder_context);
}


static VAStatus
intel_encoder_get_status(VADriverContextP ctx, struct hw_context *hw_context, void *buffer)
{
    struct intel_encoder_context *encoder_context = (struct intel_encoder_context *)hw_context;
    struct i965_coded_buffer_segment *coded_buffer_segment = (struct i965_coded_buffer_segment *)buffer;

    if (encoder_context->get_status)
        return encoder_context->get_status(ctx, encoder_context, coded_buffer_segment);

    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

typedef Bool(* hw_init_func)(VADriverContextP, struct intel_encoder_context *);

static struct hw_context *
intel_enc_hw_context_init(VADriverContextP ctx,
                          struct object_config *obj_config,
                          hw_init_func vme_context_init,
                          hw_init_func mfc_context_init)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct intel_driver_data *intel = intel_driver_data(ctx);
    struct intel_encoder_context *encoder_context = calloc(1, sizeof(struct intel_encoder_context));
    int i;

    assert(encoder_context);
    encoder_context->base.destroy = intel_encoder_context_destroy;
    encoder_context->base.run = intel_encoder_end_picture;
    encoder_context->base.get_status = intel_encoder_get_status;
    encoder_context->base.batch = intel_batchbuffer_new(intel, I915_EXEC_RENDER, 0);
    encoder_context->input_yuv_surface = VA_INVALID_SURFACE;
    encoder_context->is_tmp_id = 0;
    encoder_context->low_power_mode = 0;
    encoder_context->rate_control_mode = VA_RC_NONE;
    encoder_context->quality_level = ENCODER_DEFAULT_QUALITY;
    encoder_context->quality_range = 1;
    encoder_context->layer.num_layers = 1;
    encoder_context->max_slice_or_seg_num = 1;
    encoder_context->ctx = ctx;

    if (obj_config->entrypoint == VAEntrypointEncSliceLP)
        encoder_context->low_power_mode = 1;

    switch (obj_config->profile) {
    case VAProfileMPEG2Simple:
    case VAProfileMPEG2Main:
        encoder_context->codec = CODEC_MPEG2;
        break;

    case VAProfileH264ConstrainedBaseline:
    case VAProfileH264Main:
    case VAProfileH264High:
        encoder_context->codec = CODEC_H264;

        if (obj_config->entrypoint == VAEntrypointEncSliceLP)
            encoder_context->quality_range = ENCODER_LP_QUALITY_RANGE;
        else if (IS_GEN9(i965->intel.device_info)) {
            encoder_context->quality_level = ENCODER_DEFAULT_QUALITY_AVC;
            encoder_context->quality_range = ENCODER_QUALITY_RANGE_AVC;
        } else
            encoder_context->quality_range = ENCODER_QUALITY_RANGE;
        break;

    case VAProfileH264StereoHigh:
    case VAProfileH264MultiviewHigh:
        if (IS_GEN9(i965->intel.device_info)) {
            encoder_context->quality_level = ENCODER_DEFAULT_QUALITY_AVC;
            encoder_context->quality_range = ENCODER_QUALITY_RANGE_AVC;
        }
        encoder_context->codec = CODEC_H264_MVC;
        break;

    case VAProfileJPEGBaseline:
        encoder_context->codec = CODEC_JPEG;
        break;

    case VAProfileVP8Version0_3:
        encoder_context->codec = CODEC_VP8;
        encoder_context->quality_range = ENCODER_QUALITY_RANGE;

        break;

    case VAProfileHEVCMain:
    case VAProfileHEVCMain10:
        encoder_context->codec = CODEC_HEVC;

        encoder_context->quality_level = ENCODER_DEFAULT_QUALITY_HEVC;
        encoder_context->quality_range = ENCODER_QUALITY_RANGE_HEVC;

        encoder_context->max_slice_or_seg_num = I965_MAX_NUM_SLICE;
        break;

    case VAProfileVP9Profile0:
        encoder_context->codec = CODEC_VP9;
        break;

    default:
        /* Never get here */
        assert(0);
        break;
    }

    for (i = 0; i < obj_config->num_attribs; i++) {
        if (obj_config->attrib_list[i].type == VAConfigAttribRateControl) {
            encoder_context->rate_control_mode = obj_config->attrib_list[i].value;

            if (encoder_context->codec == CODEC_MPEG2 &&
                encoder_context->rate_control_mode & VA_RC_CBR) {
                WARN_ONCE("Don't support CBR for MPEG-2 encoding\n");
                encoder_context->rate_control_mode &= ~VA_RC_CBR;
            }
        }
        if (obj_config->attrib_list[i].type == VAConfigAttribEncROI) {
            if (encoder_context->codec == CODEC_H264)
                encoder_context->context_roi = 1;
        }
        if (obj_config->attrib_list[i].type == VAConfigAttribEncMaxSlices) {
            if (encoder_context->codec == CODEC_H264 ||
                encoder_context->codec == CODEC_HEVC)
                encoder_context->max_slice_or_seg_num = obj_config->attrib_list[i].value;
        }
    }

    if (vme_context_init) {
        vme_context_init(ctx, encoder_context);
        assert(!encoder_context->vme_context ||
               (encoder_context->vme_context_destroy && encoder_context->vme_pipeline));
    }

    mfc_context_init(ctx, encoder_context);
    assert(encoder_context->mfc_context);
    assert(encoder_context->mfc_context_destroy);
    assert(encoder_context->mfc_pipeline);

    return (struct hw_context *)encoder_context;
}

struct hw_context *
gen6_enc_hw_context_init(VADriverContextP ctx, struct object_config *obj_config)
{
    return intel_enc_hw_context_init(ctx, obj_config, gen6_vme_context_init, gen6_mfc_context_init);
}

struct hw_context *
gen7_enc_hw_context_init(VADriverContextP ctx, struct object_config *obj_config)
{

    return intel_enc_hw_context_init(ctx, obj_config, gen7_vme_context_init, gen7_mfc_context_init);
}

struct hw_context *
gen75_enc_hw_context_init(VADriverContextP ctx, struct object_config *obj_config)
{
    return intel_enc_hw_context_init(ctx, obj_config, gen75_vme_context_init, gen75_mfc_context_init);
}

struct hw_context *
gen8_enc_hw_context_init(VADriverContextP ctx, struct object_config *obj_config)
{
    return intel_enc_hw_context_init(ctx, obj_config, gen8_vme_context_init, gen8_mfc_context_init);
}

struct hw_context *
gen9_enc_hw_context_init(VADriverContextP ctx, struct object_config *obj_config)
{
    return intel_enc_hw_context_init(ctx, obj_config, gen9_vme_context_init, gen9_mfc_context_init);
}
