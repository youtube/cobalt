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

// Thread joining is mostly tested in the other tests.

#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbThreadJoinTest, RainyDayInvalid) {
  void* result = NULL;
  EXPECT_FALSE(SbThreadJoin(kSbThreadInvalid, NULL));
  EXPECT_FALSE(SbThreadJoin(kSbThreadInvalid, &result));
  EXPECT_EQ(NULL, result);
}

// Tests the expectation that SbThreadJoin() will block until
// the thread function has been run.
TEST(SbThreadLocalValueTest, ThreadJoinWaitsForFunctionRun) {
  // Thread functionality needs to bind to functions. In C++11 we'd use a
  // lambda function to tie everything together locally, but this
  // function-scoped struct with static function emulates this functionality
  // pretty well.
  struct LocalStatic {
    static void* ThreadEntryPoint(void* input) {
      int* value = static_cast<int*>(input);
      static const SbTime kSleepTime = 10*kSbTimeMillisecond;  // 10 ms.
      // Wait to write the value to increase likelyhood of catching
      // a race condition.
      SbThreadSleep(kSleepTime);
      (*value)++;
      return NULL;
    }
  };

  // Try to increase likelyhood of a race condition by running multiple times.
  for (int i = 0; i < 10; ++i) {
    int num_times_thread_entry_point_run = 0;
    SbThread thread = SbThreadCreate(
        0,                    // Signals automatic thread stack size.
        kSbThreadNoPriority,  // Signals default priority.
        kSbThreadNoAffinity,  // Signals default affinity.
        true,                 // joinable thread.
        "TestThread",
        LocalStatic::ThreadEntryPoint,
        &num_times_thread_entry_point_run);

    ASSERT_NE(kSbThreadInvalid, thread) << "Thread creation not successful";
    ASSERT_TRUE(SbThreadJoin(thread, NULL));

    ASSERT_EQ(1, num_times_thread_entry_point_run)
        << "Expected SbThreadJoin() to be blocked until ThreadFunction runs.";
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
