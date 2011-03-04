// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "media/base/clock.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::DefaultValue;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;

namespace base {

// Provide a stream output operator so we can use EXPECT_EQ(...) with TimeDelta.
//
// TODO(scherkus): move this into the testing package.
static std::ostream& operator<<(std::ostream& stream, const TimeDelta& time) {
  return (stream << time.ToInternalValue());
}

}  // namespace

namespace media {

class MockTimeProvider {
 public:
  MockTimeProvider() {
    DCHECK(!instance_) << "Only one instance of MockTimeProvider can exist";
    DCHECK(!DefaultValue<base::Time>::IsSet());
    instance_ = this;
    DefaultValue<base::Time>::Set(base::Time::FromInternalValue(0));
  }

  ~MockTimeProvider() {
    instance_ = NULL;
    DefaultValue<base::Time>::Clear();
  }

  MOCK_METHOD0(Now, base::Time());

  static base::Time StaticNow() {
    return instance_->Now();
  }

 private:
  static MockTimeProvider* instance_;
  DISALLOW_COPY_AND_ASSIGN(MockTimeProvider);
};

MockTimeProvider* MockTimeProvider::instance_ = NULL;

TEST(ClockTest, Created) {
  StrictMock<MockTimeProvider> mock_time;
  const base::TimeDelta kExpected = base::TimeDelta::FromSeconds(0);

  Clock clock(&MockTimeProvider::StaticNow);
  EXPECT_EQ(kExpected, clock.Elapsed());
}

TEST(ClockTest, Play_NormalSpeed) {
  InSequence s;
  StrictMock<MockTimeProvider> mock_time;
  EXPECT_CALL(mock_time, Now())
      .WillOnce(Return(base::Time::FromDoubleT(4)));
  EXPECT_CALL(mock_time, Now())
      .WillOnce(Return(base::Time::FromDoubleT(6)));
  const base::TimeDelta kZero;
  const base::TimeDelta kExpected = base::TimeDelta::FromSeconds(2);

  Clock clock(&MockTimeProvider::StaticNow);
  EXPECT_EQ(kZero, clock.Play());
  EXPECT_EQ(kExpected, clock.Elapsed());
}

TEST(ClockTest, Play_DoubleSpeed) {
  InSequence s;
  StrictMock<MockTimeProvider> mock_time;
  EXPECT_CALL(mock_time, Now())
      .WillOnce(Return(base::Time::FromDoubleT(4)));
  EXPECT_CALL(mock_time, Now())
      .WillOnce(Return(base::Time::FromDoubleT(9)));
  const base::TimeDelta kZero;
  const base::TimeDelta kExpected = base::TimeDelta::FromSeconds(10);

  Clock clock(&MockTimeProvider::StaticNow);
  clock.SetPlaybackRate(2.0f);
  EXPECT_EQ(kZero, clock.Play());
  EXPECT_EQ(kExpected, clock.Elapsed());
}

TEST(ClockTest, Play_HalfSpeed) {
  InSequence s;
  StrictMock<MockTimeProvider> mock_time;
  EXPECT_CALL(mock_time, Now())
      .WillOnce(Return(base::Time::FromDoubleT(4)));
  EXPECT_CALL(mock_time, Now())
      .WillOnce(Return(base::Time::FromDoubleT(8)));
  const base::TimeDelta kZero;
  const base::TimeDelta kExpected = base::TimeDelta::FromSeconds(2);

  Clock clock(&MockTimeProvider::StaticNow);
  clock.SetPlaybackRate(0.5f);
  EXPECT_EQ(kZero, clock.Play());
  EXPECT_EQ(kExpected, clock.Elapsed());
}

TEST(ClockTest, Play_ZeroSpeed) {
  // We'll play for 2 seconds at normal speed, 4 seconds at zero speed, and 8
  // seconds at normal speed:
  //   (1.0 x 2) + (0.0 x 4) + (1.0 x 8) = 10
  InSequence s;
  StrictMock<MockTimeProvider> mock_time;
  EXPECT_CALL(mock_time, Now())
      .WillOnce(Return(base::Time::FromDoubleT(4)));
  EXPECT_CALL(mock_time, Now())
      .WillOnce(Return(base::Time::FromDoubleT(6)));
  EXPECT_CALL(mock_time, Now())
      .WillOnce(Return(base::Time::FromDoubleT(10)));
  EXPECT_CALL(mock_time, Now())
      .WillOnce(Return(base::Time::FromDoubleT(18)));
  const base::TimeDelta kZero;
  const base::TimeDelta kExpected = base::TimeDelta::FromSeconds(10);

  Clock clock(&MockTimeProvider::StaticNow);
  EXPECT_EQ(kZero, clock.Play());
  clock.SetPlaybackRate(0.0f);
  clock.SetPlaybackRate(1.0f);
  EXPECT_EQ(kExpected, clock.Elapsed());
}

TEST(ClockTest, Play_MultiSpeed) {
  // We'll play for 2 seconds at half speed, 4 seconds at normal speed, and 8
  // seconds at double speed:
  //   (0.5 x 2) + (1.0 x 4) + (2.0 x 8) = 21
  InSequence s;
  StrictMock<MockTimeProvider> mock_time;
  EXPECT_CALL(mock_time, Now())
      .WillOnce(Return(base::Time::FromDoubleT(4)));
  EXPECT_CALL(mock_time, Now())
      .WillOnce(Return(base::Time::FromDoubleT(6)));
  EXPECT_CALL(mock_time, Now())
      .WillOnce(Return(base::Time::FromDoubleT(10)));
  EXPECT_CALL(mock_time, Now())
      .WillOnce(Return(base::Time::FromDoubleT(18)));
  const base::TimeDelta kZero;
  const base::TimeDelta kExpected = base::TimeDelta::FromSeconds(21);

  Clock clock(&MockTimeProvider::StaticNow);
  clock.SetPlaybackRate(0.5f);
  EXPECT_EQ(kZero, clock.Play());
  clock.SetPlaybackRate(1.0f);
  clock.SetPlaybackRate(2.0f);
  EXPECT_EQ(kExpected, clock.Elapsed());
}

TEST(ClockTest, Pause) {
  InSequence s;
  StrictMock<MockTimeProvider> mock_time;
  EXPECT_CALL(mock_time, Now())
      .WillOnce(Return(base::Time::FromDoubleT(4)));
  EXPECT_CALL(mock_time, Now())
      .WillOnce(Return(base::Time::FromDoubleT(8)));
  EXPECT_CALL(mock_time, Now())
      .WillOnce(Return(base::Time::FromDoubleT(12)));
  EXPECT_CALL(mock_time, Now())
      .WillOnce(Return(base::Time::FromDoubleT(16)));
  const base::TimeDelta kZero;
  const base::TimeDelta kFirstPause = base::TimeDelta::FromSeconds(4);
  const base::TimeDelta kSecondPause = base::TimeDelta::FromSeconds(8);

  Clock clock(&MockTimeProvider::StaticNow);
  EXPECT_EQ(kZero, clock.Play());
  EXPECT_EQ(kFirstPause, clock.Pause());
  EXPECT_EQ(kFirstPause, clock.Elapsed());
  EXPECT_EQ(kFirstPause, clock.Play());
  EXPECT_EQ(kSecondPause, clock.Pause());
  EXPECT_EQ(kSecondPause, clock.Elapsed());
}

TEST(ClockTest, SetTime_Paused) {
  // We'll remain paused while we set the time.  The time should be simply
  // updated without accessing the time provider.
  InSequence s;
  StrictMock<MockTimeProvider> mock_time;
  const base::TimeDelta kFirstTime = base::TimeDelta::FromSeconds(4);
  const base::TimeDelta kSecondTime = base::TimeDelta::FromSeconds(16);

  Clock clock(&MockTimeProvider::StaticNow);
  clock.SetTime(kFirstTime);
  EXPECT_EQ(kFirstTime, clock.Elapsed());
  clock.SetTime(kSecondTime);
  EXPECT_EQ(kSecondTime, clock.Elapsed());
}

TEST(ClockTest, SetTime_Playing) {
  // We'll play for 4 seconds, then set the time to 12, then play for 4 more
  // seconds.  We'll expect a media time of 16.
  InSequence s;
  StrictMock<MockTimeProvider> mock_time;
  EXPECT_CALL(mock_time, Now())
      .WillOnce(Return(base::Time::FromDoubleT(4)));
  EXPECT_CALL(mock_time, Now())
      .WillOnce(Return(base::Time::FromDoubleT(8)));
  EXPECT_CALL(mock_time, Now())
      .WillOnce(Return(base::Time::FromDoubleT(12)));
  const base::TimeDelta kZero;
  const base::TimeDelta kExepected = base::TimeDelta::FromSeconds(16);

  Clock clock(&MockTimeProvider::StaticNow);
  EXPECT_EQ(kZero, clock.Play());
  clock.SetTime(base::TimeDelta::FromSeconds(12));
  EXPECT_EQ(kExepected, clock.Elapsed());
}

}  // namespace media
