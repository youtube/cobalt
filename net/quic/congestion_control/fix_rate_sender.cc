// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/fix_rate_sender.h"

#include <math.h>

#include "base/logging.h"
#include "net/quic/quic_protocol.h"

namespace {
  static const int kInitialBitrate = 100000;  // In bytes per second.
  static const int kNoBytesSent = 0;
}

namespace net {

FixRateSender::FixRateSender(QuicClock* clock)
    : bitrate_in_bytes_per_second_(kInitialBitrate),
      clock_(clock),
      time_last_sent_us_(0),
      bytes_last_sent_(kNoBytesSent),
      bytes_in_flight_(0) {
  DLOG(INFO) << "FixRateSender";
}

void FixRateSender::OnIncomingCongestionInfo(
    const CongestionInfo& congestion_info) {
  DCHECK(congestion_info.type == kFixRate) <<
      "Invalid incoming CongestionFeedbackType:" << congestion_info.type;
  if (congestion_info.type == kFixRate) {
    bitrate_in_bytes_per_second_ =
        congestion_info.fix_rate.bitrate_in_bytes_per_second;
  }
  // Silently ignore invalid messages in release mode.
}

void FixRateSender::OnIncomingAck(
    QuicPacketSequenceNumber /*acked_sequence_number*/,
    size_t bytes_acked, uint64 /*rtt_us*/) {
  bytes_in_flight_ -= bytes_acked;
}

void FixRateSender::OnIncomingLoss(int /*number_of_lost_packets*/) {
  // Ignore losses for fix rate sender.
}

void FixRateSender::SentPacket(QuicPacketSequenceNumber /*sequence_number*/,
                               size_t bytes,
                               bool retransmit) {
  if (bytes > 0) {
    time_last_sent_us_ = clock_->NowInUsec();
    bytes_last_sent_ = bytes;
  }
  if (!retransmit) {
    bytes_in_flight_ += bytes;
  }
}

int FixRateSender::TimeUntilSend(bool /*retransmit*/) {
  if (time_last_sent_us_ == 0 && bytes_last_sent_ == kNoBytesSent) {
    // No sent packets.
    return 0;  // We can send now.
  }
  uint64 elapsed_time_us = clock_->NowInUsec() - time_last_sent_us_;
  uint64 time_to_transmit_us = (bytes_last_sent_ * 1000000) /
      bitrate_in_bytes_per_second_;
  if (elapsed_time_us > time_to_transmit_us) {
    return 0;  // We can send now.
  }
  return time_to_transmit_us - elapsed_time_us;
}

size_t FixRateSender::AvailableCongestionWindow() {
  if (TimeUntilSend(false) > 0) {
    return 0;
  }
  // Note: Since this is for testing only we have the total window size equal to
  // kMaxPacketSize.
  return  kMaxPacketSize - bytes_in_flight_;
}

int FixRateSender::BandwidthEstimate() {
  return bitrate_in_bytes_per_second_;
}

}  // namespace net
