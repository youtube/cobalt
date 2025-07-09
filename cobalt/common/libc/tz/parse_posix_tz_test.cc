// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/common/libc/tz/parse_posix_tz.h"

#include <ostream>
#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace cobalt {
namespace common {
namespace libc {
namespace tz {

// --- Equality Operators for Comparison ---
bool operator==(const DateRule& lhs, const DateRule& rhs) {
  return lhs.format == rhs.format && lhs.month == rhs.month &&
         lhs.week == rhs.week && lhs.day == rhs.day;
}

bool operator==(const TransitionRule& lhs, const TransitionRule& rhs) {
  return lhs.date_rule == rhs.date_rule && lhs.time == rhs.time;
}

bool operator==(const TimezoneData& lhs, const TimezoneData& rhs) {
  return lhs.std_name == rhs.std_name && lhs.std_offset == rhs.std_offset &&
         lhs.dst_name == rhs.dst_name && lhs.dst_offset == rhs.dst_offset &&
         lhs.start_rule == rhs.start_rule && lhs.end_rule == rhs.end_rule;
}

namespace {

// Structure to hold all parameters for a single test case.
struct TzParseTestParam {
  std::string test_name;
  std::string tz_string;
  std::optional<TimezoneData> expected_result;

  // Allows gtest to print readable output.
  friend void PrintTo(const TzParseTestParam& param, std::ostream* os);
};

std::string GetTestName(
    const ::testing::TestParamInfo<TzParseTestParam>& info) {
  return info.param.test_name;
}

// Number of seconds in an hour.
constexpr int kSecondsInHour = 3600;
// Number of seconds in a minute.
constexpr int kSecondsInMinute = 60;

// `PrintTo` function for `TzParseTestParam` so that gtest will print readable
// output when a test fails. This version prints on a single line.
void PrintTo(const TzParseTestParam& param, std::ostream* os) {
  *os << "{ test_name: \"" << param.test_name << "\", "
      << "tz_string: \"" << param.tz_string << "\", "
      << "expected_result: ";
  if (param.expected_result.has_value()) {
    const auto& data = *param.expected_result;
    *os << "{ std_name: \"" << data.std_name << "\", "
        << "std_offset: " << data.std_offset;
    if (data.dst_name && !data.dst_name->empty()) {
      auto print_rule = [&](const TransitionRule& rule) {
        *os << "{ date_rule: { format: ";
        switch (rule.date_rule.format) {
          case DateRule::Format::MonthWeekDay:
            *os << "MonthWeekDay";
            break;
          case DateRule::Format::Julian:
            *os << "Julian";
            break;
          case DateRule::Format::ZeroBasedJulian:
            *os << "ZeroBasedJulian";
            break;
        }
        *os << ", month: " << rule.date_rule.month
            << ", week: " << rule.date_rule.week
            << ", day: " << rule.date_rule.day << " }, time: " << rule.time
            << " }";
      };

      *os << ", dst_name: \"" << *data.dst_name << "\", "
          << "dst_offset: " << data.dst_offset.value_or(0) << ", "
          << "start_rule: ";
      if (data.start_rule) {
        print_rule(*data.start_rule);
      } else {
        *os << "null";
      }
      *os << ", end_rule: ";
      if (data.end_rule) {
        print_rule(*data.end_rule);
      } else {
        *os << "null";
      }
    }
    *os << " }";
  } else {
    *os << "std::nullopt";
  }
  *os << " }";
}
}  // namespace

class Tests : public ::testing::TestWithParam<TzParseTestParam> {
 public:
  // Default DST rules. POSIX specifies the default time as 02:00:00,
  // and the start and end dates as implementation-defined. The parser
  // being tested here uses second Sunday in March as the start and
  // the first Sunday in November as the end.
  static constexpr TransitionRule kDefaultStartRule = {
      {DateRule::Format::MonthWeekDay, 3, 2, 0},
      2 * kSecondsInHour};
  static constexpr TransitionRule kDefaultEndRule = {
      {DateRule::Format::MonthWeekDay, 11, 1, 0},
      2 * kSecondsInHour};

  static const inline std::vector<TzParseTestParam> kAllTests = {
      // --- Success Cases ---
      {"StdAndOffset", "PST8", TimezoneData{"PST", 8 * kSecondsInHour}},
      {"NegativeOffset", "CET-1", TimezoneData{"CET", -1 * kSecondsInHour}},
      {"PositiveSignOffset", "PST+8", TimezoneData{"PST", 8 * kSecondsInHour}},
      {"HourMinuteOffset", "IST-5:30",  // codespell:ignore ist
       TimezoneData{"IST",              // codespell:ignore ist
                    -(5 * kSecondsInHour + 30 * kSecondsInMinute)}},
      {"HourMinuteSecondOffset", "XYZ-1:23:45",
       TimezoneData{"XYZ", -(1 * kSecondsInHour + 23 * kSecondsInMinute + 45)}},

      // --- Cases that rely on the parser's DEFAULT rules ---
      {"DefaultRulesForPositiveOffset", "PST8PDT",
       TimezoneData{"PST", 8 * kSecondsInHour, "PDT", 7 * kSecondsInHour,
                    kDefaultStartRule, kDefaultEndRule}},
      {"DefaultRulesForNegativeOffset", "EET-2EEST",
       TimezoneData{"EET", -2 * kSecondsInHour, "EEST", -3 * kSecondsInHour,
                    kDefaultStartRule, kDefaultEndRule}},
      {"DefaultRulesWithExplicitDstOffset", "EST5EDT4",
       TimezoneData{"EST", 5 * kSecondsInHour, "EDT", 4 * kSecondsInHour,
                    kDefaultStartRule, kDefaultEndRule}},
      {"DefaultRulesForQuotedNames", "<MYT>-8<MYST>-9",
       TimezoneData{"MYT", -8 * kSecondsInHour, "MYST", -9 * kSecondsInHour,
                    kDefaultStartRule, kDefaultEndRule}},
      {"DefaultRulesWithLeadingColon", ":PST8PDT",
       TimezoneData{"PST", 8 * kSecondsInHour, "PDT", 7 * kSecondsInHour,
                    kDefaultStartRule, kDefaultEndRule}},

      // --- Cases that test EXPLICITLY PROVIDED rules ---
      {"WithVariedMonthWeekDayRules", "CST6CDT,M2.1.3,M12.4.4",
       TimezoneData{"CST", 6 * kSecondsInHour, "CDT", 5 * kSecondsInHour,
                    TransitionRule{{DateRule::Format::MonthWeekDay, 2, 1, 3},
                                   2 * kSecondsInHour},
                    TransitionRule{{DateRule::Format::MonthWeekDay, 12, 4, 4},
                                   2 * kSecondsInHour}}},
      {"WithVariedRulesAndTimes", "CST6CDT,M1.1.6/01:30,M8.3.5/23:59:59",
       TimezoneData{
           "CST", 6 * kSecondsInHour, "CDT", 5 * kSecondsInHour,
           TransitionRule{{DateRule::Format::MonthWeekDay, 1, 1, 6},
                          1 * kSecondsInHour + 30 * kSecondsInMinute},
           TransitionRule{{DateRule::Format::MonthWeekDay, 8, 3, 5},
                          23 * kSecondsInHour + 59 * kSecondsInMinute + 59}}},
      {"WithJulianDayRules", "EET-2EEST,J60,J305",
       TimezoneData{"EET", -2 * kSecondsInHour, "EEST", -3 * kSecondsInHour,
                    TransitionRule{{DateRule::Format::Julian, 0, 0, 60},
                                   2 * kSecondsInHour},
                    TransitionRule{{DateRule::Format::Julian, 0, 0, 305},
                                   2 * kSecondsInHour}}},
      {"WithJulianDayRulesAndTimes", "EET-2EEST,J65/03:00,J300/04:00",
       TimezoneData{"EET", -2 * kSecondsInHour, "EEST", -3 * kSecondsInHour,
                    TransitionRule{{DateRule::Format::Julian, 0, 0, 65},
                                   3 * kSecondsInHour},
                    TransitionRule{{DateRule::Format::Julian, 0, 0, 300},
                                   4 * kSecondsInHour}}},
      {"WithZeroBasedDayRules", "EET-2EEST,59,304",
       TimezoneData{
           "EET", -2 * kSecondsInHour, "EEST", -3 * kSecondsInHour,
           TransitionRule{{DateRule::Format::ZeroBasedJulian, 0, 0, 59},
                          2 * kSecondsInHour},
           TransitionRule{{DateRule::Format::ZeroBasedJulian, 0, 0, 304},
                          2 * kSecondsInHour}}},
      {"WithZeroBasedDayRulesAndTimes", "EET-2EEST,64/01:30:15,309/23:00",
       TimezoneData{
           "EET", -2 * kSecondsInHour, "EEST", -3 * kSecondsInHour,
           TransitionRule{{DateRule::Format::ZeroBasedJulian, 0, 0, 64},
                          1 * kSecondsInHour + 30 * kSecondsInMinute + 15},
           TransitionRule{{DateRule::Format::ZeroBasedJulian, 0, 0, 309},
                          23 * kSecondsInHour}}},
      {"QuotedStdNameOnly", "<UTC+0>0", TimezoneData{"UTC+0", 0}},

      // --- Failure Cases ---
      {"EmptyString", "", std::nullopt},
      {"ColonOnly", ":", std::nullopt},
      {"ShortStdName", "PS8", std::nullopt},
      {"NonAlphaStdName", "PST!8", std::nullopt},
      {"MissingStdOffset", "PST", std::nullopt},
      {"StdNameWithSlash", "America/New_York5", std::nullopt},

      // --- Partial Success (Graceful Stop) Cases ---
      {"ShortDstName", "PST8PD", TimezoneData{"PST", 8 * kSecondsInHour}},
      {"NonAlphaDstName", "PST8P&T", TimezoneData{"PST", 8 * kSecondsInHour}},
      {"DstNameWithSlash", "EST5America/New_York",
       TimezoneData{"EST", 5 * kSecondsInHour}},
      {"MalformedRule", "CST6CDT,M3.2.0",
       TimezoneData{"CST", 6 * kSecondsInHour, "CDT", 5 * kSecondsInHour}},
      {"InvalidRuleCharacter", "CST6CDT,M3.X.0,M11.1.0",
       TimezoneData{"CST", 6 * kSecondsInHour, "CDT", 5 * kSecondsInHour}},
      {"InvalidTransitionTime", "CST6CDT,M3.2.0/invalid,M11.1.0",
       TimezoneData{"CST", 6 * kSecondsInHour, "CDT", 5 * kSecondsInHour}},

  };
};

TEST_P(Tests, Parses) {
  const auto& param = GetParam();
  auto actual_result = ParsePosixTz(param.tz_string);

  if (param.expected_result.has_value()) {
    // This case expects a successful parse.
    ASSERT_TRUE(actual_result.has_value())
        << "Expected parsing to succeed for: " << param.tz_string;
    EXPECT_EQ(*actual_result, *param.expected_result);
  } else {
    // This case expects a parse failure.
    EXPECT_FALSE(actual_result.has_value())
        << "Expected parsing to fail for: " << param.tz_string;
  }
}

// Instantiate a set of tests named
// "ParsePosixTzTests/Tests.Parses/Testname"
INSTANTIATE_TEST_SUITE_P(ParsePosixTzTests,
                         Tests,
                         ::testing::ValuesIn(Tests::kAllTests),
                         GetTestName);

}  // namespace tz
}  // namespace libc
}  // namespace common
}  // namespace cobalt
