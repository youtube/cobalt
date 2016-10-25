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
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

#if SB_HAS(MICROPHONE) && SB_VERSION(2)

TEST(SbMicrophoneReadTest, SunnyDay) {
  SbMicrophoneInfo info_array[kMaxNumberOfMicrophone];
  int available_microphones =
      SbMicrophoneGetAvailable(info_array, kMaxNumberOfMicrophone);
  EXPECT_GE(available_microphones, 0);

  if (available_microphones != 0) {
    ASSERT_TRUE(SbMicrophoneIsSampleRateSupported(
        info_array[0].id, info_array[0].max_sample_rate_hz));
    SbMicrophone microphone = SbMicrophoneCreate(
        info_array[0].id, info_array[0].max_sample_rate_hz, kBufferSize);
    ASSERT_TRUE(SbMicrophoneIsValid(microphone));

    bool success = SbMicrophoneOpen(microphone);
    ASSERT_TRUE(success);

    void* audio_data[1024];
    int read_bytes =
        SbMicrophoneRead(microphone, audio_data, sizeof(audio_data));
    EXPECT_GE(read_bytes, 0);

    success = SbMicrophoneClose(microphone);
    EXPECT_TRUE(success);
    SbMicrophoneDestroy(microphone);
  }
}

TEST(SbMicrophoneReadTest, SunnyDayOpenSleepCloseAndOpenRead) {
  SbMicrophoneInfo info_array[kMaxNumberOfMicrophone];
  int available_microphones =
      SbMicrophoneGetAvailable(info_array, kMaxNumberOfMicrophone);
  EXPECT_GE(available_microphones, 0);

  if (available_microphones != 0) {
    ASSERT_TRUE(SbMicrophoneIsSampleRateSupported(
        info_array[0].id, info_array[0].max_sample_rate_hz));
    SbMicrophone microphone = SbMicrophoneCreate(
        info_array[0].id, info_array[0].max_sample_rate_hz, kBufferSize);
    ASSERT_TRUE(SbMicrophoneIsValid(microphone));

    bool success = SbMicrophoneOpen(microphone);
    EXPECT_TRUE(success);

    SbThreadSleep(50 * kSbTimeMillisecond);

    success = SbMicrophoneClose(microphone);
    EXPECT_TRUE(success);

    success = SbMicrophoneOpen(microphone);
    EXPECT_TRUE(success);

    void* audio_data[16 * 1024];
    int read_bytes =
        SbMicrophoneRead(microphone, audio_data, sizeof(audio_data));
    EXPECT_GE(read_bytes, 0);
    EXPECT_LT(read_bytes, sizeof(audio_data));

    SbMicrophoneDestroy(microphone);
  }
}

TEST(SbMicrophoneReadTest, RainyDayAudioBufferIsNULL) {
  SbMicrophoneInfo info_array[kMaxNumberOfMicrophone];
  int available_microphones =
      SbMicrophoneGetAvailable(info_array, kMaxNumberOfMicrophone);
  EXPECT_GE(available_microphones, 0);

  if (available_microphones != 0) {
    ASSERT_TRUE(SbMicrophoneIsSampleRateSupported(
        info_array[0].id, info_array[0].max_sample_rate_hz));
    SbMicrophone microphone = SbMicrophoneCreate(
        info_array[0].id, info_array[0].max_sample_rate_hz, kBufferSize);
    ASSERT_TRUE(SbMicrophoneIsValid(microphone));

    bool success = SbMicrophoneOpen(microphone);
    ASSERT_TRUE(success);

    int read_bytes = SbMicrophoneRead(microphone, NULL, 0);
    EXPECT_EQ(read_bytes, 0);

    success = SbMicrophoneClose(microphone);
    EXPECT_TRUE(success);
    SbMicrophoneDestroy(microphone);
  }
}

TEST(SbMicrophoneReadTest, RainyDayAudioBufferSizeIsSmallerThanRequestedSize) {
  SbMicrophoneInfo info_array[kMaxNumberOfMicrophone];
  int available_microphones =
      SbMicrophoneGetAvailable(info_array, kMaxNumberOfMicrophone);
  EXPECT_GE(available_microphones, 0);

  if (available_microphones != 0) {
    ASSERT_TRUE(SbMicrophoneIsSampleRateSupported(
        info_array[0].id, info_array[0].max_sample_rate_hz));
    SbMicrophone microphone = SbMicrophoneCreate(
        info_array[0].id, info_array[0].max_sample_rate_hz, kBufferSize);
    ASSERT_TRUE(SbMicrophoneIsValid(microphone));

    bool success = SbMicrophoneOpen(microphone);
    ASSERT_TRUE(success);

    int read_bytes = SbMicrophoneRead(microphone, NULL, 1024);
    EXPECT_LE(read_bytes, 0);

    success = SbMicrophoneClose(microphone);
    EXPECT_TRUE(success);
    SbMicrophoneDestroy(microphone);
  }
}

TEST(SbMicrophoneReadTest, RainyDayOpenIsNotCalled) {
  SbMicrophoneInfo info_array[kMaxNumberOfMicrophone];
  int available_microphones =
      SbMicrophoneGetAvailable(info_array, kMaxNumberOfMicrophone);
  EXPECT_GE(available_microphones, 0);

  if (available_microphones != 0) {
    ASSERT_TRUE(SbMicrophoneIsSampleRateSupported(
        info_array[0].id, info_array[0].max_sample_rate_hz));
    SbMicrophone microphone = SbMicrophoneCreate(
        info_array[0].id, info_array[0].max_sample_rate_hz, kBufferSize);
    ASSERT_TRUE(SbMicrophoneIsValid(microphone));

    void* audio_data[1024];
    int read_bytes =
        SbMicrophoneRead(microphone, audio_data, sizeof(audio_data));
    EXPECT_EQ(read_bytes, 0);

    SbMicrophoneDestroy(microphone);
  }
}

TEST(SbMicrophoneReadTest, RainyDayOpenCloseAndRead) {
  SbMicrophoneInfo info_array[kMaxNumberOfMicrophone];
  int available_microphones =
      SbMicrophoneGetAvailable(info_array, kMaxNumberOfMicrophone);
  EXPECT_GE(available_microphones, 0);

  if (available_microphones != 0) {
    ASSERT_TRUE(SbMicrophoneIsSampleRateSupported(
        info_array[0].id, info_array[0].max_sample_rate_hz));
    SbMicrophone microphone = SbMicrophoneCreate(
        info_array[0].id, info_array[0].max_sample_rate_hz, kBufferSize);
    ASSERT_TRUE(SbMicrophoneIsValid(microphone));

    bool success = SbMicrophoneOpen(microphone);
    EXPECT_TRUE(success);

    success = SbMicrophoneClose(microphone);
    EXPECT_TRUE(success);

    void* audio_data[1024];
    int read_bytes =
        SbMicrophoneRead(microphone, audio_data, sizeof(audio_data));
    // No data can be read.
    EXPECT_EQ(read_bytes, 0);

    SbMicrophoneDestroy(microphone);
  }
}

TEST(SbMicrophoneReadTest, RainyDayMicrophoneIsInvalid) {
  void* audio_data[1024];
  int read_bytes =
      SbMicrophoneRead(kSbMicrophoneInvalid, audio_data, sizeof(audio_data));
  EXPECT_LT(read_bytes, 0);
}

#endif  // SB_HAS(MICROPHONE) && SB_VERSION(2)

}  // namespace nplb
}  // namespace starboard
