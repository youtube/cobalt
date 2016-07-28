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

#include "starboard/nplb/time_constants.h"
#include "starboard/time_zone.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbTimeZoneGetNameTest, IsKindOfSane) {
  const char* name = SbTimeZoneGetName();

  ASSERT_NE(name, static_cast<const char*>(NULL));

  int i = 0;
  while (name[i] != '\0') {
    ++i;
  }

  // Most time zones are 3 letters.
  // I haven't been able to get Linux to produce a 2 or 1 letter time zone.
  EXPECT_GE(i, 3);

  // Some, like WART for Western Argentina, are 4.
  // A very few, like ANAST, or CHAST is 5
  // http://www.timeanddate.com/time/zones/
  EXPECT_LE(i, 5);

  // On Linux, TZ=":Pacific/Chatham" is a good test of boundary conditions.
  // ":Pacific/Kiritimati" is the western-most timezone at UTC+14.
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
