// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Test for FixRate sender and receiver.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/congestion_control/fix_rate_receiver.h"
#include "net/quic/congestion_control/fix_rate_sender.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/quic_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace testing {

class FixRateTest : public ::testing::Test {
 protected:
  FixRateTest()
      : rtt_(QuicTime::Delta::FromMilliseconds(30)) {
  }
  void SetUp() {
    sender_.reset(new FixRateSender(&clock_));
    receiver_.reset(new FixRateReceiver());
    // Make sure clock does not start at 0.
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(2));
  }
  const QuicTime::Delta rtt_;
  MockClock clock_;
  scoped_ptr<FixRateSender> sender_;
  scoped_ptr<FixRateReceiver> receiver_;
};

TEST_F(FixRateTest, ReceiverAPI) {
  QuicCongestionFeedbackFrame feedback;
  QuicTime timestamp;
  receiver_->SetBitrate(300000);  // Bytes per second.
  receiver_->RecordIncomingPacket(1, 1, timestamp, false);
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  EXPECT_EQ(kFixRate, feedback.type);
  EXPECT_EQ(300000u, feedback.fix_rate.bitrate_in_bytes_per_second);
}

TEST_F(FixRateTest, SenderAPI) {
  QuicCongestionFeedbackFrame feedback;
  feedback.type = kFixRate;
  feedback.fix_rate.bitrate_in_bytes_per_second = 300000;
  sender_->OnIncomingQuicCongestionFeedbackFrame(feedback);
  EXPECT_EQ(300000, sender_->BandwidthEstimate());
  EXPECT_TRUE(sender_->TimeUntilSend(false).IsZero());
  EXPECT_EQ(kMaxPacketSize * 2, sender_->AvailableCongestionWindow());
  sender_->SentPacket(1, kMaxPacketSize, false);
  EXPECT_EQ(3000u - kMaxPacketSize, sender_->AvailableCongestionWindow());
  EXPECT_TRUE(sender_->TimeUntilSend(false).IsZero());
  sender_->SentPacket(2, kMaxPacketSize, false);
  sender_->SentPacket(3, 600, false);
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10),
            sender_->TimeUntilSend(false));
  EXPECT_EQ(0u, sender_->AvailableCongestionWindow());
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(2));
  EXPECT_EQ(QuicTime::Delta::Infinite(), sender_->TimeUntilSend(false));
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(8));
  sender_->OnIncomingAck(1, kMaxPacketSize, rtt_);
  sender_->OnIncomingAck(2, kMaxPacketSize, rtt_);
  sender_->OnIncomingAck(3, 600, rtt_);
  EXPECT_TRUE(sender_->TimeUntilSend(false).IsZero());
}

TEST_F(FixRateTest, FixRatePacing) {
  const int64 packet_size = 1200;
  const int64 bit_rate = 240000;
  const int64 num_packets = 200;
  QuicCongestionFeedbackFrame feedback;
  receiver_->SetBitrate(240000);  // Bytes per second.
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  sender_->OnIncomingQuicCongestionFeedbackFrame(feedback);
  QuicTime acc_advance_time;
  QuicPacketSequenceNumber sequence_number = 0;
  for (int i = 0; i < num_packets; i += 2) {
    EXPECT_TRUE(sender_->TimeUntilSend(false).IsZero());
    EXPECT_EQ(kMaxPacketSize * 2, sender_->AvailableCongestionWindow());
    sender_->SentPacket(sequence_number++, packet_size, false);
    EXPECT_TRUE(sender_->TimeUntilSend(false).IsZero());
    sender_->SentPacket(sequence_number++, packet_size, false);
    QuicTime::Delta advance_time = sender_->TimeUntilSend(false);
    clock_.AdvanceTime(advance_time);
    sender_->OnIncomingAck(sequence_number - 1, packet_size, rtt_);
    sender_->OnIncomingAck(sequence_number - 2, packet_size, rtt_);
    acc_advance_time = acc_advance_time.Add(advance_time);
  }
  EXPECT_EQ(num_packets * packet_size * 1000000 / bit_rate,
            acc_advance_time.ToMicroseconds());
}

}  // namespace testing
}  // namespace net
