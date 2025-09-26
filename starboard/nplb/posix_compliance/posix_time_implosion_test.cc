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
#include "starboard/nplb/posix_compliance/scoped_tz_set.h"

namespace nplb {
namespace {

// Define a function pointer type for the time conversion functions.
using TimeFunction = std::function<time_t(struct tm*)>;

struct TimeFunctionInfo {
  // The name of the function.
  const char* name;
  // The function to be tested.
  TimeFunction function;

  friend void PrintTo(const TimeFunctionInfo& data, ::std::ostream* os) {
    *os << "{ name: \"" << data.name << "\" }";
  }
};

// Parameter struct for the tests.
struct TimeTestData {
  // A description for this test case.
  const char* case_name;
  // The timezone to be used for the test.
  const char* timezone;
  // The tm values to be converted.
  struct tm input_tm;
  // The expected tm values after normalization.
  struct tm expected_tm;
  // The expected time_t value.
  time_t expected_time_val;
  // Whether the test is hermetic only.
  bool hermetic_only = false;

  friend void PrintTo(const TimeTestData& data, ::std::ostream* os) {
    *os << "{ case_name: \"" << data.case_name << ", timezone: \""
        << data.timezone << ", expected_time_val: " << data.expected_time_val
        << ", hermetic_only: " << data.hermetic_only
        << ", input_tm: { tm_sec: " << data.input_tm.tm_sec
        << ", tm_min: " << data.input_tm.tm_min
        << ", tm_hour: " << data.input_tm.tm_hour
        << ", tm_mday: " << data.input_tm.tm_mday
        << ", tm_mon: " << data.input_tm.tm_mon
        << ", tm_year: " << data.input_tm.tm_year
        << ", tm_wday: " << data.input_tm.tm_wday
        << ", tm_yday: " << data.input_tm.tm_yday
        << ", tm_isdst: " << data.input_tm.tm_isdst << " } }";
  }
};

class PosixTimeImplosionTest : public ::testing::TestWithParam<
                                   std::tuple<TimeFunctionInfo, TimeTestData>> {
};

TEST_P(PosixTimeImplosionTest, HandlesTime) {
  const auto& function_info = std::get<0>(GetParam());
  const auto& test_data = std::get<1>(GetParam());
  // TODO: b/390675141 - Remove this after non-hermetic linux build is removed.
#if !BUILDFLAG(ENABLE_COBALT_HERMETIC_HACKS)
  if (test_data.hermetic_only) {
    GTEST_SKIP() << "Non-hermetic builds fail this test for "
                 << test_data.case_name;
  }
#endif
  ScopedTzSet tz(test_data.timezone);

  struct tm tm_to_convert = test_data.input_tm;
  time_t result = function_info.function(&tm_to_convert);

  EXPECT_EQ(result, test_data.expected_time_val);
  ExpectTmEqual(tm_to_convert, test_data.expected_tm, test_data.case_name);
}

// Data for timegm
constexpr TimeTestData kTimegmCases[] = {
    {.case_name = "EpochTime",
     .timezone = "UTC",
     .input_tm = {.tm_sec = 0,
                  .tm_min = 0,
                  .tm_hour = 0,
                  .tm_mday = 1,    // 1st day of the month.
                  .tm_mon = 0,     // January (0-11).
                  .tm_year = 70,   // Years since 1900 (1970-1900).
                  .tm_isdst = 0},  // Daylight Saving Time is not in effect.
     .expected_tm = {.tm_sec = 0,
                     .tm_min = 0,
                     .tm_hour = 0,
                     .tm_mday = 1,    // 1st day of the month.
                     .tm_mon = 0,     // January (0-11).
                     .tm_year = 70,   // Years since 1900 (1970-1900).
                     .tm_wday = 4,    // Thursday.
                     .tm_yday = 0,    // Day of the year (0-365).
                     .tm_isdst = 0},  // Daylight Saving Time is not in effect.
     .expected_time_val = 0},
    {.case_name = "SpecificKnownPositiveTime",
     .timezone = "UTC",
     .input_tm = {.tm_sec = 0,
                  .tm_min = 30,
                  .tm_hour = 10,
                  .tm_mday = 26,   // 26th day of the month.
                  .tm_mon = 9,     // October (0-11).
                  .tm_year = 123,  // Years since 1900 (2023-1900).
                  .tm_isdst = 0},  // Daylight Saving Time is not in effect.
     .expected_tm = {.tm_sec = 0,
                     .tm_min = 30,
                     .tm_hour = 10,
                     .tm_mday = 26,   // 26th day of the month.
                     .tm_mon = 9,     // October (0-11).
                     .tm_year = 123,  // Years since 1900 (2023-1900).
                     .tm_wday = 4,    // Thursday.
                     .tm_yday = 298,  // Day of the year (0-365).
                     .tm_isdst = 0},  // Daylight Saving Time is not in effect.
     .expected_time_val = 1698316200},
    {.case_name = "NegativeTimeBeforeEpoch",
     .timezone = "UTC",
     .input_tm = {.tm_sec = 0,
                  .tm_min = 0,
                  .tm_hour = 0,
                  .tm_mday = 31,   // 31st day of the month.
                  .tm_mon = 11,    // December (0-11).
                  .tm_year = 69,   // Years since 1900 (1969-1900).
                  .tm_isdst = 0},  // Daylight Saving Time is not in effect.
     .expected_tm = {.tm_sec = 0,
                     .tm_min = 0,
                     .tm_hour = 0,
                     .tm_mday = 31,   // 31st day of the month.
                     .tm_mon = 11,    // December (0-11).
                     .tm_year = 69,   // Years since 1900 (1969-1900).
                     .tm_wday = 3,    // Wednesday.
                     .tm_yday = 364,  // Day of the year (0-365).
                     .tm_isdst = 0},  // Daylight Saving Time is not in effect.
     .expected_time_val = -kSecondsInDay},
    {.case_name = "Normalization",
     .timezone = "UTC",
     // The time being tested is:
     // Dec+1 33 27:64:66 UTC 2023
     // all members except year overflow and have to be normalized.
     .input_tm = {.tm_sec = 66,
                  .tm_min = 64,
                  .tm_hour = 27,
                  .tm_mday = 33,   // 33rd day of the month.
                  .tm_mon = 12,    // December + 1 (0-11).
                  .tm_year = 123,  // Years since 1900 (2023-1900).
                  .tm_isdst = 1},  // Daylight Saving Time is in effect.
     // All members have to be normalized.
     // $ date -u --date='@1706933106'
     // Sat Feb  3 04:05:06 AM UTC 2024
     .expected_tm = {.tm_sec = 6,     // 66 - 60 = 6
                     .tm_min = 5,     // 64 - 60 + overflow = 5
                     .tm_hour = 4,    // 27 - 24 + overflow = 4
                     .tm_mday = 3,    // 33 - 31 + overflow = 3
                     .tm_mon = 1,     // February (December + 1 + overflow)
                     .tm_year = 124,  // Years since 1900 (2024-1900).
                     .tm_wday = 6,    // Saturday
                     .tm_yday = 33,   // 31 + 2 = 33nd day starting at day 0.
                     .tm_isdst = 0},  // UTC is never daylight savings time.
     .expected_time_val = 1706933106,
     .hermetic_only = true},
    {.case_name = "NegativeNormalization",
     .timezone = "UTC",
     // The time being tested is:
     // Jan-1 -4 -3:-2:-1 UTC 2023
     // all members except year overflow and have to be normalized.
     .input_tm = {.tm_sec = -1,
                  .tm_min = -2,
                  .tm_hour = -3,
                  .tm_mday = -4,   // -4th day of the month.
                  .tm_mon = -1,    // January - 1 (0-11).
                  .tm_year = 123,  // Years since 1900 (2023-1900).
                  .tm_isdst = 1},  // Daylight Saving Time is in effect.
     // All members have to be normalized.
     // $ date -u --date='@1669409879'
     // Fri Nov 25 08:57:59 PM UTC 2022
     .expected_tm = {.tm_sec = 59,    // -1 + 60 = 59
                     .tm_min = 57,    // -2 + 60 - overflow = 57
                     .tm_hour = 20,   // -3 + 24 - overflow = 20
                     .tm_mday = 25,   // -4 + 30 - overflow = 25
                     .tm_mon = 10,    // November (January - 1 - overflow)
                     .tm_year = 122,  // Years since 1900 (2022-1900).
                     .tm_wday = 5,    // Friday
                     .tm_yday = 328,  // 6*31 + 3*30 + 28 + 24 = 328nd day.
                     .tm_isdst = 0},  // UTC is never daylight savings time.
     .expected_time_val = 1669409879,
     .hermetic_only = true},
};

// Data for mktime
constexpr TimeTestData kMktimeCases[] = {
    {.case_name = "EpochTime",
     .timezone = "America/New_York",
     .input_tm = {.tm_sec = 0,
                  .tm_min = 0,
                  .tm_hour = 19,
                  .tm_mday = 31,   // 31st day of the month.
                  .tm_mon = 11,    // December (0-11).
                  .tm_year = 69,   // Years since 1900 (1969-1900).
                  .tm_isdst = 0},  // Daylight Saving Time is not in effect.
     // $ TZ="America/New_York" date --date='@0'
     // Wed Dec 31 19:00:00 EST 1969
     .expected_tm = {.tm_sec = 0,
                     .tm_min = 0,
                     .tm_hour = 19,
                     .tm_mday = 31,   // 31st day of the month.
                     .tm_mon = 11,    // December (0-11).
                     .tm_year = 69,   // Years since 1900 (1969-1900).
                     .tm_wday = 3,    // Wednesday.
                     .tm_yday = 364,  // Day of the year (0-365).
                     .tm_isdst = 0},  // Daylight Saving Time is not in effect.
     .expected_time_val = 0},
    {.case_name = "SpecificKnownPositiveTimeInDST",
     .timezone = "America/New_York",
     .input_tm = {.tm_sec = 0,
                  .tm_min = 30,
                  .tm_hour = 6,
                  .tm_mday = 26,   // 26th day of the month.
                  .tm_mon = 9,     // October (0-11).
                  .tm_year = 123,  // Years since 1900 (2023-1900).
                  .tm_isdst = 1},  // Daylight Saving Time is in effect.
     // $ TZ="America/New_York" date --date='@1698316200'
     // Thu Oct 26 06:30:00 EDT 2023
     .expected_tm = {.tm_sec = 0,
                     .tm_min = 30,
                     .tm_hour = 6,
                     .tm_mday = 26,   // 26th day of the month.
                     .tm_mon = 9,     // October (0-11).
                     .tm_year = 123,  // Years since 1900 (2023-1900).
                     .tm_wday = 4,    // Thursday.
                     .tm_yday = 298,  // Day of the year (0-365).
                     .tm_isdst = 1},  // Daylight Saving Time is in effect.
     .expected_time_val = 1698316200},
    {.case_name = "SpecificKnownPositiveTimeNotInDST",
     .timezone = "America/New_York",
     .input_tm = {.tm_sec = 0,
                  .tm_min = 30,
                  .tm_hour = 5,
                  .tm_mday = 25,   // 25th day of the month.
                  .tm_mon = 11,    // December (0-11).
                  .tm_year = 123,  // Years since 1900 (2023-1900).
                  .tm_isdst = 0},  // Daylight Saving Time is not in effect.
     // $ TZ="America/New_York" date --date='@1703500200'
     // Mon Dec 25 05:30:00 EST 2023
     .expected_tm = {.tm_sec = 0,
                     .tm_min = 30,
                     .tm_hour = 5,
                     .tm_mday = 25,   // 25th day of the month.
                     .tm_mon = 11,    // December (0-11).
                     .tm_year = 123,  // Years since 1900 (2023-1900).
                     .tm_wday = 1,    // Monday.
                     .tm_yday = 358,  // Day of the year (0-365).
                     .tm_isdst = 0},  // Daylight Saving Time is not in effect.
     .expected_time_val = 1703500200},
    {.case_name = "NegativeTimeBeforeEpoch",
     .timezone = "America/New_York",
     .input_tm = {.tm_sec = 0,
                  .tm_min = 0,
                  .tm_hour = 19,
                  .tm_mday = 30,   // 30th day of the month.
                  .tm_mon = 11,    // December (0-11).
                  .tm_year = 69,   // Years since 1900 (1969-1900).
                  .tm_isdst = 0},  // Daylight Saving Time is not in effect.
     // $ TZ="America/New_York" date --date='@-86400'
     // Tue Dec 30 19:00:00 EST 1969
     .expected_tm = {.tm_sec = 0,
                     .tm_min = 0,
                     .tm_hour = 19,
                     .tm_mday = 30,   // 30th day of the month.
                     .tm_mon = 11,    // December (0-11).
                     .tm_year = 69,   // Years since 1900 (1969-1900).
                     .tm_wday = 2,    // Tuesday.
                     .tm_yday = 363,  // Day of the year (0-365).
                     .tm_isdst = 0},  // Daylight Saving Time is not in effect.
     .expected_time_val = -kSecondsInDay},
    {.case_name = "NormalizationNotInDST",
     .timezone = "America/New_York",
     // The time being tested is:
     // Dec+1 33 27:64:66 2023
     // all members except year overflow and have to be normalized.
     .input_tm = {.tm_sec = 66,
                  .tm_min = 64,
                  .tm_hour = 27,
                  .tm_mday = 33,   // 33rd day of the month.
                  .tm_mon = 12,    // December + 1 (0-11).
                  .tm_year = 123,  // Years since 1900 (2023-1900).
                  .tm_isdst = 1},  // Daylight Saving Time is in effect.
     // All members have to be normalized.
     // $TZ="America/New_York" date --date='@1706951106'
     // Sat Feb  3 04:05:06 AM EST 2024
     .expected_tm = {.tm_sec = 6,     // 66 - 60 = 6
                     .tm_min = 5,     // 64 - 60 + overflow = 5
                     .tm_hour = 4,    // 27 - 24 + overflow = 4
                     .tm_mday = 3,    // 33 - 31 + overflow = 3
                     .tm_mon = 1,     // February (December + 1 + overflow)
                     .tm_year = 124,  // Years since 1900 (2024-1900).
                     .tm_wday = 6,    // Saturday
                     .tm_yday = 33,   // 31 + 2 = 33nd day starting at day 0.
                     .tm_isdst = 0},  // February in New York is not DST.
     .expected_time_val = 1706951106,
     .hermetic_only = true},
    {.case_name = "NormalizationInDST",
     .timezone = "America/New_York",
     // The time being tested is:
     // Jul 33 27:64:66 2023
     // tm_mday, tm_hour, tm_min, and tm_sec all overflow and have to be
     // normalized.
     .input_tm = {.tm_sec = 66,
                  .tm_min = 64,
                  .tm_hour = 27,
                  .tm_mday = 33,   // 33rd day of the month.
                  .tm_mon = 6,     // July (0-11).
                  .tm_year = 123,  // Years since 1900 (2023-1900).
                  .tm_isdst = 0},  // Daylight Saving Time is not in effect.
     // All members have to be normalized.
     // $TZ="America/New_York" date --date='@1691049906'
     // Thu Aug  3 04:05:06 AM EDT 2023
     .expected_tm =
         {
             .tm_sec = 6,     // 66 - 60 = 6
             .tm_min = 5,     // 64 - 60 + overflow = 5
             .tm_hour = 4,    // 27 - 24 + overflow = 4
             .tm_mday = 3,    // 33 - 31 + overflow = 3
             .tm_mon = 7,     // August
             .tm_year = 123,  // Years since 1900 (2023-1900).
             .tm_wday = 4,    // Thursday
             .tm_yday = 214,  // (4*31+28+2*30+2 = 214)th day starting at day 0.
             .tm_isdst = 1},  // Summer in New York is daylight savings time.
     .expected_time_val = 1691049906,
     .hermetic_only = true},
    {.case_name = "NegativeNormalization",
     .timezone = "America/New_York",
     // The time being tested is:
     // Jan-1 -4 -3:-2:-1 2023
     // All members except year overflow and have to be normalized.
     .input_tm = {.tm_sec = -1,
                  .tm_min = -2,
                  .tm_hour = -3,
                  .tm_mday = -4,   // -4th day of the month.
                  .tm_mon = -1,    // January -1 (0-11).
                  .tm_year = 123,  // Years since 1900 (2023-1900).
                  .tm_isdst = 1},  // Daylight Saving Time is in effect.
     // All members have to be normalized.
     // $TZ="America/New_York" date --date='@1669427879'
     // Fri Nov 25 08:57:59 PM EST 2022
     .expected_tm = {.tm_sec = 59,    // -1 + 60 = 59
                     .tm_min = 57,    // -2 + 60 - overflow = 57
                     .tm_hour = 20,   // -3 + 24 - overflow = 20
                     .tm_mday = 25,   // -4 + 30 - overflow = 26
                     .tm_mon = 10,    // November (January - 1 - overflow)
                     .tm_year = 122,  // Years since 1900 (2022-1900).
                     .tm_wday = 5,    // Friday
                     .tm_yday = 328,  // 6*31 + 3*30 + 28 + 24 = 328nd day.
                     .tm_isdst = 0},  // February in New York is not DST.
     .expected_time_val = 1669427879,
     .hermetic_only = true}};

INSTANTIATE_TEST_SUITE_P(Timegm,
                         PosixTimeImplosionTest,
                         ::testing::Combine(::testing::Values(TimeFunctionInfo{
                                                "Timegm", timegm}),
                                            ::testing::ValuesIn(kTimegmCases)),
                         [](const auto& info) {
                           return std::string(std::get<0>(info.param).name) +
                                  "_" + std::get<1>(info.param).case_name;
                         });

INSTANTIATE_TEST_SUITE_P(Mktime,
                         PosixTimeImplosionTest,
                         ::testing::Combine(::testing::Values(TimeFunctionInfo{
                                                "Mktime", mktime}),
                                            ::testing::ValuesIn(kMktimeCases)),
                         [](const auto& info) {
                           return std::string(std::get<0>(info.param).name) +
                                  "_" + std::get<1>(info.param).case_name;
                         });

}  // namespace
}  // namespace nplb
