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
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

TEST(SbAudioSinkIsAudioSampleTypeSupportedTest, SunnyDay) {
  bool float32_supported =
      SbAudioSinkIsAudioSampleTypeSupported(kSbMediaAudioSampleTypeFloat32);
  // All platforms must support the float32 sample type.
  EXPECT_TRUE(float32_supported);

  // It's ok for a platform to not support int16 sample type, but the call with
  // `kSbMediaAudioSampleTypeInt16Deprecated` as a parameter shouldn't crash.
  SbAudioSinkIsAudioSampleTypeSupported(kSbMediaAudioSampleTypeInt16Deprecated);
}

}  // namespace nplb
}  // namespace starboard
