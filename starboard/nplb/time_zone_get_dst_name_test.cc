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

#if SB_API_VERSION < SB_TIME_ZONE_FLEXIBLE_API_VERSION
TEST(SbTimeZoneGetDstNameTest, IsKindOfSane) {
  const char* name = SbTimeZoneGetDstName();

  ASSERT_NE(name, static_cast<const char*>(NULL));

  int i = 0;
  while (name[i] != '\0') {
    ++i;
  }

  // See SbTimeZoneGetNameTest for more discussion.
  EXPECT_GE(i, 3);
  EXPECT_LE(i, 5);
}
#endif  // SB_API_VERSION < SB_TIME_ZONE_FLEXIBLE_API_VERSION

}  // namespace
}  // namespace nplb
}  // namespace starboard
