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

using ::testing::_;

namespace net {
namespace testing {

const int kHundredKBytesPerS = 100000;

class PacedSenderTest : public ::testing::Test {
 protected:
  void SetUp() {
    paced_sender_.reset(new PacedSender(&clock_, kHundredKBytesPerS));
  }
  MockClock clock_;
  scoped_ptr<net::PacedSender> paced_sender_;
};

TEST_F(PacedSenderTest, Basic) {
  paced_sender_->UpdateBandwidthEstimate(kHundredKBytesPerS * 10);
  EXPECT_EQ(0, paced_sender_->TimeUntilSend(0));
  EXPECT_EQ(kMaxPacketSize * 2,
            paced_sender_->AvailableWindow(kMaxPacketSize * 4));
  paced_sender_->SentPacket(kMaxPacketSize);
  EXPECT_EQ(0, paced_sender_->TimeUntilSend(0));
  paced_sender_->SentPacket(kMaxPacketSize);
  EXPECT_EQ(int(kMaxPacketSize * 2), paced_sender_->TimeUntilSend(0));
  EXPECT_EQ(0u, paced_sender_->AvailableWindow(kMaxPacketSize * 4));
  clock_.AdvanceTime(0.0024);
  EXPECT_EQ(0, paced_sender_->TimeUntilSend(0));
  EXPECT_EQ(kMaxPacketSize * 2,
            paced_sender_->AvailableWindow(kMaxPacketSize * 4));
}

}  // namespace testing
}  // namespace net
