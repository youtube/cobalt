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

#include "starboard/audio_sink.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

TEST(SbAudioSinkIsAudioSampleTypeSupportedTest, SunnyDay) {
  bool int16_supported = SbAudioSinkIsAudioSampleTypeSupported(
      kSbMediaAudioSampleTypeInt16Deprecated);
  bool float32_supported =
      SbAudioSinkIsAudioSampleTypeSupported(kSbMediaAudioSampleTypeFloat32);
  // A platform must support at least one of the sample types.
  EXPECT_TRUE(int16_supported || float32_supported);
}

}  // namespace nplb
}  // namespace starboard
