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

#include "starboard/memory.h"
#include "starboard/user.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

void TestProperty(SbUser user, SbUserPropertyId property_id) {
  int size = SbUserGetPropertySize(user, property_id);
  EXPECT_LE(0, size);

  if (size > 0) {
    char* property = new char[size + 10];
    memset(property, 0, size + 10);
    EXPECT_TRUE(SbUserGetProperty(user, property_id, property, size));
    for (int i = 0; i < size - 1; ++i) {
      EXPECT_NE(property[i], '\0') << "position " << i;
    }
    for (int i = size - 1; i < size + 10; ++i) {
      EXPECT_EQ(property[i], '\0') << "position " << i;
    }
    delete[] property;
  } else {
    char property[10] = {0};
    bool result = SbUserGetProperty(user, property_id, property, 10);
    for (int i = 0; i < size; ++i) {
      EXPECT_EQ(property[i], '\0') << "position " << i;
    }
  }
}

TEST(SbUserGetPropertyTest, SunnyDay) {
  SbUser current = SbUserGetCurrent();
  if (!SbUserIsValid(current)) {
    // Sadly, we can't really test in this case.
    return;
  }

  TestProperty(current, kSbUserPropertyUserName);
  TestProperty(current, kSbUserPropertyUserId);
  TestProperty(current, kSbUserPropertyAvatarUrl);
}

TEST(SbUserGetPropertyTest, MultipleTimes) {
  SbUser current = SbUserGetCurrent();
  if (!SbUserIsValid(current)) {
    // Sadly, we can't really test in this case.
    return;
  }

  TestProperty(current, kSbUserPropertyUserName);
  TestProperty(current, kSbUserPropertyUserName);

  TestProperty(current, kSbUserPropertyUserId);
  TestProperty(current, kSbUserPropertyUserId);

  TestProperty(current, kSbUserPropertyAvatarUrl);
  TestProperty(current, kSbUserPropertyAvatarUrl);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
