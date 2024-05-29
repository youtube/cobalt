// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

// Broadcast is Sunny Day tested in most of the other SbConditionVariable tests.

#include "starboard/condition_variable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbConditionVariableBroadcastTest, SunnyDayAutoInit) {
  SbConditionVariable condition = SB_CONDITION_VARIABLE_INITIALIZER;
  EXPECT_TRUE(SbConditionVariableBroadcast(&condition));
  EXPECT_TRUE(SbConditionVariableDestroy(&condition));
}

TEST(SbConditionVariableBroadcastTest, RainyDayNull) {
  EXPECT_FALSE(SbConditionVariableBroadcast(NULL));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
