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

#include "starboard/configuration.h"
#include "starboard/mutex.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

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

  result = SbMutexAcquireTry(&mutex);
  EXPECT_EQ(result, kSbMutexBusy);
  EXPECT_FALSE(SbMutexIsSuccess(result));

  EXPECT_TRUE(SbMutexRelease(&mutex));
  EXPECT_TRUE(SbMutexDestroy(&mutex));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
