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

#include "starboard/socket_waiter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbSocketWaiterCreateTest, SunnyDay) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));
  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST(SbSocketWaiterCreateTest, ATon) {
  const int kATon = 256;
  for (int i = 0; i < kATon; ++i) {
    SbSocketWaiter waiter = SbSocketWaiterCreate();
    EXPECT_TRUE(SbSocketWaiterIsValid(waiter));
    EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
  }
}

TEST(SbSocketWaiterCreateTest, ManyAtOnce) {
  const int kMany = 16;
  SbSocketWaiter waiters[kMany] = {0};
  for (int i = 0; i < kMany; ++i) {
    waiters[i] = SbSocketWaiterCreate();
    EXPECT_TRUE(SbSocketWaiterIsValid(waiters[i]));
  }

  for (int i = 0; i < kMany; ++i) {
    EXPECT_TRUE(SbSocketWaiterDestroy(waiters[i]));
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
