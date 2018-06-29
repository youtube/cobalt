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

#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbSystemGetTotalGPUMemoryTest, SunnyDay) {
  if (SbSystemHasCapability(kSbSystemCapabilityCanQueryGPUMemoryStats)) {
    // If we claim to have GPU memory reporting capabilities, then this value
    // should be larger than 0.
    EXPECT_LT(0, SbSystemGetTotalGPUMemory());
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
