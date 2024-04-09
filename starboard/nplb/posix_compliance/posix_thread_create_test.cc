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

#include "starboard/configuration_constants.h"
#include "starboard/nplb/posix_compliance/posix_thread_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(PosixThreadCreateTest, SunnyDay) {
  const int kTrials = 64;
  for (int i = 0; i < kTrials; ++i) {
    pthread_t thread;
    EXPECT_EQ(pthread_create(&thread, NULL, posix::AddOneEntryPoint,
                             posix::kSomeContext),
              0);
    void* result = NULL;
    EXPECT_EQ(pthread_join(thread, &result), 0);
    EXPECT_EQ(posix::kSomeContextPlusOne, result);
  }
}

TEST(PosixThreadCreateTest, SunnyDayNoContext) {
  pthread_t thread;
  EXPECT_EQ(pthread_create(&thread, NULL, posix::AddOneEntryPoint, NULL), 0);
  void* result = NULL;
  EXPECT_EQ(pthread_join(thread, &result), 0);
  EXPECT_EQ(posix::ToVoid(1), result);
}

TEST(PosixThreadCreateTest, Summertime) {
  const int kMany = kSbMaxThreads;
  std::vector<pthread_t> threads(kMany);
  for (int i = 0; i < kMany; ++i) {
    EXPECT_EQ(pthread_create(&(threads[i]), NULL, posix::AddOneEntryPoint,
                             posix::ToVoid(i)),
              0);
  }

  for (int i = 0; i < kMany; ++i) {
    void* result = NULL;
    void* const kExpected = posix::ToVoid(i + 1);

    EXPECT_EQ(pthread_join(threads[i], &result), 0);
    EXPECT_EQ(kExpected, result);
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
