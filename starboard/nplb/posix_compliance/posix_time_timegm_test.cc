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
  struct tm tm = {};
  tm.tm_year = 70;  // 1970
  tm.tm_mon = 0;    // January
  tm.tm_mday = 32;  // 32nd day
  tm.tm_hour = 0;
  tm.tm_min = 0;
  tm.tm_sec = 0;
  tm.tm_isdst = 0;

  time_t result = timegm(&tm);
  EXPECT_EQ(result, 2678400);
  EXPECT_EQ(tm.tm_mon, 1);
  EXPECT_EQ(tm.tm_mday, 1);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
