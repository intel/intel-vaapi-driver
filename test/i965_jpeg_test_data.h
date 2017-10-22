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

#ifndef I965_JPEG_TEST_DATA_H
#define I965_JPEG_TEST_DATA_H

#include "i965_test_image_utils.h"

#include <array>
#include <iostream>
#include <map>
#include <memory>
#include <va/va.h>
#include <vector>

namespace JPEG {
    typedef std::vector<uint8_t> ByteData;

    static const VAProfile profile = VAProfileJPEGBaseline;

    static inline const ByteData generateSolid(
        const std::array<uint8_t, 3>& yuv, const std::array<size_t, 2>& dim)
    {
        size_t count(dim[0] * dim[1]);
        ByteData data(count, yuv[0]);
        data.insert(data.end(), count, yuv[1]);
        data.insert(data.end(), count, yuv[2]);
        return data;
    }
} // namespace JPEG

namespace JPEG {
namespace Decode {
    typedef VAIQMatrixBufferJPEGBaseline                IQMatrix;
    typedef VAHuffmanTableBufferJPEGBaseline            HuffmanTable;
    typedef VAPictureParameterBufferJPEGBaseline        PictureParameter;
    typedef VASliceParameterBufferJPEGBaseline          SliceParameter;

    static const VAEntrypoint entrypoint = VAEntrypointVLD;

    static const HuffmanTable defaultHuffmanTable = {
        load_huffman_table: { 0x01, 0x01 },
        huffman_table: {
            { // luminance
                num_dc_codes: {
                    0x00,0x01,0x05,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,
                    0x00,0x00,0x00,0x00
                },
                dc_values:    {
                    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b
                },
                num_ac_codes: {
                    0x00,0x02,0x01,0x03,0x03,0x02,0x04,0x03,0x05,0x05,0x04,0x04,
                    0x00,0x00,0x01,0x7d
                },
                ac_values:    {
                    0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,
                    0x13,0x51,0x61,0x07,0x22,0x71,0x14,0x32,0x81,0x91,0xa1,0x08,
                    0x23,0x42,0xb1,0xc1,0x15,0x52,0xd1,0xf0,0x24,0x33,0x62,0x72,
                    0x82,0x09,0x0a,0x16,0x17,0x18,0x19,0x1a,0x25,0x26,0x27,0x28,
                    0x29,0x2a,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x43,0x44,0x45,
                    0x46,0x47,0x48,0x49,0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,
                    0x5a,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x73,0x74,0x75,
                    0x76,0x77,0x78,0x79,0x7a,0x83,0x84,0x85,0x86,0x87,0x88,0x89,
                    0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,
                    0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,
                    0xb7,0xb8,0xb9,0xba,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,
                    0xca,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xe1,0xe2,
                    0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xf1,0xf2,0xf3,0xf4,
                    0xf5,0xf6,0xf7,0xf8,0xf9,0xfa
                },
                pad: { 0x00, 0x00 }
            },
            { // chrominance
                num_dc_codes: {
                    0x00,0x03,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x00,
                    0x00,0x00,0x00,0x00
                },
                dc_values:    {
                    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b
                },
                num_ac_codes: {
                    0x00,0x02,0x01,0x02,0x04,0x04,0x03,0x04,0x07,0x05,0x04,0x04,
                    0x00,0x01,0x02,0x77
                },
                ac_values:    {
                    0x00,0x01,0x02,0x03,0x11,0x04,0x05,0x21,0x31,0x06,0x12,0x41,
                    0x51,0x07,0x61,0x71,0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91,
                    0xa1,0xb1,0xc1,0x09,0x23,0x33,0x52,0xf0,0x15,0x62,0x72,0xd1,
                    0x0a,0x16,0x24,0x34,0xe1,0x25,0xf1,0x17,0x18,0x19,0x1a,0x26,
                    0x27,0x28,0x29,0x2a,0x35,0x36,0x37,0x38,0x39,0x3a,0x43,0x44,
                    0x45,0x46,0x47,0x48,0x49,0x4a,0x53,0x54,0x55,0x56,0x57,0x58,
                    0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x73,0x74,
                    0x75,0x76,0x77,0x78,0x79,0x7a,0x82,0x83,0x84,0x85,0x86,0x87,
                    0x88,0x89,0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,
                    0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,
                    0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,
                    0xc8,0xc9,0xca,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,
                    0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xf2,0xf3,0xf4,
                    0xf5,0xf6,0xf7,0xf8,0xf9,0xfa,
                },
                pad: { 0x00, 0x00 }
            }
        }
    };

    static const IQMatrix defaultIQMatrix = { /* Quality 100 */
        load_quantiser_table: { 0x1,0x1,0x0,0x0 },
        quantiser_table: {
            {
                0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
                0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
                0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
                0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
                0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
                0x01,0x01,0x01,0x01,
            },
            {
                0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
                0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
                0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
                0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
                0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
                0x01,0x01,0x01,0x01,
            },
        },
    };

    static const PictureParameter defaultPictureParameter = {
        picture_width:  10,
        picture_height: 10,
        /* component_id, h_sampling_factor, v_sampling_factor, quantiser_table_selector */
        components:     {{1,1,1,0}, {2,1,1,1}, {3,1,1,1}},
        num_components: 3,
    };

    static const SliceParameter defaultSliceParameter = {
        slice_data_size:                        0,
        slice_data_offset:                      0,
        slice_data_flag:                        VA_SLICE_DATA_FLAG_ALL,
        slice_horizontal_position:              0,
        slice_vertical_position:                0,

        /* component_selector, dc_table_selector, ac_table_selector */
        components: {{1,0,0},{2,1,1},{3,1,1}},

        num_components:                         3,
        restart_interval:                       0,
        num_mcus:                               4,
    };

    class PictureData
    {
    public:
        typedef std::shared_ptr<PictureData> Shared;
        typedef std::shared_ptr<const PictureData> SharedConst;

        template<const unsigned W, const unsigned H>
        static SharedConst make(
            const unsigned fourcc,
            const ByteData& slice,
            const SliceParameter& sparam = defaultSliceParameter,
            const PictureParameter& pparam = defaultPictureParameter,
            const HuffmanTable& huffman = defaultHuffmanTable,
            const IQMatrix& iqmatrix = defaultIQMatrix)
        {
            return make(fourcc, slice, W, H, sparam, pparam, huffman, iqmatrix);
        }

        static SharedConst make(
            const unsigned fourcc,
            const ByteData& slice,
            const unsigned w, const unsigned h,
            const SliceParameter& sparam = defaultSliceParameter,
            const PictureParameter& pparam = defaultPictureParameter,
            const HuffmanTable& huffman = defaultHuffmanTable,
            const IQMatrix& iqmatrix = defaultIQMatrix)
        {
            Shared pd(
                new PictureData {
                    slice: slice,
                    sparam: sparam,
                    pparam: pparam,
                    huffman: huffman,
                    iqmatrix: iqmatrix,
                    format: 0,
                    fourcc: fourcc,
                }
            );

            pd->sparam.slice_data_size = slice.size();
            pd->pparam.picture_width = w;
            pd->pparam.picture_height = h;

            switch(fourcc)
            {
            case VA_FOURCC_IMC3:
                pd->format = VA_RT_FORMAT_YUV420;
                pd->pparam.components[0].h_sampling_factor = 2;
                pd->pparam.components[0].v_sampling_factor = 2;
                break;
            case VA_FOURCC_422H:
                pd->format = VA_RT_FORMAT_YUV422;
                pd->pparam.components[0].h_sampling_factor = 2;
                pd->pparam.components[0].v_sampling_factor = 1;
                break;
            case VA_FOURCC_422V:
                pd->format = VA_RT_FORMAT_YUV422;
                pd->pparam.components[0].h_sampling_factor = 1;
                pd->pparam.components[0].v_sampling_factor = 2;
                break;
            case VA_FOURCC_411P:
                pd->format = VA_RT_FORMAT_YUV411;
                pd->pparam.components[0].h_sampling_factor = 4;
                pd->pparam.components[0].v_sampling_factor = 1;
                break;
            case VA_FOURCC_444P:
                pd->format = VA_RT_FORMAT_YUV444;
                pd->pparam.components[0].h_sampling_factor = 1;
                pd->pparam.components[0].v_sampling_factor = 1;
                break;
            case VA_FOURCC_Y800:
                pd->format = VA_RT_FORMAT_YUV400;
                pd->pparam.components[0].h_sampling_factor = 1;
                pd->pparam.components[0].h_sampling_factor = 1;
                pd->pparam.num_components = 1;
                pd->sparam.num_components = 1;
                break;
            default:
                break;
            }

            /* Calculate num_mcus */
            int hfactor = pd->pparam.components[0].h_sampling_factor << 3;
            int vfactor = pd->pparam.components[0].v_sampling_factor << 3;
            int wmcu = (w + hfactor - 1) / hfactor;
            int hmcu = (h + vfactor - 1) / vfactor;
            pd->sparam.num_mcus = wmcu * hmcu;

            return pd;
        }

        const ByteData          slice;
        SliceParameter          sparam;
        PictureParameter        pparam;
        HuffmanTable            huffman;
        IQMatrix                iqmatrix;
        unsigned                format;
        unsigned                fourcc;
    };

    class TestPattern
    {
    public:
        typedef std::shared_ptr<TestPattern> Shared;
        typedef std::shared_ptr<const TestPattern> SharedConst;

        virtual const ByteData& decoded() const = 0;
        virtual PictureData::SharedConst encoded(unsigned) const = 0;

        virtual void repr(std::ostream&) const = 0;

        friend std::ostream& operator <<(std::ostream& os, const TestPattern& t)
        {
            t.repr(os);
            return os;
        }
    };

    template <const unsigned N>
    class TestPatternData
        : public TestPattern
    {
    private:
        typedef std::map<const unsigned, PictureData::SharedConst> EncodedMap;
        typedef std::map<const unsigned, const ByteData> ByteDataMap;

    public:
        const ByteData& decoded() const { return getDecoded(); }

        PictureData::SharedConst encoded(const unsigned fourcc) const
        {
            const EncodedMap& em = getEncodedMap();
            const EncodedMap::const_iterator match(em.find(fourcc));
            if (match == em.end())
                return PictureData::SharedConst();
            return match->second;
        }

        void repr(std::ostream& os) const { os << N; }

        template <const unsigned W, const unsigned H>
        static bool initialize(ByteData d, const ByteDataMap& e)
        {
            getDecoded().swap(d);

            EncodedMap& em = getEncodedMap();
            bool result = true;
            for (auto const &b : e) {
                auto pd(PictureData::make<W, H>(b.first, b.second));
                auto pair = std::make_pair(b.first, pd);
                result &= em.insert(pair).second;
            }
            return result;
        }

    private:
        static ByteData& getDecoded()
        {
            static ByteData d;
            return d;
        }

        static EncodedMap& getEncodedMap()
        {
            static EncodedMap em;
            return em;
        }

        static const bool m_valid;
    };
} // namespace Decode
} // namespace JPEG

namespace JPEG {
namespace Encode {
    typedef VAQMatrixBufferJPEG                 IQMatrix;
    typedef VAHuffmanTableBufferJPEGBaseline    HuffmanTable;
    typedef VAEncPictureParameterBufferJPEG     PictureParameter;
    typedef VAEncSliceParameterBufferJPEG       SliceParameter;

    static const VAEntrypoint entrypoint = VAEntrypointEncPicture;

    static const IQMatrix defaultIQMatrix = { /* Quality 50 */
        load_lum_quantiser_matrix: 1,
        load_chroma_quantiser_matrix: 1,
        lum_quantiser_matrix: {
            0x10,0x0b,0x0c,0x0e,0x0c,0x0a,0x10,0x0e,
            0x0d,0x0e,0x12,0x11,0x10,0x13,0x18,0x28,
            0x1a,0x18,0x16,0x16,0x18,0x31,0x23,0x25,
            0x1d,0x28,0x3a,0x33,0x3d,0x3c,0x39,0x33,
            0x38,0x37,0x40,0x48,0x5c,0x4e,0x40,0x44,
            0x57,0x45,0x37,0x38,0x50,0x6d,0x51,0x57,
            0x5f,0x62,0x67,0x68,0x67,0x3e,0x4d,0x71,
            0x79,0x70,0x64,0x78,0x5c,0x65,0x67,0x63,
        },
        chroma_quantiser_matrix: {
            0x11,0x12,0x12,0x18,0x15,0x18,0x2f,0x1a,
            0x1a,0x2f,0x63,0x42,0x38,0x42,0x63,0x63,
            0x63,0x63,0x63,0x63,0x63,0x63,0x63,0x63,
            0x63,0x63,0x63,0x63,0x63,0x63,0x63,0x63,
            0x63,0x63,0x63,0x63,0x63,0x63,0x63,0x63,
            0x63,0x63,0x63,0x63,0x63,0x63,0x63,0x63,
            0x63,0x63,0x63,0x63,0x63,0x63,0x63,0x63,
            0x63,0x63,0x63,0x63,0x63,0x63,0x63,0x63,
        },
    };

    static const HuffmanTable defaultHuffmanTable =
        ::JPEG::Decode::defaultHuffmanTable;

    static const PictureParameter defaultPictureParameter = {
        reconstructed_picture:      VA_INVALID_ID,
        picture_width:              10,
        picture_height:             10,
        coded_buf:                  VA_INVALID_ID,
        pic_flags:                  {value: 0x00100},
        sample_bit_depth:           8,
        num_scan:                   1,
        num_components:             3,
        component_id:               {0, 1, 2, 0},
        quantiser_table_selector:   {0, 1, 1, 0},
        quality:                    100,
    };

    static const SliceParameter defaultSliceParameter = {
        restart_interval:   0,
        num_components:     3,
        /* component_selector, dc_table_selector, ac_table_selector */
        components:         {{1,0,0},{2,1,1},{3,1,1}},
    };

    class TestInput
        : public std::enable_shared_from_this<TestInput>
    {
    public:
        typedef std::shared_ptr<TestInput> Shared;
        typedef std::shared_ptr<const TestInput> SharedConst;

        static Shared create(const unsigned, const unsigned, const unsigned);
        const YUVImage::SharedConst toExpectedOutput() const;

        friend ::std::ostream& operator<<(::std::ostream&, const TestInput&);
        friend ::std::ostream& operator<<(::std::ostream&, const Shared&);
        friend ::std::ostream& operator<<(::std::ostream&, const SharedConst&);

        YUVImage::Shared    image;
        PictureParameter    picture;
        IQMatrix            matrix;
        HuffmanTable        huffman;
        SliceParameter      slice;

    private:
        TestInput();
    };

    class TestInputCreator
    {
    public:
        typedef std::shared_ptr<TestInputCreator> Shared;
        typedef std::shared_ptr<const TestInputCreator> SharedConst;

        TestInput::Shared create(const unsigned) const;

        friend ::std::ostream& operator<<(
            ::std::ostream&, const TestInputCreator&);
        friend ::std::ostream& operator<<(
            ::std::ostream&, const TestInputCreator::Shared&);
        friend ::std::ostream& operator<<(
            ::std::ostream&, const TestInputCreator::SharedConst&);
        virtual std::array<unsigned, 2> getResolution() const = 0;

    protected:
        virtual void repr(::std::ostream& os) const = 0;
    };

    class RandomSizeCreator
        : public TestInputCreator
    {
    public:
        std::array<unsigned, 2> getResolution() const;

    protected:
        void repr(::std::ostream&) const;
    };

    class FixedSizeCreator
        : public TestInputCreator
    {
    public:
        FixedSizeCreator(const std::array<unsigned, 2>&);
        std::array<unsigned, 2> getResolution() const;

    protected:
        void repr(::std::ostream& os) const;

    private:
        const std::array<unsigned, 2> res;
    };

    typedef std::vector<TestInputCreator::SharedConst> InputCreators;

} // namespace Encode
} // namespace JPEG

#endif
