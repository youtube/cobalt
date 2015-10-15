// Copyright 2015 Google Inc. All Rights Reserved.
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

using namespace starboard::nplb;

namespace {

TEST(SbThreadCreateTest, SunnyDay) {
  const int kTrials = 64;
  for (int i = 0; i < kTrials; ++i) {
    SbThread thread =
        SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity, true,
                       kThreadName, AddOneEntryPoint, kSomeContext);
    EXPECT_TRUE(SbThreadIsValid(thread));
    void *result = NULL;
    EXPECT_TRUE(SbThreadJoin(thread, &result));
    EXPECT_EQ(kSomeContextPlusOne, result);
  }
}

TEST(SbThreadCreateTest, SunnyDayWithPriorities) {
  const int kTrials = 64;
  for (int i = 0; i < kTrials; ++i) {
    SbThreadPriority priority;
    switch (i % 5) {
      case 0: priority = kSbThreadPriorityLowest; break;
      case 1: priority = kSbThreadPriorityLow; break;
      case 2: priority = kSbThreadPriorityNormal; break;
      case 3: priority = kSbThreadPriorityHigh; break;
      case 4: priority = kSbThreadPriorityHighest; break;
    }
    SbThread thread = SbThreadCreate(0, priority, kSbThreadNoAffinity,
                                     true, kThreadName, AddOneEntryPoint,
                                     kSomeContext);
    EXPECT_TRUE(SbThreadIsValid(thread));
    void *result = NULL;
    EXPECT_TRUE(SbThreadJoin(thread, &result));
    EXPECT_EQ(kSomeContextPlusOne, result);
  }
}

TEST(SbThreadCreateTest, SunnyDayNoName) {
  SbThread thread =
      SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity, true,
                     NULL, AddOneEntryPoint, kSomeContext);
  EXPECT_TRUE(SbThreadIsValid(thread));
  void *result = NULL;
  EXPECT_TRUE(SbThreadJoin(thread, &result));
  EXPECT_EQ(kSomeContextPlusOne, result);
}

TEST(SbThreadCreateTest, SunnyDayNoContext) {
  SbThread thread =
      SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity, true,
                     kThreadName, AddOneEntryPoint, NULL);
  EXPECT_TRUE(SbThreadIsValid(thread));
  void *result = NULL;
  EXPECT_TRUE(SbThreadJoin(thread, &result));
  EXPECT_EQ(toVoid(1), result);
}

TEST(SbThreadCreateTest, SunnyDayWithAffinity) {
  SbThread thread =
      SbThreadCreate(0, kSbThreadNoPriority, 0, true,
                     kThreadName, AddOneEntryPoint, kSomeContext);
  EXPECT_TRUE(SbThreadIsValid(thread));
  void *result = NULL;
  EXPECT_TRUE(SbThreadJoin(thread, &result));
  EXPECT_EQ(kSomeContextPlusOne, result);
}

TEST(SbThreadCreateTest, SunnyDayDetached) {
  SbThread thread =
      SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity, false,
                     kThreadName, AddOneEntryPoint, kSomeContext);
  EXPECT_TRUE(SbThreadIsValid(thread));
  void *result = NULL;
  EXPECT_FALSE(SbThreadJoin(thread, &result));
}

TEST(SbThreadCreateTest, Summertime) {
  const int kMany = 1024;
  SbThread threads[kMany];
  for (int i = 0; i < kMany; ++i) {
    threads[i] = SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity,
                                true, kThreadName, AddOneEntryPoint,
                                toVoid(i));
    EXPECT_TRUE(SbThreadIsValid(threads[i]));
  }

  for (int i = 0; i < kMany; ++i) {
    void *result = NULL;
    void * const kExpected = toVoid(i + 1);
    EXPECT_TRUE(SbThreadJoin(threads[i], &result));
    EXPECT_EQ(kExpected, result);
  }
}

TEST(SbThreadCreateTest, RainyDayNoEntryPoint) {
  SbThread thread =
      SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity, true,
                     kThreadName, NULL, kSomeContext);
  EXPECT_FALSE(SbThreadIsValid(thread));
}

}  // namespace
