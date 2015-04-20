// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "net/quic/congestion_control/tcp_receiver.h"

namespace net {

// Originally 64K bytes for TCP, setting it to 256K to support higher bitrates.
const size_t kReceiveWindowTCP = 256000;

TcpReceiver::TcpReceiver()
    : accumulated_number_of_recoverd_lost_packets_(0),
      receive_window_in_bytes_(kReceiveWindowTCP) {
}

bool TcpReceiver::GenerateCongestionFeedback(
    QuicCongestionFeedbackFrame* feedback) {
  feedback->type = kTCP;
  feedback->tcp.accumulated_number_of_lost_packets =
      accumulated_number_of_recoverd_lost_packets_;
  feedback->tcp.receive_window = receive_window_in_bytes_ >> 4;
  return true;
}

void TcpReceiver::RecordIncomingPacket(size_t bytes,
                                       QuicPacketSequenceNumber sequence_number,
                                       QuicTime timestamp,
                                       bool revived) {
  if (revived) {
    ++accumulated_number_of_recoverd_lost_packets_;
  }
}

}  // namespace net
