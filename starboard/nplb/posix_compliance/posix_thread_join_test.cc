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

// Thread joining is mostly tested in the other tests.

#include <pthread.h>
#include <unistd.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

void* ThreadEntryPoint(void* input) {
  int* value = static_cast<int*>(input);
  static const int64_t kSleepTime = 10'000;  // 10 ms.
  // Wait to write the value to increase likelihood of catching
  // a race condition.
  usleep(kSleepTime);
  (*value)++;
  return NULL;
}

// Tests the expectation that pthread_join() will block until
// the thread function has been run.
TEST(PosixThreadLocalValueTest, ThreadJoinWaitsForFunctionRun) {
  // Try to increase likelihood of a race condition by running multiple times.
  for (int i = 0; i < 10; ++i) {
    int num_times_thread_entry_point_run = 0;
    pthread_t thread;
    EXPECT_EQ(pthread_create(&thread, NULL, ThreadEntryPoint,
                             &num_times_thread_entry_point_run),
              0);

    ASSERT_EQ(pthread_join(thread, NULL), 0);

    ASSERT_EQ(1, num_times_thread_entry_point_run)
        << "Expected pthread_join() to be blocked until ThreadFunction runs.";
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
