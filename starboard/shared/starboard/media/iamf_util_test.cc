// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/media/iamf_util.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {
namespace {

TEST(IamfUtilTest, Valid) {
  std::string codec_param = "iamf.000.000.Opus";
  IamfMimeUtil util(codec_param);
  EXPECT_TRUE(util.is_valid());

  codec_param = "iamf.000.000.fLaC";
  util = IamfMimeUtil(codec_param);
  EXPECT_TRUE(util.is_valid());

  codec_param = "iamf.000.000.ipcm";
  util = IamfMimeUtil(codec_param);
  EXPECT_TRUE(util.is_valid());

  codec_param = "iamf.000.000.mp4a.40.2";
  util = IamfMimeUtil(codec_param);
  EXPECT_TRUE(util.is_valid());
}

TEST(IamfUtilTest, Invalid) {
  // Invalid substream codec
  std::string codec_param = "iamf.000.000.vorbis";
  IamfMimeUtil util(codec_param);
  EXPECT_FALSE(util.is_valid());

  // Invalid codec capitalization
  codec_param = "iamf.000.000.opus";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());
  codec_param = "iamf.000.000.flac";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());
  codec_param = "iamf.000.000.FlAc";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());
  codec_param = "iamf.000.000.MP4a.40.2";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());
  codec_param = "IAMF.000.000.Opus";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());

  // Invalid additional profile value
  codec_param = "iamf.000.999.Opus";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());

  // Invalid primary profile value
  codec_param = "iamf.999.000.Opus";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());

  // Invalid leading codec param
  codec_param = "iacb.000.000.Opus";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());

  // Invalid length for Opus substream
  codec_param = "iamf.000.000.Opus.40.2";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());

  // Invalid length for AAC-LC substream
  codec_param = "iamf.000.000.mp4a";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());

  // Invalid param for AAC-LC substream
  codec_param = "iamf.000.000.mp4a.40.3";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());

  // Invalid param for AAC-LC substream
  codec_param = "iamf.000.000.mp4a.40.20";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());

  // Too many delimiting periods
  codec_param = "iamf.000.000..Opus";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());

  // Leading delimiting period
  codec_param = ".iamf.000.000.Opus";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());

  // Trailing delimiting period
  codec_param = "iamf.000.000.Opus.";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());

  // No delimiting period between codec param string and primary profile
  codec_param = "iamf000.000.Opus";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());

  // Invalid primary profile length
  codec_param = "iamf.00.000.Opus";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());
  codec_param = "iamf.0000.000.Opus";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());

  // Invalid additional profile length
  codec_param = "iamf.000.00.Opus";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());
  codec_param = "iamf.000.0000.Opus";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());

  // Letters in primary profile value
  codec_param = "iamf.0aa.000.Opus";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());
  codec_param = "iamf.xxx.000.Opus";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());

  // Letters in additional profile value
  codec_param = "iamf.000.0aa.Opus";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());
  codec_param = "iamf.000.xxx.Opus";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());

  // Letter in primary profile value
  codec_param = "iamf.0a0.000.Opus";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());

  // Invalid additional profile with AAC-LC substream
  codec_param = "iamf.000.00.mp4a.40.2";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());

  // Misplaced delimiting period for AAC-LC substream
  codec_param = "iamf.000.000.mp4a.402.";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());

  // Negative primary profile value
  codec_param = "iamf.-12.000.Opus";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());

  // Empty param
  codec_param = "";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());

  // Leading whitespace
  codec_param = " iamf.000.000.Opus";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());

  // Trailing whitespace
  codec_param = "iamf.000.000.Opus ";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());

  // Whitespace in the middle
  codec_param = "iamf.00 0.000.Opus";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());

  // HE-AAC substream param
  codec_param = "iamf.000.000.mp4a.40.5";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());

  // Non IAMF codec param
  codec_param = "ec-3";
  util = IamfMimeUtil(codec_param);
  EXPECT_FALSE(util.is_valid());
}

TEST(IamfUtilTest, SubstreamCodec) {
  std::string codec_param = "iamf.000.000.Opus";
  IamfMimeUtil util(codec_param);
  EXPECT_TRUE(util.is_valid());
  EXPECT_EQ(util.substream_codec(), kIamfSubstreamCodecOpus);

  codec_param = "iamf.000.000.fLaC";
  util = IamfMimeUtil(codec_param);
  EXPECT_TRUE(util.is_valid());
  EXPECT_EQ(util.substream_codec(), kIamfSubstreamCodecFlac);

  codec_param = "iamf.000.000.ipcm";
  util = IamfMimeUtil(codec_param);
  EXPECT_TRUE(util.is_valid());
  EXPECT_EQ(util.substream_codec(), kIamfSubstreamCodecIpcm);

  codec_param = "iamf.000.000.mp4a.40.2";
  util = IamfMimeUtil(codec_param);
  EXPECT_TRUE(util.is_valid());
  EXPECT_EQ(util.substream_codec(), kIamfSubstreamCodecMp4a);
}

TEST(IamfUtilTest, Profile) {
  std::string codec_param = "iamf.000.000.Opus";
  IamfMimeUtil util(codec_param);
  EXPECT_TRUE(util.is_valid());
  EXPECT_EQ(util.primary_profile(), kIamfProfileSimple);
  EXPECT_EQ(util.additional_profile(), kIamfProfileSimple);

  codec_param = "iamf.001.000.Opus";
  util = IamfMimeUtil(codec_param);
  EXPECT_TRUE(util.is_valid());
  EXPECT_EQ(util.primary_profile(), kIamfProfileBase);
  EXPECT_EQ(util.additional_profile(), kIamfProfileSimple);

  codec_param = "iamf.000.001.Opus";
  util = IamfMimeUtil(codec_param);
  EXPECT_TRUE(util.is_valid());
  EXPECT_EQ(util.primary_profile(), kIamfProfileSimple);
  EXPECT_EQ(util.additional_profile(), kIamfProfileBase);

  codec_param = "iamf.002.000.Opus";
  util = IamfMimeUtil(codec_param);
  EXPECT_TRUE(util.is_valid());
  ASSERT_NE(util.primary_profile(), kIamfProfileSimple);
  ASSERT_NE(util.primary_profile(), kIamfProfileBase);
  ASSERT_EQ(util.primary_profile(), 2);
}

}  // namespace
}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
