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

#include "starboard/nplb/thread_helpers.h"
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

void* EntryPoint(void* context) {
  // NOLINTNEXTLINE(readability/casting)
  return ToVoid((intptr_t)SbThreadGetCurrent());
}

TEST(SbThreadGetCurrentTest, SunnyDay) {
  const int kThreads = 16;
  SbThread threads[kThreads];
  for (int i = 0; i < kThreads; ++i) {
    threads[i] = SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity,
                                true, NULL, EntryPoint, NULL);
    EXPECT_TRUE(SbThreadIsValid(threads[i]));
  }

  for (int i = 0; i < kThreads; ++i) {
    void* result = NULL;
    EXPECT_TRUE(SbThreadJoin(threads[i], &result));
    // NOLINTNEXTLINE(readability/casting)
    EXPECT_EQ(ToVoid((intptr_t)threads[i]), result);
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
