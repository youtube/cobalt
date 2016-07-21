// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/time_formatting.h"

#include "base/i18n/rtl.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

#if defined(__LB_SHELL__)

// Some legacy platforms' local time calculations always use the current
// Daylight Savings Time status when exploding/unexploding time ticks, no matter
// what the time to explode is. On Starboard platforms, we use ICU instead as it
// is platform-independent and considerably more correct.
#define MAYBE_TimeFormatTimeOfDayDefault12h \
    DISABLED_TimeFormatTimeOfDayDefault12h
#define MAYBE_TimeFormatTimeOfDayDefault24h \
    DISABLED_TimeFormatTimeOfDayDefault24h
#define MAYBE_TimeFormatTimeOfDayJP \
    DISABLED_TimeFormatTimeOfDayJP
#define MAYBE_TimeFormatDateUS \
    DISABLED_TimeFormatDateUS
#define MAYBE_TimeFormatDateGB \
    DISABLED_TimeFormatDateGB

#else

#define MAYBE_TimeFormatTimeOfDayDefault12h TimeFormatTimeOfDayDefault12h
#define MAYBE_TimeFormatTimeOfDayDefault24h TimeFormatTimeOfDayDefault24h
#define MAYBE_TimeFormatTimeOfDayJP TimeFormatTimeOfDayJP
#define MAYBE_TimeFormatDateUS TimeFormatDateUS
#define MAYBE_TimeFormatDateGB TimeFormatDateGB

#endif

const Time::Exploded kTestDateTimeExploded = {
  2011, 4, 6, 30, // Sat, Apr 30, 2011
  15, 42, 7, 0    // 15:42:07.000
};

TEST(TimeFormattingTest, MAYBE_TimeFormatTimeOfDayDefault12h) {
  // Test for a locale defaulted to 12h clock.
  // As an instance, we use third_party/icu/source/data/locales/en.txt.
  i18n::SetICUDefaultLocale("en_US");

  Time time(Time::FromLocalExploded(kTestDateTimeExploded));
  string16 clock24h(ASCIIToUTF16("15:42"));
  string16 clock12h_pm(ASCIIToUTF16("3:42 PM"));
  string16 clock12h(ASCIIToUTF16("3:42"));

  // The default is 12h clock.
  EXPECT_EQ(clock12h_pm, TimeFormatTimeOfDay(time));
  EXPECT_EQ(k12HourClock, GetHourClockType());
  // k{Keep,Drop}AmPm should not affect for 24h clock.
  EXPECT_EQ(clock24h,
            TimeFormatTimeOfDayWithHourClockType(time,
                                                 k24HourClock,
                                                 kKeepAmPm));
  EXPECT_EQ(clock24h,
            TimeFormatTimeOfDayWithHourClockType(time,
                                                 k24HourClock,
                                                 kDropAmPm));
  // k{Keep,Drop}AmPm affects for 12h clock.
  EXPECT_EQ(clock12h_pm,
            TimeFormatTimeOfDayWithHourClockType(time,
                                                 k12HourClock,
                                                 kKeepAmPm));
  EXPECT_EQ(clock12h,
            TimeFormatTimeOfDayWithHourClockType(time,
                                                 k12HourClock,
                                                 kDropAmPm));
}

TEST(TimeFormattingTest, MAYBE_TimeFormatTimeOfDayDefault24h) {
  // Test for a locale defaulted to 24h clock.
  // As an instance, we use third_party/icu/source/data/locales/en_GB.txt.
  i18n::SetICUDefaultLocale("en_GB");

  Time time(Time::FromLocalExploded(kTestDateTimeExploded));
  string16 clock24h(ASCIIToUTF16("15:42"));
  string16 clock12h_pm(ASCIIToUTF16("3:42 PM"));
  string16 clock12h(ASCIIToUTF16("3:42"));

  // The default is 24h clock.
  EXPECT_EQ(clock24h, TimeFormatTimeOfDay(time));
  EXPECT_EQ(k24HourClock, GetHourClockType());
  // k{Keep,Drop}AmPm should not affect for 24h clock.
  EXPECT_EQ(clock24h,
            TimeFormatTimeOfDayWithHourClockType(time,
                                                 k24HourClock,
                                                 kKeepAmPm));
  EXPECT_EQ(clock24h,
            TimeFormatTimeOfDayWithHourClockType(time,
                                                 k24HourClock,
                                                 kDropAmPm));
  // k{Keep,Drop}AmPm affects for 12h clock.
  EXPECT_EQ(clock12h_pm,
            TimeFormatTimeOfDayWithHourClockType(time,
                                                 k12HourClock,
                                                 kKeepAmPm));
  EXPECT_EQ(clock12h,
            TimeFormatTimeOfDayWithHourClockType(time,
                                                 k12HourClock,
                                                 kDropAmPm));
}

TEST(TimeFormattingTest, MAYBE_TimeFormatTimeOfDayJP) {
  // Test for a locale that uses different mark than "AM" and "PM".
  // As an instance, we use third_party/icu/source/data/locales/ja.txt.
  i18n::SetICUDefaultLocale("ja_JP");

  Time time(Time::FromLocalExploded(kTestDateTimeExploded));
  string16 clock24h(ASCIIToUTF16("15:42"));
  string16 clock12h_pm(WideToUTF16(L"\x5348\x5f8c" L"3:42"));
  string16 clock12h(ASCIIToUTF16("3:42"));

  // The default is 24h clock.
  EXPECT_EQ(clock24h, TimeFormatTimeOfDay(time));
  EXPECT_EQ(k24HourClock, GetHourClockType());
  // k{Keep,Drop}AmPm should not affect for 24h clock.
  EXPECT_EQ(clock24h,
            TimeFormatTimeOfDayWithHourClockType(time,
                                                 k24HourClock,
                                                 kKeepAmPm));
  EXPECT_EQ(clock24h,
            TimeFormatTimeOfDayWithHourClockType(time,
                                                 k24HourClock,
                                                 kDropAmPm));
  // k{Keep,Drop}AmPm affects for 12h clock.
  EXPECT_EQ(clock12h_pm,
            TimeFormatTimeOfDayWithHourClockType(time,
                                                 k12HourClock,
                                                 kKeepAmPm));
  EXPECT_EQ(clock12h,
            TimeFormatTimeOfDayWithHourClockType(time,
                                                 k12HourClock,
                                                 kDropAmPm));
}

TEST(TimeFormattingTest, MAYBE_TimeFormatDateUS) {
  // See third_party/icu/source/data/locales/en.txt.
  // The date patterns are "EEEE, MMMM d, y", "MMM d, y", and "M/d/yy".
  i18n::SetICUDefaultLocale("en_US");

  Time time(Time::FromLocalExploded(kTestDateTimeExploded));

  EXPECT_EQ(ASCIIToUTF16("Apr 30, 2011"), TimeFormatShortDate(time));
  EXPECT_EQ(ASCIIToUTF16("4/30/11"), TimeFormatShortDateNumeric(time));
  EXPECT_EQ(ASCIIToUTF16("4/30/11 3:42:07 PM"),
            TimeFormatShortDateAndTime(time));
  EXPECT_EQ(ASCIIToUTF16("Saturday, April 30, 2011 3:42:07 PM"),
            TimeFormatFriendlyDateAndTime(time));
  EXPECT_EQ(ASCIIToUTF16("Saturday, April 30, 2011"),
            TimeFormatFriendlyDate(time));
}

TEST(TimeFormattingTest, MAYBE_TimeFormatDateGB) {
  // See third_party/icu/source/data/locales/en_GB.txt.
  // The date patterns are "EEEE, d MMMM y", "d MMM y", and "dd/MM/yyyy".
  i18n::SetICUDefaultLocale("en_GB");

  Time time(Time::FromLocalExploded(kTestDateTimeExploded));

  EXPECT_EQ(ASCIIToUTF16("30 Apr 2011"), TimeFormatShortDate(time));
  EXPECT_EQ(ASCIIToUTF16("30/04/2011"), TimeFormatShortDateNumeric(time));
  EXPECT_EQ(ASCIIToUTF16("30/04/2011 15:42:07"),
            TimeFormatShortDateAndTime(time));
  EXPECT_EQ(ASCIIToUTF16("Saturday, 30 April 2011 15:42:07"),
            TimeFormatFriendlyDateAndTime(time));
  EXPECT_EQ(ASCIIToUTF16("Saturday, 30 April 2011"),
            TimeFormatFriendlyDate(time));
}

}  // namespace
}  // namespace base
