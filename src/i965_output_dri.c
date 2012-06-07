/*
 * Copyright (C) 2012 Intel Corporation. All Rights Reserved.
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
 */

#include "config.h"
#include <string.h>
#include <assert.h>
#include <va/va_dricommon.h>
#include "i965_drv_video.h"
#include "i965_output_dri.h"

bool
i965_output_dri_init(VADriverContextP ctx)
{
    return true;
}

void
i965_output_dri_terminate(VADriverContextP ctx)
{
}

VAStatus
i965_put_surface_dri(
    VADriverContextP    ctx,
    VASurfaceID         surface,
    void               *draw,
    const VARectangle  *src_rect,
    const VARectangle  *dst_rect,
    const VARectangle  *cliprects,
    unsigned int        num_cliprects,
    unsigned int        flags
)
{
    struct i965_driver_data * const i965 = i965_driver_data(ctx); 
    struct dri_state * const dri_state = (struct dri_state *)ctx->drm_state;
    struct i965_render_state * const render_state = &i965->render_state;
    struct dri_drawable *dri_drawable;
    union dri_buffer *buffer;
    struct intel_region *dest_region;
    struct object_surface *obj_surface; 
    unsigned int pp_flag = 0;
    bool new_region = false;
    uint32_t name;
    int ret;

    /* Currently don't support DRI1 */
    if (dri_state->base.auth_type != VA_DRM_AUTH_DRI2)
        return VA_STATUS_ERROR_UNKNOWN;

    /* Some broken sources such as H.264 conformance case FM2_SVA_C
     * will get here
     */
    obj_surface = SURFACE(surface);
    if (!obj_surface || !obj_surface->bo)
        return VA_STATUS_SUCCESS;

    _i965LockMutex(&i965->render_mutex);

    dri_drawable = dri_get_drawable(ctx, (Drawable)draw);
    assert(dri_drawable);

    buffer = dri_get_rendering_buffer(ctx, dri_drawable);
    assert(buffer);
    
    dest_region = render_state->draw_region;

    if (dest_region) {
        assert(dest_region->bo);
        dri_bo_flink(dest_region->bo, &name);
        
        if (buffer->dri2.name != name) {
            new_region = True;
            dri_bo_unreference(dest_region->bo);
        }
    } else {
        dest_region = (struct intel_region *)calloc(1, sizeof(*dest_region));
        assert(dest_region);
        render_state->draw_region = dest_region;
        new_region = True;
    }

    if (new_region) {
        dest_region->x = dri_drawable->x;
        dest_region->y = dri_drawable->y;
        dest_region->width = dri_drawable->width;
        dest_region->height = dri_drawable->height;
        dest_region->cpp = buffer->dri2.cpp;
        dest_region->pitch = buffer->dri2.pitch;

        dest_region->bo = intel_bo_gem_create_from_name(i965->intel.bufmgr, "rendering buffer", buffer->dri2.name);
        assert(dest_region->bo);

        ret = dri_bo_get_tiling(dest_region->bo, &(dest_region->tiling), &(dest_region->swizzle));
        assert(ret == 0);
    }

    if ((flags & VA_FILTER_SCALING_MASK) == VA_FILTER_SCALING_NL_ANAMORPHIC)
        pp_flag |= I965_PP_FLAG_AVS;

    if (flags & VA_TOP_FIELD)
        pp_flag |= I965_PP_FLAG_TOP_FIELD;
    else if (flags & VA_BOTTOM_FIELD)
        pp_flag |= I965_PP_FLAG_BOTTOM_FIELD;

    intel_render_put_surface(ctx, surface, src_rect, dst_rect, pp_flag);

    if(obj_surface->subpic != VA_INVALID_ID) {
        intel_render_put_subpicture(ctx, surface, src_rect, dst_rect);
    }

    dri_swap_buffer(ctx, dri_drawable);
    obj_surface->flags |= SURFACE_DISPLAYED;

    if ((obj_surface->flags & SURFACE_ALL_MASK) == SURFACE_DISPLAYED) {
        dri_bo_unreference(obj_surface->bo);
        obj_surface->bo = NULL;
        obj_surface->flags &= ~SURFACE_REF_DIS_MASK;

        if (obj_surface->free_private_data)
            obj_surface->free_private_data(&obj_surface->private_data);
    }

    _i965UnlockMutex(&i965->render_mutex);

    return VA_STATUS_SUCCESS;
}
