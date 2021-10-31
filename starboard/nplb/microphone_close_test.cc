// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

#if SB_API_VERSION >= 12 || SB_HAS(MICROPHONE)

TEST(SbMicrophoneCloseTest, SunnyDayCloseAreCalledMultipleTimes) {
  SbMicrophoneInfo info_array[kMaxNumberOfMicrophone];
  int available_info_array =
      SbMicrophoneGetAvailable(info_array, kMaxNumberOfMicrophone);
  EXPECT_GE(available_info_array, 0);

  if (available_info_array != 0) {
    ASSERT_TRUE(SbMicrophoneIsSampleRateSupported(
        info_array[0].id, info_array[0].max_sample_rate_hz));
    SbMicrophone microphone = SbMicrophoneCreate(
        info_array[0].id, info_array[0].max_sample_rate_hz, kBufferSize);
    ASSERT_TRUE(SbMicrophoneIsValid(microphone));
    bool success = SbMicrophoneOpen(microphone);
    EXPECT_TRUE(success);

    success = SbMicrophoneClose(microphone);
    EXPECT_TRUE(success);

    success = SbMicrophoneClose(microphone);
    EXPECT_TRUE(success);

    success = SbMicrophoneClose(microphone);
    EXPECT_TRUE(success);

    SbMicrophoneDestroy(microphone);
  }
}

TEST(SbMicrophoneCloseTest, SunnyDayOpenIsNotCalled) {
  SbMicrophoneInfo info_array[kMaxNumberOfMicrophone];
  int available_info_array =
      SbMicrophoneGetAvailable(info_array, kMaxNumberOfMicrophone);
  EXPECT_GE(available_info_array, 0);

  if (available_info_array != 0) {
    ASSERT_TRUE(SbMicrophoneIsSampleRateSupported(
        info_array[0].id, info_array[0].max_sample_rate_hz));
    SbMicrophone microphone = SbMicrophoneCreate(
        info_array[0].id, info_array[0].max_sample_rate_hz, kBufferSize);
    ASSERT_TRUE(SbMicrophoneIsValid(microphone));

    bool success = SbMicrophoneClose(microphone);
    EXPECT_TRUE(success);
    SbMicrophoneDestroy(microphone);
  }
}

TEST(SbMicrophoneCloseTest, RainyDayCloseWithInvalidMicrophone) {
  bool success = SbMicrophoneClose(kSbMicrophoneInvalid);
  EXPECT_FALSE(success);
}

#endif  // SB_API_VERSION >= 12 ||
        // SB_HAS(MICROPHONE)

}  // namespace
}  // namespace nplb
}  // namespace starboard
