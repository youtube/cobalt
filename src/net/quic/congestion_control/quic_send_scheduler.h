// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The is the base class for QUIC send side congestion control.
// It decides when we can send a QUIC packet to the wire.
// This class handles the basic bookkeeping of sent bitrate and packet loss.
// The actual send side algorithm is implemented via the
// SendAlgorithmInterface.

#ifndef NET_QUIC_CONGESTION_CONTROL_QUIC_SEND_SCHEDULER_H_
#define NET_QUIC_CONGESTION_CONTROL_QUIC_SEND_SCHEDULER_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"
#include "net/quic/congestion_control/quic_receipt_metrics_collector.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_time.h"

namespace net {

const uint32 kBitrateSmoothingBuckets = 300;

// 10 ms in micro seconds, must be less than 1 second for current
// implementation due to overflow resulting in a potential divide by zero.
const uint32 kBitrateSmoothingPeriod = 10000;

class NET_EXPORT_PRIVATE QuicSendScheduler {
 public:
  class PendingPacket {
   public:
    PendingPacket(size_t bytes, QuicTime timestamp)
        : bytes_sent_(bytes),
          send_timestamp_(timestamp) {
    }
    size_t BytesSent() { return bytes_sent_; }
    QuicTime& SendTimestamp() { return send_timestamp_; }

   private:
    size_t bytes_sent_;
    QuicTime send_timestamp_;
  };
  typedef std::map<QuicPacketSequenceNumber, PendingPacket*> PendingPacketsMap;

  // Enable pacing to prevent a large congestion window to be sent all at once,
  // when pacing is enabled a large congestion window will be sent in multiple
  // bursts of packet(s) instead of one big burst that might introduce packet
  // loss.
  QuicSendScheduler(const QuicClock* clock,
                    CongestionFeedbackType congestion_type);
  virtual ~QuicSendScheduler();

  // Called when we have received an ack frame from remote peer.
  virtual void OnIncomingAckFrame(const QuicAckFrame& ack_frame);

  // Called when we have received an congestion feedback frame from remote peer.
  virtual void OnIncomingQuicCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& congestion_feedback_frame);

  // Inform that we sent x bytest to the wire, and if that was a retransmission.
  // Note: this function must be called for every packet sent to the wire.
  virtual void SentPacket(QuicPacketSequenceNumber sequence_number,
                          size_t bytes,
                          bool retransmit);

  // Calculate the time until we can send the next packet to the wire.
  // Note 1: When kUnknownWaitTime is returned, there is no need to poll
  // TimeUntilSend again until we receive an OnIncomingAckFrame event.
  // Note 2: Send algorithms may or may not use |retransmit| in their
  // calculations.
  virtual QuicTime::Delta TimeUntilSend(bool retransmit);

  // Returns the current available congestion window in bytes, the number of
  // bytes that can be sent now.
  // Note: due to pacing this function might return a smaller value than the
  // real available congestion window. This way we hold off the sender to avoid
  // queuing in the lower layers in the stack.
  size_t AvailableCongestionWindow();

  int BandwidthEstimate();  // Current estimate, in bytes per second.

  int SentBandwidth();  // Current smooth acctually sent, in bytes per second.

  int PeakSustainedBandwidth();  // In bytes per second.

 private:
  // Have we sent any packets during this session?
  bool HasSentPacket();
  int UpdatePacketHistory();

  const QuicClock* clock_;
  int current_estimated_bandwidth_;
  int max_estimated_bandwidth_;
  QuicTime last_sent_packet_;
  // To keep track of the real sent bitrate we keep track of the last sent bytes
  // by keeping an array containing the number of bytes sent in a short timespan
  // kBitrateSmoothingPeriod; multiple of these buckets kBitrateSmoothingBuckets
  // create a time window in which we calculate the average bitrate.
  int current_packet_bucket_;  // Last active bucket in window.
  int first_packet_bucket_;  // First active bucket in window.
  uint32 packet_history_[kBitrateSmoothingBuckets];  // The window.
  scoped_ptr<SendAlgorithmInterface> send_algorithm_;
  // TODO(pwestin): should we combine the packet_history_ bucket with this map?
  // For now I keep it separate for easy implementation.
  PendingPacketsMap pending_packets_;
};

}  // namespace net

#endif  // NET_QUIC_CONGESTION_CONTROL_QUIC_SEND_SCHEDULER_H_
