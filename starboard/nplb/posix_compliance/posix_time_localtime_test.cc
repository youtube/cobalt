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

#include <errno.h>
#include <gtest/gtest.h>
#include <climits>
#include <cstring>
#include <ctime>
#include <limits>

#include "starboard/nplb/posix_compliance/posix_time_helper.h"
#include "starboard/nplb/posix_compliance/scoped_tz_environment.h"

namespace starboard {
namespace nplb {
namespace {

// localtime converts a time_t value to a struct tm in the local timezone.
// According to POSIX:
// - It returns a pointer to a statically allocated struct tm.
// - Subsequent calls to localtime() or gmtime() may overwrite this struct.
// - If the time_t value cannot be converted (e.g., too large/small for struct
// tm), it returns nullptr.

TEST(PosixLocaltimeTests, HandlesEpochTime) {
  ScopedTzEnvironment tz("America/New_York");
  time_t epoch_time = 0;  // January 1, 1970, 00:00:00 UTC.

  struct tm* result_tm_ptr = localtime(&epoch_time);
  ASSERT_NE(result_tm_ptr, nullptr)
      << "localtime returned nullptr for epoch_time.";

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

TEST(PosixLocaltimeTests, HandlesSpecificKnownPositiveTimeInDST) {
  ScopedTzEnvironment tz("America/New_York");
  // Timestamp for 2023-10-26 10:30:00 UTC which is Thu Oct 26 06:30:00 AM EDT
  // 2023. Generated using: date -u -d "2023-10-26 10:30:00 UTC" +%s  (gives
  // 1698316200)
  time_t specific_time = 1698316200;

  struct tm* result_tm_ptr = localtime(&specific_time);
  ASSERT_NE(result_tm_ptr, nullptr)
      << "localtime returned nullptr for specific_time.";

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

TEST(PosixLocaltimeTests, HandlesSpecificKnownPositiveTimeNotInDST) {
  ScopedTzEnvironment tz("America/New_York");
  // Timestamp for 2023-12-25 10:30:00 UTC which is Mon Dec 25 05:30:00 AM EST
  // 2023. Generated using: date -u -d "2023-12-25 10:30:00 UTC" +%s  (gives
  // 1703500200)
  time_t specific_time = 1703500200;

  struct tm* result_tm_ptr = localtime(&specific_time);
  ASSERT_NE(result_tm_ptr, nullptr)
      << "localtime returned nullptr for specific_time.";

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

TEST(PosixLocaltimeTests, HandlesSpecificKnownPositiveTimeInDifferentTimezone) {
  ScopedTzEnvironment tz("Europe/Paris");
  // Timestamp for 2023-10-26 10:30:00 UTC which is Thu Oct 26 12:30:00 PM CEST
  // 2023. Generated using: date -u -d "2023-10-26 10:30:00 UTC" +%s  (gives
  // 1698316200)
  time_t specific_time = 1698316200;

  struct tm* result_tm_ptr = localtime(&specific_time);
  ASSERT_NE(result_tm_ptr, nullptr)
      << "localtime returned nullptr for specific_time.";

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
  EXPECT_TRUE(strcmp(result_tm_ptr->tm_zone, "CEST") == 0 ||
              strcmp(result_tm_ptr->tm_zone, "GMT+2") == 0);
}

TEST(PosixLocaltimeTests, HandlesSpecificKnownPositiveTimeInUTC) {
  ScopedTzEnvironment tz("UTC");
  // Timestamp for 2023-10-26 10:30:00 UTC.
  time_t specific_time = 1698316200;

  struct tm* result_tm_ptr = localtime(&specific_time);
  ASSERT_NE(result_tm_ptr, nullptr)
      << "localtime returned nullptr for specific_time.";

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

TEST(PosixLocaltimeTests, HandlesNegativeStdTimeBeforeEpoch) {
  ScopedTzEnvironment tz("America/New_York");
  // Timestamp for 1969-12-31 00:00:00 UTC
  time_t negative_time = -kSecondsInDay;  // -86400 seconds.

  struct tm* result_tm_ptr = localtime(&negative_time);
  ASSERT_NE(result_tm_ptr, nullptr)
      << "localtime returned nullptr for negative_time.";

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

TEST(PosixLocaltimeTests, HandlesNegativeDstTimeBeforeEpoch) {
  ScopedTzEnvironment tz("America/New_York");
  // Timestamp for 1969-06-30 00:00:00 UTC
  // Six months before December, 4 of which have 31 days.
  time_t negative_time = -(30 * 6 + 4) * kSecondsInDay;

  struct tm* result_tm_ptr = localtime(&negative_time);
  ASSERT_NE(result_tm_ptr, nullptr)
      << "localtime returned nullptr for negative_time.";

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

TEST(PosixLocaltimeTests, ReturnsPointerToStaticBufferAndOverwrites) {
  ScopedTzEnvironment tz("America/New_York");
  time_t time1 = 0;              // Jan 1, 1970, 00:00:00 UTC.
  time_t time2 = kSecondsInDay;  // Jan 2, 1970, 00:00:00 UTC.

  struct tm* tm_ptr_call1 = localtime(&time1);
  ASSERT_NE(tm_ptr_call1, nullptr) << "localtime(&time1) returned nullptr.";
  struct tm tm_val_call1 = *tm_ptr_call1;

  struct tm* tm_ptr_call2 = localtime(&time2);
  ASSERT_NE(tm_ptr_call2, nullptr) << "localtime(&time2) returned nullptr.";

  // 1. Check that the pointers returned by both calls are the same.
  EXPECT_EQ(tm_ptr_call1, tm_ptr_call2)
      << "localtime should return pointers to the same static buffer on "
         "subsequent calls.";

  // 2. Verify that the content of the static buffer now corresponds to the data
  // for time2.
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
                "static buffer content after second call (time2)");
  EXPECT_EQ(tm_ptr_call2->tm_gmtoff, -5 * 3600);
  EXPECT_STREQ(tm_ptr_call2->tm_zone, "EST");

  // 3. Verify that the original content (tm_val_call1, copied from the buffer
  // after the first call) is different from the current content of the static
  // buffer (which now holds data for time2). This confirms the overwrite. We
  // compare a few differing fields.
  bool is_overwritten = (tm_val_call1.tm_mday != tm_ptr_call2->tm_mday ||
                         tm_val_call1.tm_wday != tm_ptr_call2->tm_wday ||
                         tm_val_call1.tm_yday != tm_ptr_call2->tm_yday);
  EXPECT_TRUE(is_overwritten)
      << "The content of the static buffer should be overwritten by the "
         "subsequent call to localtime.";

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
  ExpectTmEqual(tm_val_call1, expected_tm_for_time1,
                "copied data from first call (time1)");
  EXPECT_EQ(tm_val_call1.tm_gmtoff, -5 * 3600);
  EXPECT_STREQ(tm_val_call1.tm_zone, "EST");
}

TEST(PosixLocaltimeTests, HandlesTimeTMaxValueOverflow) {
  ScopedTzEnvironment tz("America/New_York");
  ASSERT_GE(sizeof(time_t), static_cast<size_t>(8))
      << "The size of time_t has to be at least 64 bit";
  time_t time_val_max = std::numeric_limits<time_t>::max();
  struct tm* local_result_ptr = localtime(&time_val_max);

  // For 64-bit time_t, max_time_val is a very large positive number,
  // representing a year extremely far in the future (e.g., year ~2.9e11).
  // The corresponding tm_year (year - 1900) would vastly exceed INT_MAX
  // (typically ~2.1e9). Thus, localtime is expected to return nullptr.
  EXPECT_EQ(local_result_ptr, nullptr)
      << "localtime(&time_val_max) should return nullptr "
      << "due to expected overflow in struct tm.tm_year. time_val_max: "
      << time_val_max;
  EXPECT_EQ(errno, EOVERFLOW)
      << "localtime(&time_val_max) should set errno to EOVERFLOW "
      << "due to expected overflow in struct tm.tm_year. time_val_max: "
      << time_val_max << ", errno: " << errno << " (" << strerror(errno) << ")";
}

// Test localtime's behavior with time_t's high value.
// While technically not specified in POSIX. Various functions can not process
// values for tm_year that overflow in 32-bit. This tests that the value is not
// exceeded.
TEST(PosixLocaltimeTests, HandlesTimeTHighValueOverflow) {
  ScopedTzEnvironment tz("America/New_York");
#if !BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
  GTEST_SKIP() << "Non-hermetic builds fail this test.";
#endif
  ASSERT_GE(sizeof(time_t), static_cast<size_t>(8))
      << "The size of time_t has to be at least 64 bit";
  time_t time_val = 67767976233521999;  // Tue Dec 31 23:59:59  2147483647
  // Ensure that time_val will overflow the tm_year member variable.
  time_val += 1;
  struct tm* local_result_ptr = localtime(&time_val);

  // Time_val is a number representing the year 2147483648. The corresponding
  // tm_year (year - 1900) would exceed INT_MAX. Thus, localtime is expected to
  // return nullptr.
  EXPECT_EQ(local_result_ptr, nullptr)
      << "localtime(&time_val) should return nullptr "
      << "due to expected overflow in struct tm.tm_year. time_t: " << time_val;
  EXPECT_EQ(errno, EOVERFLOW)
      << "localtime(&time_val_max) should set errno to EOVERFLOW "
      << "due to expected overflow in struct tm.tm_year. time_t: " << time_val
      << ", errno: " << errno << " (" << strerror(errno) << ")";
}

// Test localtime's behavior with time_t's minimum value.
// POSIX: "If the specified time cannot be converted... (e.g., if the time_t
// object has a value that is too small)... gmtime() shall return a null
// pointer."
TEST(PosixLocaltimeTests, HandlesTimeTMinValueOverflow) {
  ScopedTzEnvironment tz("America/New_York");
  ASSERT_GE(sizeof(time_t), static_cast<size_t>(8))
      << "The size of time_t has to be at least 64 bit";
  time_t time_val_min = std::numeric_limits<time_t>::min();
  struct tm* local_result_ptr = localtime(&time_val_min);

  // For 64-bit time_t, min_time_val is a very large negative number,
  // representing a year extremely far in the past (e.g., year ~ -2.9e11).
  // The corresponding tm_year (year - 1900) would be a very large negative
  // number, likely less than INT_MIN or otherwise unrepresentable. Thus,
  // gmtime is expected to return nullptr.
  EXPECT_EQ(local_result_ptr, nullptr)
      << "localtime(&time_val_min) should return nullptr "
      << "due to expected underflow in struct tm.tm_year. Min "
         "time_t: "
      << time_val_min;
  EXPECT_EQ(errno, EOVERFLOW)
      << "localtime(&time_val_min) should set errno to EOVERFLOW "
      << "due to expected underflow in struct tm.tm_year. time_t: "
      << time_val_min << ", errno: " << errno << " (" << strerror(errno) << ")";
}

// Test gmtime's behavior with time_t's low value.
// While technically not specified in POSIX. Various functions can not process
// values for tm_year that overflow in 32-bit. This tests that the value is not
// exceeded.
TEST(PosixLocaltimeTests, HandlesTimeTLowValueOverflow) {
  ScopedTzEnvironment tz("America/New_York");
#if !BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
  GTEST_SKIP() << "Non-hermetic builds fail this test.";
#endif
  ASSERT_GE(sizeof(time_t), static_cast<size_t>(8))
      << "The size of time_t has to be at least 64 bit";
  time_t time_val = -67768040609712422;  // Thu Jan  1 00:00:00 -2147481748
  // Ensure that time_val will overflow the tm_year member variable.
  time_val -= 1;
  struct tm* local_result_ptr = localtime(&time_val);

  // Time_val is a number representing the year -2147481749. The corresponding
  // tm_year (year - 1900) would be less than INT_MIN. Thus, localtime is
  // expected to return nullptr.
  EXPECT_EQ(local_result_ptr, nullptr)
      << "localtime(&time_t_min) should return nullptr due to expected "
         "underflow in struct tm.tm_year. time_t: "
      << time_val;
  EXPECT_EQ(errno, EOVERFLOW)
      << "localtime(&time_val_min) should set errno to EOVERFLOW "
      << "due to expected underflow in struct tm.tm_year. time_t: " << time_val
      << ", errno: " << errno << " (" << strerror(errno) << ")";
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
