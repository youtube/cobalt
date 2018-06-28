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

#include "starboard/audio_sink.h"

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

namespace {
const int kFrequencyCD = 44100;
}  // namespace

TEST(SbAudioSinkGetNearestSupportedSampleFrequencyTest, SunnyDay) {
  int nearest_frequency =
      SbAudioSinkGetNearestSupportedSampleFrequency(kFrequencyCD);
  EXPECT_GT(nearest_frequency, 0);
  EXPECT_EQ(nearest_frequency,
            SbAudioSinkGetNearestSupportedSampleFrequency(nearest_frequency));
}

TEST(SbAudioSinkGetNearestSupportedSampleFrequencyTest, ZeroFrequency) {
  int nearest_frequency = SbAudioSinkGetNearestSupportedSampleFrequency(0);
  EXPECT_GT(nearest_frequency, 0);
}

TEST(SbAudioSinkGetNearestSupportedSampleFrequencyTest, SnapUp) {
  int nearest_frequency = SbAudioSinkGetNearestSupportedSampleFrequency(1);
  EXPECT_GT(nearest_frequency, 0);
  if (nearest_frequency > 1) {
    EXPECT_EQ(nearest_frequency,
              SbAudioSinkGetNearestSupportedSampleFrequency(2));
  }
}

TEST(SbAudioSinkGetNearestSupportedSampleFrequencyTest, Snap) {
  int nearest_frequency = SbAudioSinkGetNearestSupportedSampleFrequency(
      std::numeric_limits<int>::max());
  if (nearest_frequency < std::numeric_limits<int>::max()) {
    EXPECT_EQ(nearest_frequency, SbAudioSinkGetNearestSupportedSampleFrequency(
                                     std::numeric_limits<int>::max() - 1));
  }
}

}  // namespace nplb
}  // namespace starboard
