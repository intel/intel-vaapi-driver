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

#include "i965_avce_test_common.h"
#include "i965_test_environment.h"

namespace AVC {
namespace Encode {

/**
 * This is similar to i965_validate_config(...) in i965_drv_video.c
 * except that there are a few other checks in regards to HW support
 * expectations.
 */
VAStatus CheckSupported(VAProfile profile, VAEntrypoint entrypoint)
{
    I965TestEnvironment *env(I965TestEnvironment::instance());
    EXPECT_PTR(env);

    struct i965_driver_data *i965(*env);
    EXPECT_PTR(i965);

    switch(profile) {
    case VAProfileH264Baseline:
        return VA_STATUS_ERROR_UNSUPPORTED_PROFILE;

    case VAProfileH264ConstrainedBaseline:
    case VAProfileH264Main:
    case VAProfileH264High:
        if (entrypoint == VAEntrypointEncSlice) {
            if (HAS_H264_ENCODING(i965)) {
                return VA_STATUS_SUCCESS;
            }
        } else if (entrypoint == VAEntrypointEncSliceLP) {
            if (IS_SKL(i965->intel.device_info)) {
                return VA_STATUS_SUCCESS;
            }
            if (HAS_LP_H264_ENCODING(i965)) {
                return VA_STATUS_SUCCESS;
            }
        }
        break;

    case VAProfileH264MultiviewHigh:
    case VAProfileH264StereoHigh:
        if (entrypoint == VAEntrypointEncSlice) {
            if (HAS_H264_MVC_ENCODING(i965)) {
                return VA_STATUS_SUCCESS;
            }
        }
        break;

    default:
        return VA_STATUS_ERROR_UNSUPPORTED_PROFILE;
    }

    return VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT;
}

bool IsSupported(VAProfile profile, VAEntrypoint entrypoint)
{
    return VA_STATUS_SUCCESS == CheckSupported(profile, entrypoint);
}

} // namespace Encode
} // namespace AVC
