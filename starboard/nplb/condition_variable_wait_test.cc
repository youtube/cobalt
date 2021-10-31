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

TEST(SbConditionVariableWaitTest, SunnyDayAutoInit) {
  TakeThenSignalContext context = {TestSemaphore(0), SB_MUTEX_INITIALIZER,
                                   SB_CONDITION_VARIABLE_INITIALIZER};
  // Start the thread.
  SbThread thread =
      SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity, true, NULL,
                     TakeThenSignalEntryPoint, &context);

  EXPECT_TRUE(SbMutexIsSuccess(SbMutexAcquire(&context.mutex)));

  // Tell the thread to signal the condvar, which will cause it to attempt to
  // acquire the mutex we are holding.
  context.do_signal.Put();

  // We release the mutex when we wait, allowing the thread to actually do the
  // signaling, and ensuring we are waiting before it signals.
  SbConditionVariableResult result =
      SbConditionVariableWait(&context.condition, &context.mutex);
  EXPECT_EQ(kSbConditionVariableSignaled, result);

  EXPECT_TRUE(SbMutexRelease(&context.mutex));

  // Now we wait for the thread to exit.
  EXPECT_TRUE(SbThreadJoin(thread, NULL));
  EXPECT_TRUE(SbConditionVariableDestroy(&context.condition));
  EXPECT_TRUE(SbMutexDestroy(&context.mutex));
}

TEST(SbConditionVariableWaitTest, SunnyDay) {
  const int kMany = kSbMaxThreads > 64 ? 64 : kSbMaxThreads;
  WaiterContext context;

  std::vector<SbThread> threads(kMany);
  for (int i = 0; i < kMany; ++i) {
    threads[i] = SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity,
                                true, NULL, WaiterEntryPoint, &context);
  }

  for (int i = 0; i < kMany; ++i) {
    context.WaitForReturnSignal();
  }

  // Signal the conditions to make the thread wake up and exit.
  EXPECT_TRUE(SbConditionVariableBroadcast(&context.condition));

  // Now we wait for the threads to exit.
  for (int i = 0; i < kMany; ++i) {
    EXPECT_TRUE(SbThreadJoin(threads[i], NULL)) << "thread = " << threads[i];
  }
}

TEST(SbConditionVariableWaitTest, RainyDayNull) {
  SbConditionVariableResult result = SbConditionVariableWait(NULL, NULL);
  EXPECT_EQ(kSbConditionVariableFailed, result);

  SbMutex mutex = SB_MUTEX_INITIALIZER;
  result = SbConditionVariableWait(NULL, &mutex);
  EXPECT_EQ(kSbConditionVariableFailed, result);

  SbConditionVariable condition = SB_CONDITION_VARIABLE_INITIALIZER;
  result = SbConditionVariableWait(&condition, NULL);
  EXPECT_EQ(kSbConditionVariableFailed, result);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
