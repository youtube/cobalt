// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/congestion_control/leaky_bucket.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace net {

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
  EXPECT_EQ(10000u, leaky_bucket_->TimeRemaining());
  clock_.AdvanceTime(0.005);
  EXPECT_EQ(1000u, leaky_bucket_->BytesPending());
  EXPECT_EQ(5000u, leaky_bucket_->TimeRemaining());
  clock_.AdvanceTime(0.005);
  EXPECT_EQ(0u, leaky_bucket_->BytesPending());
  EXPECT_EQ(0u, leaky_bucket_->TimeRemaining());
  clock_.AdvanceTime(0.005);
  EXPECT_EQ(0u, leaky_bucket_->BytesPending());
  EXPECT_EQ(0u, leaky_bucket_->TimeRemaining());
  leaky_bucket_->Add(2000);
  clock_.AdvanceTime(0.011);
  EXPECT_EQ(0u, leaky_bucket_->BytesPending());
  EXPECT_EQ(0u, leaky_bucket_->TimeRemaining());
  leaky_bucket_->Add(2000);
  clock_.AdvanceTime(0.005);
  leaky_bucket_->Add(2000);
  clock_.AdvanceTime(0.005);
  EXPECT_EQ(2000u, leaky_bucket_->BytesPending());
  EXPECT_EQ(10000u, leaky_bucket_->TimeRemaining());
  clock_.AdvanceTime(0.010);
  EXPECT_EQ(0u, leaky_bucket_->BytesPending());
  EXPECT_EQ(0u, leaky_bucket_->TimeRemaining());
}

TEST_F(LeakyBucketTest, ChangeDrainRate) {
  int bytes_per_second = 200000;
  leaky_bucket_->SetDrainingRate(bytes_per_second);
  leaky_bucket_->Add(2000);
  EXPECT_EQ(2000u, leaky_bucket_->BytesPending());
  EXPECT_EQ(10000u, leaky_bucket_->TimeRemaining());
  clock_.AdvanceTime(0.005);
  EXPECT_EQ(1000u, leaky_bucket_->BytesPending());
  EXPECT_EQ(5000u, leaky_bucket_->TimeRemaining());
  bytes_per_second = 100000;  // Cut drain rate in half.
  leaky_bucket_->SetDrainingRate(bytes_per_second);
  EXPECT_EQ(1000u, leaky_bucket_->BytesPending());
  EXPECT_EQ(10000u, leaky_bucket_->TimeRemaining());
}

}  // namespace net
