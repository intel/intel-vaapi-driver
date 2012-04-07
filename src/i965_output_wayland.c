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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <va/va_backend.h>
#include <va/va_backend_wayland.h>
#include <wayland-client.h>
#include <wayland-drm-client-protocol.h>
#include "intel_driver.h"
#include "i965_output_wayland.h"
#include "i965_drv_video.h"
#include "i965_defines.h"

struct va_wl_output {
    struct wl_drm      *wl_drm;
};

/* Ensure wl_drm instance is created */
static bool
ensure_wl_output(VADriverContextP ctx)
{
    struct i965_driver_data * const i965 = i965_driver_data(ctx);
    struct va_wl_output * const wl_output = i965->wl_output;
    uint32_t id;

    if (wl_output->wl_drm)
        return true;

    id = wl_display_get_global(ctx->native_dpy, "wl_drm", 1);
    if (!id) {
        wl_display_roundtrip(ctx->native_dpy);
        id = wl_display_get_global(ctx->native_dpy, "wl_drm", 1);
        if (!id)
            return false;
    }

    wl_output->wl_drm = wl_display_bind(ctx->native_dpy, id, &wl_drm_interface);
    if (!wl_output->wl_drm)
        return false;
    return true;
}

/* Hook to return Wayland buffer associated with the VA surface */
static VAStatus
va_GetSurfaceBufferWl(
    struct VADriverContext *ctx,
    VASurfaceID             surface,
    unsigned int            flags,
    struct wl_buffer      **out_buffer
)
{
    struct i965_driver_data * const i965 = i965_driver_data(ctx);
    struct object_surface *obj_surface;
    struct wl_buffer *buffer;
    uint32_t name, drm_format;
    int offsets[3], pitches[3];

    obj_surface = SURFACE(surface);
    if (!obj_surface)
        return VA_STATUS_ERROR_INVALID_SURFACE;

    if (flags != VA_FRAME_PICTURE)
        return VA_STATUS_ERROR_FLAG_NOT_SUPPORTED;

    if (!out_buffer)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    if (!ensure_wl_output(ctx))
        return VA_STATUS_ERROR_INVALID_DISPLAY;

    if (drm_intel_bo_flink(obj_surface->bo, &name) != 0)
        return VA_STATUS_ERROR_INVALID_SURFACE;

    switch (obj_surface->fourcc) {
    case VA_FOURCC('N','V','1','2'):
        drm_format = WL_DRM_FORMAT_NV12;
        offsets[0] = 0;
        pitches[0] = obj_surface->width;
        offsets[1] = obj_surface->width * obj_surface->y_cb_offset;
        pitches[1] = obj_surface->cb_cr_pitch;
        offsets[2] = 0;
        pitches[2] = 0;
        break;
    case VA_FOURCC('Y','V','1','2'):
    case VA_FOURCC('I','4','2','0'):
    case VA_FOURCC('I','M','C','1'):
        switch (obj_surface->subsampling) {
        case SUBSAMPLE_YUV411:
            drm_format = WL_DRM_FORMAT_YUV411;
            break;
        case SUBSAMPLE_YUV420:
            drm_format = WL_DRM_FORMAT_YUV420;
            break;
        case SUBSAMPLE_YUV422H:
        case SUBSAMPLE_YUV422V:
            drm_format = WL_DRM_FORMAT_YUV422;
            break;
        case SUBSAMPLE_YUV444:
            drm_format = WL_DRM_FORMAT_YUV444;
            break;
        default:
            assert(0 && "unsupported subsampling");
            return VA_STATUS_ERROR_INVALID_IMAGE_FORMAT;
        }
        offsets[0] = 0;
        pitches[0] = obj_surface->width;
        offsets[1] = obj_surface->width * obj_surface->y_cb_offset;
        pitches[1] = obj_surface->cb_cr_pitch;
        offsets[2] = obj_surface->width * obj_surface->y_cr_offset;
        pitches[2] = obj_surface->cb_cr_pitch;
        break;
    default:
        assert(0 && "unsupported format");
        return VA_STATUS_ERROR_INVALID_IMAGE_FORMAT;
    }

    buffer = wl_drm_create_planar_buffer(
        i965->wl_output->wl_drm,
        name,
        obj_surface->orig_width,
        obj_surface->orig_height,
        drm_format,
        offsets[0], pitches[0],
        offsets[1], pitches[1],
        offsets[2], pitches[2]
    );
    if (!buffer)
        return VA_STATUS_ERROR_ALLOCATION_FAILED;

    *out_buffer = buffer;
    return VA_STATUS_SUCCESS;
}

/* Hook to return Wayland buffer associated with the VA image */
static VAStatus
va_GetImageBufferWl(
    struct VADriverContext *ctx,
    VAImageID               image,
    unsigned int            flags,
    struct wl_buffer      **out_buffer
)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

bool
i965_output_wayland_init(VADriverContextP ctx)
{
    struct i965_driver_data * const i965 = i965_driver_data(ctx);
    struct VADriverVTableWayland *vtable;

    if (ctx->display_type != VA_DISPLAY_WAYLAND)
        return false;

    i965->wl_output = calloc(1, sizeof(struct va_wl_output));
    if (!i965->wl_output)
        return false;

    vtable = ctx->vtable_wayland;
    if (!vtable)
        return false;

    vtable->vaGetSurfaceBufferWl        = va_GetSurfaceBufferWl;
    vtable->vaGetImageBufferWl          = va_GetImageBufferWl;
    return true;
}

void
i965_output_wayland_terminate(VADriverContextP ctx)
{
    struct i965_driver_data * const i965 = i965_driver_data(ctx);
    struct va_wl_output *wl_output;

    if (ctx->display_type != VA_DISPLAY_WAYLAND)
        return;

    wl_output = i965->wl_output;
    if (!wl_output)
        return;

    if (wl_output->wl_drm) {
        wl_drm_destroy(wl_output->wl_drm);
        wl_output->wl_drm = NULL;
    }
    free(wl_output);
    i965->wl_output = NULL;
}
