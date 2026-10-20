// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/platform/impl/quic_chromium_clock.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace quic::test {

TEST(QuicChromiumClockTest, Now) {
  QuicChromiumClock clock;

  QuicTime start = clock.Now();
  QuicTime now = clock.ApproximateNow();
  QuicTime end = clock.Now();

  EXPECT_LE(start, now);
  EXPECT_LE(now, end);
}

TEST(QuicChromiumClockTest, WallNow) {
  QuicChromiumClock clock;

  base::Time start = base::Time::Now();
  QuicWallTime now = clock.WallNow();
  base::Time end = base::Time::Now();

  // If end > start, then we can check now is between start and end.
  if (end > start) {
    EXPECT_LE(static_cast<uint64_t>(start.ToTimeT()), now.ToUNIXSeconds());
    EXPECT_LE(now.ToUNIXSeconds(), static_cast<uint64_t>(end.ToTimeT()));
  }
}

#if BUILDFLAG(IS_COBALT)
TEST(QuicChromiumClockTest, ApproximateNowIsNowOutsideScope) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  QuicChromiumClock clock;

  QuicTime start = clock.Now();
  task_environment.FastForwardBy(base::Seconds(8));

  EXPECT_EQ(clock.ApproximateNow(), start + QuicTime::Delta::FromSeconds(8));
}

TEST(QuicChromiumClockTest, ApproximateNowIsCachedInsideScope) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  QuicChromiumClock clock;

  QuicTime start = QuicTime::Zero();
  {
    QuicChromiumClock::ScopedApproximateNow scope;
    start = clock.Now();
    task_environment.FastForwardBy(base::Milliseconds(1));
    EXPECT_EQ(clock.ApproximateNow(), start);
  }

  task_environment.FastForwardBy(base::Seconds(8));

  // The cached time doesn't outlive the scope.
  EXPECT_EQ(clock.ApproximateNow(),
            start + QuicTime::Delta::FromMilliseconds(8001));
}

TEST(QuicChromiumClockTest, ScopeStartsWithoutCachedTime) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  QuicChromiumClock clock;

  QuicTime start = clock.Now();
  task_environment.FastForwardBy(base::Seconds(8));

  QuicChromiumClock::ScopedApproximateNow scope;
  EXPECT_EQ(clock.ApproximateNow(), start + QuicTime::Delta::FromSeconds(8));
}

TEST(QuicChromiumClockTest, NowRefreshesCachedTimeInsideScope) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  QuicChromiumClock clock;

  QuicChromiumClock::ScopedApproximateNow scope;
  clock.Now();
  task_environment.FastForwardBy(base::Seconds(8));
  QuicTime later = clock.Now();
  task_environment.FastForwardBy(base::Milliseconds(1));

  EXPECT_EQ(clock.ApproximateNow(), later);
}

TEST(QuicChromiumClockTest, OuterScopeCachesAfterInnerScopeEnds) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  QuicChromiumClock clock;

  QuicChromiumClock::ScopedApproximateNow outer_scope;
  QuicTime start = QuicTime::Zero();
  {
    QuicChromiumClock::ScopedApproximateNow inner_scope;
    start = clock.Now();
  }
  task_environment.FastForwardBy(base::Seconds(8));

  // The inner scope cleared the cached time, so the clock is read.
  EXPECT_EQ(clock.ApproximateNow(), start + QuicTime::Delta::FromSeconds(8));

  // That read is cached for the rest of the outer scope.
  task_environment.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(clock.ApproximateNow(), start + QuicTime::Delta::FromSeconds(8));
}
#endif  // BUILDFLAG(IS_COBALT)

}  // namespace quic::test
