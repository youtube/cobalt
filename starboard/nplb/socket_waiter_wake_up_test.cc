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

#include <pthread.h>
#include <unistd.h>

#include "starboard/common/semaphore.h"
#include "starboard/common/time.h"
#include "starboard/nplb/posix_compliance/posix_thread_helpers.h"
#include "starboard/nplb/socket_helpers.h"
#include "starboard/socket_waiter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

const int kMultiple = 5;
const int kTrials = 8;

struct WakeUpContext {
  SbSocketWaiter waiter;
  Semaphore semaphore;
};

typedef void* (*ThreadEntryPoint)(void*);

void* WakeUpEntryPoint(void* context) {
  SbSocketWaiter waiter = reinterpret_cast<SbSocketWaiter>(context);
  SbSocketWaiterWakeUp(waiter);
  return NULL;
}

void* WakeUpSleepEntryPoint(void* context) {
  WakeUpContext* wake_up_context = reinterpret_cast<WakeUpContext*>(context);
  SbSocketWaiter waiter = wake_up_context->waiter;
  wake_up_context->semaphore.Take();
  usleep(kSocketTimeout);
  SbSocketWaiterWakeUp(waiter);
  return NULL;
}

pthread_t Spawn(void* context, ThreadEntryPoint entry) {
  pthread_t thread = 0;
  pthread_create(&thread, nullptr, entry, context);
  EXPECT_TRUE(thread != 0);
  return thread;
}

void Join(pthread_t thread) {
  EXPECT_EQ(pthread_join(thread, NULL), 0);
}

void SpawnJoin(void* context, ThreadEntryPoint entry) {
  Join(Spawn(context, entry));
}

TEST(SbSocketWaiterWakeUpTest, EarlyCallWakesUp) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  SbSocketWaiterWakeUp(waiter);

  TimedWaitShouldNotBlock(waiter, kSocketTimeout);
  TimedWaitShouldBlock(waiter, kSocketTimeout);

  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST(SbSocketWaiterWakeUpTest, EarlyCallWakesUpMultiply) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  for (int i = 0; i < kMultiple; ++i) {
    SbSocketWaiterWakeUp(waiter);
  }

  for (int i = 0; i < kMultiple; ++i) {
    TimedWaitShouldNotBlock(waiter, kSocketTimeout);
  }
  TimedWaitShouldBlock(waiter, kSocketTimeout);

  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST(SbSocketWaiterWakeUpTest, EarlyCallFromOtherThreadWakesUp) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  SpawnJoin(waiter, &WakeUpEntryPoint);

  TimedWaitShouldNotBlock(waiter, kSocketTimeout);
  TimedWaitShouldBlock(waiter, kSocketTimeout);

  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST(SbSocketWaiterWakeUpTest, EarlyCallFromOtherThreadWakesUpMultiply) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  for (int i = 0; i < kMultiple; ++i) {
    SpawnJoin(waiter, &WakeUpEntryPoint);
  }

  for (int i = 0; i < kMultiple; ++i) {
    TimedWaitShouldNotBlock(waiter, kSocketTimeout);
  }
  TimedWaitShouldBlock(waiter, kSocketTimeout);

  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST(SbSocketWaiterWakeUpTest, CallFromOtherThreadWakesUp) {
  for (int i = 0; i < kTrials; ++i) {
    SbSocketWaiter waiter = SbSocketWaiterCreate();
    EXPECT_TRUE(SbSocketWaiterIsValid(waiter));
    WakeUpContext context;
    context.waiter = waiter;

    pthread_t thread = Spawn(&context, &WakeUpSleepEntryPoint);
    int64_t start = CurrentMonotonicTime();
    context.semaphore.Put();
    TimedWait(waiter);
    int64_t duration = CurrentMonotonicTime() - start;
    EXPECT_GT(kSocketTimeout * 2, duration);
    EXPECT_LE(kSocketTimeout, duration);
    Join(thread);

    EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
