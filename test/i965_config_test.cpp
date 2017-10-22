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
#include "i965_streamable.h"

std::ostream& operator<<(std::ostream& os, const ConfigTestInput& input)
{
    return os << input.profile << " : " << input.entrypoint;
}

I965ConfigTest::I965ConfigTest()
    : I965TestFixture()
    , config(VA_INVALID_ID) // invalid
{
    return;
}

void I965ConfigTest::TearDown()
{
    if (config != VA_INVALID_ID) {
        destroyConfig(config);
        config = VA_INVALID_ID;
    }
    I965TestFixture::TearDown();
}

TEST_P(I965ConfigTest, Create)
{
    const ConfigTestInput& input = GetParam();
    const VAStatus expect = input.expect();

    RecordProperty("expect_status", toString(VaapiStatus(expect)));

    const VAStatus actual = i965_CreateConfig(
        *this, input.profile, input.entrypoint, NULL, 0, &config);

    EXPECT_STATUS_EQ(expect, actual);

    if (actual != VA_STATUS_SUCCESS) {
        EXPECT_INVALID_ID(config);
    }
}
