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

// The tzset() implementation in this folder initializes the ICU default
// timezone from the configured timezone name, so that it can be relied on for
// querying the local time. This file contains the corresponding tests.

// Note: The required POSIX functionality for tzset() which initializes
// the globals tzname, timezone, and daylight is not tested here. That
// functionality is tested in starboard/nplb/posix_compliance.

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

// A base test fixture to manage the TZ environment variable and ICU's default
// timezone state. This ensures tests are isolated from each other.
class TzsetTest : public ::testing::Test {
 protected:
  void SetUp() override {
    original_icu_tz_.reset(icu::TimeZone::createDefault()->clone());
    const char* current_tz_env = getenv("TZ");
    if (current_tz_env != nullptr) {
      original_tz_value_ = current_tz_env;  // Store the original TZ string
    }
    unsetenv("TZ");
  }

  void TearDown() override {
    if (original_icu_tz_) {
      icu::TimeZone::setDefault(*original_icu_tz_);
    }
    if (original_tz_value_) {
      setenv("TZ", original_tz_value_->c_str(), 1);
    } else {
      unsetenv("TZ");
    }
    tzset();
  }

  void SetTimezoneAndCallTzset(const char* tz_value) {
    setenv("TZ", tz_value, 1);
    tzset();
  }

  std::unique_ptr<icu::TimeZone> original_icu_tz_;
  std::optional<std::string> original_tz_value_;
};

struct YearMonthDay {
  int year;
  int month;
  int day;
};

struct TimezoneTestData {
  std::string test_name;
  std::string tz_value;
  std::string timezone_id;
  int32_t timezone_offset;

  YearMonthDay standard_start_day;

  std::optional<YearMonthDay> daylight_start_day = std::nullopt;

  friend void PrintTo(const TimezoneTestData& data, ::std::ostream* os) {
    *os << "{ test_name: \"" << data.test_name << "\", tz_value: \""
        << data.tz_value << "\", timezone_id: \"" << data.timezone_id
        << "\", timezone_offset: " << data.timezone_offset;
    auto print_date_check = [&](const char* name,
                                const std::optional<YearMonthDay>& check) {
      if (check) {
        *os << ", " << name << ": { year: " << check->year
            << ", month: " << check->month << ", day: " << check->day << " }";
      }
    };
    print_date_check("standard_start_day",
                     std::optional(data.standard_start_day));
    print_date_check("daylight_start_day", data.daylight_start_day);
    *os << " }";
  }
};

class Timezone : public TzsetTest,
                 public ::testing::WithParamInterface<TimezoneTestData> {
 protected:
  // Verify the timezone offset at a specific date.
  void VerifyDateOffset(const icu::TimeZone& tz,
                        const YearMonthDay& date,
                        int32_t expected_timezone_offset,
                        const char* transition_name) {
    UErrorCode status = U_ZERO_ERROR;
    icu::GregorianCalendar calendar(tz, status);
    ASSERT_TRUE(U_SUCCESS(status));
    // Set the time to noon to avoid ambiguity around the default 2 AM
    // transition time.
    calendar.set(date.year, date.month, date.day, 12, 0, 0);
    int32_t raw_offset, dst_offset;
    tz.getOffset(calendar.getTime(status), false, raw_offset, dst_offset,
                 status);
    ASSERT_TRUE(U_SUCCESS(status));
    EXPECT_EQ(expected_timezone_offset * 1000, raw_offset + dst_offset)
        << "Failed " << transition_name << " time offset check for date "
        << date.year << "/" << date.month + 1 << "/" << date.day;
  }

  // Calculates the day before a given daylight savings transition
  // date.
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
      TimezoneTestData{"IanaUTC",
                       "UTC",
                       "UTC",
                       0,
                       {kTestyear, UCAL_JANUARY, 1}},
      TimezoneTestData{"IanaInvalidName",
                       "Invalid/Timezone",
                       "GMT",
                       0,
                       {kTestyear, UCAL_JANUARY, 1}},
      TimezoneTestData{
          "IanaAmericaNewYork",
          "America/New_York",
          "America/New_York",
          -5 * kSecondsInHour,
          {kTestyear, UCAL_NOVEMBER, 5},
          std::make_optional<YearMonthDay>({kTestyear, UCAL_MARCH, 12})},
      TimezoneTestData{
          "IanaAmericaChicago",
          "America/Chicago",
          "America/Chicago",
          -6 * kSecondsInHour,
          {kTestyear, UCAL_NOVEMBER, 5},
          std::make_optional<YearMonthDay>({kTestyear, UCAL_MARCH, 12})},
      TimezoneTestData{
          "IanaAmericaDenver",
          "America/Denver",
          "America/Denver",
          -7 * kSecondsInHour,
          {kTestyear, UCAL_NOVEMBER, 5},
          std::make_optional<YearMonthDay>({kTestyear, UCAL_MARCH, 12})},
      TimezoneTestData{
          "IanaAmericaLosAngeles",
          "America/Los_Angeles",
          "America/Los_Angeles",
          -8 * kSecondsInHour,
          {kTestyear, UCAL_NOVEMBER, 5},
          std::make_optional<YearMonthDay>({kTestyear, UCAL_MARCH, 12})},
      TimezoneTestData{
          "IanaAmericaAnchorage",
          "America/Anchorage",
          "America/Anchorage",
          -9 * kSecondsInHour,
          {kTestyear, UCAL_NOVEMBER, 5},
          std::make_optional<YearMonthDay>({kTestyear, UCAL_MARCH, 12})},
      TimezoneTestData{"IanaPacificHonoluluNoDst",
                       "Pacific/Honolulu",
                       "Pacific/Honolulu",
                       -10 * kSecondsInHour,
                       {kTestyear, UCAL_JANUARY, 1}},
      TimezoneTestData{
          "IanaEuropeLondon",
          "Europe/London",
          "Europe/London",
          0,
          {kTestyear, UCAL_OCTOBER, 29},
          std::make_optional<YearMonthDay>({kTestyear, UCAL_MARCH, 26})},
      TimezoneTestData{
          "IanaEuropeParis",
          "Europe/Paris",
          "Europe/Paris",
          1 * kSecondsInHour,
          {kTestyear, UCAL_OCTOBER, 29},
          std::make_optional<YearMonthDay>({kTestyear, UCAL_MARCH, 26})},
      TimezoneTestData{
          "IanaEuropeAthens",
          "Europe/Athens",
          "Europe/Athens",
          2 * kSecondsInHour,
          {kTestyear, UCAL_OCTOBER, 29},
          std::make_optional<YearMonthDay>({kTestyear, UCAL_MARCH, 26})},
      TimezoneTestData{"IanaEuropeMoscowNoDst",
                       "Europe/Moscow",
                       "Europe/Moscow",
                       3 * kSecondsInHour,
                       {kTestyear, UCAL_JANUARY, 1}},
      TimezoneTestData{"IanaAsiaTokyoNoDst",
                       "Asia/Tokyo",
                       "Asia/Tokyo",
                       9 * kSecondsInHour,
                       {kTestyear, UCAL_JANUARY, 1}},
      TimezoneTestData{"IanaAsiaKolkataNoDst",
                       "Asia/Kolkata",
                       "Asia/Kolkata",
                       (5 * kSecondsInHour + 30 * 60),
                       {kTestyear, UCAL_JANUARY, 1}},
      TimezoneTestData{
          "IanaAfricaCairo",
          "Africa/Cairo",
          "Africa/Cairo",
          2 * kSecondsInHour,
          {kTestyear, UCAL_OCTOBER, 27},
          std::make_optional<YearMonthDay>({kTestyear, UCAL_APRIL, 28})},

      // --- Non-IANA POSIX Test Cases ---
      TimezoneTestData{"PosixFixedOffsetNoDst",
                       "PST8",
                       "PST",
                       -8 * kSecondsInHour,
                       {kTestyear, UCAL_JANUARY, 1}},
      TimezoneTestData{
          "PosixStandardUsDstRules",
          "EST5EDT,M3.2.0,M11.1.0",
          "EST",
          -5 * kSecondsInHour,
          {kTestyear, UCAL_NOVEMBER, 5},
          std::make_optional<YearMonthDay>({kTestyear, UCAL_MARCH, 12})},
      TimezoneTestData{
          "PosixEuropeanDstRules",
          "CET-1CEST,M3.5.0/2,M10.5.0/3",
          "CET",
          1 * kSecondsInHour,
          {kTestyear, UCAL_OCTOBER, 29},
          std::make_optional<YearMonthDay>({kTestyear, UCAL_MARCH, 26})},
      TimezoneTestData{
          "PosixWeekdayRuleSaturday",
          "CAT-2CAST,M4.1.6,M10.5.6",
          "CAT",
          2 * kSecondsInHour,
          {kTestyear, UCAL_OCTOBER, 28},
          std::make_optional<YearMonthDay>({kTestyear, UCAL_APRIL, 1})},
      TimezoneTestData{
          "PosixWeekdayRuleFriday",
          "FKT-4FKST,M3.2.5,M11.1.5",
          "FKT",
          4 * kSecondsInHour,
          {kTestyear, UCAL_NOVEMBER, 3},
          std::make_optional<YearMonthDay>({kTestyear, UCAL_MARCH, 10})},
      TimezoneTestData{
          "PosixJulianRuleOneBased",
          "JST-9JDT,J70,J300",
          "JST",
          9 * kSecondsInHour,
          {kTestyear, UCAL_OCTOBER, 27},
          std::make_optional<YearMonthDay>({kTestyear, UCAL_MARCH, 11})},
      TimezoneTestData{
          "PosixJulianRuleZeroBased",
          "ZST+5ZDT,69,299",
          "ZST",
          -5 * kSecondsInHour,
          {kTestyear, UCAL_OCTOBER, 27},
          std::make_optional<YearMonthDay>({kTestyear, UCAL_MARCH, 11})},
  };
};

TEST_P(Timezone, SetsIcuDefaultCorrectly) {
  const auto& param = GetParam();
  SetTimezoneAndCallTzset(param.tz_value.c_str());

  const icu::TimeZone* default_tz = icu::TimeZone::createDefault();

  EXPECT_EQ(param.timezone_offset * 1000, default_tz->getRawOffset());
  EXPECT_EQ(param.daylight_start_day.has_value(),
            default_tz->useDaylightTime());

  icu::UnicodeString id;
  default_tz->getID(id);
  EXPECT_EQ(param.timezone_id, ToString(id));

  if (param.daylight_start_day) {
    // Verify that the timezone offset changes on the expected day.
    EXPECT_EQ(kSecondsInHour * 1000, default_tz->getDSTSavings());

    VerifyDateOffset(*default_tz, *param.daylight_start_day,
                     param.timezone_offset + kSecondsInHour,
                     "on daylight time start");

    VerifyDateOffset(*default_tz,
                     GetDayBefore(*default_tz, *param.daylight_start_day),
                     param.timezone_offset, "before daylight time start");

    VerifyDateOffset(*default_tz, param.standard_start_day,
                     param.timezone_offset, "on standard time start");

    VerifyDateOffset(
        *default_tz, GetDayBefore(*default_tz, param.standard_start_day),
        param.timezone_offset + kSecondsInHour, "before standard time start");

  } else {
    // Verify the expected timezone offset.
    VerifyDateOffset(*default_tz, param.standard_start_day,
                     param.timezone_offset, "standard time");
  }
}

// Instantiates the test suite timezones, named
// "Tzset/Timezone.SetsIcuDefaultCorrectly/LocationName"
INSTANTIATE_TEST_SUITE_P(Tzset,
                         Timezone,
                         ::testing::ValuesIn(Timezone::kAllTestCases),
                         [](const auto& info) { return info.param.test_name; });

}  // namespace
