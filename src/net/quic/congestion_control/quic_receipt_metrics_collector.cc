// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/quic_receipt_metrics_collector.h"

#include "net/quic/congestion_control/receive_algorithm_interface.h"

namespace net {

QuicReceiptMetricsCollector::QuicReceiptMetricsCollector(
    const QuicClock* clock,
    CongestionFeedbackType type)
    : receive_algorithm_(ReceiveAlgorithmInterface::Create(clock, type)) {
}

QuicReceiptMetricsCollector::~QuicReceiptMetricsCollector() {
}

bool QuicReceiptMetricsCollector::GenerateCongestionFeedback(
    QuicCongestionFeedbackFrame* feedback) {
  return receive_algorithm_->GenerateCongestionFeedback(feedback);
}

void QuicReceiptMetricsCollector::RecordIncomingPacket(
    size_t bytes,
    QuicPacketSequenceNumber sequence_number,
    QuicTime timestamp,
    bool revived) {
  receive_algorithm_->RecordIncomingPacket(bytes, sequence_number, timestamp,
                                           revived);
}

}  // namespace net
