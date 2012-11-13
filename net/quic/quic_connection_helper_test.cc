// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_connection_helper.h"

#include <vector>

#include "base/stl_util.h"
#include "net/base/net_errors.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/test_tools/test_task_runner.h"
#include "net/socket/socket_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::map;
using testing::_;

namespace net {

class QuicConnectionPeer {
 public:
  static void SendAck(QuicConnection* connection) {
    connection->SendAck();
  }

  static void SetScheduler(QuicConnection* connection,
                           QuicSendScheduler* scheduler) {
    connection->scheduler_.reset(scheduler);
  }
};

namespace test {

const char data1[] = "foo";

class TestConnection : public QuicConnection {
 public:
  TestConnection(QuicGuid guid,
                 IPEndPoint address,
                 QuicConnectionHelper* helper)
      : QuicConnection(guid, address, helper) {
  }

  void SendAck() {
    QuicConnectionPeer::SendAck(this);
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
};

class TestConnectionHelper : public QuicConnectionHelper {
 public:
  TestConnectionHelper(base::TaskRunner* runner,
                       QuicClock* clock,
                       MockUDPClientSocket* socket)
      : QuicConnectionHelper(runner, clock, socket) {
  }

  virtual ~TestConnectionHelper() {}

  virtual int WritePacketToWire(const QuicEncryptedPacket& packet,
                                int* error) {
    QuicFramer framer(QuicDecrypter::Create(kNULL),
                      QuicEncrypter::Create(kNULL));
    FramerVisitorCapturingAcks visitor;
    framer.set_visitor(&visitor);
    EXPECT_TRUE(framer.ProcessPacket(IPEndPoint(), IPEndPoint(), packet));
    header_ = *visitor.header();
    return packet.length();
  }

  QuicPacketHeader* header() { return &header_; }

 private:
  QuicPacketHeader header_;
};

class QuicConnectionHelperTest : public ::testing::Test {
 protected:
  QuicConnectionHelperTest()
      : guid_(0),
        framer_(QuicDecrypter::Create(kNULL), QuicEncrypter::Create(kNULL)),
        creator_(guid_, &framer_),
        net_log_(BoundNetLog()),
        scheduler_(new MockScheduler()),
        socket_(&empty_data_, net_log_.net_log()),
        runner_(new TestTaskRunner(&clock_)),
        helper_(new TestConnectionHelper(runner_.get(), &clock_, &socket_)),
        connection_(guid_, IPEndPoint(), helper_),
        frame1_(1, false, 0, data1) {
    connection_.set_visitor(&visitor_);
    connection_.SetScheduler(scheduler_);
    EXPECT_CALL(*scheduler_, TimeUntilSend(_)).WillRepeatedly(testing::Return(
        QuicTime::Delta()));
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
    QuicPacket* packet;
    framer_.ConstructFrameDataPacket(header_, frames, &packet);
    return packet;
  }

  QuicGuid guid_;
  QuicFramer framer_;
  QuicPacketCreator creator_;
  QuicPacketHeader header_;

  BoundNetLog net_log_;
  StaticSocketDataProvider empty_data_;
  MockScheduler* scheduler_;
  MockUDPClientSocket socket_;
  scoped_refptr<TestTaskRunner> runner_;
  MockClock clock_;
  TestConnectionHelper* helper_;
  TestConnection connection_;
  QuicStreamFrame frame1_;
  testing::StrictMock<MockConnectionVisitor> visitor_;
};


TEST_F(QuicConnectionHelperTest, GetClock) {
  EXPECT_EQ(&clock_, helper_->GetClock());
}

TEST_F(QuicConnectionHelperTest, IsSendAlarmSet) {
  EXPECT_FALSE(helper_->IsSendAlarmSet());
}

TEST_F(QuicConnectionHelperTest, SetSendAlarm) {
  helper_->SetSendAlarm(QuicTime::Delta());
  EXPECT_TRUE(helper_->IsSendAlarmSet());
}

TEST_F(QuicConnectionHelperTest, UnregisterSendAlarmIfRegistered) {
  helper_->SetSendAlarm(QuicTime::Delta());
  helper_->UnregisterSendAlarmIfRegistered() ;
  EXPECT_FALSE(helper_->IsSendAlarmSet());
}

TEST_F(QuicConnectionHelperTest, TestResend) {
  //FLAGS_fake_packet_loss_percentage = 100;
  QuicTime::Delta kDefaultResendTime = QuicTime::Delta::FromMilliseconds(500);

  connection_.SendStreamData(1, "foo", 0, false, NULL);
  EXPECT_EQ(1u, helper_->header()->packet_sequence_number);

  QuicTime start = clock_.Now();
  runner_->RunNextTask();
  EXPECT_EQ(kDefaultResendTime, clock_.Now().Subtract(start));
  EXPECT_EQ(2u, helper_->header()->packet_sequence_number);
}

TEST_F(QuicConnectionHelperTest, InitialTimeout) {
  // Verify that a single task was posted.
  EXPECT_EQ(1u, runner_->tasks()->size());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(kDefaultTimeoutUs),
            runner_->GetTask(0).delta);

  // After we run the next task, we should close the connection.
  EXPECT_CALL(visitor_, ConnectionClose(QUIC_CONNECTION_TIMED_OUT, false));

  runner_->RunNextTask();
  EXPECT_EQ(QuicTime::FromMicroseconds(kDefaultTimeoutUs), clock_.Now());
  EXPECT_FALSE(connection_.connected());
}

TEST_F(QuicConnectionHelperTest, TimeoutAfterSend) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_EQ(0u, clock_.Now().ToMicroseconds());

  // When we send a packet, the timeout will change to 5000 + kDefaultTimeout.
  clock_.AdvanceTime(QuicTime::Delta::FromMicroseconds(5000));
  EXPECT_EQ(5000u, clock_.Now().ToMicroseconds());

  // Send an ack so we don't set the resend alarm.
  connection_.SendAck();

  // The original alarm will fire.  We should not time out because we had a
  // network event at t=5000.  The alarm will reregister.
  runner_->RunNextTask();

  EXPECT_EQ(QuicTime::FromMicroseconds(kDefaultTimeoutUs), clock_.Now());

  // This time, we should time out.
  EXPECT_CALL(visitor_, ConnectionClose(QUIC_CONNECTION_TIMED_OUT, false));
  runner_->RunNextTask();
  EXPECT_EQ(kDefaultTimeoutUs + 5000, clock_.Now().ToMicroseconds());
  EXPECT_FALSE(connection_.connected());
}

TEST_F(QuicConnectionHelperTest, SendSchedulerDelayThenSend) {
  // Test that if we send a packet with a delay, it ends up queued.
  scoped_ptr<QuicPacket> packet(ConstructDataPacket(1, 0));
  EXPECT_CALL(*scheduler_, TimeUntilSend(true)).WillOnce(testing::Return(
      QuicTime::Delta::FromMicroseconds(1)));

  bool should_resend = true;
  bool force = false;
  bool is_retransmit = false;
  connection_.SendPacket(1, packet.get(), should_resend, force, is_retransmit);
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // Advance the clock to fire the alarm, and configure the scheduler
  // to permit the packet to be sent.
  EXPECT_CALL(*scheduler_, TimeUntilSend(true)).WillOnce(testing::Return(
      QuicTime::Delta()));
  runner_->RunNextTask();
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
}

}  // namespace test

}  // namespace net
