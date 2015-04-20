// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/congestion_control/paced_sender.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace testing {

const int kHundredKBytesPerS = 100000;

class PacedSenderTest : public ::testing::Test {
 protected:
  PacedSenderTest()
      : paced_sender_(new PacedSender(&clock_, kHundredKBytesPerS)) {
  }

  const QuicTime::Delta zero_time_;
  MockClock clock_;
  scoped_ptr<PacedSender> paced_sender_;
};

TEST_F(PacedSenderTest, Basic) {
  paced_sender_->UpdateBandwidthEstimate(kHundredKBytesPerS * 10);
  EXPECT_TRUE(paced_sender_->TimeUntilSend(zero_time_).IsZero());
  EXPECT_EQ(kMaxPacketSize * 2,
            paced_sender_->AvailableWindow(kMaxPacketSize * 4));
  paced_sender_->SentPacket(kMaxPacketSize);
  EXPECT_TRUE(paced_sender_->TimeUntilSend(zero_time_).IsZero());
  paced_sender_->SentPacket(kMaxPacketSize);
  EXPECT_EQ(static_cast<int64>(kMaxPacketSize * 2),
            paced_sender_->TimeUntilSend(zero_time_).ToMicroseconds());
  EXPECT_EQ(0u, paced_sender_->AvailableWindow(kMaxPacketSize * 4));
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(24));
  EXPECT_TRUE(paced_sender_->TimeUntilSend(zero_time_).IsZero());
  EXPECT_EQ(kMaxPacketSize * 2,
            paced_sender_->AvailableWindow(kMaxPacketSize * 4));
}

TEST_F(PacedSenderTest, LowRate) {
  paced_sender_->UpdateBandwidthEstimate(kHundredKBytesPerS);
  EXPECT_TRUE(paced_sender_->TimeUntilSend(zero_time_).IsZero());
  size_t window = paced_sender_->AvailableWindow(kMaxPacketSize * 4);
  EXPECT_EQ(kMaxPacketSize * 2, window);
  paced_sender_->SentPacket(kMaxPacketSize);
  EXPECT_TRUE(paced_sender_->TimeUntilSend(zero_time_).IsZero());
  paced_sender_->SentPacket(kMaxPacketSize);
  EXPECT_EQ(static_cast<int64>(kMaxPacketSize * 20),
            paced_sender_->TimeUntilSend(zero_time_).ToMicroseconds());
  EXPECT_EQ(0u, paced_sender_->AvailableWindow(kMaxPacketSize * 4));
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(24));
  EXPECT_TRUE(paced_sender_->TimeUntilSend(zero_time_).IsZero());
  EXPECT_EQ(kMaxPacketSize * 2,
            paced_sender_->AvailableWindow(kMaxPacketSize * 4));
}

TEST_F(PacedSenderTest, HighRate) {
  int bandwidth_estimate = kHundredKBytesPerS * 100;
  paced_sender_->UpdateBandwidthEstimate(bandwidth_estimate);
  EXPECT_TRUE(paced_sender_->TimeUntilSend(zero_time_).IsZero());
  EXPECT_EQ(bandwidth_estimate / 500u,
            paced_sender_->AvailableWindow(kMaxPacketSize * 100));
  for (int i = 0; i < 16; ++i) {
    paced_sender_->SentPacket(kMaxPacketSize);
    EXPECT_TRUE(paced_sender_->TimeUntilSend(zero_time_).IsZero());
  }
  paced_sender_->SentPacket(kMaxPacketSize);
  EXPECT_EQ(0u, paced_sender_->AvailableWindow(kMaxPacketSize * 100));
  EXPECT_EQ(2040, paced_sender_->TimeUntilSend(zero_time_).ToMicroseconds());
  clock_.AdvanceTime(QuicTime::Delta::FromMicroseconds(20400));
  EXPECT_TRUE(paced_sender_->TimeUntilSend(zero_time_).IsZero());
  EXPECT_EQ(bandwidth_estimate / 500u,
            paced_sender_->AvailableWindow(kMaxPacketSize * 100));
}

}  // namespace testing
}  // namespace net
