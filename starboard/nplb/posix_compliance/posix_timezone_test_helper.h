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

#ifndef STARBOARD_NPLB_POSIX_COMPLIANCE_POSIX_TIMEZONE_TEST_HELPER_H_
#define STARBOARD_NPLB_POSIX_COMPLIANCE_POSIX_TIMEZONE_TEST_HELPER_H_

#include <gtest/gtest.h>
#include <stdlib.h>
#include <time.h>
#include <optional>
#include <string>
#include <type_traits>

namespace starboard {
namespace nplb {

// The declarations for tzname, timezone, and daylight are provided by <time.h>.
// We use static_assert to ensure they have the expected types as defined by
// POSIX.

// POSIX defines tzname as "char *tzname[]", but both musl and glibc define it
// as "char *tzname[2]". This check allows either.
static_assert(std::is_same_v<decltype(tzname), char* [2]> ||
                  std::is_same_v<decltype(tzname), char*[]>,
              "Type of tzname must be 'char* [2]' or 'char* []'.");

static_assert(std::is_same_v<decltype(timezone), long>,
              "Type of timezone must be 'long'.");
static_assert(std::is_same_v<decltype(daylight), int>,
              "Type of daylight must be 'int'.");

// Number of seconds in an hour.
constexpr int kSecondsInHour = 3600;
// Number of seconds in a minute.
constexpr int kSecondsInMinute = 60;

// Helper to create a time_t for a specific UTC date.
__attribute__((unused)) static time_t CreateTime(int year, int month, int day) {
  struct tm tminfo = {0};
  tminfo.tm_year = year - 1900;
  tminfo.tm_mon = month - 1;
  tminfo.tm_mday = day;
  return timegm(&tminfo);
}

struct TimeSamples {
  std::optional<time_t> standard;
  std::optional<time_t> daylight;
};

// Helper to deterministically find a sample of standard and daylight time
// for a given year.
__attribute__((unused)) static TimeSamples GetTimeSamples(int year) {
  TimeSamples samples;
  for (int month = 1; month <= 12; ++month) {
    time_t current_time = CreateTime(year, month, 1);
    struct tm* tm_current = localtime(&current_time);
    if (!tm_current) {
      continue;
    }

    if (tm_current->tm_isdst > 0) {
      if (!samples.daylight) {
        samples.daylight = current_time;
      }
    } else {
      if (!samples.standard) {
        samples.standard = current_time;
      }
    }
  }
  return samples;
}

// Helper to assert the values of a tm struct.
__attribute__((unused)) static void AssertTM(
    const tm& tminfo,
    long offset,
    const char* zone,
    bool is_dst,
    const std::optional<const char*>& dst_zone,
    const char* tz,
    const char* message_suffix = "") {
  if (is_dst) {
    EXPECT_EQ(tminfo.tm_gmtoff, -offset + 3600)
        << "Daylight time gmtoff mismatch for " << tz << message_suffix;
    ASSERT_NE(tminfo.tm_zone, nullptr);
    EXPECT_STREQ(tminfo.tm_zone, dst_zone.value())
        << "Daylight time tm_zone mismatch for " << tz << message_suffix;
    EXPECT_GT(tminfo.tm_isdst, 0)
        << "Daylight time tm_isdst should be > 0 for " << tz << message_suffix;
  } else {
    EXPECT_EQ(tminfo.tm_gmtoff, -offset)
        << "Standard time gmtoff mismatch for " << tz << message_suffix;
    ASSERT_NE(tminfo.tm_zone, nullptr);
    EXPECT_STREQ(tminfo.tm_zone, zone)
        << "Standard time tm_zone mismatch for " << tz << message_suffix;
    EXPECT_EQ(tminfo.tm_isdst, 0)
        << "Standard time tm_isdst should be 0 for " << tz << message_suffix;
  }
}

}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_POSIX_COMPLIANCE_POSIX_TIMEZONE_TEST_HELPER_H_
