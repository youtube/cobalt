// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/media.h"

#include "starboard/nplb/performance_helpers.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbMediaConfigurationTest, ValidatePerformance) {
  TEST_PERF_FUNCNOARGS_DEFAULT(SbMediaGetAudioOutputCount);

  const int count_audio_output = SbMediaGetAudioOutputCount();
  for (int i = 0; i < count_audio_output; ++i) {
    constexpr int kNumberOfCalls = 100;
    constexpr SbTime kMaxAverageTimePerCall = 500;

    SbMediaAudioConfiguration configuration;
    TEST_PERF_FUNCWITHARGS_EXPLICIT(kNumberOfCalls, kMaxAverageTimePerCall,
                                    SbMediaGetAudioConfiguration, i,
                                    &configuration);
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
