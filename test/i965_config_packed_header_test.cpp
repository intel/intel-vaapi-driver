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

#include "i965_test_fixture.h"

class PackedHeaders
    : public I965TestFixture
{
};

TEST_F(PackedHeaders, ConfigCreate)
{
  VAProfile profiles[I965_MAX_PROFILES];
  VAEntrypoint entrypoints[I965_MAX_ENTRYPOINTS];
  int nb_profiles, nb_entrypoints;
  VAConfigID config;
  VAStatus vas;
  int i, j;

  vas = i965_QueryConfigProfiles(*this, profiles, &nb_profiles);
  ASSERT_STATUS(vas);

  for (i = 0; i < nb_profiles; i++) {
    vas = i965_QueryConfigEntrypoints(*this, profiles[i],
                                      entrypoints, &nb_entrypoints);
    ASSERT_STATUS(vas);

    for (j = 0; j < nb_entrypoints; j++) {
      VAConfigAttrib packed_headers = {
        .type = VAConfigAttribEncPackedHeaders
      };

      vas = i965_GetConfigAttributes(*this, profiles[i], entrypoints[j],
                                     &packed_headers, 1);
      ASSERT_STATUS(vas);

      uint32_t k;
      for (k = 0x00; k < 0xff; k++) {
        VAConfigAttrib attr = {
          .type  = VAConfigAttribEncPackedHeaders,
          .value = k,
        };

        vas = i965_CreateConfig(*this, profiles[i], entrypoints[j],
                                &attr, 1, &config);

        if (packed_headers.value == VA_ATTRIB_NOT_SUPPORTED) {
          // Not supported: creating a config with any packed headers
          // should always fail.
          EXPECT_STATUS_EQ(VA_STATUS_ERROR_INVALID_VALUE, vas);

        } else {
          // Some headers supported: any combination of headers in the
          // set should succeed, all other values should fail.
          if (k & ~packed_headers.value) {
            EXPECT_STATUS_EQ(VA_STATUS_ERROR_INVALID_VALUE, vas);
          } else {
            EXPECT_STATUS(vas);
            EXPECT_ID(config);
            vas = i965_DestroyConfig(*this, config);
            EXPECT_STATUS(vas);
          }
        }
      }
    }
  }
}
