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
#include "net/quic/quic_time.h"
#include "net/quic/congestion_control/leaky_bucket.h"
#include "net/quic/congestion_control/paced_sender.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"

namespace net {

class NET_EXPORT_PRIVATE FixRateSender : public SendAlgorithmInterface {
 public:
  explicit FixRateSender(const QuicClock* clock);

  // Start implementation of SendAlgorithmInterface.
  virtual void OnIncomingQuicCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& feedback) override;
  virtual void OnIncomingAck(QuicPacketSequenceNumber acked_sequence_number,
                             size_t acked_bytes,
                             QuicTime::Delta rtt) override;
  virtual void OnIncomingLoss(int number_of_lost_packets) override;
  virtual void SentPacket(QuicPacketSequenceNumber equence_number,
                          size_t bytes,
                          bool retransmit) override;
  virtual QuicTime::Delta TimeUntilSend(bool retransmit) override;
  virtual size_t AvailableCongestionWindow() override;
  virtual int BandwidthEstimate() override;
  // End implementation of SendAlgorithmInterface.

 private:
  size_t CongestionWindow();

  uint32 bitrate_in_bytes_per_s_;
  LeakyBucket fix_rate_leaky_bucket_;
  PacedSender paced_sender_;
  size_t bytes_in_flight_;
};

}  // namespace net

#endif  // NET_QUIC_CONGESTION_CONTROL_FIX_RATE_SENDER_H_
