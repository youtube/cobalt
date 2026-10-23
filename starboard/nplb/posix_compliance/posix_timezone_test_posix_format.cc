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
namespace {

struct PosixTestData {
  const char* test_name;
  const char* tz;
  long std_offset;
  const char* std;
  long dst_offset = 0;
  std::optional<const char*> dst = std::nullopt;

  friend void PrintTo(const PosixTestData& data, ::std::ostream* os) {
    *os << "{ test_name: \"" << data.test_name << "\", tz: \"" << data.tz
        << "\", offset: " << data.std_offset << ", std: \"" << data.std << "\"";
    if (data.dst.has_value()) {
      *os << ", dst: \"" << data.dst.value() << "\"";
    } else {
      *os << ", dst: (not checked)";
    }
    *os << " }\n";
  }
};

std::string GetTestName(const ::testing::TestParamInfo<PosixTestData>& info) {
  return info.param.test_name;
}

}  // namespace

class PosixFormat : public ::testing::TestWithParam<PosixTestData> {
 public:
  static const std::array<PosixTestData, 13> kAllTests;
};

const std::array<PosixTestData, 13> PosixFormat::kAllTests = {{
    {"EmptyTZStringAsUTC", "", 0, "UTC"},
    {"LeadingColon", ":UTC0", 0, "UTC"},
    {"ISTMinus530", "IST-5:30", -(5 * kSecondsInHour + 30 * kSecondsInMinute),
     "IST"},
    {"FullHmsOffset", "FOO+08:30:15",
     8 * kSecondsInHour + 30 * kSecondsInMinute + 15, "FOO"},
    {"CST6CDT", "CST6CDT", 6 * kSecondsInHour, "CST", 5 * kSecondsInHour,
     "CDT"},
    {"DefaultDstOffset", "PST8PDT", 8 * kSecondsInHour, "PST",
     7 * kSecondsInHour, "PDT"},
    {"ExplicitDSTOffset", "AAA+5BBB+4", 5 * kSecondsInHour, "AAA",
     4 * kSecondsInHour, "BBB"},
    {"ExplicitNegativeDSTOffset", "AAA-3BBB-2", -3 * kSecondsInHour, "AAA",
     -2 * kSecondsInHour, "BBB"},
    {"QuotedNames", "<UTC+3>-3<-UTC+2>-2", -3 * kSecondsInHour, "UTC+3",
     -2 * kSecondsInHour, "-UTC+2"},
    {"JulianDayDSTRule", "CST6CDT,J60,J300", 6 * kSecondsInHour, "CST",
     5 * kSecondsInHour, "CDT"},
    {"ZeroBasedJulianDayDSTRule", "MST7MDT,59,299", 7 * kSecondsInHour, "MST",
     6 * kSecondsInHour, "MDT"},
    {"DSTRulesEST5EDT", "EST5EDT,M3.2.0/2,M11.1.0/2", 5 * kSecondsInHour, "EST",
     4 * kSecondsInHour, "EDT"},
    {"TransitionRuleWithExplicitTime",
     "CST6CDT,M3.2.0/02:30:00,M11.1.0/03:00:00", 6 * kSecondsInHour, "CST",
     5 * kSecondsInHour, "CDT"},
}};

TEST_P(PosixFormat, TzSetHandles) {
  const auto& param = GetParam();
  ScopedTzSet tz_manager(param.tz);

  EXPECT_EQ(timezone, param.std_offset);

  bool expected_daylight = param.dst.has_value();
  EXPECT_EQ(daylight, expected_daylight);

  ASSERT_NE(tzname[0], nullptr);
  EXPECT_STREQ(tzname[0], param.std);

  if (param.dst.has_value()) {
    ASSERT_NE(tzname[1], nullptr);
    EXPECT_STREQ(tzname[1], param.dst.value());
  }
}

TEST_P(PosixFormat, Localtime) {
  const auto& param = GetParam();
  ScopedTzSet tz_manager(param.tz);

  TimeSamples samples = GetTimeSamples(2023);

  // Every timezone must have a standard time.
  ASSERT_TRUE(samples.standard.has_value());
  struct tm* tm_standard = localtime(&samples.standard.value());
  ASSERT_NE(tm_standard, nullptr);
  AssertTM(*tm_standard, param.std_offset, param.std, std::nullopt, param.tz);

  if (param.dst.has_value()) {
    // If the test data expects DST, we must have found a DST sample.
    ASSERT_TRUE(samples.daylight.has_value())
        << "Test data has DST, but no DST time was found for " << param.tz;
    struct tm* tm_daylight = localtime(&samples.daylight.value());
    ASSERT_NE(tm_daylight, nullptr);
    AssertTM(*tm_daylight, param.dst_offset, param.std, param.dst, param.tz);
  } else {
    // If the test data does not expect DST, we must not have found one.
    ASSERT_FALSE(samples.daylight.has_value())
        << "Test data has no DST, but a DST time was found for " << param.tz;
  }
}

TEST_P(PosixFormat, Localtime_r) {
  const auto& param = GetParam();
  ScopedTzSet tz_manager(param.tz);

  TimeSamples samples = GetTimeSamples(2023);

  // Every timezone must have a standard time.
  ASSERT_TRUE(samples.standard.has_value());
  struct tm tm_standard_r;
  struct tm* tm_standard_r_res =
      localtime_r(&samples.standard.value(), &tm_standard_r);
  ASSERT_EQ(tm_standard_r_res, &tm_standard_r);
  AssertTM(tm_standard_r, param.std_offset, param.std, std::nullopt, param.tz);

  if (param.dst.has_value()) {
    // If the test data expects DST, we must have found a DST sample.
    ASSERT_TRUE(samples.daylight.has_value())
        << "Test data has DST, but no DST time was found for " << param.tz;
    struct tm tm_daylight_r;
    struct tm* tm_daylight_r_res =
        localtime_r(&samples.daylight.value(), &tm_daylight_r);
    ASSERT_EQ(tm_daylight_r_res, &tm_daylight_r);
    AssertTM(tm_daylight_r, param.dst_offset, param.std, param.dst, param.tz);
  } else {
    // If the test data does not expect DST, we must not have found one.
    ASSERT_FALSE(samples.daylight.has_value())
        << "Test data has no DST, but a DST time was found for " << param.tz;
  }
}

TEST_P(PosixFormat, Mktime) {
  const auto& param = GetParam();
  ScopedTzSet tz_manager(param.tz);

  TimeSamples samples = GetTimeSamples(2023);

  // Every timezone must have a standard time.
  ASSERT_TRUE(samples.standard.has_value());
  struct tm* tm_standard = localtime(&samples.standard.value());
  ASSERT_NE(tm_standard, nullptr);
  time_t standard_time_rt = mktime(tm_standard);
  EXPECT_EQ(standard_time_rt, samples.standard.value());

  if (samples.daylight.has_value()) {
    struct tm* tm_daylight = localtime(&samples.daylight.value());
    ASSERT_NE(tm_daylight, nullptr);
    time_t daylight_time_rt = mktime(tm_daylight);
    EXPECT_EQ(daylight_time_rt, samples.daylight.value());
  }
}

INSTANTIATE_TEST_SUITE_P(PosixTimezoneTests,
                         PosixFormat,
                         ::testing::ValuesIn(PosixFormat::kAllTests),
                         GetTestName);

}  // namespace nplb
