// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/configuration_constants.h"
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbThreadPriorityTest, SunnyDay) {
  SbThreadPriority priority = kSbThreadPriorityNormal;

  if (kSbHasThreadPrioritySupport) {
    // The test only lower the priority as raising the priority
    // requires permissions.
    EXPECT_TRUE(SbThreadSetPriority(kSbThreadPriorityNormal));
    EXPECT_TRUE(SbThreadGetPriority(&priority));
    EXPECT_EQ(priority, kSbThreadPriorityNormal);

    EXPECT_TRUE(SbThreadSetPriority(kSbThreadPriorityLow));
    EXPECT_TRUE(SbThreadGetPriority(&priority));
    EXPECT_EQ(priority, kSbThreadPriorityLow);

    EXPECT_TRUE(SbThreadSetPriority(kSbThreadPriorityLowest));
    EXPECT_TRUE(SbThreadGetPriority(&priority));
    EXPECT_EQ(priority, kSbThreadPriorityLowest);
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
