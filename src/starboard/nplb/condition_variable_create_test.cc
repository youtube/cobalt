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

#include "starboard/common/condition_variable.h"
#include "starboard/common/mutex.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

const int kALot = 128 * 1024;
const int kABunch = 2 * 1024;

TEST(SbConditionVariableCreateTest, SunnyDay) {
  SbMutex mutex;
  EXPECT_TRUE(SbMutexCreate(&mutex));
  SbConditionVariable condition;
  EXPECT_TRUE(SbConditionVariableCreate(&condition, &mutex));
  EXPECT_TRUE(SbConditionVariableDestroy(&condition));
  EXPECT_TRUE(SbMutexDestroy(&mutex));
}

TEST(SbConditionVariableCreateTest, SunnyDayAutoInit) {
  SbMutex mutex = SB_MUTEX_INITIALIZER;
  EXPECT_TRUE(SbMutexCreate(&mutex));
  SbConditionVariable condition = SB_CONDITION_VARIABLE_INITIALIZER;
  EXPECT_TRUE(SbConditionVariableCreate(&condition, &mutex));
  EXPECT_TRUE(SbConditionVariableDestroy(&condition));
}

TEST(SbConditionVariableCreateTest, SunnyDayALot) {
  for (int i = 0; i < kALot; ++i) {
    SbMutex mutex;
    EXPECT_TRUE(SbMutexCreate(&mutex));
    SbConditionVariable condition;
    EXPECT_TRUE(SbConditionVariableCreate(&condition, &mutex));
    EXPECT_TRUE(SbConditionVariableDestroy(&condition));
    EXPECT_TRUE(SbMutexDestroy(&mutex));
  }
}

TEST(SbConditionVariableCreateTest, SunnyDayABunchAtOnce) {
  SbMutex mutices[kABunch];
  SbConditionVariable conditions[kABunch];
  for (int i = 0; i < kABunch; ++i) {
    EXPECT_TRUE(SbMutexCreate(&mutices[i]));
    EXPECT_TRUE(SbConditionVariableCreate(&conditions[i], &mutices[i]));
  }

  for (int i = 0; i < kABunch; ++i) {
    EXPECT_TRUE(SbConditionVariableDestroy(&conditions[i]));
    EXPECT_TRUE(SbMutexDestroy(&mutices[i]));
  }
}

TEST(SbConditionVariableCreateTest, RainyDayNull) {
  EXPECT_FALSE(SbConditionVariableCreate(NULL, NULL));

  SbMutex mutex;
  EXPECT_TRUE(SbMutexCreate(&mutex));
  EXPECT_FALSE(SbConditionVariableCreate(NULL, &mutex));
  EXPECT_TRUE(SbMutexDestroy(&mutex));
}

TEST(SbConditionVariableCreateTest, SunnyDayNullMutex) {
  SbConditionVariable condition;
  EXPECT_TRUE(SbConditionVariableCreate(&condition, NULL));
  EXPECT_TRUE(SbConditionVariableDestroy(&condition));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
