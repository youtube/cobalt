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

// Here we are not trying to do anything comprehensive, just to sanity check
// that this is hooked up to something.

#include "starboard/common/string.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_API_VERSION < 13

namespace starboard {
namespace nplb {
namespace {

TEST(SbStringParseSignedIntegerTest, SunnyDayBase10) {
  const char kSignedInteger[] = "  -19873612xxxxx";
  const long kExpected = -19873612;  // NOLINT(runtime/int)
  char* end = NULL;
  EXPECT_EQ(kExpected, SbStringParseSignedInteger(kSignedInteger, &end, 10));
  EXPECT_EQ(kSignedInteger + 11, end);
}

TEST(SbStringParseSignedIntegerTest, SunnyDayBase16) {
  const char kSignedInteger[] = "  -FEDF00DG";
  const long kExpected = -0xFEDF00DL;  // NOLINT(runtime/int)
  char* end = NULL;
  EXPECT_EQ(kExpected, SbStringParseSignedInteger(kSignedInteger, &end, 16));
  EXPECT_EQ(kSignedInteger + 10, end);
}

TEST(SbStringParseSignedIntegerTest, SunnyDayNoDigits) {
  const char kSignedInteger[] = "abc123";
  const long kExpected = 0;  // NOLINT(runtime/int)
  char* end = NULL;
  EXPECT_EQ(kExpected, SbStringParseSignedInteger(kSignedInteger, &end, 10));
  EXPECT_EQ(kSignedInteger, end);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_API_VERSION < 13
