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

#ifndef I965_TEST_FIXTURE_H
#define I965_TEST_FIXTURE_H

#include "i965_test_environment.h"

#include <string>
#include <vector>

typedef std::vector<VASurfaceID> Surfaces;
typedef std::vector<VASurfaceAttrib> SurfaceAttribs;
typedef std::vector<VAConfigAttrib> ConfigAttribs;
typedef std::vector<VABufferID> Buffers;

/**
 * This test fixture defines various operators to make it implicitly convertible
 * to a VADriverContextP, VADisplay, VADisplayContextP, and i965_driver_data*.
 * Other operators may be defined, too.  These operators allow an instance of
 * the test fixture to be passed to various driver functions that take one of
 * those parameter types.  Various driver functions are also wrapped by this
 * test fixture to simplify writing test cases.
 *
 * Test cases that wish to use this fixture should define their own test
 * fixture class that derives from this one.
 *
 * See the "Test Fixtures" section in gtest/docs/Primer.md for more details
 * on how test fixtures are used.
 */
class I965TestFixture
    : public ::testing::Test
{
public:
    virtual ~I965TestFixture() { }

    const std::string getFullTestName() const;

    /**
     * Convenience wrapper for i965_CreateSurfaces or i965_CreateSurfaces2.
     * If SurfaceAttribs are specified then i965_CreateSurfaces2 is used,
     * otherwise i965_CreateSurfaces is used. May generate a non-fatal test
     * assertion failure.
     */
    Surfaces createSurfaces(int w, int h, int format, size_t count = 1,
        const SurfaceAttribs& = SurfaceAttribs());

    /**
     * Convenience wrapper for i965_DestroySurfaces.  May generate a non-fatal
     * test assertion failure.
     */
    void destroySurfaces(Surfaces&);

    /**
     * Convenience wrapper for i965_CreateConfig.  May generate a non-fatal
     * test assertion failure.
     */
    VAConfigID createConfig(VAProfile, VAEntrypoint,
        const ConfigAttribs& = ConfigAttribs(),
        const VAStatus = VA_STATUS_SUCCESS);

    /**
     * Convenience wrapper for i965_DestroyConfig.  May generate a non-fatal
     * test assertion failure.
     */
    void destroyConfig(VAConfigID);

    /**
     * Convenience wrapper for i965_CreateContext.  May generate a non-fatal
     * test assertion failure.
     */
    VAContextID createContext(VAConfigID, int, int, int = 0,
        const Surfaces& = Surfaces());

    /**
     * Convenience wrapper for i965_DestroyContext.  May generate a non-fatal
     * test assertion failure.
     */
    void destroyContext(VAContextID);

    /**
     * Convenience wrapper for i965_CreateBuffer.  May generate a non-fatal
     * test assertion failure.
     */
    VABufferID createBuffer(
        VAContextID, VABufferType, unsigned, unsigned = 1, const void * = NULL);

    /**
     * Convenience wrapper for i965_DestroyBuffer.  May generate a non-fatal
     * test assertion failure.
     */
    void destroyBuffer(VABufferID);

    /**
     * Convenience wrapper for i965_MapBuffer.  May generate a non-fatal
     * test assertion failure.
     */
    template<typename T>
    T* mapBuffer(VABufferID id)
    {
        T* data = NULL;
        EXPECT_STATUS(
            i965_MapBuffer(*this, id, (void**)&data));
        EXPECT_PTR(data);
        return data;
    }

    /**
     * Convenience Wrapper for i965_UnmapBuffer.  May generate a non-fatal
     * test assertion failure.
     */
    void unmapBuffer(VABufferID id)
    {
        EXPECT_STATUS(
            i965_UnmapBuffer(*this, id));
    }

    /**
     * Convenience wrapper for i965_BeginPicture.  May generate a non-fatal
     * test assertion failure.
     */
    void beginPicture(VAContextID, VASurfaceID);

    /**
     * Convenience wrapper for i965_RenderPicture.  May generate a non-fatal
     * test assertion failure.
     */
    void renderPicture(VAContextID, VABufferID *, int = 1);

    /**
     * Convenience wrapper for i965_EndPicture.  May generate a non-fatal
     * test assertion failure.
     */
    void endPicture(VAContextID);

    /**
     * Convenience wrapper for i965_DeriveImage.  May generate a non-fatal
     * test assertion failure.
     */
    void deriveImage(VASurfaceID, VAImage &);

    /**
     * Convenience wrapper for i965_DestroyImage.  May generate a non-fatal
     * test assertion failure.
     */
    void destroyImage(VAImage &);

    /**
     * Convenience wrapper for i965_SyncSurface.  May generate a non-fatal
     * test assertion failure.
     */
    void syncSurface(VASurfaceID);

    /**
     * VADisplay implicit and explicit conversion operator.
     */
    inline operator VADisplay()
    { return *I965TestEnvironment::instance(); }

    /**
     * VADisplayContextP implict and explicit conversion operator.
     */
    inline operator VADisplayContextP()
    { return *I965TestEnvironment::instance(); }

    /**
     * VADriverContextP implict and explicit conversion operator.
     */
    inline operator VADriverContextP()
    { return *I965TestEnvironment::instance(); }

    /**
     * i965_driver_data * implict and explicit conversion operator.
     */
    inline operator struct i965_driver_data *()
    { return *I965TestEnvironment::instance(); }
};

#endif
