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

#ifndef I965_STREAMABLE_H
#define I965_STREAMABLE_H

#include <array>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <valarray>
#include <va/va.h>

namespace std {
    template <typename T, const size_t S> inline std::ostream&
    operator<<(std::ostream& os, const std::array<T, S>& a)
    {
        os << "{";
        for (const auto& s : a) {
            if (&s != &a[0])
                os << ",";
            os << s;
        }
        return os << "}";
    }

    template <const size_t S> inline std::ostream&
    operator<<(std::ostream& os, const std::array<uint8_t, S>& a)
    {
        os << "{" << std::hex;
        for (const auto& s : a) {
            if (&s != &a[0])
                os << ",";
            os << "0x" << std::setfill('0') << std::setw(2) << unsigned(s);
        }
        return os << std::dec << "}";
    }

    template <typename T> inline std::ostream&
    operator<<(std::ostream& os, const std::valarray<T>& a)
    {
        os << "{";
        for (const auto& s : a) {
            if (&s != &a[0])
                os << ",";
            os << s;
        }
        return os << "}";
    }

    template <> inline std::ostream&
    operator<<(std::ostream& os, const std::valarray<uint8_t>& a)
    {
        os << "{" << std::hex;
        for (const auto& s : a) {
            if (&s != &a[0])
                os << ",";
            os << "0x" << std::setfill('0') << std::setw(2) << unsigned(s);
        }
        return os << std::dec << "}";
    }
}// namespace std

template <typename T>
const std::string toString(const T& t)
{
    std::ostringstream os;
    os << t;
    return os.str();
}

inline std::ostream&
operator<<(std::ostream& os, const VAEncPictureParameterBufferJPEG& b)
{
    os  << "VAEncPictureParameterBufferJPEG (" << &b << ")" << std::endl
        << "  .reconstructed_picture    = " << b.reconstructed_picture
        << std::endl

        << "  .picture_width            = " << b.picture_width
        << std::endl

        << "  .picture_height           = " << b.picture_height
        << std::endl

        << "  .coded_buf                = " << b.coded_buf
        << std::endl

        << "  .pic_flags.value          = "
        << std::hex << std::setw(8) << std::setfill('0') << b.pic_flags.value
        << std::dec
        << std::endl

        << "  ...profile                = " << b.pic_flags.bits.profile
        << " (0 - Baseline, 1 - Extended, 2 - Lossless, 3 - Hierarchical)"
        << std::endl

        << "  ...progressive            = " << b.pic_flags.bits.progressive
        << " (0 - sequential, 1 - extended, 2 - progressive)"
        << std::endl

        << "  ...huffman                = " << b.pic_flags.bits.huffman
        << " (0 - arithmetic, 1 - huffman)"
        << std::endl

        << "  ...interleaved            = " << b.pic_flags.bits.interleaved
        << " (0 - non interleaved, 1 - interleaved)"
        << std::endl

        << "  ...differential           = " << b.pic_flags.bits.differential
        << " (0 - non differential, 1 - differential)"
        << std::endl

        << "  .sample_bit_depth         = " << (unsigned)b.sample_bit_depth
        << std::endl

        << "  .num_scan                 = " << (unsigned)b.num_scan
        << std::endl

        << "  .num_components           = " << (unsigned)b.num_components
        << std::endl

        << "  .component_id             = "
        << (unsigned)b.component_id[0] << ","
        << (unsigned)b.component_id[1] << ","
        << (unsigned)b.component_id[2] << ","
        << (unsigned)b.component_id[3] << ","
        << std::endl

        << "  .quantiser_table_selector = "
        << (unsigned)b.quantiser_table_selector[0] << ","
        << (unsigned)b.quantiser_table_selector[1] << ","
        << (unsigned)b.quantiser_table_selector[2] << ","
        << (unsigned)b.quantiser_table_selector[3] << ","
        << std::endl

        << "  .quality                  = " << (unsigned)b.quality
        << std::endl
    ;
    return os;
}

inline std::ostream&
operator<<(std::ostream& os, const VAEncPictureParameterBufferJPEG* b)
{
    os << *b;
    return os;
}

inline std::ostream&
operator<<(std::ostream& os, const VAQMatrixBufferJPEG& b)
{
      os << "VAQMatrixBufferJPEG (" << &b << ")" << std::endl;

      os << "  .load_lum_quantiser_matrix     = " << b.load_lum_quantiser_matrix
          << std::endl
      ;

      if (b.load_lum_quantiser_matrix)
      {
          os << "  .lum_quantiser_matrix          = ";
          for (size_t i(0); i < sizeof(b.lum_quantiser_matrix); ++i) {
              os << std::hex << std::setw(2) << std::setfill('0')
                  << (unsigned)b.lum_quantiser_matrix[i] << ",";
              if (((i+1) % 12) == 0)
                  os << std::endl << "                                   ";
          }
          os << std::dec << std::endl;
      }

      os << "  .load_chroma_quantiser_matrix  = " << b.load_chroma_quantiser_matrix
          << std::endl
      ;

      if (b.load_lum_quantiser_matrix)
      {
          os << "  .chroma_quantiser_matrix       = ";
          for (size_t i(0); i < sizeof(b.chroma_quantiser_matrix); ++i) {
              os << std::hex << std::setw(2) << std::setfill('0')
                  << (unsigned)b.chroma_quantiser_matrix[i] << ",";
              if (((i+1) % 12) == 0)
                  os << std::endl << "                                   ";
          }
          os << std::dec << std::endl;
      }

      return os;
}

inline std::ostream&
operator<<(std::ostream& os, const VAQMatrixBufferJPEG* b)
{
    os << *b;
    return os;
}

inline std::ostream&
operator<<(std::ostream& os, const VAHuffmanTableBufferJPEGBaseline& b)
{
    os << "VAHuffmanTableBufferJPEGBaseline (" << &b << ")" << std::endl;

    os  << "  .load_huffman_table            = "
        << (unsigned)b.load_huffman_table[0] << ","
        << (unsigned)b.load_huffman_table[1] << ","
        << std::endl
    ;

    for (size_t i(0); i < 2; ++i) {
        unsigned sum(0);
        os << "  .huffman_table[" << i << "].num_dc_codes = ";
        for (size_t j(0); j < sizeof(b.huffman_table[i].num_dc_codes); ++j) {
            if (j and (j % 12) == 0)
                os << std::endl << "                                   ";
            os << std::hex << std::setfill('0') << std::setw(2)
                << (unsigned)b.huffman_table[i].num_dc_codes[j] << ","
                << std::dec
            ;
            sum += b.huffman_table[i].num_dc_codes[j];
        }
        os << " (sum = " << sum << ")" << std::endl;

        os << "  .huffman_table[" << i << "].dc_values    = ";
        for (size_t j(0); j < sizeof(b.huffman_table[i].dc_values); ++j) {
            if (j and (j % 12) == 0)
                os << std::endl << "                                   ";
            os << std::hex << std::setfill('0') << std::setw(2)
                << (unsigned)b.huffman_table[i].dc_values[j] << ","
                << std::dec
            ;
        }
        os << std::endl;

        sum = 0;
        os << "  .huffman_table[" << i << "].num_ac_codes = ";
        for (size_t j(0); j < sizeof(b.huffman_table[i].num_ac_codes); ++j) {
            if (j and (j % 12) == 0)
                os << std::endl << "                                   ";
            os << std::hex << std::setfill('0') << std::setw(2)
                << (unsigned)b.huffman_table[i].num_ac_codes[j] << ","
                << std::dec
            ;
            sum += b.huffman_table[i].num_ac_codes[j];
        }
        os << " (sum = " << sum << ")" << std::endl;

        os << "  .huffman_table[" << i << "].ac_values    = ";
        for (size_t j(0); j < sizeof(b.huffman_table[i].ac_values); ++j) {
            if (j and (j % 12) == 0)
                os << std::endl << "                                   ";
            os << std::hex << std::setfill('0') << std::setw(2)
                << (unsigned)b.huffman_table[i].ac_values[j] << ","
                << std::dec
            ;
        }
        os << std::endl;
    }
    return os;
}

inline std::ostream&
operator<<(std::ostream& os, const VAHuffmanTableBufferJPEGBaseline* b)
{
    os << *b;
    return os;
}

inline std::ostream&
operator<<(std::ostream& os, const VAEncSliceParameterBufferJPEG& b)
{
    os  << "VAEncSliceParameterBufferJPEG (" << &b << ")" << std::endl
        << "  .restart_interval     = " << b.restart_interval
        << std::endl

        << "  .num_components       = " << b.num_components
        << std::endl
    ;

    for (size_t i(0); i < 4; ++i) {
        os  << "  .components[" << i << "]" << std::endl
            << "    .component_selector = "
            << (unsigned)b.components[i].component_selector
            << std::endl

            << "    .dc_table_selector  = "
            << (unsigned)b.components[i].dc_table_selector
            << std::endl

            << "    .ac_table_selector  = "
            << (unsigned)b.components[i].ac_table_selector
            << std::endl
        ;
    }

    return os;
}

inline std::ostream&
operator<<(std::ostream& os, const VAEncSliceParameterBufferJPEG* b)
{
    os << *b;
    return os;
}

inline std::ostream&
operator<<(std::ostream& os, const VAEncPackedHeaderParameterBuffer& b)
{
    os  << "VAEncPackedHeaderParameterBuffer (" << &b << ")" << std::endl
        << "  .type                = " << b.type
        << std::endl
        << "  .bit_length          = " << b.bit_length
        << std::endl
        << "  .has_emulation_bytes = " << (unsigned)b.has_emulation_bytes
        << std::endl
    ;

    return os;
}

inline std::ostream&
operator<<(std::ostream& os, const VAEncPackedHeaderParameterBuffer* b)
{
    os << *b;
    return os;
}

inline std::ostream&
operator<<(std::ostream& os, const VAImage& image)
{
    os << "VAImage (" << &image << ")"
        << std::dec << std::endl
        << "  id       : " << image.image_id
        << std::endl
        << "  fourcc   : "
        << std::string(reinterpret_cast<const char*>(&image.format.fourcc), 4)
        << std::endl
        << "  size     : " << image.width << "x" << image.height
        << std::endl
        << "  planes   : " << image.num_planes
        << std::endl
        << "  offsets  : "
        << "{"
        << image.offsets[0] << ","
        << image.offsets[1] << ","
        << image.offsets[2]
        << "}"
        << std::endl
        << "  pitches  : "
        << "{"
        << image.pitches[0] << ","
        << image.pitches[1] << ","
        << image.pitches[2]
        << "}"
        << std::endl
        << "  bpp      : " << image.format.bits_per_pixel
        << std::endl
        << "  depth    : " << image.format.depth
        << std::endl
        << "  byteorder: " << image.format.byte_order
        << std::endl
        << "  rgba mask: "
        << "{"
        << image.format.red_mask << ","
        << image.format.green_mask << ","
        << image.format.blue_mask << ","
        << image.format.alpha_mask
        << "}"
        << std::endl
        << "  buffer id: " << image.buf
        << std::endl
        << "  data size: " << image.data_size
    ;
    return os;
}

inline std::ostream&
operator<<(std::ostream& os, const VAProfile& profile)
{
    switch(profile) {
    case VAProfileNone:
        return os << "VAProfileNone";
    case VAProfileMPEG2Simple:
        return os << "VAProfileMPEG2Simple";
    case VAProfileMPEG2Main:
        return os << "VAProfileMPEG2Main";
    case VAProfileMPEG4Simple:
        return os << "VAProfileMPEG4Simple";
    case VAProfileMPEG4AdvancedSimple:
        return os << "VAProfileMPEG4AdvancedSimple";
    case VAProfileMPEG4Main:
        return os << "VAProfileMPEG4Main";
    case VAProfileVC1Simple:
        return os << "VAProfileVC1Simple";
    case VAProfileVC1Main:
        return os << "VAProfileVC1Main";
    case VAProfileVC1Advanced:
        return os << "VAProfileVC1Advanced";
    case VAProfileH263Baseline:
        return os << "VAProfileH263Baseline";
    case VAProfileJPEGBaseline:
        return os << "VAProfileJPEGBaseline";
    case VAProfileVP8Version0_3:
        return os << "VAProfileVP8Version0_3";
    case VAProfileHEVCMain:
        return os << "VAProfileHEVCMain";
    case VAProfileHEVCMain10:
        return os << "VAProfileHEVCMain10";
    case VAProfileVP9Profile0:
        return os << "VAProfileVP9Profile0";
    case VAProfileVP9Profile1:
        return os << "VAProfileVP9Profile1";
    case VAProfileVP9Profile2:
        return os << "VAProfileVP9Profile2";
    case VAProfileVP9Profile3:
        return os << "VAProfileVP9Profile3";
    case VAProfileH264Baseline:
        return os << "VAProfileH264Baseline";
    case VAProfileH264ConstrainedBaseline:
        return os << "VAProfileH264ConstrainedBaseline";
    case VAProfileH264High:
        return os << "VAProfileH264High";
    case VAProfileH264Main:
        return os << "VAProfileH264Main";
    case VAProfileH264MultiviewHigh:
        return os << "VAProfileH264MultiviewHigh";
    case VAProfileH264StereoHigh:
        return os << "VAProfileH264StereoHigh";
    default:
        return os << "Unknown VAProfile: " << static_cast<int>(profile);
    }
}

inline std::ostream&
operator<<(std::ostream& os, const VAEntrypoint& entrypoint)
{
    switch(entrypoint) {
    case VAEntrypointVLD:
        return os << "VAEntrypointVLD";
    case VAEntrypointIZZ:
        return os << "VAEntrypointIZZ";
    case VAEntrypointIDCT:
        return os << "VAEntrypointIDCT";
    case VAEntrypointMoComp:
        return os << "VAEntrypointMoComp";
    case VAEntrypointDeblocking:
        return os << "VAEntrypointDeblocking";
    case VAEntrypointVideoProc:
        return os << "VAEntrypointVideoProc";
    case VAEntrypointEncSlice:
        return os << "VAEntrypointEncSlice";
    case VAEntrypointEncSliceLP:
        return os << "VAEntrypointEncSliceLP";
    case VAEntrypointEncPicture:
        return os << "VAEntrypointEncPicture";
    default:
        return os << "Unknown VAEntrypoint: " << static_cast<int>(entrypoint);
    }
}

#endif // I965_STREAMABLE_H
