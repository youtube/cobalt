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
  return lhs.date == rhs.date && lhs.time == rhs.time;
}

bool operator==(const TimezoneData& lhs, const TimezoneData& rhs) {
  return lhs.std == rhs.std && lhs.std_offset == rhs.std_offset &&
         lhs.dst == rhs.dst && lhs.dst_offset == rhs.dst_offset &&
         lhs.start == rhs.start && lhs.end == rhs.end;
}

namespace {

// Structure to hold all parameters for a single test case.
struct TzParseTestParam {
  std::string test_name;
  std::string tz;
  std::optional<TimezoneData> result;

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
      << "tz: \"" << param.tz << "\", "
      << "result: ";
  if (param.result.has_value()) {
    const auto& data = *param.result;
    *os << "{ std: \"" << data.std << "\", "
        << "std_offset: " << data.std_offset;
    if (data.dst && !data.dst->empty()) {
      auto print_rule = [&](const TransitionRule& rule) {
        *os << "{ date: { format: ";
        switch (rule.date.format) {
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
        *os << ", month: " << rule.date.month << ", week: " << rule.date.week
            << ", day: " << rule.date.day << " }, time: " << rule.time << " }";
      };

      *os << ", dst: \"" << *data.dst << "\", "
          << "dst_offset: " << data.dst_offset.value_or(0) << ", "
          << "start: ";
      if (data.start) {
        print_rule(*data.start);
      } else {
        *os << "null";
      }
      *os << ", end: ";
      if (data.end) {
        print_rule(*data.end);
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
  static constexpr TransitionRule kDefaultStart = {
      .date = {.format = DateRule::Format::MonthWeekDay,
               .month = 3,
               .week = 2,
               .day = 0},
      .time = 2 * kSecondsInHour};
  static constexpr TransitionRule kDefaultEnd = {
      .date = {.format = DateRule::Format::MonthWeekDay,
               .month = 11,
               .week = 1,
               .day = 0},
      .time = 2 * kSecondsInHour};

  static const inline std::vector<TzParseTestParam> kAllTests = {
      // --- Success Cases ---
      {.test_name = "StdAndOffset",
       .tz = "PST8",
       .result = TimezoneData{.std = "PST", .std_offset = 8 * kSecondsInHour}},
      {.test_name = "NegativeOffset",
       .tz = "CET-1",
       .result = TimezoneData{.std = "CET", .std_offset = -1 * kSecondsInHour}},
      {.test_name = "PositiveSignOffset",
       .tz = "PST+8",
       .result = TimezoneData{.std = "PST", .std_offset = 8 * kSecondsInHour}},
      {.test_name = "HourMinuteOffset",
       .tz = "IST-5:30",
       .result = TimezoneData{.std = "IST",
                              .std_offset = -(5 * kSecondsInHour +
                                              30 * kSecondsInMinute)}},
      {.test_name = "HourMinuteSecondOffset",
       .tz = "XYZ-1:23:45",
       .result = TimezoneData{.std = "XYZ",
                              .std_offset = -(1 * kSecondsInHour +
                                              23 * kSecondsInMinute + 45)}},

      // --- Cases that rely on the parser's DEFAULT rules ---
      {.test_name = "DefaultRulesForPositiveOffset",
       .tz = "PST8PDT",
       .result = TimezoneData{.std = "PST",
                              .std_offset = 8 * kSecondsInHour,
                              .dst = "PDT",
                              .dst_offset = 7 * kSecondsInHour,
                              .start = kDefaultStart,
                              .end = kDefaultEnd}},
      {.test_name = "DefaultRulesForNegativeOffset",
       .tz = "EET-2EEST",
       .result = TimezoneData{.std = "EET",
                              .std_offset = -2 * kSecondsInHour,
                              .dst = "EEST",
                              .dst_offset = -3 * kSecondsInHour,
                              .start = kDefaultStart,
                              .end = kDefaultEnd}},
      {.test_name = "DefaultRulesWithExplicitDstOffset",
       .tz = "EST5EDT4",
       .result = TimezoneData{.std = "EST",
                              .std_offset = 5 * kSecondsInHour,
                              .dst = "EDT",
                              .dst_offset = 4 * kSecondsInHour,
                              .start = kDefaultStart,
                              .end = kDefaultEnd}},
      {.test_name = "DefaultRulesForQuotedNames",
       .tz = "<MYT>-8<MYST>-9",
       .result = TimezoneData{.std = "MYT",
                              .std_offset = -8 * kSecondsInHour,
                              .dst = "MYST",
                              .dst_offset = -9 * kSecondsInHour,
                              .start = kDefaultStart,
                              .end = kDefaultEnd}},
      {.test_name = "DefaultRulesWithLeadingColon",
       .tz = ":PST8PDT",
       .result = TimezoneData{.std = "PST",
                              .std_offset = 8 * kSecondsInHour,
                              .dst = "PDT",
                              .dst_offset = 7 * kSecondsInHour,
                              .start = kDefaultStart,
                              .end = kDefaultEnd}},

      // --- Cases that test EXPLICITLY PROVIDED rules ---
      {.test_name = "WithVariedMonthWeekDayRules",
       .tz = "CST6CDT,M2.1.3,M12.4.4",
       .result =
           TimezoneData{
               .std = "CST",
               .std_offset = 6 * kSecondsInHour,
               .dst = "CDT",
               .dst_offset = 5 * kSecondsInHour,
               .start =
                   TransitionRule{
                       .date = {.format = DateRule::Format::MonthWeekDay,
                                .month = 2,
                                .week = 1,
                                .day = 3},
                       .time = 2 * kSecondsInHour},
               .end =
                   TransitionRule{
                       .date = {.format = DateRule::Format::MonthWeekDay,
                                .month = 12,
                                .week = 4,
                                .day = 4},
                       .time = 2 * kSecondsInHour}}},
      {.test_name = "WithVariedRulesAndTimes",
       .tz = "CST6CDT,M1.1.6/01:30,M8.3.5/23:59:59",
       .result =
           TimezoneData{
               .std = "CST",
               .std_offset = 6 * kSecondsInHour,
               .dst = "CDT",
               .dst_offset = 5 * kSecondsInHour,
               .start = TransitionRule{.date = {.format = DateRule::Format::
                                                    MonthWeekDay,
                                                .month = 1,
                                                .week = 1,
                                                .day = 6},
                                       .time = 1 * kSecondsInHour +
                                               30 *
                                                   kSecondsInMinute},
               .end =
                   TransitionRule{
                       .date = {.format = DateRule::Format::MonthWeekDay,
                                .month =
                                    8,
                                .week = 3,
                                .day =
                                    5},
                       .time = 23 * kSecondsInHour + 59 * kSecondsInMinute +
                               59}}},
      {.test_name = "WithJulianDayRules",
       .tz = "EET-2EEST,J60,J305",
       .result =
           TimezoneData{
               .std = "EET",
               .std_offset = -2 * kSecondsInHour,
               .dst = "EEST",
               .dst_offset = -3 * kSecondsInHour,
               .start = TransitionRule{.date = {.format =
                                                    DateRule::Format::Julian,
                                                .month = 0,
                                                .week = 0,
                                                .day = 60},
                                       .time = 2 * kSecondsInHour},
               .end =
                   TransitionRule{.date = {.format = DateRule::Format::Julian,
                                           .month = 0,
                                           .week = 0,
                                           .day = 305},
                                  .time = 2 * kSecondsInHour}}},
      {.test_name = "WithJulianDayRulesAndTimes",
       .tz = "EET-2EEST,J65/03:00,J300/04:00",
       .result =
           TimezoneData{
               .std = "EET",
               .std_offset = -2 * kSecondsInHour,
               .dst = "EEST",
               .dst_offset = -3 * kSecondsInHour,
               .start = TransitionRule{.date = {.format =
                                                    DateRule::Format::Julian,
                                                .month = 0,
                                                .week = 0,
                                                .day = 65},
                                       .time = 3 * kSecondsInHour},
               .end =
                   TransitionRule{.date = {.format = DateRule::Format::Julian,
                                           .month = 0,
                                           .week = 0,
                                           .day = 300},
                                  .time = 4 * kSecondsInHour}}},
      {.test_name = "WithZeroBasedDayRules",
       .tz = "EET-2EEST,59,304",
       .result =
           TimezoneData{.std = "EET",
                        .std_offset = -2 * kSecondsInHour,
                        .dst = "EEST",
                        .dst_offset = -3 * kSecondsInHour,
                        .start =
                            TransitionRule{
                                .date = {.format =
                                             DateRule::Format::ZeroBasedJulian,
                                         .month = 0,
                                         .week = 0,
                                         .day = 59},
                                .time = 2 * kSecondsInHour},
                        .end =
                            TransitionRule{
                                .date = {.format =
                                             DateRule::Format::ZeroBasedJulian,
                                         .month = 0,
                                         .week = 0,
                                         .day = 304},
                                .time = 2 * kSecondsInHour}}},
      {.test_name = "WithZeroBasedDayRulesAndTimes",
       .tz = "EET-2EEST,64/01:30:15,309/23:00",
       .result =
           TimezoneData{
               .std = "EET",
               .std_offset = -2 * kSecondsInHour,
               .dst = "EEST",
               .dst_offset = -3 * kSecondsInHour,
               .start = TransitionRule{.date = {.format = DateRule::Format::
                                                    ZeroBasedJulian,
                                                .month = 0,
                                                .week = 0,
                                                .day = 64},
                                       .time = 1 * kSecondsInHour +
                                               30 * kSecondsInMinute + 15},
               .end = TransitionRule{.date =
                                         {
                                             .format =
                                                 DateRule::Format::ZeroBasedJulian,
                                             .month = 0,
                                             .week = 0,
                                             .day = 309},
                                     .time = 23 * kSecondsInHour}}},
      {.test_name = "QuotedStdNameOnly",
       .tz = "<UTC+0>0",
       .result = TimezoneData{.std = "UTC+0", .std_offset = 0}},

      // --- Failure Cases ---
      {.test_name = "EmptyString", .tz = "", .result = std::nullopt},
      {.test_name = "ColonOnly", .tz = ":", .result = std::nullopt},
      {.test_name = "ShortStdName", .tz = "PS8", .result = std::nullopt},
      {.test_name = "NonAlphaStdName", .tz = "PST!8", .result = std::nullopt},
      {.test_name = "MissingStdOffset", .tz = "PST", .result = std::nullopt},
      {.test_name = "StdNameWithSlash",
       .tz = "America/New_York5",
       .result = std::nullopt},

      // --- Partial Success (Graceful Stop) Cases ---
      {.test_name = "ShortDstName",
       .tz = "PST8PD",
       .result = TimezoneData{.std = "PST", .std_offset = 8 * kSecondsInHour}},
      {.test_name = "NonAlphaDstName",
       .tz = "PST8P&T",
       .result = TimezoneData{.std = "PST", .std_offset = 8 * kSecondsInHour}},
      {.test_name = "DstNameWithSlash",
       .tz = "EST5America/New_York",
       .result = TimezoneData{.std = "EST", .std_offset = 5 * kSecondsInHour}},
      {.test_name = "MalformedRule",
       .tz = "CST6CDT,M3.2.0",
       .result = TimezoneData{.std = "CST",
                              .std_offset = 6 * kSecondsInHour,
                              .dst = "CDT",
                              .dst_offset = 5 * kSecondsInHour}},
      {.test_name = "InvalidRuleCharacter",
       .tz = "CST6CDT,M3.X.0,M11.1.0",
       .result = TimezoneData{.std = "CST",
                              .std_offset = 6 * kSecondsInHour,
                              .dst = "CDT",
                              .dst_offset = 5 * kSecondsInHour}},
      {.test_name = "InvalidTransitionTime",
       .tz = "CST6CDT,M3.2.0/invalid,M11.1.0",
       .result = TimezoneData{.std = "CST",
                              .std_offset = 6 * kSecondsInHour,
                              .dst = "CDT",
                              .dst_offset = 5 * kSecondsInHour}},

  };
};

TEST_P(Tests, Parses) {
  const auto& param = GetParam();
  auto actual_result = ParsePosixTz(param.tz);

  if (param.result.has_value()) {
    // This case expects a successful parse.
    ASSERT_TRUE(actual_result.has_value())
        << "Expected parsing to succeed for: " << param.tz;
    EXPECT_EQ(*actual_result, *param.result);
  } else {
    // This case expects a parse failure.
    EXPECT_FALSE(actual_result.has_value())
        << "Expected parsing to fail for: " << param.tz;
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
