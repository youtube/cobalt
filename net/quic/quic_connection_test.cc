// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_connection.h"

#include "net/base/net_errors.h"
#include "net/quic/congestion_control/quic_receipt_metrics_collector.h"
#include "net/quic/congestion_control/quic_send_scheduler.h"
#include "net/quic/crypto/null_encrypter.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/quic_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using std::map;
using testing::_;
using testing::ContainerEq;
using testing::Return;
using testing::StrictMock;

namespace net {

// Peer to make public a number of otherwise private QuicConnection methods.
class QuicConnectionPeer {
 public:
  static void SendAck(QuicConnection* connection) {
    connection->SendAck();
  }

  static void SetCollector(QuicConnection* connection,
                           QuicReceiptMetricsCollector* collector) {
    connection->collector_.reset(collector);
  }

  static void SetScheduler(QuicConnection* connection,
                           QuicSendScheduler* scheduler) {
    connection->scheduler_.reset(scheduler);
  }
  static QuicAckFrame* GetOutgoingAck(QuicConnection* connection) {
    return &connection->outgoing_ack_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicConnectionPeer);
};

namespace test {
namespace {

const char data1[] = "foo";
const char data2[] = "bar";

class TestCollector : public QuicReceiptMetricsCollector {
 public:
  explicit TestCollector(QuicCongestionFeedbackFrame* feedback)
      : QuicReceiptMetricsCollector(&clock_, kFixRate),
        feedback_(feedback) {
  }

  bool GenerateCongestionFeedback(
      QuicCongestionFeedbackFrame* congestion_feedback) {
    if (feedback_ == NULL) {
      return false;
    }
    *congestion_feedback = *feedback_;
    return true;
  }

  MOCK_METHOD4(RecordIncomingPacket,
               void(size_t, QuicPacketSequenceNumber, QuicTime, bool));

 private:
  MockClock clock_;
  QuicCongestionFeedbackFrame* feedback_;

  DISALLOW_COPY_AND_ASSIGN(TestCollector);
};

class TestConnectionHelper : public QuicConnectionHelperInterface {
 public:
  explicit TestConnectionHelper(MockClock* clock)
      : clock_(clock),
        blocked_(false) {
  }

  // QuicConnectionHelperInterface
  virtual void SetConnection(QuicConnection* connection) {}

  virtual const QuicClock* GetClock() const {
    return clock_;
  }

  virtual int WritePacketToWire(const QuicEncryptedPacket& packet,
                                int* error) {
    QuicFramer framer(QuicDecrypter::Create(kNULL),
                      QuicEncrypter::Create(kNULL));
    FramerVisitorCapturingAcks visitor;
    framer.set_visitor(&visitor);
    EXPECT_TRUE(framer.ProcessPacket(IPEndPoint(), IPEndPoint(), packet));
    header_ = *visitor.header();
    if (visitor.ack()) {
      ack_.reset(new QuicAckFrame(*visitor.ack()));
    }
    if (visitor.feedback()) {
      feedback_.reset(new QuicCongestionFeedbackFrame(*visitor.feedback()));
    }
    if (blocked_) {
      *error = ERR_IO_PENDING;
      return -1;
    }
    return packet.length();
  }

  virtual void SetResendAlarm(QuicPacketSequenceNumber sequence_number,
                              QuicTime::Delta delay) {
    resend_alarms_[sequence_number] = clock_->Now().Add(delay);
  }

  virtual void SetSendAlarm(QuicTime::Delta delay) {
    send_alarm_ = clock_->Now().Add(delay);
  }

  virtual void SetTimeoutAlarm(QuicTime::Delta delay) {
    timeout_alarm_ = clock_->Now().Add(delay);
  }

  virtual bool IsSendAlarmSet() {
    return send_alarm_ > clock_->Now();
  }

  virtual void UnregisterSendAlarmIfRegistered() {
    send_alarm_ = QuicTime();
  }

  const map<QuicPacketSequenceNumber, QuicTime>& resend_alarms() const {
    return resend_alarms_;
  }

  QuicTime timeout_alarm() const { return timeout_alarm_; }

  QuicPacketHeader* header() { return &header_; }

  QuicAckFrame* ack() { return ack_.get(); }

  QuicCongestionFeedbackFrame* feedback() { return feedback_.get(); }

  void set_blocked(bool blocked) { blocked_ = blocked; }

 private:
  MockClock* clock_;
  map<QuicPacketSequenceNumber, QuicTime> resend_alarms_;
  QuicTime send_alarm_;
  QuicTime timeout_alarm_;
  QuicPacketHeader header_;
  scoped_ptr<QuicAckFrame> ack_;
  scoped_ptr<QuicCongestionFeedbackFrame> feedback_;
  bool blocked_;

  DISALLOW_COPY_AND_ASSIGN(TestConnectionHelper);
};

class TestConnection : public QuicConnection {
 public:
  TestConnection(QuicGuid guid,
                 IPEndPoint address,
                 TestConnectionHelper* helper)
      : QuicConnection(guid, address, helper) {
  }

  void SendAck() {
    QuicConnectionPeer::SendAck(this);
  }

  void SetCollector(QuicReceiptMetricsCollector* collector) {
    QuicConnectionPeer::SetCollector(this, collector);
  }

  void SetScheduler(QuicSendScheduler* scheduler) {
    QuicConnectionPeer::SetScheduler(this, scheduler);
  }

  bool SendPacket(QuicPacketSequenceNumber sequence_number,
                  QuicPacket* packet,
                  bool should_resend,
                  bool force,
                  bool is_retransmit) {
    return QuicConnection::SendPacket(
        sequence_number, packet, should_resend, force, is_retransmit);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestConnection);
};

class QuicConnectionTest : public ::testing::Test {
 protected:
  QuicConnectionTest()
      : guid_(42),
        framer_(QuicDecrypter::Create(kNULL), QuicEncrypter::Create(kNULL)),
        creator_(guid_, &framer_),
        scheduler_(new StrictMock<MockScheduler>),
        helper_(new TestConnectionHelper(&clock_)),
        connection_(guid_, IPEndPoint(), helper_.get()),
        frame1_(1, false, 0, data1),
        frame2_(1, false, 3, data2),
        accept_packet_(true) {
    connection_.set_visitor(&visitor_);
    connection_.SetScheduler(scheduler_);
    // Simplify tests by not sending feedback unless specifically configured.
    SetFeedback(NULL);
    EXPECT_CALL(*scheduler_, TimeUntilSend(_)).WillRepeatedly(Return(
        QuicTime::Delta()));
  }

  QuicAckFrame* last_ack() {
    return helper_->ack();
  }

  QuicCongestionFeedbackFrame* last_feedback() {
    return helper_->feedback();
  }

  QuicPacketHeader* last_header() {
    return helper_->header();
  }

  void ProcessPacket(QuicPacketSequenceNumber number) {
    EXPECT_CALL(visitor_, OnPacket(_, _, _, _))
        .WillOnce(Return(accept_packet_));
    EXPECT_CALL(*scheduler_, SentPacket(_, _, _));
    ProcessDataPacket(number, 0);
  }

  void ProcessFecProtectedPacket(QuicPacketSequenceNumber number,
                                 bool expect_revival) {
    if (expect_revival) {
      EXPECT_CALL(visitor_, OnPacket(_, _, _, _)).Times(2).WillRepeatedly(
          Return(accept_packet_));
      EXPECT_CALL(*scheduler_, SentPacket(_, _, _)).Times(2);
    } else {
      EXPECT_CALL(visitor_, OnPacket(_, _, _, _)).WillOnce(
          Return(accept_packet_));
      EXPECT_CALL(*scheduler_, SentPacket(_, _, _));
    }
    ProcessDataPacket(number, 1);
  }

  void ProcessDataPacket(QuicPacketSequenceNumber number,
                         QuicFecGroupNumber fec_group) {
    scoped_ptr<QuicPacket> packet(ConstructDataPacket(number, fec_group));
    scoped_ptr<QuicEncryptedPacket> encrypted(framer_.EncryptPacket(*packet));
    connection_.ProcessUdpPacket(IPEndPoint(), IPEndPoint(), *encrypted);
  }

  // Sends an FEC packet that covers the packets that would have been sent.
  void ProcessFecPacket(QuicPacketSequenceNumber number,
                        QuicPacketSequenceNumber min_protected_packet,
                        bool expect_revival) {
    if (expect_revival) {
      EXPECT_CALL(visitor_, OnPacket(_, _, _, _)).WillOnce(
          Return(accept_packet_));
      EXPECT_CALL(*scheduler_, SentPacket(_, _, _));
    }

    // Construct the decrypted data packet so we can compute the correct
    // redundancy.
    scoped_ptr<QuicPacket> data_packet(ConstructDataPacket(number, 1));

    header_.guid = guid_;
    header_.packet_sequence_number = number;
    header_.flags = PACKET_FLAGS_FEC;
    header_.fec_group = 1;
    QuicFecData fec_data;
    fec_data.min_protected_packet_sequence_number = min_protected_packet;
    fec_data.fec_group = 1;
    // Since all data packets in this test have the same payload, the
    // redundancy is either equal to that payload or the xor of that payload
    // with itself, depending on the number of packets.
    if (((number - min_protected_packet) % 2) == 0) {
      for (size_t i = kStartOfFecProtectedData; i < data_packet->length();
           ++i) {
        data_packet->mutable_data()[i] ^= data_packet->data()[i];
      }
    }
    fec_data.redundancy = data_packet->FecProtectedData();
    QuicPacket* fec_packet;
    framer_.ConstructFecPacket(header_, fec_data, &fec_packet);
    scoped_ptr<QuicEncryptedPacket> encrypted(
        framer_.EncryptPacket(*fec_packet));

    connection_.ProcessUdpPacket(IPEndPoint(), IPEndPoint(), *encrypted);
    delete fec_packet;
  }

  void SendStreamDataToPeer(QuicStreamId id, StringPiece data,
                      QuicStreamOffset offset, bool fin,
                      QuicPacketSequenceNumber* last_packet) {
    EXPECT_CALL(*scheduler_, SentPacket(_, _, _));
    connection_.SendStreamData(id, data, offset, fin, last_packet);
  }

  void SendAckPacketToPeer() {
    EXPECT_CALL(*scheduler_, SentPacket(_, _, _)).Times(num_packets_per_ack_);
    connection_.SendAck();
  }

  void ProcessAckPacket(QuicAckFrame* frame, bool expect_success = true) {
    if (expect_success) {
      EXPECT_CALL(*scheduler_, OnIncomingAckFrame(_));
    }
    scoped_ptr<QuicPacket> packet(creator_.AckPacket(frame).second);
    scoped_ptr<QuicEncryptedPacket> encrypted(framer_.EncryptPacket(*packet));
    connection_.ProcessUdpPacket(IPEndPoint(), IPEndPoint(), *encrypted);
  }

  bool IsMissing(QuicPacketSequenceNumber number) {
    return last_ack()->received_info.IsAwaitingPacket(number);
  }

  QuicPacket* ConstructDataPacket(QuicPacketSequenceNumber number,
                                  QuicFecGroupNumber fec_group) {
    header_.guid = guid_;
    header_.packet_sequence_number = number;
    header_.flags = PACKET_FLAGS_NONE;
    header_.fec_group = fec_group;

    QuicFrames frames;
    QuicFrame frame(&frame1_);
    frames.push_back(frame);
    QuicPacket* packet = NULL;
    EXPECT_TRUE(framer_.ConstructFrameDataPacket(header_, frames, &packet));
    return packet;
  }

  void SetFeedback(QuicCongestionFeedbackFrame* feedback) {
    num_packets_per_ack_ = feedback != NULL ? 2 : 1;
    collector_ = new TestCollector(feedback);
    connection_.SetCollector(collector_);
  }

  QuicGuid guid_;
  QuicFramer framer_;
  QuicPacketCreator creator_;

  MockScheduler* scheduler_;
  TestCollector* collector_;
  MockClock clock_;
  scoped_ptr<TestConnectionHelper> helper_;
  TestConnection connection_;
  testing::StrictMock<MockConnectionVisitor> visitor_;

  QuicPacketHeader header_;
  QuicStreamFrame frame1_;
  QuicStreamFrame frame2_;
  bool accept_packet_;
  size_t num_packets_per_ack_;

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicConnectionTest);
};

TEST_F(QuicConnectionTest, PacketsInOrder) {
  ProcessPacket(1);
  EXPECT_EQ(1u, last_ack()->received_info.largest_received);
  EXPECT_EQ(0u, last_ack()->received_info.missing_packets.size());

  ProcessPacket(2);
  EXPECT_EQ(2u, last_ack()->received_info.largest_received);
  EXPECT_EQ(0u, last_ack()->received_info.missing_packets.size());

  ProcessPacket(3);
  EXPECT_EQ(3u, last_ack()->received_info.largest_received);
  EXPECT_EQ(0u, last_ack()->received_info.missing_packets.size());
}

TEST_F(QuicConnectionTest, PacketsRejected) {
  ProcessPacket(1);
  EXPECT_EQ(1u, last_ack()->received_info.largest_received);
  EXPECT_EQ(0u, last_ack()->received_info.missing_packets.size());

  accept_packet_ = false;
  ProcessPacket(2);
  // We should not have an ack for two.
  EXPECT_EQ(1u, last_ack()->received_info.largest_received);
  EXPECT_EQ(0u, last_ack()->received_info.missing_packets.size());
}

TEST_F(QuicConnectionTest, PacketsOutOfOrder) {
  ProcessPacket(3);
  EXPECT_EQ(3u, last_ack()->received_info.largest_received);
  EXPECT_TRUE(IsMissing(2));
  EXPECT_TRUE(IsMissing(1));

  ProcessPacket(2);
  EXPECT_EQ(3u, last_ack()->received_info.largest_received);
  EXPECT_FALSE(IsMissing(2));
  EXPECT_TRUE(IsMissing(1));

  ProcessPacket(1);
  EXPECT_EQ(3u, last_ack()->received_info.largest_received);
  EXPECT_FALSE(IsMissing(2));
  EXPECT_FALSE(IsMissing(1));
}

TEST_F(QuicConnectionTest, DuplicatePacket) {
  ProcessPacket(3);
  EXPECT_EQ(3u, last_ack()->received_info.largest_received);
  EXPECT_TRUE(IsMissing(2));
  EXPECT_TRUE(IsMissing(1));

  // Send packet 3 again, but do not set the expectation that
  // the visitor OnPacket() will be called.
  ProcessDataPacket(3, 0);
  EXPECT_EQ(3u, last_ack()->received_info.largest_received);
  EXPECT_TRUE(IsMissing(2));
  EXPECT_TRUE(IsMissing(1));
}

TEST_F(QuicConnectionTest, PacketsOutOfOrderWithAdditionsAndLeastAwaiting) {
  ProcessPacket(3);
  EXPECT_EQ(3u, last_ack()->received_info.largest_received);
  EXPECT_TRUE(IsMissing(2));
  EXPECT_TRUE(IsMissing(1));

  ProcessPacket(2);
  EXPECT_EQ(3u, last_ack()->received_info.largest_received);
  EXPECT_TRUE(IsMissing(1));

  ProcessPacket(5);
  EXPECT_EQ(5u, last_ack()->received_info.largest_received);
  EXPECT_TRUE(IsMissing(1));
  EXPECT_TRUE(IsMissing(4));

  // Pretend at this point the client has gotten acks for 2 and 3 and 1 is a
  // packet the peer will not retransmit.  It indicates this by sending 'least
  // awaiting' is 4.  The connection should then realize 1 will not be
  // retransmitted, and will remove it from the missing list.
  creator_.set_sequence_number(5);
  QuicAckFrame frame(0, 4);
  ProcessAckPacket(&frame);

  // Force an ack to be sent.
  SendAckPacketToPeer();
  EXPECT_TRUE(IsMissing(4));
}

TEST_F(QuicConnectionTest, RejectPacketTooFarOut) {
  // Call ProcessDataPacket rather than ProcessPacket, as we should not get a
  // packet call to the visitor.
  ProcessDataPacket(6000, 0);

  SendAckPacketToPeer();  // Packet 2
  EXPECT_EQ(0u, last_ack()->received_info.largest_received);
}

TEST_F(QuicConnectionTest, LeastUnackedLower) {
  SendStreamDataToPeer(1, "foo", 0, false, NULL);
  SendStreamDataToPeer(1, "bar", 3, false, NULL);
  SendStreamDataToPeer(1, "eep", 6, false, NULL);

  // Start out saying the least unacked is 2
  creator_.set_sequence_number(5);
  QuicAckFrame frame(0, 2);
  ProcessAckPacket(&frame);

  // Change it to 1, but lower the sequence number to fake out-of-order packets.
  // This should be fine.
  creator_.set_sequence_number(1);
  QuicAckFrame frame2(0, 1);
  // The scheduler will not process out of order acks.
  ProcessAckPacket(&frame2, false);

  // Now claim it's one, but set the ordering so it was sent "after" the first
  // one.  This should cause a connection error.
  EXPECT_CALL(visitor_, ConnectionClose(QUIC_INVALID_ACK_DATA, false));
  EXPECT_CALL(*scheduler_, SentPacket(_, _, _));
  creator_.set_sequence_number(7);
  ProcessAckPacket(&frame2, false);
}

TEST_F(QuicConnectionTest, LeastUnackedGreaterThanPacketSequenceNumber) {
  EXPECT_CALL(visitor_, ConnectionClose(QUIC_INVALID_ACK_DATA, false));
  EXPECT_CALL(*scheduler_, SentPacket(_, _, _));
  // Create an ack with least_unacked is 2 in packet number 1.
  creator_.set_sequence_number(0);
  QuicAckFrame frame(0, 2);
  ProcessAckPacket(&frame, false);
}

TEST_F(QuicConnectionTest, NackSequenceNumberGreaterThanLargestReceived) {
  SendStreamDataToPeer(1, "foo", 0, false, NULL);
  SendStreamDataToPeer(1, "bar", 3, false, NULL);
  SendStreamDataToPeer(1, "eep", 6, false, NULL);

  EXPECT_CALL(visitor_, ConnectionClose(QUIC_INVALID_ACK_DATA, false));
  EXPECT_CALL(*scheduler_, SentPacket(_, _, _));
  QuicAckFrame frame(0, 1);
  frame.received_info.missing_packets.insert(3);
  ProcessAckPacket(&frame, false);
}

TEST_F(QuicConnectionTest, AckUnsentData) {
  // Ack a packet which has not been sent.
  EXPECT_CALL(visitor_, ConnectionClose(QUIC_INVALID_ACK_DATA, false));
  EXPECT_CALL(*scheduler_, SentPacket(_, _, _));
  QuicAckFrame frame(1, 0);
  ProcessAckPacket(&frame, false);
}

TEST_F(QuicConnectionTest, AckAll) {
  ProcessPacket(1);

  creator_.set_sequence_number(1);
  QuicAckFrame frame1(1, 1);
  ProcessAckPacket(&frame1);
}

TEST_F(QuicConnectionTest, BasicSending) {
  QuicPacketSequenceNumber last_packet;
  SendStreamDataToPeer(1, "foo", 0, false, &last_packet);  // Packet 1
  EXPECT_EQ(1u, last_packet);
  SendAckPacketToPeer();  // Packet 2

  EXPECT_EQ(1u, last_ack()->sent_info.least_unacked);

  SendAckPacketToPeer();  // Packet 3
  EXPECT_EQ(1u, last_ack()->sent_info.least_unacked);

  SendStreamDataToPeer(1u, "bar", 3, false, &last_packet);  // Packet 4
  EXPECT_EQ(4u, last_packet);
  SendAckPacketToPeer();  // Packet 5
  EXPECT_EQ(1u, last_ack()->sent_info.least_unacked);

  QuicConnectionVisitorInterface::AckedPackets expected_acks;
  expected_acks.insert(1);

  // Client acks up to packet 3
  EXPECT_CALL(visitor_, OnAck(ContainerEq(expected_acks)));
  QuicAckFrame frame(3, 0);
  ProcessAckPacket(&frame);
  SendAckPacketToPeer();  // Packet 6

  // As soon as we've acked one, we skip ack packets 2 and 3 and note lack of
  // ack for 4.
  EXPECT_EQ(4u, last_ack()->sent_info.least_unacked);

  expected_acks.clear();
  expected_acks.insert(4);

  // Client acks up to packet 4, the last packet
  EXPECT_CALL(visitor_, OnAck(ContainerEq(expected_acks)));
  QuicAckFrame frame2(6, 0);
  ProcessAckPacket(&frame2);
  SendAckPacketToPeer();  // Packet 7

  // The least packet awaiting ack should now be 7
  EXPECT_EQ(7u, last_ack()->sent_info.least_unacked);

  // If we force an ack, we shouldn't change our retransmit state.
  SendAckPacketToPeer();  // Packet 8
  EXPECT_EQ(8u, last_ack()->sent_info.least_unacked);

  // But if we send more data it should.
  SendStreamDataToPeer(1, "eep", 6, false, &last_packet);  // Packet 9
  EXPECT_EQ(9u, last_packet);
  SendAckPacketToPeer();  // Packet10
  EXPECT_EQ(9u, last_ack()->sent_info.least_unacked);
}

TEST_F(QuicConnectionTest, ResendOnNack) {
  QuicPacketSequenceNumber last_packet;
  SendStreamDataToPeer(1, "foo", 0, false, &last_packet);  // Packet 1
  SendStreamDataToPeer(1, "foos", 3, false, &last_packet);  // Packet 2
  SendStreamDataToPeer(1, "fooos", 7, false, &last_packet);  // Packet 3

  QuicConnectionVisitorInterface::AckedPackets expected_acks;
  expected_acks.insert(1);
  EXPECT_CALL(visitor_, OnAck(ContainerEq(expected_acks)));

  // Client acks one but not two or three.  Right now we only resend on explicit
  // nack, so it should not trigger resend.
  QuicAckFrame ack_one(1, 0);
  ProcessAckPacket(&ack_one);
  ProcessAckPacket(&ack_one);
  ProcessAckPacket(&ack_one);

  expected_acks.clear();
  expected_acks.insert(3);
  EXPECT_CALL(visitor_, OnAck(ContainerEq(expected_acks)));

  // Client acks up to 3 with two explicitly missing.  Two nacks should cause no
  // change.
  QuicAckFrame nack_two(3, 0);
  nack_two.received_info.missing_packets.insert(2);
  ProcessAckPacket(&nack_two);
  ProcessAckPacket(&nack_two);

  // The third nack should trigger resend.
  EXPECT_CALL(*scheduler_, SentPacket(4, 37, true)).Times(1);
  ProcessAckPacket(&nack_two);
}

TEST_F(QuicConnectionTest, LimitPacketsPerNack) {
  int offset = 0;
  // Send packets 1 to 12
  for (int i = 0; i < 12; ++i) {
    SendStreamDataToPeer(1, "foo", offset, false, NULL);
    offset += 3;
  }

  // Ack 12, nack 1-11
  QuicAckFrame nack(12, 0);
  for (int i = 1; i < 12; ++i) {
    nack.received_info.missing_packets.insert(i);
  }

  QuicConnectionVisitorInterface::AckedPackets expected_acks;
  expected_acks.insert(12);
  EXPECT_CALL(visitor_, OnAck(ContainerEq(expected_acks)));

  // Nack three times.
  ProcessAckPacket(&nack);
  ProcessAckPacket(&nack);
  // The third call should trigger resending 10 packets and an updated ack.
  EXPECT_CALL(*scheduler_, SentPacket(_, _, _)).Times(11);
  ProcessAckPacket(&nack);

  // The fourth call should triggre resending the 11th packet.
  EXPECT_CALL(*scheduler_, SentPacket(_, _, _)).Times(1);
  ProcessAckPacket(&nack);
}

// Test sending multiple acks from the connection to the session.
TEST_F(QuicConnectionTest, MultipleAcks) {
  QuicPacketSequenceNumber last_packet;
  SendStreamDataToPeer(1u, "foo", 0, false, &last_packet);  // Packet 1
  EXPECT_EQ(1u, last_packet);
  SendStreamDataToPeer(3u, "foo", 0, false, &last_packet);  // Packet 2
  EXPECT_EQ(2u, last_packet);
  SendAckPacketToPeer();  // Packet 3
  SendStreamDataToPeer(5u, "foo", 0, false, &last_packet);  // Packet 4
  EXPECT_EQ(4u, last_packet);
  SendStreamDataToPeer(1u, "foo", 3, false, &last_packet);  // Packet 5
  EXPECT_EQ(5u, last_packet);
  SendStreamDataToPeer(3u, "foo", 3, false, &last_packet);  // Packet 6
  EXPECT_EQ(6u, last_packet);

  // Client will ack packets 1, [!2], 3, 4, 5
  QuicAckFrame frame1(5, 0);
  frame1.received_info.missing_packets.insert(2);

  // The connection should pass up acks for 1, 4, 5.  2 is not acked, and 3 was
  // an ackframe so should not be passed up.
  QuicConnectionVisitorInterface::AckedPackets expected_acks;
  expected_acks.insert(1);
  expected_acks.insert(4);
  expected_acks.insert(5);

  EXPECT_CALL(visitor_, OnAck(ContainerEq(expected_acks)));
  ProcessAckPacket(&frame1);

  // Now the client implicitly acks 2, and explicitly acks 6
  QuicAckFrame frame2(6, 0);
  expected_acks.clear();
  // Both acks should be passed up.
  expected_acks.insert(2);
  expected_acks.insert(6);

  EXPECT_CALL(visitor_, OnAck(ContainerEq(expected_acks)));
  ProcessAckPacket(&frame2);
}

TEST_F(QuicConnectionTest, DontLatchUnackedPacket) {
  SendStreamDataToPeer(1, "foo", 0, false, NULL);  // Packet 1;
  SendAckPacketToPeer();  // Packet 2

  // This sets least unacked to 2, the ack packet.
  QuicConnectionVisitorInterface::AckedPackets expected_acks;
  expected_acks.insert(1);
  // Client acks packet 1
  EXPECT_CALL(visitor_, OnAck(ContainerEq(expected_acks)));
  QuicAckFrame frame(1, 0);
  ProcessAckPacket(&frame);

  // Verify that our internal state has least-unacked as 2.
  QuicAckFrame* outgoing_ack = QuicConnectionPeer::GetOutgoingAck(&connection_);
  EXPECT_EQ(2u, outgoing_ack->sent_info.least_unacked);

  // When we send an ack, we make sure our least-unacked makes sense.  In this
  // case since we're not waiting on an ack for 2 and all packets are acked, we
  // set it to 3.
  SendAckPacketToPeer();  // Packet 3
  EXPECT_EQ(3u, outgoing_ack->sent_info.least_unacked);
  EXPECT_EQ(3u, last_ack()->sent_info.least_unacked);
}

TEST_F(QuicConnectionTest, ReviveMissingPacketAfterFecPacket) {
  // Don't send missing packet 1.
  ProcessFecPacket(2, 1, true);
}

TEST_F(QuicConnectionTest, ReviveMissingPacketAfterDataPacketThenFecPacket) {
  ProcessFecProtectedPacket(1, false);
  // Don't send missing packet 2.
  ProcessFecPacket(3, 1, true);
}

TEST_F(QuicConnectionTest, ReviveMissingPacketAfterDataPacketsThenFecPacket) {
  ProcessFecProtectedPacket(1, false);
  // Don't send missing packet 2.
  ProcessFecProtectedPacket(3, false);
  ProcessFecPacket(4, 1, true);
}

TEST_F(QuicConnectionTest, ReviveMissingPacketAfterDataPacket) {
  // Don't send missing packet 1.
  ProcessFecPacket(3, 1, false);  // out of order
  ProcessFecProtectedPacket(2, true);
}

TEST_F(QuicConnectionTest, ReviveMissingPacketAfterDataPackets) {
  ProcessFecProtectedPacket(1, false);
  // Don't send missing packet 2.
  ProcessFecPacket(6, 1, false);
  ProcessFecProtectedPacket(3, false);
  ProcessFecProtectedPacket(4, false);
  ProcessFecProtectedPacket(5, true);
}

TEST_F(QuicConnectionTest, TestResend) {
  // TODO(rch): make this work
  // FLAGS_fake_packet_loss_percentage = 100;
  const QuicTime::Delta kDefaultResendTime =
      QuicTime::Delta::FromMilliseconds(500);

  QuicTime default_resend_time = clock_.Now().Add(kDefaultResendTime);

  QuicAckFrame* outgoing_ack = QuicConnectionPeer::GetOutgoingAck(&connection_);
  SendStreamDataToPeer(1, "foo", 0, false, NULL);
  EXPECT_EQ(1u, outgoing_ack->sent_info.least_unacked);

  EXPECT_EQ(1u, last_header()->packet_sequence_number);
  EXPECT_EQ(1u, helper_->resend_alarms().size());
  EXPECT_EQ(default_resend_time,
            helper_->resend_alarms().find(1)->second);
  // Simulate the resend alarm firing
  clock_.AdvanceTime(kDefaultResendTime);
  EXPECT_CALL(*scheduler_, SentPacket(_, _, _));
  connection_.MaybeResendPacket(1);
  EXPECT_EQ(2u, last_header()->packet_sequence_number);
  EXPECT_EQ(2u, outgoing_ack->sent_info.least_unacked);
}

// TODO(rch): Enable after we get non-blocking sockets.
TEST_F(QuicConnectionTest, DISABLED_TestQueued) {
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  helper_->set_blocked(true);
  SendStreamDataToPeer(1, "foo", 0, false, NULL);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // Attempt to send all packets, but since we're actually still
  // blocked, they should all remain queued.
  EXPECT_FALSE(connection_.OnCanWrite());
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // Unblock the writes and actually send.
  helper_->set_blocked(false);
  EXPECT_CALL(visitor_, OnCanWrite());
  EXPECT_TRUE(connection_.OnCanWrite());
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
}

TEST_F(QuicConnectionTest, CloseFecGroup) {
  // Don't send missing packet 1
  // Don't send missing packet 2
  ProcessFecProtectedPacket(3, false);
  // Don't send missing FEC packet 3
  ASSERT_EQ(1u, connection_.NumFecGroups());

  // Now send non-fec protected ack packet and close the group
  QuicAckFrame frame(0, 5);
  creator_.set_sequence_number(4);
  ProcessAckPacket(&frame);
  ASSERT_EQ(0u, connection_.NumFecGroups());
}

TEST_F(QuicConnectionTest, NoQuicCongestionFeedbackFrame) {
  SendAckPacketToPeer();
  EXPECT_TRUE(last_feedback() == NULL);
}

TEST_F(QuicConnectionTest, WithQuicCongestionFeedbackFrame) {
  QuicCongestionFeedbackFrame info;
  info.type = kFixRate;
  info.fix_rate.bitrate_in_bytes_per_second = 123;
  SetFeedback(&info);

  SendAckPacketToPeer();
  EXPECT_EQ(kFixRate, last_feedback()->type);
  EXPECT_EQ(info.fix_rate.bitrate_in_bytes_per_second,
            last_feedback()->fix_rate.bitrate_in_bytes_per_second);
}

TEST_F(QuicConnectionTest, UpdateQuicCongestionFeedbackFrame) {
  SendAckPacketToPeer();
  EXPECT_CALL(*collector_, RecordIncomingPacket(_, _, _, _));
  ProcessPacket(1);
}

TEST_F(QuicConnectionTest, DontUpdateQuicCongestionFeedbackFrameForRevived) {
  SendAckPacketToPeer();
  // Process an FEC packet, and revive the missing data packet
  // but only contact the collector once.
  EXPECT_CALL(*collector_, RecordIncomingPacket(_, _, _, _));
  ProcessFecPacket(2, 1, true);
}

TEST_F(QuicConnectionTest, InitialTimeout) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(visitor_, ConnectionClose(QUIC_CONNECTION_TIMED_OUT, false));
  EXPECT_CALL(*scheduler_, SentPacket(_, _, _));

  QuicTime default_timeout = clock_.Now().Add(
      QuicTime::Delta::FromMicroseconds(kDefaultTimeoutUs));
  EXPECT_EQ(default_timeout, helper_->timeout_alarm());

  // Simulate the timeout alarm firing
  clock_.AdvanceTime(QuicTime::Delta::FromMicroseconds(kDefaultTimeoutUs));
  EXPECT_TRUE(connection_.CheckForTimeout());
  EXPECT_FALSE(connection_.connected());
}

TEST_F(QuicConnectionTest, TimeoutAfterSend) {
  EXPECT_TRUE(connection_.connected());

  QuicTime default_timeout = clock_.Now().Add(
      QuicTime::Delta::FromMicroseconds(kDefaultTimeoutUs));

  // When we send a packet, the timeout will change to 5000 + kDefaultTimeout.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));

  // Send an ack so we don't set the resend alarm.
  SendAckPacketToPeer();
  EXPECT_EQ(default_timeout, helper_->timeout_alarm());

  // The original alarm will fire.  We should not time out because we had a
  // network event at t=5000.  The alarm will reregister.
  clock_.AdvanceTime(QuicTime::Delta::FromMicroseconds(
      kDefaultTimeoutUs - 5000));
  EXPECT_EQ(default_timeout, clock_.Now());
  EXPECT_FALSE(connection_.CheckForTimeout());
  EXPECT_TRUE(connection_.connected());
  EXPECT_EQ(default_timeout.Add(QuicTime::Delta::FromMilliseconds(5)),
            helper_->timeout_alarm());

  // This time, we should time out.
  EXPECT_CALL(visitor_, ConnectionClose(QUIC_CONNECTION_TIMED_OUT, false));
  EXPECT_CALL(*scheduler_, SentPacket(_, _, _));
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  EXPECT_EQ(default_timeout.Add(QuicTime::Delta::FromMilliseconds(5)),
            clock_.Now());
  EXPECT_TRUE(connection_.CheckForTimeout());
  EXPECT_FALSE(connection_.connected());
}

// TODO(ianswett): Add scheduler tests when resend is false.
TEST_F(QuicConnectionTest, SendScheduler) {
  // Test that if we send a packet without delay, it is not queued.
  scoped_ptr<QuicPacket> packet(ConstructDataPacket(1, 0));
  EXPECT_CALL(*scheduler_, TimeUntilSend(true)).WillOnce(testing::Return(
      QuicTime::Delta()));
  EXPECT_CALL(*scheduler_, SentPacket(_, _, _));
  connection_.SendPacket(1, packet.get(), true, false, false);
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
}

TEST_F(QuicConnectionTest, SendSchedulerDelay) {
  // Test that if we send a packet with a delay, it ends up queued.
  scoped_ptr<QuicPacket> packet(ConstructDataPacket(1, 0));
  EXPECT_CALL(*scheduler_, TimeUntilSend(true)).WillOnce(testing::Return(
      QuicTime::Delta::FromMicroseconds(1)));
  EXPECT_CALL(*scheduler_, SentPacket(1, _, _)).Times(0);
  connection_.SendPacket(1, packet.get(), true, false, false);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());
}

TEST_F(QuicConnectionTest, SendSchedulerForce) {
  // Test that if we force send a packet, it is not queued.
  scoped_ptr<QuicPacket> packet(ConstructDataPacket(1, 0));
  EXPECT_CALL(*scheduler_, TimeUntilSend(true)).Times(0);
  EXPECT_CALL(*scheduler_, SentPacket(_, _, _));
  connection_.SendPacket(1, packet.get(), true, true, false);
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
}

// TODO(rch): Enable after we get non-blocking sockets.
TEST_F(QuicConnectionTest, DISABLED_SendSchedulerEAGAIN) {
  scoped_ptr<QuicPacket> packet(ConstructDataPacket(1, 0));
  helper_->set_blocked(true);
  EXPECT_CALL(*scheduler_, TimeUntilSend(true)).WillOnce(testing::Return(
      QuicTime::Delta()));
  EXPECT_CALL(*scheduler_, SentPacket(1, _, _)).Times(0);
  connection_.SendPacket(1, packet.get(), true, false, false);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());
}

TEST_F(QuicConnectionTest, SendSchedulerDelayThenSend) {
  // Test that if we send a packet with a delay, it ends up queued.
  scoped_ptr<QuicPacket> packet(ConstructDataPacket(1, 0));
  EXPECT_CALL(*scheduler_, TimeUntilSend(true)).WillOnce(testing::Return(
      QuicTime::Delta::FromMicroseconds(1)));
  connection_.SendPacket(1, packet.get(), true, false, false);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // Advance the clock to fire the alarm, and configure the scheduler
  // to permit the packet to be sent.
  EXPECT_CALL(*scheduler_, TimeUntilSend(true)).WillOnce(testing::Return(
      QuicTime::Delta()));
  clock_.AdvanceTime(QuicTime::Delta::FromMicroseconds(1));
  EXPECT_CALL(*scheduler_, SentPacket(_, _, _));
  EXPECT_CALL(visitor_, OnCanWrite());
  connection_.OnCanWrite();
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
}

TEST_F(QuicConnectionTest, SendSchedulerDelayThenRetransmit) {
  // Test that if we send a retransmit with a delay, it ends up queued.
  scoped_ptr<QuicPacket> packet(ConstructDataPacket(1, 0));
  EXPECT_CALL(*scheduler_, TimeUntilSend(true)).WillOnce(testing::Return(
      QuicTime::Delta::FromMicroseconds(1)));
  connection_.SendPacket(1, packet.get(), true, false, true);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // Advance the clock to fire the alarm, and configure the scheduler
  // to permit the packet to be sent.
  EXPECT_CALL(*scheduler_, TimeUntilSend(true)).WillOnce(testing::Return(
      QuicTime::Delta()));

  // Ensure the scheduler is notified this is a retransmit.
  EXPECT_CALL(*scheduler_, SentPacket(1, _, true));
  clock_.AdvanceTime(QuicTime::Delta::FromMicroseconds(1));
  EXPECT_CALL(visitor_, OnCanWrite());
  connection_.OnCanWrite();
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
}

TEST_F(QuicConnectionTest, SendSchedulerDelayAndQueue) {
  scoped_ptr<QuicPacket> packet(ConstructDataPacket(1, 0));
  EXPECT_CALL(*scheduler_, TimeUntilSend(true)).WillOnce(testing::Return(
      QuicTime::Delta::FromMicroseconds(1)));
  connection_.SendPacket(1, packet.get(), true, false, false);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // Attempt to send another packet and make sure that it gets queued.
  connection_.SendPacket(2, packet.get(), true, false, false);
  EXPECT_EQ(2u, connection_.NumQueuedPackets());
}

TEST_F(QuicConnectionTest, SendSchedulerDelayThenAckAndSend) {
  scoped_ptr<QuicPacket> packet(ConstructDataPacket(1, 0));
  EXPECT_CALL(*scheduler_, TimeUntilSend(true)).WillOnce(testing::Return(
      QuicTime::Delta::FromMicroseconds(10)));
  connection_.SendPacket(1, packet.get(), true, false, false);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // Now send non-retransmitting information, that we're not going to resend 3.
  // The far end should stop waiting for it.
  QuicAckFrame frame(0, 1);
  EXPECT_CALL(*scheduler_, TimeUntilSend(true)).WillRepeatedly(testing::Return(
      QuicTime::Delta()));
  EXPECT_CALL(*scheduler_, SentPacket(_, _, _));
  EXPECT_CALL(visitor_, OnCanWrite());
  ProcessAckPacket(&frame);

  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  // Ensure alarm is not set
  EXPECT_FALSE(helper_->IsSendAlarmSet());
}

TEST_F(QuicConnectionTest, SendSchedulerDelayThenAckAndHold) {
  scoped_ptr<QuicPacket> packet(ConstructDataPacket(1, 0));
  EXPECT_CALL(*scheduler_, TimeUntilSend(true)).WillOnce(testing::Return(
      QuicTime::Delta::FromMicroseconds(10)));
  connection_.SendPacket(1, packet.get(), true, false, false);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // Now send non-resending information, that we're not going to resend 3.
  // The far end should stop waiting for it.
  QuicAckFrame frame(0, 1);
  EXPECT_CALL(*scheduler_, TimeUntilSend(true)).WillOnce(testing::Return(
      QuicTime::Delta::FromMicroseconds(1)));
  EXPECT_CALL(visitor_, OnCanWrite());
  ProcessAckPacket(&frame);

  EXPECT_EQ(1u, connection_.NumQueuedPackets());
}

TEST_F(QuicConnectionTest, SendSchedulerDelayThenOnCanWrite) {
  scoped_ptr<QuicPacket> packet(ConstructDataPacket(1, 0));
  EXPECT_CALL(*scheduler_, TimeUntilSend(true)).WillOnce(testing::Return(
      QuicTime::Delta::FromMicroseconds(10)));
  connection_.SendPacket(1, packet.get(), true, false, false);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // OnCanWrite should not send the packet (because of the delay)
  // but should still return true.
  EXPECT_CALL(visitor_, OnCanWrite());
  EXPECT_TRUE(connection_.OnCanWrite());
  EXPECT_EQ(1u, connection_.NumQueuedPackets());
}

TEST_F(QuicConnectionTest, TestQueueLimitsOnSendStreamData) {
  // Limit to one byte per packet.
  size_t ciphertext_size = NullEncrypter().GetCiphertextSize(1);
  connection_.options()->max_packet_length =
      ciphertext_size + QuicUtils::StreamFramePacketOverhead(1);

  // Queue the first packet.
  EXPECT_CALL(*scheduler_, TimeUntilSend(true)).WillOnce(testing::Return(
      QuicTime::Delta::FromMicroseconds(10)));
  EXPECT_EQ(6u,
            connection_.SendStreamData(1, "EnoughDataToQueue", 0, false, NULL));
  EXPECT_EQ(6u, connection_.NumQueuedPackets());
}

TEST_F(QuicConnectionTest, LoopThroughSendingPackets) {
  // Limit to one byte per packet.
  size_t ciphertext_size = NullEncrypter().GetCiphertextSize(1);
  connection_.options()->max_packet_length =
      ciphertext_size + QuicUtils::StreamFramePacketOverhead(1);

  // Queue the first packet.
  EXPECT_CALL(*scheduler_, SentPacket(_, _, _)).Times(17);
  EXPECT_EQ(17u,
            connection_.SendStreamData(1, "EnoughDataToQueue", 0, false, NULL));
}

}  // namespace
}  // namespace test
}  // namespace net
