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

// Destroy is mostly Sunny Day tested in Create.

#if SB_API_VERSION < 16

#include "starboard/configuration.h"
#include "starboard/mutex.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbMutexDestroyTest, SunnyDayAutoInit) {
  SbMutex mutex = SB_MUTEX_INITIALIZER;
  EXPECT_TRUE(SbMutexDestroy(&mutex));
}

// Destroying a mutex that has already been destroyed is undefined behavior
// and cannot be tested.
TEST(SbMutexDestroyTest, RainyDayNull) {
  EXPECT_FALSE(SbMutexDestroy(NULL));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_API_VERSION < 16
