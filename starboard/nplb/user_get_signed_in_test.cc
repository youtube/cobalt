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

TEST(SbUserGetSignedInTest, SunnyDay) {
  SbUser users[100] = {0};
  int result = SbUserGetSignedIn(users + 10, SB_ARRAY_SIZE_INT(users) - 10);
  EXPECT_LE(0, result);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(kSbUserInvalid, users[i]);
  }

  for (int i = 0; i < result; ++i) {
    EXPECT_NE(kSbUserInvalid, users[10 + i]);
  }

  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(kSbUserInvalid, users[10 + result + i]);
  }
}

TEST(SbUserGetSignedInTest, NullUsers) {
  SbUser users[100] = {0};
  int result = SbUserGetSignedIn(users, SB_ARRAY_SIZE_INT(users));
  EXPECT_LE(0, result);

  int result2 = SbUserGetSignedIn(NULL, SB_ARRAY_SIZE_INT(users));
  EXPECT_EQ(result, result2);

  SbUser users2[100] = {0};
  int result3 = SbUserGetSignedIn(users2, 0);
  EXPECT_EQ(result, result3);
  for (int i = 0; i < SB_ARRAY_SIZE_INT(users2); ++i) {
    EXPECT_EQ(kSbUserInvalid, users2[i]);
  }
  int result4 = SbUserGetSignedIn(NULL, 0);
  EXPECT_EQ(result, result4);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
