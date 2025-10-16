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

#include "starboard/nplb/posix_compliance/posix_thread_helpers.h"

#include <unistd.h>

#include <cstddef>

#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

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
    usleep(test_context->delay_after_signal);
  }

  // Signal the condition variable.
  EXPECT_EQ(pthread_mutex_lock(&test_context->mutex), 0);
  EXPECT_EQ(pthread_cond_signal(&test_context->condition), 0);
  EXPECT_EQ(pthread_mutex_unlock(&test_context->mutex), 0);

  return NULL;
}

WaiterContext::WaiterContext() : unreturned_waiters(0) {
  EXPECT_EQ(pthread_mutex_init(&mutex, NULL), 0);
  EXPECT_EQ(pthread_cond_init(&condition, NULL), 0);
  EXPECT_EQ(pthread_cond_init(&return_condition, NULL), 0);
}

WaiterContext::~WaiterContext() {
  EXPECT_EQ(pthread_cond_destroy(&return_condition), 0);
  EXPECT_EQ(pthread_cond_destroy(&condition), 0);
  EXPECT_EQ(pthread_mutex_destroy(&mutex), 0);
}

void WaiterContext::Wait() {
  EXPECT_EQ(pthread_mutex_lock(&mutex), 0);
  ++unreturned_waiters;
  EXPECT_EQ(pthread_cond_signal(&return_condition), 0);
  EXPECT_EQ(pthread_cond_wait(&condition, &mutex), 0);
  EXPECT_EQ(pthread_mutex_unlock(&mutex), 0);
}

void WaiterContext::WaitForReturnSignal() {
  EXPECT_EQ(pthread_mutex_lock(&mutex), 0);
  while (unreturned_waiters == 0) {
    EXPECT_EQ(pthread_cond_wait(&return_condition, &mutex), 0);
  }
  --unreturned_waiters;
  EXPECT_EQ(pthread_mutex_unlock(&mutex), 0);
}

}  // namespace nplb
