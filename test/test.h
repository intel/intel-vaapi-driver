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
#include <gtest/gtest-spi.h> // for EXPECT_FATAL_FAILURE/EXPECT_NONFATAL_FAILURE
#include <iostream>
#include <string>
#include <va/va.h>

#define EXPECT_STATUS(status) \
    EXPECT_EQ(VaapiStatus(VA_STATUS_SUCCESS), VaapiStatus(status))

#define ASSERT_STATUS(status) \
    ASSERT_EQ(VaapiStatus(VA_STATUS_SUCCESS), VaapiStatus(status))

#define EXPECT_STATUS_EQ(expect, status) \
    EXPECT_EQ(VaapiStatus(expect), VaapiStatus(status))

#define ASSERT_STATUS_EQ(expect, status) \
    ASSERT_EQ(VaapiStatus(expect), VaapiStatus(status))

#define EXPECT_ID(id) \
    EXPECT_NE(VA_INVALID_ID, (id))

#define ASSERT_ID(id) \
    ASSERT_NE(VA_INVALID_ID, (id))

#define EXPECT_INVALID_ID(id) \
    EXPECT_EQ(VA_INVALID_ID, (id))

#define ASSERT_INVALID_ID(id) \
    ASSERT_EQ(VA_INVALID_ID, (id))

#define EXPECT_PTR(ptr) \
    EXPECT_FALSE(NULL == (ptr))

#define ASSERT_PTR(ptr) \
    ASSERT_FALSE(NULL == (ptr))

#define EXPECT_PTR_NULL(ptr) \
    EXPECT_TRUE(NULL == (ptr))

#define ASSERT_PTR_NULL(ptr) \
    ASSERT_TRUE(NULL == (ptr))

#define ASSERT_NO_FAILURE(statement) \
    statement; \
    ASSERT_FALSE(HasFailure());

class VaapiStatus
{
public:
    explicit VaapiStatus(VAStatus status)
      : m_status(status)
    { }

    bool operator ==(const VaapiStatus& other) const
    {
        return m_status == other.m_status;
    }

    friend std::ostream& operator <<(std::ostream& os, const VaapiStatus& t)
    {
        std::string status;
        switch(t.m_status) {
        case VA_STATUS_SUCCESS:
            status = "VA_STATUS_SUCCESS"; break;
        case VA_STATUS_ERROR_OPERATION_FAILED:
            status = "VA_STATUS_ERROR_OPERATION_FAILED"; break;
        case VA_STATUS_ERROR_ALLOCATION_FAILED:
            status = "VA_STATUS_ERROR_ALLOCATION_FAILED"; break;
        case VA_STATUS_ERROR_INVALID_DISPLAY:
            status = "VA_STATUS_ERROR_INVALID_DISPLAY"; break;
        case VA_STATUS_ERROR_INVALID_CONFIG:
            status = "VA_STATUS_ERROR_INVALID_CONFIG"; break;
        case VA_STATUS_ERROR_INVALID_CONTEXT:
            status = "VA_STATUS_ERROR_INVALID_CONTEXT"; break;
        case VA_STATUS_ERROR_INVALID_SURFACE:
            status = "VA_STATUS_ERROR_INVALID_SURFACE"; break;
        case VA_STATUS_ERROR_INVALID_BUFFER:
            status = "VA_STATUS_ERROR_INVALID_BUFFER"; break;
        case VA_STATUS_ERROR_INVALID_IMAGE:
            status = "VA_STATUS_ERROR_INVALID_IMAGE"; break;
        case VA_STATUS_ERROR_INVALID_SUBPICTURE:
            status = "VA_STATUS_ERROR_INVALID_SUBPICTURE"; break;
        case VA_STATUS_ERROR_ATTR_NOT_SUPPORTED:
            status = "VA_STATUS_ERROR_ATTR_NOT_SUPPORTED"; break;
        case VA_STATUS_ERROR_MAX_NUM_EXCEEDED:
            status = "VA_STATUS_ERROR_MAX_NUM_EXCEEDED"; break;
        case VA_STATUS_ERROR_UNSUPPORTED_PROFILE:
            status = "VA_STATUS_ERROR_UNSUPPORTED_PROFILE"; break;
        case VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT:
            status = "VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT"; break;
        case VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT:
            status = "VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT"; break;
        case VA_STATUS_ERROR_UNSUPPORTED_BUFFERTYPE:
            status = "VA_STATUS_ERROR_UNSUPPORTED_BUFFERTYPE"; break;
        case VA_STATUS_ERROR_SURFACE_BUSY:
            status = "VA_STATUS_ERROR_SURFACE_BUSY"; break;
        case VA_STATUS_ERROR_FLAG_NOT_SUPPORTED:
            status = "VA_STATUS_ERROR_FLAG_NOT_SUPPORTED"; break;
        case VA_STATUS_ERROR_INVALID_PARAMETER:
            status = "VA_STATUS_ERROR_INVALID_PARAMETER"; break;
        case VA_STATUS_ERROR_RESOLUTION_NOT_SUPPORTED:
            status = "VA_STATUS_ERROR_RESOLUTION_NOT_SUPPORTED"; break;
        case VA_STATUS_ERROR_UNIMPLEMENTED:
            status = "VA_STATUS_ERROR_UNIMPLEMENTED"; break;
        case VA_STATUS_ERROR_SURFACE_IN_DISPLAYING:
            status = "VA_STATUS_ERROR_SURFACE_IN_DISPLAYING"; break;
        case VA_STATUS_ERROR_INVALID_IMAGE_FORMAT:
            status = "VA_STATUS_ERROR_INVALID_IMAGE_FORMAT"; break;
        case VA_STATUS_ERROR_DECODING_ERROR:
            status = "VA_STATUS_ERROR_DECODING_ERROR"; break;
        case VA_STATUS_ERROR_ENCODING_ERROR:
            status = "VA_STATUS_ERROR_ENCODING_ERROR"; break;
        case VA_STATUS_ERROR_INVALID_VALUE:
            status = "VA_STATUS_ERROR_INVALID_VALUE"; break;
        case VA_STATUS_ERROR_UNSUPPORTED_FILTER:
            status = "VA_STATUS_ERROR_UNSUPPORTED_FILTER"; break;
        case VA_STATUS_ERROR_INVALID_FILTER_CHAIN:
            status = "VA_STATUS_ERROR_INVALID_FILTER_CHAIN"; break;
        case VA_STATUS_ERROR_HW_BUSY:
            status = "VA_STATUS_ERROR_HW_BUSY"; break;
        case VA_STATUS_ERROR_UNSUPPORTED_MEMORY_TYPE:
            status = "VA_STATUS_ERROR_UNSUPPORTED_MEMORY_TYPE"; break;
        case VA_STATUS_ERROR_UNKNOWN:
            status = "VA_STATUS_ERROR_UNKNOWN"; break;
        default:
            status = "Unknown VAStatus";
        }
        os << status;
        return os;
    }

    VAStatus m_status;
};

#endif // TEST_H
