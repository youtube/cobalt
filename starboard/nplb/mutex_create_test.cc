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

#include "starboard/configuration.h"
#include "starboard/mutex.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_API_VERSION >= 12
#include "starboard/condition_variable.h"
#include "starboard/once.h"
#endif  // SB_API_VERSION >= 12

namespace starboard {
namespace nplb {
namespace {

const int kALot = 128 * 1024;
const int kABunch = 2 * 1024;

TEST(SbMutexCreateTest, SunnyDay) {
  SbMutex mutex;
  EXPECT_TRUE(SbMutexCreate(&mutex));
  EXPECT_TRUE(SbMutexDestroy(&mutex));
}

TEST(SbMutexCreateTest, SunnyDayAutoInit) {
  SbMutex mutex = SB_MUTEX_INITIALIZER;
  EXPECT_TRUE(SbMutexCreate(&mutex));
  EXPECT_TRUE(SbMutexDestroy(&mutex));
}

TEST(SbMutexCreateTest, SunnyDayALot) {
  for (int i = 0; i < kALot; ++i) {
    SbMutex mutex;
    EXPECT_TRUE(SbMutexCreate(&mutex));
    EXPECT_TRUE(SbMutexDestroy(&mutex));
  }
}

TEST(SbMutexCreateTest, SunnyDayABunchAtOnce) {
  SbMutex mutices[kABunch];
  for (int i = 0; i < kABunch; ++i) {
    EXPECT_TRUE(SbMutexCreate(&mutices[i]));
  }

  for (int i = 0; i < kABunch; ++i) {
    EXPECT_TRUE(SbMutexDestroy(&mutices[i]));
  }
}

TEST(SbMutexCreateTest, RainyDayNull) {
  EXPECT_FALSE(SbMutexCreate(NULL));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
