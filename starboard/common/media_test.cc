// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/media.h"

#include "starboard/common/log.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

class ParseVideoCodecTest : public ::testing::Test {
 protected:
  bool Parse(const char* codec_string) {
    return ParseVideoCodec(codec_string, &codec_, &profile_, &level_,
                           &bit_depth_, &color_primary_id_, &transfer_id_,
                           &matrix_id_);
  }

  SbMediaVideoCodec codec_;
  int profile_;
  int level_;
  int bit_depth_;
  SbMediaPrimaryId color_primary_id_;
  SbMediaTransferId transfer_id_;
  SbMediaMatrixId matrix_id_;
};

TEST_F(ParseVideoCodecTest, SimpleCodecs) {
  const char* kCodecStrings[] = {"vp8", "vp9"};
  const SbMediaVideoCodec kVideoCodecs[] = {kSbMediaVideoCodecVp8,
                                            kSbMediaVideoCodecVp9};
  for (size_t i = 0; i < SB_ARRAY_SIZE(kCodecStrings); ++i) {
    ASSERT_TRUE(Parse(kCodecStrings[i]));
    EXPECT_EQ(codec_, kVideoCodecs[i]);
    EXPECT_EQ(profile_, -1);
    EXPECT_EQ(level_, -1);
    EXPECT_EQ(bit_depth_, 8);
    EXPECT_EQ(color_primary_id_, kSbMediaPrimaryIdUnspecified);
    EXPECT_EQ(transfer_id_, kSbMediaTransferIdUnspecified);
    EXPECT_EQ(matrix_id_, kSbMediaMatrixIdUnspecified);
  }
}

TEST_F(ParseVideoCodecTest, EmptyString) {
  ASSERT_FALSE(Parse(""));
}

TEST_F(ParseVideoCodecTest, ShortFormAv1) {
  ASSERT_TRUE(Parse("av01.0.01M.08"));
  EXPECT_EQ(codec_, kSbMediaVideoCodecAv1);
  EXPECT_EQ(profile_, 0);
  EXPECT_EQ(level_, 21);
  EXPECT_EQ(bit_depth_, 8);
  EXPECT_EQ(color_primary_id_, kSbMediaPrimaryIdBt709);
  EXPECT_EQ(transfer_id_, kSbMediaTransferIdBt709);
  EXPECT_EQ(matrix_id_, kSbMediaMatrixIdBt709);
}

TEST_F(ParseVideoCodecTest, LongFormAv1) {
  ASSERT_TRUE(Parse("av01.0.04M.10.0.110.09.16.09.0"));
  EXPECT_EQ(codec_, kSbMediaVideoCodecAv1);
  EXPECT_EQ(profile_, 0);
  EXPECT_EQ(level_, 30);
  EXPECT_EQ(bit_depth_, 10);
  EXPECT_EQ(color_primary_id_, kSbMediaPrimaryIdBt2020);
  EXPECT_EQ(transfer_id_, kSbMediaTransferIdSmpteSt2084);
  EXPECT_EQ(matrix_id_, kSbMediaMatrixIdBt2020NonconstantLuminance);
}

TEST_F(ParseVideoCodecTest, InvalidAv1) {
  EXPECT_FALSE(Parse("av01.0.04M.10.0.110.9.16.9.0"));
  EXPECT_FALSE(Parse("av01.0.04M.10.0.110.09.16.09"));
  EXPECT_FALSE(Parse("av01.0.04M.10.0.110.09.16"));
  EXPECT_FALSE(Parse("av01.0.04M.10.0.110.09"));
  EXPECT_FALSE(Parse("av01.0.04M.10.0.110"));
  EXPECT_FALSE(Parse("av01.0.04M.10.0"));
  EXPECT_FALSE(Parse("av01.0.04M"));
  EXPECT_FALSE(Parse("av01.0"));
  EXPECT_FALSE(Parse("av01"));

  EXPECT_FALSE(Parse("av02.0.04M.10.0.110.09.16.09.0"));
  EXPECT_FALSE(Parse("av01.0.04X.10.0.110.09.16.09.0"));
  EXPECT_FALSE(Parse("av01.0.04M.10.0.110.09.16.09.2"));
}

TEST_F(ParseVideoCodecTest, Avc) {
  ASSERT_TRUE(Parse("avc1.640028"));
  EXPECT_EQ(codec_, kSbMediaVideoCodecH264);
  EXPECT_EQ(profile_, 100);
  EXPECT_EQ(level_, 40);
  EXPECT_EQ(bit_depth_, 8);
  EXPECT_EQ(color_primary_id_, kSbMediaPrimaryIdUnspecified);
  EXPECT_EQ(transfer_id_, kSbMediaTransferIdUnspecified);
  EXPECT_EQ(matrix_id_, kSbMediaMatrixIdUnspecified);

  ASSERT_TRUE(Parse("avc3.640028"));
  EXPECT_EQ(codec_, kSbMediaVideoCodecH264);
  EXPECT_EQ(profile_, 100);
  EXPECT_EQ(level_, 40);
  EXPECT_EQ(bit_depth_, 8);
  EXPECT_EQ(color_primary_id_, kSbMediaPrimaryIdUnspecified);
  EXPECT_EQ(transfer_id_, kSbMediaTransferIdUnspecified);
  EXPECT_EQ(matrix_id_, kSbMediaMatrixIdUnspecified);
}

TEST_F(ParseVideoCodecTest, InvalidAvc) {
  EXPECT_FALSE(Parse("avc1.64002"));
  EXPECT_FALSE(Parse("avc2.640028"));
  EXPECT_FALSE(Parse("avc3.640028.1"));
}

TEST_F(ParseVideoCodecTest, H265) {
  ASSERT_TRUE(Parse("hvc1.1.2.L93.B0"));
  EXPECT_EQ(codec_, kSbMediaVideoCodecH265);
  EXPECT_EQ(profile_, 1);
  EXPECT_EQ(level_, 31);
  ASSERT_TRUE(Parse("hev1.A4.41.H120.B0.12.34.56.78.90"));
  EXPECT_EQ(codec_, kSbMediaVideoCodecH265);
  EXPECT_EQ(profile_, 1);
  EXPECT_EQ(level_, 40);

  EXPECT_TRUE(Parse("hvc1.1.2.H93.B0"));
  EXPECT_TRUE(Parse("hvc1.A1.2.H93.B0"));
  EXPECT_TRUE(Parse("hvc1.B1.2.H93.B0"));
  EXPECT_TRUE(Parse("hvc1.C1.2.H93.B0"));
  EXPECT_TRUE(Parse("hvc1.C1.2.H93"));
  EXPECT_TRUE(Parse("hvc1.C1.ABCDEF01.H93.B0"));
}

TEST_F(ParseVideoCodecTest, InvalidH265) {
  EXPECT_FALSE(Parse("hvc2.1.2.L93.B0"));
  EXPECT_FALSE(Parse("hvc1.D1.2.L93.B0"));
  EXPECT_FALSE(Parse("hvc1.A111.2.L93.B0"));
  EXPECT_FALSE(Parse("hvc1.111.2.L93.B0"));
  EXPECT_FALSE(Parse("hvc1.1.ABCDEF012.L93.B0"));
  EXPECT_FALSE(Parse("hvc1.1.2.L92.B0"));
  EXPECT_FALSE(Parse("hvc1.1.2.P93.B0"));
  EXPECT_FALSE(Parse("hvc1.1.2.L93.B0.B1.B2.B3.B4.B5.B6"));
}

TEST_F(ParseVideoCodecTest, ShortFormVp9) {
  ASSERT_TRUE(Parse("vp09.00.41.08"));
  EXPECT_EQ(codec_, kSbMediaVideoCodecVp9);
  EXPECT_EQ(profile_, 0);
  EXPECT_EQ(level_, 41);
  EXPECT_EQ(bit_depth_, 8);
  EXPECT_EQ(color_primary_id_, kSbMediaPrimaryIdBt709);
  EXPECT_EQ(transfer_id_, kSbMediaTransferIdBt709);
  EXPECT_EQ(matrix_id_, kSbMediaMatrixIdBt709);
}

TEST_F(ParseVideoCodecTest, MediumFormVp9) {
  ASSERT_TRUE(Parse("vp09.02.10.10.01.09.16.09"));
  EXPECT_EQ(codec_, kSbMediaVideoCodecVp9);
  EXPECT_EQ(profile_, 2);
  EXPECT_EQ(level_, 10);
  EXPECT_EQ(bit_depth_, 10);
  EXPECT_EQ(color_primary_id_, kSbMediaPrimaryIdBt2020);
  EXPECT_EQ(transfer_id_, kSbMediaTransferIdSmpteSt2084);
  EXPECT_EQ(matrix_id_, kSbMediaMatrixIdBt2020NonconstantLuminance);
}

TEST_F(ParseVideoCodecTest, LongFormVp9) {
  ASSERT_TRUE(Parse("vp09.02.10.10.01.09.16.09.01"));
  EXPECT_EQ(codec_, kSbMediaVideoCodecVp9);
  EXPECT_EQ(profile_, 2);
  EXPECT_EQ(level_, 10);
  EXPECT_EQ(bit_depth_, 10);
  EXPECT_EQ(color_primary_id_, kSbMediaPrimaryIdBt2020);
  EXPECT_EQ(transfer_id_, kSbMediaTransferIdSmpteSt2084);
  EXPECT_EQ(matrix_id_, kSbMediaMatrixIdBt2020NonconstantLuminance);
}

TEST_F(ParseVideoCodecTest, InvalidVp9) {
  EXPECT_FALSE(Parse("vp09.02.10.10.01.9.16.9"));
  EXPECT_FALSE(Parse("vp09.02.10.10.01.09.16"));
  EXPECT_FALSE(Parse("vp09.02.10.10.01.09"));
  EXPECT_FALSE(Parse("vp09.02.10.10.01"));
  EXPECT_FALSE(Parse("vp09.02.10"));
  EXPECT_FALSE(Parse("vp09.02"));
  EXPECT_FALSE(Parse("vp09"));
}

}  // namespace
}  // namespace starboard
