// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "media/audio/audio_util.h"
#include "testing/gtest/include/gtest/gtest.h"

// Number of samples in each audio array.
static const size_t kNumberOfSamples = 4;

namespace media {

TEST(AudioUtilTest, AdjustVolume_u8) {
  // Test AdjustVolume() on 8 bit samples.
  uint8 samples_u8[kNumberOfSamples] = { 4, 0x40, 0x80, 0xff };
  uint8 expected_u8[kNumberOfSamples] = { (4 - 128) / 2 + 128,
                                          (0x40 - 128) / 2 + 128,
                                          (0x80 - 128) / 2 + 128,
                                          (0xff - 128) / 2 + 128 };
  bool result_u8 = media::AdjustVolume(samples_u8, sizeof(samples_u8),
                                       1,  // channels.
                                       sizeof(samples_u8[0]),
                                       0.5f);
  EXPECT_EQ(true, result_u8);
  int expected_test = memcmp(samples_u8, expected_u8, sizeof(expected_u8));
  EXPECT_EQ(0, expected_test);
}

TEST(AudioUtilTest, AdjustVolume_s16) {
  // Test AdjustVolume() on 16 bit samples.
  int16 samples_s16[kNumberOfSamples] = { -4, 0x40, -32768, 123 };
  int16 expected_s16[kNumberOfSamples] = { -1, 0x10, -8192, 30 };
  bool result_s16 = media::AdjustVolume(samples_s16, sizeof(samples_s16),
                                        2,  // channels.
                                        sizeof(samples_s16[0]),
                                        0.25f);
  EXPECT_EQ(true, result_s16);
  int expected_test = memcmp(samples_s16, expected_s16, sizeof(expected_s16));
  EXPECT_EQ(0, expected_test);
}

TEST(AudioUtilTest, AdjustVolume_s16_zero) {
  // Test AdjustVolume() on 16 bit samples.
  int16 samples_s16[kNumberOfSamples] = { -4, 0x40, -32768, 123 };
  int16 expected_s16[kNumberOfSamples] = { 0, 0, 0, 0 };
  bool result_s16 = media::AdjustVolume(samples_s16, sizeof(samples_s16),
                                        2,  // channels.
                                        sizeof(samples_s16[0]),
                                        0.0f);
  EXPECT_EQ(true, result_s16);
  int expected_test = memcmp(samples_s16, expected_s16, sizeof(expected_s16));
  EXPECT_EQ(0, expected_test);
}

TEST(AudioUtilTest, AdjustVolume_s16_one) {
  // Test AdjustVolume() on 16 bit samples.
  int16 samples_s16[kNumberOfSamples] = { -4, 0x40, -32768, 123 };
  int16 expected_s16[kNumberOfSamples] = { -4, 0x40, -32768, 123 };
  bool result_s16 = media::AdjustVolume(samples_s16, sizeof(samples_s16),
                                        2,  // channels.
                                        sizeof(samples_s16[0]),
                                        1.0f);
  EXPECT_EQ(true, result_s16);
  int expected_test = memcmp(samples_s16, expected_s16, sizeof(expected_s16));
  EXPECT_EQ(0, expected_test);
}

TEST(AudioUtilTest, AdjustVolume_f32) {
  // Test AdjustVolume() on 32 bit samples.
  float samples_f32[kNumberOfSamples] = { -4.0f, 0.5f, -.05f, 123.0f };
  float expected_f32[kNumberOfSamples] = { -4.0f * 0.25f,
                                           0.5f * 0.25f,
                                           -.05f * 0.25f,
                                           123.0f * 0.25f };
  bool result_f32 = media::AdjustVolume(samples_f32, sizeof(samples_f32),
                                        4,  // channels.
                                        sizeof(samples_f32[0]),
                                        0.25f);
  EXPECT_EQ(true, result_f32);
  int expected_test = memcmp(samples_f32, expected_f32, sizeof(expected_f32));
  EXPECT_EQ(0, expected_test);
}

TEST(AudioUtilTest, FoldChannels_u8) {
  // Test FoldChannels() on 8 bit samples.
  uint8 samples_u8[6] = { 130, 100, 150, 70, 130, 170 };
  uint8 expected_u8[2] = { static_cast<uint8>((130 - 128) * 0.707 +
                                              (100 - 128) + 128),
                           static_cast<uint8>((130 - 128) * 0.707 +
                                              (150 - 128) + 128) };
  bool result_u8 = media::FoldChannels(samples_u8, sizeof(samples_u8),
                                        6,  // channels.
                                        sizeof(samples_u8[0]),
                                        1.0f);
  EXPECT_EQ(true, result_u8);
  int expected_test = memcmp(samples_u8, expected_u8, sizeof(expected_u8));
  EXPECT_EQ(0, expected_test);
}

TEST(AudioUtilTest, FoldChannels_s16) {
  // Test FoldChannels() on 16 bit samples.
  int16 samples_s16[6] = { 12, 1, 3, 7, 13, 17 };
  int16 expected_s16[2] = { static_cast<int16>(12 * .707 + 1),
                            static_cast<int16>(12 * .707 + 3) };
  bool result_s16 = media::FoldChannels(samples_s16, sizeof(samples_s16),
                                        6,  // channels.
                                        sizeof(samples_s16[0]),
                                        1.00f);
  EXPECT_EQ(true, result_s16);
  int expected_test = memcmp(samples_s16, expected_s16, sizeof(expected_s16));
  EXPECT_EQ(0, expected_test);
}

TEST(AudioUtilTest, FoldChannels_f32) {
  // Test FoldChannels() on 32 bit samples.
  float samples_f32[6] = { 12.0f, 1.0f, 3.0f, 7.0f, 13.0f, 17.0f };
  float expected_f32[2] = { 12.0f * .707f + 1.0f,
                            12.0f * .707f + 3.0f };
  bool result_f32 = media::FoldChannels(samples_f32, sizeof(samples_f32),
                                        6,  // channels.
                                        sizeof(samples_f32[0]),
                                        1.00f);
  EXPECT_EQ(true, result_f32);
  int expected_test = memcmp(samples_f32, expected_f32, sizeof(expected_f32));
  EXPECT_EQ(0, expected_test);
}

// This mimics 1 second of audio at 48000 samples per second.
// Running the unittest will produce timing.
TEST(AudioUtilTest, DISABLED_FoldChannels_s16_benchmark) {
  const int kBufferSize = 1024 * 6;
  // Test AdjustVolume() on 16 bit samples.
  for (int i = 0; i < 48000; ++i) {
    int16 samples_s16[kBufferSize];
    for (int j = 0; j < kBufferSize; ++j)
      samples_s16[j] = j;

    bool result_s16 = media::FoldChannels(samples_s16, sizeof(samples_s16),
                                          6,  // channels.
                                          sizeof(samples_s16[0]),
                                          0.5f);
    EXPECT_EQ(true, result_s16);
  }
}

}  // namespace media
