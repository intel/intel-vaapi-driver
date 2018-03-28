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
#include "i965_internal_decl.h"
#include "i965_output_wayland.h"
#include "i965_drv_video.h"
#include "i965_defines.h"
#include "dso_utils.h"

/* We need mesa's libEGL, first try the soname of a glvnd enabled mesa build */
#define LIBEGL_NAME             "libEGL_mesa.so.0"
/* Then fallback to plain libEGL.so.1 (which might not be mesa) */
#define LIBEGL_NAME_FALLBACK    "libEGL.so.1"
#define LIBWAYLAND_CLIENT_NAME  "libwayland-client.so.0"

#define VPP_OUT_FOURCC VA_FOURCC_RGBX
#define DEFAULT_RT_FORMAT VA_RT_FORMAT_RGB32

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
    uint32_t               wl_drm_name;
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
    uint32_t            name,
    const char         *interface,
    uint32_t            version
)
{
    VADriverContextP ctx = data;
    struct i965_driver_data * const i965 = i965_driver_data(ctx);
    struct va_wl_output * const wl_output = i965->wl_output;
    struct wl_vtable * const wl_vtable = &wl_output->vtable;

    if (strcmp(interface, "wl_drm") == 0) {
        wl_output->wl_drm_name = name;
        wl_output->wl_drm = registry_bind(wl_vtable, wl_output->wl_registry,
                                          name, wl_vtable->drm_interface,
                                          (version < 2) ? version : 2);
    }
}

static void
registry_handle_global_remove(
    void               *data,
    struct wl_registry *registry,
    uint32_t            name
)
{
    VADriverContextP ctx = data;
    struct i965_driver_data * const i965 = i965_driver_data(ctx);
    struct va_wl_output * const wl_output = i965->wl_output;

    if (wl_output->wl_drm && name == wl_output->wl_drm_name) {
        wl_output->vtable.proxy_destroy((struct wl_proxy *)wl_output->wl_drm);
        wl_output->wl_drm = NULL;
    }
}

static const struct wl_registry_listener registry_listener = {
    registry_handle_global,
    registry_handle_global_remove
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

    if (wl_output->queue) {
        wl_output->vtable.event_queue_destroy(wl_output->queue);
        wl_output->queue = NULL;
    }
    wl_output->queue = wl_vtable->display_create_queue(ctx->native_dpy);
    if (!wl_output->queue)
        return false;

    display_wrapper = wl_vtable->proxy_create_wrapper(ctx->native_dpy);
    if (!display_wrapper)
        return false;
    wl_vtable->proxy_set_queue((struct wl_proxy *) display_wrapper, wl_output->queue);

    if (wl_output->wl_registry) {
        wl_output->vtable.proxy_destroy((struct wl_proxy *)wl_output->wl_registry);
        wl_output->wl_registry = NULL;
    }
    wl_output->wl_registry = display_get_registry(wl_vtable, display_wrapper);
    wl_vtable->proxy_wrapper_destroy(display_wrapper);
    registry_add_listener(wl_vtable, wl_output->wl_registry,
                          &registry_listener, ctx);
    wl_vtable->display_roundtrip_queue(ctx->native_dpy, wl_output->queue);
    if (!wl_output->wl_drm)
        return false;
    return true;
}

static uint8_t
parse_fourcc_and_format(uint32_t fourcc, uint32_t *format)
{
    uint32_t tformat = VA_RT_FORMAT_YUV420;

    switch(fourcc) {
        case VA_FOURCC_YV12:
        case VA_FOURCC_I420:
        case VA_FOURCC_NV12:
            tformat = VA_RT_FORMAT_YUV420;
            break;
        case VA_FOURCC_YUY2:
            tformat = VA_RT_FORMAT_YUV422;
            break;
        case VA_FOURCC_RGBA:
        case VA_FOURCC_RGBX:
        case VA_FOURCC_BGRA:
        case VA_FOURCC_BGRX:
            tformat = VA_RT_FORMAT_RGB32;
            break;
        default:
            return VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT;
    }
    if (format)
        *format = tformat;
    return 0;
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
    struct object_surface *obj_surface, *obj_surface_rgbx;
    struct wl_buffer *buffer;
    uint32_t name, drm_format;
    int offsets[3], pitches[3];
    int fd = -1;

    static VASurfaceID out_surface_id = VA_INVALID_ID;
    static VAContextID context_id = 0;
    static VAConfigID config_id = 0;
    static uint32_t in_format, out_format;
    VAStatus va_status = VA_STATUS_SUCCESS;
    VAConfigAttrib attrib;
    VASurfaceAttrib surface_attrib;
    VAProcPipelineParameterBuffer pipeline_param;
    VARectangle surface_region, output_region;
    VABufferID pipeline_param_buf_id = VA_INVALID_ID;

    obj_surface = SURFACE(surface);
    if (!obj_surface)
        return VA_STATUS_ERROR_INVALID_SURFACE;

    if (flags != VA_FRAME_PICTURE)
        return VA_STATUS_ERROR_FLAG_NOT_SUPPORTED;

    if (!out_buffer)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    if (!ensure_wl_output(ctx))
        return VA_STATUS_ERROR_INVALID_DISPLAY;

    parse_fourcc_and_format(obj_surface->fourcc, &in_format);
    parse_fourcc_and_format(VPP_OUT_FOURCC, &out_format);

    if (DEFAULT_RT_FORMAT != in_format) {
        //Create surface/config/context for VPP pipeline
        //desired out format is RGB
        attrib.type = VAConfigAttribRTFormat;

        va_status = i965_GetConfigAttributes(ctx,
                                        VAProfileNone,
                                        VAEntrypointVideoProc,
                                        &attrib,
                                        1);
        if (VA_STATUS_SUCCESS !=va_status) {
            return VA_STATUS_ERROR_INVALID_CONFIG;
        }
        if (!(attrib.value & out_format)) {
            return VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT;
        }

        va_status = i965_CreateConfig(ctx,
                                VAProfileNone,
                                VAEntrypointVideoProc,
                                &attrib,
                                1,
                                &config_id);
        if (VA_STATUS_SUCCESS !=va_status) {
            return VA_STATUS_ERROR_OPERATION_FAILED;
        }

        surface_attrib.type =  VASurfaceAttribPixelFormat;
        surface_attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
        surface_attrib.value.type = VAGenericValueTypeInteger;
        surface_attrib.value.value.i = VPP_OUT_FOURCC;

        va_status = i965_CreateSurfaces2(ctx,
                                        out_format,
                                        obj_surface->orig_width,
                                        obj_surface->orig_height,
                                        &out_surface_id,
                                        1, &surface_attrib, 1);

        if (VA_STATUS_SUCCESS !=va_status) {
            return VA_STATUS_ERROR_OPERATION_FAILED;
        }

        va_status = i965_CreateContext(ctx,
                                    config_id,
                                    obj_surface->orig_width,
                                    obj_surface->orig_height,
                                    VA_PROGRESSIVE,
                                    &out_surface_id,
                                    1,
                                    &context_id);
        if (VA_STATUS_SUCCESS !=va_status) {
            return VA_STATUS_ERROR_OPERATION_FAILED;
        }

        /* Fill pipeline buffer */
        surface_region.x = 0;
        surface_region.y = 0;
        surface_region.width = obj_surface->orig_width;
        surface_region.height = obj_surface->orig_height;
        output_region.x = 0;
        output_region.y = 0;
        output_region.width = obj_surface->orig_width;
        output_region.height = obj_surface->orig_height;

        memset(&pipeline_param, 0, sizeof(pipeline_param));
        pipeline_param.surface = surface;
        pipeline_param.surface_region = &surface_region;
        pipeline_param.output_region = &output_region;

        va_status = i965_CreateBuffer(ctx,
                                context_id,
                                VAProcPipelineParameterBufferType,
                                sizeof(pipeline_param),
                                1,
                                &pipeline_param,
                                &pipeline_param_buf_id);
        if (VA_STATUS_SUCCESS !=va_status) {
            return VA_STATUS_ERROR_OPERATION_FAILED;
        }

        va_status = i965_BeginPicture(ctx,
                                context_id,
                                out_surface_id);
        if (VA_STATUS_SUCCESS !=va_status) {
            return VA_STATUS_ERROR_OPERATION_FAILED;
        }

        va_status = i965_RenderPicture(ctx,
                                    context_id,
                                    &pipeline_param_buf_id,
                                    1);
        if (VA_STATUS_SUCCESS !=va_status) {
            return VA_STATUS_ERROR_OPERATION_FAILED;
        }

        va_status = i965_EndPicture(ctx, context_id);
        if (VA_STATUS_SUCCESS !=va_status) {
            return VA_STATUS_ERROR_OPERATION_FAILED;
        }

        if (pipeline_param_buf_id != VA_INVALID_ID)
            i965_DestroyBuffer(ctx,pipeline_param_buf_id);

        obj_surface_rgbx = SURFACE(out_surface_id);
        if (!obj_surface_rgbx)
            return VA_STATUS_ERROR_INVALID_SURFACE;
    }

    if (DEFAULT_RT_FORMAT == in_format) {
            obj_surface_rgbx = obj_surface;
    }

    if (!vtable->has_prime_sharing ||
            (drm_intel_bo_gem_export_to_prime(obj_surface_rgbx->bo, &fd) != 0)){
            fd = -1;

        if (drm_intel_bo_flink(obj_surface_rgbx->bo, &name) != 0)
            return VA_STATUS_ERROR_INVALID_SURFACE;
    }

    drm_format = WL_DRM_FORMAT_XBGR8888;
    offsets[0] = 0;
    pitches[0] = obj_surface->orig_width * 4;
    offsets[1] = obj_surface->orig_width * obj_surface->height;
    pitches[1] = 0;
    offsets[2] = 0;
    pitches[2] = 0;

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

    // Release resource
    if (DEFAULT_RT_FORMAT != in_format) {
        i965_DestroySurfaces(ctx, &out_surface_id, 1);
        i965_DestroyContext(ctx, context_id);
        i965_DestroyConfig(ctx, config_id);
    }
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
