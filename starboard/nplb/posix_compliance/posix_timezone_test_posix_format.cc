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

#include "starboard/nplb/posix_compliance/posix_timezone_test_helper.h"
#include "starboard/nplb/posix_compliance/scoped_tz_set.h"

namespace starboard {
namespace nplb {
namespace {

struct PosixTestData {
  const char* test_name;
  const char* tz;
  long offset;
  const char* std;
  std::optional<const char*> dst = std::nullopt;

  friend void PrintTo(const PosixTestData& data, ::std::ostream* os) {
    *os << "{ test_name: \"" << data.test_name << "\", tz: \"" << data.tz
        << "\", offset: " << data.offset << ", std: \"" << data.std << "\"";
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
  static const std::array<PosixTestData, 12> kAllTests;
};

const std::array<PosixTestData, 12> PosixFormat::kAllTests = {{
    {.test_name = "EmptyTZStringAsUTC", .tz = "", .offset = 0, .std = "UTC"},
    {.test_name = "LeadingColon", .tz = ":UTC0", .offset = 0, .std = "UTC"},
    {.test_name = "ISTMinus530",
     .tz = "IST-5:30",
     .offset = -(5 * kSecondsInHour + 30 * kSecondsInMinute),
     .std = "IST"},
    {.test_name = "FullHmsOffset",
     .tz = "FOO+08:30:15",
     .offset = 8 * kSecondsInHour + 30 * kSecondsInMinute + 15,
     .std = "FOO"},
    {.test_name = "CST6CDT",
     .tz = "CST6CDT",
     .offset = 6 * kSecondsInHour,
     .std = "CST",
     .dst = "CDT"},
    {.test_name = "DefaultDstOffset",
     .tz = "PST8PDT",
     .offset = 8 * kSecondsInHour,
     .std = "PST",
     .dst = "PDT"},
    {.test_name = "ExplicitDSTOffset",
     .tz = "AAA+5BBB+4",
     .offset = 5 * kSecondsInHour,
     .std = "AAA",
     .dst = "BBB"},
    {.test_name = "QuotedNames",
     .tz = "<UTC+3>-3<-UTC+2>-2",
     .offset = -3 * kSecondsInHour,
     .std = "UTC+3",
     .dst = "-UTC+2"},
    {.test_name = "JulianDayDSTRule",
     .tz = "CST6CDT,J60,J300",
     .offset = 6 * kSecondsInHour,
     .std = "CST",
     .dst = "CDT"},
    {.test_name = "ZeroBasedJulianDayDSTRule",
     .tz = "MST7MDT,59,299",
     .offset = 7 * kSecondsInHour,
     .std = "MST",
     .dst = "MDT"},
    {.test_name = "DSTRulesEST5EDT",
     .tz = "EST5EDT,M3.2.0/2,M11.1.0/2",
     .offset = 5 * kSecondsInHour,
     .std = "EST",
     .dst = "EDT"},
    {.test_name = "TransitionRuleWithExplicitTime",
     .tz = "CST6CDT,M3.2.0/02:30:00,M11.1.0/03:00:00",
     .offset = 6 * kSecondsInHour,
     .std = "CST",
     .dst = "CDT"},
}};

TEST_P(PosixFormat, Handles) {
  const auto& param = GetParam();
  ScopedTzSet tz_manager(param.tz);

  EXPECT_EQ(timezone, param.offset);

  bool expected_daylight = param.dst.has_value();
  EXPECT_EQ(daylight, expected_daylight);

  ASSERT_NE(tzname[0], nullptr);
  EXPECT_STREQ(tzname[0], param.std);

  if (param.dst.has_value()) {
    ASSERT_NE(tzname[1], nullptr);
    EXPECT_STREQ(tzname[1], param.dst.value());
  }
}

INSTANTIATE_TEST_SUITE_P(PosixTimezoneTests,
                         PosixFormat,
                         ::testing::ValuesIn(PosixFormat::kAllTests),
                         GetTestName);

}  // namespace nplb
}  // namespace starboard
