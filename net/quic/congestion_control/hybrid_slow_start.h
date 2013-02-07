// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class is a helper class to TcpCubicSender.
// Slow start is the initial startup phase of TCP, it lasts until first packet
// loss. This class implements hybrid slow start of the TCP cubic send side
// congestion algorithm. The key feaure of hybrid slow start is that it tries to
// avoid running into the wall too hard during the slow start phase, which
// the traditional TCP implementation does.
// http://netsrv.csc.ncsu.edu/export/hybridstart_pfldnet08.pdf
// http://research.csc.ncsu.edu/netsrv/sites/default/files/hystart_techreport_2008.pdf

#ifndef NET_QUIC_CONGESTION_CONTROL_HYBRID_SLOW_START_H_
#define NET_QUIC_CONGESTION_CONTROL_HYBRID_SLOW_START_H_

#include "base/basictypes.h"
#include "net/base/net_export.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_time.h"

namespace net {

class NET_EXPORT_PRIVATE HybridSlowStart {
 public:
  explicit HybridSlowStart(const QuicClock* clock);

  // Start a new slow start phase.
  void Restart();

  // Returns true if this ack the last sequence number of our current slow start
  // round.
  // Call Reset if this returns true.
  bool EndOfRound(QuicPacketSequenceNumber ack);

  // Call for each round (burst) in the slow start phase.
  void Reset(QuicPacketSequenceNumber end_sequence_number);

  // rtt: it the RTT for this ack packet.
  // delay_min: is the lowest delay (RTT) we have seen during the session.
  void Update(QuicTime::Delta rtt, QuicTime::Delta delay_min);

  // Returns true when we should exit slow start.
  bool Exit();

  bool started() { return started_; }

 private:
  const QuicClock* clock_;
  bool started_;
  bool found_ack_train_;
  bool found_delay_;
  QuicTime round_start_;  // Beginning of each slow start round.
  QuicPacketSequenceNumber end_sequence_number_;  // End of slow start round.
  QuicTime last_time_;  // Last time when the ACK spacing was close.
  uint8 sample_count_;  // Number of samples to decide current RTT.
  QuicTime::Delta current_rtt_;  // The minimum rtt of current round.
};

}  // namespace net

#endif  // NET_QUIC_CONGESTION_CONTROL_HYBRID_SLOW_START_H_
