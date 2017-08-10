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

const std::string I965TestFixture::getFullTestName() const
{
    const ::testing::TestInfo * const info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    return std::string(info->test_case_name())
        + std::string(".")
        + std::string(info->name());
}

Surfaces I965TestFixture::createSurfaces(int w, int h, int format, size_t count,
    const SurfaceAttribs& attributes)
{
    Surfaces surfaces(count, VA_INVALID_ID);
    if (attributes.empty()) {
        EXPECT_STATUS(
            i965_CreateSurfaces(
                *this, w, h, format, surfaces.size(), surfaces.data()));
    } else {
        VADriverContextP ctx(*this);
        EXPECT_PTR(ctx);
        if (ctx) {
            EXPECT_STATUS(
                ctx->vtable->vaCreateSurfaces2(
                    *this, format, w, h, surfaces.data(), surfaces.size(),
                    const_cast<VASurfaceAttrib*>(attributes.data()),
                    attributes.size()));
        }
    }

    for (size_t i(0); i < count; ++i) {
        EXPECT_ID(surfaces[i]);
    }

    return surfaces;
}

void I965TestFixture::destroySurfaces(Surfaces& surfaces)
{
    EXPECT_STATUS(
        i965_DestroySurfaces(*this, surfaces.data(), surfaces.size()));
}

VAConfigID I965TestFixture::createConfig(
    VAProfile profile, VAEntrypoint entrypoint, const ConfigAttribs& attribs,
    const VAStatus expect)
{
    VAConfigID id = VA_INVALID_ID;
    EXPECT_STATUS_EQ(
        expect,
        i965_CreateConfig(
            *this, profile, entrypoint,
            const_cast<VAConfigAttrib*>(attribs.data()), attribs.size(), &id));
    if (expect == VA_STATUS_SUCCESS) {
        EXPECT_ID(id);
    } else {
        EXPECT_INVALID_ID(id);
    }
    return id;
}

void I965TestFixture::destroyConfig(VAConfigID id)
{
    EXPECT_STATUS(i965_DestroyConfig(*this, id));
}

VAContextID I965TestFixture::createContext(
    VAConfigID config, int w, int h, int flags, const Surfaces& targets)
{
    VAContextID id = VA_INVALID_ID;
    EXPECT_STATUS(
        i965_CreateContext(
            *this, config, w, h, flags,
            const_cast<VASurfaceID*>(targets.data()), targets.size(), &id));
    EXPECT_ID(id);

    return id;
}

void I965TestFixture::destroyContext(VAContextID id)
{
    EXPECT_STATUS(i965_DestroyContext(*this, id));
}

VABufferID I965TestFixture::createBuffer(
    VAContextID context, VABufferType type,
    unsigned size, unsigned num, const void *data)
{
    VABufferID id;
    EXPECT_STATUS(
        i965_CreateBuffer(*this, context, type, size, num, (void*)data, &id));
    EXPECT_ID(id);

    return id;
}

void I965TestFixture::beginPicture(VAContextID context, VASurfaceID target)
{
    EXPECT_STATUS(
        i965_BeginPicture(*this, context, target));
}

void I965TestFixture::renderPicture(
    VAContextID context, VABufferID *bufs, int num_bufs)
{
    EXPECT_STATUS(
        i965_RenderPicture(*this, context, bufs, num_bufs));
}

void I965TestFixture::endPicture(VAContextID context)
{
    EXPECT_STATUS(
        i965_EndPicture(*this, context));
}

void I965TestFixture::destroyBuffer(VABufferID id)
{
    EXPECT_STATUS(
        i965_DestroyBuffer(*this, id));
}

void I965TestFixture::deriveImage(VASurfaceID surface, VAImage &image)
{
    EXPECT_STATUS(
        i965_DeriveImage(*this, surface, &image));
}

void I965TestFixture::destroyImage(VAImage &image)
{
    EXPECT_STATUS(
        i965_DestroyImage(*this, image.image_id));
}

void I965TestFixture::syncSurface(VASurfaceID surface)
{
    EXPECT_STATUS(
        i965_SyncSurface(*this, surface));
}
