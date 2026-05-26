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
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

TEST(IamfUtilTest, Valid) {
  EXPECT_TRUE(IamfMimeUtil::Create("iamf.000.000.Opus").has_value());
  EXPECT_TRUE(IamfMimeUtil::Create("iamf.000.000.fLaC").has_value());
  EXPECT_TRUE(IamfMimeUtil::Create("iamf.000.000.ipcm").has_value());
  EXPECT_TRUE(IamfMimeUtil::Create("iamf.000.000.mp4a.40.2").has_value());
}

TEST(IamfUtilTest, Invalid) {
  // Invalid substream codec
  EXPECT_FALSE(IamfMimeUtil::Create("iamf.000.000.vorbis").has_value());

  // Invalid codec capitalization
  EXPECT_FALSE(IamfMimeUtil::Create("iamf.000.000.opus").has_value());
  EXPECT_FALSE(IamfMimeUtil::Create("iamf.000.000.flac").has_value());
  EXPECT_FALSE(IamfMimeUtil::Create("iamf.000.000.FlAc").has_value());
  EXPECT_FALSE(IamfMimeUtil::Create("iamf.000.000.MP4a.40.2").has_value());
  EXPECT_FALSE(IamfMimeUtil::Create("IAMF.000.000.Opus").has_value());

  // Invalid additional profile value
  EXPECT_FALSE(IamfMimeUtil::Create("iamf.000.999.Opus").has_value());

  // Invalid primary profile value
  EXPECT_FALSE(IamfMimeUtil::Create("iamf.999.000.Opus").has_value());

  // Invalid leading codec param
  EXPECT_FALSE(IamfMimeUtil::Create("iacb.000.000.Opus").has_value());

  // Invalid length for Opus substream
  EXPECT_FALSE(IamfMimeUtil::Create("iamf.000.000.Opus.40.2").has_value());

  // Invalid length for AAC-LC substream
  EXPECT_FALSE(IamfMimeUtil::Create("iamf.000.000.mp4a").has_value());

  // Invalid param for AAC-LC substream
  EXPECT_FALSE(IamfMimeUtil::Create("iamf.000.000.mp4a.40.3").has_value());

  // Invalid param for AAC-LC substream
  EXPECT_FALSE(IamfMimeUtil::Create("iamf.000.000.mp4a.40.20").has_value());

  // Too many delimiting periods
  EXPECT_FALSE(IamfMimeUtil::Create("iamf.000.000..Opus").has_value());

  // Leading delimiting period
  EXPECT_FALSE(IamfMimeUtil::Create(".iamf.000.000.Opus").has_value());

  // Trailing delimiting period
  EXPECT_FALSE(IamfMimeUtil::Create("iamf.000.000.Opus.").has_value());

  // No delimiting period between codec param string and primary profile
  EXPECT_FALSE(IamfMimeUtil::Create("iamf000.000.Opus").has_value());

  // Invalid primary profile length
  EXPECT_FALSE(IamfMimeUtil::Create("iamf.00.000.Opus").has_value());
  EXPECT_FALSE(IamfMimeUtil::Create("iamf.0000.000.Opus").has_value());

  // Invalid additional profile length
  EXPECT_FALSE(IamfMimeUtil::Create("iamf.000.00.Opus").has_value());
  EXPECT_FALSE(IamfMimeUtil::Create("iamf.000.0000.Opus").has_value());

  // Letters in primary profile value
  EXPECT_FALSE(IamfMimeUtil::Create("iamf.0aa.000.Opus").has_value());
  EXPECT_FALSE(IamfMimeUtil::Create("iamf.xxx.000.Opus").has_value());

  // Letters in additional profile value
  EXPECT_FALSE(IamfMimeUtil::Create("iamf.000.0aa.Opus").has_value());
  EXPECT_FALSE(IamfMimeUtil::Create("iamf.000.xxx.Opus").has_value());

  // Letter in primary profile value
  EXPECT_FALSE(IamfMimeUtil::Create("iamf.0a0.000.Opus").has_value());

  // Invalid additional profile with AAC-LC substream
  EXPECT_FALSE(IamfMimeUtil::Create("iamf.000.00.mp4a.40.2").has_value());

  // Misplaced delimiting period for AAC-LC substream
  EXPECT_FALSE(IamfMimeUtil::Create("iamf.000.000.mp4a.402.").has_value());

  // Negative primary profile value
  EXPECT_FALSE(IamfMimeUtil::Create("iamf.-12.000.Opus").has_value());

  EXPECT_FALSE(IamfMimeUtil::Create("").has_value());

  // Leading whitespace
  EXPECT_FALSE(IamfMimeUtil::Create(" iamf.000.000.Opus").has_value());

  // Trailing whitespace
  EXPECT_FALSE(IamfMimeUtil::Create("iamf.000.000.Opus ").has_value());

  // Whitespace in the middle
  EXPECT_FALSE(IamfMimeUtil::Create("iamf.00 0.000.Opus").has_value());

  // HE-AAC substream param
  EXPECT_FALSE(IamfMimeUtil::Create("iamf.000.000.mp4a.40.5").has_value());

  // Non IAMF codec param
  EXPECT_FALSE(IamfMimeUtil::Create("ec-3").has_value());
}

TEST(IamfUtilTest, SubstreamCodecOpus) {
  auto util = IamfMimeUtil::Create("iamf.000.000.Opus");
  ASSERT_TRUE(util.has_value());
  EXPECT_EQ(util->substream_codec(), kIamfSubstreamCodecOpus);
}

TEST(IamfUtilTest, SubstreamCodecFlac) {
  auto util = IamfMimeUtil::Create("iamf.000.000.fLaC");
  ASSERT_TRUE(util.has_value());
  EXPECT_EQ(util->substream_codec(), kIamfSubstreamCodecFlac);
}

TEST(IamfUtilTest, SubstreamCodecIpcm) {
  auto util = IamfMimeUtil::Create("iamf.000.000.ipcm");
  ASSERT_TRUE(util.has_value());
  EXPECT_EQ(util->substream_codec(), kIamfSubstreamCodecIpcm);
}

TEST(IamfUtilTest, SubstreamCodecMp4a) {
  auto util = IamfMimeUtil::Create("iamf.000.000.mp4a.40.2");
  ASSERT_TRUE(util.has_value());
  EXPECT_EQ(util->substream_codec(), kIamfSubstreamCodecMp4a);
}

TEST(IamfUtilTest, ProfileSimpleSimple) {
  auto util = IamfMimeUtil::Create("iamf.000.000.Opus");
  ASSERT_TRUE(util.has_value());
  EXPECT_EQ(util->primary_profile(), kIamfProfileSimple);
  EXPECT_EQ(util->additional_profile(), kIamfProfileSimple);
}

TEST(IamfUtilTest, ProfileBaseSimple) {
  auto util = IamfMimeUtil::Create("iamf.001.000.Opus");
  ASSERT_TRUE(util.has_value());
  EXPECT_EQ(util->primary_profile(), kIamfProfileBase);
  EXPECT_EQ(util->additional_profile(), kIamfProfileSimple);
}

TEST(IamfUtilTest, ProfileSimpleBase) {
  auto util = IamfMimeUtil::Create("iamf.000.001.Opus");
  ASSERT_TRUE(util.has_value());
  EXPECT_EQ(util->primary_profile(), kIamfProfileSimple);
  EXPECT_EQ(util->additional_profile(), kIamfProfileBase);
}

TEST(IamfUtilTest, ProfileOther) {
  auto util = IamfMimeUtil::Create("iamf.002.000.Opus");
  ASSERT_TRUE(util.has_value());
  ASSERT_NE(util->primary_profile(), kIamfProfileSimple);
  ASSERT_NE(util->primary_profile(), kIamfProfileBase);
  ASSERT_EQ(util->primary_profile(), 2U);
}

TEST(IamfUtilTest, ParsesSequenceHeaderObu) {
  // From iamf_simple_profile_5_1.dmp.
  const std::vector<uint8_t> kSimpleProfileSequenceHeaderObu = {
      0xF8, 0x06, 0x69, 0x61, 0x6D, 0x66, 0x00, 0x00};

  // From iamf_base_profile_stereo_ambisonics.dmp.
  const std::vector<uint8_t> kBaseProfileSequenceHeaderObu = {
      0xF8, 0x06, 0x69, 0x61, 0x6D, 0x66, 0x01, 0x01};

  auto result =
      IamfMimeUtil::ParseIamfSequenceHeaderObu(kSimpleProfileSequenceHeaderObu);

  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_EQ(result->primary_profile, kIamfProfileSimple);
  EXPECT_EQ(result->additional_profile, kIamfProfileSimple);

  result =
      IamfMimeUtil::ParseIamfSequenceHeaderObu(kBaseProfileSequenceHeaderObu);

  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_EQ(result->primary_profile, kIamfProfileBase);
  EXPECT_EQ(result->additional_profile, kIamfProfileBase);
}

TEST(IamfUtilTest, ParseSequenceHeaderFailsOnInvalidObuType) {
  // First byte 0x08 means OBU type 1, not 31.
  const uint8_t kInvalidObuType[] = {0x08, 0x06, 0x69, 0x61,
                                     0x6D, 0x66, 0x00, 0x00};
  std::vector<uint8_t> data(kInvalidObuType,
                            kInvalidObuType + SB_ARRAY_SIZE(kInvalidObuType));
  auto result = IamfMimeUtil::ParseIamfSequenceHeaderObu(data);
  ASSERT_FALSE(result.has_value())
      << "Parsed IA Sequence Header OBU when an error was expected. Primary "
         "profile: "
      << result->primary_profile
      << " additional profile: " << result->additional_profile;
}

TEST(IamfUtilTest, ParseSequenceHeaderFailsOnTruncatedData) {
  const uint8_t kTruncatedData[] = {0xF8, 0x06, 0x69, 0x61,
                                    0x6D};  // Truncated ia_code.
  std::vector<uint8_t> data(kTruncatedData,
                            kTruncatedData + SB_ARRAY_SIZE(kTruncatedData));
  auto result = IamfMimeUtil::ParseIamfSequenceHeaderObu(data);
  ASSERT_FALSE(result.has_value())
      << "Parsed IA Sequence Header OBU when an error was expected. Primary "
         "profile: "
      << result->primary_profile
      << " additional profile: " << result->additional_profile;
}

TEST(IamfUtilTest, ParseSequenceHeaderFailsOnTruncatedLeb128) {
  const uint8_t kTruncatedLeb128[] = {0xF8, 0x81};  // Incomplete LEB128 size.
  std::vector<uint8_t> data(kTruncatedLeb128,
                            kTruncatedLeb128 + SB_ARRAY_SIZE(kTruncatedLeb128));
  auto result = IamfMimeUtil::ParseIamfSequenceHeaderObu(data);
  ASSERT_FALSE(result.has_value())
      << "Parsed IA Sequence Header OBU when an error was expected. Primary "
         "profile: "
      << result->primary_profile
      << " additional profile: " << result->additional_profile;
}

TEST(IamfUtilTest, ParseSequenceHeaderFailsOnInvalid5ByteLeb128) {
  // This is an invalid encoding for a 32-bit value.
  const uint8_t kInvalidLeb128[] = {0xF8, 0x81, 0x81, 0x81, 0x81, 0x10};
  std::vector<uint8_t> data(kInvalidLeb128,
                            kInvalidLeb128 + SB_ARRAY_SIZE(kInvalidLeb128));
  auto result = IamfMimeUtil::ParseIamfSequenceHeaderObu(data);
  ASSERT_FALSE(result.has_value())
      << "Parsed IA Sequence Header OBU when an error was expected. Primary "
         "profile: "
      << result->primary_profile
      << " additional profile: " << result->additional_profile;
}

TEST(IamfUtilTest, ParseSequenceHeaderFailsWhenObuSizeExceedsBufferSize) {
  // LEB128 size is 7, but only 6 bytes remain.
  const uint8_t kObuSizeTooLarge[] = {0xF8, 0x07, 0x69, 0x61,
                                      0x6D, 0x66, 0x00, 0x00};
  std::vector<uint8_t> data(kObuSizeTooLarge,
                            kObuSizeTooLarge + SB_ARRAY_SIZE(kObuSizeTooLarge));
  auto result = IamfMimeUtil::ParseIamfSequenceHeaderObu(data);
  ASSERT_FALSE(result.has_value())
      << "Parsed IA Sequence Header OBU when an error was expected. Primary "
         "profile: "
      << result->primary_profile
      << " additional profile: " << result->additional_profile;
}

TEST(IamfUtilTest, ParseSequenceHeaderFailsWhenObuSizeIsTooSmall) {
  // LEB128 size is 5, but 6 bytes are needed for ia_code and profiles.
  const uint8_t kObuSizeTooSmall[] = {0xF8, 0x05, 0x69, 0x61,
                                      0x6D, 0x66, 0x00, 0x00};
  std::vector<uint8_t> data(kObuSizeTooSmall,
                            kObuSizeTooSmall + SB_ARRAY_SIZE(kObuSizeTooSmall));
  auto result = IamfMimeUtil::ParseIamfSequenceHeaderObu(data);
  ASSERT_FALSE(result.has_value())
      << "Parsed IA Sequence Header OBU when an error was expected. Primary "
         "profile: "
      << result->primary_profile
      << " additional profile: " << result->additional_profile;
}

}  // namespace
}  // namespace starboard
