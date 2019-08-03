// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_time.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace testing {

class QuicTimeDeltaTest : public ::testing::Test {
 protected:
};

TEST_F(QuicTimeDeltaTest, Zero) {
  EXPECT_TRUE(QuicTime::Delta().IsZero());
  EXPECT_FALSE(QuicTime::Delta::FromMilliseconds(1).IsZero());
}

TEST_F(QuicTimeDeltaTest, Infinite) {
  EXPECT_TRUE(QuicTime::Delta::Infinite().IsInfinite());
  EXPECT_FALSE(QuicTime::Delta().IsInfinite());
  EXPECT_FALSE(QuicTime::Delta::FromMilliseconds(1).IsInfinite());
}

TEST_F(QuicTimeDeltaTest, FromTo) {
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(1),
            QuicTime::Delta::FromMicroseconds(1000));
  EXPECT_EQ(1, QuicTime::Delta::FromMicroseconds(1000).ToMilliseconds());
  EXPECT_EQ(1000, QuicTime::Delta::FromMilliseconds(1).ToMicroseconds());
}

TEST_F(QuicTimeDeltaTest, Add) {
  EXPECT_EQ(QuicTime::Delta::FromMicroseconds(2000),
            QuicTime::Delta().Add(QuicTime::Delta::FromMilliseconds(2)));
}

TEST_F(QuicTimeDeltaTest, Subtract) {
  EXPECT_EQ(QuicTime::Delta::FromMicroseconds(1000),
            QuicTime::Delta::FromMilliseconds(2).Subtract(
                QuicTime::Delta::FromMilliseconds(1)));
}

class QuicTimeTest : public ::testing::Test {
 protected:
  MockClock clock_;
};

TEST_F(QuicTimeTest, Initialized) {
  EXPECT_FALSE(QuicTime().IsInitialized());
  EXPECT_TRUE(QuicTime::FromMilliseconds(1).IsInitialized());
}

TEST_F(QuicTimeTest, FromTo) {
  EXPECT_EQ(QuicTime::FromMilliseconds(1),
            QuicTime::FromMicroseconds(1000));
  EXPECT_EQ(1u, QuicTime::FromMicroseconds(1000).ToMilliseconds());
  EXPECT_EQ(1000u, QuicTime::FromMilliseconds(1).ToMicroseconds());
}

TEST_F(QuicTimeTest, Add) {
  QuicTime time_1 = QuicTime::FromMilliseconds(1);
  QuicTime time_2 = QuicTime::FromMilliseconds(1);

  time_1 = time_1.Subtract(QuicTime::Delta::FromMilliseconds(1));

  QuicTime::Delta diff = time_2.Subtract(time_1);

  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(1), diff);
  EXPECT_EQ(1000, diff.ToMicroseconds());
  EXPECT_EQ(1, diff.ToMilliseconds());
}

TEST_F(QuicTimeTest, Subtract) {
  QuicTime time_1 = QuicTime::FromMilliseconds(1);
  QuicTime time_2 = QuicTime::FromMilliseconds(2);

  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(1), time_2.Subtract(time_1));
}

TEST_F(QuicTimeTest, SubtractDelta) {
  QuicTime time = QuicTime::FromMilliseconds(2);
  EXPECT_EQ(QuicTime::FromMilliseconds(1),
            time.Subtract(QuicTime::Delta::FromMilliseconds(1)));
}

TEST_F(QuicTimeTest, MockClock) {
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(1));

  QuicTime now = clock_.Now();
  QuicTime time = QuicTime::FromMicroseconds(1000);

  EXPECT_EQ(now, time);

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(1));
  now = clock_.Now();

  EXPECT_NE(now, time);

  time = time.Add(QuicTime::Delta::FromMilliseconds(1));
  EXPECT_EQ(now, time);
}

}  // namespace testing
}  // namespace net
