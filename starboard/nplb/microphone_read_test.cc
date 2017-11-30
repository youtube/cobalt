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
namespace {

#if SB_HAS(MICROPHONE)

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

    EXPECT_TRUE(SbMicrophoneOpen(microphone));

    int requested_bytes = info_array[0].min_read_size;
    std::vector<uint8_t> audio_data(requested_bytes, 0);
    int read_bytes =
        SbMicrophoneRead(microphone, &audio_data[0], audio_data.size());
    EXPECT_GE(read_bytes, 0);

    EXPECT_TRUE(SbMicrophoneClose(microphone));
    SbMicrophoneDestroy(microphone);
  }
}

TEST(SbMicrophoneReadTest, SunnyDayReadIsLargerThanMinReadSize) {
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

    EXPECT_TRUE(SbMicrophoneOpen(microphone));

    int requested_bytes = info_array[0].min_read_size;
    std::vector<uint8_t> audio_data(requested_bytes * 2, 0);
    int read_bytes =
        SbMicrophoneRead(microphone, &audio_data[0], audio_data.size());
    EXPECT_GE(read_bytes, 0);

    EXPECT_TRUE(SbMicrophoneClose(microphone));
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

    EXPECT_TRUE(SbMicrophoneOpen(microphone));

    SbThreadSleep(50 * kSbTimeMillisecond);

    EXPECT_TRUE(SbMicrophoneClose(microphone));
    EXPECT_TRUE(SbMicrophoneOpen(microphone));

    int requested_bytes = info_array[0].min_read_size;
    std::vector<uint8_t> audio_data(requested_bytes, 0);
    int read_bytes =
        SbMicrophoneRead(microphone, &audio_data[0], audio_data.size());
    EXPECT_GE(read_bytes, 0);
    EXPECT_LT(read_bytes, audio_data.size());

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

    EXPECT_TRUE(SbMicrophoneOpen(microphone));

    int read_bytes = SbMicrophoneRead(microphone, NULL, 0);
    EXPECT_EQ(read_bytes, 0);

    EXPECT_TRUE(SbMicrophoneClose(microphone));
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

    EXPECT_TRUE(SbMicrophoneOpen(microphone));

    int read_bytes = SbMicrophoneRead(microphone, NULL, 1024);
    EXPECT_LE(read_bytes, 0);

    EXPECT_TRUE(SbMicrophoneClose(microphone));
    SbMicrophoneDestroy(microphone);
  }
}

TEST(SbMicrophoneReadTest, RainyDayAudioBufferSizeIsSmallerThanMinReadSize) {
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

    EXPECT_TRUE(SbMicrophoneOpen(microphone));

    int requested_bytes = info_array[0].min_read_size;
    std::vector<uint8_t> audio_data(requested_bytes / 2, 0);
    int read_bytes =
        SbMicrophoneRead(microphone, &audio_data[0], audio_data.size());
    EXPECT_GE(read_bytes, 0);

    EXPECT_TRUE(SbMicrophoneClose(microphone));
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

    int requested_bytes = info_array[0].min_read_size;
    std::vector<uint8_t> audio_data(requested_bytes, 0);
    int read_bytes =
        SbMicrophoneRead(microphone, &audio_data[0], audio_data.size());
    // An error should have occurred because open was not called.
    EXPECT_LT(read_bytes, 0);

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

    EXPECT_TRUE(SbMicrophoneOpen(microphone));
    EXPECT_TRUE(SbMicrophoneClose(microphone));

    int requested_bytes = info_array[0].min_read_size;
    std::vector<uint8_t> audio_data(requested_bytes, 0);
    int read_bytes =
        SbMicrophoneRead(microphone, &audio_data[0], audio_data.size());
    // An error should have occurred because the microphone was closed.
    EXPECT_LT(read_bytes, 0);

    SbMicrophoneDestroy(microphone);
  }
}

TEST(SbMicrophoneReadTest, RainyDayMicrophoneIsInvalid) {
  std::vector<uint8_t> audio_data(1024, 0);
  int read_bytes =
      SbMicrophoneRead(kSbMicrophoneInvalid, &audio_data[0], audio_data.size());
  EXPECT_LT(read_bytes, 0);
}

#endif  // SB_HAS(MICROPHONE)

}  // namespace
}  // namespace nplb
}  // namespace starboard
