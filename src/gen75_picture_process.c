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

#include "intel_batchbuffer.h"
#include "intel_driver.h"
#include "i965_defines.h"
#include "i965_structs.h"

#include "i965_drv_video.h"
#include "i965_post_processing.h"
#include "gen75_vpp_vebox.h"
#include "gen75_picture_process.h"

extern void 
i965_proc_picture(VADriverContextP ctx, 
                  VAProfile profile, 
                  union codec_state *codec_state,
                  struct hw_context *hw_context);

void
gen75_proc_picture(VADriverContextP ctx,
                          VAProfile profile,
                          union codec_state *codec_state,
                          struct hw_context *hw_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct proc_state* proc_st = &(codec_state->proc);
    struct intel_video_process_context *proc_ctx = 
             (struct intel_video_process_context *)hw_context;
    VAProcPipelineParameterBuffer* pipeline_param = 
             (VAProcPipelineParameterBuffer *)proc_st->pipeline_param->buffer;
    unsigned int i = 0, is_vebox_used = 0;
 
    proc_ctx->pipeline_param = pipeline_param;
    proc_ctx->surface_input  = pipeline_param->surface;
    proc_ctx->surface_output = proc_st->current_render_target;
    assert(proc_ctx->surface_input != VA_INVALID_SURFACE &&
           proc_ctx->surface_output != VA_INVALID_SURFACE);
 
    struct object_surface * obj_surf = SURFACE(proc_ctx->surface_output);
    if(!obj_surf->bo){
       unsigned int is_tiled = 0;
       unsigned int fourcc = VA_FOURCC('Y','V','1','2');
       int sampling = SUBSAMPLE_YUV420;
       i965_check_alloc_surface_bo(ctx, obj_surf, is_tiled, fourcc, sampling);
    }  

    VABufferID *filter_id = (VABufferID*)pipeline_param->filters ;
    for( i = 0; i < pipeline_param->num_filters; i++ ){
          struct object_buffer * obj_buf = BUFFER((*filter_id) + i);
          VAProcFilterParameterBuffer* filter =
          (VAProcFilterParameterBuffer*)obj_buf-> buffer_store->buffer;

          if(filter->type == VAProcFilterNoiseReduction   ||
             filter->type == VAProcFilterDeinterlacing    ||
             filter->type == VAProcFilterColorBalance     ||
             filter->type == VAProcFilterColorStandard ){

             is_vebox_used = 1;
             break;
        }
    } 

    if(is_vebox_used){
        /* vpp features based on VEBox fixed function */
       if(proc_ctx->vpp_vebox_ctx == NULL)
          proc_ctx->vpp_vebox_ctx = gen75_vebox_context_init(ctx);

        proc_ctx->vpp_vebox_ctx->surface_input   = proc_ctx->surface_input;
        proc_ctx->vpp_vebox_ctx->surface_output  = proc_ctx->surface_output;
        proc_ctx->vpp_vebox_ctx->pipeline_param  = proc_ctx->pipeline_param;
  
        gen75_vebox_process_picture(ctx, proc_ctx->vpp_vebox_ctx);
 
     } else {
      /* implicity surface format coversion and scaling */
       if(proc_ctx->vpp_fmt_cvt_ctx == NULL)
            proc_ctx->vpp_fmt_cvt_ctx = i965_proc_context_init(ctx, NULL);
       
       i965_proc_picture(ctx, profile, codec_state,proc_ctx->vpp_fmt_cvt_ctx);

    }
}

void
gen75_proc_context_destroy(void *hw_context)
{
    struct intel_video_process_context *proc_ctx = (struct intel_video_process_context *)hw_context;
    VADriverContextP ctx = (VADriverContextP)(proc_ctx->driver_context);

    if(proc_ctx->vpp_fmt_cvt_ctx){
        proc_ctx->vpp_fmt_cvt_ctx->base.destroy(proc_ctx->vpp_fmt_cvt_ctx);
        proc_ctx->vpp_fmt_cvt_ctx = NULL;
    }

    if(proc_ctx->vpp_vebox_ctx){
       gen75_vebox_context_destroy(ctx,proc_ctx->vpp_vebox_ctx);
       proc_ctx->vpp_vebox_ctx = NULL;
    }

    free(proc_ctx);
}

struct hw_context *
gen75_proc_context_init(VADriverContextP ctx, struct object_config *obj_config)
{
   struct intel_video_process_context *proc_context 
           = calloc(1, sizeof(struct intel_video_process_context));

    proc_context->base.destroy = gen75_proc_context_destroy;
    proc_context->base.run     = gen75_proc_picture;

    proc_context->vpp_vebox_ctx    = NULL;
    proc_context->vpp_fmt_cvt_ctx  = NULL;
 
    proc_context->surface_input  = -1;
    proc_context->surface_output = -1;
    proc_context->driver_context = ctx;

    return (struct hw_context *)proc_context;
}

