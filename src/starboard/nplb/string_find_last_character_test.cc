// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "starboard/string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

const char kTestString[] = "012345678901234567890123456789";
const char* kNull = NULL;

TEST(SbStringFindLastCharacterTest, SunnyDay) {
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(kTestString + SbStringGetLength(kTestString) - 10 + i,
              SbStringFindLastCharacter(kTestString, '0' + i));
  }
}

TEST(SbStringFindLastCharacterTest, RainyDayNotFound) {
  EXPECT_EQ(kNull, SbStringFindLastCharacter(kTestString, 'X'));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
