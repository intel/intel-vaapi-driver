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

#ifndef _I965_GPE_UTILS_H_
#define _I965_GPE_UTILS_H_

#include <i915_drm.h>
#include <intel_bufmgr.h>

#include "i965_defines.h"
#include "i965_structs.h"

#define MAX_GPE_KERNELS    32

struct i965_buffer_surface {
    dri_bo *bo;
    unsigned int num_blocks;
    unsigned int size_block;
    unsigned int pitch;
};

enum {
    I965_GPE_RESOURCE_BUFFER = 0,
    I965_GPE_RESOURCE_2D
};

struct i965_gpe_resource {
    dri_bo *bo;
    char *map;
    uint32_t type;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t size;
    uint32_t tiling;
    uint32_t cb_cr_pitch;
    uint32_t x_cb_offset;
    uint32_t y_cb_offset;
};

struct gpe_dynamic_state_parameter {
    dri_bo *bo;
    int bo_size;
    unsigned int curbe_offset;
    unsigned int idrt_offset;
    unsigned int sampler_offset;
};

#define PIPE_CONTROL_FLUSH_NONE         0
#define PIPE_CONTROL_FLUSH_WRITE_CACHE  1
#define PIPE_CONTROL_FLUSH_READ_CACHE   2

struct gpe_pipe_control_parameter {
    dri_bo *bo;
    unsigned int offset;
    unsigned int flush_mode;
    unsigned int disable_cs_stall;
    unsigned int dw0;
    unsigned int dw1;
};

struct i965_gpe_context {
    struct {
        dri_bo *bo;
        unsigned int length;            /* in bytes */
        unsigned int max_entries;
        unsigned int binding_table_offset;
        unsigned int surface_state_offset;
    } surface_state_binding_table;

    struct {
        dri_bo *bo;
        unsigned int max_entries;
        unsigned int entry_size;        /* in bytes */
        unsigned int offset;
    } idrt;

    struct {
        dri_bo *bo;
        unsigned int length;            /* in bytes */
        unsigned int offset;
    } curbe;

    struct {
        dri_bo *bo;
        unsigned int max_entries;
        unsigned int entry_size;        /* in bytes */
        unsigned int offset;
    } sampler;

    struct {
        unsigned int gpgpu_mode : 1;
        unsigned int pad0 : 7;
        unsigned int max_num_threads : 16;
        unsigned int num_urb_entries : 8;
        unsigned int urb_entry_size : 16;
        unsigned int curbe_allocation_size : 16;
    } vfe_state;

    /* vfe_desc5/6/7 is used to determine whether the HW scoreboard is used.
     * If scoreboard is not used, don't touch them
     */
    union {
        unsigned int dword;
        struct {
            unsigned int mask: 8;
            unsigned int pad: 22;
            unsigned int type: 1;
            unsigned int enable: 1;
        } scoreboard0;
    } vfe_desc5;

    union {
        unsigned int dword;
        struct {
            int delta_x0: 4;
            int delta_y0: 4;
            int delta_x1: 4;
            int delta_y1: 4;
            int delta_x2: 4;
            int delta_y2: 4;
            int delta_x3: 4;
            int delta_y3: 4;
        } scoreboard1;
    } vfe_desc6;

    union {
        unsigned int dword;
        struct {
            int delta_x4: 4;
            int delta_y4: 4;
            int delta_x5: 4;
            int delta_y5: 4;
            int delta_x6: 4;
            int delta_y6: 4;
            int delta_x7: 4;
            int delta_y7: 4;
        } scoreboard2;
    } vfe_desc7;

    unsigned int num_kernels;
    struct i965_kernel kernels[MAX_GPE_KERNELS];

    struct {
        dri_bo *bo;
        int bo_size;
        unsigned int end_offset;
    } instruction_state;

    struct {
        dri_bo *bo;
    } indirect_state;

    struct {
        dri_bo *bo;
        int bo_size;
        unsigned int end_offset;
    } dynamic_state;
};

struct gpe_mi_flush_dw_parameter {
    dri_bo *bo;
    unsigned int offset;
    unsigned int video_pipeline_cache_invalidate;
    unsigned int dw0;
    unsigned int dw1;
};

struct gpe_mi_store_data_imm_parameter {
    dri_bo *bo;
    unsigned int is_qword;
    unsigned int offset;
    unsigned int dw0;
    unsigned int dw1;
};

struct gpe_mi_store_register_mem_parameter {
    dri_bo *bo;
    unsigned int offset;
    unsigned int mmio_offset;
};

struct gpe_mi_load_register_mem_parameter {
    dri_bo *bo;
    unsigned int offset;
    unsigned int mmio_offset;
};

struct gpe_mi_load_register_imm_parameter {
    unsigned int data;
    unsigned int mmio_offset;
};

struct gpe_mi_load_register_reg_parameter {
    unsigned int src_mmio_offset;
    unsigned int dst_mmio_offset;
};

struct gpe_mi_math_parameter {
    unsigned int num_instructions;
    unsigned int *instruction_list;
};

struct gpe_mi_conditional_batch_buffer_end_parameter {
    dri_bo *bo;
    unsigned int offset;
    unsigned int compare_mask_mode_disabled;
    unsigned int compare_data;
};

struct gpe_mi_batch_buffer_start_parameter {
    dri_bo *bo;
    unsigned int offset;
    unsigned int is_second_level;
    unsigned int use_global_gtt;
};

void i965_gpe_context_destroy(struct i965_gpe_context *gpe_context);
void i965_gpe_context_init(VADriverContextP ctx,
                           struct i965_gpe_context *gpe_context);
void i965_gpe_load_kernels(VADriverContextP ctx,
                           struct i965_gpe_context *gpe_context,
                           struct i965_kernel *kernel_list,
                           unsigned int num_kernels);
void gen6_gpe_pipeline_setup(VADriverContextP ctx,
                             struct i965_gpe_context *gpe_context,
                             struct intel_batchbuffer *batch);
void i965_gpe_surface2_setup(VADriverContextP ctx,
                             struct i965_gpe_context *gpe_context,
                             struct object_surface *obj_surface,
                             unsigned long binding_table_offset,
                             unsigned long surface_state_offset);
void i965_gpe_media_rw_surface_setup(VADriverContextP ctx,
                                     struct i965_gpe_context *gpe_context,
                                     struct object_surface *obj_surface,
                                     unsigned long binding_table_offset,
                                     unsigned long surface_state_offset,
                                     int write_enabled);
void i965_gpe_buffer_suface_setup(VADriverContextP ctx,
                                  struct i965_gpe_context *gpe_context,
                                  struct i965_buffer_surface *buffer_surface,
                                  unsigned long binding_table_offset,
                                  unsigned long surface_state_offset);
void gen7_gpe_surface2_setup(VADriverContextP ctx,
                             struct i965_gpe_context *gpe_context,
                             struct object_surface *obj_surface,
                             unsigned long binding_table_offset,
                             unsigned long surface_state_offset);
void gen7_gpe_media_rw_surface_setup(VADriverContextP ctx,
                                     struct i965_gpe_context *gpe_context,
                                     struct object_surface *obj_surface,
                                     unsigned long binding_table_offset,
                                     unsigned long surface_state_offset,
                                     int write_enabled);
void gen7_gpe_buffer_suface_setup(VADriverContextP ctx,
                                  struct i965_gpe_context *gpe_context,
                                  struct i965_buffer_surface *buffer_surface,
                                  unsigned long binding_table_offset,
                                  unsigned long surface_state_offset);
void gen75_gpe_media_chroma_surface_setup(VADriverContextP ctx,
                                          struct i965_gpe_context *gpe_context,
                                          struct object_surface *obj_surface,
                                          unsigned long binding_table_offset,
                                          unsigned long surface_state_offset,
                                          int write_enabled);

extern void gen8_gpe_surface2_setup(VADriverContextP ctx,
                                    struct i965_gpe_context *gpe_context,
                                    struct object_surface *obj_surface,
                                    unsigned long binding_table_offset,
                                    unsigned long surface_state_offset);
extern void gen8_gpe_media_rw_surface_setup(VADriverContextP ctx,
                                            struct i965_gpe_context *gpe_context,
                                            struct object_surface *obj_surface,
                                            unsigned long binding_table_offset,
                                            unsigned long surface_state_offset,
                                            int write_enabled);
extern void gen8_gpe_buffer_suface_setup(VADriverContextP ctx,
                                         struct i965_gpe_context *gpe_context,
                                         struct i965_buffer_surface *buffer_surface,
                                         unsigned long binding_table_offset,
                                         unsigned long surface_state_offset);
extern void gen8_gpe_media_chroma_surface_setup(VADriverContextP ctx,
                                                struct i965_gpe_context *gpe_context,
                                                struct object_surface *obj_surface,
                                                unsigned long binding_table_offset,
                                                unsigned long surface_state_offset,
                                                int write_enabled);

void gen8_gpe_pipeline_setup(VADriverContextP ctx,
                             struct i965_gpe_context *gpe_context,
                             struct intel_batchbuffer *batch);
extern void
gen8_gpe_context_set_dynamic_buffer(VADriverContextP ctx,
                                    struct i965_gpe_context *gpe_context,
                                    struct gpe_dynamic_state_parameter *ds);


void gen8_gpe_context_destroy(struct i965_gpe_context *gpe_context);
void gen8_gpe_context_init(VADriverContextP ctx,
                           struct i965_gpe_context *gpe_context);

void gen8_gpe_load_kernels(VADriverContextP ctx,
                           struct i965_gpe_context *gpe_context,
                           struct i965_kernel *kernel_list,
                           unsigned int num_kernels);

void gen9_gpe_pipeline_setup(VADriverContextP ctx,
                             struct i965_gpe_context *gpe_context,
                             struct intel_batchbuffer *batch);

void gen9_gpe_pipeline_end(VADriverContextP ctx,
                           struct i965_gpe_context *gpe_context,
                           struct intel_batchbuffer *batch);

Bool i965_allocate_gpe_resource(dri_bufmgr *bufmgr,
                                struct i965_gpe_resource *res,
                                int size,
                                const char *name);

void i965_object_surface_to_2d_gpe_resource(struct i965_gpe_resource *res,
                                            struct object_surface *obj_surface);

void i965_object_surface_to_2d_gpe_resource_with_align(struct i965_gpe_resource *res,
                                                       struct object_surface *obj_surface,
                                                       unsigned int alignment);

void i965_dri_object_to_buffer_gpe_resource(struct i965_gpe_resource *res,
                                            dri_bo *bo);

void i965_dri_object_to_2d_gpe_resource(struct i965_gpe_resource *res,
                                        dri_bo *bo,
                                        unsigned int width,
                                        unsigned int height,
                                        unsigned int pitch);

void i965_zero_gpe_resource(struct i965_gpe_resource *res);

void i965_free_gpe_resource(struct i965_gpe_resource *res);

void *i965_map_gpe_resource(struct i965_gpe_resource *res);

void i965_unmap_gpe_resource(struct i965_gpe_resource *res);

void gen8_gpe_mi_flush_dw(VADriverContextP ctx,
                          struct intel_batchbuffer *batch,
                          struct gpe_mi_flush_dw_parameter *params);

void gen8_gpe_mi_store_data_imm(VADriverContextP ctx,
                                struct intel_batchbuffer *batch,
                                struct gpe_mi_store_data_imm_parameter *params);

void gen8_gpe_mi_store_register_mem(VADriverContextP ctx,
                                    struct intel_batchbuffer *batch,
                                    struct gpe_mi_store_register_mem_parameter *params);

void gen8_gpe_mi_load_register_mem(VADriverContextP ctx,
                                   struct intel_batchbuffer *batch,
                                   struct gpe_mi_load_register_mem_parameter *params);

void gen8_gpe_mi_load_register_imm(VADriverContextP ctx,
                                   struct intel_batchbuffer *batch,
                                   struct gpe_mi_load_register_imm_parameter *params);

void gen8_gpe_mi_load_register_reg(VADriverContextP ctx,
                                   struct intel_batchbuffer *batch,
                                   struct gpe_mi_load_register_reg_parameter *params);

void gen9_gpe_mi_math(VADriverContextP ctx,
                      struct intel_batchbuffer *batch,
                      struct gpe_mi_math_parameter *params);

void gen9_gpe_mi_conditional_batch_buffer_end(VADriverContextP ctx,
                                              struct intel_batchbuffer *batch,
                                              struct gpe_mi_conditional_batch_buffer_end_parameter *params);

void gen8_gpe_mi_batch_buffer_start(VADriverContextP ctx,
                                    struct intel_batchbuffer *batch,
                                    struct gpe_mi_batch_buffer_start_parameter *params);


struct gpe_media_object_inline_data {
    union {
        struct {
            unsigned int x: 8;
            unsigned int y: 8;
            unsigned int reserved: 16;
        };
        unsigned int value;
    };
};

struct gpe_media_object_parameter {
    unsigned int use_scoreboard;
    unsigned int scoreboard_x;
    unsigned int scoreboard_y;
    unsigned int scoreboard_mask;
    unsigned int interface_offset;
    void *pinline_data;
    unsigned int inline_size;
};

struct i965_gpe_surface {
    unsigned int is_buffer: 1;
    unsigned int is_2d_surface: 1;
    unsigned int is_adv_surface: 1;
    unsigned int is_uv_surface: 1;
    unsigned int is_media_block_rw: 1;
    unsigned int is_raw_buffer: 1;
    unsigned int is_16bpp     : 1;
    /* use the override_offset for 2d_surface */
    unsigned int is_override_offset : 1;

    unsigned int vert_line_stride_offset;
    unsigned int vert_line_stride;
    unsigned int cacheability_control;
    unsigned int format; // 2d surface only
    unsigned int v_direction; // adv surface only
    unsigned int size; // buffer only
    unsigned int offset;

    struct i965_gpe_resource *gpe_resource;
};

extern void
gen9_gpe_reset_binding_table(VADriverContextP ctx,
                             struct i965_gpe_context *gpe_context);
extern
void *i965_gpe_context_map_curbe(struct i965_gpe_context *gpe_context);

extern
void i965_gpe_context_unmap_curbe(struct i965_gpe_context *gpe_context);

extern
void gen8_gpe_setup_interface_data(VADriverContextP ctx,
                                   struct i965_gpe_context *gpe_context);
extern void
gen9_gpe_context_add_surface(struct i965_gpe_context *gpe_context,
                             struct i965_gpe_surface *gpe_surface,
                             int index);

extern bool
i965_gpe_allocate_2d_resource(dri_bufmgr *bufmgr,
                              struct i965_gpe_resource *res,
                              int width,
                              int height,
                              int pitch,
                              const char *name);

struct gpe_walker_xy {
    union {
        struct {
            unsigned int x: 16;
            unsigned int y: 16;
        };
        unsigned int value;
    };
};

struct gpe_media_object_walker_parameter {
    void *pinline_data;
    unsigned int inline_size;
    unsigned int interface_offset;
    unsigned int use_scoreboard;
    unsigned int scoreboard_mask;
    unsigned int group_id_loop_select;
    unsigned int color_count_minus1;
    unsigned int mid_loop_unit_x;
    unsigned int mid_loop_unit_y;
    unsigned int middle_loop_extra_steps;
    unsigned int local_loop_exec_count;
    unsigned int global_loop_exec_count;
    struct gpe_walker_xy block_resolution;
    struct gpe_walker_xy local_start;
    struct gpe_walker_xy local_end;
    struct gpe_walker_xy local_outer_loop_stride;
    struct gpe_walker_xy local_inner_loop_unit;
    struct gpe_walker_xy global_resolution;
    struct gpe_walker_xy global_start;
    struct gpe_walker_xy global_outer_loop_stride;
    struct gpe_walker_xy global_inner_loop_unit;
};

enum walker_degree {
    WALKER_NO_DEGREE = 0,
    WALKER_45_DEGREE,
    WALKER_26_DEGREE,
    WALKER_26Z_DEGREE,
    WALKER_45Z_DEGREE,
};
struct gpe_encoder_kernel_walker_parameter {
    unsigned int walker_degree;
    unsigned int use_scoreboard;
    unsigned int scoreboard_mask;
    unsigned int no_dependency;
    unsigned int resolution_x;
    unsigned int resolution_y;
    unsigned int use_vertical_raster_scan;
};

extern void
gen8_gpe_media_object(VADriverContextP ctx,
                      struct i965_gpe_context *gpe_context,
                      struct intel_batchbuffer *batch,
                      struct gpe_media_object_parameter *param);

extern void
gen8_gpe_media_state_flush(VADriverContextP ctx,
                           struct i965_gpe_context *gpe_context,
                           struct intel_batchbuffer *batch);

extern void
gen8_gpe_media_object_walker(VADriverContextP ctx,
                             struct i965_gpe_context *gpe_context,
                             struct intel_batchbuffer *batch,
                             struct gpe_media_object_walker_parameter *param);


struct intel_vpp_kernel_walker_parameter {
    unsigned int                use_scoreboard;
    unsigned int                scoreboard_mask;
    unsigned int                no_dependency;
    unsigned int                resolution_x;
    unsigned int                resolution_y;
};

extern void
intel_vpp_init_media_object_walker_parameter(struct intel_vpp_kernel_walker_parameter *kernel_walker_param,
                                             struct gpe_media_object_walker_parameter *walker_param);
extern void
gen8_gpe_reset_binding_table(VADriverContextP ctx, struct i965_gpe_context *gpe_context);

extern void
gen8_gpe_context_add_surface(struct i965_gpe_context *gpe_context,
                             struct i965_gpe_surface *gpe_surface,
                             int index);

extern void
gen8_gpe_mi_conditional_batch_buffer_end(VADriverContextP ctx,
                                         struct intel_batchbuffer *batch,
                                         struct gpe_mi_conditional_batch_buffer_end_parameter *param);

extern void
gen8_gpe_pipe_control(VADriverContextP ctx,
                      struct intel_batchbuffer *batch,
                      struct gpe_pipe_control_parameter *param);

extern void
i965_init_media_object_walker_parameter(struct gpe_encoder_kernel_walker_parameter *kernel_walker_param,
                                        struct gpe_media_object_walker_parameter *walker_param);

extern void
gen9_add_2d_gpe_surface(VADriverContextP ctx,
                        struct i965_gpe_context *gpe_context,
                        struct object_surface *obj_surface,
                        int is_uv_surface,
                        int is_media_block_rw,
                        unsigned int format,
                        int index);
extern void
gen9_add_adv_gpe_surface(VADriverContextP ctx,
                         struct i965_gpe_context *gpe_context,
                         struct object_surface *obj_surface,
                         int index);
extern void
gen9_add_buffer_gpe_surface(VADriverContextP ctx,
                            struct i965_gpe_context *gpe_context,
                            struct i965_gpe_resource *gpe_buffer,
                            int is_raw_buffer,
                            unsigned int size,
                            unsigned int offset,
                            int index);
extern void
gen9_add_buffer_2d_gpe_surface(VADriverContextP ctx,
                               struct i965_gpe_context *gpe_context,
                               struct i965_gpe_resource *gpe_buffer,
                               int is_media_block_rw,
                               unsigned int format,
                               int index);
extern void
gen9_add_dri_buffer_gpe_surface(VADriverContextP ctx,
                                struct i965_gpe_context *gpe_context,
                                dri_bo *bo,
                                int is_raw_buffer,
                                unsigned int size,
                                unsigned int offset,
                                int index);

struct i965_gpe_table {
    void (*context_init)(VADriverContextP ctx,
                         struct i965_gpe_context *gpe_context);

    void (*context_destroy)(struct i965_gpe_context *gpe_context);

    void (*context_add_surface)(struct i965_gpe_context *gpe_context,
                                struct i965_gpe_surface *gpe_surface,
                                int index);

    void (*reset_binding_table)(VADriverContextP ctx, struct i965_gpe_context *gpe_context);

    void (*load_kernels)(VADriverContextP ctx,
                         struct i965_gpe_context *gpe_context,
                         struct i965_kernel *kernel_list,
                         unsigned int num_kernels);

    void (*setup_interface_data)(VADriverContextP ctx, struct i965_gpe_context *gpe_context);

    void (*set_dynamic_buffer)(VADriverContextP ctx,
                               struct i965_gpe_context *gpe_context,
                               struct gpe_dynamic_state_parameter *ds);

    void (*media_object)(VADriverContextP ctx,
                         struct i965_gpe_context *gpe_context,
                         struct intel_batchbuffer *batch,
                         struct gpe_media_object_parameter *param);

    void (*media_object_walker)(VADriverContextP ctx,
                                struct i965_gpe_context *gpe_context,
                                struct intel_batchbuffer *batch,
                                struct gpe_media_object_walker_parameter *param);

    void (*media_state_flush)(VADriverContextP ctx,
                              struct i965_gpe_context *gpe_context,
                              struct intel_batchbuffer *batch);


    void (*pipe_control)(VADriverContextP ctx,
                         struct intel_batchbuffer *batch,
                         struct gpe_pipe_control_parameter *param);

    void (*pipeline_end)(VADriverContextP ctx,
                         struct i965_gpe_context *gpe_context,
                         struct intel_batchbuffer *batch);              // only available on gen9+

    void (*pipeline_setup)(VADriverContextP ctx,
                           struct i965_gpe_context *gpe_context,
                           struct intel_batchbuffer *batch);

    void (*mi_conditional_batch_buffer_end)(VADriverContextP ctx,
                                            struct intel_batchbuffer *batch,
                                            struct gpe_mi_conditional_batch_buffer_end_parameter *param);

    void (*mi_batch_buffer_start)(VADriverContextP ctx,
                                  struct intel_batchbuffer *batch,
                                  struct gpe_mi_batch_buffer_start_parameter *params);

    void (*mi_load_register_reg)(VADriverContextP ctx,
                                 struct intel_batchbuffer *batch,
                                 struct gpe_mi_load_register_reg_parameter *params);

    void (*mi_load_register_imm)(VADriverContextP ctx,
                                 struct intel_batchbuffer *batch,
                                 struct gpe_mi_load_register_imm_parameter *params);

    void (*mi_load_register_mem)(VADriverContextP ctx,
                                 struct intel_batchbuffer *batch,
                                 struct gpe_mi_load_register_mem_parameter *params);


    void (*mi_store_register_mem)(VADriverContextP ctx,
                                  struct intel_batchbuffer *batch,
                                  struct gpe_mi_store_register_mem_parameter *params);

    void (*mi_store_data_imm)(VADriverContextP ctx,
                              struct intel_batchbuffer *batch,
                              struct gpe_mi_store_data_imm_parameter *params);

    void (*mi_flush_dw)(VADriverContextP ctx,
                        struct intel_batchbuffer *batch,
                        struct gpe_mi_flush_dw_parameter *params);
};

extern bool
i965_gpe_table_init(VADriverContextP ctx);

extern void
i965_gpe_table_terminate(VADriverContextP ctx);

#endif /* _I965_GPE_UTILS_H_ */
