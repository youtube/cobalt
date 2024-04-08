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

#include <pthread.h>

#include "starboard/nplb/posix_compliance/posix_thread_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

void* EntryPoint(void* context) {
  // NOLINTNEXTLINE(readability/casting)
  pthread_t ret = pthread_self();
  return reinterpret_cast<void*>(ret);
}

TEST(PosixThreadGetCurrentTest, SunnyDay) {
  const int kThreads = 16;
  pthread_t threads[kThreads];
  for (int i = 0; i < kThreads; ++i) {
    EXPECT_EQ(pthread_create(&threads[i], NULL, EntryPoint, NULL), 0);
  }

  for (int i = 0; i < kThreads; ++i) {
    void* result = NULL;
    EXPECT_EQ(pthread_join(threads[i], &result), 0);
    // NOLINTNEXTLINE(readability/casting)
    EXPECT_EQ(posix::ToVoid((intptr_t)threads[i]), result);
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
