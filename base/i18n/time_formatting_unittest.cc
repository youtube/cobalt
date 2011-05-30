// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/time_formatting.h"

#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "unicode/locid.h"

namespace {

void SetICUDefaultLocale(const std::string& locale_string) {
  icu::Locale locale(locale_string.c_str());
  UErrorCode error_code = U_ZERO_ERROR;
  icu::Locale::setDefault(locale, error_code);
  EXPECT_TRUE(U_SUCCESS(error_code));
}

const base::Time::Exploded kTestDateTimeExploded = {
  2011, 4, 6, 30, // Sat, Apr 30, 2011
  15, 42, 7, 0    // 15:42:07.000
};

}  // namespace

TEST(TimeFormattingTest, TimeFormatTimeOfDayDefault12h) {
  // Test for a locale defaulted to 12h clock.
  // As an instance, we use third_party/icu/source/data/locales/en.txt.
  SetICUDefaultLocale("en_US");

  base::Time time(base::Time::FromLocalExploded(kTestDateTimeExploded));
  string16 clock24h(ASCIIToUTF16("15:42"));
  string16 clock12h_pm(ASCIIToUTF16("3:42 PM"));
  string16 clock12h(ASCIIToUTF16("3:42"));

  // The default is 12h clock.
  EXPECT_EQ(clock12h_pm,
            base::TimeFormatTimeOfDay(time));
  EXPECT_EQ(base::k12HourClock, base::GetHourClockType());
  // k{Keep,Drop}AmPm should not affect for 24h clock.
  EXPECT_EQ(clock24h,
            base::TimeFormatTimeOfDayWithHourClockType(time,
                                                       base::k24HourClock,
                                                       base::kKeepAmPm));
  EXPECT_EQ(clock24h,
            base::TimeFormatTimeOfDayWithHourClockType(time,
                                                       base::k24HourClock,
                                                       base::kDropAmPm));
  // k{Keep,Drop}AmPm affects for 12h clock.
  EXPECT_EQ(clock12h_pm,
            base::TimeFormatTimeOfDayWithHourClockType(time,
                                                       base::k12HourClock,
                                                       base::kKeepAmPm));
  EXPECT_EQ(clock12h,
            base::TimeFormatTimeOfDayWithHourClockType(time,
                                                       base::k12HourClock,
                                                       base::kDropAmPm));
}


TEST(TimeFormattingTest, TimeFormatTimeOfDayDefault24h) {
  // Test for a locale defaulted to 24h clock.
  // As an instance, we use third_party/icu/source/data/locales/en_GB.txt.
  SetICUDefaultLocale("en_GB");

  base::Time time(base::Time::FromLocalExploded(kTestDateTimeExploded));
  string16 clock24h(ASCIIToUTF16("15:42"));
  string16 clock12h_pm(ASCIIToUTF16("3:42 PM"));
  string16 clock12h(ASCIIToUTF16("3:42"));

  // The default is 24h clock.
  EXPECT_EQ(clock24h,
            base::TimeFormatTimeOfDay(time));
  EXPECT_EQ(base::k24HourClock, base::GetHourClockType());
  // k{Keep,Drop}AmPm should not affect for 24h clock.
  EXPECT_EQ(clock24h,
            base::TimeFormatTimeOfDayWithHourClockType(time,
                                                       base::k24HourClock,
                                                       base::kKeepAmPm));
  EXPECT_EQ(clock24h,
            base::TimeFormatTimeOfDayWithHourClockType(time,
                                                       base::k24HourClock,
                                                       base::kDropAmPm));
  // k{Keep,Drop}AmPm affects for 12h clock.
  EXPECT_EQ(clock12h_pm,
            base::TimeFormatTimeOfDayWithHourClockType(time,
                                                       base::k12HourClock,
                                                       base::kKeepAmPm));
  EXPECT_EQ(clock12h,
            base::TimeFormatTimeOfDayWithHourClockType(time,
                                                       base::k12HourClock,
                                                       base::kDropAmPm));
}

TEST(TimeFormattingTest, TimeFormatTimeOfDayJP) {
  // Test for a locale that uses different mark than "AM" and "PM".
  // As an instance, we use third_party/icu/source/data/locales/ja.txt.
  SetICUDefaultLocale("ja_JP");

  base::Time time(base::Time::FromLocalExploded(kTestDateTimeExploded));
  string16 clock24h(ASCIIToUTF16("15:42"));
  string16 clock12h_pm(WideToUTF16(L"\x5348\x5f8c"L"3:42"));
  string16 clock12h(ASCIIToUTF16("3:42"));

  // The default is 24h clock.
  EXPECT_EQ(clock24h,
            base::TimeFormatTimeOfDay(time));
  EXPECT_EQ(base::k24HourClock, base::GetHourClockType());
  // k{Keep,Drop}AmPm should not affect for 24h clock.
  EXPECT_EQ(clock24h,
            base::TimeFormatTimeOfDayWithHourClockType(time,
                                                       base::k24HourClock,
                                                       base::kKeepAmPm));
  EXPECT_EQ(clock24h,
            base::TimeFormatTimeOfDayWithHourClockType(time,
                                                       base::k24HourClock,
                                                       base::kDropAmPm));
  // k{Keep,Drop}AmPm affects for 12h clock.
  EXPECT_EQ(clock12h_pm,
            base::TimeFormatTimeOfDayWithHourClockType(time,
                                                       base::k12HourClock,
                                                       base::kKeepAmPm));
  EXPECT_EQ(clock12h,
            base::TimeFormatTimeOfDayWithHourClockType(time,
                                                       base::k12HourClock,
                                                       base::kDropAmPm));
}

TEST(TimeFormattingTest, TimeFormatDateUS) {
  // See third_party/icu/source/data/locales/en.txt.
  // The date patterns are "EEEE, MMMM d, y", "MMM d, y", and "M/d/yy".
  SetICUDefaultLocale("en_US");

  base::Time time(base::Time::FromLocalExploded(kTestDateTimeExploded));

  EXPECT_EQ(ASCIIToUTF16("Apr 30, 2011"),
            base::TimeFormatShortDate(time));
  EXPECT_EQ(ASCIIToUTF16("4/30/11"),
            base::TimeFormatShortDateNumeric(time));
  EXPECT_EQ(ASCIIToUTF16("4/30/11 3:42:07 PM"),
            base::TimeFormatShortDateAndTime(time));
  EXPECT_EQ(ASCIIToUTF16("Saturday, April 30, 2011 3:42:07 PM"),
            base::TimeFormatFriendlyDateAndTime(time));
  EXPECT_EQ(ASCIIToUTF16("Saturday, April 30, 2011"),
            base::TimeFormatFriendlyDate(time));
}

TEST(TimeFormattingTest, TimeFormatDateGB) {
  // See third_party/icu/source/data/locales/en_GB.txt.
  // The date patterns are "EEEE, d MMMM y", "d MMM y", and "dd/MM/yyyy".
  SetICUDefaultLocale("en_GB");

  base::Time time(base::Time::FromLocalExploded(kTestDateTimeExploded));

  EXPECT_EQ(ASCIIToUTF16("30 Apr 2011"),
            base::TimeFormatShortDate(time));
  EXPECT_EQ(ASCIIToUTF16("30/04/2011"),
            base::TimeFormatShortDateNumeric(time));
  EXPECT_EQ(ASCIIToUTF16("30/04/2011 15:42:07"),
            base::TimeFormatShortDateAndTime(time));
  EXPECT_EQ(ASCIIToUTF16("Saturday, 30 April 2011 15:42:07"),
            base::TimeFormatFriendlyDateAndTime(time));
  EXPECT_EQ(ASCIIToUTF16("Saturday, 30 April 2011"),
            base::TimeFormatFriendlyDate(time));
}
