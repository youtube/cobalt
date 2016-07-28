// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/congestion_control/quic_receipt_metrics_collector.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class QuicReceiptMetricsCollectorTest : public ::testing::Test {
 protected:
  void SetUpCongestionType(CongestionFeedbackType congestion_type) {
    receiver_.reset(new QuicReceiptMetricsCollector(&clock_, congestion_type));
  }

  MockClock clock_;
  scoped_ptr<QuicReceiptMetricsCollector> receiver_;
};

TEST_F(QuicReceiptMetricsCollectorTest, FixedRateReceiverAPI) {
  SetUpCongestionType(kFixRate);
  QuicCongestionFeedbackFrame feedback;
  QuicTime timestamp;
  receiver_->RecordIncomingPacket(1, 1, timestamp, false);
  ASSERT_TRUE(receiver_->GenerateCongestionFeedback(&feedback));
  EXPECT_EQ(kFixRate, feedback.type);
}

}  // namespace net
