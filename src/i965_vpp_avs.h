/*
 * i965_vpp_avs.h - Adaptive Video Scaler (AVS) block
 *
 * Copyright (C) 2014 Intel Corporation
 *   Author: Gwenole Beauchesne <gwenole.beauchesne@intel.com>
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors. Title to the Material remains with Intel Corporation or its
 * suppliers and licensors. The Material contains trade secrets and proprietary
 * and confidential information of Intel or its suppliers and licensors. The
 * Material is protected by worldwide copyright and trade secret laws and
 * treaty provisions. No part of the Material may be used, copied, reproduced,
 * modified, published, uploaded, posted, transmitted, distributed, or
 * disclosed in any way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 */

#ifndef I965_VPP_AVS_H
#define I965_VPP_AVS_H

#include <stdint.h>
#include <stdbool.h>

/** Maximum number of phases for the sharp filter */
#define AVS_MAX_PHASES 16

/** Maximum number of coefficients for luma samples */
#define AVS_MAX_LUMA_COEFFS 8

/** Maximum number of coefficients for chroma samples */
#define AVS_MAX_CHROMA_COEFFS 4

typedef struct avs_coeffs               AVSCoeffs;
typedef struct avs_coeffs_range         AVSCoeffsRange;
typedef struct avs_config               AVSConfig;
typedef struct avs_state                AVSState;

/** AVS coefficients for one phase */
struct avs_coeffs {
    /** Coefficients for luma samples on the X-axis (horizontal) */
    float y_k_h[AVS_MAX_LUMA_COEFFS];
    /** Coefficients for luma samples on the Y-axis (vertical) */
    float y_k_v[AVS_MAX_LUMA_COEFFS];
    /** Coefficients for chroma samples on the X-axis (horizontal) */
    float uv_k_h[AVS_MAX_CHROMA_COEFFS];
    /** Coefficients for chroma samples on the Y-axis (vertical) */
    float uv_k_v[AVS_MAX_CHROMA_COEFFS];
};

/** AVS coefficients range used for validation */
struct avs_coeffs_range {
    /** Lower bound for all coefficients */
    AVSCoeffs lower_bound;
    /** Upper bound for all coefficients */
    AVSCoeffs upper_bound;
};

/** Static configuration (per-generation) */
struct avs_config {
    /** Number of bits used for the fractional part of a coefficient */
    int coeff_frac_bits;
    /** The smallest float that could be represented as a coefficient */
    float coeff_epsilon;
    /** Coefficients range */
    AVSCoeffsRange coeff_range;
    /** Number of phases for the sharp filter */
    int num_phases;
    /** Number of coefficients for luma samples */
    int num_luma_coeffs;
    /** Number of coefficients for chroma samples */
    int num_chroma_coeffs;
};

/** AVS block state */
struct avs_state {
    /** Per-generation configuration parameters */
    const AVSConfig *config;
    /** Scaling flags */
    uint32_t flags;
    /** Scaling factor on the X-axis (horizontal) */
    float scale_x;
    /** Scaling factor on the Y-axis (vertical) */
    float scale_y;
    /** Coefficients for the polyphase scaler */
    AVSCoeffs coeffs[AVS_MAX_PHASES + 1];
};

/** Initializes AVS state with the supplied configuration */
void
avs_init_state(AVSState *avs, const AVSConfig *config);

/** Updates AVS coefficients for the supplied factors and quality level */
bool
avs_update_coefficients(AVSState *avs, float sx, float sy, uint32_t flags);

#endif /* I965_VPP_AVS_H */
