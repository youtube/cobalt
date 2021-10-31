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
const char kTestSubstring[] = "012345678901234567890123456789";
const char kTestSubstring2[] = "01234";
const char* kNull = NULL;

TEST(SbStringFindStringTest, SunnyDay) {
  const int kMaxLength = 10;
  for (int length = 1; length < kMaxLength; ++length) {
    for (int i = 0; i < kMaxLength; ++i) {
      char term[kMaxLength] = {0};
      for (int j = 0; j < length; ++j) {
        term[j] = '0' + (i + j) % kMaxLength;
      }

      EXPECT_EQ(kTestString + i, SbStringFindString(kTestString, term))
          << "i = " << i << ", length = " << length << ", term = \"" << term
          << "\"";
    }
  }

  EXPECT_EQ(kTestString, SbStringFindString(kTestString, kTestSubstring));
  EXPECT_EQ(kTestString, SbStringFindString(kTestString, kTestSubstring2));
  EXPECT_EQ(kTestString, SbStringFindString(kTestString, kTestString));
  EXPECT_EQ(kTestString, SbStringFindString(kTestString, ""));
  EXPECT_EQ(kTestString + 9, SbStringFindString(kTestString, kTestString + 9));
}

TEST(SbStringFindStringTest, RainyDayNotFound) {
  EXPECT_EQ(kNull, SbStringFindString(kTestString, "987"));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_API_VERSION < 13
