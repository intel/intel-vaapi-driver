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

#ifndef I965_TEST_ENVIRONMENT_H
#define I965_TEST_ENVIRONMENT_H

#include "test.h"
#include "i965_internal_decl.h"

/**
 * This test environment handles initialization and termination of the i965
 * driver and display. It defines various operators to make it implicitly
 * convertible to a VADriverContextP, VADisplay, VADisplayContextP, and
 * i965_driver_data*.  Other operators may be defined, too.  These operators
 * allow an instance of the test environment to be passed to various driver
 * functions that take one of those parameter types.
 *
 * See the "Global Set-Up and Tear-Down" section in gtest/docs/AdvancedGuide.md
 * for more details on how a ::testing::Environment operates.
 */
class I965TestEnvironment
    : public ::testing::Environment
{
protected:
    /**
     * This is invoked by gtest before any tests are executed.  Gtest will not
     * run any tests if this method generates a fatal test assertion failure.
     */
    virtual void SetUp();

    /**
     * This is invoked by gtest after all the tests are executed. If SetUp()
     * generates a fatal test assertion, this is also invoked by gtest
     * afterwards.
     */
    virtual void TearDown();

private:
    I965TestEnvironment();

    int m_handle; /* current native display handle */
    VADisplay m_vaDisplay; /* current VADisplay handle */

public:
    static I965TestEnvironment* instance();

    /**
     * VADisplay implicit and explicit conversion operator.
     */
    inline operator VADisplay() { return m_vaDisplay; }

    /**
     * VADisplayContextP implict and explicit conversion operator.
     */
    inline operator VADisplayContextP()
    {
        return (VADisplayContextP)((VADisplay)*this);
    }

    /**
     * VADriverContextP implict and explicit conversion operator.
     */
    inline operator VADriverContextP()
    {
        VADisplayContextP dctx(*this);
        return dctx ? dctx->pDriverContext : NULL;
    }

    /**
     * i965_driver_data * implict and explicit conversion operator.
     */
    inline operator struct i965_driver_data *()
    {
        VADriverContextP ctx(*this);
        return ctx ? i965_driver_data(ctx) : NULL;
    }
};

#endif
