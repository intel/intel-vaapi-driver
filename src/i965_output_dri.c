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

#include "sysdeps.h"

#include <va/va_dricommon.h>

#include "i965_drv_video.h"
#include "i965_output_dri.h"
#include "dso_utils.h"

#include <X11/Xlib-xcb.h>

#define LIBVA_X11_NAME "libva-x11.so.2"

#ifdef HAVE_VA_DRI3
typedef int (*dri3_createfd_func)(VADriverContextP ctx,
                                  Pixmap pixmap, int *stride);
typedef Pixmap
(*dri3_createPixmap_func)(VADriverContextP ctx, Drawable draw,
                          int width, int height, int depth,
                          int fd, int bpp, int stride, int size);
typedef void
(*dri3_presentPixmap_func)(VADriverContextP ctx, Drawable draw,
                           Pixmap pixmap,
                           unsigned int serial,
                           xcb_xfixes_region_t valid,
                           xcb_xfixes_region_t update,
                           unsigned short int x_off,
                           unsigned short int y_off,
                           xcb_randr_crtc_t target_crtc,
                           xcb_sync_fence_t wait_fence,
                           xcb_sync_fence_t idle_fence,
                           unsigned int options,
                           unsigned long int target_msc,
                           unsigned long int divisor,
                           unsigned long int  remainder,
                           unsigned int notifies_len,
                           const xcb_present_notify_t *notifies);
typedef int
(*dri3_createfence_func)(VADriverContextP ctx, Pixmap pixmap,
                         struct dri3_fence *fence);
typedef void
(*dri3_freefence_func)(VADriverContextP ctx,
                       struct dri3_fence *fence);
typedef void (*dri3_syncfence_func)(VADriverContextP ctx,
                                    struct dri3_fence *fence);
#else
typedef struct dri_drawable *(*dri_get_drawable_func)(VADriverContextP ctx, XID drawable);
typedef union dri_buffer *(*dri_get_rendering_buffer_func)(VADriverContextP ctx, struct dri_drawable *d);
typedef void (*dri_swap_buffer_func)(VADriverContextP ctx, struct dri_drawable *d);
#endif

struct dri_vtable {
#ifdef HAVE_VA_DRI3
    dri3_createPixmap_func              createPixmap;
    dri3_createfd_func                  createfd;
    dri3_presentPixmap_func             presentPixmap;
    dri3_createfence_func               createfence;
    dri3_freefence_func                 freefence;
    dri3_syncfence_func                 syncfence;
#else
    dri_get_drawable_func               get_drawable;
    dri_get_rendering_buffer_func       get_rendering_buffer;
    dri_swap_buffer_func                swap_buffer;
#endif
};

struct va_dri_output {
    struct dso_handle  *handle;
    struct dri_vtable   vtable;
};

bool
i965_output_dri_init(VADriverContextP ctx)
{
    struct i965_driver_data * const i965 = i965_driver_data(ctx);
    struct dso_handle *dso_handle;
    struct dri_vtable *dri_vtable;

    static const struct dso_symbol symbols[] = {
#ifdef HAVE_VA_DRI3
        {
            "va_dri3_createPixmap",
            offsetof(struct dri_vtable, createPixmap)
        },
        {
            "va_dri3_createfd",
            offsetof(struct dri_vtable, createfd)
        },
        {
            "va_dri3_presentPixmap",
            offsetof(struct dri_vtable, presentPixmap)
        },
        {
            "va_dri3_create_fence",
            offsetof(struct dri_vtable, createfence)
        },
        {
            "va_dri3_fence_free",
            offsetof(struct dri_vtable, freefence)
        },
        {
            "va_dri3_fence_sync",
            offsetof(struct dri_vtable, syncfence)
        },
        { NULL, }
#else
        {
            "va_dri_get_drawable",
            offsetof(struct dri_vtable, get_drawable)
        },
        {
            "va_dri_get_rendering_buffer",
            offsetof(struct dri_vtable, get_rendering_buffer)
        },
        {
            "va_dri_swap_buffer",
            offsetof(struct dri_vtable, swap_buffer)
        },
        { NULL, }
#endif
    };

    i965->dri_output = calloc(1, sizeof(struct va_dri_output));
    if (!i965->dri_output)
        goto error;

    i965->dri_output->handle = dso_open(LIBVA_X11_NAME);
    if (!i965->dri_output->handle)
        goto error;

    dso_handle = i965->dri_output->handle;
    dri_vtable = &i965->dri_output->vtable;
    if (!dso_get_symbols(dso_handle, dri_vtable, sizeof(*dri_vtable), symbols))
        goto error;
    return true;

error:
    i965_output_dri_terminate(ctx);
    return false;
}

void
i965_output_dri_terminate(VADriverContextP ctx)
{
    struct i965_driver_data * const i965 = i965_driver_data(ctx);
    struct va_dri_output * const dri_output = i965->dri_output;

    if (!dri_output)
        return;

    if (dri_output->handle) {
        dso_close(dri_output->handle);
        dri_output->handle = NULL;
    }

    free(dri_output);
    i965->dri_output = NULL;
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
    struct dri_vtable * const dri_vtable = &i965->dri_output->vtable;
    struct i965_render_state * const render_state = &i965->render_state;
    struct intel_region *dest_region;
    struct object_surface *obj_surface;
    int i, ret;
#ifndef HAVE_VA_DRI3
    struct dri_drawable *dri_drawable;
    union dri_buffer *buffer;
    uint32_t name;
#else
    int bpp, x, y, fd;
    Window root;
    Pixmap pixmap;
    struct dri3_fence fence;
    unsigned int border, depth, stride, size, width, height;
#endif

    /* Currently don't support DRI1 */
    if (!(VA_CHECK_DRM_AUTH_TYPE(ctx, VA_DRM_AUTH_DRI2) ||
            VA_CHECK_DRM_AUTH_TYPE(ctx, VA_DRM_AUTH_CUSTOM)))
        return VA_STATUS_ERROR_UNKNOWN;

    /* Some broken sources such as H.264 conformance case FM2_SVA_C
     * will get here
     */
    obj_surface = SURFACE(surface);
    ASSERT_RET(obj_surface && obj_surface->bo, VA_STATUS_SUCCESS);
    ASSERT_RET(obj_surface->fourcc != VA_FOURCC_YUY2 &&
               obj_surface->fourcc != VA_FOURCC_UYVY &&
               obj_surface->fourcc != VA_FOURCC_RGBX &&
               obj_surface->fourcc != VA_FOURCC_BGRX,
               VA_STATUS_ERROR_UNIMPLEMENTED);

    _i965LockMutex(&i965->render_mutex);

#ifndef HAVE_VA_DRI3
    dri_drawable = dri_vtable->get_drawable(ctx, (Drawable)draw);
    ASSERT_RET(dri_drawable, VA_STATUS_ERROR_ALLOCATION_FAILED);

    buffer = dri_vtable->get_rendering_buffer(ctx, dri_drawable);
    ASSERT_RET(buffer, VA_STATUS_ERROR_ALLOCATION_FAILED);
#endif

    dest_region = render_state->draw_region;
    if (dest_region == NULL) {
        dest_region = (struct intel_region *)calloc(1, sizeof(*dest_region));
        ASSERT_RET(dest_region, VA_STATUS_ERROR_ALLOCATION_FAILED);
        render_state->draw_region = dest_region;
    }

#ifdef HAVE_VA_DRI3
    XGetGeometry(ctx->native_dpy, (Window)draw, &root,
                 &x, &y, &width, &height, &border, &depth);

    switch(depth) {
        case 8: bpp = 8; break;
        case 15: case 16: bpp = 16; break;
        case 24: case 32: bpp = 32; break;
        default: return VA_STATUS_ERROR_INVALID_VALUE;
    }

    stride = obj_surface->width * bpp/8;
    size = ALIGN((stride * obj_surface->height) , 0x1000);

    dest_region->cpp = bpp/8;
    dest_region->pitch = stride;
#else
    if (dest_region->bo) {
        dri_bo_flink(dest_region->bo, &name);
        if (buffer->dri2.name != name) {
            dri_bo_unreference(dest_region->bo);
            dest_region->bo = NULL;
        }
    }
#endif

    if (dest_region->bo == NULL) {
#ifdef HAVE_VA_DRI3
        dest_region->bo =
            drm_intel_bo_alloc(i965->intel.bufmgr,
                               "prime", size, 0x1000);
#else
        dest_region->cpp = buffer->dri2.cpp;
        dest_region->pitch = buffer->dri2.pitch;
        dest_region->bo =
            intel_bo_gem_create_from_name(i965->intel.bufmgr,
                                          "rendering buffer",
                                          buffer->dri2.name);
#endif
        ASSERT_RET(dest_region->bo, VA_STATUS_ERROR_ALLOCATION_FAILED);

        ret = dri_bo_get_tiling(dest_region->bo,
                                &(dest_region->tiling),
                                &(dest_region->swizzle));
        ASSERT_RET((ret == 0), VA_STATUS_ERROR_UNKNOWN);
    }

#ifdef HAVE_VA_DRI3
    if(drm_intel_bo_gem_export_to_prime(dest_region->bo, &fd) != 0) {
        fd = -1;
        return VA_STATUS_ERROR_OPERATION_FAILED;
    }

    dest_region->height = obj_surface->orig_height;
    dest_region->width = obj_surface->orig_width;
    dest_region->x = src_rect->x;
    dest_region->y = src_rect->y;
    obj_surface->exported_primefd = fd;

    xcb_connection_t *xcbconn = XGetXCBConnection(ctx->native_dpy);

    if (dri_vtable->createfence(ctx, (Window)draw, &fence))
        return VA_STATUS_ERROR_OPERATION_FAILED;

    dri_vtable->syncfence(ctx, &fence);

    pixmap = dri_vtable->createPixmap(ctx, root, dest_region->width,
                                      dest_region->height, depth, fd,
                                      bpp, stride, dest_region->bo->size);
#else
    dest_region->x = dri_drawable->x;
    dest_region->y = dri_drawable->y;
    dest_region->width = dri_drawable->width;
    dest_region->height = dri_drawable->height;
#endif

    if (!(flags & VA_SRC_COLOR_MASK))
        flags |= VA_SRC_BT601;

    intel_render_put_surface(ctx, obj_surface, src_rect, dst_rect, flags);

    for (i = 0; i < I965_MAX_SUBPIC_SUM; i++) {
        if (obj_surface->obj_subpic[i] != NULL) {
            assert(obj_surface->subpic[i] != VA_INVALID_ID);
            obj_surface->subpic_render_idx = i;
            intel_render_put_subpicture(ctx, obj_surface, src_rect, dst_rect);
        }
    }
#ifdef HAVE_VA_DRI3
    dri_vtable->presentPixmap(ctx, (Window)draw, pixmap,
                              0 , 0, 0, 0, 0,
                              None, None, fence.xid,
                              XCB_PRESENT_OPTION_NONE,
                              0 , 0, 0, 0, NULL);

    xcb_free_pixmap(xcbconn, pixmap);
    xcb_flush(xcbconn);
    dri_vtable->freefence(ctx, &fence);
#else
    if (!(g_intel_debug_option_flags & VA_INTEL_DEBUG_OPTION_BENCH))
        dri_vtable->swap_buffer(ctx, dri_drawable);
#endif
    _i965UnlockMutex(&i965->render_mutex);

    return VA_STATUS_SUCCESS;
}
