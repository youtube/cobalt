// Copyright 2019 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/shared/starboard/media/codec_util.h"

#include <vector>

#include "starboard/shared/starboard/media/avc_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {
namespace {

const uint8_t kIdrStartCode = 0x65;
const auto kSpsStartCode = AvcParameterSets::kSpsStartCode;
const auto kPpsStartCode = AvcParameterSets::kPpsStartCode;
const auto kAnnexB = AvcParameterSets::kAnnexB;

const std::vector<uint8_t> kSpsInAnnexB = {0, 0, 0, 1, kSpsStartCode, 10, 11};
const std::vector<uint8_t> kPpsInAnnexB = {0, 0, 0, 1, kPpsStartCode, 20};
const std::vector<uint8_t> kIdrInAnnexB = {0, 0, 0, 1, kIdrStartCode,
                                           1, 2, 3, 4};

std::vector<uint8_t> operator+(const std::vector<uint8_t>& left,
                               const std::vector<uint8_t>& right) {
  std::vector<uint8_t> result(left);
  result.insert(result.end(), right.begin(), right.end());
  return result;
}

TEST(VideoConfigTest, CtorWithSbMediaVideoSampleInfo) {
  SbMediaVideoSampleInfo video_sample_info = {kSbMediaVideoCodecH264};
  video_sample_info.is_key_frame = true;
  video_sample_info.frame_width = 1920;
  video_sample_info.frame_height = 1080;

  std::vector<uint8_t> nalus_in_annex_b =
      kSpsInAnnexB + kPpsInAnnexB + kIdrInAnnexB;

  VideoConfig config_1(video_sample_info.codec, video_sample_info.frame_width,
                       video_sample_info.frame_height, nalus_in_annex_b.data(),
                       nalus_in_annex_b.size());
  VideoConfig config_2(video_sample_info, nalus_in_annex_b.data(),
                       nalus_in_annex_b.size());
  ASSERT_TRUE(config_1 == config_2);
}

TEST(VideoConfigTest, IsValid) {
  std::vector<uint8_t> nalus_in_annex_b =
      kSpsInAnnexB + kPpsInAnnexB + kIdrInAnnexB;

  {
    VideoConfig config(kSbMediaVideoCodecH264, 1920, 1080, kPpsInAnnexB.data(),
                       kPpsInAnnexB.size());
    ASSERT_TRUE(config.is_valid());
  }
  {
    VideoConfig config(kSbMediaVideoCodecH264, 1920, 1080, kIdrInAnnexB.data(),
                       kIdrInAnnexB.size());
    ASSERT_TRUE(config.is_valid());
  }
  {
    VideoConfig config(kSbMediaVideoCodecH264, 1920, 1080,
                       nalus_in_annex_b.data(), nalus_in_annex_b.size());
    ASSERT_TRUE(config.is_valid());
  }
  {
    // The implementation only fails when the format is avc and the input isn't
    // empty and doesn't start with a nalu header.
    VideoConfig config(kSbMediaVideoCodecH264, 1920, 1080,
                       nalus_in_annex_b.data() + 1,
                       nalus_in_annex_b.size() - 1);
    ASSERT_FALSE(config.is_valid());
  }
  {
    VideoConfig config(kSbMediaVideoCodecVp9, 1920, 1080, nullptr, 0);
    ASSERT_TRUE(config.is_valid());
  }
}

TEST(VideoConfigTest, SelfComparison) {
  {
    std::vector<uint8_t> nalus_in_annex_b =
        kSpsInAnnexB + kPpsInAnnexB + kIdrInAnnexB;

    VideoConfig config(kSbMediaVideoCodecH264, 640, 480,
                       nalus_in_annex_b.data(), nalus_in_annex_b.size());
    EXPECT_TRUE(config == config);
    EXPECT_FALSE(config != config);
  }
  {
    VideoConfig config(kSbMediaVideoCodecVp9, 640, 480, nullptr, 0);
    EXPECT_TRUE(config == config);
    EXPECT_FALSE(config != config);
  }
}

TEST(VideoConfigTest, H264) {
  std::vector<uint8_t> nalus_in_annex_b =
      kSpsInAnnexB + kPpsInAnnexB + kIdrInAnnexB;

  VideoConfig config(kSbMediaVideoCodecH264, 640, 480, nalus_in_annex_b.data(),
                     nalus_in_annex_b.size());

  // Different resolution, same parameter sets.
  VideoConfig config_1(kSbMediaVideoCodecH264, 1920, 1080,
                       nalus_in_annex_b.data(), nalus_in_annex_b.size());
  EXPECT_TRUE(config != config_1);
  EXPECT_TRUE(config.avc_parameter_sets() == config_1.avc_parameter_sets());
  EXPECT_FALSE(config == config_1);

  // Same resolution, different parameter sets.
  nalus_in_annex_b =
      kSpsInAnnexB + kPpsInAnnexB + std::vector<uint8_t>({99}) + kIdrInAnnexB;
  VideoConfig config_2(kSbMediaVideoCodecH264, 640, 480,
                       nalus_in_annex_b.data(), nalus_in_annex_b.size());
  EXPECT_TRUE(config != config_2);
  EXPECT_FALSE(config == config_2);

  // Same resolution, same parameter sets, different idr data.
  nalus_in_annex_b = kSpsInAnnexB + kPpsInAnnexB + kIdrInAnnexB;
  nalus_in_annex_b.push_back(99);

  VideoConfig config_3(kSbMediaVideoCodecH264, 640, 480,
                       nalus_in_annex_b.data(), nalus_in_annex_b.size());
  EXPECT_TRUE(config == config_3);
  EXPECT_FALSE(config != config_3);
}

TEST(VideoConfigTest, H264MultiSpsPps) {
  // Single sps and pps.
  std::vector<uint8_t> nalus_in_annex_b =
      kSpsInAnnexB + kPpsInAnnexB + kIdrInAnnexB;

  VideoConfig config_single_sps_pps(kSbMediaVideoCodecH264, 640, 480,
                                    nalus_in_annex_b.data(),
                                    nalus_in_annex_b.size());

  // Same resolution, multiple parameter sets.
  nalus_in_annex_b =
      kSpsInAnnexB + kSpsInAnnexB + kPpsInAnnexB + kPpsInAnnexB + kIdrInAnnexB;

  VideoConfig config_dual_sps_pps(kSbMediaVideoCodecH264, 640, 480,
                                  nalus_in_annex_b.data(),
                                  nalus_in_annex_b.size());
  EXPECT_TRUE(config_single_sps_pps != config_dual_sps_pps);
  EXPECT_FALSE(config_single_sps_pps == config_dual_sps_pps);
  EXPECT_TRUE(config_dual_sps_pps.avc_parameter_sets() ==
              AvcParameterSets(kAnnexB, nalus_in_annex_b.data(),
                               nalus_in_annex_b.size()));

  // Same resolution, different parameter sets.
  nalus_in_annex_b =
      kSpsInAnnexB + kPpsInAnnexB + std::vector<uint8_t>({99}) + kIdrInAnnexB;
  VideoConfig config_1(kSbMediaVideoCodecH264, 640, 480,
                       nalus_in_annex_b.data(), nalus_in_annex_b.size());
  EXPECT_TRUE(config_single_sps_pps != config_1);
  EXPECT_FALSE(config_single_sps_pps == config_1);

  // Same resolution, same parameter sets, different idr data.
  nalus_in_annex_b = kSpsInAnnexB + kPpsInAnnexB + kIdrInAnnexB;
  nalus_in_annex_b.push_back(99);

  VideoConfig config_2(kSbMediaVideoCodecH264, 640, 480,
                       nalus_in_annex_b.data(), nalus_in_annex_b.size());
  EXPECT_TRUE(config_single_sps_pps == config_2);
  EXPECT_FALSE(config_single_sps_pps != config_2);
}

TEST(VideoConfigTest, Vp9) {
  // The class shouldn't look into vp9 bitstreams.
  const uint8_t kInvalidData[] = {1, 7, 25};

  VideoConfig config(kSbMediaVideoCodecVp9, 640, 480, kInvalidData,
                     SB_ARRAY_SIZE(kInvalidData));

  // Different resolution, same data.
  VideoConfig config_1(kSbMediaVideoCodecVp9, 1920, 1080, kInvalidData,
                       SB_ARRAY_SIZE(kInvalidData));
  EXPECT_TRUE(config != config_1);
  EXPECT_FALSE(config == config_1);

  // Same resolution, different data (one less byte).
  VideoConfig config_2(kSbMediaVideoCodecVp9, 640, 480, kInvalidData,
                       SB_ARRAY_SIZE(kInvalidData) - 1);
  EXPECT_TRUE(config == config_2);
  EXPECT_FALSE(config != config_2);
}

TEST(VideoConfigTest, H264VsVp9) {
  std::vector<uint8_t> nalus_in_annex_b =
      kSpsInAnnexB + kPpsInAnnexB + kIdrInAnnexB;

  VideoConfig config_h264(kSbMediaVideoCodecH264, 640, 480,
                          nalus_in_annex_b.data(), nalus_in_annex_b.size());
  VideoConfig config_vp9(kSbMediaVideoCodecVp9, 640, 480,
                         nalus_in_annex_b.data(), nalus_in_annex_b.size());

  EXPECT_TRUE(config_h264 != config_vp9);
  EXPECT_FALSE(config_h264 == config_vp9);
}

}  // namespace
}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
