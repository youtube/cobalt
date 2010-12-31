// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>

#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

// Test conversions to/from time_t and exploding/unexploding.
TEST(Time, TimeT) {
  // C library time and exploded time.
  time_t now_t_1 = time(NULL);
  struct tm tms;
#if defined(OS_WIN)
  localtime_s(&tms, &now_t_1);
#elif defined(OS_POSIX)
  localtime_r(&now_t_1, &tms);
#endif

  // Convert to ours.
  Time our_time_1 = Time::FromTimeT(now_t_1);
  Time::Exploded exploded;
  our_time_1.LocalExplode(&exploded);

  // This will test both our exploding and our time_t -> Time conversion.
  EXPECT_EQ(tms.tm_year + 1900, exploded.year);
  EXPECT_EQ(tms.tm_mon + 1, exploded.month);
  EXPECT_EQ(tms.tm_mday, exploded.day_of_month);
  EXPECT_EQ(tms.tm_hour, exploded.hour);
  EXPECT_EQ(tms.tm_min, exploded.minute);
  EXPECT_EQ(tms.tm_sec, exploded.second);

  // Convert exploded back to the time struct.
  Time our_time_2 = Time::FromLocalExploded(exploded);
  EXPECT_TRUE(our_time_1 == our_time_2);

  time_t now_t_2 = our_time_2.ToTimeT();
  EXPECT_EQ(now_t_1, now_t_2);

  EXPECT_EQ(10, Time().FromTimeT(10).ToTimeT());
  EXPECT_EQ(10.0, Time().FromTimeT(10).ToDoubleT());

  // Conversions of 0 should stay 0.
  EXPECT_EQ(0, Time().ToTimeT());
  EXPECT_EQ(0, Time::FromTimeT(0).ToInternalValue());
}

TEST(Time, FromExplodedWithMilliseconds) {
  // Some platform implementations of FromExploded are liable to drop
  // milliseconds if we aren't careful.
  Time now = Time::NowFromSystemTime();
  Time::Exploded exploded1 = {0};
  now.UTCExplode(&exploded1);
  exploded1.millisecond = 500;
  Time time = Time::FromUTCExploded(exploded1);
  Time::Exploded exploded2 = {0};
  time.UTCExplode(&exploded2);
  EXPECT_EQ(exploded1.millisecond, exploded2.millisecond);
}

TEST(Time, ZeroIsSymmetric) {
  Time zero_time(Time::FromTimeT(0));
  EXPECT_EQ(0, zero_time.ToTimeT());

  EXPECT_EQ(0.0, zero_time.ToDoubleT());
}

TEST(Time, LocalExplode) {
  Time a = Time::Now();
  Time::Exploded exploded;
  a.LocalExplode(&exploded);

  Time b = Time::FromLocalExploded(exploded);

  // The exploded structure doesn't have microseconds, and on Mac & Linux, the
  // internal OS conversion uses seconds, which will cause truncation. So we
  // can only make sure that the delta is within one second.
  EXPECT_TRUE((a - b) < TimeDelta::FromSeconds(1));
}

TEST(Time, UTCExplode) {
  Time a = Time::Now();
  Time::Exploded exploded;
  a.UTCExplode(&exploded);

  Time b = Time::FromUTCExploded(exploded);
  EXPECT_TRUE((a - b) < TimeDelta::FromSeconds(1));
}

TEST(Time, LocalMidnight) {
  Time::Exploded exploded;
  Time::Now().LocalMidnight().LocalExplode(&exploded);
  EXPECT_EQ(0, exploded.hour);
  EXPECT_EQ(0, exploded.minute);
  EXPECT_EQ(0, exploded.second);
  EXPECT_EQ(0, exploded.millisecond);
}

TEST(TimeTicks, Deltas) {
  for (int index = 0; index < 50; index++) {
    TimeTicks ticks_start = TimeTicks::Now();
    base::PlatformThread::Sleep(10);
    TimeTicks ticks_stop = TimeTicks::Now();
    TimeDelta delta = ticks_stop - ticks_start;
    // Note:  Although we asked for a 10ms sleep, if the
    // time clock has a finer granularity than the Sleep()
    // clock, it is quite possible to wakeup early.  Here
    // is how that works:
    //      Time(ms timer)      Time(us timer)
    //          5                   5010
    //          6                   6010
    //          7                   7010
    //          8                   8010
    //          9                   9000
    // Elapsed  4ms                 3990us
    //
    // Unfortunately, our InMilliseconds() function truncates
    // rather than rounds.  We should consider fixing this
    // so that our averages come out better.
    EXPECT_GE(delta.InMilliseconds(), 9);
    EXPECT_GE(delta.InMicroseconds(), 9000);
    EXPECT_EQ(delta.InSeconds(), 0);
  }
}

TEST(TimeTicks, HighResNow) {
#if defined(OS_WIN)
  // HighResNow doesn't work on some systems.  Since the product still works
  // even if it doesn't work, it makes this entire test questionable.
  if (!TimeTicks::IsHighResClockWorking())
    return;
#endif

  // Why do we loop here?
  // We're trying to measure that intervals increment in a VERY small amount
  // of time --  less than 15ms.  Unfortunately, if we happen to have a
  // context switch in the middle of our test, the context switch could easily
  // exceed our limit.  So, we iterate on this several times.  As long as we're
  // able to detect the fine-granularity timers at least once, then the test
  // has succeeded.

  const int kTargetGranularityUs = 15000;  // 15ms

  bool success = false;
  int retries = 100;  // Arbitrary.
  TimeDelta delta;
  while (!success && retries--) {
    TimeTicks ticks_start = TimeTicks::HighResNow();
    // Loop until we can detect that the clock has changed.  Non-HighRes timers
    // will increment in chunks, e.g. 15ms.  By spinning until we see a clock
    // change, we detect the minimum time between measurements.
    do {
      delta = TimeTicks::HighResNow() - ticks_start;
    } while (delta.InMilliseconds() == 0);

    if (delta.InMicroseconds() <= kTargetGranularityUs)
      success = true;
  }

  // In high resolution mode, we expect to see the clock increment
  // in intervals less than 15ms.
  EXPECT_TRUE(success);
}

TEST(TimeDelta, FromAndIn) {
  EXPECT_TRUE(TimeDelta::FromDays(2) == TimeDelta::FromHours(48));
  EXPECT_TRUE(TimeDelta::FromHours(3) == TimeDelta::FromMinutes(180));
  EXPECT_TRUE(TimeDelta::FromMinutes(2) == TimeDelta::FromSeconds(120));
  EXPECT_TRUE(TimeDelta::FromSeconds(2) == TimeDelta::FromMilliseconds(2000));
  EXPECT_TRUE(TimeDelta::FromMilliseconds(2) ==
              TimeDelta::FromMicroseconds(2000));
  EXPECT_EQ(13, TimeDelta::FromDays(13).InDays());
  EXPECT_EQ(13, TimeDelta::FromHours(13).InHours());
  EXPECT_EQ(13, TimeDelta::FromMinutes(13).InMinutes());
  EXPECT_EQ(13, TimeDelta::FromSeconds(13).InSeconds());
  EXPECT_EQ(13.0, TimeDelta::FromSeconds(13).InSecondsF());
  EXPECT_EQ(13, TimeDelta::FromMilliseconds(13).InMilliseconds());
  EXPECT_EQ(13.0, TimeDelta::FromMilliseconds(13).InMillisecondsF());
  EXPECT_EQ(13, TimeDelta::FromMicroseconds(13).InMicroseconds());
}

#if defined(OS_POSIX)
TEST(TimeDelta, TimeSpecConversion) {
  struct timespec result = TimeDelta::FromSeconds(0).ToTimeSpec();
  EXPECT_EQ(result.tv_sec, 0);
  EXPECT_EQ(result.tv_nsec, 0);

  result = TimeDelta::FromSeconds(1).ToTimeSpec();
  EXPECT_EQ(result.tv_sec, 1);
  EXPECT_EQ(result.tv_nsec, 0);

  result = TimeDelta::FromMicroseconds(1).ToTimeSpec();
  EXPECT_EQ(result.tv_sec, 0);
  EXPECT_EQ(result.tv_nsec, 1000);

  result = TimeDelta::FromMicroseconds(
      Time::kMicrosecondsPerSecond + 1).ToTimeSpec();
  EXPECT_EQ(result.tv_sec, 1);
  EXPECT_EQ(result.tv_nsec, 1000);
}
#endif  // OS_POSIX

// Our internal time format is serialized in things like databases, so it's
// important that it's consistent across all our platforms.  We use the 1601
// Windows epoch as the internal format across all platforms.
TEST(TimeDelta, WindowsEpoch) {
  Time::Exploded exploded;
  exploded.year = 1970;
  exploded.month = 1;
  exploded.day_of_week = 0;  // Should be unusued.
  exploded.day_of_month = 1;
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;
  Time t = Time::FromUTCExploded(exploded);
  // Unix 1970 epoch.
  EXPECT_EQ(GG_INT64_C(11644473600000000), t.ToInternalValue());

  // We can't test 1601 epoch, since the system time functions on Linux
  // only compute years starting from 1900.
}
