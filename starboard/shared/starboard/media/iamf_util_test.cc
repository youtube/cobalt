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
namespace {

// From iamf_simple_profile_5_1.dmp.
constexpr uint8_t kSimpleProfileSequenceHeaderObu[] = {0xF8, 0x06, 0x69, 0x61,
                                                       0x6D, 0x66, 0x00, 0x00};

// From iamf_base_profile_stereo_ambisonics.dmp.
constexpr uint8_t kBaseProfileSequenceHeaderObu[] = {0xF8, 0x06, 0x69, 0x61,
                                                     0x6D, 0x66, 0x01, 0x01};

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

TEST(IamfUtilTest, ParsesSequenceHeaderObu) {
  uint8_t primary_profile = 0;
  uint8_t additional_profile = 0;

  bool parse_result = IamfMimeUtil::ParseIamfSequenceHeaderObu(
      kSimpleProfileSequenceHeaderObu,
      SB_ARRAY_SIZE(kSimpleProfileSequenceHeaderObu), &primary_profile,
      &additional_profile);

  ASSERT_TRUE(parse_result);

  EXPECT_EQ(primary_profile, kIamfProfileSimple);
  EXPECT_EQ(additional_profile, kIamfProfileSimple);

  primary_profile = 0;
  additional_profile = 0;

  parse_result = IamfMimeUtil::ParseIamfSequenceHeaderObu(
      kBaseProfileSequenceHeaderObu,
      SB_ARRAY_SIZE(kBaseProfileSequenceHeaderObu), &primary_profile,
      &additional_profile);

  ASSERT_TRUE(parse_result);

  EXPECT_EQ(primary_profile, kIamfProfileBase);
  EXPECT_EQ(additional_profile, kIamfProfileBase);
}

TEST(IamfUtilTest, ParseSequenceHeaderFailsOnInvalidObuType) {
  // First byte 0x08 means OBU type 1, not 31.
  const uint8_t kInvalidObuType[] = {0x08, 0x06, 0x69, 0x61,
                                     0x6D, 0x66, 0x00, 0x00};
  uint8_t primary_profile = 0;
  uint8_t additional_profile = 0;

  bool parse_result = IamfMimeUtil::ParseIamfSequenceHeaderObu(
      kInvalidObuType, SB_ARRAY_SIZE(kInvalidObuType), &primary_profile,
      &additional_profile);

  ASSERT_FALSE(parse_result);
}

TEST(IamfUtilTest, ParseSequenceHeaderFailsOnTruncatedData) {
  const uint8_t kTruncatedData[] = {0xF8, 0x06, 0x69, 0x61,
                                    0x6D};  // Truncated ia_code.
  uint8_t primary_profile = 0;
  uint8_t additional_profile = 0;

  bool parse_result = IamfMimeUtil::ParseIamfSequenceHeaderObu(
      kTruncatedData, SB_ARRAY_SIZE(kTruncatedData), &primary_profile,
      &additional_profile);

  ASSERT_FALSE(parse_result);
}

TEST(IamfUtilTest, ParseSequenceHeaderFailsOnTruncatedLeb128) {
  const uint8_t kTruncatedLeb128[] = {0xF8, 0x81};  // Incomplete LEB128 size.
  uint8_t primary_profile = 0;
  uint8_t additional_profile = 0;

  bool parse_result = IamfMimeUtil::ParseIamfSequenceHeaderObu(
      kTruncatedLeb128, SB_ARRAY_SIZE(kTruncatedLeb128), &primary_profile,
      &additional_profile);

  ASSERT_FALSE(parse_result);
}

TEST(IamfUtilTest, ParseSequenceHeaderFailsOnInvalid5ByteLeb128) {
  // This is an invalid encoding for a 32-bit value.
  const uint8_t kInvalidLeb128[] = {0xF8, 0x81, 0x81, 0x81, 0x81, 0x10};
  uint8_t primary_profile = 0;
  uint8_t additional_profile = 0;

  bool parse_result = IamfMimeUtil::ParseIamfSequenceHeaderObu(
      kInvalidLeb128, SB_ARRAY_SIZE(kInvalidLeb128), &primary_profile,
      &additional_profile);

  ASSERT_FALSE(parse_result);
}

TEST(IamfUtilTest, ParseSequenceHeaderFailsWhenObuSizeExceedsBufferSize) {
  // LEB128 size is 7, but only 6 bytes remain.
  const uint8_t kObuSizeTooLarge[] = {0xF8, 0x07, 0x69, 0x61,
                                      0x6D, 0x66, 0x00, 0x00};
  uint8_t primary_profile = 0;
  uint8_t additional_profile = 0;

  bool parse_result = IamfMimeUtil::ParseIamfSequenceHeaderObu(
      kObuSizeTooLarge, SB_ARRAY_SIZE(kObuSizeTooLarge), &primary_profile,
      &additional_profile);

  ASSERT_FALSE(parse_result);
}

TEST(IamfUtilTest, ParseSequenceHeaderFailsWhenObuSizeIsTooSmall) {
  // LEB128 size is 5, but 6 bytes are needed for ia_code and profiles.
  const uint8_t kObuSizeTooSmall[] = {0xF8, 0x05, 0x69, 0x61,
                                      0x6D, 0x66, 0x00, 0x00};
  uint8_t primary_profile = 0;
  uint8_t additional_profile = 0;

  bool parse_result = IamfMimeUtil::ParseIamfSequenceHeaderObu(
      kObuSizeTooSmall, SB_ARRAY_SIZE(kObuSizeTooSmall), &primary_profile,
      &additional_profile);

  ASSERT_FALSE(parse_result);
}

}  // namespace
}  // namespace starboard
