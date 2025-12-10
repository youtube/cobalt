// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_codecs.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

#if BUILDFLAG(ENABLE_PLATFORM_IAMF_AUDIO)
TEST(ParseIamfCodecIdTest, IamfCodecIds) {
  EXPECT_TRUE(ParseIamfCodecId("iamf.000.000.opus", nullptr, nullptr));
  EXPECT_TRUE(ParseIamfCodecId("iamf.000.000.Opus", nullptr, nullptr));

  EXPECT_TRUE(ParseIamfCodecId("iamf.000.000.mp4a.40.2", nullptr, nullptr));

  EXPECT_TRUE(ParseIamfCodecId("iamf.000.000.flac", nullptr, nullptr));
  EXPECT_TRUE(ParseIamfCodecId("iamf.000.000.fLaC", nullptr, nullptr));

  EXPECT_TRUE(ParseIamfCodecId("iamf.000.000.ipcm", nullptr, nullptr));

  uint8_t primary_profile;
  uint8_t additional_profile;

  EXPECT_TRUE(ParseIamfCodecId("iamf.000.000.ipcm", &primary_profile,
                               &additional_profile));
  EXPECT_EQ(primary_profile, 0x00);
  EXPECT_EQ(additional_profile, 0x00);

  EXPECT_TRUE(ParseIamfCodecId("iamf.001.001.ipcm", &primary_profile,
                               &additional_profile));
  EXPECT_EQ(primary_profile, 0x01);
  EXPECT_EQ(additional_profile, 0x01);

  EXPECT_TRUE(ParseIamfCodecId("iamf.000.001.ipcm", &primary_profile,
                               &additional_profile));
  EXPECT_EQ(primary_profile, 0x00);
  EXPECT_EQ(additional_profile, 0x01);

  EXPECT_TRUE(ParseIamfCodecId("iamf.001.000.ipcm", &primary_profile,
                               &additional_profile));
  EXPECT_EQ(primary_profile, 0x01);
  EXPECT_EQ(additional_profile, 0x00);

  EXPECT_TRUE(ParseIamfCodecId("iamf.001.002.ipcm", &primary_profile,
                               &additional_profile));
  EXPECT_EQ(primary_profile, 0x01);
  EXPECT_EQ(additional_profile, 0x02);

  EXPECT_TRUE(ParseIamfCodecId("iamf.255.255.ipcm", &primary_profile,
                               &additional_profile));
  EXPECT_EQ(primary_profile, 0xFF);
  EXPECT_EQ(additional_profile, 0xFF);

  // Invalid codec strings
  EXPECT_FALSE(ParseIamfCodecId("", nullptr, nullptr));

  EXPECT_FALSE(ParseIamfCodecId("iamf.000.000.mp4a", nullptr, nullptr));
  EXPECT_FALSE(ParseIamfCodecId("iamf.000.000.mp4a.30.5", nullptr, nullptr));
  EXPECT_FALSE(ParseIamfCodecId("iamf.000.000.mp4a.40.5", nullptr, nullptr));
  EXPECT_FALSE(
      ParseIamfCodecId("iamf.000.000.000.mp4a.40.2", nullptr, nullptr));

  EXPECT_FALSE(ParseIamfCodecId("iamf.256.000.Opus", nullptr, nullptr));
  EXPECT_FALSE(ParseIamfCodecId("iamf.000.256.Opus", nullptr, nullptr));

  EXPECT_FALSE(ParseIamfCodecId("iamf.00.000.ipcm", nullptr, nullptr));
  EXPECT_FALSE(ParseIamfCodecId("iamf.000.00.ipcm", nullptr, nullptr));
  EXPECT_FALSE(ParseIamfCodecId("iamd.000.000.ipcm", nullptr, nullptr));
  EXPECT_FALSE(ParseIamfCodecId("ia.000.000.ipcm", nullptr, nullptr));
}
#endif  // BUILDFLAG(ENABLE_PLATFORM_IAMF_AUDIO)

}  // namespace media
