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

#include "i965_avce_test_common.h"
#include "i965_streamable.h"
#include "i965_test_fixture.h"

#include <map>
#include <tuple>
#include <vector>

namespace AVC {
namespace Encode {

class AVCEContextTest
    : public I965TestFixture
    , public ::testing::WithParamInterface<
        std::tuple<VAProfile, VAEntrypoint> >
{
protected:
    void SetUp()
    {
        I965TestFixture::SetUp();
        std::tie(profile, entrypoint) = GetParam();
    }

    void TearDown()
    {
        if (context != VA_INVALID_ID)
            destroyContext(context);
        if (config != VA_INVALID_ID)
            destroyConfig(config);
        I965TestFixture::TearDown();
    }

    operator struct intel_encoder_context const *()
    {
        if (config == VA_INVALID_ID) return NULL;

        struct i965_driver_data *i965(*this);
        if (not i965) return NULL;

        if (IS_GEN9(i965->intel.device_info))
            is_gen9 = true;

        struct object_context const *obj_context = CONTEXT(context);
        if (not obj_context) return NULL;

        return reinterpret_cast<struct intel_encoder_context const *>(
            obj_context->hw_context);
    }

    VAProfile       profile;
    VAEntrypoint    entrypoint;
    VAConfigID      config = VA_INVALID_ID;
    VAContextID     context = VA_INVALID_ID;
    bool            is_gen9 = false;
};

TEST_P(AVCEContextTest, RateControl)
{
    if (not IsSupported(profile, entrypoint)) {
        RecordProperty("skipped", true);
        std::cout << "[  SKIPPED ] " << getFullTestName()
            << " is unsupported on this hardware" << std::endl;
        return;
    }

    static const std::vector<unsigned> rateControls = {
        VA_RC_NONE, VA_RC_CBR, VA_RC_VBR, VA_RC_VCM, VA_RC_CQP,
        VA_RC_VBR_CONSTRAINED, VA_RC_MB,
    };

    struct i965_driver_data *i965(*this);
    ASSERT_PTR(i965);
    const unsigned supportedBRC = (entrypoint == VAEntrypointEncSlice) ?
        i965->codec_info->h264_brc_mode : i965->codec_info->lp_h264_brc_mode;

    for (auto rc : rateControls) {
        ConfigAttribs attribs(1, {type:VAConfigAttribRateControl, value:rc});

        const VAStatus expect =
            ((rc & supportedBRC) ||
                profile == VAProfileH264MultiviewHigh ||
                profile == VAProfileH264StereoHigh) ?
            VA_STATUS_SUCCESS : VA_STATUS_ERROR_INVALID_CONFIG;

        config = createConfig(profile, entrypoint, attribs, expect);
        if (expect != VA_STATUS_SUCCESS) continue;

        context = createContext(config, 1, 1);
        if (HasFailure()) continue;

        struct intel_encoder_context const *hw_context(*this);
        EXPECT_PTR(hw_context);
        if (HasFailure()) continue;

        EXPECT_EQ(rc, hw_context->rate_control_mode);

        destroyContext(context);
        destroyConfig(config);
        context = VA_INVALID_ID;
        config = VA_INVALID_ID;
    }
}

TEST_P(AVCEContextTest, Codec)
{
    if (not IsSupported(profile, entrypoint)) {
        RecordProperty("skipped", true);
        std::cout << "[  SKIPPED ] " << getFullTestName()
            << " is unsupported on this hardware" << std::endl;
        return;
    }

    static const std::map<VAProfile, int> codecs = {
        {VAProfileH264ConstrainedBaseline, CODEC_H264},
        {VAProfileH264Main, CODEC_H264},
        {VAProfileH264High, CODEC_H264},
        {VAProfileH264MultiviewHigh, CODEC_H264_MVC},
        {VAProfileH264StereoHigh, CODEC_H264_MVC},
    };

    ASSERT_NO_FAILURE(
        config = createConfig(profile, entrypoint);
        context = createContext(config, 1, 1);
    );

    struct intel_encoder_context const *hw_context(*this);
    ASSERT_PTR(hw_context);

    EXPECT_EQ(codecs.at(profile), hw_context->codec);
}

TEST_P(AVCEContextTest, LowPowerMode)
{
    if (not IsSupported(profile, entrypoint)) {
        RecordProperty("skipped", true);
        std::cout << "[  SKIPPED ] " << getFullTestName()
            << " is unsupported on this hardware" << std::endl;
        return;
    }

    ASSERT_NO_FAILURE(
        config = createConfig(profile, entrypoint);
        context = createContext(config, 1, 1);
    );

    struct intel_encoder_context const *hw_context(*this);
    ASSERT_PTR(hw_context);

    EXPECT_EQ(
        (entrypoint == VAEntrypointEncSliceLP ? 1u : 0u),
        hw_context->low_power_mode
    );
}

TEST_P(AVCEContextTest, ROINotSpecified)
{
    if (not IsSupported(profile, entrypoint)) {
        RecordProperty("skipped", true);
        std::cout << "[  SKIPPED ] " << getFullTestName()
            << " is unsupported on this hardware" << std::endl;
        return;
    }

    // The lack of the VAConfigAttribEncROI config attribute
    // will disable it.
    ASSERT_NO_FAILURE(
        config = createConfig(profile, entrypoint);
        context = createContext(config, 1, 1);
    );

    struct intel_encoder_context const *hw_context(*this);
    ASSERT_PTR(hw_context);

    EXPECT_EQ(0u, hw_context->context_roi);
}

TEST_P(AVCEContextTest, ROISpecified)
{
    if (not IsSupported(profile, entrypoint)) {
        RecordProperty("skipped", true);
        std::cout << "[  SKIPPED ] " << getFullTestName()
            << " is unsupported on this hardware" << std::endl;
        return;
    }

    static const std::map<VAProfile, unsigned> roiSupport = {
        {VAProfileH264ConstrainedBaseline, 1}, {VAProfileH264Main, 1},
        {VAProfileH264High, 1}, {VAProfileH264MultiviewHigh, 0},
        {VAProfileH264StereoHigh, 0},
    };

    // The presence of the VAConfigAttribEncROI config attribute
    // will enable it for supported profile
    ConfigAttribs attribs(1, {type:VAConfigAttribEncROI});
    ASSERT_NO_FAILURE(
        config = createConfig(profile, entrypoint, attribs);
        context = createContext(config, 1, 1);
    );

    struct intel_encoder_context const *hw_context(*this);
    ASSERT_PTR(hw_context);

    EXPECT_EQ(roiSupport.at(profile), hw_context->context_roi);
}

TEST_P(AVCEContextTest, QualityRange)
{
    if (not IsSupported(profile, entrypoint)) {
        RecordProperty("skipped", true);
        std::cout << "[  SKIPPED ] " << getFullTestName()
            << " is unsupported on this hardware" << std::endl;
        return;
    }

    ASSERT_NO_FAILURE(
        config = createConfig(profile, entrypoint);
        context = createContext(config, 1, 1);
    );

    struct intel_encoder_context const *hw_context(*this);
    ASSERT_PTR(hw_context);

    std::map<VAProfile, unsigned> qranges;
    if(is_gen9) {
        qranges = {
            {VAProfileH264ConstrainedBaseline, entrypoint == VAEntrypointEncSliceLP
                ? ENCODER_LP_QUALITY_RANGE : ENCODER_QUALITY_RANGE_AVC},
            {VAProfileH264Main, entrypoint == VAEntrypointEncSliceLP
                ? ENCODER_LP_QUALITY_RANGE : ENCODER_QUALITY_RANGE_AVC},
            {VAProfileH264High, entrypoint == VAEntrypointEncSliceLP
                ? ENCODER_LP_QUALITY_RANGE : ENCODER_QUALITY_RANGE_AVC},
            {VAProfileH264MultiviewHigh, ENCODER_QUALITY_RANGE_AVC},
            {VAProfileH264StereoHigh, ENCODER_QUALITY_RANGE_AVC},
        };
    }else {
        qranges = {
            {VAProfileH264ConstrainedBaseline, entrypoint == VAEntrypointEncSliceLP
                ? ENCODER_LP_QUALITY_RANGE : ENCODER_QUALITY_RANGE},
            {VAProfileH264Main, entrypoint == VAEntrypointEncSliceLP
                ? ENCODER_LP_QUALITY_RANGE : ENCODER_QUALITY_RANGE},
            {VAProfileH264High, entrypoint == VAEntrypointEncSliceLP
                ? ENCODER_LP_QUALITY_RANGE : ENCODER_QUALITY_RANGE},
            {VAProfileH264MultiviewHigh, 1u},
            {VAProfileH264StereoHigh, 1u},
        };
    }

    EXPECT_EQ(qranges.at(profile), hw_context->quality_range);
}

INSTANTIATE_TEST_CASE_P(
    AVCEncode, AVCEContextTest, ::testing::Values(
        std::make_tuple(VAProfileH264ConstrainedBaseline, VAEntrypointEncSlice),
        std::make_tuple(VAProfileH264ConstrainedBaseline, VAEntrypointEncSliceLP),
        std::make_tuple(VAProfileH264Main, VAEntrypointEncSlice),
        std::make_tuple(VAProfileH264Main, VAEntrypointEncSliceLP),
        std::make_tuple(VAProfileH264High, VAEntrypointEncSlice),
        std::make_tuple(VAProfileH264High, VAEntrypointEncSliceLP),
        std::make_tuple(VAProfileH264MultiviewHigh, VAEntrypointEncSlice),
        std::make_tuple(VAProfileH264StereoHigh, VAEntrypointEncSlice)
    )
);

} // namespace Encode
} // namespace AVC
