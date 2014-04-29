/*
 * Copyright © 2014 Intel Corporation
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "intel_batchbuffer.h"
#include "intel_driver.h"
#include "i965_defines.h"
#include "i965_structs.h"
#include "i965_drv_video.h"
#include "i965_post_processing.h"
#include "i965_render.h"
#include "intel_media.h"

#include "gen8_post_processing.h"

static const uint32_t pp_null_gen9[][4] = {
};

static const uint32_t pp_nv12_load_save_nv12_gen9[][4] = {
#include "shaders/post_processing/gen9/pl2_to_pl2.g9b"
};

static const uint32_t pp_nv12_load_save_pl3_gen9[][4] = {
#include "shaders/post_processing/gen9/pl2_to_pl3.g9b"
};

static const uint32_t pp_pl3_load_save_nv12_gen9[][4] = {
#include "shaders/post_processing/gen9/pl3_to_pl2.g9b"
};

static const uint32_t pp_pl3_load_save_pl3_gen9[][4] = {
#include "shaders/post_processing/gen9/pl3_to_pl3.g9b"
};

static const uint32_t pp_nv12_scaling_gen9[][4] = {
#include "shaders/post_processing/gen9/pl2_to_pl2.g9b"
};

static const uint32_t pp_nv12_avs_gen9[][4] = {
#include "shaders/post_processing/gen9/pl2_to_pl2.g9b"
};

static const uint32_t pp_nv12_dndi_gen9[][4] = {
};

static const uint32_t pp_nv12_dn_gen9[][4] = {
};

static const uint32_t pp_nv12_load_save_pa_gen9[][4] = {
#include "shaders/post_processing/gen9/pl2_to_pa.g9b"
};

static const uint32_t pp_pl3_load_save_pa_gen9[][4] = {
#include "shaders/post_processing/gen9/pl3_to_pa.g9b"
};

static const uint32_t pp_pa_load_save_nv12_gen9[][4] = {
#include "shaders/post_processing/gen9/pa_to_pl2.g9b"
};

static const uint32_t pp_pa_load_save_pl3_gen9[][4] = {
#include "shaders/post_processing/gen9/pa_to_pl3.g9b"
};

static const uint32_t pp_pa_load_save_pa_gen9[][4] = {
#include "shaders/post_processing/gen9/pa_to_pa.g9b"
};

static const uint32_t pp_rgbx_load_save_nv12_gen9[][4] = {
#include "shaders/post_processing/gen9/rgbx_to_nv12.g9b"
};

static const uint32_t pp_nv12_load_save_rgbx_gen9[][4] = {
#include "shaders/post_processing/gen9/pl2_to_rgbx.g9b"
};

static const uint32_t pp_nv12_blending_gen9[][4] = {
};

static struct pp_module pp_modules_gen9[] = {
    {
        {
            "NULL module (for testing)",
            PP_NULL,
            pp_null_gen9,
            sizeof(pp_null_gen9),
            NULL,
        },

        pp_null_initialize,
    },

    {
        {
            "NV12_NV12",
            PP_NV12_LOAD_SAVE_N12,
            pp_nv12_load_save_nv12_gen9,
            sizeof(pp_nv12_load_save_nv12_gen9),
            NULL,
        },

        gen8_pp_plx_avs_initialize,
    },

    {
        {
            "NV12_PL3",
            PP_NV12_LOAD_SAVE_PL3,
            pp_nv12_load_save_pl3_gen9,
            sizeof(pp_nv12_load_save_pl3_gen9),
            NULL,
        },
        gen8_pp_plx_avs_initialize,
    },

    {
        {
            "PL3_NV12",
            PP_PL3_LOAD_SAVE_N12,
            pp_pl3_load_save_nv12_gen9,
            sizeof(pp_pl3_load_save_nv12_gen9),
            NULL,
        },

        gen8_pp_plx_avs_initialize,
    },

    {
        {
            "PL3_PL3",
            PP_PL3_LOAD_SAVE_N12,
            pp_pl3_load_save_pl3_gen9,
            sizeof(pp_pl3_load_save_pl3_gen9),
            NULL,
        },

        gen8_pp_plx_avs_initialize,
    },

    {
        {
            "NV12 Scaling module",
            PP_NV12_SCALING,
            pp_nv12_scaling_gen9,
            sizeof(pp_nv12_scaling_gen9),
            NULL,
        },

        gen8_pp_plx_avs_initialize,
    },

    {
        {
            "NV12 AVS module",
            PP_NV12_AVS,
            pp_nv12_avs_gen9,
            sizeof(pp_nv12_avs_gen9),
            NULL,
        },

        gen8_pp_plx_avs_initialize,
    },

    {
        {
            "NV12 DNDI module",
            PP_NV12_DNDI,
            pp_nv12_dndi_gen9,
            sizeof(pp_nv12_dndi_gen9),
            NULL,
        },

        pp_null_initialize,
    },

    {
        {
            "NV12 DN module",
            PP_NV12_DN,
            pp_nv12_dn_gen9,
            sizeof(pp_nv12_dn_gen9),
            NULL,
        },

        pp_null_initialize,
    },
    {
        {
            "NV12_PA module",
            PP_NV12_LOAD_SAVE_PA,
            pp_nv12_load_save_pa_gen9,
            sizeof(pp_nv12_load_save_pa_gen9),
            NULL,
        },

        gen8_pp_plx_avs_initialize,
    },

    {
        {
            "PL3_PA module",
            PP_PL3_LOAD_SAVE_PA,
            pp_pl3_load_save_pa_gen9,
            sizeof(pp_pl3_load_save_pa_gen9),
            NULL,
        },

        gen8_pp_plx_avs_initialize,
    },

    {
        {
            "PA_NV12 module",
            PP_PA_LOAD_SAVE_NV12,
            pp_pa_load_save_nv12_gen9,
            sizeof(pp_pa_load_save_nv12_gen9),
            NULL,
        },

        gen8_pp_plx_avs_initialize,
    },

    {
        {
            "PA_PL3 module",
            PP_PA_LOAD_SAVE_PL3,
            pp_pa_load_save_pl3_gen9,
            sizeof(pp_pa_load_save_pl3_gen9),
            NULL,
        },

        gen8_pp_plx_avs_initialize,
    },

    {
        {
            "PA_PA module",
            PP_PA_LOAD_SAVE_PA,
            pp_pa_load_save_pa_gen9,
            sizeof(pp_pa_load_save_pa_gen9),
            NULL,
        },

        gen8_pp_plx_avs_initialize,
    },

    {
        {
            "RGBX_NV12 module",
            PP_RGBX_LOAD_SAVE_NV12,
            pp_rgbx_load_save_nv12_gen9,
            sizeof(pp_rgbx_load_save_nv12_gen9),
            NULL,
        },

        gen8_pp_plx_avs_initialize,
    },

    {
        {
            "NV12_RGBX module",
            PP_NV12_LOAD_SAVE_RGBX,
            pp_nv12_load_save_rgbx_gen9,
            sizeof(pp_nv12_load_save_rgbx_gen9),
            NULL,
        },

        gen8_pp_plx_avs_initialize,
    },
};

void
gen9_post_processing_context_init(VADriverContextP ctx,
                                  void *data,
                                  struct intel_batchbuffer *batch)
{
    gen8_post_processing_context_common_init(ctx, data, pp_modules_gen9, ARRAY_ELEMS(pp_modules_gen9), batch);
}
