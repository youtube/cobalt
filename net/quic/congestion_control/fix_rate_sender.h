// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Fix rate send side congestion control, used for testing.

#ifndef NET_QUIC_CONGESTION_CONTROL_FIX_RATE_SENDER_H_
#define NET_QUIC_CONGESTION_CONTROL_FIX_RATE_SENDER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/base/net_export.h"
#include "net/quic/quic_clock.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"

namespace net {

class NET_EXPORT_PRIVATE FixRateSender : public SendAlgorithmInterface {
 public:
  explicit FixRateSender(QuicClock* clock);

  // Start implementation of SendAlgorithmInterface.
  virtual void OnIncomingCongestionInfo(
      const CongestionInfo& congestion_info) OVERRIDE;
  virtual void OnIncomingAck(QuicPacketSequenceNumber acked_sequence_number,
                             size_t acked_bytes,
                             uint64 rtt_us) OVERRIDE;
  virtual void OnIncomingLoss(int number_of_lost_packets) OVERRIDE;
  virtual void SentPacket(QuicPacketSequenceNumber equence_number,
                          size_t bytes, bool retransmit) OVERRIDE;
  virtual int TimeUntilSend(bool retransmit) OVERRIDE;
  virtual size_t AvailableCongestionWindow() OVERRIDE;
  virtual int BandwidthEstimate() OVERRIDE;
  // End implementation of SendAlgorithmInterface.

 private:
  uint32 bitrate_in_bytes_per_second_;
  QuicClock* clock_;
  uint64 time_last_sent_us_;
  int bytes_last_sent_;
  size_t bytes_in_flight_;
};

}  // namespace net

#endif  // NET_QUIC_CONGESTION_CONTROL_FIX_RATE_SENDER_H_
