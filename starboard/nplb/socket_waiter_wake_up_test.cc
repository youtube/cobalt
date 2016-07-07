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

#include "starboard/nplb/socket_helpers.h"
#include "starboard/socket_waiter.h"
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

const int kMultiple = 5;
const int kTrials = 8;

void* WakeUpEntryPoint(void* context) {
  SbSocketWaiter waiter = reinterpret_cast<SbSocketWaiter>(context);
  SbSocketWaiterWakeUp(waiter);
  return NULL;
}

void* WakeUpSleepEntryPoint(void* context) {
  SbSocketWaiter waiter = reinterpret_cast<SbSocketWaiter>(context);
  SbThreadSleep(kTimeout);
  SbSocketWaiterWakeUp(waiter);
  return NULL;
}

SbThread Spawn(void* context, SbThreadEntryPoint entry) {
  SbThread thread =
      SbThreadCreate(0, kSbThreadPriorityNormal, kSbThreadNoAffinity, true,
                     NULL, entry, context);
  EXPECT_TRUE(SbThreadIsValid(thread));
  return thread;
}

void Join(SbThread thread) {
  EXPECT_TRUE(SbThreadJoin(thread, NULL));
}

void SpawnJoin(void* context, SbThreadEntryPoint entry) {
  Join(Spawn(context, entry));
}

TEST(SbSocketWaiterWakeUpTest, EarlyCallWakesUp) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  SbSocketWaiterWakeUp(waiter);

  TimedWaitShouldNotBlock(waiter, kTimeout);
  TimedWaitShouldBlock(waiter, kTimeout);

  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST(SbSocketWaiterWakeUpTest, EarlyCallWakesUpMultiply) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  for (int i = 0; i < kMultiple; ++i) {
    SbSocketWaiterWakeUp(waiter);
  }

  for (int i = 0; i < kMultiple; ++i) {
    TimedWaitShouldNotBlock(waiter, kTimeout);
  }
  TimedWaitShouldBlock(waiter, kTimeout);

  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST(SbSocketWaiterWakeUpTest, EarlyCallFromOtherThreadWakesUp) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  SpawnJoin(waiter, &WakeUpEntryPoint);

  TimedWaitShouldNotBlock(waiter, kTimeout);
  TimedWaitShouldBlock(waiter, kTimeout);

  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST(SbSocketWaiterWakeUpTest, EarlyCallFromOtherThreadWakesUpMultiply) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  for (int i = 0; i < kMultiple; ++i) {
    SpawnJoin(waiter, &WakeUpEntryPoint);
  }

  for (int i = 0; i < kMultiple; ++i) {
    TimedWaitShouldNotBlock(waiter, kTimeout);
  }
  TimedWaitShouldBlock(waiter, kTimeout);

  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST(SbSocketWaiterWakeUpTest, CallFromOtherThreadWakesUp) {
  for (int i = 0; i < kTrials; ++i) {
    SbSocketWaiter waiter = SbSocketWaiterCreate();
    EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

    SbThread thread = Spawn(waiter, &WakeUpSleepEntryPoint);
    WaitShouldBlockBetween(waiter, kTimeout, kTimeout * 2);
    Join(thread);

    EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
