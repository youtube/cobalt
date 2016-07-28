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

// Here we are not trying to do anything fancy, just to really sanity check that
// this is hooked up to something.

#include "starboard/string.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

const char kScrambled[] = "qremabhitnklopuyzcjsdvwxfg";
const char kSorted[] = "abcdefghijklmnopqrstuvwxyz";
SB_COMPILE_ASSERT(SB_ARRAY_SIZE(kScrambled) == SB_ARRAY_SIZE(kSorted),
                  input_and_output_same_length);

int CharComparator(const void* a, const void* b) {
  const char* ac = reinterpret_cast<const char*>(a);
  const char* bc = reinterpret_cast<const char*>(b);
  return *ac - *bc;
}

TEST(SbSystemSortTest, SunnyDayLetters) {
  char buf[SB_ARRAY_SIZE(kSorted)] = {0};
  SbStringCopy(buf, kScrambled, SB_ARRAY_SIZE(buf));
  SbSystemSort(buf, SB_ARRAY_SIZE(buf) - 1, 1, &CharComparator);
  EXPECT_TRUE(SbStringCompareAll(kSorted, kScrambled));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
