// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/congestion_control/quic_receipt_metrics_collector.h"
#include "net/quic/congestion_control/quic_send_scheduler.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/quic_protocol.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace testing {

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
  ack.congestion_info.fix_rate.bitrate_in_bytes_per_second = 30000;
  sender_->OnIncomingAckFrame(ack);
  EXPECT_EQ(-1, sender_->PeakSustainedBandwidth());
  EXPECT_TRUE(sender_->TimeUntilSend(false).IsZero());
  EXPECT_EQ(kMaxPacketSize, sender_->AvailableCongestionWindow());
  sender_->SentPacket(1, kMaxPacketSize, false);
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(40),
            sender_->TimeUntilSend(false));
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(35));
  EXPECT_EQ(QuicTime::Delta::Infinite(),
            sender_->TimeUntilSend(false));
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_EQ(QuicTime::Delta::Infinite(),
            sender_->TimeUntilSend(false));
}

TEST_F(QuicSendSchedulerTest, FixedRatePacing) {
  SetUpCongestionType(kFixRate);
  QuicAckFrame ack;
  ack.congestion_info.type = kFixRate;
  ack.congestion_info.fix_rate.bitrate_in_bytes_per_second = 100000;
  ack.received_info.largest_received = 0;
  sender_->OnIncomingAckFrame(ack);
  QuicTime acc_advance_time;
  for (int i = 0; i < 100; ++i) {
    EXPECT_TRUE(sender_->TimeUntilSend(false).IsZero());
    EXPECT_EQ(kMaxPacketSize, sender_->AvailableCongestionWindow());
    sender_->SentPacket(i, kMaxPacketSize, false);
    QuicTime::Delta advance_time = sender_->TimeUntilSend(false);
    clock_.AdvanceTime(advance_time);
    acc_advance_time = acc_advance_time.Add(advance_time);
    // Ack the packet we sent.
    ack.received_info.RecordAck(i, acc_advance_time);
    sender_->OnIncomingAckFrame(ack);
  }
  EXPECT_EQ(QuicTime::FromMilliseconds(1200), acc_advance_time);
}

TEST_F(QuicSendSchedulerTest, AvailableCongestionWindow) {
  SetUpCongestionType(kFixRate);
  QuicAckFrame ack;
  ack.congestion_info.type = kFixRate;
  ack.congestion_info.fix_rate.bitrate_in_bytes_per_second = 100000;
  sender_->OnIncomingAckFrame(ack);
  EXPECT_TRUE(sender_->TimeUntilSend(false).IsZero());
  EXPECT_EQ(kMaxPacketSize, sender_->AvailableCongestionWindow());
  for (int i = 1; i <= 12; i++) {
    EXPECT_TRUE(sender_->TimeUntilSend(false).IsZero());
    sender_->SentPacket(i, 100, false);
    EXPECT_EQ(kMaxPacketSize - (i * 100), sender_->AvailableCongestionWindow());
  }
  // Ack the packets we sent.
  for (int i = 1; i <= 12; i++) {
    ack.received_info.RecordAck(i, QuicTime::FromMilliseconds(100));
  }
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
    QuicTime::Delta advance_time = sender_->TimeUntilSend(false);
    clock_.AdvanceTime(advance_time);
    EXPECT_TRUE(sender_->TimeUntilSend(false).IsZero());
    EXPECT_EQ(kMaxPacketSize, sender_->AvailableCongestionWindow());
    sender_->SentPacket(i, 1000, false);
    // Ack the packet we sent.
    ack.received_info.RecordAck(i, clock_.Now());
    sender_->OnIncomingAckFrame(ack);
  }
  EXPECT_EQ(100000, sender_->BandwidthEstimate());
  EXPECT_EQ(101010, sender_->PeakSustainedBandwidth());
  EXPECT_EQ(101010, sender_->SentBandwidth());
}

TEST_F(QuicSendSchedulerTest, BandwidthWith3SecondGap) {
  SetUpCongestionType(kFixRate);
  QuicAckFrame ack;
  ack.congestion_info.type = kFixRate;
  ack.congestion_info.fix_rate.bitrate_in_bytes_per_second = 100000;
  sender_->OnIncomingAckFrame(ack);
  for (int i = 0; i < 100; ++i) {
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
    EXPECT_TRUE(sender_->TimeUntilSend(false).IsZero());
    EXPECT_EQ(kMaxPacketSize, sender_->AvailableCongestionWindow());
    sender_->SentPacket(i, 1000, false);
    // Ack the packet we sent.
    ack.received_info.RecordAck(i, clock_.Now());
    sender_->OnIncomingAckFrame(ack);
  }
  EXPECT_EQ(100000, sender_->BandwidthEstimate());
  EXPECT_EQ(100000, sender_->PeakSustainedBandwidth());
  EXPECT_EQ(100000, sender_->SentBandwidth());
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(1000));
  EXPECT_EQ(50000, sender_->SentBandwidth());
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(2100));
  EXPECT_EQ(100000, sender_->BandwidthEstimate());
  EXPECT_EQ(100000, sender_->PeakSustainedBandwidth());
  EXPECT_EQ(0, sender_->SentBandwidth());
  for (int i = 0; i < 150; ++i) {
    EXPECT_TRUE(sender_->TimeUntilSend(false).IsZero());
    EXPECT_EQ(kMaxPacketSize, sender_->AvailableCongestionWindow());
    sender_->SentPacket(i + 100, 1000, false);
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
    // Ack the packet we sent.
    ack.received_info.RecordAck(i + 100, clock_.Now());
    sender_->OnIncomingAckFrame(ack);
  }
  EXPECT_EQ(100000, sender_->BandwidthEstimate());
  EXPECT_EQ(100000, sender_->PeakSustainedBandwidth());
  EXPECT_EQ(50000, sender_->SentBandwidth());
}

TEST_F(QuicSendSchedulerTest, Pacing) {
  SetUpCongestionType(kFixRate);
  QuicAckFrame ack;
  ack.congestion_info.type = kFixRate;
  // Test a high bitrate (8Mbit/s) to trigger pacing.
  ack.congestion_info.fix_rate.bitrate_in_bytes_per_second = 1000000;
  ack.received_info.largest_received = 0;
  sender_->OnIncomingAckFrame(ack);
  QuicTime acc_advance_time;
  for (int i = 0; i < 100;) {
    EXPECT_TRUE(sender_->TimeUntilSend(false).IsZero());
    EXPECT_EQ(kMaxPacketSize * 2, sender_->AvailableCongestionWindow());
    sender_->SentPacket(i++, kMaxPacketSize, false);
    EXPECT_TRUE(sender_->TimeUntilSend(false).IsZero());
    sender_->SentPacket(i++, kMaxPacketSize, false);
    QuicTime::Delta advance_time = sender_->TimeUntilSend(false);
    clock_.AdvanceTime(advance_time);
    acc_advance_time = acc_advance_time.Add(advance_time);
    // Ack the packets we sent.
    ack.received_info.RecordAck(i - 2, clock_.Now());
    sender_->OnIncomingAckFrame(ack);
    ack.received_info.RecordAck(i - 1, clock_.Now());
    sender_->OnIncomingAckFrame(ack);
  }
  EXPECT_EQ(QuicTime::FromMilliseconds(120), acc_advance_time);
}

}  // namespace testing
}  // namespace net
