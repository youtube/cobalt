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

TEST_F(IcuTimeSupportTest, HandlesNormalization) {
  IcuTimeSupport* time_support = IcuTimeSupport::GetInstance();

  // The time being tested is:
  // Dec+1 33 27:64:66 UTC 2023
  // All members except year overflow and have to be normalized.
  struct tm start_tm = {.tm_sec = 66,
                        .tm_min = 64,
                        .tm_hour = 27,
                        .tm_mday = 33,   // 33nd day
                        .tm_mon = 12,    // December + 1
                        .tm_year = 123,  // 2023 - 1900 = 123
                        .tm_isdst = 1};
  struct tm tm(start_tm);

  time_t imploded = time_support->ImplodeGmtTime(&tm);

  // date -u -d "2024-02-03 04:05:06 UTC" +%s
  EXPECT_EQ(imploded, 1706933106);

  // All members have to be normalized.
  // $ date -u --date='@1706933106'
  // Sat Feb  3 04:05:06 AM UTC 2024
  EXPECT_EQ(tm.tm_year, 2024 - 1900);
  EXPECT_EQ(tm.tm_mon, 1);    // February (December + 1 + overflow)
  EXPECT_EQ(tm.tm_mday, 3);   // 33 - 31 + overflow = 3
  EXPECT_EQ(tm.tm_hour, 4);   // 27 - 24 + overflow = 4
  EXPECT_EQ(tm.tm_min, 5);    // 64 - 60 + overflow = 5
  EXPECT_EQ(tm.tm_sec, 6);    // 66 - 60 = 6
  EXPECT_EQ(tm.tm_wday, 6);   // Saturday
  EXPECT_EQ(tm.tm_yday, 33);  // 31 + 2 = 33nd day starting at day 0.
  EXPECT_EQ(tm.tm_isdst, 0);  // UTC is never daylight savings time.
}

TEST_F(IcuTimeSupportTest, HandlesNegativeNormalization) {
  IcuTimeSupport* time_support = IcuTimeSupport::GetInstance();

  // The time being tested is:
  // Jan-1 -4 -3:-2:-1 UTC 2023
  // All members except year overflow and have to be normalized.
  struct tm start_tm = {.tm_sec = -1,
                        .tm_min = -2,
                        .tm_hour = -3,
                        .tm_mday = -4,
                        .tm_mon = -1,    // January - 1
                        .tm_year = 123,  // 2023 - 1900 = 123
                        .tm_isdst = 1};
  struct tm tm(start_tm);

  time_t imploded = time_support->ImplodeGmtTime(&tm);

  // date -u -d "2022-11-25 20:57:59 UTC" +%s
  EXPECT_EQ(imploded, 1669409879);

  // All members have to be normalized.
  // $ date -u --date='@1669409879'
  // Fri Nov 25 08:57:59 PM UTC 2022
  EXPECT_EQ(tm.tm_year, 2022 - 1900);
  EXPECT_EQ(tm.tm_mon, 10);    // November (January - 1 - overflow)
  EXPECT_EQ(tm.tm_mday, 25);   // -4 + 30 - overflow = 25
  EXPECT_EQ(tm.tm_hour, 20);   // -3 + 24 - overflow = 20
  EXPECT_EQ(tm.tm_min, 57);    // -2 + 60 - overflow = 57
  EXPECT_EQ(tm.tm_sec, 59);    // -1 + 60 = 59
  EXPECT_EQ(tm.tm_wday, 5);    // Friday
  EXPECT_EQ(tm.tm_yday, 328);  // 6*31 + 3*30 + 28 + 24 = 328nd day.
  EXPECT_EQ(tm.tm_isdst, 0);   // UTC is never daylight savings time.
}

}  // namespace time
}  // namespace libc
}  // namespace common
}  // namespace cobalt
