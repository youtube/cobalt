// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

const int kMaxAllowedMinRequiredFrames = 16 * 1024;

TEST(SbAudioSinkGetMinBufferSizeInFramesTest, SunnyDay) {
  SbMediaAudioSampleType sample_type = kSbMediaAudioSampleTypeFloat32;
  if (!SbAudioSinkIsAudioSampleTypeSupported(sample_type)) {
    sample_type = kSbMediaAudioSampleTypeInt16Deprecated;
  }
  const int kDefaultSampleFrequency = 48000;
  int max_channels = SbAudioSinkGetMaxChannels();
  int min_required_frames = SbAudioSinkGetMinBufferSizeInFrames(
      max_channels, sample_type,
      SbAudioSinkGetNearestSupportedSampleFrequency(kDefaultSampleFrequency));

  EXPECT_GT(min_required_frames, 0);
  EXPECT_LE(min_required_frames, kMaxAllowedMinRequiredFrames);

  if (max_channels > 1) {
    min_required_frames = SbAudioSinkGetMinBufferSizeInFrames(
        1, sample_type,
        SbAudioSinkGetNearestSupportedSampleFrequency(kDefaultSampleFrequency));

    EXPECT_GT(min_required_frames, 0);
    EXPECT_LE(min_required_frames, kMaxAllowedMinRequiredFrames);
  }
  if (max_channels > 2) {
    min_required_frames = SbAudioSinkGetMinBufferSizeInFrames(
        2, sample_type,
        SbAudioSinkGetNearestSupportedSampleFrequency(kDefaultSampleFrequency));

    EXPECT_GT(min_required_frames, 0);
    EXPECT_LE(min_required_frames, kMaxAllowedMinRequiredFrames);
  }
}

}  // namespace nplb
}  // namespace starboard
