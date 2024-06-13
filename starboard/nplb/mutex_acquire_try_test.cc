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

#if SB_API_VERSION < 16

#include "starboard/configuration.h"
#include "starboard/mutex.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "starboard/nplb/thread_helpers.h"
#include "starboard/thread.h"

namespace starboard {
namespace nplb {
namespace {

struct TestContext {
  explicit TestContext(SbMutex* mutex) : was_locked_(false), mutex_(mutex) {}
  bool was_locked_;
  SbMutex* mutex_;
};

void* EntryPoint(void* parameter) {
  TestContext* context = static_cast<TestContext*>(parameter);
  context->was_locked_ =
      (SbMutexAcquireTry(context->mutex_) == kSbMutexAcquired);
  return NULL;
}

TEST(SbMutexAcquireTryTest, SunnyDayUncontended) {
  SbMutex mutex;
  EXPECT_TRUE(SbMutexCreate(&mutex));

  SbMutexResult result = SbMutexAcquireTry(&mutex);
  EXPECT_EQ(result, kSbMutexAcquired);
  EXPECT_TRUE(SbMutexIsSuccess(result));
  EXPECT_TRUE(SbMutexRelease(&mutex));
  EXPECT_TRUE(SbMutexDestroy(&mutex));
}

TEST(SbMutexAcquireTest, SunnyDayAutoInit) {
  SbMutex mutex = SB_MUTEX_INITIALIZER;
  SbMutexResult result = SbMutexAcquireTry(&mutex);
  EXPECT_TRUE(SbMutexRelease(&mutex));
  EXPECT_TRUE(SbMutexDestroy(&mutex));
}

TEST(SbMutexAcquireTryTest, RainyDayReentrant) {
  SbMutex mutex;
  EXPECT_TRUE(SbMutexCreate(&mutex));

  SbMutexResult result = SbMutexAcquireTry(&mutex);
  EXPECT_EQ(result, kSbMutexAcquired);
  EXPECT_TRUE(SbMutexIsSuccess(result));

  TestContext context(&mutex);
  SbThread thread =
      SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity, true,
                     nplb::kThreadName, &EntryPoint, &context);

  EXPECT_TRUE(SbThreadIsValid(thread));
  EXPECT_TRUE(SbThreadJoin(thread, NULL));
  EXPECT_FALSE(context.was_locked_);
  EXPECT_TRUE(SbMutexRelease(&mutex));
  EXPECT_TRUE(SbMutexDestroy(&mutex));
}

TEST(SbMutexAcquireTest, RainyDayAcquireTryNull) {
  SbMutexResult result = SbMutexAcquireTry(NULL);
  EXPECT_EQ(kSbMutexDestroyed, result);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_API_VERSION < 16
