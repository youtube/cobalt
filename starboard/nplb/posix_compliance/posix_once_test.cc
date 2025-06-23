// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
#include <sched.h>

#include "starboard/configuration_constants.h"
#include "starboard/nplb/posix_compliance/posix_thread_helpers.h"
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

int s_global_value;

void IncrementGlobalValue() {
  ++s_global_value;
}

TEST(PosixOnceTest, SunnyDaySingleInit) {
  pthread_once_t once_control = PTHREAD_ONCE_INIT;

  s_global_value = 0;
  EXPECT_EQ(pthread_once(&once_control, &IncrementGlobalValue), 0);

  EXPECT_EQ(s_global_value, 1);
}

TEST(PosixOnceTest, SunnyDayMultipleInit) {
  pthread_once_t once_control = PTHREAD_ONCE_INIT;

  s_global_value = 0;
  EXPECT_EQ(pthread_once(&once_control, &IncrementGlobalValue), 0);
  EXPECT_EQ(s_global_value, 1);

  s_global_value = 0;
  EXPECT_EQ(pthread_once(&once_control, &IncrementGlobalValue), 0);
  EXPECT_EQ(s_global_value, 0);

  s_global_value = 0;
  EXPECT_EQ(pthread_once(&once_control, &IncrementGlobalValue), 0);
  EXPECT_EQ(s_global_value, 0);
}

struct RunPosixOnceContext {
  RunPosixOnceContext() : once_control(PTHREAD_ONCE_INIT) {
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&condition, NULL);
  }
  ~RunPosixOnceContext() {
    pthread_cond_destroy(&condition);
    pthread_mutex_destroy(&mutex);
  }

  posix::TestSemaphore semaphore;
  pthread_mutex_t mutex;
  pthread_cond_t condition;

  pthread_once_t once_control;
};

void* RunPosixOnceEntryPoint(void* context) {
  pthread_setname_np(pthread_self(), posix::kThreadName);

  RunPosixOnceContext* run_sbonce_context =
      reinterpret_cast<RunPosixOnceContext*>(context);

  {
    pthread_mutex_lock(&run_sbonce_context->mutex);
    run_sbonce_context->semaphore.Put();
    pthread_cond_wait(&run_sbonce_context->condition,
                      &run_sbonce_context->mutex);
    pthread_mutex_unlock(&run_sbonce_context->mutex);
  }

  sched_yield();
  static const int kIterationCount = 3;
  for (int i = 0; i < kIterationCount; ++i) {
    pthread_once(&run_sbonce_context->once_control, &IncrementGlobalValue);
  }

  return NULL;
}

// Here we spawn many threads each of which will call pthread_once multiple
// times using a shared pthread_once_t object.  We then test that the
// initialization routine got called exactly one time.
TEST(PosixOnceTest, SunnyDayMultipleThreadsInit) {
  const int kMany = kSbMaxThreads;
  std::vector<pthread_t> threads(kMany);

  const int kIterationCount = 10;
  for (int i = 0; i < kIterationCount; ++i) {
    RunPosixOnceContext context;

    s_global_value = 0;
    for (int j = 0; j < kMany; ++j) {
      pthread_create(&threads[j], NULL, RunPosixOnceEntryPoint, &context);
    }

    // Wait for all threads to finish initializing and become ready, then
    // broadcast the signal to begin.  We do this to increase the chances that
    // threads will call pthread_once() at the same time as each other.
    for (int j = 0; j < kMany; ++j) {
      context.semaphore.Take();
    }
    {
      pthread_mutex_lock(&context.mutex);
      pthread_cond_broadcast(&context.condition);
      pthread_mutex_unlock(&context.mutex);
    }

    // Signal threads to beginWait for all threads to complete.
    for (int j = 0; j < kMany; ++j) {
      void* result;
      pthread_join(threads[j], &result);
    }

    EXPECT_EQ(s_global_value, 1);
  }
}

int* GetIntSingleton() {
  static pthread_once_t s_once_flag = PTHREAD_ONCE_INIT;
  static int* s_singleton = NULL;
  struct Local {
    static void Init() { s_singleton = new int(); }
  };
  pthread_once(&s_once_flag, Local::Init);
  return s_singleton;
}

TEST(PosixOnceTest, InitializeOnceMacroFunction) {
  int* int_singelton = GetIntSingleton();
  ASSERT_TRUE(int_singelton);
  EXPECT_EQ(*int_singelton, 0)
      << "Singleton Macro does not default initialize.";
}

}  // namespace.
}  // namespace nplb.
}  // namespace starboard.
