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
#include "testing/gtest/include/gtest/gtest.h"

#if SB_API_VERSION < 13

namespace starboard {
namespace nplb {
namespace {

TEST(SbStringCompareAllTest, SunnyDay) {
  const char kString1[] = "0123456788";
  const char kString2[] = "0123456789";
  EXPECT_EQ(0, SbStringCompareAll(kString1, kString1));
  EXPECT_GT(0, SbStringCompareAll(kString1, kString2));
  EXPECT_LT(0, SbStringCompareAll(kString2, kString1));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_API_VERSION < 13
