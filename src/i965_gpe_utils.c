/*
 * Copyright © 2012 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Xiang Haihao <haihao.xiang@intel.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "intel_batchbuffer.h"
#include "intel_driver.h"

#include "i965_drv_video.h"
#include "i965_gpe_utils.h"

static void
i965_gpe_select(VADriverContextP ctx,
                struct i965_gpe_context *gpe_context,
                struct intel_batchbuffer *batch)
{
    BEGIN_BATCH(batch, 1);
    OUT_BATCH(batch, CMD_PIPELINE_SELECT | PIPELINE_SELECT_MEDIA);
    ADVANCE_BATCH(batch);
}

static void
gen6_gpe_state_base_address(VADriverContextP ctx,
                            struct i965_gpe_context *gpe_context,
                            struct intel_batchbuffer *batch)
{
    BEGIN_BATCH(batch, 10);

    OUT_BATCH(batch, CMD_STATE_BASE_ADDRESS | (10 - 2));
    OUT_BATCH(batch, BASE_ADDRESS_MODIFY);              /* General State Base Address */
    OUT_RELOC(batch,
              gpe_context->surface_state_binding_table.bo,
              I915_GEM_DOMAIN_INSTRUCTION,
              0,
              BASE_ADDRESS_MODIFY);                     /* Surface state base address */
    OUT_BATCH(batch, BASE_ADDRESS_MODIFY);              /* Dynamic State Base Address */
    OUT_BATCH(batch, BASE_ADDRESS_MODIFY);              /* Indirect Object Base Address */
    OUT_BATCH(batch, BASE_ADDRESS_MODIFY);              /* Instruction Base Address */
    OUT_BATCH(batch, BASE_ADDRESS_MODIFY);              /* General State Access Upper Bound */
    OUT_BATCH(batch, BASE_ADDRESS_MODIFY);              /* Dynamic State Access Upper Bound */
    OUT_BATCH(batch, BASE_ADDRESS_MODIFY);              /* Indirect Object Access Upper Bound */
    OUT_BATCH(batch, BASE_ADDRESS_MODIFY);              /* Instruction Access Upper Bound */

    ADVANCE_BATCH(batch);
}

static void
gen6_gpe_vfe_state(VADriverContextP ctx,
                   struct i965_gpe_context *gpe_context,
                   struct intel_batchbuffer *batch)
{

    BEGIN_BATCH(batch, 8);

    OUT_BATCH(batch, CMD_MEDIA_VFE_STATE | (8 - 2));
    OUT_BATCH(batch, 0);                                        /* Scratch Space Base Pointer and Space */
    OUT_BATCH(batch,
              gpe_context->vfe_state.max_num_threads << 16 |    /* Maximum Number of Threads */
              gpe_context->vfe_state.num_urb_entries << 8 |     /* Number of URB Entries */
              gpe_context->vfe_state.gpgpu_mode << 2);          /* MEDIA Mode */
    OUT_BATCH(batch, 0);                                        /* Debug: Object ID */
    OUT_BATCH(batch,
              gpe_context->vfe_state.urb_entry_size << 16 |     /* URB Entry Allocation Size */
              gpe_context->vfe_state.curbe_allocation_size);    /* CURBE Allocation Size */
    /* the vfe_desc5/6/7 will decide whether the scoreboard is used. */
    OUT_BATCH(batch, gpe_context->vfe_desc5.dword);
    OUT_BATCH(batch, gpe_context->vfe_desc6.dword);
    OUT_BATCH(batch, gpe_context->vfe_desc7.dword);

    ADVANCE_BATCH(batch);

}

static void
gen6_gpe_curbe_load(VADriverContextP ctx,
                    struct i965_gpe_context *gpe_context,
                    struct intel_batchbuffer *batch)
{
    BEGIN_BATCH(batch, 4);

    OUT_BATCH(batch, CMD_MEDIA_CURBE_LOAD | (4 - 2));
    OUT_BATCH(batch, 0);
    OUT_BATCH(batch, gpe_context->curbe.length);
    OUT_RELOC(batch, gpe_context->curbe.bo, I915_GEM_DOMAIN_INSTRUCTION, 0, 0);

    ADVANCE_BATCH(batch);
}

static void
gen6_gpe_idrt(VADriverContextP ctx,
              struct i965_gpe_context *gpe_context,
              struct intel_batchbuffer *batch)
{
    BEGIN_BATCH(batch, 4);

    OUT_BATCH(batch, CMD_MEDIA_INTERFACE_LOAD | (4 - 2));
    OUT_BATCH(batch, 0);
    OUT_BATCH(batch, gpe_context->idrt.max_entries * gpe_context->idrt.entry_size);
    OUT_RELOC(batch, gpe_context->idrt.bo, I915_GEM_DOMAIN_INSTRUCTION, 0, 0);

    ADVANCE_BATCH(batch);
}

void
i965_gpe_load_kernels(VADriverContextP ctx,
                      struct i965_gpe_context *gpe_context,
                      struct i965_kernel *kernel_list,
                      unsigned int num_kernels)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    int i;

    assert(num_kernels <= MAX_GPE_KERNELS);
    memcpy(gpe_context->kernels, kernel_list, sizeof(*kernel_list) * num_kernels);
    gpe_context->num_kernels = num_kernels;

    for (i = 0; i < num_kernels; i++) {
        struct i965_kernel *kernel = &gpe_context->kernels[i];

        kernel->bo = dri_bo_alloc(i965->intel.bufmgr,
                                  kernel->name,
                                  kernel->size,
                                  0x1000);
        assert(kernel->bo);
        dri_bo_subdata(kernel->bo, 0, kernel->size, kernel->bin);
    }
}

void
i965_gpe_context_destroy(struct i965_gpe_context *gpe_context)
{
    int i;

    dri_bo_unreference(gpe_context->surface_state_binding_table.bo);
    gpe_context->surface_state_binding_table.bo = NULL;

    dri_bo_unreference(gpe_context->idrt.bo);
    gpe_context->idrt.bo = NULL;

    dri_bo_unreference(gpe_context->curbe.bo);
    gpe_context->curbe.bo = NULL;

    for (i = 0; i < gpe_context->num_kernels; i++) {
        struct i965_kernel *kernel = &gpe_context->kernels[i];

        dri_bo_unreference(kernel->bo);
        kernel->bo = NULL;
    }
}

void
i965_gpe_context_init(VADriverContextP ctx,
                      struct i965_gpe_context *gpe_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    dri_bo *bo;

    dri_bo_unreference(gpe_context->surface_state_binding_table.bo);
    bo = dri_bo_alloc(i965->intel.bufmgr,
                      "surface state & binding table",
                      gpe_context->surface_state_binding_table.length,
                      4096);
    assert(bo);
    gpe_context->surface_state_binding_table.bo = bo;

    dri_bo_unreference(gpe_context->idrt.bo);
    bo = dri_bo_alloc(i965->intel.bufmgr,
                      "interface descriptor table",
                      gpe_context->idrt.entry_size * gpe_context->idrt.max_entries,
                      4096);
    assert(bo);
    gpe_context->idrt.bo = bo;

    dri_bo_unreference(gpe_context->curbe.bo);
    bo = dri_bo_alloc(i965->intel.bufmgr,
                      "curbe buffer",
                      gpe_context->curbe.length,
                      4096);
    assert(bo);
    gpe_context->curbe.bo = bo;
}

void
gen6_gpe_pipeline_setup(VADriverContextP ctx,
                        struct i965_gpe_context *gpe_context,
                        struct intel_batchbuffer *batch)
{
    intel_batchbuffer_emit_mi_flush(batch);

    i965_gpe_select(ctx, gpe_context, batch);
    gen6_gpe_state_base_address(ctx, gpe_context, batch);
    gen6_gpe_vfe_state(ctx, gpe_context, batch);
    gen6_gpe_curbe_load(ctx, gpe_context, batch);
    gen6_gpe_idrt(ctx, gpe_context, batch);
}

static void
gen8_gpe_pipeline_end(VADriverContextP ctx,
                      struct i965_gpe_context *gpe_context,
                      struct intel_batchbuffer *batch)
{
    /* No thing to do */
}

static void
i965_gpe_set_surface_tiling(struct i965_surface_state *ss, unsigned int tiling)
{
    switch (tiling) {
    case I915_TILING_NONE:
        ss->ss3.tiled_surface = 0;
        ss->ss3.tile_walk = 0;
        break;
    case I915_TILING_X:
        ss->ss3.tiled_surface = 1;
        ss->ss3.tile_walk = I965_TILEWALK_XMAJOR;
        break;
    case I915_TILING_Y:
        ss->ss3.tiled_surface = 1;
        ss->ss3.tile_walk = I965_TILEWALK_YMAJOR;
        break;
    }
}

static void
i965_gpe_set_surface2_tiling(struct i965_surface_state2 *ss, unsigned int tiling)
{
    switch (tiling) {
    case I915_TILING_NONE:
        ss->ss2.tiled_surface = 0;
        ss->ss2.tile_walk = 0;
        break;
    case I915_TILING_X:
        ss->ss2.tiled_surface = 1;
        ss->ss2.tile_walk = I965_TILEWALK_XMAJOR;
        break;
    case I915_TILING_Y:
        ss->ss2.tiled_surface = 1;
        ss->ss2.tile_walk = I965_TILEWALK_YMAJOR;
        break;
    }
}

static void
gen7_gpe_set_surface_tiling(struct gen7_surface_state *ss, unsigned int tiling)
{
    switch (tiling) {
    case I915_TILING_NONE:
        ss->ss0.tiled_surface = 0;
        ss->ss0.tile_walk = 0;
        break;
    case I915_TILING_X:
        ss->ss0.tiled_surface = 1;
        ss->ss0.tile_walk = I965_TILEWALK_XMAJOR;
        break;
    case I915_TILING_Y:
        ss->ss0.tiled_surface = 1;
        ss->ss0.tile_walk = I965_TILEWALK_YMAJOR;
        break;
    }
}

static void
gen7_gpe_set_surface2_tiling(struct gen7_surface_state2 *ss, unsigned int tiling)
{
    switch (tiling) {
    case I915_TILING_NONE:
        ss->ss2.tiled_surface = 0;
        ss->ss2.tile_walk = 0;
        break;
    case I915_TILING_X:
        ss->ss2.tiled_surface = 1;
        ss->ss2.tile_walk = I965_TILEWALK_XMAJOR;
        break;
    case I915_TILING_Y:
        ss->ss2.tiled_surface = 1;
        ss->ss2.tile_walk = I965_TILEWALK_YMAJOR;
        break;
    }
}

static void
gen8_gpe_set_surface_tiling(struct gen8_surface_state *ss, unsigned int tiling)
{
    switch (tiling) {
    case I915_TILING_NONE:
        ss->ss0.tiled_surface = 0;
        ss->ss0.tile_walk = 0;
        break;
    case I915_TILING_X:
        ss->ss0.tiled_surface = 1;
        ss->ss0.tile_walk = I965_TILEWALK_XMAJOR;
        break;
    case I915_TILING_Y:
        ss->ss0.tiled_surface = 1;
        ss->ss0.tile_walk = I965_TILEWALK_YMAJOR;
        break;
    }
}

static void
gen8_gpe_set_surface2_tiling(struct gen8_surface_state2 *ss, unsigned int tiling)
{
    switch (tiling) {
    case I915_TILING_NONE:
        ss->ss2.tiled_surface = 0;
        ss->ss2.tile_walk = 0;
        break;
    case I915_TILING_X:
        ss->ss2.tiled_surface = 1;
        ss->ss2.tile_walk = I965_TILEWALK_XMAJOR;
        break;
    case I915_TILING_Y:
        ss->ss2.tiled_surface = 1;
        ss->ss2.tile_walk = I965_TILEWALK_YMAJOR;
        break;
    }
}

static void
i965_gpe_set_surface2_state(VADriverContextP ctx,
                            struct object_surface *obj_surface,
                            struct i965_surface_state2 *ss)
{
    int w, h, w_pitch;
    unsigned int tiling, swizzle;

    assert(obj_surface->bo);
    assert(obj_surface->fourcc == VA_FOURCC_NV12);

    dri_bo_get_tiling(obj_surface->bo, &tiling, &swizzle);
    w = obj_surface->orig_width;
    h = obj_surface->orig_height;
    w_pitch = obj_surface->width;

    memset(ss, 0, sizeof(*ss));
    /* ss0 */
    ss->ss0.surface_base_address = obj_surface->bo->offset;
    /* ss1 */
    ss->ss1.cbcr_pixel_offset_v_direction = 2;
    ss->ss1.width = w - 1;
    ss->ss1.height = h - 1;
    /* ss2 */
    ss->ss2.surface_format = MFX_SURFACE_PLANAR_420_8;
    ss->ss2.interleave_chroma = 1;
    ss->ss2.pitch = w_pitch - 1;
    ss->ss2.half_pitch_for_chroma = 0;
    i965_gpe_set_surface2_tiling(ss, tiling);
    /* ss3: UV offset for interleave mode */
    ss->ss3.x_offset_for_cb = obj_surface->x_cb_offset;
    ss->ss3.y_offset_for_cb = obj_surface->y_cb_offset;
}

void
i965_gpe_surface2_setup(VADriverContextP ctx,
                        struct i965_gpe_context *gpe_context,
                        struct object_surface *obj_surface,
                        unsigned long binding_table_offset,
                        unsigned long surface_state_offset)
{
    struct i965_surface_state2 *ss;
    dri_bo *bo;

    bo = gpe_context->surface_state_binding_table.bo;
    dri_bo_map(bo, 1);
    assert(bo->virtual);

    ss = (struct i965_surface_state2 *)((char *)bo->virtual + surface_state_offset);
    i965_gpe_set_surface2_state(ctx, obj_surface, ss);
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_RENDER, 0,
                      0,
                      surface_state_offset + offsetof(struct i965_surface_state2, ss0),
                      obj_surface->bo);

    *((unsigned int *)((char *)bo->virtual + binding_table_offset)) = surface_state_offset;
    dri_bo_unmap(bo);
}

static void
i965_gpe_set_media_rw_surface_state(VADriverContextP ctx,
                                    struct object_surface *obj_surface,
                                    struct i965_surface_state *ss)
{
    int w, h, w_pitch;
    unsigned int tiling, swizzle;

    dri_bo_get_tiling(obj_surface->bo, &tiling, &swizzle);
    w = obj_surface->orig_width;
    h = obj_surface->orig_height;
    w_pitch = obj_surface->width;

    memset(ss, 0, sizeof(*ss));
    /* ss0 */
    ss->ss0.surface_type = I965_SURFACE_2D;
    ss->ss0.surface_format = I965_SURFACEFORMAT_R8_UNORM;
    /* ss1 */
    ss->ss1.base_addr = obj_surface->bo->offset;
    /* ss2 */
    ss->ss2.width = w / 4 - 1;  /* in DWORDs for media read & write message */
    ss->ss2.height = h - 1;
    /* ss3 */
    ss->ss3.pitch = w_pitch - 1;
    i965_gpe_set_surface_tiling(ss, tiling);
}

void
i965_gpe_media_rw_surface_setup(VADriverContextP ctx,
                                struct i965_gpe_context *gpe_context,
                                struct object_surface *obj_surface,
                                unsigned long binding_table_offset,
                                unsigned long surface_state_offset,
                                int write_enabled)
{
    struct i965_surface_state *ss;
    dri_bo *bo;

    bo = gpe_context->surface_state_binding_table.bo;
    dri_bo_map(bo, True);
    assert(bo->virtual);

    ss = (struct i965_surface_state *)((char *)bo->virtual + surface_state_offset);
    i965_gpe_set_media_rw_surface_state(ctx, obj_surface, ss);
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_RENDER, write_enabled ? I915_GEM_DOMAIN_RENDER : 0,
                      0,
                      surface_state_offset + offsetof(struct i965_surface_state, ss1),
                      obj_surface->bo);

    *((unsigned int *)((char *)bo->virtual + binding_table_offset)) = surface_state_offset;
    dri_bo_unmap(bo);
}

static void
i965_gpe_set_buffer_surface_state(VADriverContextP ctx,
                                  struct i965_buffer_surface *buffer_surface,
                                  struct i965_surface_state *ss)
{
    int num_entries;

    assert(buffer_surface->bo);
    num_entries = buffer_surface->num_blocks * buffer_surface->size_block / buffer_surface->pitch;

    memset(ss, 0, sizeof(*ss));
    /* ss0 */
    ss->ss0.render_cache_read_mode = 1;
    ss->ss0.surface_type = I965_SURFACE_BUFFER;
    /* ss1 */
    ss->ss1.base_addr = buffer_surface->bo->offset;
    /* ss2 */
    ss->ss2.width = ((num_entries - 1) & 0x7f);
    ss->ss2.height = (((num_entries - 1) >> 7) & 0x1fff);
    /* ss3 */
    ss->ss3.depth = (((num_entries - 1) >> 20) & 0x7f);
    ss->ss3.pitch = buffer_surface->pitch - 1;
}

void
i965_gpe_buffer_suface_setup(VADriverContextP ctx,
                             struct i965_gpe_context *gpe_context,
                             struct i965_buffer_surface *buffer_surface,
                             unsigned long binding_table_offset,
                             unsigned long surface_state_offset)
{
    struct i965_surface_state *ss;
    dri_bo *bo;

    bo = gpe_context->surface_state_binding_table.bo;
    dri_bo_map(bo, 1);
    assert(bo->virtual);

    ss = (struct i965_surface_state *)((char *)bo->virtual + surface_state_offset);
    i965_gpe_set_buffer_surface_state(ctx, buffer_surface, ss);
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER,
                      0,
                      surface_state_offset + offsetof(struct i965_surface_state, ss1),
                      buffer_surface->bo);

    *((unsigned int *)((char *)bo->virtual + binding_table_offset)) = surface_state_offset;
    dri_bo_unmap(bo);
}

static void
gen7_gpe_set_surface2_state(VADriverContextP ctx,
                            struct object_surface *obj_surface,
                            struct gen7_surface_state2 *ss)
{
    int w, h, w_pitch;
    unsigned int tiling, swizzle;

    assert(obj_surface->bo);
    assert(obj_surface->fourcc == VA_FOURCC_NV12);

    dri_bo_get_tiling(obj_surface->bo, &tiling, &swizzle);
    w = obj_surface->orig_width;
    h = obj_surface->orig_height;
    w_pitch = obj_surface->width;

    memset(ss, 0, sizeof(*ss));
    /* ss0 */
    ss->ss0.surface_base_address = obj_surface->bo->offset;
    /* ss1 */
    ss->ss1.cbcr_pixel_offset_v_direction = 2;
    ss->ss1.width = w - 1;
    ss->ss1.height = h - 1;
    /* ss2 */
    ss->ss2.surface_format = MFX_SURFACE_PLANAR_420_8;
    ss->ss2.interleave_chroma = 1;
    ss->ss2.pitch = w_pitch - 1;
    ss->ss2.half_pitch_for_chroma = 0;
    gen7_gpe_set_surface2_tiling(ss, tiling);
    /* ss3: UV offset for interleave mode */
    ss->ss3.x_offset_for_cb = obj_surface->x_cb_offset;
    ss->ss3.y_offset_for_cb = obj_surface->y_cb_offset;
}

void
gen7_gpe_surface2_setup(VADriverContextP ctx,
                        struct i965_gpe_context *gpe_context,
                        struct object_surface *obj_surface,
                        unsigned long binding_table_offset,
                        unsigned long surface_state_offset)
{
    struct gen7_surface_state2 *ss;
    dri_bo *bo;

    bo = gpe_context->surface_state_binding_table.bo;
    dri_bo_map(bo, 1);
    assert(bo->virtual);

    ss = (struct gen7_surface_state2 *)((char *)bo->virtual + surface_state_offset);
    gen7_gpe_set_surface2_state(ctx, obj_surface, ss);
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_RENDER, 0,
                      0,
                      surface_state_offset + offsetof(struct gen7_surface_state2, ss0),
                      obj_surface->bo);

    *((unsigned int *)((char *)bo->virtual + binding_table_offset)) = surface_state_offset;
    dri_bo_unmap(bo);
}

static void
gen7_gpe_set_media_rw_surface_state(VADriverContextP ctx,
                                    struct object_surface *obj_surface,
                                    struct gen7_surface_state *ss)
{
    int w, h, w_pitch;
    unsigned int tiling, swizzle;

    dri_bo_get_tiling(obj_surface->bo, &tiling, &swizzle);
    w = obj_surface->orig_width;
    h = obj_surface->orig_height;
    w_pitch = obj_surface->width;

    memset(ss, 0, sizeof(*ss));
    /* ss0 */
    ss->ss0.surface_type = I965_SURFACE_2D;
    ss->ss0.surface_format = I965_SURFACEFORMAT_R8_UNORM;
    /* ss1 */
    ss->ss1.base_addr = obj_surface->bo->offset;
    /* ss2 */
    ss->ss2.width = w / 4 - 1;  /* in DWORDs for media read & write message */
    ss->ss2.height = h - 1;
    /* ss3 */
    ss->ss3.pitch = w_pitch - 1;
    gen7_gpe_set_surface_tiling(ss, tiling);
}

static void
gen75_gpe_set_media_chroma_surface_state(VADriverContextP ctx,
                                         struct object_surface *obj_surface,
                                         struct gen7_surface_state *ss)
{
    int w, w_pitch;
    unsigned int tiling, swizzle;
    int cbcr_offset;

    dri_bo_get_tiling(obj_surface->bo, &tiling, &swizzle);
    w = obj_surface->orig_width;
    w_pitch = obj_surface->width;

    cbcr_offset = obj_surface->height * obj_surface->width;
    memset(ss, 0, sizeof(*ss));
    /* ss0 */
    ss->ss0.surface_type = I965_SURFACE_2D;
    ss->ss0.surface_format = I965_SURFACEFORMAT_R8_UNORM;
    /* ss1 */
    ss->ss1.base_addr = obj_surface->bo->offset + cbcr_offset;
    /* ss2 */
    ss->ss2.width = w / 4 - 1;  /* in DWORDs for media read & write message */
    ss->ss2.height = (obj_surface->height / 2) - 1;
    /* ss3 */
    ss->ss3.pitch = w_pitch - 1;
    gen7_gpe_set_surface_tiling(ss, tiling);
}

void
gen7_gpe_media_rw_surface_setup(VADriverContextP ctx,
                                struct i965_gpe_context *gpe_context,
                                struct object_surface *obj_surface,
                                unsigned long binding_table_offset,
                                unsigned long surface_state_offset,
                                int write_enabled)
{
    struct gen7_surface_state *ss;
    dri_bo *bo;

    bo = gpe_context->surface_state_binding_table.bo;
    dri_bo_map(bo, True);
    assert(bo->virtual);

    ss = (struct gen7_surface_state *)((char *)bo->virtual + surface_state_offset);
    gen7_gpe_set_media_rw_surface_state(ctx, obj_surface, ss);
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_RENDER, write_enabled ? I915_GEM_DOMAIN_RENDER : 0,
                      0,
                      surface_state_offset + offsetof(struct gen7_surface_state, ss1),
                      obj_surface->bo);

    *((unsigned int *)((char *)bo->virtual + binding_table_offset)) = surface_state_offset;
    dri_bo_unmap(bo);
}

void
gen75_gpe_media_chroma_surface_setup(VADriverContextP ctx,
                                     struct i965_gpe_context *gpe_context,
                                     struct object_surface *obj_surface,
                                     unsigned long binding_table_offset,
                                     unsigned long surface_state_offset,
                                     int write_enabled)
{
    struct gen7_surface_state *ss;
    dri_bo *bo;
    int cbcr_offset;

    assert(obj_surface->fourcc == VA_FOURCC_NV12);
    bo = gpe_context->surface_state_binding_table.bo;
    dri_bo_map(bo, True);
    assert(bo->virtual);

    cbcr_offset = obj_surface->height * obj_surface->width;
    ss = (struct gen7_surface_state *)((char *)bo->virtual + surface_state_offset);
    gen75_gpe_set_media_chroma_surface_state(ctx, obj_surface, ss);
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_RENDER, write_enabled ? I915_GEM_DOMAIN_RENDER : 0,
                      cbcr_offset,
                      surface_state_offset + offsetof(struct gen7_surface_state, ss1),
                      obj_surface->bo);

    *((unsigned int *)((char *)bo->virtual + binding_table_offset)) = surface_state_offset;
    dri_bo_unmap(bo);
}


static void
gen7_gpe_set_buffer_surface_state(VADriverContextP ctx,
                                  struct i965_buffer_surface *buffer_surface,
                                  struct gen7_surface_state *ss)
{
    int num_entries;

    assert(buffer_surface->bo);
    num_entries = buffer_surface->num_blocks * buffer_surface->size_block / buffer_surface->pitch;

    memset(ss, 0, sizeof(*ss));
    /* ss0 */
    ss->ss0.surface_type = I965_SURFACE_BUFFER;
    /* ss1 */
    ss->ss1.base_addr = buffer_surface->bo->offset;
    /* ss2 */
    ss->ss2.width = ((num_entries - 1) & 0x7f);
    ss->ss2.height = (((num_entries - 1) >> 7) & 0x3fff);
    /* ss3 */
    ss->ss3.depth = (((num_entries - 1) >> 21) & 0x3f);
    ss->ss3.pitch = buffer_surface->pitch - 1;
}

void
gen7_gpe_buffer_suface_setup(VADriverContextP ctx,
                             struct i965_gpe_context *gpe_context,
                             struct i965_buffer_surface *buffer_surface,
                             unsigned long binding_table_offset,
                             unsigned long surface_state_offset)
{
    struct gen7_surface_state *ss;
    dri_bo *bo;

    bo = gpe_context->surface_state_binding_table.bo;
    dri_bo_map(bo, 1);
    assert(bo->virtual);

    ss = (struct gen7_surface_state *)((char *)bo->virtual + surface_state_offset);
    gen7_gpe_set_buffer_surface_state(ctx, buffer_surface, ss);
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER,
                      0,
                      surface_state_offset + offsetof(struct gen7_surface_state, ss1),
                      buffer_surface->bo);

    *((unsigned int *)((char *)bo->virtual + binding_table_offset)) = surface_state_offset;
    dri_bo_unmap(bo);
}

static void
gen8_gpe_set_surface2_state(VADriverContextP ctx,
                            struct object_surface *obj_surface,
                            struct gen8_surface_state2 *ss)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    int w, h, w_pitch;
    unsigned int tiling, swizzle;

    assert(obj_surface->bo);
    assert(obj_surface->fourcc == VA_FOURCC_NV12
           || obj_surface->fourcc == VA_FOURCC_P010);

    dri_bo_get_tiling(obj_surface->bo, &tiling, &swizzle);
    w = obj_surface->orig_width;
    h = obj_surface->orig_height;
    w_pitch = obj_surface->width;

    memset(ss, 0, sizeof(*ss));
    /* ss0 */
    if (IS_GEN9(i965->intel.device_info))
        ss->ss5.surface_object_mocs = GEN9_CACHE_PTE;

    ss->ss6.base_addr = (uint32_t)obj_surface->bo->offset64;
    ss->ss7.base_addr_high = (uint32_t)(obj_surface->bo->offset64 >> 32);
    /* ss1 */
    ss->ss1.cbcr_pixel_offset_v_direction = 2;
    ss->ss1.width = w - 1;
    ss->ss1.height = h - 1;
    /* ss2 */
    ss->ss2.surface_format = MFX_SURFACE_PLANAR_420_8;
    ss->ss2.interleave_chroma = 1;
    ss->ss2.pitch = w_pitch - 1;
    ss->ss2.half_pitch_for_chroma = 0;
    gen8_gpe_set_surface2_tiling(ss, tiling);
    /* ss3: UV offset for interleave mode */
    ss->ss3.x_offset_for_cb = obj_surface->x_cb_offset;
    ss->ss3.y_offset_for_cb = obj_surface->y_cb_offset;
}

void
gen8_gpe_surface2_setup(VADriverContextP ctx,
                        struct i965_gpe_context *gpe_context,
                        struct object_surface *obj_surface,
                        unsigned long binding_table_offset,
                        unsigned long surface_state_offset)
{
    struct gen8_surface_state2 *ss;
    dri_bo *bo;

    bo = gpe_context->surface_state_binding_table.bo;
    dri_bo_map(bo, 1);
    assert(bo->virtual);

    ss = (struct gen8_surface_state2 *)((char *)bo->virtual + surface_state_offset);
    gen8_gpe_set_surface2_state(ctx, obj_surface, ss);
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_RENDER, 0,
                      0,
                      surface_state_offset + offsetof(struct gen8_surface_state2, ss6),
                      obj_surface->bo);

    *((unsigned int *)((char *)bo->virtual + binding_table_offset)) = surface_state_offset;
    dri_bo_unmap(bo);
}

static void
gen8_gpe_set_media_rw_surface_state(VADriverContextP ctx,
                                    struct object_surface *obj_surface,
                                    struct gen8_surface_state *ss)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    int w, h, w_pitch;
    unsigned int tiling, swizzle;

    dri_bo_get_tiling(obj_surface->bo, &tiling, &swizzle);
    w = obj_surface->orig_width;
    h = obj_surface->orig_height;
    w_pitch = obj_surface->width;

    memset(ss, 0, sizeof(*ss));
    /* ss0 */
    if (IS_GEN9(i965->intel.device_info))
        ss->ss1.surface_mocs = GEN9_CACHE_PTE;

    ss->ss0.surface_type = I965_SURFACE_2D;
    ss->ss0.surface_format = I965_SURFACEFORMAT_R8_UNORM;
    /* ss1 */
    ss->ss8.base_addr = (uint32_t)obj_surface->bo->offset64;
    ss->ss9.base_addr_high = (uint32_t)(obj_surface->bo->offset64 >> 32);
    /* ss2 */
    ss->ss2.width = w / 4 - 1;  /* in DWORDs for media read & write message */
    ss->ss2.height = h - 1;
    /* ss3 */
    ss->ss3.pitch = w_pitch - 1;
    gen8_gpe_set_surface_tiling(ss, tiling);
}

static void
gen8_gpe_set_media_chroma_surface_state(VADriverContextP ctx,
                                        struct object_surface *obj_surface,
                                        struct gen8_surface_state *ss)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    int w, w_pitch;
    unsigned int tiling, swizzle;
    int cbcr_offset;
    uint64_t base_offset;

    dri_bo_get_tiling(obj_surface->bo, &tiling, &swizzle);
    w = obj_surface->orig_width;
    w_pitch = obj_surface->width;

    cbcr_offset = obj_surface->height * obj_surface->width;
    memset(ss, 0, sizeof(*ss));
    /* ss0 */
    if (IS_GEN9(i965->intel.device_info))
        ss->ss1.surface_mocs = GEN9_CACHE_PTE;

    ss->ss0.surface_type = I965_SURFACE_2D;
    ss->ss0.surface_format = I965_SURFACEFORMAT_R8_UNORM;
    /* ss1 */
    base_offset = obj_surface->bo->offset64 + cbcr_offset;
    ss->ss8.base_addr = (uint32_t) base_offset;
    ss->ss9.base_addr_high = (uint32_t)(base_offset >> 32);
    /* ss2 */
    ss->ss2.width = w / 4 - 1;  /* in DWORDs for media read & write message */
    ss->ss2.height = (obj_surface->height / 2) - 1;
    /* ss3 */
    ss->ss3.pitch = w_pitch - 1;
    gen8_gpe_set_surface_tiling(ss, tiling);
}

void
gen8_gpe_media_rw_surface_setup(VADriverContextP ctx,
                                struct i965_gpe_context *gpe_context,
                                struct object_surface *obj_surface,
                                unsigned long binding_table_offset,
                                unsigned long surface_state_offset,
                                int write_enabled)
{
    struct gen8_surface_state *ss;
    dri_bo *bo;

    bo = gpe_context->surface_state_binding_table.bo;
    dri_bo_map(bo, True);
    assert(bo->virtual);

    ss = (struct gen8_surface_state *)((char *)bo->virtual + surface_state_offset);
    gen8_gpe_set_media_rw_surface_state(ctx, obj_surface, ss);
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_RENDER, write_enabled ? I915_GEM_DOMAIN_RENDER : 0,
                      0,
                      surface_state_offset + offsetof(struct gen8_surface_state, ss8),
                      obj_surface->bo);

    *((unsigned int *)((char *)bo->virtual + binding_table_offset)) = surface_state_offset;
    dri_bo_unmap(bo);
}

void
gen8_gpe_media_chroma_surface_setup(VADriverContextP ctx,
                                    struct i965_gpe_context *gpe_context,
                                    struct object_surface *obj_surface,
                                    unsigned long binding_table_offset,
                                    unsigned long surface_state_offset,
                                    int write_enabled)
{
    struct gen8_surface_state *ss;
    dri_bo *bo;
    int cbcr_offset;

    assert(obj_surface->fourcc == VA_FOURCC_NV12
           || obj_surface->fourcc == VA_FOURCC_P010);
    bo = gpe_context->surface_state_binding_table.bo;
    dri_bo_map(bo, True);
    assert(bo->virtual);

    cbcr_offset = obj_surface->height * obj_surface->width;
    ss = (struct gen8_surface_state *)((char *)bo->virtual + surface_state_offset);
    gen8_gpe_set_media_chroma_surface_state(ctx, obj_surface, ss);
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_RENDER, write_enabled ? I915_GEM_DOMAIN_RENDER : 0,
                      cbcr_offset,
                      surface_state_offset + offsetof(struct gen8_surface_state, ss8),
                      obj_surface->bo);

    *((unsigned int *)((char *)bo->virtual + binding_table_offset)) = surface_state_offset;
    dri_bo_unmap(bo);
}


static void
gen8_gpe_set_buffer_surface_state(VADriverContextP ctx,
                                  struct i965_buffer_surface *buffer_surface,
                                  struct gen8_surface_state *ss)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    int num_entries;

    assert(buffer_surface->bo);
    num_entries = buffer_surface->num_blocks * buffer_surface->size_block / buffer_surface->pitch;

    memset(ss, 0, sizeof(*ss));
    /* ss0 */
    ss->ss0.surface_type = I965_SURFACE_BUFFER;
    if (IS_GEN9(i965->intel.device_info))
        ss->ss1.surface_mocs = GEN9_CACHE_PTE;

    /* ss1 */
    ss->ss8.base_addr = (uint32_t)buffer_surface->bo->offset64;
    ss->ss9.base_addr_high = (uint32_t)(buffer_surface->bo->offset64 >> 32);
    /* ss2 */
    ss->ss2.width = ((num_entries - 1) & 0x7f);
    ss->ss2.height = (((num_entries - 1) >> 7) & 0x3fff);
    /* ss3 */
    ss->ss3.depth = (((num_entries - 1) >> 21) & 0x3f);
    ss->ss3.pitch = buffer_surface->pitch - 1;
}

void
gen8_gpe_buffer_suface_setup(VADriverContextP ctx,
                             struct i965_gpe_context *gpe_context,
                             struct i965_buffer_surface *buffer_surface,
                             unsigned long binding_table_offset,
                             unsigned long surface_state_offset)
{
    struct gen8_surface_state *ss;
    dri_bo *bo;

    bo = gpe_context->surface_state_binding_table.bo;
    dri_bo_map(bo, 1);
    assert(bo->virtual);

    ss = (struct gen8_surface_state *)((char *)bo->virtual + surface_state_offset);
    gen8_gpe_set_buffer_surface_state(ctx, buffer_surface, ss);
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER,
                      0,
                      surface_state_offset + offsetof(struct gen8_surface_state, ss8),
                      buffer_surface->bo);

    *((unsigned int *)((char *)bo->virtual + binding_table_offset)) = surface_state_offset;
    dri_bo_unmap(bo);
}

static void
gen8_gpe_state_base_address(VADriverContextP ctx,
                            struct i965_gpe_context *gpe_context,
                            struct intel_batchbuffer *batch)
{
    BEGIN_BATCH(batch, 16);

    OUT_BATCH(batch, CMD_STATE_BASE_ADDRESS | 14);

    OUT_BATCH(batch, 0 | BASE_ADDRESS_MODIFY);              //General State Base Address
    OUT_BATCH(batch, 0);
    OUT_BATCH(batch, 0);

    /*DW4 Surface state base address */
    OUT_RELOC64(batch, gpe_context->surface_state_binding_table.bo, I915_GEM_DOMAIN_INSTRUCTION, 0, BASE_ADDRESS_MODIFY); /* Surface state base address */

    /*DW6. Dynamic state base address */
    if (gpe_context->dynamic_state.bo)
        OUT_RELOC64(batch, gpe_context->dynamic_state.bo,
                    I915_GEM_DOMAIN_RENDER | I915_GEM_DOMAIN_SAMPLER,
                    0, BASE_ADDRESS_MODIFY);
    else {
        OUT_BATCH(batch, 0 | BASE_ADDRESS_MODIFY);
        OUT_BATCH(batch, 0);
    }


    /*DW8. Indirect Object base address */
    if (gpe_context->indirect_state.bo)
        OUT_RELOC64(batch, gpe_context->indirect_state.bo,
                    I915_GEM_DOMAIN_SAMPLER,
                    0, BASE_ADDRESS_MODIFY);
    else {
        OUT_BATCH(batch, 0 | BASE_ADDRESS_MODIFY);
        OUT_BATCH(batch, 0);
    }


    /*DW10. Instruct base address */
    if (gpe_context->instruction_state.bo)
        OUT_RELOC64(batch, gpe_context->instruction_state.bo,
                    I915_GEM_DOMAIN_INSTRUCTION,
                    0, BASE_ADDRESS_MODIFY);
    else {
        OUT_BATCH(batch, 0 | BASE_ADDRESS_MODIFY);
        OUT_BATCH(batch, 0);
    }

    /* DW12. Size limitation */
    OUT_BATCH(batch, 0xFFFFF000 | BASE_ADDRESS_MODIFY);     //General State Access Upper Bound
    OUT_BATCH(batch, 0xFFFFF000 | BASE_ADDRESS_MODIFY);     //Dynamic State Access Upper Bound
    OUT_BATCH(batch, 0xFFFFF000 | BASE_ADDRESS_MODIFY);     //Indirect Object Access Upper Bound
    OUT_BATCH(batch, 0xFFFFF000 | BASE_ADDRESS_MODIFY);     //Instruction Access Upper Bound

    /*
      OUT_BATCH(batch, 0 | BASE_ADDRESS_MODIFY);                //LLC Coherent Base Address
      OUT_BATCH(batch, 0xFFFFF000 | BASE_ADDRESS_MODIFY );      //LLC Coherent Upper Bound
    */

    ADVANCE_BATCH(batch);
}

static void
gen8_gpe_vfe_state(VADriverContextP ctx,
                   struct i965_gpe_context *gpe_context,
                   struct intel_batchbuffer *batch)
{

    BEGIN_BATCH(batch, 9);

    OUT_BATCH(batch, CMD_MEDIA_VFE_STATE | (9 - 2));
    /* Scratch Space Base Pointer and Space */
    OUT_BATCH(batch, 0);
    OUT_BATCH(batch, 0);

    OUT_BATCH(batch,
              gpe_context->vfe_state.max_num_threads << 16 |    /* Maximum Number of Threads */
              gpe_context->vfe_state.num_urb_entries << 8 |     /* Number of URB Entries */
              gpe_context->vfe_state.gpgpu_mode << 2);          /* MEDIA Mode */
    OUT_BATCH(batch, 0);                                        /* Debug: Object ID */
    OUT_BATCH(batch,
              gpe_context->vfe_state.urb_entry_size << 16 |     /* URB Entry Allocation Size */
              gpe_context->vfe_state.curbe_allocation_size);    /* CURBE Allocation Size */

    /* the vfe_desc5/6/7 will decide whether the scoreboard is used. */
    OUT_BATCH(batch, gpe_context->vfe_desc5.dword);
    OUT_BATCH(batch, gpe_context->vfe_desc6.dword);
    OUT_BATCH(batch, gpe_context->vfe_desc7.dword);

    ADVANCE_BATCH(batch);

}


static void
gen8_gpe_curbe_load(VADriverContextP ctx,
                    struct i965_gpe_context *gpe_context,
                    struct intel_batchbuffer *batch)
{
    BEGIN_BATCH(batch, 4);

    OUT_BATCH(batch, CMD_MEDIA_CURBE_LOAD | (4 - 2));
    OUT_BATCH(batch, 0);
    OUT_BATCH(batch, ALIGN(gpe_context->curbe.length, 64));
    OUT_BATCH(batch, gpe_context->curbe.offset);

    ADVANCE_BATCH(batch);
}

static void
gen8_gpe_idrt(VADriverContextP ctx,
              struct i965_gpe_context *gpe_context,
              struct intel_batchbuffer *batch)
{
    BEGIN_BATCH(batch, 6);

    OUT_BATCH(batch, CMD_MEDIA_STATE_FLUSH);
    OUT_BATCH(batch, 0);

    OUT_BATCH(batch, CMD_MEDIA_INTERFACE_LOAD | (4 - 2));
    OUT_BATCH(batch, 0);
    OUT_BATCH(batch, gpe_context->idrt.max_entries * gpe_context->idrt.entry_size);
    OUT_BATCH(batch, gpe_context->idrt.offset);

    ADVANCE_BATCH(batch);
}


void
gen8_gpe_pipeline_setup(VADriverContextP ctx,
                        struct i965_gpe_context *gpe_context,
                        struct intel_batchbuffer *batch)
{
    intel_batchbuffer_emit_mi_flush(batch);

    i965_gpe_select(ctx, gpe_context, batch);
    gen8_gpe_state_base_address(ctx, gpe_context, batch);
    gen8_gpe_vfe_state(ctx, gpe_context, batch);
    gen8_gpe_curbe_load(ctx, gpe_context, batch);
    gen8_gpe_idrt(ctx, gpe_context, batch);
}

void
gen8_gpe_context_init(VADriverContextP ctx,
                      struct i965_gpe_context *gpe_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    dri_bo *bo;
    int bo_size;
    unsigned int start_offset, end_offset;

    dri_bo_unreference(gpe_context->surface_state_binding_table.bo);
    bo = dri_bo_alloc(i965->intel.bufmgr,
                      "surface state & binding table",
                      gpe_context->surface_state_binding_table.length,
                      4096);
    assert(bo);
    gpe_context->surface_state_binding_table.bo = bo;

    bo_size = gpe_context->idrt.max_entries * ALIGN(gpe_context->idrt.entry_size, 64) +
              ALIGN(gpe_context->curbe.length, 64) +
              gpe_context->sampler.max_entries * ALIGN(gpe_context->sampler.entry_size, 64);
    dri_bo_unreference(gpe_context->dynamic_state.bo);
    bo = dri_bo_alloc(i965->intel.bufmgr,
                      "surface state & binding table",
                      bo_size,
                      4096);
    assert(bo);
    gpe_context->dynamic_state.bo = bo;
    gpe_context->dynamic_state.bo_size = bo_size;

    end_offset = 0;
    gpe_context->dynamic_state.end_offset = 0;

    /* Constant buffer offset */
    start_offset = ALIGN(end_offset, 64);
    dri_bo_unreference(gpe_context->curbe.bo);
    gpe_context->curbe.bo = bo;
    dri_bo_reference(gpe_context->curbe.bo);
    gpe_context->curbe.offset = start_offset;
    end_offset = start_offset + gpe_context->curbe.length;

    /* Interface descriptor offset */
    start_offset = ALIGN(end_offset, 64);
    dri_bo_unreference(gpe_context->idrt.bo);
    gpe_context->idrt.bo = bo;
    dri_bo_reference(gpe_context->idrt.bo);
    gpe_context->idrt.offset = start_offset;
    end_offset = start_offset + ALIGN(gpe_context->idrt.entry_size, 64) * gpe_context->idrt.max_entries;

    /* Sampler state offset */
    start_offset = ALIGN(end_offset, 64);
    dri_bo_unreference(gpe_context->sampler.bo);
    gpe_context->sampler.bo = bo;
    dri_bo_reference(gpe_context->sampler.bo);
    gpe_context->sampler.offset = start_offset;
    end_offset = start_offset + ALIGN(gpe_context->sampler.entry_size, 64) * gpe_context->sampler.max_entries;

    /* update the end offset of dynamic_state */
    gpe_context->dynamic_state.end_offset = end_offset;
}


void
gen8_gpe_context_destroy(struct i965_gpe_context *gpe_context)
{
    dri_bo_unreference(gpe_context->surface_state_binding_table.bo);
    gpe_context->surface_state_binding_table.bo = NULL;

    dri_bo_unreference(gpe_context->instruction_state.bo);
    gpe_context->instruction_state.bo = NULL;

    dri_bo_unreference(gpe_context->dynamic_state.bo);
    gpe_context->dynamic_state.bo = NULL;

    dri_bo_unreference(gpe_context->indirect_state.bo);
    gpe_context->indirect_state.bo = NULL;

    dri_bo_unreference(gpe_context->curbe.bo);
    gpe_context->curbe.bo = NULL;

    dri_bo_unreference(gpe_context->idrt.bo);
    gpe_context->idrt.bo = NULL;

    dri_bo_unreference(gpe_context->sampler.bo);
    gpe_context->sampler.bo = NULL;
}


void
gen8_gpe_load_kernels(VADriverContextP ctx,
                      struct i965_gpe_context *gpe_context,
                      struct i965_kernel *kernel_list,
                      unsigned int num_kernels)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    int i, kernel_size = 0;
    unsigned int kernel_offset, end_offset;
    unsigned char *kernel_ptr;
    struct i965_kernel *kernel;

    assert(num_kernels <= MAX_GPE_KERNELS);
    memcpy(gpe_context->kernels, kernel_list, sizeof(*kernel_list) * num_kernels);
    gpe_context->num_kernels = num_kernels;

    for (i = 0; i < num_kernels; i++) {
        kernel = &gpe_context->kernels[i];

        kernel_size += ALIGN(kernel->size, 64);
    }

    gpe_context->instruction_state.bo = dri_bo_alloc(i965->intel.bufmgr,
                                                     "kernel shader",
                                                     kernel_size,
                                                     0x1000);
    if (gpe_context->instruction_state.bo == NULL) {
        WARN_ONCE("failure to allocate the buffer space for kernel shader\n");
        return;
    }

    assert(gpe_context->instruction_state.bo);

    gpe_context->instruction_state.bo_size = kernel_size;
    gpe_context->instruction_state.end_offset = 0;
    end_offset = 0;

    dri_bo_map(gpe_context->instruction_state.bo, 1);
    kernel_ptr = (unsigned char *)(gpe_context->instruction_state.bo->virtual);
    for (i = 0; i < num_kernels; i++) {
        kernel_offset = ALIGN(end_offset, 64);
        kernel = &gpe_context->kernels[i];
        kernel->kernel_offset = kernel_offset;

        if (kernel->size) {
            memcpy(kernel_ptr + kernel_offset, kernel->bin, kernel->size);

            end_offset = kernel_offset + kernel->size;
        }
    }

    gpe_context->instruction_state.end_offset = end_offset;

    dri_bo_unmap(gpe_context->instruction_state.bo);

    return;
}

static void
gen9_gpe_state_base_address(VADriverContextP ctx,
                            struct i965_gpe_context *gpe_context,
                            struct intel_batchbuffer *batch)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    BEGIN_BATCH(batch, 19);

    OUT_BATCH(batch, CMD_STATE_BASE_ADDRESS | (19 - 2));

    OUT_BATCH(batch, 0 | BASE_ADDRESS_MODIFY);              //General State Base Address
    OUT_BATCH(batch, 0);
    OUT_BATCH(batch, 0);

    /*DW4 Surface state base address */
    OUT_RELOC64(batch, gpe_context->surface_state_binding_table.bo, I915_GEM_DOMAIN_INSTRUCTION, 0, BASE_ADDRESS_MODIFY | (i965->intel.mocs_state << 4)); /* Surface state base address */

    /*DW6. Dynamic state base address */
    if (gpe_context->dynamic_state.bo)
        OUT_RELOC64(batch, gpe_context->dynamic_state.bo,
                    I915_GEM_DOMAIN_RENDER | I915_GEM_DOMAIN_SAMPLER,
                    I915_GEM_DOMAIN_RENDER,
                    BASE_ADDRESS_MODIFY | (i965->intel.mocs_state << 4));
    else {
        OUT_BATCH(batch, 0 | BASE_ADDRESS_MODIFY);
        OUT_BATCH(batch, 0);
    }


    /*DW8. Indirect Object base address */
    if (gpe_context->indirect_state.bo)
        OUT_RELOC64(batch, gpe_context->indirect_state.bo,
                    I915_GEM_DOMAIN_SAMPLER,
                    0, BASE_ADDRESS_MODIFY | (i965->intel.mocs_state << 4));
    else {
        OUT_BATCH(batch, 0 | BASE_ADDRESS_MODIFY);
        OUT_BATCH(batch, 0);
    }


    /*DW10. Instruct base address */
    if (gpe_context->instruction_state.bo)
        OUT_RELOC64(batch, gpe_context->instruction_state.bo,
                    I915_GEM_DOMAIN_INSTRUCTION,
                    0, BASE_ADDRESS_MODIFY | (i965->intel.mocs_state << 4));
    else {
        OUT_BATCH(batch, 0 | BASE_ADDRESS_MODIFY);
        OUT_BATCH(batch, 0);
    }


    /* DW12. Size limitation */
    OUT_BATCH(batch, 0xFFFFF000 | BASE_ADDRESS_MODIFY);     //General State Access Upper Bound
    OUT_BATCH(batch, 0xFFFFF000 | BASE_ADDRESS_MODIFY);     //Dynamic State Access Upper Bound
    OUT_BATCH(batch, 0xFFFFF000 | BASE_ADDRESS_MODIFY);     //Indirect Object Access Upper Bound
    OUT_BATCH(batch, 0xFFFFF000 | BASE_ADDRESS_MODIFY);     //Instruction Access Upper Bound

    /* the bindless surface state address */
    OUT_BATCH(batch, 0 | BASE_ADDRESS_MODIFY);
    OUT_BATCH(batch, 0);
    OUT_BATCH(batch, 0xFFFFF000);

    ADVANCE_BATCH(batch);
}

static void
gen9_gpe_select(VADriverContextP ctx,
                struct i965_gpe_context *gpe_context,
                struct intel_batchbuffer *batch)
{
    BEGIN_BATCH(batch, 1);
    OUT_BATCH(batch, CMD_PIPELINE_SELECT | PIPELINE_SELECT_MEDIA |
              GEN9_PIPELINE_SELECTION_MASK |
              GEN9_MEDIA_DOP_GATE_OFF |
              GEN9_MEDIA_DOP_GATE_MASK |
              GEN9_FORCE_MEDIA_AWAKE_ON |
              GEN9_FORCE_MEDIA_AWAKE_MASK);
    ADVANCE_BATCH(batch);
}

void
gen9_gpe_pipeline_setup(VADriverContextP ctx,
                        struct i965_gpe_context *gpe_context,
                        struct intel_batchbuffer *batch)
{
    intel_batchbuffer_emit_mi_flush(batch);

    gen9_gpe_select(ctx, gpe_context, batch);
    gen9_gpe_state_base_address(ctx, gpe_context, batch);
    gen8_gpe_vfe_state(ctx, gpe_context, batch);
    gen8_gpe_curbe_load(ctx, gpe_context, batch);
    gen8_gpe_idrt(ctx, gpe_context, batch);
}

void
gen9_gpe_pipeline_end(VADriverContextP ctx,
                      struct i965_gpe_context *gpe_context,
                      struct intel_batchbuffer *batch)
{
    BEGIN_BATCH(batch, 1);
    OUT_BATCH(batch, CMD_PIPELINE_SELECT | PIPELINE_SELECT_MEDIA |
              GEN9_PIPELINE_SELECTION_MASK |
              GEN9_MEDIA_DOP_GATE_ON |
              GEN9_MEDIA_DOP_GATE_MASK |
              GEN9_FORCE_MEDIA_AWAKE_OFF |
              GEN9_FORCE_MEDIA_AWAKE_MASK);
    ADVANCE_BATCH(batch);
}

Bool
i965_allocate_gpe_resource(dri_bufmgr *bufmgr,
                           struct i965_gpe_resource *res,
                           int size,
                           const char *name)
{
    if (!res || !size)
        return false;

    res->size = size;
    res->bo = dri_bo_alloc(bufmgr, name, res->size, 4096);
    res->map = NULL;

    return (res->bo != NULL);
}

void
i965_object_surface_to_2d_gpe_resource_with_align(struct i965_gpe_resource *res,
                                                  struct object_surface *obj_surface,
                                                  unsigned int alignment)
{
    unsigned int swizzle;

    res->type = I965_GPE_RESOURCE_2D;
    res->width = ALIGN(obj_surface->orig_width, (1 << alignment));
    res->height = ALIGN(obj_surface->orig_height, (1 << alignment));
    res->pitch = obj_surface->width;
    res->size = obj_surface->size;
    res->cb_cr_pitch = obj_surface->cb_cr_pitch;
    res->x_cb_offset = obj_surface->x_cb_offset;
    res->y_cb_offset = obj_surface->y_cb_offset;
    res->bo = obj_surface->bo;
    res->map = NULL;

    dri_bo_reference(res->bo);
    dri_bo_get_tiling(obj_surface->bo, &res->tiling, &swizzle);
}

void
i965_object_surface_to_2d_gpe_resource(struct i965_gpe_resource *res,
                                       struct object_surface *obj_surface)
{
    i965_object_surface_to_2d_gpe_resource_with_align(res, obj_surface, 0);
}

void
i965_dri_object_to_buffer_gpe_resource(struct i965_gpe_resource *res,
                                       dri_bo *bo)
{
    unsigned int swizzle;

    res->type = I965_GPE_RESOURCE_BUFFER;
    res->width = bo->size;
    res->height = 1;
    res->pitch = res->width;
    res->size = res->pitch * res->width;
    res->bo = bo;
    res->map = NULL;

    dri_bo_reference(res->bo);
    dri_bo_get_tiling(res->bo, &res->tiling, &swizzle);
}

void
i965_dri_object_to_2d_gpe_resource(struct i965_gpe_resource *res,
                                   dri_bo *bo,
                                   unsigned int width,
                                   unsigned int height,
                                   unsigned int pitch)
{
    unsigned int swizzle;

    res->type = I965_GPE_RESOURCE_2D;
    res->width = width;
    res->height = height;
    res->pitch = pitch;
    res->size = res->pitch * res->width;
    res->bo = bo;
    res->map = NULL;

    dri_bo_reference(res->bo);
    dri_bo_get_tiling(res->bo, &res->tiling, &swizzle);
}

void
i965_zero_gpe_resource(struct i965_gpe_resource *res)
{
    if (res->bo) {
        dri_bo_map(res->bo, 1);
        memset(res->bo->virtual, 0, res->size);
        dri_bo_unmap(res->bo);
    }
}

void
i965_free_gpe_resource(struct i965_gpe_resource *res)
{
    dri_bo_unreference(res->bo);
    res->bo = NULL;
    res->map = NULL;
}

void *
i965_map_gpe_resource(struct i965_gpe_resource *res)
{
    int ret;

    if (res->bo) {
        ret = dri_bo_map(res->bo, 1);

        if (ret == 0)
            res->map = res->bo->virtual;
        else
            res->map = NULL;
    } else
        res->map = NULL;

    return res->map;
}

void
i965_unmap_gpe_resource(struct i965_gpe_resource *res)
{
    if (res->bo && res->map)
        dri_bo_unmap(res->bo);

    res->map = NULL;
}

void
gen8_gpe_mi_flush_dw(VADriverContextP ctx,
                     struct intel_batchbuffer *batch,
                     struct gpe_mi_flush_dw_parameter *params)
{
    int video_pipeline_cache_invalidate = 0;
    int post_sync_operation = MI_FLUSH_DW_NOWRITE;

    if (params->video_pipeline_cache_invalidate)
        video_pipeline_cache_invalidate = MI_FLUSH_DW_VIDEO_PIPELINE_CACHE_INVALIDATE;

    if (params->bo)
        post_sync_operation = MI_FLUSH_DW_WRITE_QWORD;

    __OUT_BATCH(batch, (MI_FLUSH_DW2 |
                        video_pipeline_cache_invalidate |
                        post_sync_operation |
                        (5 - 2))); /* Always use PPGTT */

    if (params->bo) {
        __OUT_RELOC64(batch,
                      params->bo,
                      I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER,
                      params->offset);
    } else {
        __OUT_BATCH(batch, 0);
        __OUT_BATCH(batch, 0);
    }

    __OUT_BATCH(batch, params->dw0);
    __OUT_BATCH(batch, params->dw1);
}

void
gen8_gpe_mi_store_data_imm(VADriverContextP ctx,
                           struct intel_batchbuffer *batch,
                           struct gpe_mi_store_data_imm_parameter *params)
{
    if (params->is_qword) {
        __OUT_BATCH(batch, MI_STORE_DATA_IMM |
                    (1 << 21) |
                    (5 - 2)); /* Always use PPGTT */
    } else {
        __OUT_BATCH(batch, MI_STORE_DATA_IMM | (4 - 2)); /* Always use PPGTT */
    }

    __OUT_RELOC64(batch,
                  params->bo,
                  I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER,
                  params->offset);
    __OUT_BATCH(batch, params->dw0);

    if (params->is_qword)
        __OUT_BATCH(batch, params->dw1);
}

void
gen8_gpe_mi_store_register_mem(VADriverContextP ctx,
                               struct intel_batchbuffer *batch,
                               struct gpe_mi_store_register_mem_parameter *params)
{
    __OUT_BATCH(batch, (MI_STORE_REGISTER_MEM | (4 - 2))); /* Always use PPGTT */
    __OUT_BATCH(batch, params->mmio_offset);
    __OUT_RELOC64(batch,
                  params->bo,
                  I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER,
                  params->offset);
}

void
gen8_gpe_mi_load_register_mem(VADriverContextP ctx,
                              struct intel_batchbuffer *batch,
                              struct gpe_mi_load_register_mem_parameter *params)
{
    __OUT_BATCH(batch, (MI_LOAD_REGISTER_MEM | (4 - 2))); /* Always use PPGTT */
    __OUT_BATCH(batch, params->mmio_offset);
    __OUT_RELOC64(batch,
                  params->bo,
                  I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER,
                  params->offset);
}

void
gen8_gpe_mi_load_register_imm(VADriverContextP ctx,
                              struct intel_batchbuffer *batch,
                              struct gpe_mi_load_register_imm_parameter *params)
{
    __OUT_BATCH(batch, (MI_LOAD_REGISTER_IMM | (3 - 2)));
    __OUT_BATCH(batch, params->mmio_offset);
    __OUT_BATCH(batch, params->data);
}

void
gen8_gpe_mi_load_register_reg(VADriverContextP ctx,
                              struct intel_batchbuffer *batch,
                              struct gpe_mi_load_register_reg_parameter *params)
{
    __OUT_BATCH(batch, (MI_LOAD_REGISTER_REG | (3 - 2)));
    __OUT_BATCH(batch, params->src_mmio_offset);
    __OUT_BATCH(batch, params->dst_mmio_offset);
}

void
gen9_gpe_mi_math(VADriverContextP ctx,
                 struct intel_batchbuffer *batch,
                 struct gpe_mi_math_parameter *params)
{
    __OUT_BATCH(batch, (MI_MATH | (params->num_instructions - 1)));
    intel_batchbuffer_data(batch, params->instruction_list, params->num_instructions * 4);
}

void
gen9_gpe_mi_conditional_batch_buffer_end(VADriverContextP ctx,
                                         struct intel_batchbuffer *batch,
                                         struct gpe_mi_conditional_batch_buffer_end_parameter *params)
{
    int compare_mask_mode_enabled = MI_COMPARE_MASK_MODE_ENANBLED;

    if (params->compare_mask_mode_disabled)
        compare_mask_mode_enabled = 0;

    __OUT_BATCH(batch, (MI_CONDITIONAL_BATCH_BUFFER_END |
                        (1 << 21) |
                        compare_mask_mode_enabled |
                        (4 - 2))); /* Always use PPGTT */
    __OUT_BATCH(batch, params->compare_data);
    __OUT_RELOC64(batch,
                  params->bo,
                  I915_GEM_DOMAIN_RENDER | I915_GEM_DOMAIN_INSTRUCTION, 0,
                  params->offset);
}

void
gen8_gpe_mi_batch_buffer_start(VADriverContextP ctx,
                               struct intel_batchbuffer *batch,
                               struct gpe_mi_batch_buffer_start_parameter *params)
{
    __OUT_BATCH(batch, (MI_BATCH_BUFFER_START |
                        (!!params->is_second_level << 22) |
                        (!params->use_global_gtt << 8) |
                        (1 << 0)));
    __OUT_RELOC64(batch,
                  params->bo,
                  I915_GEM_DOMAIN_RENDER | I915_GEM_DOMAIN_INSTRUCTION, 0,
                  params->offset);
}

void
gen8_gpe_context_set_dynamic_buffer(VADriverContextP ctx,
                                    struct i965_gpe_context *gpe_context,
                                    struct gpe_dynamic_state_parameter *ds)
{
    if (!ds->bo || !gpe_context)
        return;

    dri_bo_unreference(gpe_context->dynamic_state.bo);
    gpe_context->dynamic_state.bo = ds->bo;
    dri_bo_reference(gpe_context->dynamic_state.bo);
    gpe_context->dynamic_state.bo_size = ds->bo_size;

    /* curbe buffer is a part of the dynamic buffer */
    dri_bo_unreference(gpe_context->curbe.bo);
    gpe_context->curbe.bo = ds->bo;
    dri_bo_reference(gpe_context->curbe.bo);
    gpe_context->curbe.offset = ds->curbe_offset;

    /* idrt buffer is a part of the dynamic buffer */
    dri_bo_unreference(gpe_context->idrt.bo);
    gpe_context->idrt.bo = ds->bo;
    dri_bo_reference(gpe_context->idrt.bo);
    gpe_context->idrt.offset = ds->idrt_offset;

    /* sampler buffer is a part of the dynamic buffer */
    dri_bo_unreference(gpe_context->sampler.bo);
    gpe_context->sampler.bo = ds->bo;
    dri_bo_reference(gpe_context->sampler.bo);
    gpe_context->sampler.offset = ds->sampler_offset;

    return;
}

void *
i965_gpe_context_map_curbe(struct i965_gpe_context *gpe_context)
{
    dri_bo_map(gpe_context->curbe.bo, 1);

    return (char *)gpe_context->curbe.bo->virtual + gpe_context->curbe.offset;
}

void
i965_gpe_context_unmap_curbe(struct i965_gpe_context *gpe_context)
{
    dri_bo_unmap(gpe_context->curbe.bo);
}

void
gen9_gpe_reset_binding_table(VADriverContextP ctx,
                             struct i965_gpe_context *gpe_context)
{
    unsigned int *binding_table;
    unsigned int binding_table_offset = gpe_context->surface_state_binding_table.binding_table_offset;
    int i;

    dri_bo_map(gpe_context->surface_state_binding_table.bo, 1);
    binding_table = (unsigned int*)((char *)gpe_context->surface_state_binding_table.bo->virtual + binding_table_offset);

    for (i = 0; i < gpe_context->surface_state_binding_table.max_entries; i++) {
        *(binding_table + i) = gpe_context->surface_state_binding_table.surface_state_offset + i * SURFACE_STATE_PADDED_SIZE_GEN9;
    }

    dri_bo_unmap(gpe_context->surface_state_binding_table.bo);
}

void
gen8_gpe_setup_interface_data(VADriverContextP ctx,
                              struct i965_gpe_context *gpe_context)
{
    struct gen8_interface_descriptor_data *desc;
    int i;
    dri_bo *bo;
    unsigned char *desc_ptr;

    bo = gpe_context->idrt.bo;
    dri_bo_map(bo, 1);
    assert(bo->virtual);
    desc_ptr = (unsigned char *)bo->virtual + gpe_context->idrt.offset;
    desc = (struct gen8_interface_descriptor_data *)desc_ptr;

    for (i = 0; i < gpe_context->num_kernels; i++) {
        struct i965_kernel *kernel;

        kernel = &gpe_context->kernels[i];
        assert(sizeof(*desc) == 32);

        /*Setup the descritor table*/
        memset(desc, 0, sizeof(*desc));
        desc->desc0.kernel_start_pointer = kernel->kernel_offset >> 6;
        desc->desc3.sampler_count = 0;
        desc->desc3.sampler_state_pointer = (gpe_context->sampler.offset >> 5);
        desc->desc4.binding_table_entry_count = 0;
        desc->desc4.binding_table_pointer = (gpe_context->surface_state_binding_table.binding_table_offset >> 5);
        desc->desc5.constant_urb_entry_read_offset = 0;
        desc->desc5.constant_urb_entry_read_length = ALIGN(gpe_context->curbe.length, 32) >> 5; // in registers

        desc++;
    }

    dri_bo_unmap(bo);
}

static void
gen9_gpe_set_surface_tiling(struct gen9_surface_state *ss, unsigned int tiling)
{
    switch (tiling) {
    case I915_TILING_NONE:
        ss->ss0.tiled_surface = 0;
        ss->ss0.tile_walk = 0;
        break;
    case I915_TILING_X:
        ss->ss0.tiled_surface = 1;
        ss->ss0.tile_walk = I965_TILEWALK_XMAJOR;
        break;
    case I915_TILING_Y:
        ss->ss0.tiled_surface = 1;
        ss->ss0.tile_walk = I965_TILEWALK_YMAJOR;
        break;
    }
}

static void
gen9_gpe_set_surface2_tiling(struct gen9_surface_state2 *ss, unsigned int tiling)
{
    switch (tiling) {
    case I915_TILING_NONE:
        ss->ss2.tiled_surface = 0;
        ss->ss2.tile_walk = 0;
        break;
    case I915_TILING_X:
        ss->ss2.tiled_surface = 1;
        ss->ss2.tile_walk = I965_TILEWALK_XMAJOR;
        break;
    case I915_TILING_Y:
        ss->ss2.tiled_surface = 1;
        ss->ss2.tile_walk = I965_TILEWALK_YMAJOR;
        break;
    }
}

static void
gen9_gpe_set_2d_surface_state(struct gen9_surface_state *ss,
                              unsigned int cacheability_control,
                              unsigned int format,
                              unsigned int tiling,
                              unsigned int width,
                              unsigned int height,
                              unsigned int pitch,
                              uint64_t base_offset,
                              unsigned int y_offset)
{
    memset(ss, 0, sizeof(*ss));

    /* Always set 1(align 4 mode) */
    ss->ss0.vertical_alignment = 1;
    ss->ss0.horizontal_alignment = 1;

    ss->ss0.surface_format = format;
    ss->ss0.surface_type = I965_SURFACE_2D;

    ss->ss1.surface_mocs = cacheability_control;

    ss->ss2.width = width - 1;
    ss->ss2.height = height - 1;

    ss->ss3.pitch = pitch - 1;

    ss->ss5.y_offset = y_offset;

    ss->ss7.shader_chanel_select_a = HSW_SCS_ALPHA;
    ss->ss7.shader_chanel_select_b = HSW_SCS_BLUE;
    ss->ss7.shader_chanel_select_g = HSW_SCS_GREEN;
    ss->ss7.shader_chanel_select_r = HSW_SCS_RED;

    ss->ss8.base_addr = (uint32_t)base_offset;
    ss->ss9.base_addr_high = (uint32_t)(base_offset >> 32);

    gen9_gpe_set_surface_tiling(ss, tiling);
}

/* This is only for NV12 format */
static void
gen9_gpe_set_adv_surface_state(struct gen9_surface_state2 *ss,
                               unsigned int v_direction,
                               unsigned int cacheability_control,
                               unsigned int format,
                               unsigned int tiling,
                               unsigned int width,
                               unsigned int height,
                               unsigned int pitch,
                               uint64_t base_offset,
                               unsigned int y_cb_offset)
{
    memset(ss, 0, sizeof(*ss));

    ss->ss1.cbcr_pixel_offset_v_direction = v_direction;
    ss->ss1.width = width - 1;
    ss->ss1.height = height - 1;

    ss->ss2.surface_format = format;
    ss->ss2.interleave_chroma = 1;
    ss->ss2.pitch = pitch - 1;

    ss->ss3.y_offset_for_cb = y_cb_offset;

    ss->ss5.surface_object_mocs = cacheability_control;

    ss->ss6.base_addr = (uint32_t)base_offset;
    ss->ss7.base_addr_high = (uint32_t)(base_offset >> 32);

    gen9_gpe_set_surface2_tiling(ss, tiling);
}

static void
gen9_gpe_set_buffer2_surface_state(struct gen9_surface_state *ss,
                                   unsigned int cacheability_control,
                                   unsigned int format,
                                   unsigned int size,
                                   unsigned int pitch,
                                   uint64_t base_offset)
{
    memset(ss, 0, sizeof(*ss));

    ss->ss0.surface_format = format;
    ss->ss0.surface_type = I965_SURFACE_BUFFER;

    ss->ss1.surface_mocs = cacheability_control;

    ss->ss2.width = (size - 1) & 0x7F;
    ss->ss2.height = ((size - 1) & 0x1FFF80) >> 7;

    ss->ss3.depth = ((size - 1) & 0xFE00000) >> 21;
    ss->ss3.pitch = pitch - 1;

    ss->ss7.shader_chanel_select_a = HSW_SCS_ALPHA;
    ss->ss7.shader_chanel_select_b = HSW_SCS_BLUE;
    ss->ss7.shader_chanel_select_g = HSW_SCS_GREEN;
    ss->ss7.shader_chanel_select_r = HSW_SCS_RED;

    ss->ss8.base_addr = (uint32_t)base_offset;
    ss->ss9.base_addr_high = (uint32_t)(base_offset >> 32);
}

void
gen9_gpe_context_add_surface(struct i965_gpe_context *gpe_context,
                             struct i965_gpe_surface *gpe_surface,
                             int index)
{
    char *buf;
    unsigned int tiling, swizzle, width, height, pitch, tile_alignment, y_offset = 0;
    unsigned int surface_state_offset = gpe_context->surface_state_binding_table.surface_state_offset +
                                        index * SURFACE_STATE_PADDED_SIZE_GEN9;
    unsigned int binding_table_offset = gpe_context->surface_state_binding_table.binding_table_offset +
                                        index * 4;
    struct i965_gpe_resource *gpe_resource = gpe_surface->gpe_resource;

    dri_bo_get_tiling(gpe_resource->bo, &tiling, &swizzle);

    dri_bo_map(gpe_context->surface_state_binding_table.bo, 1);
    buf = (char *)gpe_context->surface_state_binding_table.bo->virtual;
    *((unsigned int *)(buf + binding_table_offset)) = surface_state_offset;

    if (gpe_surface->is_2d_surface && gpe_surface->is_override_offset) {
        struct gen9_surface_state *ss = (struct gen9_surface_state *)(buf + surface_state_offset);

        width = gpe_resource->width;
        height = gpe_resource->height;
        pitch = gpe_resource->pitch;

        if (gpe_surface->is_media_block_rw) {
            if (gpe_surface->is_16bpp)
                width = (ALIGN(width * 2, 4) >> 2);
            else
                width = (ALIGN(width, 4) >> 2);
        }


        gen9_gpe_set_2d_surface_state(ss,
                                      gpe_surface->cacheability_control,
                                      gpe_surface->format,
                                      tiling,
                                      width, height, pitch,
                                      gpe_resource->bo->offset64 + gpe_surface->offset,
                                      0);

        dri_bo_emit_reloc(gpe_context->surface_state_binding_table.bo,
                          I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER,
                          gpe_surface->offset,
                          surface_state_offset + offsetof(struct gen9_surface_state, ss8),
                          gpe_resource->bo);
    } else if (gpe_surface->is_2d_surface && gpe_surface->is_uv_surface) {
        unsigned int cbcr_offset;
        struct gen9_surface_state *ss = (struct gen9_surface_state *)(buf + surface_state_offset);

        width = gpe_resource->width;
        height = gpe_resource->height / 2;
        pitch = gpe_resource->pitch;

        if (gpe_surface->is_media_block_rw) {
            if (gpe_surface->is_16bpp)
                width = (ALIGN(width * 2, 4) >> 2);
            else
                width = (ALIGN(width, 4) >> 2);
        }

        if (tiling == I915_TILING_Y) {
            tile_alignment = 32;
        } else if (tiling == I915_TILING_X) {
            tile_alignment = 8;
        } else
            tile_alignment = 1;

        y_offset = (gpe_resource->y_cb_offset % tile_alignment);
        cbcr_offset = ALIGN_FLOOR(gpe_resource->y_cb_offset, tile_alignment) * pitch;

        gen9_gpe_set_2d_surface_state(ss,
                                      gpe_surface->cacheability_control,
                                      I965_SURFACEFORMAT_R16_UINT,
                                      tiling,
                                      width, height, pitch,
                                      gpe_resource->bo->offset64 + cbcr_offset,
                                      y_offset);

        dri_bo_emit_reloc(gpe_context->surface_state_binding_table.bo,
                          I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER,
                          cbcr_offset,
                          surface_state_offset + offsetof(struct gen9_surface_state, ss8),
                          gpe_resource->bo);
    } else if (gpe_surface->is_2d_surface) {
        struct gen9_surface_state *ss = (struct gen9_surface_state *)(buf + surface_state_offset);

        width = gpe_resource->width;
        height = gpe_resource->height;
        pitch = gpe_resource->pitch;

        if (gpe_surface->is_media_block_rw) {
            if (gpe_surface->is_16bpp)
                width = (ALIGN(width * 2, 4) >> 2);
            else
                width = (ALIGN(width, 4) >> 2);
        }

        gen9_gpe_set_2d_surface_state(ss,
                                      gpe_surface->cacheability_control,
                                      gpe_surface->format,
                                      tiling,
                                      width, height, pitch,
                                      gpe_resource->bo->offset64,
                                      y_offset);

        dri_bo_emit_reloc(gpe_context->surface_state_binding_table.bo,
                          I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER,
                          0,
                          surface_state_offset + offsetof(struct gen9_surface_state, ss8),
                          gpe_resource->bo);
    } else if (gpe_surface->is_adv_surface) {
        struct gen9_surface_state2 *ss = (struct gen9_surface_state2 *)(buf + surface_state_offset);

        width = gpe_resource->width;
        height = gpe_resource->height;
        pitch = gpe_resource->pitch;

        gen9_gpe_set_adv_surface_state(ss,
                                       gpe_surface->v_direction,
                                       gpe_surface->cacheability_control,
                                       MFX_SURFACE_PLANAR_420_8,
                                       tiling,
                                       width, height, pitch,
                                       gpe_resource->bo->offset64,
                                       gpe_resource->y_cb_offset);

        dri_bo_emit_reloc(gpe_context->surface_state_binding_table.bo,
                          I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER,
                          0,
                          surface_state_offset + offsetof(struct gen9_surface_state2, ss6),
                          gpe_resource->bo);
    } else {
        struct gen9_surface_state *ss = (struct gen9_surface_state *)(buf + surface_state_offset);
        unsigned int format;

        assert(gpe_surface->is_buffer);

        if (gpe_surface->is_raw_buffer) {
            format = I965_SURFACEFORMAT_RAW;
            pitch = 1;
        } else {
            format = I965_SURFACEFORMAT_R32_UINT;
            pitch = sizeof(unsigned int);
        }

        gen9_gpe_set_buffer2_surface_state(ss,
                                           gpe_surface->cacheability_control,
                                           format,
                                           gpe_surface->size,
                                           pitch,
                                           gpe_resource->bo->offset64 + gpe_surface->offset);

        dri_bo_emit_reloc(gpe_context->surface_state_binding_table.bo,
                          I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER,
                          gpe_surface->offset,
                          surface_state_offset + offsetof(struct gen9_surface_state, ss8),
                          gpe_resource->bo);
    }

    dri_bo_unmap(gpe_context->surface_state_binding_table.bo);
}

bool
i965_gpe_allocate_2d_resource(dri_bufmgr *bufmgr,
                              struct i965_gpe_resource *res,
                              int width,
                              int height,
                              int pitch,
                              const char *name)
{
    int bo_size;

    if (!res)
        return false;

    res->type = I965_GPE_RESOURCE_2D;
    res->width = width;
    res->height = height;
    res->pitch = pitch;

    bo_size = ALIGN(height, 16) * pitch;
    res->size = bo_size;

    res->bo = dri_bo_alloc(bufmgr, name, res->size, 4096);
    res->map = NULL;

    return true;
}

void
gen8_gpe_media_state_flush(VADriverContextP ctx,
                           struct i965_gpe_context *gpe_context,
                           struct intel_batchbuffer *batch)
{
    BEGIN_BATCH(batch, 2);

    OUT_BATCH(batch, CMD_MEDIA_STATE_FLUSH | (2 - 2));
    OUT_BATCH(batch, 0);

    ADVANCE_BATCH(batch);
}

void
gen8_gpe_media_object(VADriverContextP ctx,
                      struct i965_gpe_context *gpe_context,
                      struct intel_batchbuffer *batch,
                      struct gpe_media_object_parameter *param)
{
    int batch_size, subdata_size;

    batch_size = 6;
    subdata_size = 0;
    if (param->pinline_data && param->inline_size) {
        subdata_size = ALIGN(param->inline_size, 4);
        batch_size += subdata_size / 4;
    }
    BEGIN_BATCH(batch, batch_size);
    OUT_BATCH(batch, CMD_MEDIA_OBJECT | (batch_size - 2));
    OUT_BATCH(batch, param->interface_offset);
    OUT_BATCH(batch, param->use_scoreboard << 21);
    OUT_BATCH(batch, 0);
    OUT_BATCH(batch, (param->scoreboard_y << 16 |
                      param->scoreboard_x));
    OUT_BATCH(batch, param->scoreboard_mask);

    if (subdata_size)
        intel_batchbuffer_data(batch, param->pinline_data, subdata_size);

    ADVANCE_BATCH(batch);
}

void
gen8_gpe_media_object_walker(VADriverContextP ctx,
                             struct i965_gpe_context *gpe_context,
                             struct intel_batchbuffer *batch,
                             struct gpe_media_object_walker_parameter *param)
{
    int walker_length;

    walker_length = 17;
    if (param->inline_size)
        walker_length += ALIGN(param->inline_size, 4) / 4;
    BEGIN_BATCH(batch, walker_length);
    OUT_BATCH(batch, CMD_MEDIA_OBJECT_WALKER | (walker_length - 2));
    OUT_BATCH(batch, param->interface_offset);
    OUT_BATCH(batch, param->use_scoreboard << 21);
    OUT_BATCH(batch, 0);
    OUT_BATCH(batch, 0);
    OUT_BATCH(batch, (param->group_id_loop_select << 8 |
                      param->scoreboard_mask)); // DW5
    OUT_BATCH(batch, (param->color_count_minus1 << 24 |
                      param->middle_loop_extra_steps << 16 |
                      param->mid_loop_unit_y << 12 |
                      param->mid_loop_unit_x << 8));
    OUT_BATCH(batch, ((param->global_loop_exec_count & 0x3ff) << 16 |
                      (param->local_loop_exec_count & 0x3ff)));
    OUT_BATCH(batch, param->block_resolution.value);
    OUT_BATCH(batch, param->local_start.value);
    OUT_BATCH(batch, 0); // DW10
    OUT_BATCH(batch, param->local_outer_loop_stride.value);
    OUT_BATCH(batch, param->local_inner_loop_unit.value);
    OUT_BATCH(batch, param->global_resolution.value);
    OUT_BATCH(batch, param->global_start.value);
    OUT_BATCH(batch, param->global_outer_loop_stride.value);
    OUT_BATCH(batch, param->global_inner_loop_unit.value);

    if (param->pinline_data && param->inline_size)
        intel_batchbuffer_data(batch, param->pinline_data, ALIGN(param->inline_size, 4));

    ADVANCE_BATCH(batch);
}


void
intel_vpp_init_media_object_walker_parameter(struct intel_vpp_kernel_walker_parameter *kernel_walker_param,
                                             struct gpe_media_object_walker_parameter *walker_param)
{
    memset(walker_param, 0, sizeof(*walker_param));

    walker_param->use_scoreboard = kernel_walker_param->use_scoreboard;

    walker_param->block_resolution.x = kernel_walker_param->resolution_x;
    walker_param->block_resolution.y = kernel_walker_param->resolution_y;

    walker_param->global_resolution.x = kernel_walker_param->resolution_x;
    walker_param->global_resolution.y = kernel_walker_param->resolution_y;

    walker_param->global_outer_loop_stride.x = kernel_walker_param->resolution_x;
    walker_param->global_outer_loop_stride.y = 0;

    walker_param->global_inner_loop_unit.x = 0;
    walker_param->global_inner_loop_unit.y = kernel_walker_param->resolution_y;

    walker_param->local_loop_exec_count = 0xFFFF;  //MAX VALUE
    walker_param->global_loop_exec_count = 0xFFFF;  //MAX VALUE

    if (kernel_walker_param->no_dependency) {
        /* The no_dependency is used for VPP */
        walker_param->scoreboard_mask = 0;
        walker_param->use_scoreboard = 0;
        // Raster scan walking pattern
        walker_param->local_outer_loop_stride.x = 0;
        walker_param->local_outer_loop_stride.y = 1;
        walker_param->local_inner_loop_unit.x = 1;
        walker_param->local_inner_loop_unit.y = 0;
        walker_param->local_end.x = kernel_walker_param->resolution_x - 1;
        walker_param->local_end.y = 0;
    } else {
        walker_param->local_end.x = 0;
        walker_param->local_end.y = 0;

        // 26 degree
        walker_param->scoreboard_mask = 0x0F;
        walker_param->local_outer_loop_stride.x = 1;
        walker_param->local_outer_loop_stride.y = 0;
        walker_param->local_inner_loop_unit.x = -2;
        walker_param->local_inner_loop_unit.y = 1;
    }
}

void
gen8_gpe_reset_binding_table(VADriverContextP ctx, struct i965_gpe_context *gpe_context)
{
    unsigned int *binding_table;
    unsigned int binding_table_offset = gpe_context->surface_state_binding_table.binding_table_offset;
    int i;

    dri_bo_map(gpe_context->surface_state_binding_table.bo, 1);
    binding_table = (unsigned int*)((char *)gpe_context->surface_state_binding_table.bo->virtual + binding_table_offset);

    for (i = 0; i < gpe_context->surface_state_binding_table.max_entries; i++) {
        *(binding_table + i) = gpe_context->surface_state_binding_table.surface_state_offset + i * SURFACE_STATE_PADDED_SIZE_GEN8;
    }

    dri_bo_unmap(gpe_context->surface_state_binding_table.bo);
}

static void
gen8_gpe_set_2d_surface_state(struct gen8_surface_state *ss,
                              unsigned int vert_line_stride_offset,
                              unsigned int vert_line_stride,
                              unsigned int cacheability_control,
                              unsigned int format,
                              unsigned int tiling,
                              unsigned int width,
                              unsigned int height,
                              unsigned int pitch,
                              unsigned int base_offset,
                              unsigned int y_offset)
{
    memset(ss, 0, sizeof(*ss));

    ss->ss0.vert_line_stride_ofs = vert_line_stride_offset;
    ss->ss0.vert_line_stride = vert_line_stride;
    ss->ss0.surface_format = format;
    ss->ss0.surface_type = I965_SURFACE_2D;

    ss->ss1.surface_mocs = cacheability_control;

    ss->ss2.width = width - 1;
    ss->ss2.height = height - 1;

    ss->ss3.pitch = pitch - 1;

    ss->ss5.y_offset = y_offset;

    ss->ss7.shader_chanel_select_a = HSW_SCS_ALPHA;
    ss->ss7.shader_chanel_select_b = HSW_SCS_BLUE;
    ss->ss7.shader_chanel_select_g = HSW_SCS_GREEN;
    ss->ss7.shader_chanel_select_r = HSW_SCS_RED;

    ss->ss8.base_addr = base_offset;

    gen8_gpe_set_surface_tiling(ss, tiling);
}

static void
gen8_gpe_set_adv_surface_state(struct gen8_surface_state2 *ss,
                               unsigned int v_direction,
                               unsigned int cacheability_control,
                               unsigned int format,
                               unsigned int tiling,
                               unsigned int width,
                               unsigned int height,
                               unsigned int pitch,
                               unsigned int base_offset,
                               unsigned int y_cb_offset)
{
    memset(ss, 0, sizeof(*ss));

    ss->ss1.cbcr_pixel_offset_v_direction = v_direction;
    ss->ss1.width = width - 1;
    ss->ss1.height = height - 1;

    ss->ss2.surface_format = format;
    ss->ss2.interleave_chroma = 1;
    ss->ss2.pitch = pitch - 1;

    ss->ss3.y_offset_for_cb = y_cb_offset;

    ss->ss5.surface_object_mocs = cacheability_control;

    ss->ss6.base_addr = base_offset;

    gen8_gpe_set_surface2_tiling(ss, tiling);
}

static void
gen8_gpe_set_buffer2_surface_state(struct gen8_surface_state *ss,
                                   unsigned int cacheability_control,
                                   unsigned int format,
                                   unsigned int size,
                                   unsigned int pitch,
                                   unsigned int base_offset)
{
    memset(ss, 0, sizeof(*ss));

    ss->ss0.surface_format = format;
    ss->ss0.surface_type = I965_SURFACE_BUFFER;

    ss->ss1.surface_mocs = cacheability_control;

    ss->ss2.width = (size - 1) & 0x7F;
    ss->ss2.height = ((size - 1) & 0x1FFF80) >> 7;

    ss->ss3.depth = ((size - 1) & 0xFE00000) >> 21;
    ss->ss3.pitch = pitch - 1;

    ss->ss7.shader_chanel_select_a = HSW_SCS_ALPHA;
    ss->ss7.shader_chanel_select_b = HSW_SCS_BLUE;
    ss->ss7.shader_chanel_select_g = HSW_SCS_GREEN;
    ss->ss7.shader_chanel_select_r = HSW_SCS_RED;

    ss->ss8.base_addr = base_offset;
}

void
gen8_gpe_context_add_surface(struct i965_gpe_context *gpe_context,
                             struct i965_gpe_surface *gpe_surface,
                             int index)
{
    char *buf;
    unsigned int tiling, swizzle, width, height, pitch, tile_alignment, y_offset = 0;
    unsigned int surface_state_offset = gpe_context->surface_state_binding_table.surface_state_offset +
                                        index * SURFACE_STATE_PADDED_SIZE_GEN8;
    unsigned int binding_table_offset = gpe_context->surface_state_binding_table.binding_table_offset +
                                        index * 4;
    struct i965_gpe_resource *gpe_resource = gpe_surface->gpe_resource;

    dri_bo_get_tiling(gpe_resource->bo, &tiling, &swizzle);

    dri_bo_map(gpe_context->surface_state_binding_table.bo, 1);
    buf = (char *)gpe_context->surface_state_binding_table.bo->virtual;
    *((unsigned int *)(buf + binding_table_offset)) = surface_state_offset;

    if (gpe_surface->is_2d_surface) {
        struct gen8_surface_state *ss = (struct gen8_surface_state *)(buf + surface_state_offset);
        unsigned int target_offset;

        width = gpe_resource->width;
        height = gpe_resource->height;
        pitch = gpe_resource->pitch;

        if (gpe_surface->is_override_offset) {
            y_offset = 0;
            target_offset = gpe_surface->offset;
        } else if (gpe_surface->is_uv_surface) {
            height /= 2;

            if (tiling == I915_TILING_Y) {
                tile_alignment = 32;
            } else if (tiling == I915_TILING_X) {
                tile_alignment = 8;
            } else
                tile_alignment = 1;

            y_offset = (gpe_resource->y_cb_offset % tile_alignment);
            target_offset = ALIGN_FLOOR(gpe_resource->y_cb_offset, tile_alignment) * pitch;
        } else {
            y_offset = 0;
            target_offset = 0;
        }

        if (gpe_surface->is_media_block_rw) {
            width = (ALIGN(width, 4) >> 2);
        }

        gen8_gpe_set_2d_surface_state(ss,
                                      gpe_surface->vert_line_stride_offset,
                                      gpe_surface->vert_line_stride,
                                      gpe_surface->cacheability_control,
                                      gpe_surface->format,
                                      tiling,
                                      width, height, pitch,
                                      gpe_resource->bo->offset64 + target_offset,
                                      y_offset);

        dri_bo_emit_reloc(gpe_context->surface_state_binding_table.bo,
                          I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER,
                          target_offset,
                          surface_state_offset + offsetof(struct gen8_surface_state, ss8),
                          gpe_resource->bo);
    } else if (gpe_surface->is_adv_surface) {
        struct gen8_surface_state2 *ss = (struct gen8_surface_state2 *)(buf + surface_state_offset);

        width = gpe_resource->width;
        height = gpe_resource->height;
        pitch = gpe_resource->pitch;

        gen8_gpe_set_adv_surface_state(ss,
                                       gpe_surface->v_direction,
                                       gpe_surface->cacheability_control,
                                       MFX_SURFACE_PLANAR_420_8,
                                       tiling,
                                       width, height, pitch,
                                       gpe_resource->bo->offset64,
                                       gpe_resource->y_cb_offset);

        dri_bo_emit_reloc(gpe_context->surface_state_binding_table.bo,
                          I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER,
                          0,
                          surface_state_offset + offsetof(struct gen8_surface_state2, ss6),
                          gpe_resource->bo);
    } else {
        struct gen8_surface_state *ss = (struct gen8_surface_state *)(buf + surface_state_offset);
        unsigned int format;

        assert(gpe_surface->is_buffer);

        if (gpe_surface->is_raw_buffer) {
            format = I965_SURFACEFORMAT_RAW;
            pitch = 1;
        } else {
            format = I965_SURFACEFORMAT_R32_UINT;
            pitch = sizeof(unsigned int);
        }

        gen8_gpe_set_buffer2_surface_state(ss,
                                           gpe_surface->cacheability_control,
                                           format,
                                           gpe_surface->size,
                                           pitch,
                                           gpe_resource->bo->offset64 + gpe_surface->offset);

        dri_bo_emit_reloc(gpe_context->surface_state_binding_table.bo,
                          I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER,
                          gpe_surface->offset,
                          surface_state_offset + offsetof(struct gen8_surface_state, ss8),
                          gpe_resource->bo);
    }

    dri_bo_unmap(gpe_context->surface_state_binding_table.bo);
}

void
gen8_gpe_mi_conditional_batch_buffer_end(VADriverContextP ctx,
                                         struct intel_batchbuffer *batch,
                                         struct gpe_mi_conditional_batch_buffer_end_parameter *param)
{
    __OUT_BATCH(batch, (MI_CONDITIONAL_BATCH_BUFFER_END |
                        (1 << 21) |
                        (4 - 2))); /* Always use PPGTT */
    __OUT_BATCH(batch, param->compare_data);
    __OUT_RELOC64(batch,
                  param->bo,
                  I915_GEM_DOMAIN_RENDER | I915_GEM_DOMAIN_INSTRUCTION, 0,
                  param->offset);

}

void
gen8_gpe_pipe_control(VADriverContextP ctx,
                      struct intel_batchbuffer *batch,
                      struct gpe_pipe_control_parameter *param)
{
    int render_target_cache_flush_enable = CMD_PIPE_CONTROL_WC_FLUSH;
    int dc_flush_enable = 0;
    int state_cache_invalidation_enable = 0;
    int constant_cache_invalidation_enable = 0;
    int vf_cache_invalidation_enable = 0;
    int instruction_cache_invalidation_enable = 0;
    int post_sync_operation = CMD_PIPE_CONTROL_NOWRITE;
    int use_global_gtt = CMD_PIPE_CONTROL_GLOBAL_GTT_GEN8;
    int cs_stall_enable = !param->disable_cs_stall;

    switch (param->flush_mode) {
    case PIPE_CONTROL_FLUSH_WRITE_CACHE:
        render_target_cache_flush_enable = CMD_PIPE_CONTROL_WC_FLUSH;
        dc_flush_enable = CMD_PIPE_CONTROL_DC_FLUSH;
        break;

    case PIPE_CONTROL_FLUSH_READ_CACHE:
        render_target_cache_flush_enable = 0;
        state_cache_invalidation_enable = CMD_PIPE_CONTROL_SC_INVALIDATION_GEN8;
        constant_cache_invalidation_enable = CMD_PIPE_CONTROL_CC_INVALIDATION_GEN8;
        vf_cache_invalidation_enable = CMD_PIPE_CONTROL_VFC_INVALIDATION_GEN8;
        instruction_cache_invalidation_enable = CMD_PIPE_CONTROL_IS_FLUSH;
        break;

    case PIPE_CONTROL_FLUSH_NONE:
    default:
        render_target_cache_flush_enable = 0;
        break;
    }

    if (param->bo) {
        post_sync_operation = CMD_PIPE_CONTROL_WRITE_QWORD;
        use_global_gtt = CMD_PIPE_CONTROL_LOCAL_PGTT_GEN8;
    } else {
        post_sync_operation = CMD_PIPE_CONTROL_NOWRITE;
        render_target_cache_flush_enable = CMD_PIPE_CONTROL_WC_FLUSH;
        state_cache_invalidation_enable = CMD_PIPE_CONTROL_SC_INVALIDATION_GEN8;
        constant_cache_invalidation_enable = CMD_PIPE_CONTROL_CC_INVALIDATION_GEN8;
        vf_cache_invalidation_enable = CMD_PIPE_CONTROL_VFC_INVALIDATION_GEN8;
        instruction_cache_invalidation_enable = CMD_PIPE_CONTROL_IS_FLUSH;
    }

    __OUT_BATCH(batch, CMD_PIPE_CONTROL | (6 - 2));
    __OUT_BATCH(batch, (render_target_cache_flush_enable |
                        dc_flush_enable |
                        state_cache_invalidation_enable |
                        constant_cache_invalidation_enable |
                        vf_cache_invalidation_enable |
                        instruction_cache_invalidation_enable |
                        post_sync_operation |
                        use_global_gtt |
                        cs_stall_enable |
                        CMD_PIPE_CONTROL_FLUSH_ENABLE));

    if (param->bo)
        __OUT_RELOC64(batch,
                      param->bo,
                      I915_GEM_DOMAIN_RENDER | I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_RENDER,
                      param->offset);
    else {
        __OUT_BATCH(batch, 0);
        __OUT_BATCH(batch, 0);
    }

    __OUT_BATCH(batch, param->dw0);
    __OUT_BATCH(batch, param->dw1);
}

void
i965_init_media_object_walker_parameter(struct gpe_encoder_kernel_walker_parameter *kernel_walker_param,
                                        struct gpe_media_object_walker_parameter *walker_param)
{
    memset(walker_param, 0, sizeof(*walker_param));

    walker_param->use_scoreboard = kernel_walker_param->use_scoreboard;

    walker_param->block_resolution.x = kernel_walker_param->resolution_x;
    walker_param->block_resolution.y = kernel_walker_param->resolution_y;

    walker_param->global_resolution.x = kernel_walker_param->resolution_x;
    walker_param->global_resolution.y = kernel_walker_param->resolution_y;

    walker_param->global_outer_loop_stride.x = kernel_walker_param->resolution_x;
    walker_param->global_outer_loop_stride.y = 0;

    walker_param->global_inner_loop_unit.x = 0;
    walker_param->global_inner_loop_unit.y = kernel_walker_param->resolution_y;

    walker_param->local_loop_exec_count = 0xFFFF;  //MAX VALUE
    walker_param->global_loop_exec_count = 0xFFFF;  //MAX VALUE

    if (kernel_walker_param->no_dependency) {
        walker_param->scoreboard_mask = 0;
        // Raster scan walking pattern
        walker_param->local_outer_loop_stride.x = 0;
        walker_param->local_outer_loop_stride.y = 1;
        walker_param->local_inner_loop_unit.x = 1;
        walker_param->local_inner_loop_unit.y = 0;
        walker_param->local_end.x = kernel_walker_param->resolution_x - 1;
        walker_param->local_end.y = 0;
    } else if (kernel_walker_param->use_vertical_raster_scan) {
        walker_param->scoreboard_mask = 0x1;
        walker_param->use_scoreboard = 0;
        // Raster scan walking pattern
        walker_param->local_outer_loop_stride.x = 1;
        walker_param->local_outer_loop_stride.y = 0;
        walker_param->local_inner_loop_unit.x = 0;
        walker_param->local_inner_loop_unit.y = 1;
        walker_param->local_end.x = 0;
        walker_param->local_end.y = kernel_walker_param->resolution_y - 1;
    } else {
        walker_param->local_end.x = 0;
        walker_param->local_end.y = 0;

        if (kernel_walker_param->walker_degree == WALKER_45Z_DEGREE) {
            // 45z degree vp9
            walker_param->scoreboard_mask = 0x0F;

            walker_param->global_loop_exec_count = 0x3FF;
            walker_param->local_loop_exec_count = 0x3FF;

            walker_param->global_resolution.x = (unsigned int)(kernel_walker_param->resolution_x / 2.f) + 1;
            walker_param->global_resolution.y = 2 * kernel_walker_param->resolution_y;

            walker_param->global_start.x = 0;
            walker_param->global_start.y = 0;

            walker_param->global_outer_loop_stride.x = walker_param->global_resolution.x;
            walker_param->global_outer_loop_stride.y = 0;

            walker_param->global_inner_loop_unit.x = 0;
            walker_param->global_inner_loop_unit.y = walker_param->global_resolution.y;

            walker_param->block_resolution.x = walker_param->global_resolution.x;
            walker_param->block_resolution.y = walker_param->global_resolution.y;

            walker_param->local_start.x = 0;
            walker_param->local_start.y = 0;

            walker_param->local_outer_loop_stride.x = 1;
            walker_param->local_outer_loop_stride.y = 0;

            walker_param->local_inner_loop_unit.x = -1;
            walker_param->local_inner_loop_unit.y = 4;

            walker_param->middle_loop_extra_steps = 3;
            walker_param->mid_loop_unit_x = 0;
            walker_param->mid_loop_unit_y = 1;
        } else if (kernel_walker_param->walker_degree == WALKER_45_DEGREE) {

            walker_param->scoreboard_mask = 0x03;
            // 45 order in local loop
            walker_param->local_outer_loop_stride.x = 1;
            walker_param->local_outer_loop_stride.y = 0;
            walker_param->local_inner_loop_unit.x = -1;
            walker_param->local_inner_loop_unit.y = 1;
        } else if (kernel_walker_param->walker_degree == WALKER_26Z_DEGREE) {
            // 26z HEVC
            walker_param->scoreboard_mask = 0x7f;

            // z order in local loop
            walker_param->local_outer_loop_stride.x = 0;
            walker_param->local_outer_loop_stride.y = 1;
            walker_param->local_inner_loop_unit.x = 1;
            walker_param->local_inner_loop_unit.y = 0;

            walker_param->block_resolution.x = 2;
            walker_param->block_resolution.y = 2;

            walker_param->global_outer_loop_stride.x = 2;
            walker_param->global_outer_loop_stride.y = 0;

            walker_param->global_inner_loop_unit.x = 0xFFF - 4 + 1;
            walker_param->global_inner_loop_unit.y = 2;

        } else {
            // 26 degree
            walker_param->scoreboard_mask = 0x0F;
            walker_param->local_outer_loop_stride.x = 1;
            walker_param->local_outer_loop_stride.y = 0;
            walker_param->local_inner_loop_unit.x = -2;
            walker_param->local_inner_loop_unit.y = 1;
        }
    }
}

void
gen9_add_2d_gpe_surface(VADriverContextP ctx,
                        struct i965_gpe_context *gpe_context,
                        struct object_surface *obj_surface,
                        int is_uv_surface,
                        int is_media_block_rw,
                        unsigned int format,
                        int index)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct i965_gpe_resource gpe_resource;
    struct i965_gpe_surface gpe_surface;

    memset(&gpe_surface, 0, sizeof(gpe_surface));

    i965_object_surface_to_2d_gpe_resource(&gpe_resource, obj_surface);
    gpe_surface.gpe_resource = &gpe_resource;
    gpe_surface.is_2d_surface = 1;
    gpe_surface.is_uv_surface = !!is_uv_surface;
    gpe_surface.is_media_block_rw = !!is_media_block_rw;

    gpe_surface.cacheability_control = i965->intel.mocs_state;
    gpe_surface.format = format;

    if (gpe_surface.is_media_block_rw) {
        if (obj_surface->fourcc == VA_FOURCC_P010)
            gpe_surface.is_16bpp = 1;
    }

    gen9_gpe_context_add_surface(gpe_context, &gpe_surface, index);
    i965_free_gpe_resource(&gpe_resource);
}

void
gen9_add_adv_gpe_surface(VADriverContextP ctx,
                         struct i965_gpe_context *gpe_context,
                         struct object_surface *obj_surface,
                         int index)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct i965_gpe_resource gpe_resource;
    struct i965_gpe_surface gpe_surface;

    memset(&gpe_surface, 0, sizeof(gpe_surface));

    i965_object_surface_to_2d_gpe_resource(&gpe_resource, obj_surface);
    gpe_surface.gpe_resource = &gpe_resource;
    gpe_surface.is_adv_surface = 1;
    gpe_surface.cacheability_control = i965->intel.mocs_state;
    gpe_surface.v_direction = 2;

    gen9_gpe_context_add_surface(gpe_context, &gpe_surface, index);
    i965_free_gpe_resource(&gpe_resource);
}

void
gen9_add_buffer_gpe_surface(VADriverContextP ctx,
                            struct i965_gpe_context *gpe_context,
                            struct i965_gpe_resource *gpe_buffer,
                            int is_raw_buffer,
                            unsigned int size,
                            unsigned int offset,
                            int index)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct i965_gpe_surface gpe_surface;

    memset(&gpe_surface, 0, sizeof(gpe_surface));

    gpe_surface.gpe_resource = gpe_buffer;
    gpe_surface.is_buffer = 1;
    gpe_surface.is_raw_buffer = !!is_raw_buffer;
    gpe_surface.cacheability_control = i965->intel.mocs_state;
    gpe_surface.size = size;
    gpe_surface.offset = offset;

    gen9_gpe_context_add_surface(gpe_context, &gpe_surface, index);
}

void
gen9_add_buffer_2d_gpe_surface(VADriverContextP ctx,
                               struct i965_gpe_context *gpe_context,
                               struct i965_gpe_resource *gpe_buffer,
                               int is_media_block_rw,
                               unsigned int format,
                               int index)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct i965_gpe_surface gpe_surface;

    memset(&gpe_surface, 0, sizeof(gpe_surface));

    gpe_surface.gpe_resource = gpe_buffer;
    gpe_surface.is_2d_surface = 1;
    gpe_surface.is_media_block_rw = !!is_media_block_rw;
    gpe_surface.cacheability_control = i965->intel.mocs_state;
    gpe_surface.format = format;

    gen9_gpe_context_add_surface(gpe_context, &gpe_surface, index);
}

void
gen9_add_dri_buffer_gpe_surface(VADriverContextP ctx,
                                struct i965_gpe_context *gpe_context,
                                dri_bo *bo,
                                int is_raw_buffer,
                                unsigned int size,
                                unsigned int offset,
                                int index)
{
    struct i965_gpe_resource gpe_resource;

    i965_dri_object_to_buffer_gpe_resource(&gpe_resource, bo);
    gen9_add_buffer_gpe_surface(ctx,
                                gpe_context,
                                &gpe_resource,
                                is_raw_buffer,
                                size,
                                offset,
                                index);

    i965_free_gpe_resource(&gpe_resource);
}

bool
i965_gpe_table_init(VADriverContextP ctx)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct i965_gpe_table *gpe = &i965->gpe_table;

    if (IS_GEN8(i965->intel.device_info)) {
        gpe->context_init = gen8_gpe_context_init;
        gpe->context_destroy = gen8_gpe_context_destroy;
        gpe->context_add_surface = gen8_gpe_context_add_surface;
        gpe->reset_binding_table = gen8_gpe_reset_binding_table;
        gpe->load_kernels = gen8_gpe_load_kernels;
        gpe->setup_interface_data = gen8_gpe_setup_interface_data;
        gpe->set_dynamic_buffer = gen8_gpe_context_set_dynamic_buffer;
        gpe->media_object = gen8_gpe_media_object;
        gpe->media_object_walker = gen8_gpe_media_object_walker;
        gpe->media_state_flush = gen8_gpe_media_state_flush;
        gpe->pipe_control = gen8_gpe_pipe_control;
        gpe->pipeline_end = gen8_gpe_pipeline_end;
        gpe->pipeline_setup = gen8_gpe_pipeline_setup;
        gpe->mi_conditional_batch_buffer_end = gen8_gpe_mi_conditional_batch_buffer_end;
        gpe->mi_batch_buffer_start = gen8_gpe_mi_batch_buffer_start;
        gpe->mi_load_register_reg = gen8_gpe_mi_load_register_reg;
        gpe->mi_load_register_imm = gen8_gpe_mi_load_register_imm;
        gpe->mi_load_register_mem = gen8_gpe_mi_load_register_mem;
        gpe->mi_store_register_mem = gen8_gpe_mi_store_register_mem;
        gpe->mi_store_data_imm = gen8_gpe_mi_store_data_imm;
        gpe->mi_flush_dw = gen8_gpe_mi_flush_dw;
    } else if (IS_GEN9(i965->intel.device_info)) {
        gpe->context_init = gen8_gpe_context_init;
        gpe->context_destroy = gen8_gpe_context_destroy;
        gpe->context_add_surface = gen9_gpe_context_add_surface;
        gpe->reset_binding_table = gen9_gpe_reset_binding_table;
        gpe->load_kernels = gen8_gpe_load_kernels;
        gpe->setup_interface_data = gen8_gpe_setup_interface_data;
        gpe->set_dynamic_buffer = gen8_gpe_context_set_dynamic_buffer;
        gpe->media_object = gen8_gpe_media_object;
        gpe->media_object_walker = gen8_gpe_media_object_walker;
        gpe->media_state_flush = gen8_gpe_media_state_flush;
        gpe->pipe_control = gen8_gpe_pipe_control;
        gpe->pipeline_end = gen9_gpe_pipeline_end;
        gpe->pipeline_setup = gen9_gpe_pipeline_setup;
        gpe->mi_conditional_batch_buffer_end = gen9_gpe_mi_conditional_batch_buffer_end;
        gpe->mi_batch_buffer_start = gen8_gpe_mi_batch_buffer_start;
        gpe->mi_load_register_reg = gen8_gpe_mi_load_register_reg;
        gpe->mi_load_register_imm = gen8_gpe_mi_load_register_imm;
        gpe->mi_load_register_mem = gen8_gpe_mi_load_register_mem;
        gpe->mi_store_register_mem = gen8_gpe_mi_store_register_mem;
        gpe->mi_store_data_imm = gen8_gpe_mi_store_data_imm;
        gpe->mi_flush_dw = gen8_gpe_mi_flush_dw;
    } else {
        // TODO: for other platforms
    }

    return true;
}

void
i965_gpe_table_terminate(VADriverContextP ctx)
{

}
