// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/congestion_control/hybrid_slow_start.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace testing {

class HybridSlowStartTest : public ::testing::Test {
 protected:
  HybridSlowStartTest()
     : one_ms_(QuicTime::Delta::FromMilliseconds(1)),
       rtt_(QuicTime::Delta::FromMilliseconds(60)) {
  }
  void SetUp() {
    slowStart_.reset(new HybridSlowStart(&clock_));
  }
  const QuicTime::Delta one_ms_;
  const QuicTime::Delta rtt_;
  MockClock clock_;
  scoped_ptr<HybridSlowStart> slowStart_;
};

TEST_F(HybridSlowStartTest, Simple) {
  QuicPacketSequenceNumber sequence_number = 1;
  QuicPacketSequenceNumber end_sequence_number = 3;
  slowStart_->Reset(end_sequence_number);

  EXPECT_FALSE(slowStart_->EndOfRound(sequence_number++));

  // Test duplicates.
  EXPECT_FALSE(slowStart_->EndOfRound(sequence_number));

  EXPECT_FALSE(slowStart_->EndOfRound(sequence_number++));
  EXPECT_TRUE(slowStart_->EndOfRound(sequence_number++));

  // Test without a new registered end_sequence_number;
  EXPECT_TRUE(slowStart_->EndOfRound(sequence_number++));

  end_sequence_number = 20;
  slowStart_->Reset(end_sequence_number);
  while (sequence_number < end_sequence_number) {
    EXPECT_FALSE(slowStart_->EndOfRound(sequence_number++));
  }
  EXPECT_TRUE(slowStart_->EndOfRound(sequence_number++));
}

TEST_F(HybridSlowStartTest, AckTrain) {
  // At a typical RTT 60 ms, assuming that the inter arrival is 1 ms,
  // we expect to be able to send a burst of 30 packet before we trigger the
  // ack train detection.
  const int kMaxLoopCount = 5;
  QuicPacketSequenceNumber sequence_number = 2;
  QuicPacketSequenceNumber end_sequence_number = 2;
  for (int burst = 0; burst < kMaxLoopCount; ++burst) {
    slowStart_->Reset(end_sequence_number);
    do {
      clock_.AdvanceTime(one_ms_);
      slowStart_->Update(rtt_, rtt_);
      EXPECT_FALSE(slowStart_->Exit());
    }  while (!slowStart_->EndOfRound(sequence_number++));
    end_sequence_number *= 2;  // Exponential growth.
  }
  slowStart_->Reset(end_sequence_number);

  for (int n = 0; n < 29 && !slowStart_->EndOfRound(sequence_number++); ++n) {
    clock_.AdvanceTime(one_ms_);
    slowStart_->Update(rtt_, rtt_);
    EXPECT_FALSE(slowStart_->Exit());
  }
  clock_.AdvanceTime(one_ms_);
  slowStart_->Update(rtt_, rtt_);
  EXPECT_TRUE(slowStart_->Exit());
}

TEST_F(HybridSlowStartTest, Delay) {
  // We expect to detect the increase at +1/16 of the RTT; hence at a typical
  // RTT of 60ms the detection will happen at 63.75 ms.
  const int kHybridStartMinSamples = 8;  // Number of acks required to trigger.

  QuicPacketSequenceNumber end_sequence_number = 1;
  slowStart_->Reset(end_sequence_number++);

  // Will not trigger since our lowest RTT in our burst is the same as the long
  // term RTT provided.
  for (int n = 0; n < kHybridStartMinSamples; ++n) {
    slowStart_->Update(rtt_.Add(QuicTime::Delta::FromMilliseconds(n)), rtt_);
    EXPECT_FALSE(slowStart_->Exit());
  }
  slowStart_->Reset(end_sequence_number++);
  for (int n = 1; n < kHybridStartMinSamples; ++n) {
    slowStart_->Update(rtt_.Add(QuicTime::Delta::FromMilliseconds(n + 4)),
                       rtt_);
    EXPECT_FALSE(slowStart_->Exit());
  }
  // Expect to trigger since all packets in this burst was above the long term
  // RTT provided.
  slowStart_->Update(rtt_.Add(QuicTime::Delta::FromMilliseconds(4)), rtt_);
  EXPECT_TRUE(slowStart_->Exit());
}

}  // namespace testing
}  // namespace net
