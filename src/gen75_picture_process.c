/*
 * Copyright © 2011 Intel Corporation
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
 *   Li Xiaowei <xiaowei.a.li@intel.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "intel_batchbuffer.h"
#include "intel_driver.h"
#include "i965_defines.h"
#include "i965_structs.h"

#include "i965_drv_video.h"
#include "i965_post_processing.h"
#include "gen75_picture_process.h"
#include "gen8_post_processing.h"
#include "intel_gen_vppapi.h"

extern struct hw_context *
i965_proc_context_init(VADriverContextP ctx,
                       struct object_config *obj_config);

static VAStatus
gen75_vpp_fmt_cvt(VADriverContextP ctx,
                  VAProfile profile,
                  union codec_state *codec_state,
                  struct hw_context *hw_context)
{
    VAStatus va_status = VA_STATUS_SUCCESS;
    struct intel_video_process_context *proc_ctx =
        (struct intel_video_process_context *)hw_context;

    va_status = i965_proc_picture(ctx, profile, codec_state,
                                  proc_ctx->vpp_fmt_cvt_ctx);

    return va_status;
}

static VAStatus
gen75_vpp_vebox(VADriverContextP ctx,
                struct intel_video_process_context* proc_ctx)
{
    VAStatus va_status = VA_STATUS_ERROR_UNIMPLEMENTED;
    VAProcPipelineParameterBuffer* pipeline_param = proc_ctx->pipeline_param;
    struct i965_driver_data *i965 = i965_driver_data(ctx);

    /* vpp features based on VEBox fixed function */
    if (proc_ctx->vpp_vebox_ctx == NULL) {
        proc_ctx->vpp_vebox_ctx = gen75_vebox_context_init(ctx);
    }

    proc_ctx->vpp_vebox_ctx->pipeline_param  = pipeline_param;
    proc_ctx->vpp_vebox_ctx->surface_input_object = proc_ctx->surface_pipeline_input_object;
    proc_ctx->vpp_vebox_ctx->surface_output_object  = proc_ctx->surface_render_output_object;

    if (IS_HASWELL(i965->intel.device_info))
        va_status = gen75_vebox_process_picture(ctx, proc_ctx->vpp_vebox_ctx);
    else if (IS_GEN8(i965->intel.device_info))
        va_status = gen8_vebox_process_picture(ctx, proc_ctx->vpp_vebox_ctx);
    else if (IS_GEN9(i965->intel.device_info))
        va_status = gen9_vebox_process_picture(ctx, proc_ctx->vpp_vebox_ctx);
    else if (IS_GEN10(i965->intel.device_info))
        va_status = gen10_vebox_process_picture(ctx, proc_ctx->vpp_vebox_ctx);

    return va_status;
}

static void
rgb_to_yuv(unsigned int argb,
           unsigned char *y,
           unsigned char *u,
           unsigned char *v,
           unsigned char *a)
{
    int r = ((argb >> 16) & 0xff);
    int g = ((argb >> 8) & 0xff);
    int b = ((argb >> 0) & 0xff);

    *y = (257 * r + 504 * g + 98 * b) / 1000 + 16;
    *v = (439 * r - 368 * g - 71 * b) / 1000 + 128;
    *u = (-148 * r - 291 * g + 439 * b) / 1000 + 128;
    *a = ((argb >> 24) & 0xff);
}

static void
gen8plus_vpp_clear_surface(VADriverContextP ctx,
                           struct i965_post_processing_context *pp_context,
                           struct object_surface *obj_surface,
                           unsigned int color)
{
    unsigned char y, u, v, a = 0;

    if (!obj_surface ||
        !obj_surface->bo ||
        !(color & 0xFF000000))
        return;

    if (obj_surface->fourcc == VA_FOURCC_RGBA ||
        obj_surface->fourcc == VA_FOURCC_RGBX ||
        obj_surface->fourcc == VA_FOURCC_BGRA ||
        obj_surface->fourcc == VA_FOURCC_BGRX)
        intel_common_clear_surface(ctx, pp_context, obj_surface, color);
    else {
        rgb_to_yuv(color, &y, &u, &v, &a);
        intel_common_clear_surface(ctx, pp_context, obj_surface,
                                   a << 24 | y << 16 | v << 8 | u);
    }
}

VAStatus
gen75_proc_picture(VADriverContextP ctx,
                   VAProfile profile,
                   union codec_state *codec_state,
                   struct hw_context *hw_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct proc_state* proc_st = &(codec_state->proc);
    struct intel_video_process_context *proc_ctx =
        (struct intel_video_process_context *)hw_context;
    VAProcPipelineParameterBuffer *pipeline_param =
        (VAProcPipelineParameterBuffer *)proc_st->pipeline_param->buffer;
    struct object_surface *obj_dst_surf = NULL;
    struct object_surface *obj_src_surf = NULL;

    VAProcPipelineParameterBuffer pipeline_param2;
    struct object_surface *stage1_dst_surf = NULL;
    struct object_surface *stage2_dst_surf = NULL;
    VARectangle src_rect, dst_rect;
    VASurfaceID tmp_surfaces[2];
    VASurfaceID out_surface_id1 = VA_INVALID_ID, out_surface_id2 = VA_INVALID_ID;
    int num_tmp_surfaces = 0;

    VAStatus status;

    proc_ctx->pipeline_param = pipeline_param;

    if (proc_st->current_render_target == VA_INVALID_SURFACE ||
        pipeline_param->surface == VA_INVALID_SURFACE) {
        status = VA_STATUS_ERROR_INVALID_SURFACE;
        goto error;
    }

    obj_dst_surf = SURFACE(proc_st->current_render_target);

    if (!obj_dst_surf) {
        status = VA_STATUS_ERROR_INVALID_SURFACE;
        goto error;
    }

    obj_src_surf = SURFACE(proc_ctx->pipeline_param->surface);

    if (!obj_src_surf) {
        status = VA_STATUS_ERROR_INVALID_SURFACE;
        goto error;
    }

    if (!obj_src_surf->bo) {
        status = VA_STATUS_ERROR_INVALID_VALUE; /* The input surface is created without valid content */
        goto error;
    }

    if (pipeline_param->num_filters && !pipeline_param->filters) {
        status = VA_STATUS_ERROR_INVALID_PARAMETER;
        goto error;
    }

    if (pipeline_param->num_filters == 0 || pipeline_param->filters == NULL) {
        /* explicitly initialize the VPP based on Render ring */
        if (proc_ctx->vpp_fmt_cvt_ctx == NULL)
            proc_ctx->vpp_fmt_cvt_ctx = i965_proc_context_init(ctx, NULL);
    }

    if (!obj_dst_surf->bo) {
        unsigned int is_tiled = 1;
        unsigned int fourcc = VA_FOURCC_NV12;
        int sampling = SUBSAMPLE_YUV420;

        if (obj_dst_surf->expected_format == VA_RT_FORMAT_YUV420_10BPP)
            fourcc = VA_FOURCC_P010;

        i965_check_alloc_surface_bo(ctx, obj_dst_surf, is_tiled, fourcc, sampling);
    }

    if (pipeline_param->surface_region) {
        src_rect.x = pipeline_param->surface_region->x;
        src_rect.y = pipeline_param->surface_region->y;
        src_rect.width = pipeline_param->surface_region->width;
        src_rect.height = pipeline_param->surface_region->height;
    } else {
        src_rect.x = 0;
        src_rect.y = 0;
        src_rect.width = obj_src_surf->orig_width;
        src_rect.height = obj_src_surf->orig_height;
    }

    if (pipeline_param->output_region) {
        dst_rect.x = pipeline_param->output_region->x;
        dst_rect.y = pipeline_param->output_region->y;
        dst_rect.width = pipeline_param->output_region->width;
        dst_rect.height = pipeline_param->output_region->height;
    } else {
        dst_rect.x = 0;
        dst_rect.y = 0;
        dst_rect.width = obj_dst_surf->orig_width;
        dst_rect.height = obj_dst_surf->orig_height;
    }

    if (pipeline_param->num_filters == 0 || pipeline_param->filters == NULL) {
        VAStatus status = VA_STATUS_ERROR_UNIMPLEMENTED;
        struct i965_proc_context *gpe_proc_ctx;
        struct i965_surface src_surface, dst_surface;

        gpe_proc_ctx = (struct i965_proc_context *)proc_ctx->vpp_fmt_cvt_ctx;
        assert(gpe_proc_ctx != NULL); // gpe_proc_ctx must be a non-NULL pointer

        if ((gpe_proc_ctx->pp_context.scaling_gpe_context_initialized & VPPGPE_8BIT_8BIT) &&
            (pipeline_param->output_background_color & 0xFF000000))
            gen8plus_vpp_clear_surface(ctx,
                                       &gpe_proc_ctx->pp_context,
                                       obj_dst_surf,
                                       pipeline_param->output_background_color);

        src_surface.base = (struct object_base *)obj_src_surf;
        src_surface.type = I965_SURFACE_TYPE_SURFACE;
        dst_surface.base = (struct object_base *)obj_dst_surf;
        dst_surface.type = I965_SURFACE_TYPE_SURFACE;

        status = intel_common_scaling_post_processing(ctx,
                                                      &gpe_proc_ctx->pp_context,
                                                      &src_surface, &src_rect,
                                                      &dst_surface, &dst_rect);

        if (status != VA_STATUS_ERROR_UNIMPLEMENTED)
            return status;
    }

    proc_ctx->surface_render_output_object = obj_dst_surf;
    proc_ctx->surface_pipeline_input_object = obj_src_surf;
    assert(pipeline_param->num_filters <= 4);

    int vpp_stage1 = 0, vpp_stage2 = 1, vpp_stage3 = 0;


    if (obj_src_surf->fourcc == VA_FOURCC_P010) {
        vpp_stage1 = 1;
        vpp_stage2 = 0;
        vpp_stage3 = 0;
        if (pipeline_param->num_filters == 0 || pipeline_param->filters == NULL) {
            if (src_rect.x != dst_rect.x ||
                src_rect.y != dst_rect.y ||
                src_rect.width != dst_rect.width ||
                src_rect.height != dst_rect.height)
                vpp_stage2 = 1;

            if (obj_dst_surf->fourcc != VA_FOURCC_NV12 &&
                obj_dst_surf->fourcc != VA_FOURCC_P010)
                vpp_stage2 = 1;
        } else
            vpp_stage2 = 1;

        if (vpp_stage2 == 1) {
            if (obj_dst_surf->fourcc == VA_FOURCC_P010)
                vpp_stage3 = 1;
        }
    } else if (obj_dst_surf->fourcc == VA_FOURCC_P010) {
        vpp_stage2 = 1;
        vpp_stage3 = 1;

        if ((obj_src_surf->fourcc == VA_FOURCC_NV12) &&
            (pipeline_param->num_filters == 0 || pipeline_param->filters == NULL)) {
            if ((src_rect.x == dst_rect.x) &&
                (src_rect.y == dst_rect.y) &&
                (src_rect.width == dst_rect.width) &&
                (src_rect.height == dst_rect.height))
                vpp_stage2 = 0;
        }
    }

    if (vpp_stage1 == 1) {
        memset((void *)&pipeline_param2, 0, sizeof(pipeline_param2));
        pipeline_param2.surface = pipeline_param->surface;
        pipeline_param2.surface_region = &src_rect;
        pipeline_param2.output_region = &src_rect;
        pipeline_param2.filter_flags = 0;
        pipeline_param2.num_filters  = 0;

        proc_ctx->pipeline_param = &pipeline_param2;

        if (vpp_stage2 == 1) {
            status = i965_CreateSurfaces(ctx,
                                         obj_src_surf->orig_width,
                                         obj_src_surf->orig_height,
                                         VA_RT_FORMAT_YUV420,
                                         1,
                                         &out_surface_id1);
            assert(status == VA_STATUS_SUCCESS);
            tmp_surfaces[num_tmp_surfaces++] = out_surface_id1;
            stage1_dst_surf = SURFACE(out_surface_id1);
            assert(stage1_dst_surf);
            i965_check_alloc_surface_bo(ctx, stage1_dst_surf, 1, VA_FOURCC_NV12, SUBSAMPLE_YUV420);

            proc_ctx->surface_render_output_object = stage1_dst_surf;
        }

        gen75_vpp_vebox(ctx, proc_ctx);
    }

    if ((vpp_stage3 == 1) && (vpp_stage2 == 1)) {
        status = i965_CreateSurfaces(ctx,
                                     obj_dst_surf->orig_width,
                                     obj_dst_surf->orig_height,
                                     VA_RT_FORMAT_YUV420,
                                     1,
                                     &out_surface_id2);
        assert(status == VA_STATUS_SUCCESS);
        tmp_surfaces[num_tmp_surfaces++] = out_surface_id2;
        stage2_dst_surf = SURFACE(out_surface_id2);
        assert(stage2_dst_surf);
        i965_check_alloc_surface_bo(ctx, stage2_dst_surf, 1, VA_FOURCC_NV12, SUBSAMPLE_YUV420);
    }

    VABufferID *filter_id = (VABufferID*) pipeline_param->filters;

    if (vpp_stage2 == 1) {
        if (stage1_dst_surf != NULL) {
            proc_ctx->surface_pipeline_input_object = stage1_dst_surf;
            proc_ctx->surface_render_output_object = obj_dst_surf;

            pipeline_param->surface = out_surface_id1;
        }

        if (stage2_dst_surf != NULL) {
            proc_ctx->surface_render_output_object = stage2_dst_surf;

            proc_st->current_render_target = out_surface_id2;
        }

        proc_ctx->pipeline_param = pipeline_param;

        if (pipeline_param->num_filters == 0 || pipeline_param->filters == NULL) {
            /* implicity surface format coversion and scaling */

            status = gen75_vpp_fmt_cvt(ctx, profile, codec_state, hw_context);
            if (status != VA_STATUS_SUCCESS)
                goto error;
        } else if (pipeline_param->num_filters == 1) {
            struct object_buffer * obj_buf = BUFFER((*filter_id) + 0);

            assert(obj_buf && obj_buf->buffer_store && obj_buf->buffer_store->buffer);

            if (!obj_buf ||
                !obj_buf->buffer_store ||
                !obj_buf->buffer_store->buffer) {
                status = VA_STATUS_ERROR_INVALID_FILTER_CHAIN;
                goto error;
            }

            VAProcFilterParameterBuffer* filter =
                (VAProcFilterParameterBuffer*)obj_buf-> buffer_store->buffer;

            if (filter->type == VAProcFilterNoiseReduction         ||
                filter->type == VAProcFilterDeinterlacing          ||
                filter->type == VAProcFilterSkinToneEnhancement    ||
                filter->type == VAProcFilterSharpening             ||
                filter->type == VAProcFilterColorBalance) {
                gen75_vpp_vebox(ctx, proc_ctx);
            }
        } else if (pipeline_param->num_filters >= 2) {
            unsigned int i = 0;
            for (i = 0; i < pipeline_param->num_filters; i++) {
                struct object_buffer * obj_buf = BUFFER(pipeline_param->filters[i]);

                if (!obj_buf ||
                    !obj_buf->buffer_store ||
                    !obj_buf->buffer_store->buffer) {
                    status = VA_STATUS_ERROR_INVALID_FILTER_CHAIN;
                    goto error;
                }

                VAProcFilterParameterBuffer* filter =
                    (VAProcFilterParameterBuffer*)obj_buf-> buffer_store->buffer;

                if (filter->type != VAProcFilterNoiseReduction       &&
                    filter->type != VAProcFilterDeinterlacing        &&
                    filter->type != VAProcFilterSkinToneEnhancement  &&
                    filter->type != VAProcFilterColorBalance) {
                    fprintf(stderr, "Do not support multiply filters outside vebox pipeline \n");
                    assert(0);
                }
            }
            gen75_vpp_vebox(ctx, proc_ctx);
        }
    }

    if (vpp_stage3 == 1) {
        if (vpp_stage2 == 1) {
            memset(&pipeline_param2, 0, sizeof(pipeline_param2));
            pipeline_param2.surface = out_surface_id2;
            pipeline_param2.surface_region = &dst_rect;
            pipeline_param2.output_region = &dst_rect;
            pipeline_param2.filter_flags = 0;
            pipeline_param2.num_filters  = 0;

            proc_ctx->pipeline_param = &pipeline_param2;
            proc_ctx->surface_pipeline_input_object = proc_ctx->surface_render_output_object;
            proc_ctx->surface_render_output_object = obj_dst_surf;
        }

        gen75_vpp_vebox(ctx, proc_ctx);
    }

    if (num_tmp_surfaces)
        i965_DestroySurfaces(ctx,
                             tmp_surfaces,
                             num_tmp_surfaces);

    return VA_STATUS_SUCCESS;

error:
    if (num_tmp_surfaces)
        i965_DestroySurfaces(ctx,
                             tmp_surfaces,
                             num_tmp_surfaces);

    return status;
}

static void
gen75_proc_context_destroy(void *hw_context)
{
    struct intel_video_process_context *proc_ctx =
        (struct intel_video_process_context *)hw_context;
    VADriverContextP ctx = (VADriverContextP)(proc_ctx->driver_context);

    if (proc_ctx->vpp_fmt_cvt_ctx) {
        proc_ctx->vpp_fmt_cvt_ctx->destroy(proc_ctx->vpp_fmt_cvt_ctx);
        proc_ctx->vpp_fmt_cvt_ctx = NULL;
    }

    if (proc_ctx->vpp_vebox_ctx) {
        gen75_vebox_context_destroy(ctx, proc_ctx->vpp_vebox_ctx);
        proc_ctx->vpp_vebox_ctx = NULL;
    }

    free(proc_ctx);
}

struct hw_context *
gen75_proc_context_init(VADriverContextP ctx,
                        struct object_config *obj_config)
{
    struct intel_video_process_context *proc_context
        = calloc(1, sizeof(struct intel_video_process_context));

    assert(proc_context);
    proc_context->base.destroy = gen75_proc_context_destroy;
    proc_context->base.run     = gen75_proc_picture;

    proc_context->vpp_vebox_ctx    = NULL;
    proc_context->vpp_fmt_cvt_ctx  = NULL;

    proc_context->driver_context = ctx;

    return (struct hw_context *)proc_context;
}

