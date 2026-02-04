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

#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

const int kBufferSize = 256;

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

    original_locale_ = setlocale(LC_TIME, nullptr);
    ASSERT_NE(original_locale_, nullptr);
    setlocale(LC_TIME, "C");
    memset(buffer_, 0, kBufferSize);

    locale_ = newlocale(LC_TIME_MASK, "C", nullptr);
    ASSERT_NE(locale_, nullptr);

    // Ensure TZ is set before running the tests.
    tzset();
    errno = 0;
  }

  void TearDown() override {
    setlocale(LC_TIME, original_locale_);
    if (locale_) {
      freelocale(locale_);
    }
  }

  struct tm tm_;
  char buffer_[kBufferSize];
  const char* original_locale_;
  locale_t locale_;
};

TEST_F(PosixStrftimeTest, StandardSpecifiers) {
  const struct {
    const char* format;
    const char* expected;
  } kTestCases[] = {
      {"%Y-%y", "2023-23"},
      {"%B-%b-%m", "October-Oct-10"},
      {"%A-%a-%d-%j", "Thursday-Thu-26-299"},
      {"%H-%I-%M-%S", "10-10-30-00"},
      {"%p", "AM"},
      {"%F", "2023-10-26"},
      {"%T", "10:30:00"},
      {"%D", "10/26/23"},
      {"%r", "10:30:00 AM"},
      {"%R", "10:30"},
      {"%x", "10/26/23"},
      {"%X", "10:30:00"},
      {"%C", "20"},
  };

  for (const auto& test_case : kTestCases) {
    size_t result = strftime(buffer_, kBufferSize, test_case.format, &tm_);
    EXPECT_GT(result, 0U) << "Format: " << test_case.format;
    EXPECT_STREQ(test_case.expected, buffer_) << "Format: " << test_case.format;
  }
}

TEST_F(PosixStrftimeTest, StandardSpecifiersL) {
  const struct {
    const char* format;
    const char* expected;
  } kTestCases[] = {
      {"%Y-%y", "2023-23"},
      {"%B-%b-%m", "October-Oct-10"},
      {"%A-%a-%d-%j", "Thursday-Thu-26-299"},
      {"%H-%I-%M-%S", "10-10-30-00"},
      {"%p", "AM"},
      {"%F %T", "2023-10-26 10:30:00"},
      {"%x", "10/26/23"},
      {"%X", "10:30:00"},
  };

  for (const auto& test_case : kTestCases) {
    size_t result =
        strftime_l(buffer_, kBufferSize, test_case.format, &tm_, locale_);
    EXPECT_GT(result, 0U) << "Format: " << test_case.format;
    EXPECT_STREQ(test_case.expected, buffer_) << "Format: " << test_case.format;
  }
}

TEST_F(PosixStrftimeTest, SpecialCharacterSpecifiers) {
  const struct {
    const char* format;
    const char* expected;
    size_t expected_len;
  } kTestCases[] = {
      {"A\nB\tC", "A\nB\tC", 5},
      {"%%", "%", 1},
      {"%n%t", "\n\t", 2},
  };

  for (const auto& test_case : kTestCases) {
    size_t result = strftime(buffer_, kBufferSize, test_case.format, &tm_);
    EXPECT_EQ(result, test_case.expected_len) << "Format: " << test_case.format;
    EXPECT_STREQ(test_case.expected, buffer_) << "Format: " << test_case.format;
  }
}

TEST_F(PosixStrftimeTest, TimezoneAbbreviationUTC) {
  char* old_tz = getenv("TZ");
  setenv("TZ", "UTC", 1);
  tzset();

  memset(&tm_, 0, sizeof(tm_));
  tm_.tm_isdst = 0;

  memset(buffer_, 0, kBufferSize);
  size_t result = strftime(buffer_, kBufferSize, "%Z", &tm_);

  EXPECT_GT(result, 0U);

  const std::vector<std::string> kExpectedTimezones = {"UTC", "GMT", "Z"};
  bool is_valid = false;
  for (const auto& tz : kExpectedTimezones) {
    if (strcmp(buffer_, tz.c_str()) == 0) {
      is_valid = true;
      break;
    }
  }

  EXPECT_TRUE(is_valid) << "Expected UTC, GMT, or Z, but got: " << buffer_;
  if (old_tz) {
    setenv("TZ", old_tz, 1);
  } else {
    unsetenv("TZ");
  }
  tzset();
}

TEST_F(PosixStrftimeTest, PaddedAndNumericSpecifiers) {
  tm_.tm_mday = 5;
  strftime(buffer_, kBufferSize, "%e", &tm_);
  EXPECT_STREQ(" 5", buffer_);

  tm_.tm_wday = 0;  // Sunday
  strftime(buffer_, kBufferSize, "%u-%w", &tm_);
  EXPECT_STREQ("7-0", buffer_);

  tm_.tm_wday = 1;  // Monday
  strftime(buffer_, kBufferSize, "%u-%w", &tm_);
  EXPECT_STREQ("1-1", buffer_);
}

TEST_F(PosixStrftimeTest, AMPMSpecifier) {
  strftime(buffer_, kBufferSize, "%p", &tm_);
  EXPECT_STREQ("AM", buffer_);

  tm_.tm_hour = 14;
  strftime(buffer_, kBufferSize, "%p", &tm_);
  EXPECT_STREQ("PM", buffer_);
}

TEST_F(PosixStrftimeTest, AMPMSpecifierL) {
  strftime_l(buffer_, kBufferSize, "%p", &tm_, locale_);
  EXPECT_STREQ("AM", buffer_);

  tm_.tm_hour = 14;
  strftime_l(buffer_, kBufferSize, "%p", &tm_, locale_);
  EXPECT_STREQ("PM", buffer_);
}

TEST_F(PosixStrftimeTest, ISOWeekStandard) {
  tm_.tm_year = 2023 - 1900;
  tm_.tm_mon = 9;  // October
  tm_.tm_mday = 26;
  tm_.tm_wday = 4;
  tm_.tm_yday = 298;

  size_t result = strftime(buffer_, kBufferSize, "%G-W%V-%g", &tm_);
  EXPECT_GT(result, 0U);
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

TEST_F(PosixStrftimeTest, BufferHandling) {
  const char* format = "%Y-%m-%d";
  size_t result = strftime(buffer_, 10, format, &tm_);
  EXPECT_EQ(result, 0U);

  result = strftime(buffer_, 11, format, &tm_);
  EXPECT_EQ(result, 10U);
  EXPECT_STREQ("2023-10-26", buffer_);

  result = strftime(buffer_, 0, format, &tm_);
  EXPECT_EQ(result, 0U);
}

TEST_F(PosixStrftimeTest, BufferHandlingL) {
  const char* format = "%Y-%m-%d";  // Result: "2023-10-26" (10 chars)

  size_t result = strftime_l(buffer_, 10, format, &tm_, locale_);
  EXPECT_EQ(result, 0U);

  result = strftime_l(buffer_, 11, format, &tm_, locale_);
  EXPECT_EQ(result, 10U);
  EXPECT_STREQ("2023-10-26", buffer_);

  result = strftime_l(buffer_, 0, format, &tm_, locale_);
  EXPECT_EQ(result, 0U);
}

TEST_F(PosixStrftimeTest, LargeYear) {
  tm_.tm_year = 10005 - 1900;
  strftime(buffer_, kBufferSize, "%Y", &tm_);

  // Accept either "+10005" (glibc/ISO8601) OR "10005" (BSD/musl)
  // We explicitly check for both valid implementations.
  bool match =
      (strcmp(buffer_, "+10005") == 0) || (strcmp(buffer_, "10005") == 0);

  EXPECT_TRUE(match) << "Expected +10005 or 10005, but got: " << buffer_;
}

TEST_F(PosixStrftimeTest, LiteralPercentL) {
  size_t result = strftime_l(buffer_, kBufferSize, "%%", &tm_, locale_);
  EXPECT_EQ(result, 1U);
  EXPECT_STREQ("%", buffer_);
}

}  // namespace
}  // namespace nplb
