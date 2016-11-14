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

#include "starboard/string.h"
#include "starboard/memory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

void RunTest(const char* input) {
  char* dupe = SbStringDuplicate(input);
  const char* kNull = NULL;
  EXPECT_NE(kNull, dupe);
  EXPECT_EQ(0, SbStringCompareNoCase(input, dupe));
  EXPECT_EQ(SbStringGetLength(input), SbStringGetLength(dupe));
  SbMemoryDeallocate(dupe);
}

TEST(SbStringDuplicateTest, SunnyDay) {
  RunTest("0123456789");
  RunTest("0");
}

TEST(SbStringDuplicateTest, SunnyDayEmpty) {
  RunTest("");
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
