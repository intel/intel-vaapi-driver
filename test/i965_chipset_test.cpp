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

#include "test.h"
#include "i965_internal_decl.h"

#include <iostream>
#include <set>
#include <string>
#include <vector>

struct Chipset {
    int devid;
    std::string family;
    std::string device;
    std::string description;

    bool operator< (const Chipset& other) const
    {
        return devid < other.devid;
    }

    bool operator== (const Chipset& other) const
    {
        return devid == other.devid;
    }
};

std::ostream& operator<<(std::ostream& os, const Chipset& chipset)
{
    os  << std::hex << "(0x" << chipset.devid << std::dec
        << ", " << chipset.family
        << ", " << chipset.device
        << ", " << chipset.description << ")";
    return os;
}

const std::vector<Chipset>& getChipsets()
{
#undef CHIPSET
#define CHIPSET(id, family, dev, desc) {id, #family, #dev, #desc},
    static std::vector<Chipset> chipsets = {
#include "i965_pciids.h"
    };
#undef CHIPSET
    return chipsets;
}

TEST(ChipsetTest, Unique)
{
    const std::vector<Chipset>& chipsets = getChipsets();
    std::set<Chipset> unique;
    for (const Chipset& chipset : chipsets)
    {
        const std::set<Chipset>::const_iterator match = unique.find(chipset);
        EXPECT_EQ(unique.end(), match)
            << "duplicate chipsets defined:" << std::endl
            << "\t" << chipset << std::endl
            << "\t" << *match;
        unique.insert(chipset);
    }
    EXPECT_EQ(unique.size(), chipsets.size());
}

TEST(ChipsetTest, GetCodecInfo)
{
    for (const Chipset& chipset : getChipsets())
    {
        hw_codec_info *info = i965_get_codec_info(chipset.devid);
        EXPECT_PTR(info)
            << "no codec info returned for " << chipset;
    }
}

TEST(ChipsetTest, GetDeviceInfo)
{
    for (const Chipset& chipset : getChipsets())
    {
        const intel_device_info *info = i965_get_device_info(chipset.devid);
        EXPECT_PTR(info)
            << "no device info returned for " << chipset;
    }
}
