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

#include "starboard/configuration.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "starboard/nplb/thread_helpers.h"
#include "starboard/thread.h"

namespace starboard {
namespace nplb {
namespace {

struct TestContext {
  explicit TestContext(pthread_mutex_t* mutex)
      : was_locked_(false), mutex_(mutex) {}
  bool was_locked_;
  pthread_mutex_t* mutex_;
};

void* EntryPoint(void* parameter) {
  TestContext* context = static_cast<TestContext*>(parameter);
  context->was_locked_ = (pthread_mutex_trylock(context->mutex_) == 0);
  return NULL;
}

TEST(PosixMutexAcquireTryTest, SunnyDayUncontended) {
  pthread_mutex_t mutex;
  EXPECT_EQ(pthread_mutex_init(&mutex, NULL), 0);

  EXPECT_EQ(pthread_mutex_trylock(&mutex), 0);
  EXPECT_EQ(pthread_mutex_unlock(&mutex), 0);
  EXPECT_EQ(pthread_mutex_destroy(&mutex), 0);
}

TEST(PosixMutexAcquireTest, SunnyDayAutoInit) {
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  EXPECT_EQ(pthread_mutex_trylock(&mutex), 0);
  EXPECT_EQ(pthread_mutex_unlock(&mutex), 0);
  EXPECT_EQ(pthread_mutex_destroy(&mutex), 0);
}

TEST(PosixMutexAcquireTryTest, RainyDayReentrant) {
  pthread_mutex_t mutex;
  EXPECT_EQ(pthread_mutex_init(&mutex, NULL), 0);

  EXPECT_EQ(pthread_mutex_trylock(&mutex), 0);

  TestContext context(&mutex);
  // TODO: Migrate to pthread_create when available.
  SbThread thread =
      SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity, true,
                     nplb::kThreadName, &EntryPoint, &context);

  EXPECT_TRUE(SbThreadIsValid(thread));
  EXPECT_TRUE(SbThreadJoin(thread, NULL));
  EXPECT_FALSE(context.was_locked_);
  EXPECT_EQ(pthread_mutex_unlock(&mutex), 0);
  EXPECT_EQ(pthread_mutex_destroy(&mutex), 0);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
