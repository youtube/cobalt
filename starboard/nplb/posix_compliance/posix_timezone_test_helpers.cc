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
// limitations under the License

#include "starboard/nplb/posix_compliance/posix_timezone_test_helpers.h"

namespace starboard {
namespace nplb {

time_t CreateTime(int year, int month, int day) {
  struct tm tminfo = {0};
  tminfo.tm_year = year - 1900;
  tminfo.tm_mon = month - 1;
  tminfo.tm_mday = day;
  return timegm(&tminfo);
}

TimeSamples GetTimeSamples(int year) {
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
void AssertTM(const tm& tminfo,
              long offset,
              const char* std_zone,
              const std::optional<const char*>& dst_zone,
              const char* tz,
              SourceLocation location) {
  ASSERT_NE(tminfo.tm_zone, nullptr)
      << location << " tm_zone can not be nullptr for " << tz;
  EXPECT_EQ(tminfo.tm_gmtoff, -offset)
      << location << " tm_gmtoff mismatch for " << tz;
  if (dst_zone.has_value()) {
    EXPECT_STREQ(tminfo.tm_zone, dst_zone.value())
        << location << "Daylight time tm_zone mismatch for " << tz;
    EXPECT_GT(tminfo.tm_isdst, 0)
        << location << "Daylight time tm_isdst should be > 0 for " << tz;
  } else {
    EXPECT_STREQ(tminfo.tm_zone, std_zone)
        << location << "Standard time tm_zone mismatch for " << tz;
    EXPECT_EQ(tminfo.tm_isdst, 0)
        << location << "Standard time tm_isdst should be 0 for " << tz;
  }
}

}  // namespace nplb
}  // namespace starboard
