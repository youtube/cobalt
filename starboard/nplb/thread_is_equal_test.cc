// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/nplb/thread_helpers.h"
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbThreadIsEqualTest, Everything) {
  EXPECT_TRUE(SbThreadIsEqual(SbThreadGetCurrent(), SbThreadGetCurrent()));

  SbThread thread = SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity,
                                   true, NULL, AddOneEntryPoint, NULL);
  EXPECT_TRUE(SbThreadIsValid(thread));

  EXPECT_TRUE(SbThreadIsEqual(thread, thread));
  EXPECT_FALSE(SbThreadIsEqual(SbThreadGetCurrent(), thread));
  EXPECT_FALSE(SbThreadIsEqual(thread, SbThreadGetCurrent()));

  EXPECT_TRUE(SbThreadJoin(thread, NULL));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
