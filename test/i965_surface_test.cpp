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

#include <algorithm>
#include <set>

static const std::set<unsigned> pixelFormats = {
    /** Defined in va/va.h **/
    VA_FOURCC_NV12, VA_FOURCC_AI44, VA_FOURCC_RGBA, VA_FOURCC_RGBX,
    VA_FOURCC_BGRA, VA_FOURCC_BGRX, VA_FOURCC_ARGB, VA_FOURCC_XRGB,
    VA_FOURCC_ABGR, VA_FOURCC_XBGR, VA_FOURCC_UYVY, VA_FOURCC_YUY2,
    VA_FOURCC_AYUV, VA_FOURCC_NV11, VA_FOURCC_YV12, VA_FOURCC_P208,
    VA_FOURCC_IYUV, VA_FOURCC_YV24, VA_FOURCC_YV32, VA_FOURCC_Y800,
    VA_FOURCC_IMC3, VA_FOURCC_411P, VA_FOURCC_422H, VA_FOURCC_422V,
    VA_FOURCC_444P, VA_FOURCC_RGBP, VA_FOURCC_BGRP, VA_FOURCC_411R,
    VA_FOURCC_YV16, VA_FOURCC_P010, VA_FOURCC_P016,

    /** Defined in i965_fourcc.h **/
    VA_FOURCC_I420, VA_FOURCC_IA44, VA_FOURCC_IA88, VA_FOURCC_AI88,
    VA_FOURCC_IMC1, VA_FOURCC_YVY2,

    /** Bogus pixel formats **/
    VA_FOURCC('B','E','E','F'), VA_FOURCC('P','O','R','K'),
    VA_FOURCC('F','I','S','H'),
};

class CreateSurfacesTest
    : public I965TestFixture
{
protected:
    const std::set<unsigned> supported = {
        VA_FOURCC_NV12, VA_FOURCC_I420, VA_FOURCC_IYUV, VA_FOURCC_IMC3,
        VA_FOURCC_YV12, VA_FOURCC_IMC1, VA_FOURCC_P010, VA_FOURCC_422H,
        VA_FOURCC_422V, VA_FOURCC_YV16, VA_FOURCC_YUY2, VA_FOURCC_UYVY,
        VA_FOURCC_444P, VA_FOURCC_411P, VA_FOURCC_Y800, VA_FOURCC_RGBA,
        VA_FOURCC_RGBX, VA_FOURCC_BGRA, VA_FOURCC_BGRX,
    };
};

TEST_F(CreateSurfacesTest, SupportedPixelFormats)
{
    SurfaceAttribs attributes(1);
    attributes.front().flags = VA_SURFACE_ATTRIB_SETTABLE;
    attributes.front().type = VASurfaceAttribPixelFormat;
    attributes.front().value.type = VAGenericValueTypeInteger;

    for (const unsigned fourcc : supported) {
        SCOPED_TRACE(
            ::testing::Message()
            << std::string(reinterpret_cast<const char*>(&fourcc), 4)
            << "(0x" << std::hex << fourcc << std::dec << ")");

        const i965_fourcc_info *info = get_fourcc_info(fourcc);
        EXPECT_PTR(info);
        EXPECT_TRUE(info->flag & 1);

        attributes.front().value.value.i = fourcc;
        Surfaces surfaces = createSurfaces(
            10, 10, VA_RT_FORMAT_YUV420, 1, attributes);
        destroySurfaces(surfaces);
    }
}

TEST_F(CreateSurfacesTest, UnsupportedPixelFormats)
{
    SurfaceAttribs attributes(1);
    attributes.front().flags = VA_SURFACE_ATTRIB_SETTABLE;
    attributes.front().type = VASurfaceAttribPixelFormat;
    attributes.front().value.type = VAGenericValueTypeInteger;

    std::set<unsigned> unsupported;
    std::set_difference(pixelFormats.begin(), pixelFormats.end(),
        supported.begin(), supported.end(),
        std::inserter(unsupported, unsupported.begin()));

    EXPECT_EQ(pixelFormats.size() - supported.size(), unsupported.size());

    for (const unsigned fourcc : unsupported) {
        SCOPED_TRACE(
            ::testing::Message()
            << std::string(reinterpret_cast<const char*>(&fourcc), 4)
            << "(0x" << std::hex << fourcc << std::dec << ")");

        const i965_fourcc_info *info = get_fourcc_info(fourcc);
        EXPECT_FALSE(info ? info->flag & 1 : false);

        attributes.front().value.value.i = fourcc;
        Surfaces surfaces;
        EXPECT_NONFATAL_FAILURE(
            surfaces = createSurfaces(
                10, 10, VA_RT_FORMAT_YUV420, 1, attributes),
            "VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT"
        );
        EXPECT_NONFATAL_FAILURE(
            destroySurfaces(surfaces), "VA_STATUS_ERROR_INVALID_SURFACE");
    }
}
