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
#include "i965_test_image_utils.h"

#include <cstring> // std::memset

YUVImage::YUVImage()
    : bytes()
    , width(0)
    , height(0)
    , fourcc(0)
    , format(0)
    , planes(0)
    , widths{0,0,0}
    , heights{0,0,0}
    , offsets{0,0,0}
    , sizes{0,0,0}
    , slices()
{
    return;
}

YUVImage::Shared YUVImage::create(
    const unsigned fourcc, size_t w, size_t h)
{
    Shared t(new YUVImage);

    t->fourcc = fourcc;
    t->width = w = w + (w & 1);
    t->height = h = h + (h & 1);

    switch(fourcc) {
    case VA_FOURCC_444P:
        t->planes = 3;
        t->widths = {w, w, w};
        t->heights = {h, h, h};
        t->format = VA_RT_FORMAT_YUV444;
        break;
    case VA_FOURCC_IMC3:
    case VA_FOURCC_I420:
        t->planes = 3;
        t->widths = {w, w >> 1, w >> 1};
        t->heights = {h, h >> 1, h >> 1};
        t->format = VA_RT_FORMAT_YUV420;
        break;
    case VA_FOURCC_NV12:
        t->planes = 2;
        t->widths = {w, w, 0};
        t->heights = {h, h >> 1, 0};
        t->format = VA_RT_FORMAT_YUV420;
        break;
    case VA_FOURCC_UYVY:
    case VA_FOURCC_YUY2:
        t->planes = 1;
        t->widths = {w << 1, 0, 0};
        t->heights = {h, 0, 0};
        t->format = VA_RT_FORMAT_YUV422;
        break;
    case VA_FOURCC_422H:
        t->planes = 3;
        t->widths = {w, w >> 1, w >> 1};
        t->heights = {h, h, h};
        t->format = VA_RT_FORMAT_YUV422;
        break;
    case VA_FOURCC_422V:
        t->planes = 3;
        t->widths = {w, w, w};
        t->heights = {h, h >> 1,h >> 1};
        t->format = VA_RT_FORMAT_YUV422;
        break;
    case VA_FOURCC_Y800:
        t->planes = 1;
        t->widths = {w, 0, 0};
        t->heights = {h, 0, 0};
        t->format = VA_RT_FORMAT_YUV400;
        break;
    default:
        return Shared(); // fourcc is unsupported
    }

    t->sizes = t->widths * t->heights;
    t->bytes = std::valarray<uint8_t>(t->sizes.sum());

    for (size_t i(1); i < t->planes; ++i)
        t->offsets[i] = t->sizes[i - 1] + t->offsets[i - 1];

    // Initialize slices
    switch(fourcc) {
    case VA_FOURCC_444P:
    case VA_FOURCC_IMC3:
    case VA_FOURCC_I420:
    case VA_FOURCC_422H:
    case VA_FOURCC_422V:
        t->slices[1] = std::slice{t->offsets[1], t->sizes[1], 1};
        t->slices[2] = std::slice{t->offsets[2], t->sizes[2], 1};
        /* fall-through */
    case VA_FOURCC_Y800:
        t->slices[0] = std::slice{t->offsets[0], t->sizes[0], 1};
        break;
    case VA_FOURCC_NV12:
        t->slices[0] = std::slice{t->offsets[0], t->sizes[0], 1};
        t->slices[1] = std::slice{t->offsets[1], t->sizes[1]/2, 2};
        t->slices[2] = std::slice{t->offsets[1] + 1, t->sizes[1]/2, 2};
        break;
    case VA_FOURCC_UYVY:
        t->slices[0] = std::slice{t->offsets[0] + 1, t->sizes[0]/2, 2};
        t->slices[1] = std::slice{t->offsets[0], t->sizes[0]/4, 4};
        t->slices[2] = std::slice{t->offsets[0] + 2, t->sizes[0]/4, 4};
        break;
    case VA_FOURCC_YUY2:
        t->slices[0] = std::slice{t->offsets[0], t->sizes[0]/2, 2};
        t->slices[1] = std::slice{t->offsets[0] + 1, t->sizes[0]/4, 4};
        t->slices[2] = std::slice{t->offsets[0] + 3, t->sizes[0]/4, 4};
        break;
    default:
        return Shared(); // fourcc is unsupported
    }

    return t;
}

YUVImage::Shared YUVImage::create(const VAImage& image)
{
    I965TestEnvironment& env = *I965TestEnvironment::instance();

    Shared result = create(image.format.fourcc, image.width, image.height);
    EXPECT_PTR(result.get());

    if (::testing::Test::HasFailure())
        return Shared();

    EXPECT_EQ(result->fourcc, image.format.fourcc);
    EXPECT_EQ(result->planes, image.num_planes);
    EXPECT_EQ(result->width, image.width);
    EXPECT_EQ(result->height, image.height);
    EXPECT_GE(image.data_size, result->bytes.size());

    if (::testing::Test::HasFailure())
        return Shared();

    uint8_t* data = NULL;
    EXPECT_STATUS(i965_MapBuffer(env, image.buf, (void**)&data));

    if (::testing::Test::HasFailure())
        return Shared();

    auto it(std::begin(result->bytes));
    for (size_t i(0); i < image.num_planes; ++i) {
        const size_t pitch(image.pitches[i]);
        const size_t width(result->widths[i]);
        const size_t height(result->heights[i]);
        const uint8_t *source = data + image.offsets[i];

        EXPECT_GE(pitch, width);
        if (::testing::Test::HasFailure())
            break;

        for (size_t j(0); j < height; ++j) {
            std::copy(source, source + width, it);
            source += pitch;
            it += width;
        }
    }

    EXPECT_STATUS(i965_UnmapBuffer(env, image.buf));

    if (::testing::Test::HasFailure())
        return Shared();

    return result;
}

YUVImage::Shared YUVImage::create(const VASurfaceID surface)
{
    I965TestEnvironment& env = *I965TestEnvironment::instance();
    VAImage image;

    EXPECT_STATUS(i965_DeriveImage(env, surface, &image));

    if (::testing::Test::HasFailure())
        return Shared();

    Shared result = YUVImage::create(image);

    EXPECT_STATUS(i965_DestroyImage(env, image.image_id));

    return result;
}

void YUVImage::toSurface(VASurfaceID surface) const
{
    I965TestEnvironment& env = *I965TestEnvironment::instance();
    VAImage image;

    ASSERT_STATUS(i965_DeriveImage(env, surface, &image));

    EXPECT_ID(image.image_id);
    EXPECT_EQ(fourcc, image.format.fourcc);
    EXPECT_EQ(planes, image.num_planes);
    EXPECT_EQ(width, image.width);
    EXPECT_EQ(height, image.height);
    EXPECT_GE(image.data_size, bytes.size());

    if (::testing::Test::HasFailure()) {
        EXPECT_STATUS(i965_DestroyImage(env, image.image_id));
        return;
    }

    uint8_t* data = NULL;
    EXPECT_STATUS(i965_MapBuffer(env, image.buf, (void**)&data));
    EXPECT_PTR(data);

    if (::testing::Test::HasFailure()) {
        EXPECT_STATUS(i965_DestroyImage(env, image.image_id));
        return;
    }

    std::memset(data, 0, image.data_size);

    auto it(std::begin(bytes));
    for (size_t i(0); i < image.num_planes; ++i) {
        const size_t pitch(image.pitches[i]);
        const size_t w(widths[i]);
        const size_t h(heights[i]);
        uint8_t *dest = data + image.offsets[i];

        EXPECT_GE(pitch, w);
        if (::testing::Test::HasFailure())
            break;

        for (size_t j(0); j < h; ++j) {
            std::copy(it, it + w, dest);
            dest += pitch;
            it += w;
        }
    }

    EXPECT_STATUS(i965_UnmapBuffer(env, image.buf));
    EXPECT_STATUS(i965_DestroyImage(env, image.image_id));
}

TEST(YUVImageTest, 444P)
{
    std::valarray<uint8_t> data = {
        0x11,0xaf,0x23,0xff,0x00,0x73,0x54,0xcc,0xca,0x6b,0x12,0x99
    };

    YUVImage::Shared image = YUVImage::create(VA_FOURCC_444P, 2, 2);
    ASSERT_PTR(image.get());

    EXPECT_EQ(data.size(), image->bytes.size());
    EXPECT_EQ(2u, image->width);
    EXPECT_EQ(2u, image->height);
    EXPECT_EQ(3u, image->planes);
    EXPECT_TRUE( (std::valarray<size_t>{2,2,2} == image->widths).min() );
    EXPECT_TRUE( (std::valarray<size_t>{2,2,2} == image->heights).min() );
    EXPECT_TRUE( (std::valarray<size_t>{4,4,4} == image->sizes).min() );
    EXPECT_TRUE( (std::valarray<size_t>{0,4,8} == image->offsets).min() );

    image->bytes = data;

    std::valarray<uint8_t> y = image->y();
    std::valarray<uint8_t> u = image->u();
    std::valarray<uint8_t> v = image->v();

    EXPECT_TRUE( (std::valarray<uint8_t>{0x11,0xaf,0x23,0xff} == y).min() );
    EXPECT_TRUE( (std::valarray<uint8_t>{0x00,0x73,0x54,0xcc} == u).min() );
    EXPECT_TRUE( (std::valarray<uint8_t>{0xca,0x6b,0x12,0x99} == v).min() );

    image->bytes = uint8_t(0);

    EXPECT_FALSE( (image->bytes == data).min() );

    image->y() = y;
    image->u() = u;
    image->v() = v;

    EXPECT_TRUE( (image->bytes == data).min() );
}

TEST(YUVImageTest, IMC3)
{
    std::valarray<uint8_t> data = {0x11,0xaf,0x23,0xff,0x00,0x73};

    YUVImage::Shared image = YUVImage::create(VA_FOURCC_IMC3, 2, 2);
    ASSERT_PTR(image.get());

    EXPECT_EQ(data.size(), image->bytes.size());
    EXPECT_EQ(2u, image->width);
    EXPECT_EQ(2u, image->height);
    EXPECT_EQ(3u, image->planes);
    EXPECT_TRUE( (std::valarray<size_t>{2,1,1} == image->widths).min() );
    EXPECT_TRUE( (std::valarray<size_t>{2,1,1} == image->heights).min() );
    EXPECT_TRUE( (std::valarray<size_t>{4,1,1} == image->sizes).min() );
    EXPECT_TRUE( (std::valarray<size_t>{0,4,5} == image->offsets).min() );

    image->bytes = data;

    std::valarray<uint8_t> y = image->y();
    std::valarray<uint8_t> u = image->u();
    std::valarray<uint8_t> v = image->v();

    EXPECT_TRUE( (std::valarray<uint8_t>{0x11,0xaf,0x23,0xff} == y).min() );
    EXPECT_TRUE( (std::valarray<uint8_t>{0x00} == u).min() );
    EXPECT_TRUE( (std::valarray<uint8_t>{0x73} == v).min() );

    image->bytes = uint8_t(0);

    EXPECT_FALSE( (image->bytes == data).min() );

    image->y() = y;
    image->u() = u;
    image->v() = v;

    EXPECT_TRUE( (image->bytes == data).min() );
}

TEST(YUVImageTest, I420)
{
    std::valarray<uint8_t> data = {0x11,0xaf,0x23,0xff,0x00,0x73};

    YUVImage::Shared image = YUVImage::create(VA_FOURCC_I420, 2, 2);
    ASSERT_PTR(image.get());

    EXPECT_EQ(data.size(), image->bytes.size());
    EXPECT_EQ(2u, image->width);
    EXPECT_EQ(2u, image->height);
    EXPECT_EQ(3u, image->planes);
    EXPECT_TRUE( (std::valarray<size_t>{2,1,1} == image->widths).min() );
    EXPECT_TRUE( (std::valarray<size_t>{2,1,1} == image->heights).min() );
    EXPECT_TRUE( (std::valarray<size_t>{4,1,1} == image->sizes).min() );
    EXPECT_TRUE( (std::valarray<size_t>{0,4,5} == image->offsets).min() );

    image->bytes = data;

    std::valarray<uint8_t> y = image->y();
    std::valarray<uint8_t> u = image->u();
    std::valarray<uint8_t> v = image->v();

    EXPECT_TRUE( (std::valarray<uint8_t>{0x11,0xaf,0x23,0xff} == y).min() );
    EXPECT_TRUE( (std::valarray<uint8_t>{0x00} == u).min() );
    EXPECT_TRUE( (std::valarray<uint8_t>{0x73} == v).min() );

    image->bytes = uint8_t(0);

    EXPECT_FALSE( (image->bytes == data).min() );

    image->y() = y;
    image->u() = u;
    image->v() = v;

    EXPECT_TRUE( (image->bytes == data).min() );
}

TEST(YUVImageTest, NV12)
{
    std::valarray<uint8_t> data = {
        0x11,0xaf,0x23,0xff,0x00,0x73,0x54,0xcc,0xca,0x6b,0x12,0x99};

    YUVImage::Shared image = YUVImage::create(VA_FOURCC_NV12, 2, 4);
    ASSERT_PTR(image.get());

    EXPECT_EQ(data.size(), image->bytes.size());
    EXPECT_EQ(2u, image->width);
    EXPECT_EQ(4u, image->height);
    EXPECT_EQ(2u, image->planes);
    EXPECT_TRUE( (std::valarray<size_t>{2,2,0} == image->widths).min() );
    EXPECT_TRUE( (std::valarray<size_t>{4,2,0} == image->heights).min() );
    EXPECT_TRUE( (std::valarray<size_t>{8,4,0} == image->sizes).min() );
    EXPECT_TRUE( (std::valarray<size_t>{0,8,0} == image->offsets).min() );

    image->bytes = data;

    std::valarray<uint8_t> y = image->y();
    std::valarray<uint8_t> u = image->u();
    std::valarray<uint8_t> v = image->v();

    EXPECT_TRUE( (std::valarray<uint8_t>{0x11,0xaf,0x23,0xff,0x00,0x73,0x54,0xcc} == y).min() );
    EXPECT_TRUE( (std::valarray<uint8_t>{0xca,0x12} == u).min() );
    EXPECT_TRUE( (std::valarray<uint8_t>{0x6b,0x99} == v).min() );

    image->bytes = uint8_t(0);

    EXPECT_FALSE( (image->bytes == data).min() );

    image->y() = y;
    image->u() = u;
    image->v() = v;

    EXPECT_TRUE( (image->bytes == data).min() );
}

TEST(YUVImageTest, UYVY)
{
    std::valarray<uint8_t> data = {0x11,0xaf,0x23,0xff,0x00,0x73,0x54,0xcc};

    YUVImage::Shared image = YUVImage::create(VA_FOURCC_UYVY, 2, 2);
    ASSERT_PTR(image.get());

    EXPECT_EQ(data.size(), image->bytes.size());
    EXPECT_EQ(2u, image->width);
    EXPECT_EQ(2u, image->height);
    EXPECT_EQ(1u, image->planes);
    EXPECT_TRUE( (std::valarray<size_t>{4,0,0} == image->widths).min() );
    EXPECT_TRUE( (std::valarray<size_t>{2,0,0} == image->heights).min() );
    EXPECT_TRUE( (std::valarray<size_t>{8,0,0} == image->sizes).min() );
    EXPECT_TRUE( (std::valarray<size_t>{0,0,0} == image->offsets).min() );

    image->bytes = data;

    std::valarray<uint8_t> y = image->y();
    std::valarray<uint8_t> u = image->u();
    std::valarray<uint8_t> v = image->v();

    EXPECT_TRUE( (std::valarray<uint8_t>{0xaf,0xff,0x73,0xcc} == y).min() );
    EXPECT_TRUE( (std::valarray<uint8_t>{0x11,0x00} == u).min() );
    EXPECT_TRUE( (std::valarray<uint8_t>{0x23,0x54} == v).min() );

    image->bytes = uint8_t(0);

    EXPECT_FALSE( (image->bytes == data).min() );

    image->y() = y;
    image->u() = u;
    image->v() = v;

    EXPECT_TRUE( (image->bytes == data).min() );
}

TEST(YUVImageTest, YUY2)
{
    std::valarray<uint8_t> data = {0x11,0xaf,0x23,0xff,0x00,0x73,0x54,0xcc};

    YUVImage::Shared image = YUVImage::create(VA_FOURCC_YUY2, 2, 2);
    ASSERT_PTR(image.get());

    EXPECT_EQ(data.size(), image->bytes.size());
    EXPECT_EQ(2u, image->width);
    EXPECT_EQ(2u, image->height);
    EXPECT_EQ(1u, image->planes);
    EXPECT_TRUE( (std::valarray<size_t>{4,0,0} == image->widths).min() );
    EXPECT_TRUE( (std::valarray<size_t>{2,0,0} == image->heights).min() );
    EXPECT_TRUE( (std::valarray<size_t>{8,0,0} == image->sizes).min() );
    EXPECT_TRUE( (std::valarray<size_t>{0,0,0} == image->offsets).min() );

    image->bytes = data;

    std::valarray<uint8_t> y = image->y();
    std::valarray<uint8_t> u = image->u();
    std::valarray<uint8_t> v = image->v();

    EXPECT_TRUE( (std::valarray<uint8_t>{0x11,0x23,0x00,0x54} == y).min() );
    EXPECT_TRUE( (std::valarray<uint8_t>{0xaf,0x73} == u).min() );
    EXPECT_TRUE( (std::valarray<uint8_t>{0xff,0xcc} == v).min() );

    image->bytes = uint8_t(0);

    EXPECT_FALSE( (image->bytes == data).min() );

    image->y() = y;
    image->u() = u;
    image->v() = v;

    EXPECT_TRUE( (image->bytes == data).min() );
}

TEST(YUVImageTest, 422H)
{
    std::valarray<uint8_t> data = {0x11,0xaf,0x23,0xff,0x00,0x73,0x54,0xcc};

    YUVImage::Shared image = YUVImage::create(VA_FOURCC_422H, 2, 2);
    ASSERT_PTR(image.get());

    EXPECT_EQ(data.size(), image->bytes.size());
    EXPECT_EQ(2u, image->width);
    EXPECT_EQ(2u, image->height);
    EXPECT_EQ(3u, image->planes);
    EXPECT_TRUE( (std::valarray<size_t>{2,1,1} == image->widths).min() );
    EXPECT_TRUE( (std::valarray<size_t>{2,2,2} == image->heights).min() );
    EXPECT_TRUE( (std::valarray<size_t>{4,2,2} == image->sizes).min() );
    EXPECT_TRUE( (std::valarray<size_t>{0,4,6} == image->offsets).min() );

    image->bytes = data;

    std::valarray<uint8_t> y = image->y();
    std::valarray<uint8_t> u = image->u();
    std::valarray<uint8_t> v = image->v();

    EXPECT_TRUE( (std::valarray<uint8_t>{0x11,0xaf,0x23,0xff} == y).min() );
    EXPECT_TRUE( (std::valarray<uint8_t>{0x00,0x73} == u).min() );
    EXPECT_TRUE( (std::valarray<uint8_t>{0x54,0xcc} == v).min() );

    image->bytes = uint8_t(0);

    EXPECT_FALSE( (image->bytes == data).min() );

    image->y() = y;
    image->u() = u;
    image->v() = v;

    EXPECT_TRUE( (image->bytes == data).min() );
}

TEST(YUVImageTest, 422V)
{
    std::valarray<uint8_t> data = {0x11,0xaf,0x23,0xff,0x00,0x73,0x54,0xcc};

    YUVImage::Shared image = YUVImage::create(VA_FOURCC_422V, 2, 2);
    ASSERT_PTR(image.get());

    EXPECT_EQ(data.size(), image->bytes.size());
    EXPECT_EQ(2u, image->width);
    EXPECT_EQ(2u, image->height);
    EXPECT_EQ(3u, image->planes);
    EXPECT_TRUE( (std::valarray<size_t>{2,2,2} == image->widths).min() );
    EXPECT_TRUE( (std::valarray<size_t>{2,1,1} == image->heights).min() );
    EXPECT_TRUE( (std::valarray<size_t>{4,2,2} == image->sizes).min() );
    EXPECT_TRUE( (std::valarray<size_t>{0,4,6} == image->offsets).min() );

    image->bytes = data;

    std::valarray<uint8_t> y = image->y();
    std::valarray<uint8_t> u = image->u();
    std::valarray<uint8_t> v = image->v();

    EXPECT_TRUE( (std::valarray<uint8_t>{0x11,0xaf,0x23,0xff} == y).min() );
    EXPECT_TRUE( (std::valarray<uint8_t>{0x00,0x73} == u).min() );
    EXPECT_TRUE( (std::valarray<uint8_t>{0x54,0xcc} == v).min() );

    image->bytes = uint8_t(0);

    EXPECT_FALSE( (image->bytes == data).min() );

    image->y() = y;
    image->u() = u;
    image->v() = v;

    EXPECT_TRUE( (image->bytes == data).min() );
}

TEST(YUVImageTest, Y800)
{
    std::valarray<uint8_t> data = {0x11,0xaf,0x23,0xff};

    YUVImage::Shared image = YUVImage::create(VA_FOURCC_Y800, 2, 2);
    ASSERT_PTR(image.get());

    EXPECT_EQ(data.size(), image->bytes.size());
    EXPECT_EQ(2u, image->width);
    EXPECT_EQ(2u, image->height);
    EXPECT_EQ(1u, image->planes);
    EXPECT_TRUE( (std::valarray<size_t>{2,0,0} == image->widths).min() );
    EXPECT_TRUE( (std::valarray<size_t>{2,0,0} == image->heights).min() );
    EXPECT_TRUE( (std::valarray<size_t>{4,0,0} == image->sizes).min() );
    EXPECT_TRUE( (std::valarray<size_t>{0,0,0} == image->offsets).min() );

    image->bytes = data;

    std::valarray<uint8_t> y = image->y();

    EXPECT_TRUE( (std::valarray<uint8_t>{0x11,0xaf,0x23,0xff} == y).min() );

    image->bytes = uint8_t(0);

    EXPECT_FALSE( (image->bytes == data).min() );

    image->y() = y;

    EXPECT_TRUE( (image->bytes == data).min() );
}

TEST(YUVImageTest, Invalid)
{
    YUVImage::Shared image = YUVImage::create(VA_FOURCC('B','E','E','F'), 2, 2);
    EXPECT_PTR_NULL(image.get());
}

TEST(YUVImageTest, I420toNV12)
{
    std::valarray<uint8_t> data1 = {
        0x11,0xaf,0x23,0xff,0x00,0x73,0x54,0xcc,0xca,0x6b,0x12,0x99};
    std::valarray<uint8_t> data2 = {
        0x11,0xaf,0x23,0xff,0x00,0x73,0x54,0xcc,0xca,0x12,0x6b,0x99};

    YUVImage::Shared image1 = YUVImage::create(VA_FOURCC_I420, 2, 4);
    YUVImage::Shared image2 = YUVImage::create(VA_FOURCC_NV12, 2, 4);
    ASSERT_PTR(image1.get());
    ASSERT_PTR(image2.get());

    image1->bytes = data1;
    image2->y() = image1->y();
    image2->u() = image1->u();
    image2->v() = image1->v();

    EXPECT_TRUE( (image2->bytes == data2).min() );
}

TEST(YUVImageTest, NV12toI420)
{
    std::valarray<uint8_t> data1 = {
        0x11,0xaf,0x23,0xff,0x00,0x73,0x54,0xcc,0xca,0x6b,0x12,0x99};
    std::valarray<uint8_t> data2 = {
        0x11,0xaf,0x23,0xff,0x00,0x73,0x54,0xcc,0xca,0x12,0x6b,0x99};

    YUVImage::Shared image1 = YUVImage::create(VA_FOURCC_NV12, 2, 4);
    YUVImage::Shared image2 = YUVImage::create(VA_FOURCC_I420, 2, 4);
    ASSERT_PTR(image1.get());
    ASSERT_PTR(image2.get());

    image1->bytes = data1;
    image2->y() = image1->y();
    image2->u() = image1->u();
    image2->v() = image1->v();

    EXPECT_TRUE( (image2->bytes == data2).min() );
}

TEST(YUVImageTest, UYVYtoYUY2)
{
    std::valarray<uint8_t> data1 = {0x11,0xaf,0x23,0xff,0x00,0x73,0x54,0xcc};
    std::valarray<uint8_t> data2 = {0xaf,0x11,0xff,0x23,0x73,0x00,0xcc,0x54};

    YUVImage::Shared image1 = YUVImage::create(VA_FOURCC_UYVY, 2, 2);
    YUVImage::Shared image2 = YUVImage::create(VA_FOURCC_YUY2, 2, 2);
    ASSERT_PTR(image1.get());
    ASSERT_PTR(image2.get());

    image1->bytes = data1;
    image2->y() = image1->y();
    image2->u() = image1->u();
    image2->v() = image1->v();

    EXPECT_TRUE( (image2->bytes == data2).min() );
}

TEST(YUVImageTest, YUY2toUYVY)
{
    std::valarray<uint8_t> data1 = {0x11,0xaf,0x23,0xff,0x00,0x73,0x54,0xcc};
    std::valarray<uint8_t> data2 = {0xaf,0x11,0xff,0x23,0x73,0x00,0xcc,0x54};

    YUVImage::Shared image1 = YUVImage::create(VA_FOURCC_YUY2, 2, 2);
    YUVImage::Shared image2 = YUVImage::create(VA_FOURCC_UYVY, 2, 2);
    ASSERT_PTR(image1.get());
    ASSERT_PTR(image2.get());

    image1->bytes = data1;
    image2->y() = image1->y();
    image2->u() = image1->u();
    image2->v() = image1->v();

    EXPECT_TRUE( (image2->bytes == data2).min() );
}

TEST(YUVImageTest, UYVYto422H)
{
    std::valarray<uint8_t> data1 = {0x11,0xaf,0x23,0xff,0x00,0x73,0x54,0xcc};
    std::valarray<uint8_t> data2 = {0xaf,0xff,0x73,0xcc,0x11,0x00,0x23,0x54};

    YUVImage::Shared image1 = YUVImage::create(VA_FOURCC_UYVY, 2, 2);
    YUVImage::Shared image2 = YUVImage::create(VA_FOURCC_422H, 2, 2);
    ASSERT_PTR(image1.get());
    ASSERT_PTR(image2.get());

    image1->bytes = data1;
    image2->y() = image1->y();
    image2->u() = image1->u();
    image2->v() = image1->v();

    EXPECT_TRUE( (image2->bytes == data2).min() );
}

TEST(YUVImageTest, 422HtoUYVY)
{
    std::valarray<uint8_t> data1 = {0xaf,0xff,0x73,0xcc,0x11,0x00,0x23,0x54};
    std::valarray<uint8_t> data2 = {0x11,0xaf,0x23,0xff,0x00,0x73,0x54,0xcc};

    YUVImage::Shared image1 = YUVImage::create(VA_FOURCC_422H, 2, 2);
    YUVImage::Shared image2 = YUVImage::create(VA_FOURCC_UYVY, 2, 2);
    ASSERT_PTR(image1.get());
    ASSERT_PTR(image2.get());

    image1->bytes = data1;
    image2->y() = image1->y();
    image2->u() = image1->u();
    image2->v() = image1->v();

    EXPECT_TRUE( (image2->bytes == data2).min() );
}

TEST(YUVImageTest, YUY2to422H)
{
    std::valarray<uint8_t> data1 = {0xaf,0xff,0x73,0xcc,0x11,0x00,0x23,0x54};
    std::valarray<uint8_t> data2 = {0xaf,0x73,0x11,0x23,0xff,0x00,0xcc,0x54};

    YUVImage::Shared image1 = YUVImage::create(VA_FOURCC_YUY2, 2, 2);
    YUVImage::Shared image2 = YUVImage::create(VA_FOURCC_422H, 2, 2);
    ASSERT_PTR(image1.get());
    ASSERT_PTR(image2.get());

    image1->bytes = data1;
    image2->y() = image1->y();
    image2->u() = image1->u();
    image2->v() = image1->v();

    EXPECT_TRUE( (image2->bytes == data2).min() );
}

TEST(YUVImageTest, 422HtoYUY2)
{
    std::valarray<uint8_t> data1 = {0xaf,0x73,0x11,0x23,0xff,0x00,0xcc,0x54};
    std::valarray<uint8_t> data2 = {0xaf,0xff,0x73,0xcc,0x11,0x00,0x23,0x54};

    YUVImage::Shared image1 = YUVImage::create(VA_FOURCC_422H, 2, 2);
    YUVImage::Shared image2 = YUVImage::create(VA_FOURCC_YUY2, 2, 2);
    ASSERT_PTR(image1.get());
    ASSERT_PTR(image2.get());

    image1->bytes = data1;
    image2->y() = image1->y();
    image2->u() = image1->u();
    image2->v() = image1->v();

    EXPECT_TRUE( (image2->bytes == data2).min() );
}
