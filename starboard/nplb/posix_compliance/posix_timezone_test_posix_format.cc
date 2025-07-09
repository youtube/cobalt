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

#include "starboard/nplb/posix_compliance/posix_timezone_test_helper.h"

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

namespace starboard {
namespace nplb {
namespace {

struct PosixTestData {
  std::string test_name;
  std::string tz_string;
  long expected_timezone;
  std::string expected_tzname_std;
  std::string expected_tzname_dst;  // Empty if no DST rule.

  friend void PrintTo(const PosixTestData& data, ::std::ostream* os) {
    *os << "{ test_name: \"" << data.test_name << "\", tz_string: \"" << data.tz_string << "\", expected_timezone: " << data.expected_timezone << ", expected_tzname_std: \"" << data.expected_tzname_std << "\"";
    if (!data.expected_tzname_dst.empty()) {
      *os << ", expected_tzname_dst: \"" << data.expected_tzname_dst << "\"";
    } else {
      *os << ", expected_tzname_dst: (not checked)";
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
  static const inline std::vector<PosixTestData> kAllTests = {
      PosixTestData{"EmptyTZStringAsUTC", "", 0, "UTC"},
      PosixTestData{"LeadingColon", ":UTC0", 0, "UTC"},
      PosixTestData{"ISTMinus530", "IST-5:30",
                    -(5 * kSecondsInHour + 30 * kSecondsInMinute),
                    "IST"},  // codespell:ignore ist
      PosixTestData{"FullHmsOffset", "FOO+08:30:15",
                    8 * kSecondsInHour + 30 * kSecondsInMinute + 15, "FOO"},
      PosixTestData{"CST6CDT", "CST6CDT", 6 * kSecondsInHour, "CST", "CDT"},
      PosixTestData{"DefaultDstOffset", "PST8PDT", 8 * kSecondsInHour, "PST",
                    "PDT"},
      PosixTestData{"ExplicitDSTOffset", "AAA+5BBB+4", 5 * kSecondsInHour,
                    "AAA", "BBB"},
      PosixTestData{"QuotedNames", "<UTC+3>-3<-UTC+2>-2", -3 * kSecondsInHour,
                    "UTC+3", "-UTC+2"},
      PosixTestData{"JulianDayDSTRule", "CST6CDT,J60,J300", 6 * kSecondsInHour,
                    "CST", "CDT"},
      PosixTestData{"ZeroBasedJulianDayDSTRule", "MST7MDT,59,299",
                    7 * kSecondsInHour, "MST", "MDT"},
      PosixTestData{"DSTRulesEST5EDT", "EST5EDT,M3.2.0/2,M11.1.0/2",
                    5 * kSecondsInHour, "EST", "EDT"},
      PosixTestData{"TransitionRuleWithExplicitTime",
                    "CST6CDT,M3.2.0/02:30:00,M11.1.0/03:00:00",
                    6 * kSecondsInHour, "CST", "CDT"},
  };
};

TEST_P(PosixFormat, Handles) {
  const auto& param = GetParam();
  ScopedTZ tz_manager(param.tz_string.c_str());

  EXPECT_EQ(timezone, param.expected_timezone);

  bool expected_daylight = !param.expected_tzname_dst.empty();
  EXPECT_EQ(daylight, expected_daylight);

  ASSERT_NE(tzname[0], nullptr);
  EXPECT_STREQ(tzname[0], param.expected_tzname_std.c_str());

  if (!param.expected_tzname_dst.empty()) {
    ASSERT_NE(tzname[1], nullptr);
    EXPECT_STREQ(tzname[1], param.expected_tzname_dst.c_str());
  }
}

INSTANTIATE_TEST_SUITE_P(PosixTimezoneTests,
                         PosixFormat,
                         ::testing::ValuesIn(PosixFormat::kAllTests),
                         GetTestName);

}  // namespace nplb
}  // namespace starboard
