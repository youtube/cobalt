// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "starboard/microphone.h"
#include "starboard/nplb/microphone_helpers.h"
#include "starboard/types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

#if SB_HAS(MICROPHONE)

TEST(SbMicrophoneCreateTest, SunnyDayOnlyOneMicrophone) {
  SbMicrophoneInfo info_array[kMaxNumberOfMicrophone];
  int available_microphones =
      SbMicrophoneGetAvailable(info_array, kMaxNumberOfMicrophone);
  EXPECT_GE(available_microphones, 0);

  if (available_microphones != 0) {
    ASSERT_TRUE(SbMicrophoneIsSampleRateSupported(
        info_array[0].id, info_array[0].max_sample_rate_hz));
    SbMicrophone microphone = SbMicrophoneCreate(
        info_array[0].id, info_array[0].max_sample_rate_hz, kBufferSize);
    EXPECT_TRUE(SbMicrophoneIsValid(microphone));
    SbMicrophoneDestroy(microphone);
  }
}

TEST(SbMicrophoneCreateTest, RainyDayOneMicrophoneIsCreatedMultipleTimes) {
  SbMicrophoneInfo info_array[kMaxNumberOfMicrophone];
  int available_microphones =
      SbMicrophoneGetAvailable(info_array, kMaxNumberOfMicrophone);
  EXPECT_GE(available_microphones, 0);

  if (available_microphones != 0) {
    ASSERT_TRUE(SbMicrophoneIsSampleRateSupported(
        info_array[0].id, info_array[0].max_sample_rate_hz));
    SbMicrophone microphone = SbMicrophoneCreate(
        info_array[0].id, info_array[0].max_sample_rate_hz, kBufferSize);
    EXPECT_TRUE(SbMicrophoneIsValid(microphone));

    SbMicrophone microphone_1 = SbMicrophoneCreate(
        info_array[0].id, info_array[0].max_sample_rate_hz, kBufferSize);
    EXPECT_FALSE(SbMicrophoneIsValid(microphone_1));

    SbMicrophone microphone_2 = SbMicrophoneCreate(
        info_array[0].id, info_array[0].max_sample_rate_hz, kBufferSize);
    EXPECT_FALSE(SbMicrophoneIsValid(microphone_2));

    SbMicrophoneDestroy(microphone);
  }
}

TEST(SbMicrophoneCreateTest, RainyDayInvalidMicrophoneId) {
  SbMicrophone microphone = SbMicrophoneCreate(
      kSbMicrophoneIdInvalid, kNormallyUsedSampleRateInHz, kBufferSize);
  EXPECT_FALSE(SbMicrophoneIsValid(microphone));
}

TEST(SbMicrophoneCreateTest, RainyDayInvalidFrequency_0) {
  SbMicrophoneInfo info_array[kMaxNumberOfMicrophone];
  int available_microphones =
      SbMicrophoneGetAvailable(info_array, kMaxNumberOfMicrophone);
  EXPECT_GE(available_microphones, 0);

  if (available_microphones != 0) {
    ASSERT_FALSE(SbMicrophoneIsSampleRateSupported(info_array[0].id, 0));
    SbMicrophone microphone =
        SbMicrophoneCreate(info_array[0].id, 0, kBufferSize);
    EXPECT_FALSE(SbMicrophoneIsValid(microphone));
  }
}

TEST(SbMicrophoneCreateTest, RainyDayInvalidFrequency_Negative) {
  SbMicrophoneInfo info_array[kMaxNumberOfMicrophone];
  int available_microphones =
      SbMicrophoneGetAvailable(info_array, kMaxNumberOfMicrophone);
  EXPECT_GE(available_microphones, 0);

  if (available_microphones != 0) {
    ASSERT_FALSE(SbMicrophoneIsSampleRateSupported(info_array[0].id, -8000));
    SbMicrophone microphone =
        SbMicrophoneCreate(info_array[0].id, -8000, kBufferSize);
    EXPECT_FALSE(SbMicrophoneIsValid(microphone));
  }
}

TEST(SbMicrophoneCreateTest, RainyDayInvalidFrequency_MoreThanMax) {
  SbMicrophoneInfo info_array[kMaxNumberOfMicrophone];
  int available_microphones =
      SbMicrophoneGetAvailable(info_array, kMaxNumberOfMicrophone);
  EXPECT_GE(available_microphones, 0);

  if (available_microphones != 0) {
    ASSERT_FALSE(SbMicrophoneIsSampleRateSupported(
        info_array[0].id, info_array[0].max_sample_rate_hz + 1));
    SbMicrophone microphone = SbMicrophoneCreate(
        info_array[0].id, info_array[0].max_sample_rate_hz + 1, kBufferSize);
    EXPECT_FALSE(SbMicrophoneIsValid(microphone));
  }
}

TEST(SbMicrophoneCreateTest, RainyDayInvalidBufferSize_0) {
  SbMicrophoneInfo info_array[kMaxNumberOfMicrophone];
  int available_microphones =
      SbMicrophoneGetAvailable(info_array, kMaxNumberOfMicrophone);
  EXPECT_GE(available_microphones, 0);

  if (available_microphones != 0) {
    ASSERT_TRUE(SbMicrophoneIsSampleRateSupported(
        info_array[0].id, info_array[0].max_sample_rate_hz));
    SbMicrophone microphone = SbMicrophoneCreate(
        info_array[0].id, info_array[0].max_sample_rate_hz, 0);
    EXPECT_FALSE(SbMicrophoneIsValid(microphone));
    SbMicrophoneDestroy(microphone);
  }
}

TEST(SbMicrophoneCreateTest, SunnyDayBufferSize_1) {
  SbMicrophoneInfo info_array[kMaxNumberOfMicrophone];
  int available_microphones =
      SbMicrophoneGetAvailable(info_array, kMaxNumberOfMicrophone);
  EXPECT_GE(available_microphones, 0);

  if (available_microphones != 0) {
    ASSERT_TRUE(SbMicrophoneIsSampleRateSupported(
        info_array[0].id, info_array[0].max_sample_rate_hz));
    SbMicrophone microphone = SbMicrophoneCreate(
        info_array[0].id, info_array[0].max_sample_rate_hz, 1);
    EXPECT_TRUE(SbMicrophoneIsValid(microphone));
    SbMicrophoneDestroy(microphone);
  }
}

TEST(SbMicrophoneCreateTest, RainyDayInvalidBufferSize_Negative) {
  SbMicrophoneInfo info_array[kMaxNumberOfMicrophone];
  int available_microphones =
      SbMicrophoneGetAvailable(info_array, kMaxNumberOfMicrophone);
  EXPECT_GE(available_microphones, 0);

  if (available_microphones != 0) {
    ASSERT_TRUE(SbMicrophoneIsSampleRateSupported(
        info_array[0].id, info_array[0].max_sample_rate_hz));
    SbMicrophone microphone = SbMicrophoneCreate(
        info_array[0].id, info_array[0].max_sample_rate_hz, -1024);
    EXPECT_FALSE(SbMicrophoneIsValid(microphone));
    SbMicrophoneDestroy(microphone);
  }
}

TEST(SbMicrophoneCreateTest, RainyDayInvalidBufferSize_2G) {
  SbMicrophoneInfo info_array[kMaxNumberOfMicrophone];
  int available_microphones =
      SbMicrophoneGetAvailable(info_array, kMaxNumberOfMicrophone);
  EXPECT_GE(available_microphones, 0);

  if (available_microphones != 0) {
    ASSERT_TRUE(SbMicrophoneIsSampleRateSupported(
        info_array[0].id, info_array[0].max_sample_rate_hz));
    // Create a microphone with 2 gigabytes buffer size.
    int64_t kBufferSize64 = 2 * 1024 * 1024 * 1024LL;
    // This should wrap around to a negative value due to int
    // narrowing.
    int kBufferSize = static_cast<int>(kBufferSize64);
    SbMicrophone microphone =
        SbMicrophoneCreate(info_array[0].id, info_array[0].max_sample_rate_hz,
                           kBufferSize);
    EXPECT_FALSE(SbMicrophoneIsValid(microphone));
    SbMicrophoneDestroy(microphone);
  }
}

#endif  // SB_HAS(MICROPHONE)

}  // namespace
}  // namespace nplb
}  // namespace starboard
