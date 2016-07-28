// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/congestion_control/leaky_bucket.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace testing {

class LeakyBucketTest : public ::testing::Test {
 protected:
  void SetUp() {
    leaky_bucket_.reset(new LeakyBucket(&clock_, 0));
  }
  MockClock clock_;
  scoped_ptr<LeakyBucket> leaky_bucket_;
};

TEST_F(LeakyBucketTest, Basic) {
  int bytes_per_second = 200000;
  leaky_bucket_->SetDrainingRate(bytes_per_second);
  leaky_bucket_->Add(2000);
  EXPECT_EQ(2000u, leaky_bucket_->BytesPending());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10),
            leaky_bucket_->TimeRemaining());
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_EQ(1000u, leaky_bucket_->BytesPending());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(5),
            leaky_bucket_->TimeRemaining());
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_EQ(0u, leaky_bucket_->BytesPending());
  EXPECT_TRUE(leaky_bucket_->TimeRemaining().IsZero());
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_EQ(0u, leaky_bucket_->BytesPending());
  EXPECT_TRUE(leaky_bucket_->TimeRemaining().IsZero());
  leaky_bucket_->Add(2000);
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(11));
  EXPECT_EQ(0u, leaky_bucket_->BytesPending());
  EXPECT_TRUE(leaky_bucket_->TimeRemaining().IsZero());
  leaky_bucket_->Add(2000);
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  leaky_bucket_->Add(2000);
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_EQ(2000u, leaky_bucket_->BytesPending());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10),
            leaky_bucket_->TimeRemaining());
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
  EXPECT_EQ(0u, leaky_bucket_->BytesPending());
  EXPECT_TRUE(leaky_bucket_->TimeRemaining().IsZero());
}

TEST_F(LeakyBucketTest, ChangeDrainRate) {
  int bytes_per_second = 200000;
  leaky_bucket_->SetDrainingRate(bytes_per_second);
  leaky_bucket_->Add(2000);
  EXPECT_EQ(2000u, leaky_bucket_->BytesPending());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10),
            leaky_bucket_->TimeRemaining());
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_EQ(1000u, leaky_bucket_->BytesPending());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(5),
            leaky_bucket_->TimeRemaining());
  bytes_per_second = 100000;  // Cut drain rate in half.
  leaky_bucket_->SetDrainingRate(bytes_per_second);
  EXPECT_EQ(1000u, leaky_bucket_->BytesPending());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10),
            leaky_bucket_->TimeRemaining());
}

}  // namespace testing
}  // namespace net
