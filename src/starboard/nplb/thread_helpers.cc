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

#include "starboard/nplb/thread_helpers.h"

#include "starboard/common/condition_variable.h"
#include "starboard/common/mutex.h"
#include "starboard/thread.h"
#include "starboard/types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

void DoNotYield() {
  // Nope.
}

// Adds 1 to the input.
void* AddOneEntryPoint(void* context) {
  return ToVoid(FromVoid(context) + 1);
}

void* WaiterEntryPoint(void* context) {
  WaiterContext* real_context = static_cast<WaiterContext*>(context);
  real_context->Wait();
  return NULL;
}

void* TakeThenSignalEntryPoint(void* context) {
  TakeThenSignalContext* test_context =
      static_cast<TakeThenSignalContext*>(context);

  // Don't signal the condition variable until we are asked.
  test_context->do_signal.Take();

  if (test_context->delay_after_signal > 0) {
    SbThreadSleep(test_context->delay_after_signal);
  }

  // Signal the condition variable.
  EXPECT_TRUE(SbMutexIsSuccess(SbMutexAcquire(&test_context->mutex)));
  EXPECT_TRUE(SbConditionVariableSignal(&test_context->condition));
  EXPECT_TRUE(SbMutexRelease(&test_context->mutex));

  return NULL;
}

WaiterContext::WaiterContext() : unreturned_waiters(0) {
  EXPECT_TRUE(SbMutexCreate(&mutex));
  EXPECT_TRUE(SbConditionVariableCreate(&condition, &mutex));
  EXPECT_TRUE(SbConditionVariableCreate(&return_condition, &mutex));
}

WaiterContext::~WaiterContext() {
  EXPECT_TRUE(SbConditionVariableDestroy(&return_condition));
  EXPECT_TRUE(SbConditionVariableDestroy(&condition));
  EXPECT_TRUE(SbMutexDestroy(&mutex));
}

void WaiterContext::Wait() {
  EXPECT_TRUE(SbMutexIsSuccess(SbMutexAcquire(&mutex)));
  ++unreturned_waiters;
  EXPECT_TRUE(SbConditionVariableSignal(&return_condition));
  EXPECT_TRUE(SbConditionVariableIsSignaled(
      SbConditionVariableWait(&condition, &mutex)));
  EXPECT_TRUE(SbMutexRelease(&mutex));
}

void WaiterContext::WaitForReturnSignal() {
  EXPECT_TRUE(SbMutexIsSuccess(SbMutexAcquire(&mutex)));
  while (unreturned_waiters == 0) {
    EXPECT_TRUE(SbConditionVariableIsSignaled(
        SbConditionVariableWait(&return_condition, &mutex)));
  }
  --unreturned_waiters;
  EXPECT_TRUE(SbMutexRelease(&mutex));
}

}  // namespace nplb
}  // namespace starboard
