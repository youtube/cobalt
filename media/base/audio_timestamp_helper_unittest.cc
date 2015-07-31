// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_timestamp_helper.h"
#include "media/base/buffers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const int kBytesPerFrame = 4;
static const int kDefaultSampleRate = 44100;

class AudioTimestampHelperTest : public ::testing::Test {
 public:
  AudioTimestampHelperTest()
      : helper_(kBytesPerFrame, kDefaultSampleRate) {
    helper_.SetBaseTimestamp(base::TimeDelta());
  }

  // Adds bytes to the helper and returns the current timestamp in microseconds.
  int64 AddBytes(int bytes) {
    helper_.AddBytes(bytes);
    return helper_.GetTimestamp().InMicroseconds();
  }

  int64 BytesToTarget(int target_in_microseconds) {
    return helper_.GetBytesToTarget(
        base::TimeDelta::FromMicroseconds(target_in_microseconds));
  }

  void TestGetBytesToTargetRange(int byte_count, int start, int end) {
    for (int i = start; i <= end; ++i)
      EXPECT_EQ(byte_count,BytesToTarget(i)) << " Failure for timestamp "
                                             << i << " us.";
  }

 protected:
  AudioTimestampHelper helper_;

  DISALLOW_COPY_AND_ASSIGN(AudioTimestampHelperTest);
};

TEST_F(AudioTimestampHelperTest, Basic) {
  EXPECT_EQ(0, helper_.GetTimestamp().InMicroseconds());

  // Verify that the output timestamp is always rounded down to the
  // nearest microsecond. 1 frame @ 44100 is ~22.67573 microseconds,
  // which is why the timestamp sometimes increments by 23 microseconds
  // and other times it increments by 22 microseconds.
  EXPECT_EQ(0, AddBytes(0));
  EXPECT_EQ(22, AddBytes(kBytesPerFrame));
  EXPECT_EQ(45, AddBytes(kBytesPerFrame));
  EXPECT_EQ(68, AddBytes(kBytesPerFrame));
  EXPECT_EQ(90, AddBytes(kBytesPerFrame));
  EXPECT_EQ(113, AddBytes(kBytesPerFrame));

  // Verify that adding bytes one frame at a time matches the timestamp returned
  // if the same number of bytes are added all at once.
  base::TimeDelta timestamp_1  = helper_.GetTimestamp();
  helper_.SetBaseTimestamp(kNoTimestamp());
  EXPECT_TRUE(kNoTimestamp() == helper_.base_timestamp());
  helper_.SetBaseTimestamp(base::TimeDelta());
  EXPECT_EQ(0, helper_.GetTimestamp().InMicroseconds());

  helper_.AddBytes(5 * kBytesPerFrame);
  EXPECT_EQ(113, helper_.GetTimestamp().InMicroseconds());
  EXPECT_TRUE(timestamp_1 == helper_.GetTimestamp());
}


TEST_F(AudioTimestampHelperTest, GetDuration) {
  helper_.SetBaseTimestamp(base::TimeDelta::FromMicroseconds(100));

  int byte_count = 5 * kBytesPerFrame;
  int64 expected_durations[] = { 113, 113, 114, 113, 113, 114 };
  for (size_t i = 0; i < arraysize(expected_durations); ++i) {
    base::TimeDelta duration = helper_.GetDuration(byte_count);
    EXPECT_EQ(expected_durations[i], duration.InMicroseconds());

    base::TimeDelta timestamp_1 = helper_.GetTimestamp() + duration;
    helper_.AddBytes(byte_count);
    base::TimeDelta timestamp_2 = helper_.GetTimestamp();
    EXPECT_TRUE(timestamp_1 == timestamp_2);
  }
}

TEST_F(AudioTimestampHelperTest, GetBytesToTarget) {
  // Verify GetBytesToTarget() rounding behavior.
  // 1 frame @ 44100 is ~22.67573 microseconds,

  // Test values less than half of the frame duration.
  TestGetBytesToTargetRange(0, 0, 11);

  // Test values between half the frame duration & the
  // full frame duration.
  TestGetBytesToTargetRange(kBytesPerFrame, 12, 22);

  // Verify that the same number of bytes is returned up
  // to the next half a frame.
  TestGetBytesToTargetRange(kBytesPerFrame, 23, 34);

  // Verify the next 3 ranges.
  TestGetBytesToTargetRange(2 * kBytesPerFrame, 35, 56);
  TestGetBytesToTargetRange(3 * kBytesPerFrame, 57, 79);
  TestGetBytesToTargetRange(4 * kBytesPerFrame, 80, 102);
  TestGetBytesToTargetRange(5 * kBytesPerFrame, 103, 124);


  // Add bytes to the helper so negative byte counts can
  // be tested.
  helper_.AddBytes(5 * kBytesPerFrame);

  // Note: The timestamp ranges must match the positive values
  // tested above to verify that the code is rounding properly.
  TestGetBytesToTargetRange(0 * kBytesPerFrame, 103, 124);
  TestGetBytesToTargetRange(-1 * kBytesPerFrame, 80, 102);
  TestGetBytesToTargetRange(-2 * kBytesPerFrame, 57, 79);
  TestGetBytesToTargetRange(-3 * kBytesPerFrame, 35, 56);
  TestGetBytesToTargetRange(-4 * kBytesPerFrame, 12, 34);
  TestGetBytesToTargetRange(-5 * kBytesPerFrame, 0, 11);
}

}  // namespace media
