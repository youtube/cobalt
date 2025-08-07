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
  // The timestamp being tested is:
  // $ date -u --date='@1678886400'
  // Wed Mar 15 01:20:00 PM UTC 2023
  time_t t = 1678886400;

  struct tm exploded;
  ASSERT_TRUE(time_support->ExplodeGmtTime(&t, &exploded));

  EXPECT_EQ(exploded.tm_year, 123);
  EXPECT_EQ(exploded.tm_mon, 2);
  EXPECT_EQ(exploded.tm_mday, 15);
  EXPECT_EQ(exploded.tm_hour, 13);
  EXPECT_EQ(exploded.tm_min, 20);
  EXPECT_EQ(exploded.tm_sec, 0);
  EXPECT_EQ(exploded.tm_wday, 3);

  time_t imploded = time_support->ImplodeGmtTime(&exploded);
  EXPECT_EQ(t, imploded);
}

TEST_F(IcuTimeSupportTest, LocalTimeConversion) {
  setenv("TZ", "America/New_York", 1);
  IcuTimeSupport* time_support = IcuTimeSupport::GetInstance();

  // The timestamp being tested is:
  // $ TZ="America/New_York" date --date='@1678886400'
  // Wed Mar 15 09:20:00 AM EDT 2023
  time_t t = 1678886400;

  struct tm exploded;
  ASSERT_TRUE(time_support->ExplodeLocalTime(&t, &exploded));

  EXPECT_EQ(exploded.tm_year, 123);
  EXPECT_EQ(exploded.tm_mon, 2);
  EXPECT_EQ(exploded.tm_mday, 15);
  EXPECT_EQ(exploded.tm_hour, 9);
  EXPECT_EQ(exploded.tm_min, 20);
  EXPECT_EQ(exploded.tm_sec, 0);
  EXPECT_EQ(exploded.tm_wday, 3);
  EXPECT_EQ(exploded.tm_isdst, 1);

  time_t imploded = time_support->ImplodeLocalTime(&exploded);
  EXPECT_EQ(t, imploded);
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
