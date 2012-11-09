// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/quic_send_scheduler.h"

#include <cmath>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/congestion_control/quic_receipt_metrics_collector.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/quic_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class QuicSendSchedulerTest : public ::testing::Test {
 protected:
  void SetUpCongestionType(CongestionFeedbackType congestion_type) {
    sender_.reset(new QuicSendScheduler(&clock_, congestion_type));
  }

  MockClock clock_;
  scoped_ptr<QuicSendScheduler> sender_;
};

TEST_F(QuicSendSchedulerTest, FixedRateSenderAPI) {
  SetUpCongestionType(kFixRate);
  QuicAckFrame ack;
  ack.congestion_info.type = kFixRate;
  ack.congestion_info.fix_rate.bitrate_in_bytes_per_second = 300000;
  sender_->OnIncomingAckFrame(ack);
  EXPECT_EQ(-1, sender_->PeakSustainedBandwidth());
  EXPECT_EQ(0, sender_->TimeUntilSend(false));
  EXPECT_EQ(kMaxPacketSize, sender_->AvailableCongestionWindow());
  sender_->SentPacket(1, 1200, false);
  EXPECT_EQ(0u, sender_->AvailableCongestionWindow());
  EXPECT_EQ(4000, sender_->TimeUntilSend(false));
  clock_.AdvanceTime(0.002);
  EXPECT_EQ(2000, sender_->TimeUntilSend(false));
  clock_.AdvanceTime(0.002);
  EXPECT_EQ(0, sender_->TimeUntilSend(false));
}

TEST_F(QuicSendSchedulerTest, FixedRatePacing) {
  SetUpCongestionType(kFixRate);
  QuicAckFrame ack;
  ack.congestion_info.type = kFixRate;
  ack.congestion_info.fix_rate.bitrate_in_bytes_per_second = 100000;
  ack.received_info.largest_received = 0;
  sender_->OnIncomingAckFrame(ack);
  double acc_advance_time = 0.0;
  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(0, sender_->TimeUntilSend(false));
    EXPECT_EQ(kMaxPacketSize, sender_->AvailableCongestionWindow());
    sender_->SentPacket(i, 1200, false);
    double advance_time = sender_->TimeUntilSend(false) / 1000000.0;
    clock_.AdvanceTime(advance_time);
    acc_advance_time += advance_time;
    // Ack the packet we sent.
    ack.received_info.largest_received = i;
    sender_->OnIncomingAckFrame(ack);
  }
  EXPECT_EQ(1200, floor((acc_advance_time * 1000) + 0.5));
}

// TODO(rch): fix this on linux32
TEST_F(QuicSendSchedulerTest, DISABLED_AvailableCongestionWindow) {
  SetUpCongestionType(kFixRate);
  QuicAckFrame ack;
  ack.congestion_info.type = kFixRate;
  ack.congestion_info.fix_rate.bitrate_in_bytes_per_second = 100000;
  sender_->OnIncomingAckFrame(ack);
  EXPECT_EQ(0, sender_->TimeUntilSend(false));
  EXPECT_EQ(kMaxPacketSize, sender_->AvailableCongestionWindow());
  for (int i = 1; i <= 12; i++) {
    sender_->SentPacket(i, 100, false);
    double advance_time = sender_->TimeUntilSend(false) / 1000000.0;
    clock_.AdvanceTime(advance_time);
    EXPECT_EQ(kMaxPacketSize - (i * 100), sender_->AvailableCongestionWindow());
  }
  // Ack the packet we sent.
  ack.received_info.largest_received = 12;
  sender_->OnIncomingAckFrame(ack);
  EXPECT_EQ(kMaxPacketSize, sender_->AvailableCongestionWindow());
}

TEST_F(QuicSendSchedulerTest, FixedRateBandwidth) {
  SetUpCongestionType(kFixRate);
  QuicAckFrame ack;
  ack.congestion_info.type = kFixRate;
  ack.congestion_info.fix_rate.bitrate_in_bytes_per_second = 100000;
  sender_->OnIncomingAckFrame(ack);
  for (int i = 0; i < 100; ++i) {
    double advance_time = sender_->TimeUntilSend(false) / 1000000.0;
    clock_.AdvanceTime(advance_time);
    EXPECT_EQ(0, sender_->TimeUntilSend(false));
    EXPECT_EQ(kMaxPacketSize, sender_->AvailableCongestionWindow());
    sender_->SentPacket(i, 1000, false);
    // Ack the packet we sent.
    ack.received_info.largest_received = i;
    sender_->OnIncomingAckFrame(ack);
  }
  EXPECT_EQ(100000, sender_->BandwidthEstimate());
  EXPECT_EQ(100000, sender_->PeakSustainedBandwidth());
  EXPECT_EQ(100000, sender_->SentBandwidth());
}

TEST_F(QuicSendSchedulerTest, BandwidthWith3SecondGap) {
  SetUpCongestionType(kFixRate);
  QuicAckFrame ack;
  ack.congestion_info.type = kFixRate;
  ack.congestion_info.fix_rate.bitrate_in_bytes_per_second = 100000;
  sender_->OnIncomingAckFrame(ack);
  for (int i = 0; i < 100; ++i) {
    double advance_time = sender_->TimeUntilSend(false) / 1000000.0;
    clock_.AdvanceTime(advance_time);
    EXPECT_EQ(0, sender_->TimeUntilSend(false));
    EXPECT_EQ(kMaxPacketSize, sender_->AvailableCongestionWindow());
    sender_->SentPacket(i, 1000, false);
    // Ack the packet we sent.
    ack.received_info.largest_received = i;
    sender_->OnIncomingAckFrame(ack);
  }
  EXPECT_EQ(100000, sender_->BandwidthEstimate());
  EXPECT_EQ(100000, sender_->PeakSustainedBandwidth());
  EXPECT_EQ(100000, sender_->SentBandwidth());
  clock_.AdvanceTime(1.0);
  EXPECT_EQ(50000, sender_->SentBandwidth());
  clock_.AdvanceTime(2.1);
  EXPECT_EQ(100000, sender_->BandwidthEstimate());
  EXPECT_EQ(100000, sender_->PeakSustainedBandwidth());
  EXPECT_EQ(0, sender_->SentBandwidth());
  for (int i = 0; i < 150; ++i) {
    EXPECT_EQ(0, sender_->TimeUntilSend(false));
    EXPECT_EQ(kMaxPacketSize, sender_->AvailableCongestionWindow());
    sender_->SentPacket(i + 100, 1000, false);
    double advance_time = sender_->TimeUntilSend(false) / 1000000.0;
    clock_.AdvanceTime(advance_time);
    // Ack the packet we sent.
    ack.received_info.largest_received = i + 100;
    sender_->OnIncomingAckFrame(ack);
  }
  EXPECT_EQ(100000, sender_->BandwidthEstimate());
  EXPECT_EQ(100000, sender_->PeakSustainedBandwidth());
  EXPECT_EQ(50000, sender_->SentBandwidth());
}

}  // namespace net
