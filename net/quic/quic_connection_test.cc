// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_connection.h"

#include "net/base/net_errors.h"
#include "net/quic/congestion_control/quic_receipt_metrics_collector.h"
#include "net/quic/congestion_control/quic_send_scheduler.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

//DECLARE_int32(fake_packet_loss_percentage);

using std::map;
using testing::_;
using testing::ContainerEq;
using testing::Return;

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
  DISALLOW_COPY_AND_ASSIGN(QuicConnectionPeer);
};

namespace test {
namespace {

const char data1[] = "foo";
const char data2[] = "bar";

class TestCollector : public QuicReceiptMetricsCollector {
 public:
  explicit TestCollector(CongestionInfo* info)
      : QuicReceiptMetricsCollector(&clock_, kFixRate),
        info_(info) {
  }

  bool GenerateCongestionInfo(CongestionInfo* congestion_info) {
    if (info_ == NULL) {
      return false;
    }
    *congestion_info = *info_;
    return true;
  }

  MOCK_METHOD4(RecordIncomingPacket,
               void(size_t, QuicPacketSequenceNumber, QuicTime, bool));

 private:
  MockClock clock_;
  CongestionInfo* info_;

  DISALLOW_COPY_AND_ASSIGN(TestCollector);
};

class TestConnectionHelper : public QuicConnectionHelperInterface {
 public:
  TestConnectionHelper(MockClock* clock)
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
    frame_ = *visitor.frame();
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

  QuicAckFrame* frame() { return &frame_; }

  void set_blocked(bool blocked) { blocked_ = blocked; }

 private:
  MockClock* clock_;
  map<QuicPacketSequenceNumber, QuicTime> resend_alarms_;
  QuicTime send_alarm_;
  QuicTime timeout_alarm_;
  QuicPacketHeader header_;
  QuicAckFrame frame_;
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
        scheduler_(new MockScheduler()),
        helper_(new TestConnectionHelper(&clock_)),
        connection_(guid_, IPEndPoint(), helper_),
        frame1_(1, false, 0, data1),
        frame2_(1, false, 3, data2),
        accept_packet_(true) {
    connection_.set_visitor(&visitor_);
    connection_.SetScheduler(scheduler_);
    EXPECT_CALL(*scheduler_, TimeUntilSend(_)).WillRepeatedly(Return(
        QuicTime::Delta()));
  }

  QuicAckFrame* last_frame() {
    return helper_->frame();
  }

  QuicPacketHeader* last_header() {
    return helper_->header();
  }

  void ProcessPacket(QuicPacketSequenceNumber number) {
    EXPECT_CALL(visitor_, OnPacket(_, _, _, _))
        .WillOnce(Return(accept_packet_));
    ProcessDataPacket(number, 0);
  }

  void ProcessFecProtectedPacket(QuicPacketSequenceNumber number,
                                 bool expect_revival) {
    if (expect_revival) {
      EXPECT_CALL(visitor_, OnPacket(_, _, _, _)).Times(2).WillRepeatedly(
          Return(accept_packet_));
    } else {
      EXPECT_CALL(visitor_, OnPacket(_, _, _, _)).WillOnce(
          Return(accept_packet_));
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


  void SendAckPacket(QuicAckFrame* frame) {
    scoped_ptr<QuicPacket> packet(creator_.AckPacket(frame).second);
    scoped_ptr<QuicEncryptedPacket> encrypted(framer_.EncryptPacket(*packet));
    connection_.ProcessUdpPacket(IPEndPoint(), IPEndPoint(), *encrypted);
  }

  void SendAckPacket(QuicPacketSequenceNumber least_unacked) {
    QuicAckFrame frame(0, QuicTime(), least_unacked);
    SendAckPacket(&frame);
  }

  bool IsMissing(QuicPacketSequenceNumber number) {
    return last_frame()->received_info.missing_packets.find(number) !=
        last_frame()->received_info.missing_packets.end();
  }

  size_t NonRetransmittingSize() {
    return last_frame()->sent_info.non_retransmiting.size();
  }

  bool NonRetransmitting(QuicPacketSequenceNumber number) {
    return last_frame()->sent_info.non_retransmiting.find(number) !=
        last_frame()->sent_info.non_retransmiting.end();
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

  QuicGuid guid_;
  QuicFramer framer_;
  QuicPacketCreator creator_;

  MockScheduler* scheduler_;
  MockClock clock_;
  TestConnectionHelper* helper_;
  TestConnection connection_;
  testing::StrictMock<MockConnectionVisitor> visitor_;

  QuicPacketHeader header_;
  QuicStreamFrame frame1_;
  QuicStreamFrame frame2_;
  bool accept_packet_;

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicConnectionTest);
};

TEST_F(QuicConnectionTest, PacketsInOrder) {
  ProcessPacket(1);
  EXPECT_EQ(1u, last_frame()->received_info.largest_received);
  EXPECT_EQ(0u, last_frame()->received_info.missing_packets.size());

  ProcessPacket(2);
  EXPECT_EQ(2u, last_frame()->received_info.largest_received);
  EXPECT_EQ(0u, last_frame()->received_info.missing_packets.size());

  ProcessPacket(3);
  EXPECT_EQ(3u, last_frame()->received_info.largest_received);
  EXPECT_EQ(0u, last_frame()->received_info.missing_packets.size());
}

TEST_F(QuicConnectionTest, PacketsRejected) {
  ProcessPacket(1);
  EXPECT_EQ(1u, last_frame()->received_info.largest_received);
  EXPECT_EQ(0u, last_frame()->received_info.missing_packets.size());

  accept_packet_ = false;
  ProcessPacket(2);
  // We should not have an ack for two.
  EXPECT_EQ(1u, last_frame()->received_info.largest_received);
  EXPECT_EQ(0u, last_frame()->received_info.missing_packets.size());
}

TEST_F(QuicConnectionTest, PacketsOutOfOrder) {
  ProcessPacket(3);
  EXPECT_EQ(3u, last_frame()->received_info.largest_received);
  EXPECT_EQ(2u, last_frame()->received_info.missing_packets.size());
  EXPECT_TRUE(IsMissing(2));
  EXPECT_TRUE(IsMissing(1));

  ProcessPacket(2);
  EXPECT_EQ(3u, last_frame()->received_info.largest_received);
  EXPECT_EQ(1u, last_frame()->received_info.missing_packets.size());
  EXPECT_TRUE(IsMissing(1));

  ProcessPacket(1);
  EXPECT_EQ(3u, last_frame()->received_info.largest_received);
  EXPECT_EQ(0u, last_frame()->received_info.missing_packets.size());
}

TEST_F(QuicConnectionTest, DuplicatePacket) {
  ProcessPacket(3);
  EXPECT_EQ(3u, last_frame()->received_info.largest_received);
  EXPECT_EQ(2u, last_frame()->received_info.missing_packets.size());
  EXPECT_TRUE(IsMissing(2));
  EXPECT_TRUE(IsMissing(1));

  // Send packet 3 again, but do not set the expectation that
  // the visitor OnPacket() will be called.
  ProcessDataPacket(3, 0);
  EXPECT_EQ(3u, last_frame()->received_info.largest_received);
  EXPECT_EQ(2u, last_frame()->received_info.missing_packets.size());
  EXPECT_TRUE(IsMissing(2));
  EXPECT_TRUE(IsMissing(1));
}

TEST_F(QuicConnectionTest, LatePacketMarkedWillNotResend) {
  ProcessPacket(5);
  // Now send non-resending information, that we're not going to resend 3.
  // The far end should stop waiting for it.
  QuicPacketSequenceNumber largest_received = 0;
  QuicTime time_received;
  QuicPacketSequenceNumber least_unacked = 1;
  QuicAckFrame frame(largest_received, time_received, least_unacked);
  frame.sent_info.non_retransmiting.insert(3);
  SendAckPacket(&frame);
  // Force an ack to be sent.
  connection_.SendAck();
  EXPECT_EQ(5u, last_frame()->received_info.largest_received);
  EXPECT_EQ(2u, last_frame()->received_info.missing_packets.size());
  EXPECT_TRUE(IsMissing(4));
  EXPECT_TRUE(IsMissing(2));

  // Send packet 3 again, but do not set the expectation that
  // the visitor OnPacket() will be called.
  ProcessDataPacket(3, 0);
  connection_.SendAck();
  EXPECT_EQ(5u, last_frame()->received_info.largest_received);
  EXPECT_EQ(2u, last_frame()->received_info.missing_packets.size());
  EXPECT_TRUE(IsMissing(4));
  EXPECT_TRUE(IsMissing(2));
}

TEST_F(QuicConnectionTest, PacketsOutOfOrderWithAdditionsAndNonResend) {
  ProcessPacket(3);
  EXPECT_EQ(3u, last_frame()->received_info.largest_received);
  EXPECT_EQ(2u, last_frame()->received_info.missing_packets.size());
  EXPECT_TRUE(IsMissing(2));
  EXPECT_TRUE(IsMissing(1));

  ProcessPacket(2);
  EXPECT_EQ(3u, last_frame()->received_info.largest_received);
  EXPECT_EQ(1u, last_frame()->received_info.missing_packets.size());
  EXPECT_TRUE(IsMissing(1));

  ProcessPacket(6);
  EXPECT_EQ(6u, last_frame()->received_info.largest_received);
  EXPECT_EQ(3u, last_frame()->received_info.missing_packets.size());
  EXPECT_TRUE(IsMissing(1));
  EXPECT_TRUE(IsMissing(4));
  EXPECT_TRUE(IsMissing(5));

  // Now send non-resending information, that we're not going to resend 4.
  // The far end should stop waiting for it.
  // In sending the ack, we also have sent packet 1, so we'll stop waiting for
  // that as well.
  QuicAckFrame frame(0, QuicTime(), 1);
  frame.sent_info.non_retransmiting.insert(4);
  SendAckPacket(&frame);
  // Force an ack to be sent.
  connection_.SendAck();
  EXPECT_EQ(1u, last_frame()->received_info.missing_packets.size());
  EXPECT_TRUE(IsMissing(5));
}

TEST_F(QuicConnectionTest, PacketsOutOfOrderWithAdditionsAndLeastAwaiting) {
  ProcessPacket(3);
  EXPECT_EQ(3u, last_frame()->received_info.largest_received);
  EXPECT_EQ(2u, last_frame()->received_info.missing_packets.size());
  EXPECT_TRUE(IsMissing(2));
  EXPECT_TRUE(IsMissing(1));

  ProcessPacket(2);
  EXPECT_EQ(3u, last_frame()->received_info.largest_received);
  EXPECT_EQ(1u, last_frame()->received_info.missing_packets.size());
  EXPECT_TRUE(IsMissing(1));

  ProcessPacket(5);
  EXPECT_EQ(5u, last_frame()->received_info.largest_received);
  EXPECT_EQ(2u, last_frame()->received_info.missing_packets.size());
  EXPECT_TRUE(IsMissing(1));
  EXPECT_TRUE(IsMissing(4));

  // Pretend at this point the client has gotten acks for 2 and 3 and 1 is a
  // packet the peer will not retransmit.  It indicates this by sending 'least
  // awaiting' is 4.  The connection should then realize 1 will not be
  // retransmitted, and will remove it from the missing list.
  QuicAckFrame frame(0, QuicTime(), 4);
  SendAckPacket(&frame);
  // Force an ack to be sent.
  connection_.SendAck();
  EXPECT_EQ(1u, last_frame()->received_info.missing_packets.size());
  EXPECT_TRUE(IsMissing(4));
}

TEST_F(QuicConnectionTest, RejectPacketTooFarOut) {
  // Call ProcessDataPacket rather than ProcessPacket, as we should not get a
  // packet call to the visitor.
  ProcessDataPacket(6000, 0);;

  connection_.SendAck();  // Packet 2
  EXPECT_EQ(0u, last_frame()->received_info.largest_received);
}

TEST_F(QuicConnectionTest, LeastUnackedLower) {
  connection_.SendStreamData(1, "foo", 0, false, NULL);
  connection_.SendStreamData(1, "bar", 3, false, NULL);
  connection_.SendStreamData(1, "eep", 6, false, NULL);

  // Start out saying the least unacked is 2
  creator_.set_sequence_number(5);
  QuicAckFrame frame(0, QuicTime(), 2);
  SendAckPacket(&frame);

  // Change it to 1, but lower the sequence number to fake out-of-order packets.
  // This should be fine.
  creator_.set_sequence_number(1);
  QuicAckFrame frame2(0, QuicTime(), 1);
  SendAckPacket(&frame2);

  // Now claim it's one, but set the ordering so it was sent "after" the first
  // one.  This should cause a connection error.
  EXPECT_CALL(visitor_, ConnectionClose(QUIC_INVALID_ACK_DATA, false));
  creator_.set_sequence_number(7);
  SendAckPacket(&frame2);
}

TEST_F(QuicConnectionTest, AckUnsentData) {
  // Ack a packet which has not been sent.
  EXPECT_CALL(visitor_, ConnectionClose(QUIC_INVALID_ACK_DATA, false));
  QuicAckFrame frame(1, QuicTime(), 0);
  SendAckPacket(&frame);
}

TEST_F(QuicConnectionTest, AckAll) {
  ProcessPacket(1);

  creator_.set_sequence_number(1);
  QuicAckFrame frame1(1, QuicTime(), 1);
  SendAckPacket(&frame1);

  // Send an ack with least_unacked == 0, which indicates that all packets
  // we have sent have been acked.
  QuicAckFrame frame2(1, QuicTime(), 0);
  SendAckPacket(&frame2);
}

// This test is meant to validate that we can't overwhelm the far end with a ton
// of missing packets.
// We will likely fix the protocol to allow more than 190 in flight, and the
// test will need to be adjusted accordingly.
TEST_F(QuicConnectionTest, TooManyMissing) {
  connection_.SendStreamData(1, "foo", 0, false, NULL);

  EXPECT_CALL(visitor_, ConnectionClose(QUIC_PACKET_TOO_LARGE, false));
  QuicAckFrame frame(1, QuicTime(), 0);
  for (int i = 0; i < 5001; ++i) {
    frame.received_info.missing_packets.insert(i);
  }
  SendAckPacket(&frame);
}

// See comment for TooManyMissing above.
TEST_F(QuicConnectionTest, TooManyNonRetransmitting) {
  connection_.SendStreamData(1, "foo", 0, false, NULL);

  EXPECT_CALL(visitor_, ConnectionClose(QUIC_PACKET_TOO_LARGE, false));
  QuicAckFrame frame(1, QuicTime(), 0);
  for (int i = 0; i < 5001; ++i) {
    frame.sent_info.non_retransmiting.insert(i);
  }
  SendAckPacket(&frame);
}

TEST_F(QuicConnectionTest, BasicSending) {
  QuicPacketSequenceNumber last_packet;
  connection_.SendStreamData(1, "foo", 0, false, &last_packet);  // Packet 1
  EXPECT_EQ(1u, last_packet);
  connection_.SendAck();  // Packet 2

  EXPECT_EQ(1u, last_frame()->sent_info.least_unacked);

  connection_.SendAck();  // Packet 3
  EXPECT_EQ(1u, last_frame()->sent_info.least_unacked);
  EXPECT_EQ(1u, NonRetransmittingSize());
  EXPECT_TRUE(NonRetransmitting(2));

  connection_.SendStreamData(1, "bar", 3, false, &last_packet);  // Packet 4
  EXPECT_EQ(4u, last_packet);
  connection_.SendAck();  // Packet 5
  EXPECT_EQ(1u, last_frame()->sent_info.least_unacked);
  EXPECT_EQ(2u, NonRetransmittingSize());
  EXPECT_TRUE(NonRetransmitting(2));
  EXPECT_TRUE(NonRetransmitting(3));

  QuicConnectionVisitorInterface::AckedPackets expected_acks;
  expected_acks.insert(1);

  // Client acks up to packet 3
  EXPECT_CALL(visitor_, OnAck(ContainerEq(expected_acks)));
  QuicAckFrame frame(3, QuicTime(), 0);
  SendAckPacket(&frame);
  connection_.SendAck();  // Packet 6

  // As soon as we've acked one, we skip ack packets 2 and 3 and note lack of
  // ack for 4.
  EXPECT_EQ(4u, last_frame()->sent_info.least_unacked);
  EXPECT_EQ(1u, NonRetransmittingSize());
  EXPECT_TRUE(NonRetransmitting(5));

  expected_acks.clear();
  expected_acks.insert(4);

  // Client acks up to packet 4, the last packet
  EXPECT_CALL(visitor_, OnAck(ContainerEq(expected_acks)));
  QuicAckFrame frame2(6, QuicTime(), 0);
  SendAckPacket(&frame2);
  connection_.SendAck();  // Packet 7

  // The least packet awaiting ack should now be the special value of 0
  EXPECT_EQ(0u, last_frame()->sent_info.least_unacked);
  EXPECT_EQ(0u, NonRetransmittingSize());

  // If we force an ack, we shouldn't change our retransmit state.
  connection_.SendAck();  // Packet 8
  EXPECT_EQ(0u, last_frame()->sent_info.least_unacked);
  EXPECT_EQ(0u, NonRetransmittingSize());

  // But if we send more data it should.
  connection_.SendStreamData(1, "eep", 6, false, &last_packet);  // Packet 9
  EXPECT_EQ(9u, last_packet);
  connection_.SendAck();  // Packet10
  EXPECT_EQ(9u, last_frame()->sent_info.least_unacked);
}

// Test sending multiple acks from the connection to the session.
TEST_F(QuicConnectionTest, MultipleAcks) {
  QuicPacketSequenceNumber last_packet;
  connection_.SendStreamData(1, "foo", 0, false, &last_packet);  // Packet 1
  EXPECT_EQ(1u, last_packet);
  connection_.SendStreamData(3, "foo", 0, false, &last_packet);  // Packet 2
  EXPECT_EQ(2u, last_packet);
  connection_.SendAck();  // Packet 3
  connection_.SendStreamData(5, "foo", 0, false, &last_packet);  // Packet 4
  EXPECT_EQ(4u, last_packet);
  connection_.SendStreamData(1, "foo", 3, false, &last_packet);  // Packet 5
  EXPECT_EQ(5u, last_packet);
  connection_.SendStreamData(3, "foo", 3, false, &last_packet);  // Packet 6
  EXPECT_EQ(6u, last_packet);

  // Client will acks packets 1, [!2], 3, 4, 5
  QuicAckFrame frame1(5, QuicTime(), 0);
  frame1.received_info.missing_packets.insert(2);

  // The connection should pass up acks for 1, 4, 5.  2 is not acked, and 3 was
  // an ackframe so should not be passed up.
  QuicConnectionVisitorInterface::AckedPackets expected_acks;
  expected_acks.insert(1);
  expected_acks.insert(4);
  expected_acks.insert(5);

  EXPECT_CALL(visitor_, OnAck(ContainerEq(expected_acks)));
  SendAckPacket(&frame1);

  // Now the client implicitly acks 2, and explicitly acks 6
  QuicAckFrame frame2(6, QuicTime(), 0);
  expected_acks.clear();
  // Both acks should be passed up.
  expected_acks.insert(2);
  expected_acks.insert(6);

  EXPECT_CALL(visitor_, OnAck(ContainerEq(expected_acks)));
  SendAckPacket(&frame2);
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

  connection_.SendStreamData(1, "foo", 0, false, NULL);
  EXPECT_EQ(1u, last_header()->packet_sequence_number);
  EXPECT_EQ(1u, helper_->resend_alarms().size());
  EXPECT_EQ(default_resend_time,
            helper_->resend_alarms().find(1)->second);
  // Simulate the resend alarm firing
  clock_.AdvanceTime(kDefaultResendTime);
  connection_.MaybeResendPacket(1);
  EXPECT_EQ(2u, last_header()->packet_sequence_number);
}

// TODO(rch): Enable after we get non-blocking sockets.
TEST_F(QuicConnectionTest, DISABLED_TestQueued) {
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
  helper_->set_blocked(true);
  connection_.SendStreamData(1, "foo", 0, false, NULL);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // Attempt to send all packets, but since we're actually still
  // blocked, they should all remain queued.
  EXPECT_FALSE(connection_.OnCanWrite());
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // Unblock the writes and actually send.
  helper_->set_blocked(false);
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
  SendAckPacket(5);
  ASSERT_EQ(0u, connection_.NumFecGroups());
}

TEST_F(QuicConnectionTest, NoCongestionInfo) {
  TestCollector* collector(new TestCollector(NULL));
  connection_.SetCollector(collector);
  connection_.SendAck();
  EXPECT_EQ(kNone, last_frame()->congestion_info.type);
}

TEST_F(QuicConnectionTest, WithCongestionInfo) {
  CongestionInfo info;
  info.type = kFixRate;
  info.fix_rate.bitrate_in_bytes_per_second = 123;
  TestCollector* collector(new TestCollector(&info));
  connection_.SetCollector(collector);
  connection_.SendAck();
  EXPECT_EQ(kFixRate, last_frame()->congestion_info.type);
  EXPECT_EQ(info.fix_rate.bitrate_in_bytes_per_second,
      last_frame()->congestion_info.fix_rate.bitrate_in_bytes_per_second);
}

TEST_F(QuicConnectionTest, UpdateCongestionInfo) {
  TestCollector* collector(new TestCollector(NULL));
  connection_.SetCollector(collector);
  connection_.SendAck();
  EXPECT_CALL(*collector, RecordIncomingPacket(_, _, _, _));
  ProcessPacket(1);
}

TEST_F(QuicConnectionTest, DontUpdateCongestionInfoForRevived) {
  TestCollector* collector(new TestCollector(NULL));
  connection_.SetCollector(collector);
  connection_.SendAck();
  // Process an FEC packet, and revive the missing data packet
  // but only contact the collector once.
  EXPECT_CALL(*collector, RecordIncomingPacket(_, _, _, _));
  ProcessFecPacket(2, 1, true);
}

TEST_F(QuicConnectionTest, InitialTimeout) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_CALL(visitor_, ConnectionClose(QUIC_CONNECTION_TIMED_OUT, false));

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
  connection_.SendAck();
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
  QuicAckFrame frame(0, QuicTime(), 1);
  frame.sent_info.non_retransmiting.insert(3);
  EXPECT_CALL(*scheduler_, OnIncomingAckFrame(testing::_));
  EXPECT_CALL(*scheduler_, TimeUntilSend(true)).WillRepeatedly(testing::Return(
      QuicTime::Delta()));
  SendAckPacket(&frame);

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
  QuicAckFrame frame(0, QuicTime(), 1);
  frame.sent_info.non_retransmiting.insert(3);
  EXPECT_CALL(*scheduler_, OnIncomingAckFrame(testing::_));
  EXPECT_CALL(*scheduler_, TimeUntilSend(true)).WillOnce(testing::Return(
      QuicTime::Delta::FromMicroseconds(1)));
  SendAckPacket(&frame);

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
  EXPECT_TRUE(connection_.OnCanWrite());
  EXPECT_EQ(1u, connection_.NumQueuedPackets());
}

}  // namespace
}  // namespace test
}  // namespace net
