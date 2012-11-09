// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The pure virtual class for send side congestion control algorithm.

#ifndef NET_QUIC_CONGESTION_CONTROL_SEND_ALGORITHM_INTERFACE_H_
#define NET_QUIC_CONGESTION_CONTROL_SEND_ALGORITHM_INTERFACE_H_

#include "base/basictypes.h"
#include "net/base/net_export.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_protocol.h"

namespace net {

class NET_EXPORT_PRIVATE SendAlgorithmInterface {
 public:
  static SendAlgorithmInterface* Create(QuicClock* clock,
                                        CongestionFeedbackType type);

  virtual ~SendAlgorithmInterface() {}

  // Called when we receive congestion information from remote peer.
  virtual void OnIncomingCongestionInfo(
      const CongestionInfo& congestion_info) = 0;

  // Called for each received ACK, with sequence number from remote peer.
  virtual void OnIncomingAck(QuicPacketSequenceNumber acked_sequence_number,
                             size_t acked_bytes,
                             uint64 rtt_us) = 0;

  virtual void OnIncomingLoss(int number_of_lost_packets) = 0;

  // Inform that we sent x bytest to the wire, and if that was a retransmission.
  // Note: this function must be called for every packet sent to the wire.
  virtual void SentPacket(QuicPacketSequenceNumber sequence_number,
                          size_t bytes,
                          bool retransmit) = 0;

  // Calculate the time until we can send the next packet.
  // Usage: When this returns 0, CongestionWindow returns the number of bytes
  // of the congestion window.
  virtual int TimeUntilSend(bool retransmit) = 0;

  // The current available congestion window in bytes.
  virtual size_t AvailableCongestionWindow() = 0;

  // What's the current estimated bandwidth in bytes per second.
  virtual int BandwidthEstimate() = 0;
};

}  // namespace net

#endif  // NET_QUIC_CONGESTION_CONTROL_SEND_ALGORITHM_INTERFACE_H_
