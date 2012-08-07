/*
 * Copyright © 2009 Intel Corporation
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
 * SOFTWAR
 *
 * Authors:
 *    Zhou Chang <chang.zhou@intel.com>
 *
 */

#ifndef _GEN6_VME_H_
#define _GEN6_VME_H_

#include <xf86drm.h>
#include <drm.h>
#include <i915_drm.h>
#include <intel_bufmgr.h>

#include "i965_gpe_utils.h"

#define INTRA_VME_OUTPUT_IN_BYTES       16      /* in bytes */
#define INTRA_VME_OUTPUT_IN_DWS         (INTRA_VME_OUTPUT_IN_BYTES / 4)
#define INTER_VME_OUTPUT_IN_BYTES       160     /* the first 128 bytes for MVs and the last 32 bytes for other info */
#define INTER_VME_OUTPUT_IN_DWS         (INTER_VME_OUTPUT_IN_BYTES / 4)

#define MAX_INTERFACE_DESC_GEN6      MAX_GPE_KERNELS
#define MAX_MEDIA_SURFACES_GEN6      34

#define GEN6_VME_KERNEL_NUMBER          3

struct encode_state;
struct intel_encoder_context;

struct gen6_vme_context
{
    struct i965_gpe_context gpe_context;

    struct {
        dri_bo *bo;
    } vme_state;

    struct i965_buffer_surface vme_output;
    struct i965_buffer_surface vme_batchbuffer;


    void (*vme_surface2_setup)(VADriverContextP ctx,
                                struct i965_gpe_context *gpe_context,
                                struct object_surface *obj_surface,
                                unsigned long binding_table_offset,
                                unsigned long surface_state_offset);
    void (*vme_media_rw_surface_setup)(VADriverContextP ctx,
                                            struct i965_gpe_context *gpe_context,
                                            struct object_surface *obj_surface,
                                            unsigned long binding_table_offset,
                                            unsigned long surface_state_offset);
    void (*vme_buffer_suface_setup)(VADriverContextP ctx,
                                    struct i965_gpe_context *gpe_context,
                                    struct i965_buffer_surface *buffer_surface,
                                    unsigned long binding_table_offset,
                                    unsigned long surface_state_offset);
    void (*vme_media_chroma_surface_setup)(VADriverContextP ctx,
                                            struct i965_gpe_context *gpe_context,
                                            struct object_surface *obj_surface,
                                            unsigned long binding_table_offset,
                                            unsigned long surface_state_offset);
    void *vme_state_message;
};

Bool gen75_vme_context_init(VADriverContextP ctx, struct intel_encoder_context *encoder_context);
#endif /* _GEN6_VME_H_ */
