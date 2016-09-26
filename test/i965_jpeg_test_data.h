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

#include <array>
#include <iostream>
#include <map>
#include <memory>
#include <va/va.h>
#include <vector>

namespace JPEG {

    typedef VAIQMatrixBufferJPEGBaseline                IQMatrix;
    typedef VAHuffmanTableBufferJPEGBaseline            HuffmanTable;
    typedef VAPictureParameterBufferJPEGBaseline        PictureParameter;
    typedef VASliceParameterBufferJPEGBaseline          SliceParameter;

    static const VAProfile profile = VAProfileJPEGBaseline;

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
        picture_width:                          10,
        picture_height:                         10,
        components: {
            {
                component_id:                   1,
                h_sampling_factor:              1,
                v_sampling_factor:              1,
                quantiser_table_selector:       0,
            },
            {
                component_id:                   2,
                h_sampling_factor:              1,
                v_sampling_factor:              1,
                quantiser_table_selector:       1,
            },
            {
                component_id:                   3,
                h_sampling_factor:              1,
                v_sampling_factor:              1,
                quantiser_table_selector:       1,
            },
        },
        num_components:                         3,
    };

    static const SliceParameter defaultSliceParameter = {
        slice_data_size:                        0,
        slice_data_offset:                      0,
        slice_data_flag:                        VA_SLICE_DATA_FLAG_ALL,
        slice_horizontal_position:              0,
        slice_vertical_position:                0,

        components: {
            {
                component_selector:             1,
                dc_table_selector:              0,
                ac_table_selector:              0,
            },
            {
                component_selector:             2,
                dc_table_selector:              1,
                ac_table_selector:              1,
            },
            {
                component_selector:             3,
                dc_table_selector:              1,
                ac_table_selector:              1,
            },
        },
        num_components:                         3,
        restart_interval:                       0,
        num_mcus:                               4,
    };

    typedef std::vector<uint8_t> ByteData;

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
            pd->pparam.picture_width = W;
            pd->pparam.picture_height = H;

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
            default:
                break;
            }

            /* Calculate num_mcus */
            int hfactor = pd->pparam.components[0].h_sampling_factor << 3;
            int vfactor = pd->pparam.components[0].v_sampling_factor << 3;
            int wmcu = (W + hfactor - 1) / hfactor;
            int hmcu = (H + vfactor - 1) / vfactor;
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

#endif
