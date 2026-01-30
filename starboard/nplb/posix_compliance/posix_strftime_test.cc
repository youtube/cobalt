// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

const int kBufferSize = 256;

// Test fixture for strftime tests.
class PosixStrftimeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Set a known time: 2023-10-26 10:30:00 AM, Thursday
    tm_.tm_year = 2023 - 1900;  // Year since 1900
    tm_.tm_mon = 10 - 1;        // 0-indexed month
    tm_.tm_mday = 26;
    tm_.tm_hour = 10;
    tm_.tm_min = 30;
    tm_.tm_sec = 0;
    tm_.tm_wday = 4;  // Thursday
    tm_.tm_yday = 298;
    tm_.tm_isdst = 0;

    // Save the current locale
    original_locale_ = setlocale(LC_TIME, nullptr);
    ASSERT_NE(original_locale_, nullptr);
    memset(buffer_, 0, kBufferSize);

    tzset();
  }

  void TearDown() override {
    // Restore the original locale
    setlocale(LC_TIME, original_locale_);
  }

  struct tm tm_;
  char buffer_[kBufferSize];
  const char* original_locale_;
};

TEST_F(PosixStrftimeTest, YearSpecifiers) {
  size_t result = strftime(buffer_, kBufferSize, "%Y-%y", &tm_);
  EXPECT_GT(result, 0);
  EXPECT_STREQ("2023-23", buffer_);
}

TEST_F(PosixStrftimeTest, MonthSpecifiers) {
  ASSERT_NE(setlocale(LC_TIME, "en_US.UTF-8"), nullptr);
  size_t result = strftime(buffer_, kBufferSize, "%B-%b-%m", &tm_);
  EXPECT_GT(result, 0);
  EXPECT_STREQ("October-Oct-10", buffer_);
}

TEST_F(PosixStrftimeTest, DaySpecifiers) {
  ASSERT_NE(setlocale(LC_TIME, "en_US.UTF-8"), nullptr);
  size_t result = strftime(buffer_, kBufferSize, "%A-%a-%d-%j", &tm_);
  EXPECT_GT(result, 0);
  EXPECT_STREQ("Thursday-Thu-26-299", buffer_);
}

TEST_F(PosixStrftimeTest, TimeSpecifiers) {
  size_t result = strftime(buffer_, kBufferSize, "%H-%I-%M-%S", &tm_);
  EXPECT_GT(result, 0);
  EXPECT_STREQ("10-10-30-00", buffer_);
}

TEST_F(PosixStrftimeTest, AMPMSpecifier) {
  ASSERT_NE(setlocale(LC_TIME, "en_US.UTF-8"), nullptr);
  size_t result = strftime(buffer_, kBufferSize, "%p", &tm_);
  EXPECT_GT(result, 0);
  EXPECT_STREQ("AM", buffer_);

  tm_.tm_hour = 14;
  result = strftime(buffer_, kBufferSize, "%p", &tm_);
  EXPECT_GT(result, 0);
  EXPECT_STREQ("PM", buffer_);
}

TEST_F(PosixStrftimeTest, CombinedDateAndTime) {
  ASSERT_NE(setlocale(LC_TIME, "en_US.UTF-8"), nullptr);

  // %F: Equivalent to %Y-%m-%d
  // %T: Equivalent to %H:%M:%S
  // %D: Equivalent to %m/%d/%y
  // %r: 12-hour clock time (e.g., 10:30:00 AM)
  // %R: 24-hour HH:MM
  // %e: Day of month, space-padded ( '26' or ' 5')

  size_t result =
      strftime(buffer_, kBufferSize, "%F %T | %D | %R | %r | %e", &tm_);

  EXPECT_GT(result, 0);
  EXPECT_STREQ("2023-10-26 10:30:00 | 10/26/23 | 10:30 | 10:30:00 AM | 26",
               buffer_);

  // Verify %e space-padding for single-digit days
  tm_.tm_mday = 5;
  strftime(buffer_, kBufferSize, "%e", &tm_);
  EXPECT_STREQ(" 5", buffer_);
}

TEST_F(PosixStrftimeTest, ISOWeekStandard) {
  // Oct 26, 2023 is a Thursday in the middle of the year.
  // It should clearly be ISO Year 2023, Week 43.
  tm_.tm_year = 2023 - 1900;
  tm_.tm_mon = 9;  // October
  tm_.tm_mday = 26;
  tm_.tm_wday = 4;
  tm_.tm_yday = 298;

  size_t result = strftime(buffer_, kBufferSize, "%G-W%V-%g", &tm_);
  EXPECT_GT(result, 0);
  EXPECT_STREQ("2023-W43-23", buffer_);
}

TEST_F(PosixStrftimeTest, ISOWeekBackwardWrap) {
  // Jan 1, 2021 (Friday).
  // This day is technically Week 53 of 2020.
  tm_.tm_year = 2021 - 1900;
  tm_.tm_mon = 0;  // Jan
  tm_.tm_mday = 1;
  tm_.tm_wday = 5;  // Friday
  tm_.tm_yday = 0;

  // %G should decrement to 2020
  strftime(buffer_, kBufferSize, "%G-W%V", &tm_);
  EXPECT_STREQ("2020-W53", buffer_);
}

TEST_F(PosixStrftimeTest, ISOWeekForwardWrap) {
  // Dec 31, 2019 (Tuesday).
  // This day is technically Week 01 of 2020.
  tm_.tm_year = 2019 - 1900;
  tm_.tm_mon = 11;  // Dec
  tm_.tm_mday = 31;
  tm_.tm_wday = 2;  // Tuesday
  tm_.tm_yday = 364;

  // %G should increment to 2020
  strftime(buffer_, kBufferSize, "%G-W%V", &tm_);
  EXPECT_STREQ("2020-W01", buffer_);
}

TEST_F(PosixStrftimeTest, WeekdayNumericSunday) {
  tm_.tm_wday = 0;  // Sunday

  // %u: 1-7 (Monday-Sunday)
  // %w: 0-6 (Sunday-Saturday)
  strftime(buffer_, kBufferSize, "%u-%w", &tm_);
  EXPECT_STREQ("7-0", buffer_);
}

TEST_F(PosixStrftimeTest, WeekdayNumericMonday) {
  tm_.tm_wday = 1;  // Monday

  strftime(buffer_, kBufferSize, "%u-%w", &tm_);
  EXPECT_STREQ("1-1", buffer_);
}

TEST_F(PosixStrftimeTest, SpecialCharacters) {
  // Testing %n (newline) and %t (tab)
  size_t result = strftime(buffer_, kBufferSize, "A%nB%tC", &tm_);
  EXPECT_EQ(result, 5);  // 'A', '\n', 'B', '\t', 'C'
  EXPECT_STREQ("A\nB\tC", buffer_);
}

TEST_F(PosixStrftimeTest, LocaleSpecificDateTime) {
  ASSERT_NE(setlocale(LC_TIME, "en_US.UTF-8"), nullptr);
  size_t result = strftime(buffer_, kBufferSize, "%c", &tm_);
  EXPECT_GT(result, 0);
  // SB_LOG(INFO) << result;
  EXPECT_STREQ("Thu, Oct 26, 2023, 10:30:00 AM UTC", buffer_);
}

TEST_F(PosixStrftimeTest, LocaleSpecificDate) {
  ASSERT_NE(setlocale(LC_TIME, "en_US.UTF-8"), nullptr);
  size_t result = strftime(buffer_, kBufferSize, "%x", &tm_);
  EXPECT_GT(result, 0);
  EXPECT_STREQ("10/26/23", buffer_);
}

TEST_F(PosixStrftimeTest, LocaleSpecificTime) {
  ASSERT_NE(setlocale(LC_TIME, "en_US.UTF-8"), nullptr);
  size_t result = strftime(buffer_, kBufferSize, "%X", &tm_);
  EXPECT_GT(result, 0);
  EXPECT_STREQ("10:30:00 AM", buffer_);
}

TEST_F(PosixStrftimeTest, LiteralPercent) {
  size_t result = strftime(buffer_, kBufferSize, "%%", &tm_);
  EXPECT_EQ(result, 1);
  EXPECT_STREQ("%", buffer_);
}

TEST_F(PosixStrftimeTest, BufferTooSmall) {
  const char* format = "%Y-%m-%d";  // Result: "2023-10-26" (10 chars)
  size_t result = strftime(buffer_, 10, format, &tm_);
  EXPECT_EQ(result, 0);
}

TEST_F(PosixStrftimeTest, BufferExactSize) {
  const char* format = "%Y-%m-%d";  // Result: "2023-10-26" (10 chars)
  // Buffer needs space for the string + null terminator.
  size_t result = strftime(buffer_, 11, format, &tm_);
  EXPECT_EQ(result, 10);
  EXPECT_STREQ("2023-10-26", buffer_);
}

TEST_F(PosixStrftimeTest, ZeroSizedBuffer) {
  size_t result = strftime(buffer_, 0, "%Y", &tm_);
  EXPECT_EQ(result, 0);
}

TEST_F(PosixStrftimeTest, ISOWeekAndCentury) {
  // Test %C (Century)
  size_t result = strftime(buffer_, kBufferSize, "%C", &tm_);
  EXPECT_EQ(result, 2);
  EXPECT_STREQ("20", buffer_);

  // Test %e (Space-padded day)
  tm_.tm_mday = 5;
  strftime(buffer_, kBufferSize, "%e", &tm_);
  EXPECT_STREQ(" 5", buffer_);  // Note the leading space
}

TEST_F(PosixStrftimeTest, LargeYear) {
  tm_.tm_year = 10005 - 1900;
  strftime(buffer_, kBufferSize, "%Y", &tm_);
  EXPECT_STREQ("+10005", buffer_);  // Hits your 'val >= 10000' branch
}

TEST_F(PosixStrftimeTest, WhitespaceSpecifiers) {
  strftime(buffer_, kBufferSize, "%n%t", &tm_);
  EXPECT_STREQ("\n\t", buffer_);
}

// Test fixture for strftime_l tests.
class PosixStrftimeLTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Set a known time: 2023-10-26 10:30:00 AM, Thursday
    tm_.tm_year = 2023 - 1900;  // Year since 1900
    tm_.tm_mon = 10 - 1;        // 0-indexed month
    tm_.tm_mday = 26;
    tm_.tm_hour = 10;
    tm_.tm_min = 30;
    tm_.tm_sec = 0;
    tm_.tm_wday = 4;  // Thursday
    tm_.tm_yday = 298;
    tm_.tm_isdst = 0;

    locale_ = newlocale(LC_TIME_MASK, "en_US.UTF-8", nullptr);
    ASSERT_NE(locale_, nullptr);
  }

  void TearDown() override {
    if (locale_) {
      freelocale(locale_);
    }
  }

  struct tm tm_;
  char buffer_[kBufferSize];
  locale_t locale_;
};

TEST_F(PosixStrftimeLTest, YearSpecifiers) {
  size_t result = strftime_l(buffer_, kBufferSize, "%Y-%y", &tm_, locale_);
  EXPECT_GT(result, 0);
  EXPECT_STREQ("2023-23", buffer_);
}

TEST_F(PosixStrftimeLTest, MonthSpecifiers) {
  size_t result = strftime_l(buffer_, kBufferSize, "%B-%b-%m", &tm_, locale_);
  EXPECT_GT(result, 0);
  EXPECT_STREQ("October-Oct-10", buffer_);
}

TEST_F(PosixStrftimeLTest, DaySpecifiers) {
  size_t result =
      strftime_l(buffer_, kBufferSize, "%A-%a-%d-%j", &tm_, locale_);
  EXPECT_GT(result, 0);
  EXPECT_STREQ("Thursday-Thu-26-299", buffer_);
}

TEST_F(PosixStrftimeLTest, TimeSpecifiers) {
  size_t result =
      strftime_l(buffer_, kBufferSize, "%H-%I-%M-%S", &tm_, locale_);
  EXPECT_GT(result, 0);
  EXPECT_STREQ("10-10-30-00", buffer_);
}

TEST_F(PosixStrftimeLTest, AMPMSpecifier) {
  size_t result = strftime_l(buffer_, kBufferSize, "%p", &tm_, locale_);
  EXPECT_GT(result, 0);
  EXPECT_STREQ("AM", buffer_);

  tm_.tm_hour = 14;
  result = strftime_l(buffer_, kBufferSize, "%p", &tm_, locale_);
  EXPECT_GT(result, 0);
  EXPECT_STREQ("PM", buffer_);
}

TEST_F(PosixStrftimeLTest, CombinedDateAndTime) {
  size_t result = strftime_l(buffer_, kBufferSize, "%F %T", &tm_, locale_);
  EXPECT_GT(result, 0);
  EXPECT_STREQ("2023-10-26 10:30:00", buffer_);
}

TEST_F(PosixStrftimeLTest, LocaleSpecificDateTime) {
  char* old_tz = getenv("TZ");
  setenv("TZ", "EST5EDT", 1);
  tzset();
  size_t result = strftime_l(buffer_, kBufferSize, "%c", &tm_, locale_);
  EXPECT_GT(result, 0);
  EXPECT_STREQ("Thu, Oct 26, 2023, 10:30:00 AM EST", buffer_);
  if (old_tz) {
    setenv("TZ", old_tz, 1);
  } else {
    unsetenv("TZ");
  }
  tzset();
}

TEST_F(PosixStrftimeLTest, LocaleSpecificDate) {
  size_t result = strftime_l(buffer_, kBufferSize, "%x", &tm_, locale_);
  EXPECT_GT(result, 0);
  EXPECT_STREQ("10/26/23", buffer_);
}

TEST_F(PosixStrftimeLTest, LocaleSpecificTime) {
  size_t result = strftime_l(buffer_, kBufferSize, "%X", &tm_, locale_);
  EXPECT_GT(result, 0);
  EXPECT_STREQ("10:30:00 AM", buffer_);
}

TEST_F(PosixStrftimeLTest, LiteralPercent) {
  size_t result = strftime_l(buffer_, kBufferSize, "%%", &tm_, locale_);
  EXPECT_EQ(result, 1);
  EXPECT_STREQ("%", buffer_);
}

TEST_F(PosixStrftimeLTest, BufferTooSmall) {
  const char* format = "%Y-%m-%d";  // Result: "2023-10-26" (10 chars)
  size_t result = strftime_l(buffer_, 10, format, &tm_, locale_);
  EXPECT_EQ(result, 0);
}

TEST_F(PosixStrftimeLTest, BufferExactSize) {
  const char* format = "%Y-%m-%d";  // Result: "2023-10-26" (10 chars)
  // Buffer needs space for the string + null terminator.
  size_t result = strftime_l(buffer_, 11, format, &tm_, locale_);
  EXPECT_EQ(result, 10);
  EXPECT_STREQ("2023-10-26", buffer_);
}

TEST_F(PosixStrftimeLTest, ZeroSizedBuffer) {
  size_t result = strftime_l(buffer_, 0, "%Y", &tm_, locale_);
  EXPECT_EQ(result, 0);
}

}  // namespace
}  // namespace nplb
