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
#include <unistd.h>

#include <climits>
#include <ctime>
#include <limits>

#include "starboard/nplb/posix_compliance/posix_time_helper.h"
#include "starboard/nplb/posix_compliance/scoped_tz_set.h"

namespace nplb {
namespace {

constexpr const char* kUtcOrGmt = "UTC or GMT";

// Define a function pointer type for the time conversion functions.
using TimeFunction = std::function<struct tm*(const time_t*, struct tm*)>;

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
  // The expected tm values.
  struct tm expected_tm;
  // The time_t value to be converted.
  time_t time_val;
  // The expected tm_gmtoff value.
  long expected_gmtoff;
  // The expected tm_zone value.
  const char* expected_zone;
  // Whether the test is hermetic only.
  bool hermetic_only = false;

  friend void PrintTo(const TimeTestData& data, ::std::ostream* os) {
    *os << "{ case_name: \"" << data.case_name << ", timezone: \""
        << data.timezone << ", time_val: " << data.time_val
        << ", expected_gmtoff: " << data.expected_gmtoff
        << ", expected_zone: \"" << data.expected_zone
        << ", hermetic_only: " << data.hermetic_only
        << ", expected_tm: { tm_sec: " << data.expected_tm.tm_sec
        << ", tm_min: " << data.expected_tm.tm_min
        << ", tm_hour: " << data.expected_tm.tm_hour
        << ", tm_mday: " << data.expected_tm.tm_mday
        << ", tm_mon: " << data.expected_tm.tm_mon
        << ", tm_year: " << data.expected_tm.tm_year
        << ", tm_wday: " << data.expected_tm.tm_wday
        << ", tm_yday: " << data.expected_tm.tm_yday
        << ", tm_isdst: " << data.expected_tm.tm_isdst << " } }\n";
  }
};

class PosixTimeExplosionTest : public ::testing::TestWithParam<
                                   std::tuple<TimeFunctionInfo, TimeTestData>> {
};

class PosixTimeExplosionOverflowTest
    : public ::testing::TestWithParam<
          std::tuple<TimeFunctionInfo, TimeTestData>> {};

TEST_P(PosixTimeExplosionTest, HandlesTime) {
  const auto& function_info = std::get<0>(GetParam());
  const auto& test_data = std::get<1>(GetParam());
  ScopedTzSet tz(test_data.timezone);

  struct tm result_tm = {};
  struct tm* result_tm_ptr =
      function_info.function(&test_data.time_val, &result_tm);

  ASSERT_NE(result_tm_ptr, nullptr)
      << "time_function returned nullptr for " << test_data.case_name;
  ExpectTmEqual(*result_tm_ptr, test_data.expected_tm, test_data.case_name);
  EXPECT_EQ(result_tm_ptr->tm_gmtoff, test_data.expected_gmtoff);
  // Allow either "UTC" or "GMT" for gmtime tests.
  if (test_data.expected_zone == kUtcOrGmt) {
    EXPECT_TRUE(strcmp(result_tm_ptr->tm_zone, "UTC") == 0 ||
                strcmp(result_tm_ptr->tm_zone, "GMT") == 0);
  } else {
    EXPECT_STREQ(result_tm_ptr->tm_zone, test_data.expected_zone);
  }
}

TEST_P(PosixTimeExplosionOverflowTest, HandlesTimeOverflow) {
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

  struct tm result_tm = {};
  errno = 0;
  struct tm* result_tm_ptr =
      function_info.function(&test_data.time_val, &result_tm);

  // For 64-bit time_t, the time value is a very large number, representing a
  // year extremely far in the future or past. The corresponding tm_year
  // (year - 1900) would vastly exceed INT_MAX or INT_MIN. Thus, the function
  // is expected to return nullptr.
  EXPECT_EQ(result_tm_ptr, nullptr)
      << function_info.name << "(&time_val) should return nullptr "
      << "due to expected overflow in struct tm.tm_year. time_t: "
      << test_data.time_val;
  EXPECT_EQ(errno, EOVERFLOW)
      << function_info.name << "(&time_val) should set errno to EOVERFLOW "
      << "due to expected overflow in struct tm.tm_year. time_t: "
      << test_data.time_val << ", errno: " << errno << " (" << strerror(errno)
      << ")";
}

// Wrappers for gmtime and localtime to match the TimeFunction signature.
struct tm* gmtime_wrapper(const time_t* timep, struct tm* result) {
  return gmtime(timep);
}

struct tm* localtime_wrapper(const time_t* timep, struct tm* result) {
  return localtime(timep);
}

// Data for gmtime and gmtime_r
constexpr TimeTestData kGmtimeCases[] = {
    {.case_name = "EpochTime",
     .timezone = "UTC",
     .expected_tm = {.tm_sec = 0,
                     .tm_min = 0,
                     .tm_hour = 0,
                     .tm_mday = 1,    // 1st day of the month.
                     .tm_mon = 0,     // January (0-11).
                     .tm_year = 70,   // 1970 - 1900.
                     .tm_wday = 4,    // Thursday.
                     .tm_yday = 0,    // Day of the year (0-365).
                     .tm_isdst = 0},  // Daylight Saving Time is not in effect.
     .time_val = 0,                   // January 1, 1970, 00:00:00 UTC.
     .expected_gmtoff = 0,
     .expected_zone = kUtcOrGmt},
    {.case_name = "SpecificKnownPositiveTime",
     .timezone = "UTC",
     // Generated using: date -u -d "2023-10-26 10:30:00 UTC" +%s
     .expected_tm = {.tm_sec = 0,
                     .tm_min = 30,
                     .tm_hour = 10,
                     .tm_mday = 26,   // 26th day of the month.
                     .tm_mon = 9,     // October (0-11).
                     .tm_year = 123,  // 2023 - 1900.
                     .tm_wday = 4,    // Thursday.
                     // tm_yday calculation for Oct 26, 2023 (non-leap year):
                     // 31(Jan)+28(Feb)+31(Mar)+30(Apr)+31(May)+30(Jun)+
                     // 31(Jul)+31(Aug)+30(Sep) = 273.
                     // tm_yday = 273 + 26 - 1 = 298.
                     .tm_yday = 298,
                     .tm_isdst = 0},
     .time_val = 1698316200,
     .expected_gmtoff = 0,
     .expected_zone = kUtcOrGmt},
    {.case_name = "NegativeStdTimeBeforeEpoch",
     .timezone = "UTC",
     .expected_tm = {.tm_sec = 0,
                     .tm_min = 0,
                     .tm_hour = 0,
                     .tm_mday = 31,   // 31st day of the month.
                     .tm_mon = 11,    // December (0-11).
                     .tm_year = 69,   // 1969 - 1900.
                     .tm_wday = 3,    // Wednesday.
                     .tm_yday = 364,  // Day of the year (0-365).
                     .tm_isdst = 0},
     // Timestamp for 1969-12-31 00:00:00 UTC
     .time_val = -kSecondsInDay,
     .expected_gmtoff = 0,
     .expected_zone = kUtcOrGmt},
    {.case_name = "NegativeDstTimeBeforeEpoch",
     .timezone = "UTC",
     .expected_tm = {.tm_sec = 0,
                     .tm_min = 0,
                     .tm_hour = 0,
                     .tm_mday = 30,   // 30th day of the month.
                     .tm_mon = 5,     // June (0-11).
                     .tm_year = 69,   // 1969 - 1900.
                     .tm_wday = 1,    // Monday.
                     .tm_yday = 180,  // Day of the year (0-365).
                     .tm_isdst = 0},
     // Timestamp for 1969-06-30 00:00:00 UTC
     // Six months and one day before December, 4 of which have 31 days.
     .time_val = -(30 * 6 + 5) * kSecondsInDay,
     .expected_gmtoff = 0,
     .expected_zone = kUtcOrGmt},
    {.case_name = "July2024",
     .timezone = "UTC",
     // Generated with: date -u -d "2024-07-31 23:32:59 UTC" +%s.
     .expected_tm = {.tm_sec = 59,
                     .tm_min = 32,
                     .tm_hour = 23,
                     .tm_mday = 31,   // 31st day of the month.
                     .tm_mon = 6,     // July (0-11).
                     .tm_year = 124,  // 2024 - 1900.
                     .tm_wday = 3,    // Wednesday.
                     .tm_yday = 212,  // 2024 is a leap year.
                     .tm_isdst = 0},
     .time_val = 1722468779,
     .expected_gmtoff = 0,
     .expected_zone = kUtcOrGmt}};

// Data for localtime and localtime_r
constexpr TimeTestData kLocaltimeCases[] = {
    {.case_name = "EpochTime",
     .timezone = "America/New_York",
     .expected_tm = {.tm_sec = 0,
                     .tm_min = 0,
                     .tm_hour = 19,
                     .tm_mday = 31,   // 31st day of the month.
                     .tm_mon = 11,    // December (0-11).
                     .tm_year = 69,   // 1969 - 1900.
                     .tm_wday = 3,    // Wednesday.
                     .tm_yday = 364,  // Day of the year (0-365).
                     .tm_isdst = 0},  // Daylight Saving Time is not in effect.
     .time_val = 0,                   // January 1, 1970, 00:00:00 UTC.
     .expected_gmtoff = -18000,
     .expected_zone = "EST"},
    {.case_name = "SpecificKnownPositiveTimeInDST",
     .timezone = "America/New_York",
     // Generated using: date -u -d "2023-10-26 10:30:00 UTC" +%s
     .expected_tm = {.tm_sec = 0,
                     .tm_min = 30,
                     .tm_hour = 6,
                     .tm_mday = 26,   // 26th day of the month.
                     .tm_mon = 9,     // October (0-11).
                     .tm_year = 123,  // 2023 - 1900.
                     .tm_wday = 4,    // Thursday.
                     .tm_yday = 298,  // Day of the year (0-365).
                     .tm_isdst = 1},  // Daylight Saving Time is in effect.
     .time_val = 1698316200,
     .expected_gmtoff = -14400,
     .expected_zone = "EDT"},
    {.case_name = "SpecificKnownPositiveTimeNotInDST",
     .timezone = "America/New_York",
     // Generated using: date -u -d "2023-12-25 10:30:00 UTC" +%s
     .expected_tm = {.tm_sec = 0,
                     .tm_min = 30,
                     .tm_hour = 5,
                     .tm_mday = 25,   // 25th day of the month.
                     .tm_mon = 11,    // December (0-11).
                     .tm_year = 123,  // 2023 - 1900.
                     .tm_wday = 1,    // Monday.
                     .tm_yday = 358,  // Day of the year (0-365).
                     .tm_isdst = 0},  // Daylight Saving Time is not in effect.
     .time_val = 1703500200,
     .expected_gmtoff = -18000,
     .expected_zone = "EST"},
    {.case_name = "NegativeStdTimeBeforeEpoch",
     .timezone = "America/New_York",
     .expected_tm = {.tm_sec = 0,
                     .tm_min = 0,
                     .tm_hour = 19,
                     .tm_mday = 30,   // 30th day of the month.
                     .tm_mon = 11,    // December (0-11).
                     .tm_year = 69,   // 1969 - 1900.
                     .tm_wday = 2,    // Tuesday.
                     .tm_yday = 363,  // Day of the year (0-365).
                     .tm_isdst = 0},  // Daylight Saving Time is not in effect.
     // Timestamp for 1969-12-31 00:00:00 UTC
     .time_val = -kSecondsInDay,
     .expected_gmtoff = -18000,
     .expected_zone = "EST"},
    {.case_name = "NegativeDstTimeBeforeEpoch",
     .timezone = "America/New_York",
     .expected_tm = {.tm_sec = 0,
                     .tm_min = 0,
                     .tm_hour = 20,
                     .tm_mday = 30,   // 30th day of the month.
                     .tm_mon = 5,     // June (0-11).
                     .tm_year = 69,   // 1969 - 1900.
                     .tm_wday = 1,    // Monday.
                     .tm_yday = 180,  // Day of the year (0-365).
                     .tm_isdst = 1},  // Daylight Saving Time is in effect.
     // Timestamp for 1969-06-30 00:00:00 UTC
     // Six months before December, 4 of which have 31 days.
     .time_val = -(30 * 6 + 4) * kSecondsInDay,
     .expected_gmtoff = -14400,
     .expected_zone = "EDT"},
    {.case_name = "SpecificKnownPositiveTimeInDifferentTimezone",
     .timezone = "Europe/London",
     // Generated using: date -u -d "2023-10-26 10:30:00 UTC" +%s
     .expected_tm = {.tm_sec = 0,
                     .tm_min = 30,
                     .tm_hour = 11,
                     .tm_mday = 26,   // 26st day of the month.
                     .tm_mon = 9,     // October (0-11).
                     .tm_year = 123,  // 2023 - 1900.
                     .tm_wday = 4,    // Thursday.
                     .tm_yday = 298,  // Day of the year (0-365).
                     .tm_isdst = 1},
     .time_val = 1698316200,
     .expected_gmtoff = 3600,
     .expected_zone = "BST"},
    {.case_name = "SpecificKnownPositiveTimeInUTC",
     .timezone = "UTC",
     // Generated using: date -u -d "2023-10-26 10:30:00 UTC" +%s
     .expected_tm = {.tm_sec = 0,
                     .tm_min = 30,
                     .tm_hour = 10,
                     .tm_mday = 26,   // 26st day of the month.
                     .tm_mon = 9,     // October (0-11).
                     .tm_year = 123,  // 2023 - 1900.
                     .tm_wday = 4,    // Thursday.
                     .tm_yday = 298,  // Day of the year (0-365).
                     .tm_isdst = 0},
     .time_val = 1698316200,
     .expected_gmtoff = 0,
     .expected_zone = kUtcOrGmt}};

INSTANTIATE_TEST_SUITE_P(
    Gmtime,
    PosixTimeExplosionTest,
    ::testing::Combine(::testing::Values(TimeFunctionInfo{"Gmtime",
                                                          gmtime_wrapper},
                                         TimeFunctionInfo{"GmtimeR", gmtime_r}),
                       ::testing::ValuesIn(kGmtimeCases)),
    [](const auto& info) {
      return std::string(std::get<0>(info.param).name) + "_" +
             std::get<1>(info.param).case_name;
    });

INSTANTIATE_TEST_SUITE_P(
    Localtime,
    PosixTimeExplosionTest,
    ::testing::Combine(
        ::testing::Values(TimeFunctionInfo{"Localtime", localtime_wrapper},
                          TimeFunctionInfo{"LocaltimeR", localtime_r}),
        ::testing::ValuesIn(kLocaltimeCases)),
    [](const auto& info) {
      return std::string(std::get<0>(info.param).name) + "_" +
             std::get<1>(info.param).case_name;
    });

constexpr TimeTestData kGmtimeOverflowCases[] = {
    {.case_name = "TimeTMaxValueOverflow",
     .timezone = "UTC",
     .expected_tm = {},
     .time_val = std::numeric_limits<time_t>::max(),
     .expected_gmtoff = 0,
     .expected_zone = "UTC"},
    {.case_name = "TimeTHighValueOverflow",
     .timezone = "UTC",
     .expected_tm = {},
     // Tue Dec 31 23:59:59  2147483647
     .time_val = 67767976233521999 + 1,
     .expected_gmtoff = 0,
     .expected_zone = "UTC",
     .hermetic_only = true},
    {.case_name = "TimeTMinValueOverflow",
     .timezone = "UTC",
     .expected_tm = {},
     .time_val = std::numeric_limits<time_t>::min(),
     .expected_gmtoff = 0,
     .expected_zone = "UTC"},
    {.case_name = "TimeTLowValueOverflow",
     .timezone = "UTC",
     .expected_tm = {},
     // Thu Jan  1 00:00:00 -2147481748
     .time_val = -67768040609712422 - 1,
     .expected_gmtoff = 0,
     .expected_zone = "UTC",
     .hermetic_only = true}};

constexpr TimeTestData kLocaltimeOverflowCases[] = {
    {.case_name = "TimeTMaxValueOverflow",
     .timezone = "America/New_York",
     .expected_tm = {},
     .time_val = std::numeric_limits<time_t>::max(),
     .expected_gmtoff = 0,
     .expected_zone = "EST"},
    {.case_name = "TimeTHighValueOverflow",
     .timezone = "America/New_York",
     .expected_tm = {},
     // Tue Dec 31 23:59:59  2147483647
     // Ensure that time_val will overflow the tm_year member variable.
     .time_val = 67767976233521999 + 1,
     .expected_gmtoff = 0,
     .expected_zone = "EST",
     .hermetic_only = true},
    {.case_name = "TimeTMinValueOverflow",
     .timezone = "America/New_York",
     .expected_tm = {},
     .time_val = std::numeric_limits<time_t>::min(),
     .expected_gmtoff = 0,
     .expected_zone = "EST"},
    {.case_name = "TimeTLowValueOverflow",
     .timezone = "America/New_York",
     .expected_tm = {},
     // Thu Jan  1 00:00:00 -2147481748
     // Ensure that time_val will overflow the tm_year member variable.
     .time_val = -67768040609712422 - 1,
     .expected_gmtoff = 0,
     .expected_zone = "EST",
     .hermetic_only = true}};

INSTANTIATE_TEST_SUITE_P(
    Gmtime,
    PosixTimeExplosionOverflowTest,
    ::testing::Combine(::testing::Values(TimeFunctionInfo{"Gmtime",
                                                          gmtime_wrapper},
                                         TimeFunctionInfo{"GmtimeR", gmtime_r}),
                       ::testing::ValuesIn(kGmtimeOverflowCases)),
    [](const auto& info) {
      return std::string(std::get<0>(info.param).name) + "_" +
             std::get<1>(info.param).case_name;
    });

INSTANTIATE_TEST_SUITE_P(
    Localtime,
    PosixTimeExplosionOverflowTest,
    ::testing::Combine(
        ::testing::Values(TimeFunctionInfo{"Localtime", localtime_wrapper},
                          TimeFunctionInfo{"LocaltimeR", localtime_r}),
        ::testing::ValuesIn(kLocaltimeOverflowCases)),
    [](const auto& info) {
      return std::string(std::get<0>(info.param).name) + "_" +
             std::get<1>(info.param).case_name;
    });

using OverwriteFunction = std::function<struct tm*(const time_t*)>;

class PosixTimeExplosionOverwriteTest
    : public ::testing::TestWithParam<OverwriteFunction> {};

// According to POSIX, gmtime and localtime return a pointer to a statically
// allocated struct tm. Subsequent calls to gmtime() or localtime() may
// overwrite this struct.
TEST_P(PosixTimeExplosionOverwriteTest,
       ReturnsPointerToStaticBufferAndOverwrites) {
  auto time_function = GetParam();
  time_t time1 = 0;
  time_t time2 = kSecondsInDay;

  struct tm* tm_ptr_call1 = time_function(&time1);
  ASSERT_NE(tm_ptr_call1, nullptr) << "time_function(&time1) returned nullptr.";
  struct tm tm_val_call1 = *tm_ptr_call1;

  struct tm* tm_ptr_call2 = time_function(&time2);
  ASSERT_NE(tm_ptr_call2, nullptr) << "time_function(&time2) returned nullptr.";

  EXPECT_EQ(tm_ptr_call1, tm_ptr_call2)
      << "Function should return pointers to the same static buffer on "
         "subsequent calls.";

  bool is_overwritten = (tm_val_call1.tm_mday != tm_ptr_call2->tm_mday ||
                         tm_val_call1.tm_wday != tm_ptr_call2->tm_wday ||
                         tm_val_call1.tm_yday != tm_ptr_call2->tm_yday);
  EXPECT_TRUE(is_overwritten)
      << "The content of the static buffer should be overwritten by the "
         "subsequent call to the time function.";
}

INSTANTIATE_TEST_SUITE_P(PosixTimeExplosionOverwriteTests,
                         PosixTimeExplosionOverwriteTest,
                         ::testing::Values(gmtime, localtime));

using ReentrantFunction = std::function<struct tm*(const time_t*, struct tm*)>;

class PosixTimeExplosionReentrantTest
    : public ::testing::TestWithParam<ReentrantFunction> {};

// The gmtime_r and localtime_r functions are reentrant. They should not
// overwrite the results of previous calls.
TEST_P(PosixTimeExplosionReentrantTest, ReentrantFunctionsDoNotOverwrite) {
  auto time_function = GetParam();
  time_t time1 = 0;
  time_t time2 = kSecondsInDay;

  struct tm result1, result2;

  struct tm* tm_ptr_call1 = time_function(&time1, &result1);
  ASSERT_NE(tm_ptr_call1, nullptr) << "time_function(&time1) returned nullptr.";
  struct tm tm_val_call1 = *tm_ptr_call1;

  struct tm* tm_ptr_call2 = time_function(&time2, &result2);
  ASSERT_NE(tm_ptr_call2, nullptr) << "time_function(&time2) returned nullptr.";

  EXPECT_NE(&result1, &result2);
  EXPECT_EQ(tm_ptr_call1, &result1);
  EXPECT_EQ(tm_ptr_call2, &result2);

  // Verify that the first result was not overwritten
  EXPECT_EQ(tm_val_call1.tm_mday, result1.tm_mday);
  EXPECT_EQ(tm_val_call1.tm_wday, result1.tm_wday);
  EXPECT_EQ(tm_val_call1.tm_yday, result1.tm_yday);
}

INSTANTIATE_TEST_SUITE_P(PosixTimeExplosionReentrantTests,
                         PosixTimeExplosionReentrantTest,
                         ::testing::Values(gmtime_r, localtime_r));

}  // namespace
}  // namespace nplb
