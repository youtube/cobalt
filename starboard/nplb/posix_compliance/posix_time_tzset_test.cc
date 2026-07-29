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
#include <time.h>

namespace nplb {
namespace {

// Number of seconds in an hour.
constexpr int kSecondsInHour = 3600;

// Basic tests.
TEST(PosixTzsetSimpleTest, SunnyDay) {
  tzset();
  ASSERT_NE(tzname[0], nullptr);
  EXPECT_STRNE(tzname[0], "");
}

// TODO(b/436371274): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_EnvironmentChange DISABLED_EnvironmentChange
#else
#define MAYBE_EnvironmentChange EnvironmentChange
#endif
TEST(PosixTzsetSimpleTest, MAYBE_EnvironmentChange) {
  const char* original_tz = getenv("TZ");

  setenv("TZ", "UTC", 1);
  tzset();
  EXPECT_EQ(timezone, 0);
  EXPECT_EQ(daylight, 0);
  ASSERT_NE(tzname[0], nullptr);
  EXPECT_STREQ(tzname[0], "UTC");
  ASSERT_NE(tzname[1], nullptr);
  EXPECT_STREQ(tzname[1], "UTC");

  setenv("TZ", "US/Pacific", 1);
  tzset();
  EXPECT_EQ(timezone, 8 * kSecondsInHour);
  EXPECT_EQ(daylight, 1);
  ASSERT_NE(tzname[0], nullptr);
  EXPECT_STREQ(tzname[0], "PST");
  ASSERT_NE(tzname[1], nullptr);
  EXPECT_STREQ(tzname[1], "PDT");

  if (original_tz) {
    setenv("TZ", original_tz, 1);
  } else {
    unsetenv("TZ");
  }
  tzset();
}

}  // namespace
}  // namespace nplb
