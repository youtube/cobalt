// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include <pthread.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include "starboard/common/gettid.h"
#include "starboard/nplb/posix_compliance/posix_thread_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

// Helper entry point that returns the thread's TID.
void* GetThreadIdEntryPoint(void* context) {
  return ToVoid(gettid());
}

TEST(PosixGettidTest, SunnyDay) {
  pid_t tid = gettid();
  EXPECT_GT(tid, 0);
}

TEST(PosixGettidTest, SunnyDayDifferentIds) {
  const int kThreads = 16;
  pthread_t threads[kThreads];

  for (int i = 0; i < kThreads; ++i) {
    EXPECT_EQ(
        pthread_create(&threads[i], nullptr, GetThreadIdEntryPoint, nullptr),
        0);
    EXPECT_TRUE(threads[i] != 0);
  }

  pid_t main_tid = gettid();
  pid_t thread_tids[kThreads];

  for (int i = 0; i < kThreads; ++i) {
    void* result = nullptr;
    EXPECT_EQ(pthread_join(threads[i], &result), 0);
    pid_t tid = static_cast<pid_t>(FromVoid(result));

    // Child thread TID should not equal the main thread TID.
    EXPECT_NE(tid, main_tid);
    EXPECT_GT(tid, 0);
    thread_tids[i] = tid;
  }

  // Verify all child thread TIDs are mutually unique.
  for (int i = 0; i < kThreads; ++i) {
    for (int j = i + 1; j < kThreads; ++j) {
      EXPECT_NE(thread_tids[i], thread_tids[j]);
    }
  }
}

}  // namespace
}  // namespace nplb
