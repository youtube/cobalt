// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TCP receiver side congestion algorithm, emulates the behaviour of TCP.

#ifndef NET_QUIC_CONGESTION_CONTROL_TCP_RECEIVER_H_
#define NET_QUIC_CONGESTION_CONTROL_TCP_RECEIVER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/base/net_export.h"
#include "net/quic/congestion_control/receive_algorithm_interface.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_protocol.h"

namespace net {

class NET_EXPORT_PRIVATE TcpReceiver : public ReceiveAlgorithmInterface {
 public:
  TcpReceiver();

  // Start implementation of SendAlgorithmInterface.
  virtual bool GenerateCongestionFeedback(
      QuicCongestionFeedbackFrame* feedback) override;

  virtual void RecordIncomingPacket(size_t bytes,
                                    QuicPacketSequenceNumber sequence_number,
                                    QuicTime timestamp,
                                    bool revived) override;

 private:
  // We need to keep track of FEC recovered packets.
  int accumulated_number_of_recoverd_lost_packets_;
  uint32 receive_window_in_bytes_;
};

}  // namespace net

#endif  // NET_QUIC_CONGESTION_CONTROL_TCP_RECEIVER_H_
