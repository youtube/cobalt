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

#include "starboard/configuration_constants.h"
#include "starboard/nplb/thread_helpers.h"
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

const SbThreadPriority kAllThreadPriorities[] = {
    kSbThreadPriorityLowest,  kSbThreadPriorityLow,
    kSbThreadPriorityNormal,  kSbThreadPriorityHigh,
    kSbThreadPriorityHighest, kSbThreadPriorityRealTime,
    kSbThreadNoPriority,
};

TEST(SbThreadCreateTest, SunnyDay) {
  const int kTrials = 64;
  for (int i = 0; i < kTrials; ++i) {
    SbThread thread = SbThreadCreate(
        0, kSbThreadNoPriority, kSbThreadNoAffinity, true, nplb::kThreadName,
        nplb::AddOneEntryPoint, nplb::kSomeContext);
    EXPECT_TRUE(SbThreadIsValid(thread));
    void* result = NULL;
    EXPECT_TRUE(SbThreadJoin(thread, &result));
    EXPECT_EQ(nplb::kSomeContextPlusOne, result);
  }
}

TEST(SbThreadCreateTest, SunnyDayWithPriorities) {
  const int kTrials = 64;
  for (int i = 0; i < kTrials; ++i) {
    SbThreadPriority priority;
    switch (i % 7) {
      case 0:
        priority = kSbThreadPriorityLowest;
        break;
      case 1:
        priority = kSbThreadPriorityLow;
        break;
      case 2:
        priority = kSbThreadPriorityNormal;
        break;
      case 3:
        priority = kSbThreadPriorityHigh;
        break;
      case 4:
        priority = kSbThreadPriorityHighest;
        break;
      case 5:
        priority = kSbThreadPriorityRealTime;
        break;
      case 6:
        priority = kSbThreadNoPriority;
        break;
    }
    SbThread thread = SbThreadCreate(0, priority, kSbThreadNoAffinity, true,
                                     nplb::kThreadName, nplb::AddOneEntryPoint,
                                     nplb::kSomeContext);
    EXPECT_TRUE(SbThreadIsValid(thread));
    void* result = NULL;
    EXPECT_TRUE(SbThreadJoin(thread, &result));
    EXPECT_EQ(nplb::kSomeContextPlusOne, result);
  }
}

void* CreateNestedThreadFunc(void* context) {
  for (auto thread_priority : kAllThreadPriorities) {
    SbThread thread = SbThreadCreate(
        0, thread_priority, kSbThreadNoAffinity, true, nplb::kThreadName,
        nplb::AddOneEntryPoint, nplb::kSomeContext);
    EXPECT_TRUE(SbThreadIsValid(thread));
    void* result = NULL;
    EXPECT_TRUE(SbThreadJoin(thread, &result));
    EXPECT_EQ(nplb::kSomeContextPlusOne, result);
  }
  return NULL;
}

TEST(SbThreadCreateTest, SunnyDayWithNestedPriorities) {
  for (auto thread_priority : kAllThreadPriorities) {
    SbThread thread = SbThreadCreate(
        0, thread_priority, kSbThreadNoAffinity, true, nplb::kThreadName,
        CreateNestedThreadFunc, nplb::kSomeContext);
    EXPECT_TRUE(SbThreadIsValid(thread));
    void* result = NULL;
    EXPECT_TRUE(SbThreadJoin(thread, &result));
  }
}

TEST(SbThreadCreateTest, SunnyDayNoName) {
  SbThread thread =
      SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity, true, NULL,
                     nplb::AddOneEntryPoint, nplb::kSomeContext);
  EXPECT_TRUE(SbThreadIsValid(thread));
  void* result = NULL;
  EXPECT_TRUE(SbThreadJoin(thread, &result));
  EXPECT_EQ(nplb::kSomeContextPlusOne, result);
}

TEST(SbThreadCreateTest, SunnyDayNoContext) {
  SbThread thread =
      SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity, true,
                     nplb::kThreadName, nplb::AddOneEntryPoint, NULL);
  EXPECT_TRUE(SbThreadIsValid(thread));
  void* result = NULL;
  EXPECT_TRUE(SbThreadJoin(thread, &result));
  EXPECT_EQ(nplb::ToVoid(1), result);
}

TEST(SbThreadCreateTest, SunnyDayWithAffinity) {
  SbThread thread =
      SbThreadCreate(0, kSbThreadNoPriority, 0, true, nplb::kThreadName,
                     nplb::AddOneEntryPoint, nplb::kSomeContext);
  EXPECT_TRUE(SbThreadIsValid(thread));
  void* result = NULL;
  EXPECT_TRUE(SbThreadJoin(thread, &result));
  EXPECT_EQ(nplb::kSomeContextPlusOne, result);
}

TEST(SbThreadCreateTest, SunnyDayDetached) {
  SbThread thread = SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity,
                                   false, nplb::kThreadName,
                                   nplb::AddOneEntryPoint, nplb::kSomeContext);
  EXPECT_TRUE(SbThreadIsValid(thread));
  void* result = NULL;
  EXPECT_FALSE(SbThreadJoin(thread, &result));
}

TEST(SbThreadCreateTest, Summertime) {
  const int kMany = kSbMaxThreads;
  std::vector<SbThread> threads(kMany);
  for (int i = 0; i < kMany; ++i) {
    threads[i] = SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity,
                                true, nplb::kThreadName, nplb::AddOneEntryPoint,
                                nplb::ToVoid(i));
    EXPECT_TRUE(SbThreadIsValid(threads[i]));
  }

  for (int i = 0; i < kMany; ++i) {
    void* result = NULL;
    void* const kExpected = nplb::ToVoid(i + 1);
    EXPECT_TRUE(SbThreadJoin(threads[i], &result));
    EXPECT_EQ(kExpected, result);
  }
}

TEST(SbThreadCreateTest, RainyDayNoEntryPoint) {
  SbThread thread =
      SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity, true,
                     nplb::kThreadName, NULL, nplb::kSomeContext);
  EXPECT_FALSE(SbThreadIsValid(thread));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
