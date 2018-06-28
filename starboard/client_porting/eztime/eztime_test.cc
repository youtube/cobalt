// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/client_porting/eztime/eztime.h"
#include "starboard/client_porting/eztime/test_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace client_porting {
namespace eztime {
namespace {

TEST(EzTimeTFromSbTime, IsTransitive) {
  SbTime sb_time = EzTimeTToSbTime(kTestTimePositive);
  EzTimeT ez_time = EzTimeTFromSbTime(sb_time);
  EXPECT_EQ(kTestTimePositive, ez_time);

  sb_time = EzTimeTToSbTime(kTestTimeWindowsNegative);
  ez_time = EzTimeTFromSbTime(sb_time);
  EXPECT_EQ(kTestTimeWindowsNegative, ez_time);

  sb_time = EzTimeTToSbTime(kTestTimePosixZero) - 1;
  ez_time = EzTimeTFromSbTime(sb_time);
  EXPECT_EQ(kTestTimePosixZero - 1, ez_time);

  sb_time = EzTimeTToSbTime(kTestTimeWindowsZero) - 1;
  ez_time = EzTimeTFromSbTime(sb_time);
  EXPECT_EQ(kTestTimeWindowsZero - 1, ez_time);

  sb_time = EzTimeTToSbTime(kTestTimePosixZero) + 1;
  ez_time = EzTimeTFromSbTime(sb_time);
  EXPECT_EQ(kTestTimePosixZero, ez_time);

  sb_time = EzTimeTToSbTime(kTestTimeWindowsZero) + 1;
  ez_time = EzTimeTFromSbTime(sb_time);
  EXPECT_EQ(kTestTimeWindowsZero, ez_time);
}

TEST(EzTimeValueFromSbTime, IsTransitive) {
  EzTimeValue time_value = EzTimeTToEzTimeValue(kTestTimePositive);
  SbTime sb_time = EzTimeValueToSbTime(&time_value);
  EzTimeValue time_value2 = EzTimeValueFromSbTime(sb_time);
  EXPECT_EQ(time_value.tv_sec, time_value2.tv_sec);
  EXPECT_EQ(time_value.tv_usec, time_value2.tv_usec);

  time_value = EzTimeTToEzTimeValue(kTestTimeWindowsNegative);
  sb_time = EzTimeValueToSbTime(&time_value);
  time_value2 = EzTimeValueFromSbTime(sb_time);
  EXPECT_EQ(time_value.tv_sec, time_value2.tv_sec);
  EXPECT_EQ(time_value.tv_usec, time_value2.tv_usec);
}

TEST(EzTimeTGetNowTest, IsKindOfSane) {
  EzTimeT ez_time = EzTimeTGetNow(NULL);
  SbTime sb_time = SbTimeGetNow();

  // They should be within a second of each other.
  EXPECT_GT(kSbTimeSecond, sb_time - EzTimeTToSbTime(ez_time));
  EXPECT_GT(kSbTimeSecond, EzTimeTToSbTime(ez_time) - sb_time);

  // Now should be after the time I wrote this test.
  EXPECT_LE(kTestTimeWritten, ez_time);

  // And it should be before 5 years after I wrote this test, at least for a
  // while.
  EXPECT_GE(kTestTimePastWritten, ez_time);

  EzTimeT ez_time2 = 0;
  EzTimeT ez_time3 = EzTimeTGetNow(&ez_time2);
  EXPECT_EQ(ez_time2, ez_time3);
}

TEST(EzTimeValueGetNowTest, IsKindOfSane) {
  EzTimeValue time_value = {0};
  EXPECT_EQ(0, EzTimeValueGetNow(&time_value, NULL));
  SbTime sb_time = SbTimeGetNow();

  // They should be within a second of each other.
  EXPECT_GT(kSbTimeSecond, sb_time - EzTimeValueToSbTime(&time_value));
  EXPECT_GT(kSbTimeSecond, EzTimeValueToSbTime(&time_value) - sb_time);

  // Now should be after the time I wrote this test.
  EzTimeT ez_time = EzTimeTFromSbTime(EzTimeValueToSbTime(&time_value));
  EXPECT_LE(kTestTimeWritten, ez_time);

  // And it should be before 5 years after I wrote this test, at least for a
  // while.
  EXPECT_GE(kTestTimePastWritten, ez_time);
}

TEST(EzTimeTExplodeTest, Positive) {
  EzTimeExploded exploded = {0};
  EXPECT_TRUE(EzTimeTExplode(&kTestTimePositive, kEzTimeZoneUTC, &exploded));
  EXPECT_EQ(2015 - 1900, exploded.tm_year);
  EXPECT_EQ(9 - 1, exploded.tm_mon);
  EXPECT_EQ(4, exploded.tm_wday);
  EXPECT_EQ(24, exploded.tm_mday);
  EXPECT_EQ(266, exploded.tm_yday);
  EXPECT_EQ(19, exploded.tm_hour);
  EXPECT_EQ(2, exploded.tm_min);
  EXPECT_EQ(8, exploded.tm_sec);
  EXPECT_EQ(0, exploded.tm_isdst);
}

TEST(EzTimeTExplodeTest, PositiveLocal) {
  EzTimeExploded exploded = {0};
  EXPECT_EQ(&exploded, EzTimeTExplodeLocal(&kTestTimePositive, &exploded));
  EXPECT_EQ(2015 - 1900, exploded.tm_year);
  EXPECT_EQ(9 - 1, exploded.tm_mon);
  EXPECT_EQ(4, exploded.tm_wday);
  // We don't know what the local time zone is, but it should be within a day of
  // the UTC date.
  EXPECT_LE(23, exploded.tm_mday);
  EXPECT_GE(25, exploded.tm_mday);
  EXPECT_EQ(266, exploded.tm_yday);
  EXPECT_EQ(2, exploded.tm_min);
  EXPECT_EQ(8, exploded.tm_sec);
}

TEST(EzTimeTExplodeTest, PositivePacific) {
  EzTimeExploded exploded = {0};
  EXPECT_TRUE(
      EzTimeTExplode(&kTestTimePositive, kEzTimeZonePacific, &exploded));
  EXPECT_EQ(2015 - 1900, exploded.tm_year);
  EXPECT_EQ(9 - 1, exploded.tm_mon);
  EXPECT_EQ(4, exploded.tm_wday);
  EXPECT_EQ(24, exploded.tm_mday);
  EXPECT_EQ(266, exploded.tm_yday);
  EXPECT_EQ(12, exploded.tm_hour);
  EXPECT_EQ(2, exploded.tm_min);
  EXPECT_EQ(8, exploded.tm_sec);
  EXPECT_EQ(1, exploded.tm_isdst);
}

TEST(EzTimeTExplodeTest, PosixZero) {
  EzTimeExploded exploded = {0};
  EXPECT_TRUE(EzTimeTExplode(&kTestTimePosixZero, kEzTimeZoneUTC, &exploded));
  EXPECT_EQ(1970 - 1900, exploded.tm_year);
  EXPECT_EQ(1 - 1, exploded.tm_mon);
  EXPECT_EQ(4, exploded.tm_wday);
  EXPECT_EQ(1, exploded.tm_mday);
  EXPECT_EQ(0, exploded.tm_yday);
  EXPECT_EQ(0, exploded.tm_hour);
  EXPECT_EQ(0, exploded.tm_min);
  EXPECT_EQ(0, exploded.tm_sec);
  EXPECT_EQ(0, exploded.tm_isdst);
}

TEST(EzTimeTExplodeTest, PosixNegative) {
  EzTimeExploded exploded = {0};
  EXPECT_TRUE(
      EzTimeTExplode(&kTestTimePosixNegative, kEzTimeZoneUTC, &exploded));
  EXPECT_EQ(1945 - 1900, exploded.tm_year);
  EXPECT_EQ(7 - 1, exploded.tm_mon);
  EXPECT_EQ(1, exploded.tm_wday);
  EXPECT_EQ(16, exploded.tm_mday);
  EXPECT_EQ(196, exploded.tm_yday);
  EXPECT_EQ(11, exploded.tm_hour);
  EXPECT_EQ(29, exploded.tm_min);
  EXPECT_EQ(21, exploded.tm_sec);
  EXPECT_EQ(0, exploded.tm_isdst);
}

TEST(EzTimeTExplodeTest, WindowsZero) {
  EzTimeExploded exploded = {0};
  EXPECT_TRUE(EzTimeTExplode(&kTestTimeWindowsZero, kEzTimeZoneUTC, &exploded));
  EXPECT_EQ(1601 - 1900, exploded.tm_year);
  EXPECT_EQ(1 - 1, exploded.tm_mon);
  EXPECT_EQ(1, exploded.tm_wday);
  EXPECT_EQ(1, exploded.tm_mday);
  EXPECT_EQ(0, exploded.tm_yday);
  EXPECT_EQ(0, exploded.tm_hour);
  EXPECT_EQ(0, exploded.tm_min);
  EXPECT_EQ(0, exploded.tm_sec);
  EXPECT_EQ(0, exploded.tm_isdst);
}

TEST(EzTimeTExplodeTest, WindowsNegative) {
  EzTimeExploded exploded = {0};
  EXPECT_TRUE(
      EzTimeTExplode(&kTestTimeWindowsNegative, kEzTimeZoneUTC, &exploded));
  EXPECT_EQ(1583 - 1900, exploded.tm_year);
  EXPECT_EQ(1 - 1, exploded.tm_mon);
  EXPECT_EQ(6, exploded.tm_wday);
  EXPECT_EQ(1, exploded.tm_mday);
  EXPECT_EQ(0, exploded.tm_yday);
  EXPECT_EQ(0, exploded.tm_hour);
  EXPECT_EQ(0, exploded.tm_min);
  EXPECT_EQ(0, exploded.tm_sec);
  EXPECT_EQ(0, exploded.tm_isdst);
}

TEST(EzTimeTExplodeTest, NextCentury) {
  EzTimeExploded exploded = {0};
  EXPECT_TRUE(EzTimeTExplode(&kTestTimeNextCentury, kEzTimeZoneUTC, &exploded));
  EXPECT_EQ(2101 - 1900, exploded.tm_year);
  EXPECT_EQ(1 - 1, exploded.tm_mon);
  EXPECT_EQ(6, exploded.tm_wday);
  EXPECT_EQ(1, exploded.tm_mday);
  EXPECT_EQ(0, exploded.tm_yday);
  EXPECT_EQ(0, exploded.tm_hour);
  EXPECT_EQ(0, exploded.tm_min);
  EXPECT_EQ(0, exploded.tm_sec);
  EXPECT_EQ(0, exploded.tm_isdst);
}

TEST(EzTimeTImplodeTest, Positive) {
  EzTimeExploded exploded = {0};
  exploded.tm_year = 2015 - 1900;
  exploded.tm_mon = 9 - 1;
  exploded.tm_mday = 24;
  exploded.tm_hour = 19;
  exploded.tm_min = 2;
  exploded.tm_sec = 8;
  EzTimeT result = EzTimeTImplode(&exploded, kEzTimeZoneUTC);
  EXPECT_EQ(kTestTimePositive, result);
}

TEST(EzTimeTImplodeTest, PosixZero) {
  EzTimeExploded exploded = {0};
  exploded.tm_year = 1970 - 1900;
  exploded.tm_mon = 1 - 1;
  exploded.tm_mday = 1;
  EzTimeT result = EzTimeTImplode(&exploded, kEzTimeZoneUTC);
  EXPECT_EQ(kTestTimePosixZero, result);
}

TEST(EzTimeTImplodeTest, PosixNegative) {
  EzTimeExploded exploded = {0};
  exploded.tm_year = 1945 - 1900;
  exploded.tm_mon = 7 - 1;
  exploded.tm_mday = 16;
  exploded.tm_hour = 11;
  exploded.tm_min = 29;
  exploded.tm_sec = 21;
  EzTimeT result = EzTimeTImplode(&exploded, kEzTimeZoneUTC);
  EXPECT_EQ(kTestTimePosixNegative, result);
}

TEST(EzTimeTImplodeTest, WindowsZero) {
  EzTimeExploded exploded = {0};
  exploded.tm_year = 1601 - 1900;
  exploded.tm_mon = 1 - 1;
  exploded.tm_mday = 1;
  EzTimeT result = EzTimeTImplode(&exploded, kEzTimeZoneUTC);
  EXPECT_EQ(kTestTimeWindowsZero, result);
}

TEST(EzTimeTImplodeTest, WindowsNegative) {
  EzTimeExploded exploded = {0};
  exploded.tm_year = 1583 - 1900;
  exploded.tm_mon = 1 - 1;
  exploded.tm_mday = 1;
  EzTimeT result = EzTimeTImplode(&exploded, kEzTimeZoneUTC);
  EXPECT_EQ(kTestTimeWindowsNegative, result);
}

TEST(EzTimeTImplodeTest, NextCentury) {
  EzTimeExploded exploded = {0};
  exploded.tm_year = 2101 - 1900;
  exploded.tm_mon = 1 - 1;
  exploded.tm_mday = 1;
  EzTimeT result = EzTimeTImplode(&exploded, kEzTimeZoneUTC);
  EXPECT_EQ(kTestTimeNextCentury, result);
}

}  // namespace
}  // namespace eztime
}  // namespace client_porting
}  // namespace starboard
