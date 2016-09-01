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

#include "i965_test_fixture.h"

#include <string>

class DriverTest : public I965TestFixture { };

TEST_F(DriverTest, Initialize)
{
    VADriverContextP ctx(*this);

    ASSERT_PTR(ctx);

    EXPECT_EQ(VA_MAJOR_VERSION, ctx->version_major);
    EXPECT_EQ(VA_MINOR_VERSION, ctx->version_minor);
    EXPECT_EQ(I965_MAX_PROFILES, ctx->max_profiles);
    EXPECT_EQ(I965_MAX_ENTRYPOINTS, ctx->max_entrypoints);
    EXPECT_EQ(I965_MAX_CONFIG_ATTRIBUTES, ctx->max_attributes);
    EXPECT_EQ(I965_MAX_IMAGE_FORMATS, ctx->max_image_formats);
    EXPECT_EQ(I965_MAX_SUBPIC_FORMATS, ctx->max_subpic_formats);

    EXPECT_EQ(ctx->max_profiles, vaMaxNumProfiles(*this));
    EXPECT_EQ(ctx->max_entrypoints, vaMaxNumEntrypoints(*this));
    EXPECT_EQ(ctx->max_attributes, vaMaxNumConfigAttributes(*this));
    EXPECT_EQ(ctx->max_image_formats, vaMaxNumImageFormats(*this));
    EXPECT_EQ(ctx->max_subpic_formats, vaMaxNumSubpictureFormats(*this));
    EXPECT_EQ(ctx->max_display_attributes, vaMaxNumDisplayAttributes(*this));

    EXPECT_PTR(ctx->vtable);
    EXPECT_PTR(ctx->vtable_vpp);

    std::string prefix(INTEL_STR_DRIVER_VENDOR " " INTEL_STR_DRIVER_NAME);
    std::string vendor(ctx->str_vendor);

    EXPECT_EQ(0u, vendor.find(prefix));
    EXPECT_NE(std::string::npos, vendor.find(prefix));

    struct i965_driver_data *i965(*this);

    ASSERT_PTR(i965);

    EXPECT_STREQ(ctx->str_vendor, i965->va_vendor);
}
