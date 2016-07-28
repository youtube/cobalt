// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/congestion_control/tcp_cubic_sender.h"
#include "net/quic/congestion_control/tcp_receiver.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace testing {

const uint32 kDefaultWindowTCP = 10 * net::kMaxPacketSize;
const size_t kNoNBytesInFlight = 0;

class QuicTcpCubicSenderTest : public ::testing::Test {
 protected:
  QuicTcpCubicSenderTest()
     : rtt_(QuicTime::Delta::FromMilliseconds(60)),
       one_ms_(QuicTime::Delta::FromMilliseconds(1)) {
  }
  void SetUp() {
    bool reno = true;
    sender_.reset(new TcpCubicSender(&clock_, reno));
    receiver_.reset(new TcpReceiver());
    sequence_number_ = 1;
    acked_sequence_number_ = 0;
  }
  void SendAvailableCongestionWindow() {
    size_t bytes_to_send = sender_->AvailableCongestionWindow();
    while (bytes_to_send > 0) {
      size_t bytes_in_packet = std::min(kMaxPacketSize, bytes_to_send);
      sender_->SentPacket(sequence_number_++, bytes_in_packet, false);
      bytes_to_send -= bytes_in_packet;
      if (bytes_to_send > 0) {
        EXPECT_TRUE(sender_->TimeUntilSend(false).IsZero());
      }
    }
  }
  // Normal is that TCP acks every other segment.
  void AckNPackets(int n) {
    for (int i = 0; i < n; ++i) {
      acked_sequence_number_++;
      sender_->OnIncomingAck(acked_sequence_number_, kMaxPacketSize, rtt_);
    }
    clock_.AdvanceTime(one_ms_);  // 1 millisecond.
  }

  const QuicTime::Delta rtt_;
  const QuicTime::Delta one_ms_;
  MockClock clock_;
  scoped_ptr<TcpCubicSender> sender_;
  scoped_ptr<TcpReceiver> receiver_;
  QuicPacketSequenceNumber sequence_number_;
  QuicPacketSequenceNumber acked_sequence_number_;
};

TEST_F(QuicTcpCubicSenderTest, SimpleSender) {
  QuicCongestionFeedbackFrame feedback;
  // At startup make sure we are at the default.
  EXPECT_EQ(kDefaultWindowTCP,
            sender_->AvailableCongestionWindow());
  // At startup make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(false).IsZero());
  // Get default QuicCongestionFeedbackFrame from receiver.
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  sender_->OnIncomingQuicCongestionFeedbackFrame(feedback);
  // Make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(false).IsZero());
  // And that window is un-affected.
  EXPECT_EQ(kDefaultWindowTCP, sender_->AvailableCongestionWindow());

  // A retransmitt should always retun 0.
  EXPECT_TRUE(sender_->TimeUntilSend(true).IsZero());
}

TEST_F(QuicTcpCubicSenderTest, ExponentialSlowStart) {
  const int kNumberOfAck = 20;
  QuicCongestionFeedbackFrame feedback;
  // At startup make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(false).IsZero());
  // Get default QuicCongestionFeedbackFrame from receiver.
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  sender_->OnIncomingQuicCongestionFeedbackFrame(feedback);
  // Make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(false).IsZero());

  for (int n = 0; n < kNumberOfAck; ++n) {
    // Send our full congestion window.
    SendAvailableCongestionWindow();
    AckNPackets(2);
  }
  size_t bytes_to_send = sender_->CongestionWindow();
  EXPECT_EQ(kDefaultWindowTCP + kMaxPacketSize * 2 * kNumberOfAck,
            bytes_to_send);
}

TEST_F(QuicTcpCubicSenderTest, SlowStartAckTrain) {
  // Make sure that we fall out of slow start when we send ACK train longer
  // than half the RTT, in this test case 30ms, which is more than 30 calls to
  // Ack2Packets in one round.
  // Since we start at 10 packet first round will be 5 second round 10 etc
  // Hence we should pass 30 at 65 = 5 + 10 + 20 + 30
  const int kNumberOfAck = 65;
  QuicCongestionFeedbackFrame feedback;
  // At startup make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(false).IsZero());
  // Get default QuicCongestionFeedbackFrame from receiver.
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  sender_->OnIncomingQuicCongestionFeedbackFrame(feedback);
  // Make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(false).IsZero());

  for (int n = 0; n < kNumberOfAck; ++n) {
    // Send our full congestion window.
    SendAvailableCongestionWindow();
    AckNPackets(2);
  }
  size_t expected_congestion_window = kDefaultWindowTCP +
      (kMaxPacketSize * 2 * kNumberOfAck);
  EXPECT_EQ(expected_congestion_window, sender_->CongestionWindow());
  // We should now have fallen out of slow start.
  SendAvailableCongestionWindow();
  AckNPackets(2);
  expected_congestion_window += kMaxPacketSize;
  EXPECT_EQ(expected_congestion_window, sender_->CongestionWindow());

  // Testing Reno phase.
  // We should need 141(65*2+1+10) ACK:ed packets before increasing window by
  // one.
  for (int m = 0; m < 70; ++m) {
    SendAvailableCongestionWindow();
    AckNPackets(2);
    EXPECT_EQ(expected_congestion_window, sender_->CongestionWindow());
  }
  SendAvailableCongestionWindow();
  AckNPackets(2);
  expected_congestion_window += kMaxPacketSize;
  EXPECT_EQ(expected_congestion_window, sender_->CongestionWindow());
}

TEST_F(QuicTcpCubicSenderTest, SlowStartPacketLoss) {
  // Make sure that we fall out of slow start when we encounter a packet loss.
  const int kNumberOfAck = 10;
  QuicCongestionFeedbackFrame feedback;
  // At startup make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(false).IsZero());
  // Get default QuicCongestionFeedbackFrame from receiver.
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  sender_->OnIncomingQuicCongestionFeedbackFrame(feedback);
  // Make sure we can send.
  EXPECT_TRUE(sender_->TimeUntilSend(false).IsZero());

  for (int i = 0; i < kNumberOfAck; ++i) {
    // Send our full congestion window.
    SendAvailableCongestionWindow();
    AckNPackets(2);
  }
  SendAvailableCongestionWindow();
  size_t expected_congestion_window = kDefaultWindowTCP +
      (kMaxPacketSize * 2 * kNumberOfAck);
  EXPECT_EQ(expected_congestion_window, sender_->CongestionWindow());

  sender_->OnIncomingLoss(1);

  // Make sure that we should not send right now.
  EXPECT_TRUE(sender_->TimeUntilSend(false).IsInfinite());

  // We should now have fallen out of slow start.
  // We expect window to be cut in half.
  expected_congestion_window /= 2;
  EXPECT_EQ(expected_congestion_window, sender_->CongestionWindow());

  // Testing Reno phase.
  // We need to ack half of the pending packet before we can send agin.
  int number_of_packets_in_window = expected_congestion_window / kMaxPacketSize;
  AckNPackets(number_of_packets_in_window);
  EXPECT_EQ(expected_congestion_window, sender_->CongestionWindow());
  EXPECT_EQ(0u, sender_->AvailableCongestionWindow());

  AckNPackets(1);
  expected_congestion_window += kMaxPacketSize;
  number_of_packets_in_window++;
  EXPECT_EQ(expected_congestion_window, sender_->CongestionWindow());

  // We should need number_of_packets_in_window ACK:ed packets before
  // increasing window by one.
  for (int k = 0; k < number_of_packets_in_window; ++k) {
    SendAvailableCongestionWindow();
    AckNPackets(1);
    EXPECT_EQ(expected_congestion_window, sender_->CongestionWindow());
  }
  SendAvailableCongestionWindow();
  AckNPackets(1);
  expected_congestion_window += kMaxPacketSize;
  EXPECT_EQ(expected_congestion_window, sender_->CongestionWindow());
}

}  // namespace testing
}  // namespace net
