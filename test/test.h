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

#ifndef TEST_H
#define TEST_H

#include <gtest/gtest.h>
#include <va/va.h>

#define EXPECT_STATUS(status) \
    EXPECT_EQ(VA_STATUS_SUCCESS, (status))

#define ASSERT_STATUS(status) \
    ASSERT_EQ(VA_STATUS_SUCCESS, (status))

#define EXPECT_ID(id) \
    EXPECT_NE(VA_INVALID_ID, (id))

#define ASSERT_ID(id) \
    ASSERT_NE(VA_INVALID_ID, (id))

#define EXPECT_PTR(ptr) \
    EXPECT_FALSE(NULL == (ptr))

#define ASSERT_PTR(ptr) \
    ASSERT_FALSE(NULL == (ptr))

#endif // TEST_H
