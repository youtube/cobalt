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

#include "starboard/common/string.h"
#include "starboard/microphone.h"
#include "starboard/nplb/microphone_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbMicrophoneGetAvailableTest, SunnyDay) {
  SbMicrophoneInfo info_array[kMaxNumberOfMicrophone];
  int available_microphones =
      SbMicrophoneGetAvailable(info_array, kMaxNumberOfMicrophone);
  EXPECT_GE(available_microphones, 0);
}

TEST(SbMicrophoneGetAvailableTest, RainyDay0NumberOfMicrophone) {
  SbMicrophoneInfo info_array[kMaxNumberOfMicrophone];
  if (SbMicrophoneGetAvailable(info_array, kMaxNumberOfMicrophone) > 0) {
    int available_microphones = SbMicrophoneGetAvailable(info_array, 0);
    EXPECT_GT(available_microphones, 0);
  }
}

TEST(SbMicrophoneGetAvailableTest, RainyDayNegativeNumberOfMicrophone) {
  SbMicrophoneInfo info_array[kMaxNumberOfMicrophone];
  if (SbMicrophoneGetAvailable(info_array, kMaxNumberOfMicrophone) > 0) {
    int available_microphones = SbMicrophoneGetAvailable(info_array, -10);
    EXPECT_GT(available_microphones, 0);
  }
}

TEST(SbMicrophoneGetAvailableTest, RainyDayNULLInfoArray) {
  SbMicrophoneInfo info_array[kMaxNumberOfMicrophone];
  if (SbMicrophoneGetAvailable(info_array, kMaxNumberOfMicrophone) > 0) {
    int available_microphones =
        SbMicrophoneGetAvailable(NULL, kMaxNumberOfMicrophone);
    EXPECT_GT(available_microphones, 0);
  }
}

template <std::size_t N>
bool IsNullTerminated(const char (&str)[N]) {
  for (size_t i = 0; i < N; ++i) {
    if (str[i] == '\0') {
      return true;
    }
  }
  return false;
}

TEST(SbMicrophoneGetAvailableTest, LabelIsNullTerminated) {
  const int kInfoSize = 10;
  SbMicrophoneInfo infos[kInfoSize];
  int count = SbMicrophoneGetAvailable(infos, kInfoSize);
  for (int i = 0; i < std::min(count, kInfoSize); ++i) {
    EXPECT_TRUE(IsNullTerminated(infos[i].label));
  }
}

TEST(SbMicrophoneGetAvailableTest, LabelIsValid) {
  const char* kPoisonLabel = "BadLabel";
  SbMicrophoneInfo info;
  starboard::strlcpy(info.label, kPoisonLabel, SB_ARRAY_SIZE(info.label));

  if (SbMicrophoneGetAvailable(&info, 1) > 0) {
    ASSERT_TRUE(IsNullTerminated(info.label));
    size_t count = static_cast<size_t>(strlen(kPoisonLabel));
    EXPECT_NE(0, strncmp(info.label, kPoisonLabel, count));
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
