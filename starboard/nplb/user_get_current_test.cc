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

#include "starboard/user.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbUserGetCurrentTest, SunnyDay) {
  SbUser users[10] = {0};
  int result = SbUserGetSignedIn(users, SB_ARRAY_SIZE_INT(users));
  EXPECT_LE(0, result);

  SbUser current = SbUserGetCurrent();
  if (result > 0) {
    EXPECT_NE(kSbUserInvalid, current);
  } else {
    EXPECT_EQ(kSbUserInvalid, current);
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
