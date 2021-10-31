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

#include "starboard/common/string.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_API_VERSION < 13

namespace starboard {
namespace nplb {
namespace {

const char kTestString[] = "012345678901234567890123456789";
const char* kNull = NULL;

TEST(SbStringFindCharacterTest, SunnyDay) {
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(kTestString + i, SbStringFindCharacter(kTestString, '0' + i));
  }
}

TEST(SbStringFindCharacterTest, RainyDayNotFound) {
  EXPECT_EQ(kNull, SbStringFindCharacter(kTestString, 'X'));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_API_VERSION < 13
