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
#include <unistd.h>
#include <va/va_backend.h>
#include <va/va_backend_wayland.h>
#include <wayland-client.h>
#include <wayland-drm-client-protocol.h>
#include "intel_driver.h"
#include "i965_output_wayland.h"
#include "i965_drv_video.h"
#include "i965_defines.h"
#include "dso_utils.h"

/* We need mesa's libEGL, first try the soname of a glvnd enabled mesa build */
#define LIBEGL_NAME             "libEGL_mesa.so.0"
/* Then fallback to plain libEGL.so.1 (which might not be mesa) */
#define LIBEGL_NAME_FALLBACK    "libEGL.so.1"
#define LIBWAYLAND_CLIENT_NAME  "libwayland-client.so.0"

typedef uint32_t (*wl_display_get_global_func)(struct wl_display *display,
                                               const char *interface, uint32_t version);
typedef struct wl_event_queue *(*wl_display_create_queue_func)(struct wl_display *display);
typedef void (*wl_display_roundtrip_queue_func)(struct wl_display *display,
                                                struct wl_event_queue *queue);
typedef void (*wl_event_queue_destroy_func)(struct wl_event_queue *queue);
typedef void *(*wl_proxy_create_wrapper_func)(struct wl_proxy *proxy);
typedef void(*wl_proxy_wrapper_destroy_func)(void *proxy);
typedef struct wl_proxy *(*wl_proxy_create_func)(struct wl_proxy *factory,
                                                 const struct wl_interface *interface);
typedef void (*wl_proxy_destroy_func)(struct wl_proxy *proxy);
typedef void (*wl_proxy_marshal_func)(struct wl_proxy *p, uint32_t opcode, ...);
typedef int (*wl_proxy_add_listener_func)(struct wl_proxy *proxy,
                                          void (**implementation)(void), void *data);
typedef void (*wl_proxy_set_queue_func)(struct wl_proxy *proxy, struct wl_event_queue *queue);

struct wl_vtable {
    const struct wl_interface        *buffer_interface;
    const struct wl_interface        *drm_interface;
    const struct wl_interface        *registry_interface;
    wl_display_create_queue_func      display_create_queue;
    wl_display_roundtrip_queue_func   display_roundtrip_queue;
    wl_event_queue_destroy_func       event_queue_destroy;
    wl_proxy_create_wrapper_func      proxy_create_wrapper;
    wl_proxy_wrapper_destroy_func     proxy_wrapper_destroy;
    wl_proxy_create_func              proxy_create;
    wl_proxy_destroy_func             proxy_destroy;
    wl_proxy_marshal_func             proxy_marshal;
    wl_proxy_add_listener_func        proxy_add_listener;
    wl_proxy_set_queue_func           proxy_set_queue;
};

struct va_wl_output {
    struct dso_handle     *libegl_handle;
    struct dso_handle     *libwl_client_handle;
    struct wl_vtable       vtable;
    struct wl_event_queue *queue;
    struct wl_drm         *wl_drm;
    struct wl_registry    *wl_registry;
};

/* These function are copied and adapted from the version inside
 * wayland-client-protocol.h
 */
static void *
registry_bind(
    struct wl_vtable          *wl_vtable,
    struct wl_registry        *wl_registry,
    uint32_t                   name,
    const struct wl_interface *interface,
    uint32_t                   version
)
{
    struct wl_proxy *id;

    id = wl_vtable->proxy_create((struct wl_proxy *) wl_registry,
                                 interface);
    if (!id)
        return NULL;

    wl_vtable->proxy_marshal((struct wl_proxy *) wl_registry,
                             WL_REGISTRY_BIND, name, interface->name,
                             version, id);

    return (void *) id;
}

static struct wl_registry *
display_get_registry(
    struct wl_vtable  *wl_vtable,
    struct wl_display *wl_display
)
{
    struct wl_proxy *callback;

    callback = wl_vtable->proxy_create((struct wl_proxy *) wl_display,
                                       wl_vtable->registry_interface);
    if (!callback)
        return NULL;

    wl_vtable->proxy_marshal((struct wl_proxy *) wl_display,
                             WL_DISPLAY_GET_REGISTRY, callback);

    return (struct wl_registry *) callback;
}

static int
registry_add_listener(
    struct wl_vtable                  *wl_vtable,
    struct wl_registry                *wl_registry,
    const struct wl_registry_listener *listener,
    void                              *data
)
{
    return wl_vtable->proxy_add_listener((struct wl_proxy *) wl_registry,
                                         (void (**)(void)) listener, data);
}

static void
registry_handle_global(
    void               *data,
    struct wl_registry *registry,
    uint32_t            id,
    const char         *interface,
    uint32_t            version
)
{
    VADriverContextP ctx = data;
    struct i965_driver_data * const i965 = i965_driver_data(ctx);
    struct va_wl_output * const wl_output = i965->wl_output;
    struct wl_vtable * const wl_vtable = &wl_output->vtable;

    if (strcmp(interface, "wl_drm") == 0) {
        wl_output->wl_drm = registry_bind(wl_vtable, wl_output->wl_registry,
                                          id, wl_vtable->drm_interface,
                                          (version < 2) ? version : 2);
    }
}

static const struct wl_registry_listener registry_listener = {
    registry_handle_global,
    NULL
};

/* Ensure wl_drm instance is created */
static bool
ensure_wl_output(VADriverContextP ctx)
{
    struct i965_driver_data * const i965 = i965_driver_data(ctx);
    struct va_wl_output * const wl_output = i965->wl_output;
    struct wl_vtable * const wl_vtable = &wl_output->vtable;
    struct wl_display *display_wrapper;

    if (wl_output->wl_drm)
        return true;

    wl_output->queue = wl_vtable->display_create_queue(ctx->native_dpy);
    if (!wl_output->queue)
        return false;
    display_wrapper = wl_vtable->proxy_create_wrapper(ctx->native_dpy);
    if (!display_wrapper)
        return false;
    wl_vtable->proxy_set_queue((struct wl_proxy *) display_wrapper, wl_output->queue);

    wl_output->wl_registry = display_get_registry(wl_vtable, display_wrapper);
    wl_vtable->proxy_wrapper_destroy(display_wrapper);
    registry_add_listener(wl_vtable, wl_output->wl_registry,
                          &registry_listener, ctx);
    wl_vtable->display_roundtrip_queue(ctx->native_dpy, wl_output->queue);
    if (!wl_output->wl_drm)
        return false;
    return true;
}

/* Create planar/prime YUV buffer
 * Create a prime buffer if fd is not -1, otherwise a
 * planar buffer
 */
static struct wl_buffer *
create_prime_or_planar_buffer(
    struct va_wl_output *wl_output,
    uint32_t             name,
    int                  fd,
    int32_t              width,
    int32_t              height,
    uint32_t             format,
    int32_t              offsets[3],
    int32_t              pitches[3]
)
{
    struct wl_vtable * const wl_vtable = &wl_output->vtable;
    struct wl_proxy *id;

    id = wl_vtable->proxy_create(
             (struct wl_proxy *)wl_output->wl_drm,
             wl_vtable->buffer_interface
         );
    if (!id)
        return NULL;

    wl_vtable->proxy_marshal(
        (struct wl_proxy *)wl_output->wl_drm,
        (fd != -1) ? WL_DRM_CREATE_PRIME_BUFFER : WL_DRM_CREATE_PLANAR_BUFFER,
        id,
        (fd != -1) ? fd : name,
        width, height, format,
        offsets[0], pitches[0],
        offsets[1], pitches[1],
        offsets[2], pitches[2]
    );
    return (struct wl_buffer *)id;
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
    struct VADriverVTableWayland * const vtable = ctx->vtable_wayland;
    struct i965_driver_data * const i965 = i965_driver_data(ctx);
    struct object_surface *obj_surface;
    struct wl_buffer *buffer;
    uint32_t name, drm_format;
    int offsets[3], pitches[3];
    int fd = -1;

    obj_surface = SURFACE(surface);
    if (!obj_surface)
        return VA_STATUS_ERROR_INVALID_SURFACE;

    if (flags != VA_FRAME_PICTURE)
        return VA_STATUS_ERROR_FLAG_NOT_SUPPORTED;

    if (!out_buffer)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    if (!ensure_wl_output(ctx))
        return VA_STATUS_ERROR_INVALID_DISPLAY;

    if (!vtable->has_prime_sharing || (drm_intel_bo_gem_export_to_prime(obj_surface->bo, &fd) != 0)) {
        fd = -1;

        if (drm_intel_bo_flink(obj_surface->bo, &name) != 0)
            return VA_STATUS_ERROR_INVALID_SURFACE;
    }

    switch (obj_surface->fourcc) {
    case VA_FOURCC_NV12:
        drm_format = WL_DRM_FORMAT_NV12;
        offsets[0] = 0;
        pitches[0] = obj_surface->width;
        offsets[1] = obj_surface->width * obj_surface->y_cb_offset;
        pitches[1] = obj_surface->cb_cr_pitch;
        offsets[2] = 0;
        pitches[2] = 0;
        break;
    case VA_FOURCC_YV12:
    case VA_FOURCC_I420:
    case VA_FOURCC_IMC1:
    case VA_FOURCC_IMC3:
    case VA_FOURCC_422H:
    case VA_FOURCC_422V:
    case VA_FOURCC_411P:
    case VA_FOURCC_444P:
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
        return VA_STATUS_ERROR_INVALID_IMAGE_FORMAT;
    }

    buffer = create_prime_or_planar_buffer(
                 i965->wl_output,
                 name,
                 fd,
                 obj_surface->orig_width,
                 obj_surface->orig_height,
                 drm_format,
                 offsets,
                 pitches
             );

    if (fd != -1)
        close(fd);

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
ensure_driver_vtable(VADriverContextP ctx)
{
    struct VADriverVTableWayland * const vtable = ctx->vtable_wayland;

    if (!vtable)
        return false;

    vtable->vaGetSurfaceBufferWl = va_GetSurfaceBufferWl;
    vtable->vaGetImageBufferWl   = va_GetImageBufferWl;
    return true;
}

bool
i965_output_wayland_init(VADriverContextP ctx)
{
    struct i965_driver_data * const i965 = i965_driver_data(ctx);
    struct dso_handle *dso_handle;
    struct wl_vtable *wl_vtable;

    static const struct dso_symbol libegl_symbols[] = {
        {
            "wl_drm_interface",
            offsetof(struct wl_vtable, drm_interface)
        },
        { NULL, }
    };

    static const struct dso_symbol libwl_client_symbols[] = {
        {
            "wl_buffer_interface",
            offsetof(struct wl_vtable, buffer_interface)
        },
        {
            "wl_registry_interface",
            offsetof(struct wl_vtable, registry_interface)
        },
        {
            "wl_display_create_queue",
            offsetof(struct wl_vtable, display_create_queue)
        },
        {
            "wl_display_roundtrip_queue",
            offsetof(struct wl_vtable, display_roundtrip_queue)
        },
        {
            "wl_event_queue_destroy",
            offsetof(struct wl_vtable, event_queue_destroy)
        },
        {
            "wl_proxy_create_wrapper",
            offsetof(struct wl_vtable, proxy_create_wrapper)
        },
        {
            "wl_proxy_wrapper_destroy",
            offsetof(struct wl_vtable, proxy_wrapper_destroy)
        },
        {
            "wl_proxy_create",
            offsetof(struct wl_vtable, proxy_create)
        },
        {
            "wl_proxy_destroy",
            offsetof(struct wl_vtable, proxy_destroy)
        },
        {
            "wl_proxy_marshal",
            offsetof(struct wl_vtable, proxy_marshal)
        },
        {
            "wl_proxy_add_listener",
            offsetof(struct wl_vtable, proxy_add_listener)
        },
        {
            "wl_proxy_set_queue",
            offsetof(struct wl_vtable, proxy_set_queue)
        },
        { NULL, }
    };

    if (ctx->display_type != VA_DISPLAY_WAYLAND)
        return false;

    i965->wl_output = calloc(1, sizeof(struct va_wl_output));
    if (!i965->wl_output)
        goto error;

    i965->wl_output->libegl_handle = dso_open(LIBEGL_NAME);
    if (!i965->wl_output->libegl_handle) {
        i965->wl_output->libegl_handle = dso_open(LIBEGL_NAME_FALLBACK);
        if (!i965->wl_output->libegl_handle)
            goto error;
    }

    dso_handle = i965->wl_output->libegl_handle;
    wl_vtable  = &i965->wl_output->vtable;
    if (!dso_get_symbols(dso_handle, wl_vtable, sizeof(*wl_vtable),
                         libegl_symbols))
        goto error;

    i965->wl_output->libwl_client_handle = dso_open(LIBWAYLAND_CLIENT_NAME);
    if (!i965->wl_output->libwl_client_handle)
        goto error;

    dso_handle = i965->wl_output->libwl_client_handle;
    wl_vtable  = &i965->wl_output->vtable;
    if (!dso_get_symbols(dso_handle, wl_vtable, sizeof(*wl_vtable),
                         libwl_client_symbols))
        goto error;

    if (!ensure_driver_vtable(ctx))
        goto error;
    return true;

error:
    i965_output_wayland_terminate(ctx);
    return false;
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
        wl_output->vtable.proxy_destroy((struct wl_proxy *)wl_output->wl_drm);
        wl_output->wl_drm = NULL;
    }

    if (wl_output->wl_registry) {
        wl_output->vtable.proxy_destroy((struct wl_proxy *)wl_output->wl_registry);
        wl_output->wl_registry = NULL;
    }

    if (wl_output->queue) {
        wl_output->vtable.event_queue_destroy(wl_output->queue);
        wl_output->queue = NULL;
    }

    if (wl_output->libegl_handle) {
        dso_close(wl_output->libegl_handle);
        wl_output->libegl_handle = NULL;
    }

    if (wl_output->libwl_client_handle) {
        dso_close(wl_output->libwl_client_handle);
        wl_output->libwl_client_handle = NULL;
    }
    free(wl_output);
    i965->wl_output = NULL;
}
