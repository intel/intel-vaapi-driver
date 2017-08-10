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

#include "i965_test_environment.h"

#include <cstdlib>
#include <ctime>
#include <fcntl.h> // for O_RDWR
#include <unistd.h> // for close()
#include <va/va_drm.h>

I965TestEnvironment* I965TestEnvironment::instance()
{
    static I965TestEnvironment* e = new I965TestEnvironment;
    return e;
}

I965TestEnvironment::I965TestEnvironment()
    : ::testing::Environment()
    , m_handle(-1)
    , m_vaDisplay(NULL)
{
    return;
}

void I965TestEnvironment::SetUp()
{
    std::time_t seed(std::time(0));
    std::srand(seed);
    ::testing::Test::RecordProperty("rand_seed", seed);
    std::cout << "Seeded std::rand() with " << seed << "." << std::endl;

    ASSERT_EQ(-1, m_handle);
    ASSERT_PTR_NULL(m_vaDisplay);

    m_handle = open("/dev/dri/renderD128", O_RDWR);
    if (m_handle < 0)
        m_handle = open("/dev/dri/card0", O_RDWR);

    m_vaDisplay = vaGetDisplayDRM(m_handle);

    ASSERT_PTR(m_vaDisplay);

    setenv("LIBVA_DRIVERS_PATH", TEST_VA_DRIVERS_PATH, 1);
    setenv("LIBVA_DRIVER_NAME", "i965", 1);

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

void I965TestEnvironment::TearDown()
{
    if (m_vaDisplay) {
        EXPECT_STATUS(vaTerminate(m_vaDisplay));
    }

    if (m_handle >= 0)
        close(m_handle);

    m_handle = -1;
    m_vaDisplay = NULL;
}
