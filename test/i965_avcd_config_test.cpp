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

#include "i965_config_test.h"

namespace AVC {
namespace Decode {

VAStatus ProfileNotSupported()
{
    return VA_STATUS_ERROR_UNSUPPORTED_PROFILE;
}

VAStatus EntrypointNotSupported()
{
    return VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT;
}

// H264*NotSupported functions report properly if profile is not supported or
// only entrypoint is not supported
VAStatus H264NotSupported()
{
    I965TestEnvironment *env(I965TestEnvironment::instance());
    EXPECT_PTR(env);

    struct i965_driver_data *i965(*env);
    EXPECT_PTR(i965);

    if (!HAS_H264_ENCODING(i965)
        && !HAS_LP_H264_ENCODING(i965))
        return ProfileNotSupported();

    return EntrypointNotSupported();
}

VAStatus H264MVCNotSupported()
{
    I965TestEnvironment *env(I965TestEnvironment::instance());
    EXPECT_PTR(env);

    struct i965_driver_data *i965(*env);
    EXPECT_PTR(i965);

    if (!HAS_H264_MVC_ENCODING(i965))
        return ProfileNotSupported();

    return EntrypointNotSupported();
}
VAStatus HasDecodeSupport()
{
    I965TestEnvironment *env(I965TestEnvironment::instance());
    EXPECT_PTR(env);

    struct i965_driver_data *i965(*env);
    EXPECT_PTR(i965);

    if (HAS_H264_DECODING(i965))
        return VA_STATUS_SUCCESS;

    return H264NotSupported();
}

VAStatus HasMVCDecodeSupport()
{
    I965TestEnvironment *env(I965TestEnvironment::instance());
    EXPECT_PTR(env);

    struct i965_driver_data *i965(*env);
    EXPECT_PTR(i965);

    if (HAS_H264_MVC_DECODING(i965))
        return VA_STATUS_SUCCESS;

    return H264MVCNotSupported();
}

static const std::vector<ConfigTestInput> inputs = {
    { VAProfileH264Baseline, VAEntrypointVLD, &ProfileNotSupported },

    { VAProfileH264ConstrainedBaseline, VAEntrypointVLD, &HasDecodeSupport },
    { VAProfileH264Main, VAEntrypointVLD, &HasDecodeSupport },
    { VAProfileH264High, VAEntrypointVLD, &HasDecodeSupport },

    { VAProfileH264MultiviewHigh, VAEntrypointVLD,
      &HasMVCDecodeSupport },
    { VAProfileH264StereoHigh, VAEntrypointVLD,
      &HasMVCDecodeSupport },
};

INSTANTIATE_TEST_CASE_P(
    AVCDecode, I965ConfigTest, ::testing::ValuesIn(inputs));

} // namespace Decode
} // namespace AVC
