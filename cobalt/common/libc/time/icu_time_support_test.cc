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

#include "cobalt/common/libc/time/icu_time_support.h"

#include <gtest/gtest.h>
#include <stdlib.h>
#include <time.h>

#include <string>

namespace cobalt {
namespace common {
namespace libc {
namespace time {

class IcuTimeSupportTest : public ::testing::Test {
 protected:
  void SetUp() override { original_tz_ = getenv("TZ"); }

  void TearDown() override {
    if (original_tz_) {
      setenv("TZ", original_tz_, 1);
    } else {
      unsetenv("TZ");
    }
  }

  const char* original_tz_ = nullptr;
};

TEST_F(IcuTimeSupportTest, GmTimeConversion) {
  IcuTimeSupport* time_support = IcuTimeSupport::GetInstance();
  // The time being tested is:
  // Wed Mar 15 01:20:00 PM UTC 2023
  struct tm start_tm = {
      .tm_sec = 0,
      .tm_min = 20,
      .tm_hour = 13,
      .tm_mday = 15,
      .tm_mon = 2,     // March
      .tm_year = 123,  // 2023 - 1900 = 123
  };

  time_t imploded = time_support->ImplodeGmtTime(&start_tm);

  // date -u --date="Wed Mar 15 01:20:00 PM UTC 2023" +%s
  EXPECT_EQ(imploded, 1678886400);

  struct tm exploded;
  ASSERT_TRUE(time_support->ExplodeGmtTime(&imploded, &exploded));

  EXPECT_EQ(exploded.tm_year, start_tm.tm_year);
  EXPECT_EQ(exploded.tm_mon, start_tm.tm_mon);
  EXPECT_EQ(exploded.tm_mday, start_tm.tm_mday);
  EXPECT_EQ(exploded.tm_hour, start_tm.tm_hour);
  EXPECT_EQ(exploded.tm_min, start_tm.tm_min);
  EXPECT_EQ(exploded.tm_sec, start_tm.tm_sec);
  EXPECT_EQ(exploded.tm_wday, 3);  // Wednesday
}

TEST_F(IcuTimeSupportTest, LocalTimeConversion) {
  setenv("TZ", "America/New_York", 1);
  IcuTimeSupport* time_support = IcuTimeSupport::GetInstance();

  // The time being tested is:
  // Wed Mar 15 09:20:00 AM EDT 2023
  struct tm start_tm = {
      .tm_sec = 0,
      .tm_min = 20,
      .tm_hour = 9,
      .tm_mday = 15,
      .tm_mon = 2,     // March
      .tm_year = 123,  // 2023 - 1900 = 123
      .tm_isdst = 1,
  };

  time_t imploded = time_support->ImplodeLocalTime(&start_tm);

  // TZ="America/New_York" date --date="Wed Mar 15 09:20:00 AM EDT 2023" +%s
  EXPECT_EQ(imploded, 1678886400);

  struct tm exploded;
  ASSERT_TRUE(time_support->ExplodeLocalTime(&imploded, &exploded));

  EXPECT_EQ(exploded.tm_year, start_tm.tm_year);
  EXPECT_EQ(exploded.tm_mon, start_tm.tm_mon);
  EXPECT_EQ(exploded.tm_mday, start_tm.tm_mday);
  EXPECT_EQ(exploded.tm_hour, start_tm.tm_hour);
  EXPECT_EQ(exploded.tm_min, start_tm.tm_min);
  EXPECT_EQ(exploded.tm_sec, start_tm.tm_sec);
  EXPECT_EQ(exploded.tm_wday, 3);  // Wednesday
  EXPECT_EQ(exploded.tm_isdst, start_tm.tm_isdst);
}

TEST_F(IcuTimeSupportTest, UpdateFromEnvironment) {
  setenv("TZ", "America/New_York", 1);
  IcuTimeSupport* time_support = IcuTimeSupport::GetInstance();

  long timezone_sec;
  int daylight;
  char* tzname[2];
  time_support->GetPosixTimezoneGlobals(timezone_sec, daylight, tzname);
  EXPECT_EQ(daylight, 1);

  setenv("TZ", "America/Los_Angeles", 1);
  time_support->GetPosixTimezoneGlobals(timezone_sec, daylight, tzname);
  EXPECT_EQ(daylight, 1);

  setenv("TZ", "UTC", 1);
  time_support->GetPosixTimezoneGlobals(timezone_sec, daylight, tzname);
  EXPECT_EQ(daylight, 0);
}

}  // namespace time
}  // namespace libc
}  // namespace common
}  // namespace cobalt
