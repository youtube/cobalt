// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/nplb/posix_compliance/posix_timezone_test_helper.h"

#include <gtest/gtest.h>
#include <string.h>
#include <time.h>
#include <algorithm>
#include <cctype>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <vector>

namespace starboard {
namespace nplb {

TEST(PosixTimezoneTests, HandlesUnsetTZEnvironmentVariable) {
  ScopedTZ tz_manager(nullptr);
  // We can't assert specific values as they depend on the system's default.
  // We can assert that tzname pointers are not null.
  ASSERT_NE(tzname[0], nullptr);
  EXPECT_GT(strlen(tzname[0]), 0U);
  ASSERT_NE(tzname[1], nullptr);
}

TEST(PosixTimezoneTests, MultipleScopedTZCallsEnsureCorrectUpdates) {
  {
    ScopedTZ tz_manager_pst("PST8PDT");
    ASSERT_NE(tzname[0], nullptr);
    EXPECT_STREQ(tzname[0], "PST");
    ASSERT_NE(tzname[1], nullptr);
    EXPECT_STREQ(tzname[1], "PDT");
    EXPECT_EQ(timezone, 8 * kSecondsInHour);
    EXPECT_NE(daylight, 0);
  }
  {
    ScopedTZ tz_manager_est("EST5EDT");
    ASSERT_NE(tzname[0], nullptr);
    EXPECT_STREQ(tzname[0], "EST");
    ASSERT_NE(tzname[1], nullptr);
    EXPECT_STREQ(tzname[1], "EDT");
    EXPECT_EQ(timezone, 5 * kSecondsInHour);
    EXPECT_NE(daylight, 0);
  }
  {
    ScopedTZ tz_manager_utc("UTC0");
    ASSERT_NE(tzname[0], nullptr);
    EXPECT_STREQ(tzname[0], "UTC");
    EXPECT_EQ(timezone, 0L);
    EXPECT_EQ(daylight, 0);
  }
}

TEST(PosixTimezoneTests, HandlesInvalidTZStringGracefully) {
  // POSIX does not specify behavior for invalid TZ strings.
  ScopedTZ tz_manager("ThisIsClearlyInvalid!@#123");
  // We only check that there is not a crash, that tzname is not nullptr, and
  // that timezone is set to 0.
  ASSERT_NE(tzname[0], nullptr);
  EXPECT_EQ(timezone, 0L);
}

}  // namespace nplb
}  // namespace starboard
