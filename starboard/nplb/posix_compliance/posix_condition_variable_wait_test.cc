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

#include <unistd.h>

#include "starboard/configuration_constants.h"
#include "starboard/nplb/posix_compliance/posix_thread_helpers.h"
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

void* PosixTakeThenSignalEntryPoint(void* context) {
  posix::TakeThenSignalContext* test_context =
      static_cast<posix::TakeThenSignalContext*>(context);

  // Don't signal the condition variable until we are asked.
  test_context->do_signal.Take();

  if (test_context->delay_after_signal > 0) {
    usleep(test_context->delay_after_signal);
  }

  // Signal the condition variable.
  EXPECT_EQ(pthread_mutex_lock(&test_context->mutex), 0);
  EXPECT_TRUE(pthread_cond_signal(&test_context->condition));
  EXPECT_EQ(pthread_mutex_unlock(&test_context->mutex), 0);

  return NULL;
}

TEST(PosixConditionVariableWaitTest, SunnyDayAutoInit) {
  posix::TakeThenSignalContext context = {posix::TestSemaphore(0),
                                          PTHREAD_MUTEX_INITIALIZER,
                                          PTHREAD_COND_INITIALIZER};
  // Start the thread.
  pthread_t thread = 0;
  pthread_create(&thread, NULL, posix::TakeThenSignalEntryPoint, &context);

  EXPECT_EQ(pthread_mutex_lock(&context.mutex), 0);

  // Tell the thread to signal the condvar, which will cause it to attempt to
  // acquire the mutex we are holding.
  context.do_signal.Put();

  // We release the mutex when we wait, allowing the thread to actually do the
  // signaling, and ensuring we are waiting before it signals.

  EXPECT_EQ(pthread_cond_wait(&context.condition, &context.mutex), 0);

  EXPECT_EQ(pthread_mutex_unlock(&context.mutex), 0);

  // Now we wait for the thread to exit.
  EXPECT_TRUE(pthread_join(thread, NULL) == 0);
  EXPECT_EQ(pthread_cond_destroy(&context.condition), 0);
  EXPECT_EQ(pthread_mutex_destroy(&context.mutex), 0);
}

TEST(PosixConditionVariableWaitTest, SunnyDay) {
  const int kMany = kSbMaxThreads > 64 ? 64 : kSbMaxThreads;
  posix::WaiterContext context;

  std::vector<pthread_t> threads(kMany);
  for (int i = 0; i < kMany; ++i) {
    pthread_create(&threads[i], NULL, posix::WaiterEntryPoint, &context);
  }

  for (int i = 0; i < kMany; ++i) {
    context.WaitForReturnSignal();
  }

  // Signal the conditions to make the thread wake up and exit.
  EXPECT_EQ(pthread_cond_broadcast(&context.condition), 0);

  // Now we wait for the threads to exit.
  for (int i = 0; i < kMany; ++i) {
    EXPECT_TRUE(pthread_join(threads[i], NULL) == 0)
        << "thread = " << threads[i];
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
