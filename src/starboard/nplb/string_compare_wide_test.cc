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

// Here we are not trying to do anything fancy, just to really sanity check that
// this is hooked up to something.

#include "starboard/string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbStringCompareWideTest, SunnyDay) {
  const wchar_t kString1[] = L"0123456788";
  const wchar_t kString2[] = L"0123456789";
  EXPECT_EQ(0, SbStringCompareWide(kString1, kString1,
                                   SbStringGetLengthWide(kString1)));
  EXPECT_GT(0, SbStringCompareWide(kString1, kString2,
                                   SbStringGetLengthWide(kString1)));
  EXPECT_LT(0, SbStringCompareWide(kString2, kString1,
                                   SbStringGetLengthWide(kString2)));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
