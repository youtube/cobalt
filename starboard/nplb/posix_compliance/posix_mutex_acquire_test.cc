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

#include "starboard/configuration.h"
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

struct TestContext {
  TestContext() : count(0) {}
  pthread_mutex_t mutex;
  int count;
};

const int kLoops = 10000;

void* EntryPoint(void* parameter) {
  TestContext* context = static_cast<TestContext*>(parameter);

  for (int i = 0; i < kLoops; ++i) {
    pthread_mutex_lock(&context->mutex);
    context->count++;
    pthread_mutex_unlock(&context->mutex);
  }

  return NULL;
}

// This test just tries to acquire a mutex repeatedly while other threads are
// doing the same.
TEST(PosixMutexAcquireTest, SunnyDayContended) {
  TestContext context;
  EXPECT_EQ(pthread_mutex_init(&context.mutex, NULL), 0);
  const int kThreads = 4;
  pthread_t threads[kThreads];
  for (int i = 0; i < kThreads; ++i) {
    pthread_create(&threads[i], NULL, EntryPoint, &context);
  }

  for (int i = 0; i < kLoops; ++i) {
    for (int j = 0; j < kThreads; ++j) {
      EXPECT_EQ(pthread_mutex_lock(&context.mutex), 0);
      context.count--;
      EXPECT_EQ(pthread_mutex_unlock(&context.mutex), 0);
    }
  }

  // Join other threads and clean up.
  for (int i = 0; i < kThreads; ++i) {
    EXPECT_TRUE(pthread_join(threads[i], NULL) == 0);
  }
  EXPECT_EQ(pthread_mutex_destroy(&context.mutex), 0);
  EXPECT_EQ(0, context.count);
}

TEST(PosixMutexAcquireTest, SunnyDayUncontended) {
  pthread_mutex_t mutex;
  EXPECT_EQ(pthread_mutex_init(&mutex, NULL), 0);

  EXPECT_EQ(pthread_mutex_lock(&mutex), 0);
  EXPECT_EQ(pthread_mutex_unlock(&mutex), 0);
  EXPECT_EQ(pthread_mutex_destroy(&mutex), 0);
}

TEST(PosixMutexAcquireTest, SunnyDayStaticallyInitialized) {
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  EXPECT_EQ(pthread_mutex_lock(&mutex), 0);
  EXPECT_EQ(pthread_mutex_unlock(&mutex), 0);
  EXPECT_EQ(pthread_mutex_destroy(&mutex), 0);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
