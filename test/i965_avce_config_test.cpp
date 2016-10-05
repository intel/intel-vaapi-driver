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
namespace Encode {

VAStatus ProfileNotSupported()
{
    return VA_STATUS_ERROR_UNSUPPORTED_PROFILE;
}

VAStatus EntrypointNotSupported()
{
    return VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT;
}

VAStatus HasEncodeSupport()
{
    I965TestEnvironment *env(I965TestEnvironment::instance());
    EXPECT_PTR(env);

    struct i965_driver_data *i965(*env);
    EXPECT_PTR(i965);

    return HAS_H264_ENCODING(i965) ? VA_STATUS_SUCCESS :
        EntrypointNotSupported();
}

VAStatus HasLPEncodeSupport()
{
    I965TestEnvironment *env(I965TestEnvironment::instance());
    EXPECT_PTR(env);

    struct i965_driver_data *i965(*env);
    EXPECT_PTR(i965);

    if (IS_SKL(i965->intel.device_info))
        return VA_STATUS_SUCCESS;

    return HAS_LP_H264_ENCODING(i965) ? VA_STATUS_SUCCESS :
        EntrypointNotSupported();
}

VAStatus HasMVCEncodeSupport()
{
    I965TestEnvironment *env(I965TestEnvironment::instance());
    EXPECT_PTR(env);

    struct i965_driver_data *i965(*env);
    EXPECT_PTR(i965);

    return HAS_H264_MVC_ENCODING(i965) ? VA_STATUS_SUCCESS :
        EntrypointNotSupported();
}

static const std::vector<ConfigTestInput> inputs = {
    {VAProfileH264Baseline, VAEntrypointEncSlice, &ProfileNotSupported},
    {VAProfileH264Baseline, VAEntrypointEncSliceLP, &ProfileNotSupported},
    {VAProfileH264Baseline, VAEntrypointEncPicture, &ProfileNotSupported},

    {VAProfileH264ConstrainedBaseline, VAEntrypointEncSlice, &HasEncodeSupport},
    {VAProfileH264ConstrainedBaseline, VAEntrypointEncSliceLP, &HasLPEncodeSupport},
    {VAProfileH264ConstrainedBaseline, VAEntrypointEncPicture, &EntrypointNotSupported},

    {VAProfileH264Main, VAEntrypointEncSlice, &HasEncodeSupport},
    {VAProfileH264Main, VAEntrypointEncSliceLP, &HasLPEncodeSupport},
    {VAProfileH264Main, VAEntrypointEncPicture, &EntrypointNotSupported},

    {VAProfileH264High, VAEntrypointEncSlice, &HasEncodeSupport},
    {VAProfileH264High, VAEntrypointEncSliceLP, &HasLPEncodeSupport},
    {VAProfileH264High, VAEntrypointEncPicture, &EntrypointNotSupported},

    {VAProfileH264MultiviewHigh, VAEntrypointEncSlice, &HasMVCEncodeSupport},
    {VAProfileH264MultiviewHigh, VAEntrypointEncSliceLP, &EntrypointNotSupported},
    {VAProfileH264MultiviewHigh, VAEntrypointEncPicture, &EntrypointNotSupported},

    {VAProfileH264StereoHigh, VAEntrypointEncSlice, &HasMVCEncodeSupport},
    {VAProfileH264StereoHigh, VAEntrypointEncSliceLP, &EntrypointNotSupported},
    {VAProfileH264StereoHigh, VAEntrypointEncPicture, &EntrypointNotSupported},
};

INSTANTIATE_TEST_CASE_P(
    AVCEncode, I965ConfigTest, ::testing::ValuesIn(inputs));

} // namespace Encode
} // namespace AVC
