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

namespace {

const int kALot = 128 * 1024;
const int kABunch = 2 * 1024;

TEST(SbMutexCreateTest, Creates) {
  SbMutex mutex = SbMutexCreate();
  EXPECT_TRUE(SbMutexIsValid(mutex));
  bool result = SbMutexDestroy(mutex);
  EXPECT_TRUE(result);
}

TEST(SbMutexCreateTest, CreatesALot) {
  for (int i = 0; i < kALot; ++i) {
    SbMutex mutex = SbMutexCreate();
    EXPECT_TRUE(SbMutexIsValid(mutex));
    bool result = SbMutexDestroy(mutex);
    EXPECT_TRUE(result);
  }
}

TEST(SbMutexCreateTest, CreatesABunchAtOnce) {
  SbMutex mutices[kABunch] = {0};
  for (int i = 0; i < kABunch; ++i) {
    mutices[i] = SbMutexCreate();
    EXPECT_TRUE(SbMutexIsValid(mutices[i]));
  }

  for (int i = 0; i < kABunch; ++i) {
    bool result = SbMutexDestroy(mutices[i]);
    EXPECT_TRUE(result);
  }
}

}  // namespace
