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

#ifndef I965_TEST_IMAGE_UTILS_H
#define I965_TEST_IMAGE_UTILS_H

#include <array>
#include <memory>
#include <valarray>
#include <va/va.h>

class YUVImage
    : public std::enable_shared_from_this<YUVImage>
{
public:
    typedef std::shared_ptr<YUVImage> Shared;
    typedef std::shared_ptr<const YUVImage> SharedConst;

    static Shared create(const unsigned, size_t, size_t);
    static Shared create(const VAImage&);
    static Shared create(const VASurfaceID);

    std::slice_array<uint8_t> y() { return bytes[slices[0]]; }
    std::slice_array<uint8_t> u() { return bytes[slices[1]]; }
    std::slice_array<uint8_t> v() { return bytes[slices[2]]; }

    void toSurface(VASurfaceID) const;

    std::valarray<uint8_t>      bytes;
    size_t                      width;
    size_t                      height;
    unsigned                    fourcc;
    unsigned                    format;
    size_t                      planes;
    std::valarray<size_t>       widths;
    std::valarray<size_t>       heights;
    std::valarray<size_t>       offsets;
    std::valarray<size_t>       sizes;
    std::array<std::slice, 3>   slices;

private:
    YUVImage();
};

#endif
