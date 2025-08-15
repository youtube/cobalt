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
#include "starboard/nplb/posix_compliance/posix_time_helper.h"

namespace starboard {
namespace nplb {
namespace {

TEST(PosixTimegmTest, HandlesEpochTime) {
  struct tm tm = {};
  tm.tm_year = 70;  // 1970
  tm.tm_mon = 0;    // January
  tm.tm_mday = 1;
  tm.tm_hour = 0;
  tm.tm_min = 0;
  tm.tm_sec = 0;
  tm.tm_isdst = 0;

  time_t result = timegm(&tm);
  EXPECT_EQ(result, 0);
}

TEST(PosixTimegmTest, HandlesSpecificKnownPositiveTime) {
  struct tm tm = {};
  tm.tm_year = 123;  // 2023
  tm.tm_mon = 9;     // October
  tm.tm_mday = 26;
  tm.tm_hour = 10;
  tm.tm_min = 30;
  tm.tm_sec = 0;
  tm.tm_isdst = 0;

  time_t result = timegm(&tm);

  // date -u -d "2023-10-26 10:30:00 UTC" +%s
  EXPECT_EQ(result, 1698316200);
}

TEST(PosixTimegmTest, HandlesNegativeTimeBeforeEpoch) {
  struct tm tm = {};
  tm.tm_year = 69;  // 1969
  tm.tm_mon = 11;   // December
  tm.tm_mday = 31;
  tm.tm_hour = 0;
  tm.tm_min = 0;
  tm.tm_sec = 0;
  tm.tm_isdst = 0;

  time_t result = timegm(&tm);
  EXPECT_EQ(result, -kSecondsInDay);
}

TEST(PosixTimegmTest, HandlesNormalization) {
  // The time being tested is:
  // Dec+1 33 27:64:66 UTC 2023
  // all members except year overflow and have to be normalized.
  struct tm start_tm = {.tm_sec = 66,
                        .tm_min = 64,
                        .tm_hour = 27,
                        .tm_mday = 33,   // 33nd day
                        .tm_mon = 12,    // December + 1
                        .tm_year = 123,  // 2023 - 1900 = 123
                        .tm_isdst = 1};
  struct tm tm(start_tm);

  time_t result = timegm(&tm);

  // date -u -d "2024-02-03 04:05:06 UTC" +%s
  EXPECT_EQ(result, 1706933106);

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

TEST(PosixTimegmTest, HandlesNegativeNormalization) {
  // The time being tested is:
  // Jan-1 -4 -3:-2:-1 UTC 2023
  // all members except year overflow and have to be normalized.
  struct tm start_tm = {.tm_sec = -1,
                        .tm_min = -2,
                        .tm_hour = -3,
                        .tm_mday = -4,
                        .tm_mon = -1,    // January - 1
                        .tm_year = 123,  // 2023 - 1900 = 123
                        .tm_isdst = 1};
  struct tm tm(start_tm);

  time_t result = timegm(&tm);

  // date -u -d "2022-11-25 20:57:59 UTC" +%s
  EXPECT_EQ(result, 1669409879);

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

}  // namespace
}  // namespace nplb
}  // namespace starboard
