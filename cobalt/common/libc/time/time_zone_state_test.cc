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

#include "cobalt/common/libc/time/time_zone_state.h"

#include <gtest/gtest.h>
#include <stdlib.h>
#include <time.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "unicode/calendar.h"
#include "unicode/gregocal.h"
#include "unicode/simpletz.h"
#include "unicode/timezone.h"
#include "unicode/unistr.h"
#include "unicode/utypes.h"

namespace cobalt {
namespace common {
namespace libc {
namespace time {

namespace {

// Number of seconds in an hour.
constexpr int kSecondsInHour = 3600;

// The year to use for testing timezone transitions. The year 2023 corresponds
// to the tzdata database version used in the included ICU library.
constexpr int kTestyear = 2023;

std::string ToString(const icu::UnicodeString& ustr) {
  std::string s;
  ustr.toUTF8String(s);
  return s;
}

struct YearMonthDay {
  int year;
  int month;
  int day;
};

struct TimezoneTestData {
  std::string test_name;
  std::string tz;
  std::string id;
  int32_t offset;

  YearMonthDay std_start;

  std::optional<YearMonthDay> dst_start = std::nullopt;

  friend void PrintTo(const TimezoneTestData& data, ::std::ostream* os) {
    *os << "{ test_name: \"" << data.test_name << "\", tz: \"" << data.tz
        << "\", id: \"" << data.id << "\", offset: " << data.offset;
    auto print_date_check = [&](const char* name,
                                const std::optional<YearMonthDay>& check) {
      if (check) {
        *os << ", " << name << ": { year: " << check->year
            << ", month: " << check->month << ", day: " << check->day << " }";
      }
    };
    print_date_check("std_start", std::optional(data.std_start));
    print_date_check("dst_start", data.dst_start);
    *os << " }";
  }
};

std::string GetTestName(
    const ::testing::TestParamInfo<TimezoneTestData>& info) {
  return info.param.test_name;
}
}  // namespace

class TimeZoneStateTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* current_tz_env = getenv("TZ");
    if (current_tz_env != nullptr) {
      original_tz_value_ = current_tz_env;
    }
    unsetenv("TZ");
  }

  void TearDown() override {
    if (original_tz_value_) {
      setenv("TZ", original_tz_value_->c_str(), 1);
    } else {
      unsetenv("TZ");
    }
  }

  void SetTimezone(const char* tz) { setenv("TZ", tz, 1); }

  std::optional<std::string> original_tz_value_;
};

class TimezoneStateParamTest
    : public TimeZoneStateTest,
      public ::testing::WithParamInterface<TimezoneTestData> {
 protected:
  void VerifyDateOffset(const icu::TimeZone& tz,
                        const YearMonthDay& date,
                        int32_t expected_offset,
                        const char* transition_name) {
    UErrorCode status = U_ZERO_ERROR;
    icu::GregorianCalendar calendar(tz, status);
    ASSERT_TRUE(U_SUCCESS(status));
    calendar.set(date.year, date.month, date.day, 12, 0, 0);
    int32_t raw_offset, dst_offset;
    tz.getOffset(calendar.getTime(status), false, raw_offset, dst_offset,
                 status);
    ASSERT_TRUE(U_SUCCESS(status));
    EXPECT_EQ(expected_offset * 1000, raw_offset + dst_offset)
        << "Failed " << transition_name << " time offset check for date "
        << date.year << "/" << date.month + 1 << "/" << date.day;
  }

  YearMonthDay GetDayBefore(const icu::TimeZone& tz,
                            const YearMonthDay& transition_date) {
    UErrorCode status = U_ZERO_ERROR;
    icu::GregorianCalendar calendar(tz, status);
    EXPECT_TRUE(U_SUCCESS(status));
    if (U_FAILURE(status)) {
      return {};
    }

    calendar.set(transition_date.year, transition_date.month,
                 transition_date.day);
    calendar.add(UCAL_DATE, -1, status);
    EXPECT_TRUE(U_SUCCESS(status));
    if (U_FAILURE(status)) {
      return {};
    }

    int year = calendar.get(UCAL_YEAR, status);
    int month = calendar.get(UCAL_MONTH, status);
    int day = calendar.get(UCAL_DATE, status);
    EXPECT_TRUE(U_SUCCESS(status));
    if (U_FAILURE(status)) {
      return {};
    }

    return {year, month, day};
  }

 public:
  static const inline std::vector<TimezoneTestData> kAllTestCases = {
      // --- IANA Test Cases ---
      {.test_name = "IanaUTC",
       .tz = "UTC",
       .id = "UTC",
       .offset = 0,
       .std_start = {.year = kTestyear, .month = UCAL_JANUARY, .day = 1}},
      {.test_name = "IanaInvalidName",
       .tz = "Invalid/Timezone",
       .id = "UTC",
       .offset = 0,
       .std_start = {.year = kTestyear, .month = UCAL_JANUARY, .day = 1}},
      {.test_name = "IanaAmericaNewYork",
       .tz = "America/New_York",
       .id = "America/New_York",
       .offset = -5 * kSecondsInHour,
       .std_start = {.year = kTestyear, .month = UCAL_NOVEMBER, .day = 5},
       .dst_start = std::make_optional<YearMonthDay>(
           {.year = kTestyear, .month = UCAL_MARCH, .day = 12})},
      {.test_name = "IanaAmericaChicago",
       .tz = "America/Chicago",
       .id = "America/Chicago",
       .offset = -6 * kSecondsInHour,
       .std_start = {.year = kTestyear, .month = UCAL_NOVEMBER, .day = 5},
       .dst_start = std::make_optional<YearMonthDay>(
           {.year = kTestyear, .month = UCAL_MARCH, .day = 12})},
      {.test_name = "IanaAmericaDenver",
       .tz = "America/Denver",
       .id = "America/Denver",
       .offset = -7 * kSecondsInHour,
       .std_start = {.year = kTestyear, .month = UCAL_NOVEMBER, .day = 5},
       .dst_start = std::make_optional<YearMonthDay>(
           {.year = kTestyear, .month = UCAL_MARCH, .day = 12})},
      {.test_name = "IanaAmericaLosAngeles",
       .tz = "America/Los_Angeles",
       .id = "America/Los_Angeles",
       .offset = -8 * kSecondsInHour,
       .std_start = {.year = kTestyear, .month = UCAL_NOVEMBER, .day = 5},
       .dst_start = std::make_optional<YearMonthDay>(
           {.year = kTestyear, .month = UCAL_MARCH, .day = 12})},
      {.test_name = "IanaAmericaAnchorage",
       .tz = "America/Anchorage",
       .id = "America/Anchorage",
       .offset = -9 * kSecondsInHour,
       .std_start = {.year = kTestyear, .month = UCAL_NOVEMBER, .day = 5},
       .dst_start = std::make_optional<YearMonthDay>(
           {.year = kTestyear, .month = UCAL_MARCH, .day = 12})},
      {.test_name = "IanaPacificHonoluluNoDst",
       .tz = "Pacific/Honolulu",
       .id = "Pacific/Honolulu",
       .offset = -10 * kSecondsInHour,
       .std_start = {.year = kTestyear, .month = UCAL_JANUARY, .day = 1}},
      {.test_name = "IanaEuropeLondon",
       .tz = "Europe/London",
       .id = "Europe/London",
       .offset = 0,
       .std_start = {.year = kTestyear, .month = UCAL_OCTOBER, .day = 29},
       .dst_start = std::make_optional<YearMonthDay>(
           {.year = kTestyear, .month = UCAL_MARCH, .day = 26})},
      {.test_name = "IanaEuropeParis",
       .tz = "Europe/Paris",
       .id = "Europe/Paris",
       .offset = 1 * kSecondsInHour,
       .std_start = {.year = kTestyear, .month = UCAL_OCTOBER, .day = 29},
       .dst_start = std::make_optional<YearMonthDay>(
           {.year = kTestyear, .month = UCAL_MARCH, .day = 26})},
      {.test_name = "IanaEuropeAthens",
       .tz = "Europe/Athens",
       .id = "Europe/Athens",
       .offset = 2 * kSecondsInHour,
       .std_start = {.year = kTestyear, .month = UCAL_OCTOBER, .day = 29},
       .dst_start = std::make_optional<YearMonthDay>(
           {.year = kTestyear, .month = UCAL_MARCH, .day = 26})},
      {.test_name = "IanaEuropeMoscowNoDst",
       .tz = "Europe/Moscow",
       .id = "Europe/Moscow",
       .offset = 3 * kSecondsInHour,
       .std_start = {.year = kTestyear, .month = UCAL_JANUARY, .day = 1}},
      {.test_name = "IanaAsiaTokyoNoDst",
       .tz = "Asia/Tokyo",
       .id = "Asia/Tokyo",
       .offset = 9 * kSecondsInHour,
       .std_start = {.year = kTestyear, .month = UCAL_JANUARY, .day = 1}},
      {.test_name = "IanaAsiaKolkataNoDst",
       .tz = "Asia/Kolkata",
       .id = "Asia/Kolkata",
       .offset = (5 * kSecondsInHour + 30 * 60),
       .std_start = {.year = kTestyear, .month = UCAL_JANUARY, .day = 1}},
      {.test_name = "IanaAfricaCairo",
       .tz = "Africa/Cairo",
       .id = "Africa/Cairo",
       .offset = 2 * kSecondsInHour,
       .std_start = {.year = kTestyear, .month = UCAL_OCTOBER, .day = 27},
       .dst_start = std::make_optional<YearMonthDay>(
           {.year = kTestyear, .month = UCAL_APRIL, .day = 28})},
      {.test_name = "AmericaAsuncion",
       .tz = "America/Asuncion",
       .id = "America/Asuncion",
       .offset = -3 * kSecondsInHour,
       .std_start = {.year = kTestyear, .month = UCAL_JANUARY, .day = 1}},
      {.test_name = "AmericaMontreal",
       .tz = "America/Montreal",
       .id = "America/Montreal",
       .offset = -5 * kSecondsInHour,
       .std_start = {.year = kTestyear, .month = UCAL_NOVEMBER, .day = 5},
       .dst_start = std::make_optional<YearMonthDay>(
           {.year = kTestyear, .month = UCAL_MARCH, .day = 12})},
      {.test_name = "AsiaFamagusta",
       .tz = "Asia/Famagusta",
       .id = "Asia/Famagusta",
       .offset = 2 * kSecondsInHour,
       .std_start = {.year = kTestyear, .month = UCAL_OCTOBER, .day = 29},
       .dst_start = std::make_optional<YearMonthDay>(
           {.year = kTestyear, .month = UCAL_MARCH, .day = 26})},
      {.test_name = "EuropeDublin",
       .tz = "Europe/Dublin",
       .id = "Europe/Dublin",
       .offset = 0,
       .std_start = {.year = kTestyear, .month = UCAL_OCTOBER, .day = 29},
       .dst_start = std::make_optional<YearMonthDay>(
           {.year = kTestyear, .month = UCAL_MARCH, .day = 26})},
      {.test_name = "EuropeLondon",
       .tz = "Europe/London",
       .id = "Europe/London",
       .offset = 0,
       .std_start = {.year = kTestyear, .month = UCAL_OCTOBER, .day = 29},
       .dst_start = std::make_optional<YearMonthDay>(
           {.year = kTestyear, .month = UCAL_MARCH, .day = 26})},
      {.test_name = "CET",
       .tz = "CET",
       .id = "CET",
       .offset = 1 * kSecondsInHour,
       .std_start = {.year = kTestyear, .month = UCAL_OCTOBER, .day = 29},
       .dst_start = std::make_optional<YearMonthDay>(
           {.year = kTestyear, .month = UCAL_MARCH, .day = 26})},
      {.test_name = "EET",
       .tz = "EET",
       .id = "EET",
       .offset = 2 * kSecondsInHour,
       .std_start = {.year = kTestyear, .month = UCAL_OCTOBER, .day = 29},
       .dst_start = std::make_optional<YearMonthDay>(
           {.year = kTestyear, .month = UCAL_MARCH, .day = 26})},
      {.test_name = "WET",
       .tz = "WET",
       .id = "WET",
       .offset = 0,
       .std_start = {.year = kTestyear, .month = UCAL_OCTOBER, .day = 29},
       .dst_start = std::make_optional<YearMonthDay>(
           {.year = kTestyear, .month = UCAL_MARCH, .day = 26})},

      // --- Non-IANA POSIX Test Cases ---
      {.test_name = "PosixFixedOffsetNoDst",
       .tz = "PST8",
       .id = "PST",
       .offset = -8 * kSecondsInHour,
       .std_start = {.year = kTestyear, .month = UCAL_JANUARY, .day = 1}},
      {.test_name = "PosixStandardUsDstRules",
       .tz = "EST5EDT,M3.2.0,M11.1.0",
       .id = "EST",
       .offset = -5 * kSecondsInHour,
       .std_start = {.year = kTestyear, .month = UCAL_NOVEMBER, .day = 5},
       .dst_start = std::make_optional<YearMonthDay>(
           {.year = kTestyear, .month = UCAL_MARCH, .day = 12})},
      {.test_name = "PosixEuropeanDstRules",
       .tz = "CET-1CEST,M3.5.0/2,M10.5.0/3",
       .id = "CET",
       .offset = 1 * kSecondsInHour,
       .std_start = {.year = kTestyear, .month = UCAL_OCTOBER, .day = 29},
       .dst_start = std::make_optional<YearMonthDay>(
           {.year = kTestyear, .month = UCAL_MARCH, .day = 26})},
      {.test_name = "PosixWeekdayRuleSaturday",
       .tz = "CAT-2CAST,M4.1.6,M10.5.6",
       .id = "CAT",
       .offset = 2 * kSecondsInHour,
       .std_start = {.year = kTestyear, .month = UCAL_OCTOBER, .day = 28},
       .dst_start = std::make_optional<YearMonthDay>(
           {.year = kTestyear, .month = UCAL_APRIL, .day = 1})},
      {.test_name = "PosixWeekdayRuleFriday",
       .tz = "FKT-4FKST,M3.2.5,M11.1.5",
       .id = "FKT",
       .offset = 4 * kSecondsInHour,
       .std_start = {.year = kTestyear, .month = UCAL_NOVEMBER, .day = 3},
       .dst_start = std::make_optional<YearMonthDay>(
           {.year = kTestyear, .month = UCAL_MARCH, .day = 10})},
      {.test_name = "PosixJulianRuleOneBased",
       .tz = "JST-9JDT,J70,J300",
       .id = "JST",
       .offset = 9 * kSecondsInHour,
       .std_start = {.year = kTestyear, .month = UCAL_OCTOBER, .day = 27},
       .dst_start = std::make_optional<YearMonthDay>(
           {.year = kTestyear, .month = UCAL_MARCH, .day = 11})},
      {.test_name = "PosixJulianRuleZeroBased",
       .tz = "ZST+5ZDT,69,299",
       .id = "ZST",
       .offset = -5 * kSecondsInHour,
       .std_start = {.year = kTestyear, .month = UCAL_OCTOBER, .day = 27},
       .dst_start = std::make_optional<YearMonthDay>(
           {.year = kTestyear, .month = UCAL_MARCH, .day = 11})},
  };
};

TEST_P(TimezoneStateParamTest, TimeZoneIsCorrect) {
  const auto& param = GetParam();
  SetTimezone(param.tz.c_str());

  TimeZoneState state;
  const icu::TimeZone& tz = state.GetTimeZone();

  EXPECT_EQ(param.offset * 1000, tz.getRawOffset());
  EXPECT_EQ(param.dst_start.has_value(), tz.useDaylightTime());

  icu::UnicodeString id;
  tz.getID(id);
  EXPECT_EQ(param.id, ToString(id));

  if (param.dst_start) {
    EXPECT_EQ(kSecondsInHour * 1000, tz.getDSTSavings());

    VerifyDateOffset(tz, *param.dst_start, param.offset + kSecondsInHour,
                     "on daylight time start");

    VerifyDateOffset(tz, GetDayBefore(tz, *param.dst_start), param.offset,
                     "before daylight time start");

    VerifyDateOffset(tz, param.std_start, param.offset,
                     "on standard time start");

    VerifyDateOffset(tz, GetDayBefore(tz, param.std_start),
                     param.offset + kSecondsInHour,
                     "before standard time start");

  } else {
    VerifyDateOffset(tz, param.std_start, param.offset, "standard time");
  }
}

TEST_P(TimezoneStateParamTest, PosixGlobalsAreCorrect) {
  const auto& param = GetParam();
  SetTimezone(param.tz.c_str());

  TimeZoneState state;
  long timezone_sec;
  int daylight;
  char* tzname[2];

  state.GetPosixTimezoneGlobals(timezone_sec, daylight, tzname);

  EXPECT_EQ(timezone_sec, -param.offset);
  EXPECT_EQ(daylight, param.dst_start.has_value());
}

INSTANTIATE_TEST_SUITE_P(
    TimeZoneState,
    TimezoneStateParamTest,
    ::testing::ValuesIn(TimezoneStateParamTest::kAllTestCases),
    GetTestName);

}  // namespace time
}  // namespace libc
}  // namespace common
}  // namespace cobalt
