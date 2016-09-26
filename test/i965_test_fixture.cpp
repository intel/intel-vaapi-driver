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

#include <fcntl.h> // for O_RDWR
#include <unistd.h> // for close()
#include <va/va_drm.h>

I965TestFixture::I965TestFixture()
    : ::testing::Test::Test()
    , m_handle(-1)
    , m_vaDisplay(NULL)
{
    setenv("LIBVA_DRIVERS_PATH", TEST_VA_DRIVERS_PATH, 1);
    setenv("LIBVA_DRIVER_NAME", "i965", 1);
}

I965TestFixture::~I965TestFixture()
{
    if (m_handle >= 0)
        close(m_handle);
    m_handle = -1;
    m_vaDisplay = NULL;
}

const std::string I965TestFixture::getFullTestName() const
{
    const ::testing::TestInfo * const info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    return std::string(info->test_case_name())
        + std::string(".")
        + std::string(info->name());
}

void I965TestFixture::initialize()
{
    ASSERT_FALSE(NULL == (VADisplay)*this);

    int major, minor;
    ASSERT_STATUS(vaInitialize(*this, &major, &minor));

    EXPECT_EQ(VA_MAJOR_VERSION, major);
    EXPECT_EQ(VA_MINOR_VERSION, minor);

    VADriverContextP context(*this);
    ASSERT_PTR(context);

    const std::string vendor(context->str_vendor);

    ::testing::Test::RecordProperty("driver_vendor", vendor);
    ::testing::Test::RecordProperty("vaapi_version", VA_VERSION_S);
}

void I965TestFixture::terminate()
{
    if (m_vaDisplay)
        EXPECT_STATUS(vaTerminate(m_vaDisplay));
}

I965TestFixture::operator VADisplay()
{
    if (m_vaDisplay)
        return m_vaDisplay;

    m_handle = open("/dev/dri/renderD128", O_RDWR);
    if (m_handle < 0)
        m_handle = open("/dev/dri/card0", O_RDWR);

    m_vaDisplay = vaGetDisplayDRM(m_handle);
    if (!m_vaDisplay && m_handle >= 0) {
        close(m_handle);
        m_handle = -1;
    }

    return m_vaDisplay;
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
        if (ctx)
            EXPECT_STATUS(
                ctx->vtable->vaCreateSurfaces2(
                    *this, format, w, h, surfaces.data(), surfaces.size(),
                    const_cast<VASurfaceAttrib*>(attributes.data()),
                    attributes.size()));
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
    VAProfile profile, VAEntrypoint entrypoint, ConfigAttribs& attribs)
{
    VAConfigID id = VA_INVALID_ID;
    EXPECT_STATUS(
        i965_CreateConfig(
            *this, profile, entrypoint, attribs.data(), attribs.size(), &id));
    EXPECT_ID(id);

    return id;
}

void I965TestFixture::destroyConfig(VAConfigID id)
{
    EXPECT_STATUS(i965_DestroyConfig(*this, id));
}

VAContextID I965TestFixture::createContext(
    VAConfigID config, int w, int h, int flags, Surfaces& targets)
{
    VAContextID id = VA_INVALID_ID;
    EXPECT_STATUS(
        i965_CreateContext(
            *this, config, w, h, flags, targets.data(), targets.size(), &id));
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

class I965TestFixtureTest
    : public I965TestFixture
{
protected:
    virtual void SetUp() { } // override I965TestFixture::SetUp
    virtual void TearDown() { } // override I965TestFixture::TearDown
};

TEST_F(I965TestFixtureTest, Logic)
{
    VADisplayContextP dispCtx(*this);
    VADriverContextP drvCtx(*this);
    struct i965_driver_data* i965(*this);
    VADisplay display(*this);

    EXPECT_PTR(display);
    EXPECT_PTR(dispCtx);
    EXPECT_PTR(drvCtx);
    EXPECT_TRUE(NULL == i965);
    EXPECT_TRUE(NULL == drvCtx->handle);

    ASSERT_NO_FATAL_FAILURE(initialize());

    i965 = *this;
    EXPECT_PTR(i965);
    EXPECT_PTR(drvCtx->handle);

    terminate();
}
