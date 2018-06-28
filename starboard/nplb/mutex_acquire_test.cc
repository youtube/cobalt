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

#include "starboard/configuration.h"
#include "starboard/mutex.h"
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

struct TestContext {
  TestContext() : count(0) {}
  SbMutex mutex;
  int count;
};

const int kLoops = 10000;

void* EntryPoint(void* parameter) {
  TestContext* context = static_cast<TestContext*>(parameter);

  for (int i = 0; i < kLoops; ++i) {
    SbMutexAcquire(&context->mutex);
    int k = context->count;
    k = k + 1;
    context->count = k;
    SbMutexRelease(&context->mutex);
  }

  return NULL;
}

// This test just tries to acquire a mutex repeatedly while other threads are
// doing the same.
TEST(SbMutexAcquireTest, SunnyDayContended) {
  TestContext context;
  EXPECT_TRUE(SbMutexCreate(&context.mutex));
  const int kThreads = 4;
  SbThread threads[kThreads];
  for (int i = 0; i < kThreads; ++i) {
    threads[i] = SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity,
                                true, NULL, EntryPoint, &context);
  }

  for (int i = 0; i < kLoops; ++i) {
    for (int j = 0; j < kThreads; ++j) {
      EXPECT_TRUE(SbMutexIsSuccess(SbMutexAcquire(&context.mutex)));
      int k = context.count;
      k = k - 1;
      context.count = k;
      EXPECT_TRUE(SbMutexRelease(&context.mutex));
    }
  }

  // Join other threads and clean up.
  for (int i = 0; i < kThreads; ++i) {
    EXPECT_TRUE(SbThreadJoin(threads[i], NULL));
  }
  EXPECT_TRUE(SbMutexDestroy(&context.mutex));
  EXPECT_EQ(0, context.count);
}

TEST(SbMutexAcquireTest, SunnyDayUncontended) {
  SbMutex mutex;
  EXPECT_TRUE(SbMutexCreate(&mutex));

  SbMutexResult result = SbMutexAcquire(&mutex);
  EXPECT_EQ(kSbMutexAcquired, result);
  EXPECT_TRUE(SbMutexIsSuccess(result));
  EXPECT_TRUE(SbMutexRelease(&mutex));
  EXPECT_TRUE(SbMutexDestroy(&mutex));
}

TEST(SbMutexAcquireTest, SunnyDayStaticallyInitialized) {
  SbMutex mutex = SB_MUTEX_INITIALIZER;
  EXPECT_TRUE(SbMutexIsSuccess(SbMutexAcquire(&mutex)));
  EXPECT_TRUE(SbMutexRelease(&mutex));
  EXPECT_TRUE(SbMutexDestroy(&mutex));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
