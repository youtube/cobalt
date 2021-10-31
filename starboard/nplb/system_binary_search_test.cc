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

// Here we are not trying to do anything fancy, just to really sanity check that
// this is hooked up to something.

#include "starboard/common/string.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_API_VERSION < 13

namespace starboard {
namespace nplb {
namespace {

const char kSorted[] = "abcdefghijklmnopqrstuvwxyz";

int CharComparator(const void* a, const void* b) {
  const char* ac = reinterpret_cast<const char*>(a);
  const char* bc = reinterpret_cast<const char*>(b);
  return *ac - *bc;
}

TEST(SbSystemBinarySearchTest, SunnyDayLetters) {
  char buf[SB_ARRAY_SIZE(kSorted)] = {0};
  starboard::strlcpy(buf, kSorted, SB_ARRAY_SIZE(buf));
  void* result = SbSystemBinarySearch("k", kSorted, SB_ARRAY_SIZE(kSorted) - 1,
                                      1, CharComparator);
  EXPECT_EQ(result, kSorted + ('k' - 'a'));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // #if SB_API_VERSION < 13
