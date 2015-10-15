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

#include "starboard/condition_variable.h"
#include "starboard/mutex.h"
#include "starboard/nplb/thread_helpers.h"
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace starboard::nplb;

namespace {

void DoSunnyDay(TakeThenSignalContext *context, bool check_timeout) {
  SbThread thread = SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity,
                                   true, NULL, TakeThenSignalEntryPoint,
                                   context);

  const SbTime kDelay = kSbTimeMillisecond * 10;

  // We know the thread hasn't signaled the condition variable yet, and won't
  // unless we tell it, so it should wait at least the whole delay time.
  if (check_timeout) {
    EXPECT_TRUE(SbMutexIsSuccess(SbMutexAcquire(&context->mutex)));
    SbTimeMonotonic start = SbTimeGetMonotonicNow();
    SbConditionVariableResult result =
        SbConditionVariableWaitTimed(&context->condition, &context->mutex,
                                     kDelay);
    EXPECT_EQ(kSbConditionVariableTimedOut, result);
    SbTimeMonotonic elapsed = SbTimeGetMonotonicNow() - start;
    EXPECT_LE(kDelay, elapsed);
    EXPECT_GT(kDelay * 2, elapsed);
    EXPECT_TRUE(SbMutexRelease(&context->mutex));
  }

  {
    EXPECT_TRUE(SbMutexIsSuccess(SbMutexAcquire(&context->mutex)));

    // Tell the thread to signal the condvar, which will cause it to attempt to
    // acquire the mutex we are holding.
    context->do_signal.Put();

    SbTimeMonotonic start = SbTimeGetMonotonicNow();

    // We release the mutex when we wait, allowing the thread to actually do the
    // signaling, and ensuring we are waiting before it signals.
    SbConditionVariableResult result =
        SbConditionVariableWaitTimed(&context->condition, &context->mutex,
                                     kDelay);
    EXPECT_EQ(kSbConditionVariableSignaled, result);

    // We should have waited only a very small amount of time.
    EXPECT_GT(kDelay, SbTimeGetMonotonicNow() - start);

    EXPECT_TRUE(SbMutexRelease(&context->mutex));
  }

  // Now we wait for the thread to exit.
  EXPECT_TRUE(SbThreadJoin(thread, NULL));
  EXPECT_TRUE(SbConditionVariableDestroy(&context->condition));
  EXPECT_TRUE(SbMutexDestroy(&context->mutex));
}

TEST(SbConditionVariableWaitTimedTest, SunnyDay) {
  TakeThenSignalContext context;
  EXPECT_TRUE(SbMutexCreate(&context.mutex));
  EXPECT_TRUE(SbConditionVariableCreate(&context.condition, &context.mutex));
  DoSunnyDay(&context, true);
}

TEST(SbConditionVariableWaitTimedTest, SunnyDayAutoInit) {
  {
    TakeThenSignalContext context = { 0, SB_MUTEX_INITIALIZER,
                                      SB_CONDITION_VARIABLE_INITIALIZER };
    DoSunnyDay(&context, true);
  }

  // Without the initial timeout test, the two threads will be racing to
  // auto-init the mutex and condition variable. So we run several trials in
  // this mode, hoping to have the auto-initting contend in various ways.
  const int kTrials = 64;
  for (int i = 0; i < kTrials; ++i) {
    TakeThenSignalContext context = { 0, SB_MUTEX_INITIALIZER,
                                      SB_CONDITION_VARIABLE_INITIALIZER };
    DoSunnyDay(&context, false);
  }
}

TEST(SbConditionVariableWaitTimedTest, RainyDayNull) {
  SbConditionVariableResult result =
      SbConditionVariableWaitTimed(NULL, NULL, 0);
  EXPECT_EQ(kSbConditionVariableFailed, result);

  SbMutex mutex = SB_MUTEX_INITIALIZER;
  result = SbConditionVariableWaitTimed(NULL, &mutex, 0);
  EXPECT_EQ(kSbConditionVariableFailed, result);

  SbConditionVariable condition = SB_CONDITION_VARIABLE_INITIALIZER;
  result = SbConditionVariableWaitTimed(&condition, NULL, 0);
  EXPECT_EQ(kSbConditionVariableFailed, result);
}

}  // namespace
