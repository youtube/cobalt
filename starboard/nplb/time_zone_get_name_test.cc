// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

  // "UTC" is not a valid local time zone name.
  EXPECT_NE(name, "UTC");

  int i = 0;
  while (name[i] != '\0') {
    ++i;
  }

  // All time zones are 3 letters or more. This can include names like "PST"
  // or "Pacific Standard Time" or like "America/Los_Angeles"
  EXPECT_GE(i, 3);

  // On Linux, TZ=":Pacific/Chatham" is a good test of boundary conditions.
  // ":Pacific/Kiritimati" is the western-most timezone at UTC+14.
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
