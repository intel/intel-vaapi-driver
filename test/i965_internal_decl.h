/*
 * Copyright (C) 2016 Intel Corporation. All Rights Reserved.
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

#ifndef I965_INTERNAL_DECL_H
#define I965_INTERNAL_DECL_H

extern "C" {
    #include "sysdeps.h"
    #include "i965_drv_video.h"
    #include "i965_encoder.h"

    extern VAStatus i965_CreateConfig(
        VADriverContextP, VAProfile, VAEntrypoint,
        VAConfigAttrib *, int, VAConfigID *);

    extern VAStatus i965_DestroyConfig(
        VADriverContextP, VAConfigID);

    extern VAStatus i965_CreateContext(
        VADriverContextP, VAConfigID, int, int,
        int, VASurfaceID *, int, VAContextID *);

    extern VAStatus i965_DestroyContext(
        VADriverContextP, VAContextID);

    extern VAStatus i965_CreateBuffer(
        VADriverContextP, VAContextID, VABufferType,
        unsigned, unsigned, void *, VABufferID *);

    extern VAStatus i965_DestroyBuffer(
        VADriverContextP, VABufferID);

    extern VAStatus i965_BeginPicture(
        VADriverContextP, VAContextID, VASurfaceID);

    extern VAStatus i965_RenderPicture(
        VADriverContextP, VAContextID,
        VABufferID *, int);

    extern VAStatus i965_EndPicture(
        VADriverContextP, VAContextID);

    extern VAStatus i965_DeriveImage(
        VADriverContextP, VASurfaceID, VAImage *);

    extern VAStatus i965_DestroyImage(
        VADriverContextP, VAImageID);

    extern VAStatus i965_SyncSurface(
        VADriverContextP, VASurfaceID);

    extern struct hw_codec_info *i965_get_codec_info(int);
    extern const struct intel_device_info *i965_get_device_info(int);

} // extern "C"

#endif
