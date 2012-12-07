// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/congestion_control/tcp_receiver.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace testing {

class QuicTcpReceiverTest : public ::testing::Test {
 protected:
  void SetUp() {
    receiver_.reset(new TcpReceiver());
  }
  scoped_ptr<TcpReceiver> receiver_;
};

TEST_F(QuicTcpReceiverTest, SimpleReceiver) {
  CongestionInfo info;
  QuicTime timestamp;
  receiver_->RecordIncomingPacket(1, 1, timestamp, false);
  ASSERT_TRUE(receiver_->GenerateCongestionInfo(&info));
  EXPECT_EQ(kTCP, info.type);
  EXPECT_EQ(256000, info.tcp.receive_window << 4);
  EXPECT_EQ(0, info.tcp.accumulated_number_of_lost_packets);
  receiver_->RecordIncomingPacket(1, 2, timestamp, true);
  ASSERT_TRUE(receiver_->GenerateCongestionInfo(&info));
  EXPECT_EQ(kTCP, info.type);
  EXPECT_EQ(256000, info.tcp.receive_window << 4);
  EXPECT_EQ(1, info.tcp.accumulated_number_of_lost_packets);
}

}  // namespace testing
}  // namespace net
