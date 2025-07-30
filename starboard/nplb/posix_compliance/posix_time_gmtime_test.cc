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

namespace starboard {
namespace nplb {
namespace {

// gmtime converts a time_t value to a struct tm in UTC.
// According to POSIX:
// - It returns a pointer to a statically allocated struct tm.
// - Subsequent calls to gmtime() or localtime() may overwrite this struct.
// - If the time_t value cannot be converted (e.g., too large/small for struct
// tm), it returns nullptr.

TEST(PosixGmtimeTests, HandlesEpochTime) {
  time_t epoch_time = 0;  // January 1, 1970, 00:00:00 UTC.

  struct tm* result_tm_ptr = gmtime(&epoch_time);
  ASSERT_NE(result_tm_ptr, nullptr)
      << "gmtime returned nullptr for epoch_time.";

  struct tm expected_tm {};
  expected_tm.tm_sec = 0;
  expected_tm.tm_min = 0;
  expected_tm.tm_hour = 0;
  expected_tm.tm_mday = 1;   // 1st day of the month.
  expected_tm.tm_mon = 0;    // January (0-11).
  expected_tm.tm_year = 70;  // Years since 1900 (1970 - 1900).
  expected_tm.tm_wday = 4;   // Thursday (0=Sunday, 1=Monday, ..., 6=Saturday).
  expected_tm.tm_yday = 0;   // Day of the year (0-365). Jan 1st is day 0.
  expected_tm.tm_isdst = 0;  // Daylight Saving Time is not in effect for UTC.

  ExpectTmEqual(*result_tm_ptr, expected_tm, "epoch_time");
  EXPECT_EQ(result_tm_ptr->tm_gmtoff, 0);
  EXPECT_TRUE(strcmp(result_tm_ptr->tm_zone, "UTC") == 0 ||
              strcmp(result_tm_ptr->tm_zone, "GMT") == 0);
}

TEST(PosixGmtimeTests, HandlesSpecificKnownPositiveTime) {
  // Timestamp for 2023-10-26 10:30:00 UTC.
  // Generated using: date -u -d "2023-10-26 10:30:00 UTC" +%s  (gives
  // 1698316200)
  time_t specific_time = 1698316200;

  struct tm* result_tm_ptr = gmtime(&specific_time);
  ASSERT_NE(result_tm_ptr, nullptr)
      << "gmtime returned nullptr for specific_time.";

  struct tm expected_tm {};
  expected_tm.tm_sec = 0;
  expected_tm.tm_min = 30;
  expected_tm.tm_hour = 10;
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
  expected_tm.tm_isdst = 0;

  ExpectTmEqual(*result_tm_ptr, expected_tm, "specific_time (2023-10-26)");
  EXPECT_EQ(result_tm_ptr->tm_gmtoff, 0);
  EXPECT_TRUE(strcmp(result_tm_ptr->tm_zone, "UTC") == 0 ||
              strcmp(result_tm_ptr->tm_zone, "GMT") == 0);
}

TEST(PosixGmtimeTests, HandlesNegativeStdTimeBeforeEpoch) {
  // Timestamp for 1969-12-31 00:00:00 UTC
  time_t negative_time = -kSecondsInDay;  // -86400 seconds.

  struct tm* result_tm_ptr = gmtime(&negative_time);
  ASSERT_NE(result_tm_ptr, nullptr)
      << "gmtime returned nullptr for negative_time.";

  struct tm expected_tm{};
  expected_tm.tm_sec = 0;
  expected_tm.tm_min = 0;
  expected_tm.tm_hour = 0;
  expected_tm.tm_mday = 31;  // December 31st.
  expected_tm.tm_mon = 11;   // December (0-11).
  expected_tm.tm_year = 69;  // 1969 - 1900.
  expected_tm.tm_wday = 3;   // Wednesday.
  // tm_yday for Dec 31, 1969 (non-leap year): 1969 had 365 days. Dec 31 is the
  // 365th day. So, 0-indexed tm_yday is 364.
  expected_tm.tm_yday = 364;
  expected_tm.tm_isdst = 0;

  ExpectTmEqual(*result_tm_ptr, expected_tm, "negative_time (1969-12-31)");
  EXPECT_EQ(result_tm_ptr->tm_gmtoff, 0);
  EXPECT_TRUE(strcmp(result_tm_ptr->tm_zone, "UTC") == 0 ||
              strcmp(result_tm_ptr->tm_zone, "GMT") == 0);
}

TEST(PosixGmtimeTests, HandlesNegativeDstTimeBeforeEpoch) {
  // Timestamp for 1969-06-30 00:00:00 UTC
  // Six months and one day before December, 4 of which have 31 days.
  time_t negative_time = -(30 * 6 + 5) * kSecondsInDay;

  struct tm* result_tm_ptr = gmtime(&negative_time);
  ASSERT_NE(result_tm_ptr, nullptr)
      << "gmtime returned nullptr for negative_time.";

  struct tm expected_tm{};
  expected_tm.tm_sec = 0;
  expected_tm.tm_min = 0;
  expected_tm.tm_hour = 0;
  expected_tm.tm_mday = 30;  // June 30th.
  expected_tm.tm_mon = 5;    // June (0-11).
  expected_tm.tm_year = 69;  // 1969 - 1900.
  expected_tm.tm_wday = 1;   // Monday.
  // tm_yday for Jun 30, 1969 (non-leap year): 1969 had 365 days. Jun 30 is the
  // 181st day. So, 0-indexed tm_yday is 180.
  expected_tm.tm_yday = 180;  // Day of the year (0-365).
  expected_tm.tm_isdst = 0;   // Daylight Saving Time is not in effect for UTC.

  ExpectTmEqual(*result_tm_ptr, expected_tm, "negative_time (1969-06-30)");
  EXPECT_EQ(result_tm_ptr->tm_gmtoff, 0);
  EXPECT_TRUE(strcmp(result_tm_ptr->tm_zone, "UTC") == 0 ||
              strcmp(result_tm_ptr->tm_zone, "GMT") == 0);
}

TEST(PosixGmtimeTests, ReturnsPointerToStaticBufferAndOverwrites) {
  time_t time1 = 0;              // Jan 1, 1970, 00:00:00 UTC.
  time_t time2 = kSecondsInDay;  // Jan 2, 1970, 00:00:00 UTC.

  struct tm* tm_ptr_call1 = gmtime(&time1);
  ASSERT_NE(tm_ptr_call1, nullptr) << "gmtime(&time1) returned nullptr.";
  struct tm tm_val_call1 = *tm_ptr_call1;

  struct tm* tm_ptr_call2 = gmtime(&time2);
  ASSERT_NE(tm_ptr_call2, nullptr) << "gmtime(&time2) returned nullptr.";

  // 1. Check that the pointers returned by both calls are the same.
  EXPECT_EQ(tm_ptr_call1, tm_ptr_call2)
      << "gmtime should return pointers to the same static buffer on "
         "subsequent calls.";

  // 2. Verify that the content of the static buffer (pointed to by
  // tm_ptr_call1/tm_ptr_call2) now corresponds to the data for time2.
  struct tm expected_tm_for_time2 {};
  expected_tm_for_time2.tm_sec = 0;
  expected_tm_for_time2.tm_min = 0;
  expected_tm_for_time2.tm_hour = 0;
  expected_tm_for_time2.tm_mday = 2;   // Jan 2nd.
  expected_tm_for_time2.tm_mon = 0;    // January.
  expected_tm_for_time2.tm_year = 70;  // 1970 - 1900.
  expected_tm_for_time2.tm_wday = 5;   // Friday.
  expected_tm_for_time2.tm_yday = 1;   // Day 1 of year (0-indexed).
  expected_tm_for_time2.tm_isdst = 0;
  ExpectTmEqual(*tm_ptr_call2, expected_tm_for_time2,
                "static buffer content after second call (time2)");
  EXPECT_EQ(tm_ptr_call2->tm_gmtoff, 0);
  EXPECT_TRUE(strcmp(tm_ptr_call2->tm_zone, "UTC") == 0 ||
              strcmp(tm_ptr_call2->tm_zone, "GMT") == 0);

  // 3. Verify that the original content (tm_val_call1, copied from the buffer
  // after the first call) is different from the current content of the static
  // buffer (which now holds data for time2). This confirms the overwrite. We
  // compare a few differing fields.
  bool is_overwritten = (tm_val_call1.tm_mday != tm_ptr_call2->tm_mday ||
                         tm_val_call1.tm_wday != tm_ptr_call2->tm_wday ||
                         tm_val_call1.tm_yday != tm_ptr_call2->tm_yday);
  EXPECT_TRUE(is_overwritten)
      << "The content of the static buffer should be overwritten by the "
         "subsequent call to gmtime.";

  // 4. For completeness, ensure tm_val_call1 (the copied data from the first
  // call) still matches the expected data for time1.
  struct tm expected_tm_for_time1 {};
  expected_tm_for_time1.tm_sec = 0;
  expected_tm_for_time1.tm_min = 0;
  expected_tm_for_time1.tm_hour = 0;
  expected_tm_for_time1.tm_mday = 1;   // Jan 1.
  expected_tm_for_time1.tm_mon = 0;    // January.
  expected_tm_for_time1.tm_year = 70;  // 1970 - 1900.
  expected_tm_for_time1.tm_wday = 4;   // Thursday.
  expected_tm_for_time1.tm_yday = 0;   // Day 1 of year (0-indexed).
  expected_tm_for_time1.tm_isdst = 0;
  ExpectTmEqual(tm_val_call1, expected_tm_for_time1,
                "copied data from first call (time1)");
  EXPECT_EQ(tm_val_call1.tm_gmtoff, 0);
  EXPECT_TRUE(strcmp(tm_val_call1.tm_zone, "UTC") == 0 ||
              strcmp(tm_val_call1.tm_zone, "GMT") == 0);
}

TEST(PosixGmtimeTests, HandlesTimeTMaxValueOverflow) {
  ASSERT_GE(sizeof(time_t), static_cast<size_t>(8))
      << "The size of time_t has to be at least 64 bit";
  time_t time_val_max = std::numeric_limits<time_t>::max();
  struct tm* gmt_result_ptr = gmtime(&time_val_max);

  // For 64-bit time_t, max_time_val is a very large positive number,
  // representing a year extremely far in the future (e.g., year ~2.9e11).
  // The corresponding tm_year (year - 1900) would vastly exceed INT_MAX
  // (typically ~2.1e9). Thus, gmtime is expected to return nullptr.
  EXPECT_EQ(gmt_result_ptr, nullptr)
      << "gmtime(&time_val_max) should return nullptr "
      << "due to expected overflow in struct tm.tm_year. time_val_max: "
      << time_val_max;
  EXPECT_EQ(errno, EOVERFLOW)
      << "gmtime(&time_val_max) should set errno to EOVERFLOW "
      << "due to expected overflow in struct tm.tm_year. time_val_max: "
      << time_val_max << ", errno: " << errno << " (" << strerror(errno) << ")";
}

// Test gmtime's behavior with time_t's high value.
// While technically not specified in POSIX. Various functions can not process
// values for tm_year that overflow in 32-bit. This tests that the value is not
// exceeded.
TEST(PosixGmtimeTests, HandlesTimeTHighValueOverflow) {
#if !BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
  GTEST_SKIP() << "Non-hermetic builds fail this test.";
#endif
  ASSERT_GE(sizeof(time_t), static_cast<size_t>(8))
      << "The size of time_t has to be at least 64 bit";
  time_t time_val = 67767976233521999;  // Tue Dec 31 23:59:59  2147483647
  // Ensure that time_val will overflow the tm_year member variable.
  time_val += 1;
  struct tm* gmt_result_ptr = gmtime(&time_val);

  // Time_val is a number representing the year 2147483648. The corresponding
  // tm_year (year - 1900) would exceed INT_MAX. Thus, gmtime is expected to
  // return nullptr.
  EXPECT_EQ(gmt_result_ptr, nullptr)
      << "gmtime(&time_val) should return nullptr "
      << "due to expected overflow in struct tm.tm_year. time_t: " << time_val;
  EXPECT_EQ(errno, EOVERFLOW)
      << "gmtime(&time_val_max) should set errno to EOVERFLOW "
      << "due to expected overflow in struct tm.tm_year. time_t: " << time_val
      << ", errno: " << errno << " (" << strerror(errno) << ")";
}

// Test gmtime's behavior with time_t's minimum value.
// POSIX: "If the specified time cannot be converted... (e.g., if the time_t
// object has a value that is too small)... gmtime() shall return a null
// pointer."
TEST(PosixGmtimeTests, HandlesTimeTMinValueOverflow) {
  ASSERT_GE(sizeof(time_t), static_cast<size_t>(8))
      << "The size of time_t has to be at least 64 bit";
  time_t time_val_min = std::numeric_limits<time_t>::min();
  struct tm* gmt_result_ptr = gmtime(&time_val_min);

  // For 64-bit time_t, min_time_val is a very large negative number,
  // representing a year extremely far in the past (e.g., year ~ -2.9e11).
  // The corresponding tm_year (year - 1900) would be a very large negative
  // number, likely less than INT_MIN or otherwise unrepresentable. Thus,
  // gmtime is expected to return nullptr.
  EXPECT_EQ(gmt_result_ptr, nullptr)
      << "gmtime(&time_val_min) should return nullptr "
      << "due to expected underflow in struct tm.tm_year. Min "
         "time_t: "
      << time_val_min;
  EXPECT_EQ(errno, EOVERFLOW)
      << "gmtime(&time_val_min) should set errno to EOVERFLOW "
      << "due to expected underflow in struct tm.tm_year. time_t: "
      << time_val_min << ", errno: " << errno << " (" << strerror(errno) << ")";
}

// Test gmtime's behavior with time_t's low value.
// While technically not specified in POSIX. Various functions can not process
// values for tm_year that overflow in 32-bit. This tests that the value is not
// exceeded.
TEST(PosixGmtimeTests, HandlesTimeTLowValueOverflow) {
#if !BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
  GTEST_SKIP() << "Non-hermetic builds fail this test.";
#endif
  ASSERT_GE(sizeof(time_t), static_cast<size_t>(8))
      << "The size of time_t has to be at least 64 bit";
  time_t time_val = -67768040609712422;  // Thu Jan  1 00:00:00 -2147481748
  // Ensure that time_val will overflow the tm_year member variable.
  time_val -= 1;
  struct tm* gmt_result_ptr = gmtime(&time_val);

  // Time_val is a number representing the year -2147481749. The corresponding
  // tm_year (year - 1900) would be less than INT_MIN. Thus, gmtime is expected
  // to return nullptr.
  EXPECT_EQ(gmt_result_ptr, nullptr)
      << "gmtime(&time_t_min) should return nullptr due to expected "
         "underflow in struct tm.tm_year. time_t: "
      << time_val;
  EXPECT_EQ(errno, EOVERFLOW)
      << "gmtime(&time_val_min) should set errno to EOVERFLOW "
      << "due to expected underflow in struct tm.tm_year. time_t: " << time_val
      << ", errno: " << errno << " (" << strerror(errno) << ")";
}

TEST(PosixGmtimeTests, Gmtime) {
  // Generated with: date -u -d "2024-07-31 23:32:59 UTC" +%s.
  time_t timer = 1722468779;  // Wed 2024-07-31 23:32:59 UTC.
  struct tm result;
  memset(&result, 0, sizeof(result));

  struct tm* gmt_result_ptr = gmtime(&timer);
  ASSERT_NE(gmt_result_ptr, nullptr);
  EXPECT_EQ(gmt_result_ptr->tm_year, 2024 - 1900);  // Year since 1900.
  EXPECT_EQ(gmt_result_ptr->tm_mon, 7 - 1);         // Zero-indexed.
  EXPECT_EQ(gmt_result_ptr->tm_mday, 31);
  EXPECT_EQ(gmt_result_ptr->tm_hour, 23);
  EXPECT_EQ(gmt_result_ptr->tm_min, 32);
  EXPECT_EQ(gmt_result_ptr->tm_sec, 59);
  EXPECT_EQ(gmt_result_ptr->tm_wday, 3);  // Wednesday, 0==Sunday.
  EXPECT_EQ(gmt_result_ptr->tm_yday,
            212);  // Zero-indexed; 2024 is a leap year.
  EXPECT_LE(gmt_result_ptr->tm_isdst,
            0);  // <=0; GMT/UTC never has DST (even in July).
  EXPECT_EQ(gmt_result_ptr->tm_gmtoff, 0);
  EXPECT_TRUE(strcmp(gmt_result_ptr->tm_zone, "UTC") == 0 ||
              strcmp(gmt_result_ptr->tm_zone, "GMT") == 0);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
