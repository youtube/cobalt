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

#include "starboard/nplb/posix_compliance/posix_timezone_test_helpers.h"
#include "starboard/nplb/posix_compliance/scoped_tz_set.h"

namespace nplb {

TEST(PosixTimezoneTests, HandlesUnsetTZEnvironmentVariable) {
  ScopedTzSet tz_manager(nullptr);
  // We can't assert specific values as they depend on the system's default.
  // We can assert that tzname pointers are not null.
  ASSERT_NE(tzname[0], nullptr);
  EXPECT_GT(strlen(tzname[0]), 0U);
  ASSERT_NE(tzname[1], nullptr);
}

TEST(PosixTimezoneTests, TimezoneIsNotSet) {
  // Note: We are not setting the TZ environment variable to any value.
  ScopedTzSet tz_manager(nullptr);
  tzset();
  // The value of |timezone| is unspecified if TZ is not set.
  // No assertion can be made.
}

TEST(PosixTimezoneTests, MultipleScopedTzSetCallsEnsureCorrectUpdates) {
  // Test that multiple ScopedTzSet objects correctly save and restore the TZ
  // environment variable.
  {
    ScopedTzSet tz_manager_pst("PST8PDT");
    struct tm result_tm = {};
    time_t time_in_s = 0;
    localtime_r(&time_in_s, &result_tm);
    EXPECT_EQ(result_tm.tm_gmtoff, -8 * kSecondsInHour);
  }
  // After tz_manager_pst goes out of scope, the original TZ should be
  // restored.
  {
    ScopedTzSet tz_manager_est("EST5EDT");
    struct tm result_tm = {};
    time_t time_in_s = 0;
    localtime_r(&time_in_s, &result_tm);
    EXPECT_EQ(result_tm.tm_gmtoff, -5 * kSecondsInHour);
  }
  // After tz_manager_est goes out of scope, the original TZ should be
  // restored.
  {
    ScopedTzSet tz_manager_utc("UTC0");
    struct tm result_tm = {};
    time_t time_in_s = 0;
    localtime_r(&time_in_s, &result_tm);
    EXPECT_EQ(result_tm.tm_gmtoff, 0);
  }
}

TEST(PosixTimezoneTests, InvalidTimezone) {
  ScopedTzSet tz_manager("ThisIsClearlyInvalid!@#123");
  // The value of |timezone| is unspecified for invalid TZ values.
  // No assertion can be made.
}

TEST(PosixTimezoneTests, HandlesInvalidTZStringGracefully) {
  // POSIX does not specify behavior for invalid TZ strings.
  ScopedTzSet tz_manager("ThisIsClearlyInvalid!@#123");
  // We only check that there is not a crash, that tzname is not nullptr, and
  // that timezone is set to 0.
  ASSERT_NE(tzname[0], nullptr);
  EXPECT_EQ(timezone, 0L);
}

}  // namespace nplb
