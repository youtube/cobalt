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

namespace starboard {
namespace nplb {
namespace {

// localtime_r converts a time_t value to a struct tm in the local timezone.
// According to POSIX:
// - It returns a pointer to the user-provided struct tm.
// - If the time_t value cannot be converted, it returns nullptr.

TEST(PosixLocaltimeRTests, HandlesEpochTime) {
  ScopedTzSet tz("America/New_York");
  time_t epoch_time = 0;  // January 1, 1970, 00:00:00 UTC.

  struct tm result_tm = {};
  struct tm* result_tm_ptr = localtime_r(&epoch_time, &result_tm);
  ASSERT_NE(result_tm_ptr, nullptr)
      << "localtime_r returned nullptr for epoch_time.";

  struct tm expected_tm = {};
  expected_tm.tm_sec = 0;
  expected_tm.tm_min = 0;
  expected_tm.tm_hour = 19;
  expected_tm.tm_mday = 31;  // 31st day of the month.
  expected_tm.tm_mon = 11;   // December (0-11).
  expected_tm.tm_year = 69;  // Years since 1900 (1969 - 1900).
  expected_tm.tm_wday = 3;   // Wednesday (0=Sunday, 1=Monday, ..., 6=Saturday).
  expected_tm.tm_yday = 364;  // Day of the year (0-365). Dec 31st is day 364.
  expected_tm.tm_isdst = 0;   // Daylight Saving Time is not in effect.

  ExpectTmEqual(*result_tm_ptr, expected_tm, "epoch_time");
  EXPECT_EQ(result_tm_ptr->tm_gmtoff, -5 * 3600);
  EXPECT_STREQ(result_tm_ptr->tm_zone, "EST");
}

TEST(PosixLocaltimeRTests, HandlesSpecificKnownPositiveTimeInDST) {
  ScopedTzSet tz("America/New_York");
  // Timestamp for 2023-10-26 10:30:00 UTC which is Thu Oct 26 06:30:00 AM EDT
  // 2023. Generated using: date -u -d "2023-10-26 10:30:00 UTC" +%s  (gives
  // 1698316200)
  time_t specific_time = 1698316200;

  struct tm result_tm = {};
  struct tm* result_tm_ptr = localtime_r(&specific_time, &result_tm);
  ASSERT_NE(result_tm_ptr, nullptr)
      << "localtime_r returned nullptr for specific_time.";

  struct tm expected_tm = {};
  expected_tm.tm_sec = 0;
  expected_tm.tm_min = 30;
  expected_tm.tm_hour = 6;
  expected_tm.tm_mday = 26;   // 26th day of the month.
  expected_tm.tm_mon = 9;     // October (0-11).
  expected_tm.tm_year = 123;  // 2023 - 1900.
  expected_tm.tm_wday = 4;    // Thursday.
  // tm_yday calculation for Oct 26, 2023 (non-leap year):
  // Days in Jan-Sep:
  // 31(Jan)+28(Feb)+31(Mar)+30(Apr)+31(May)+30(Jun)+31(Jul)+31(Aug)+30(Sep) =
  // 273. tm_yday = 273 (days before Oct) + 26 (day in Oct) - 1 (for 0-indexing)
  // = 298.
  expected_tm.tm_yday = 298;
  expected_tm.tm_isdst = 1;  // Daylight Saving Time is in effect.

  ExpectTmEqual(*result_tm_ptr, expected_tm, "specific_time (2023-10-26)");
  EXPECT_EQ(result_tm_ptr->tm_gmtoff, -4 * 3600);
  EXPECT_STREQ(result_tm_ptr->tm_zone, "EDT");
}

TEST(PosixLocaltimeRTests, HandlesSpecificKnownPositiveTimeNotInDST) {
  ScopedTzSet tz("America/New_York");
  // Timestamp for 2023-12-25 10:30:00 UTC which is Mon Dec 25 05:30:00 AM EST
  // 2023. Generated using: date -u -d "2023-12-25 10:30:00 UTC" +%s  (gives
  // 1703500200)
  time_t specific_time = 1703500200;

  struct tm result_tm = {};
  struct tm* result_tm_ptr = localtime_r(&specific_time, &result_tm);
  ASSERT_NE(result_tm_ptr, nullptr)
      << "localtime_r returned nullptr for specific_time.";

  struct tm expected_tm = {};
  expected_tm.tm_sec = 0;
  expected_tm.tm_min = 30;
  expected_tm.tm_hour = 5;
  expected_tm.tm_mday = 25;   // 25th day of the month.
  expected_tm.tm_mon = 11;    // December (0-11).
  expected_tm.tm_year = 123;  // 2023 - 1900.
  expected_tm.tm_wday = 1;    // Monday.
  expected_tm.tm_yday = 358;  // Day of the year (0-365).
  expected_tm.tm_isdst = 0;   // Daylight Saving Time is not in effect.

  ExpectTmEqual(*result_tm_ptr, expected_tm, "specific_time (2023-12-25)");
  EXPECT_EQ(result_tm_ptr->tm_gmtoff, -5 * 3600);
  EXPECT_STREQ(result_tm_ptr->tm_zone, "EST");
}

TEST(PosixLocaltimeRTests,
     HandlesSpecificKnownPositiveTimeInDifferentTimezone) {
  ScopedTzSet tz("Europe/Paris");
  // Timestamp for 2023-10-26 10:30:00 UTC which is Thu Oct 26 12:30:00 PM CEST
  // 2023. Generated using: date -u -d "2023-10-26 10:30:00 UTC" +%s  (gives
  // 1698316200)
  time_t specific_time = 1698316200;

  struct tm result_tm = {};
  struct tm* result_tm_ptr = localtime_r(&specific_time, &result_tm);
  ASSERT_NE(result_tm_ptr, nullptr)
      << "localtime_r returned nullptr for specific_time.";

  struct tm expected_tm = {};
  expected_tm.tm_sec = 0;
  expected_tm.tm_min = 30;
  expected_tm.tm_hour = 12;
  expected_tm.tm_mday = 26;   // 26th day of the month.
  expected_tm.tm_mon = 9;     // October (0-11).
  expected_tm.tm_year = 123;  // 2023 - 1900.
  expected_tm.tm_wday = 4;    // Thursday.
  expected_tm.tm_yday = 298;  // Day of the year (0-365).
  expected_tm.tm_isdst = 1;   // Daylight Saving Time is in effect.

  ExpectTmEqual(*result_tm_ptr, expected_tm, "specific_time (2023-10-26)");
  EXPECT_EQ(result_tm_ptr->tm_gmtoff, 2 * 3600);
  EXPECT_STREQ(result_tm_ptr->tm_zone, "CEST");
}

TEST(PosixLocaltimeRTests, HandlesSpecificKnownPositiveTimeInUTC) {
  ScopedTzSet tz("UTC");
  // Timestamp for 2023-10-26 10:30:00 UTC.
  time_t specific_time = 1698316200;

  struct tm result_tm = {};
  struct tm* result_tm_ptr = localtime_r(&specific_time, &result_tm);
  ASSERT_NE(result_tm_ptr, nullptr)
      << "localtime_r returned nullptr for specific_time.";

  struct tm expected_tm = {};
  expected_tm.tm_sec = 0;
  expected_tm.tm_min = 30;
  expected_tm.tm_hour = 10;
  expected_tm.tm_mday = 26;   // 26th day of the month.
  expected_tm.tm_mon = 9;     // October (0-11).
  expected_tm.tm_year = 123;  // 2023 - 1900.
  expected_tm.tm_wday = 4;    // Thursday.
  expected_tm.tm_yday = 298;  // Day of the year (0-365).
  expected_tm.tm_isdst = 0;   // Daylight Saving Time is not in effect.

  ExpectTmEqual(*result_tm_ptr, expected_tm, "specific_time (2023-10-26)");
  EXPECT_EQ(result_tm_ptr->tm_gmtoff, 0);
  EXPECT_STREQ(result_tm_ptr->tm_zone, "UTC");
}

TEST(PosixLocaltimeRTests, HandlesNegativeStdTimeBeforeEpoch) {
  ScopedTzSet tz("America/New_York");
  // Timestamp for 1969-12-31 00:00:00 UTC
  time_t negative_time = -kSecondsInDay;  // -86400 seconds.

  struct tm result_tm = {};
  struct tm* result_tm_ptr = localtime_r(&negative_time, &result_tm);
  ASSERT_NE(result_tm_ptr, nullptr)
      << "localtime_r returned nullptr for negative_time.";

  struct tm expected_tm = {};
  expected_tm.tm_sec = 0;
  expected_tm.tm_min = 0;
  expected_tm.tm_hour = 19;
  expected_tm.tm_mday = 30;  // 30th day of the month.
  expected_tm.tm_mon = 11;   // December (0-11).
  expected_tm.tm_year = 69;  // 1969 - 1900.
  expected_tm.tm_wday = 2;   // Tuesday.
  // tm_yday for Dec 30, 1969 (non-leap year): 1969 had 365 days. Dec 30 is the
  // 364th day. So, 0-indexed tm_yday is 363.
  expected_tm.tm_yday = 363;  // Day of the year (0-365).
  expected_tm.tm_isdst = 0;   // Daylight Saving Time is not in effect.

  ExpectTmEqual(*result_tm_ptr, expected_tm, "negative_time (1969-12-30)");
  EXPECT_EQ(result_tm_ptr->tm_gmtoff, -5 * 3600);
  EXPECT_STREQ(result_tm_ptr->tm_zone, "EST");
}

TEST(PosixLocaltimeRTests, HandlesNegativeDstTimeBeforeEpoch) {
  ScopedTzSet tz("America/New_York");
  // Timestamp for 1969-06-30 00:00:00 UTC
  // Six months before December, 4 of which have 31 days.
  time_t negative_time = -(30 * 6 + 4) * kSecondsInDay;

  struct tm result_tm = {};
  struct tm* result_tm_ptr = localtime_r(&negative_time, &result_tm);
  ASSERT_NE(result_tm_ptr, nullptr)
      << "localtime_r returned nullptr for negative_time.";

  struct tm expected_tm = {};
  expected_tm.tm_sec = 0;
  expected_tm.tm_min = 0;
  expected_tm.tm_hour = 20;
  expected_tm.tm_mday = 30;  // 30th day of the month.
  expected_tm.tm_mon = 5;    // June (0-11).
  expected_tm.tm_year = 69;  // 1969 - 1900.
  expected_tm.tm_wday = 1;   // Monday.
  // tm_yday for Jun 30, 1969 (non-leap year): 1969 had 365 days. Jun 30 is the
  // 181st day. So, 0-indexed tm_yday is 180.
  expected_tm.tm_yday = 180;  // Day of the year (0-365).
  expected_tm.tm_isdst = 1;   // Daylight Saving Time is in effect.

  ExpectTmEqual(*result_tm_ptr, expected_tm, "negative_time (1969-06-30)");
  EXPECT_EQ(result_tm_ptr->tm_gmtoff, -4 * 3600);
  EXPECT_STREQ(result_tm_ptr->tm_zone, "EDT");
}

TEST(PosixLocaltimeRTests, ReturnsPointerToGivenBufferAndDoesNotOverwrite) {
  ScopedTzSet tz("America/New_York");
  time_t time1 = 0;              // Jan 1, 1970, 00:00:00 UTC.
  time_t time2 = kSecondsInDay;  // Jan 2, 1970, 00:00:00 UTC.

  struct tm result_tm1 = {};
  struct tm* tm_ptr_call1 = localtime_r(&time1, &result_tm1);
  EXPECT_EQ(tm_ptr_call1, &result_tm1)
      << "localtime_r(&result_tm1) did not return the given pointer.";
  struct tm tm_val_call1 = *tm_ptr_call1;

  struct tm result_tm2 = {};
  struct tm* tm_ptr_call2 = localtime_r(&time2, &result_tm2);
  EXPECT_EQ(tm_ptr_call2, &result_tm2)
      << "localtime_r(&result_tm2) did not return the given pointer.";

  // 1. Check that the pointers returned by both calls are the same.
  EXPECT_NE(tm_ptr_call1, tm_ptr_call2)
      << "localtime_r should return different pointers on subsequent calls.";

  // 2. Verify that the content of the first buffer was not overwritten.
  struct tm expected_tm_for_time1 = {};
  expected_tm_for_time1.tm_sec = 0;
  expected_tm_for_time1.tm_min = 0;
  expected_tm_for_time1.tm_hour = 19;
  expected_tm_for_time1.tm_mday = 31;   // Dec 31.
  expected_tm_for_time1.tm_mon = 11;    // December.
  expected_tm_for_time1.tm_year = 69;   // 1969 - 1900.
  expected_tm_for_time1.tm_wday = 3;    // Wednesday.
  expected_tm_for_time1.tm_yday = 364;  // Day 365 of year (0-indexed).
  expected_tm_for_time1.tm_isdst = 0;
  ExpectTmEqual(*tm_ptr_call1, expected_tm_for_time1,
                "buffer content after second call (time1)");
  EXPECT_EQ(tm_ptr_call1->tm_gmtoff, -5 * 3600);
  EXPECT_STREQ(tm_ptr_call1->tm_zone, "EST");

  // Verify that the second buffer has the correct data for time2.
  struct tm expected_tm_for_time2 = {};
  expected_tm_for_time2.tm_sec = 0;
  expected_tm_for_time2.tm_min = 0;
  expected_tm_for_time2.tm_hour = 19;
  expected_tm_for_time2.tm_mday = 1;   // Jan 1st.
  expected_tm_for_time2.tm_mon = 0;    // January.
  expected_tm_for_time2.tm_year = 70;  // 1970 - 1900.
  expected_tm_for_time2.tm_wday = 4;   // Friday.
  expected_tm_for_time2.tm_yday = 0;   // Day 1 of year (0-indexed).
  expected_tm_for_time2.tm_isdst = 0;
  ExpectTmEqual(*tm_ptr_call2, expected_tm_for_time2,
                "buffer content after second call (time2)");
  EXPECT_EQ(tm_ptr_call2->tm_gmtoff, -5 * 3600);
  EXPECT_STREQ(tm_ptr_call2->tm_zone, "EST");

  // 4. Verify that the original content (tm_val_call1, copied from the buffer
  // after the first call) is not different from the current content of the
  // returned buffer. This confirms that there is no overwrite. We compare a few
  // differing fields.
  bool is_overwritten = (tm_val_call1.tm_mday != tm_ptr_call1->tm_mday ||
                         tm_val_call1.tm_wday != tm_ptr_call1->tm_wday ||
                         tm_val_call1.tm_yday != tm_ptr_call1->tm_yday);
  EXPECT_FALSE(is_overwritten)
      << "The content of the given buffer should not be overwritten by the "
         "subsequent call to localtime_r.";

  // 5. For completeness, ensure tm_val_call1 (the copied data from the first
  // call) still matches the expected data for time1.
  ExpectTmEqual(tm_val_call1, expected_tm_for_time1,
                "copied data from first call (time1)");
  EXPECT_EQ(tm_val_call1.tm_gmtoff, -5 * 3600);
  EXPECT_STREQ(tm_val_call1.tm_zone, "EST");
}

TEST(PosixLocaltimeRTests, HandlesTimeTMaxValueOverflow) {
  ScopedTzSet tz("America/New_York");
  ASSERT_GE(sizeof(time_t), static_cast<size_t>(8))
      << "The size of time_t has to be at least 64 bit";
  time_t time_val_max = std::numeric_limits<time_t>::max();
  struct tm result_tm = {};
  struct tm* local_result_ptr = localtime_r(&time_val_max, &result_tm);

  // For 64-bit time_t, max_time_val is a very large positive number,
  // representing a year extremely far in the future (e.g., year ~2.9e11).
  // The corresponding tm_year (year - 1900) would vastly exceed INT_MAX
  // (typically ~2.1e9). Thus, localtime_r is expected to return nullptr.
  EXPECT_EQ(local_result_ptr, nullptr)
      << "localtime_r(&time_val_max) should return nullptr "
      << "due to expected overflow in struct tm.tm_year. time_val_max: "
      << time_val_max;
  EXPECT_EQ(errno, EOVERFLOW)
      << "localtime_r(&time_val_max) should set errno to EOVERFLOW "
      << "due to expected overflow in struct tm.tm_year. time_val_max: "
      << time_val_max << ", errno: " << errno << " (" << strerror(errno) << ")";
}

// Test localtime_r's behavior with time_t's high value.
// While technically not specified in POSIX. Various functions can not process
// values for tm_year that overflow in 32-bit. This tests that the value is not
// exceeded.
TEST(PosixLocaltimeRTests, HandlesTimeTHighValueOverflow) {
  ScopedTzSet tz("America/New_York");
#if !BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
  GTEST_SKIP() << "Non-hermetic builds fail this test.";
#endif
  ASSERT_GE(sizeof(time_t), static_cast<size_t>(8))
      << "The size of time_t has to be at least 64 bit";
  time_t time_val = 67767976233521999;  // Tue Dec 31 23:59:59  2147483647
  // Ensure that time_val will overflow the tm_year member variable.
  time_val += 1;
  struct tm result_tm = {};
  struct tm* local_result_ptr = localtime_r(&time_val, &result_tm);

  // Time_val is a number representing the year 2147483648. The corresponding
  // tm_year (year - 1900) would exceed INT_MAX. Thus, localtime_r is expected
  // to return nullptr.
  EXPECT_EQ(local_result_ptr, nullptr)
      << "localtime_r(&time_val) should return nullptr "
      << "due to expected overflow in struct tm.tm_year. time_t: " << time_val;
  EXPECT_EQ(errno, EOVERFLOW)
      << "localtime_r(&time_val_max) should set errno to EOVERFLOW "
      << "due to expected overflow in struct tm.tm_year. time_t: " << time_val
      << ", errno: " << errno << " (" << strerror(errno) << ")";
}

// Test localtime_r's behavior with time_t's minimum value.
// POSIX: "If the specified time cannot be converted... (e.g., if the time_t
// object has a value that is too small)... gmtime_r() shall return a null
// pointer."
TEST(PosixLocaltimeRTests, HandlesTimeTMinValueOverflow) {
  ScopedTzSet tz("America/New_York");
  ASSERT_GE(sizeof(time_t), static_cast<size_t>(8))
      << "The size of time_t has to be at least 64 bit";
  time_t time_val_min = std::numeric_limits<time_t>::min();
  struct tm result_tm = {};
  struct tm* local_result_ptr = localtime_r(&time_val_min, &result_tm);

  // For 64-bit time_t, min_time_val is a very large negative number,
  // representing a year extremely far in the past (e.g., year ~ -2.9e11).
  // The corresponding tm_year (year - 1900) would be a very large negative
  // number, likely less than INT_MIN or otherwise unrepresentable. Thus,
  // gmtime_r is expected to return nullptr.
  EXPECT_EQ(local_result_ptr, nullptr)
      << "localtime_r(&time_val_min) should return nullptr "
      << "due to expected underflow in struct tm.tm_year. Min "
         "time_t: "
      << time_val_min;
  EXPECT_EQ(errno, EOVERFLOW)
      << "localtime_r(&time_val_min) should set errno to EOVERFLOW "
      << "due to expected underflow in struct tm.tm_year. time_t: "
      << time_val_min << ", errno: " << errno << " (" << strerror(errno) << ")";
}

// Test gmtime's behavior with time_t's low value.
// While technically not specified in POSIX. Various functions can not process
// values for tm_year that overflow in 32-bit. This tests that the value is not
// exceeded.
TEST(PosixLocaltimeRTests, HandlesTimeTLowValueOverflow) {
  ScopedTzSet tz("America/New_York");
#if !BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
  GTEST_SKIP() << "Non-hermetic builds fail this test.";
#endif
  ASSERT_GE(sizeof(time_t), static_cast<size_t>(8))
      << "The size of time_t has to be at least 64 bit";
  time_t time_val = -67768040609712422;  // Thu Jan  1 00:00:00 -2147481748
  // Ensure that time_val will overflow the tm_year member variable.
  time_val -= 1;
  struct tm result_tm = {};
  struct tm* local_result_ptr = localtime_r(&time_val, &result_tm);

  // Time_val is a number representing the year -2147481749. The corresponding
  // tm_year (year - 1900) would be less than INT_MIN. Thus, localtime_r is
  // expected to return nullptr.
  EXPECT_EQ(local_result_ptr, nullptr)
      << "localtime_r(&time_t_min) should return nullptr due to expected "
         "underflow in struct tm.tm_year. time_t: "
      << time_val;
  EXPECT_EQ(errno, EOVERFLOW)
      << "localtime_r(&time_val_min) should set errno to EOVERFLOW "
      << "due to expected underflow in struct tm.tm_year. time_t: " << time_val
      << ", errno: " << errno << " (" << strerror(errno) << ")";
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
