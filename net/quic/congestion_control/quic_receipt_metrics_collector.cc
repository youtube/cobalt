// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/quic_receipt_metrics_collector.h"

#include "net/quic/congestion_control/receive_algorithm_interface.h"

namespace net {

QuicReceiptMetricsCollector::QuicReceiptMetricsCollector(
    QuicClock* clock,
    CongestionFeedbackType type)
    : receive_algorithm_(ReceiveAlgorithmInterface::Create(clock, type)) {
}

QuicReceiptMetricsCollector::~QuicReceiptMetricsCollector() {
}

bool QuicReceiptMetricsCollector::GenerateCongestionInfo(
    CongestionInfo* info) {
  return receive_algorithm_->GenerateCongestionInfo(info);
}

void QuicReceiptMetricsCollector::RecordIncomingPacket(
    size_t bytes,
    QuicPacketSequenceNumber sequence_number,
    uint64 timestamp_us,
    bool revived) {
  receive_algorithm_->RecordIncomingPacket(bytes, sequence_number, timestamp_us,
                                           revived);
}

}  // namespace net
