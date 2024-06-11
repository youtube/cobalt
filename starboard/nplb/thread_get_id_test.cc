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

#include "starboard/nplb/posix_compliance/posix_thread_helpers.h"
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// Returns the thread's ID.
void* GetThreadIdEntryPoint(void* context) {
  return posix::ToVoid(SbThreadGetId());
}

TEST(SbThreadGetIdTest, SunnyDay) {
  EXPECT_NE(kSbThreadInvalidId, SbThreadGetId());
}

TEST(SbThreadGetIdTest, SunnyDayDifferentIds) {
  const int kThreads = 16;

  pthread_t threads[kThreads];
  for (int i = 0; i < kThreads; ++i) {
    pthread_create(&threads[i], nullptr, GetThreadIdEntryPoint, nullptr);
    EXPECT_TRUE(threads[i] != 0);
  }

  // We join on all these threads to get their IDs back.
  SbThreadId thread_ids[kThreads];
  for (int i = 0; i < kThreads; ++i) {
    void* result = NULL;
    EXPECT_EQ(pthread_join(threads[i], &result), 0);
    SbThreadId id = static_cast<SbThreadId>(posix::FromVoid(result));
    EXPECT_NE(id, SbThreadGetId());
    thread_ids[i] = id;
  }

  // Check every thread ID against every other thread ID. None should be equal.
  for (int i = 0; i < kThreads; ++i) {
    for (int j = i + 1; j < kThreads; ++j) {
      EXPECT_NE(thread_ids[i], thread_ids[j]);
    }
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
