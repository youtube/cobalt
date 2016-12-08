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

// Broadcast is Sunny Day tested in most of the other SbConditionVariable tests.

#include "starboard/nplb/thread_helpers.h"
#include "starboard/once.h"
#include "starboard/thread.h"
#include "starboard/thread_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

int s_global_value;

void IncrementGlobalValue() {
  ++s_global_value;
}

TEST(SbOnceTest, SunnyDaySingleInit) {
  SbOnceControl once_control = SB_ONCE_INITIALIZER;

  s_global_value = 0;
  EXPECT_TRUE(SbOnce(&once_control, &IncrementGlobalValue));

  EXPECT_EQ(1, s_global_value);
}

TEST(SbOnceTest, SunnyDayMultipleInit) {
  SbOnceControl once_control = SB_ONCE_INITIALIZER;

  s_global_value = 0;
  EXPECT_TRUE(SbOnce(&once_control, &IncrementGlobalValue));
  EXPECT_EQ(1, s_global_value);

  s_global_value = 0;
  EXPECT_TRUE(SbOnce(&once_control, &IncrementGlobalValue));
  EXPECT_EQ(0, s_global_value);

  s_global_value = 0;
  EXPECT_TRUE(SbOnce(&once_control, &IncrementGlobalValue));
  EXPECT_EQ(0, s_global_value);
}

struct RunSbOnceContext {
  RunSbOnceContext() : once_control(SB_ONCE_INITIALIZER) {
    SbMutexCreate(&mutex);
    SbConditionVariableCreate(&condition, &mutex);
  }
  ~RunSbOnceContext() {
    SbConditionVariableDestroy(&condition);
    SbMutexDestroy(&mutex);
  }

  Semaphore semaphore;
  SbMutex mutex;
  SbConditionVariable condition;

  SbOnceControl once_control;
};

void* RunSbOnceEntryPoint(void* context) {
  RunSbOnceContext* run_sbonce_context =
      reinterpret_cast<RunSbOnceContext*>(context);

  {
    SbMutexAcquire(&run_sbonce_context->mutex);
    run_sbonce_context->semaphore.Put();
    SbConditionVariableWait(&run_sbonce_context->condition,
                            &run_sbonce_context->mutex);
    SbMutexRelease(&run_sbonce_context->mutex);
  }

  SbThreadYield();
  static const int kIterationCount = 3;
  for (int i = 0; i < kIterationCount; ++i) {
    SbOnce(&run_sbonce_context->once_control, &IncrementGlobalValue);
  }

  return NULL;
}

// Here we spawn many threads each of which will call SbOnce multiple times
// using a shared SbOnceControl object.  We then test that the initialization
// routine got called exactly one time.
TEST(SbOnceTest, SunnyDayMultipleThreadsInit) {
  const int kMany = SB_MAX_THREADS;
  SbThread threads[kMany];

  const int kIterationCount = 10;
  for (int i = 0; i < kIterationCount; ++i) {
    SbOnceControl once_control = SB_ONCE_INITIALIZER;
    RunSbOnceContext context;

    s_global_value = 0;
    for (int i = 0; i < kMany; ++i) {
      threads[i] =
          SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity, true,
                         kThreadName, RunSbOnceEntryPoint, &context);
    }

    // Wait for all threads to finish initializing and become ready, then
    // broadcast the signal to begin.  We do this to increase the chances that
    // threads will call SbOnce() at the same time as each other.
    for (int i = 0; i < kMany; ++i) {
      context.semaphore.Take();
    }
    {
      SbMutexAcquire(&context.mutex);
      SbConditionVariableBroadcast(&context.condition);
      SbMutexRelease(&context.mutex);
    }

    // Signal threads to beginWait for all threads to complete.
    for (int i = 0; i < kMany; ++i) {
      void* result;
      SbThreadJoin(threads[i], &result);
    }

    EXPECT_EQ(1, s_global_value);
  }
}

TEST(SbOnceTest, RainyDayBadOnceControl) {
  s_global_value = 0;
  EXPECT_FALSE(SbOnce(NULL, &IncrementGlobalValue));

  EXPECT_EQ(0, s_global_value);
}

TEST(SbOnceTest, RainyDayBadInitRoutine) {
  SbOnceControl once_control = SB_ONCE_INITIALIZER;

  s_global_value = 0;
  EXPECT_FALSE(SbOnce(&once_control, NULL));

  EXPECT_EQ(0, s_global_value);
}

}  // namespace nplb
}  // namespace starboard
