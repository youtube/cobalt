// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "net/quic/congestion_control/quic_send_scheduler.h"

#include <algorithm>
#include <cmath>
#include <map>

#include "base/stl_util.h"
#include "base/time.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"
//#include "util/gtl/map-util.h"

using std::map;
using std::max;

namespace net {

const int64 kNumMicrosPerSecond = base::Time::kMicrosecondsPerSecond;

QuicSendScheduler::QuicSendScheduler(
    const QuicClock* clock,
    CongestionFeedbackType type)
    : clock_(clock),
      current_estimated_bandwidth_(-1),
      max_estimated_bandwidth_(-1),
      last_sent_packet_(QuicTime::FromMicroseconds(0)),
      current_packet_bucket_(-1),
      first_packet_bucket_(-1),
      send_algorithm_(SendAlgorithmInterface::Create(clock, type)) {
  memset(packet_history_, 0, sizeof(packet_history_));
}

QuicSendScheduler::~QuicSendScheduler() {
  STLDeleteContainerPairSecondPointers(pending_packets_.begin(),
                                       pending_packets_.end());
}

int QuicSendScheduler::UpdatePacketHistory() {
  int timestamp_scaled = clock_->Now().ToMicroseconds() /
      kBitrateSmoothingPeriod;
  int bucket = timestamp_scaled % kBitrateSmoothingBuckets;
  if (!HasSentPacket()) {
    // First packet.
    current_packet_bucket_ = bucket;
    first_packet_bucket_ = bucket;
  }
  if (current_packet_bucket_ != bucket) {
    // We need to make sure to zero out any skipped buckets.
    // Max loop count is kBitrateSmoothingBuckets.
    do {
      current_packet_bucket_ =
          (1 + current_packet_bucket_) % kBitrateSmoothingBuckets;
      packet_history_[current_packet_bucket_] = 0;
      if (first_packet_bucket_ == current_packet_bucket_) {
        // We have filled the whole window, no need to keep track of first
        // bucket.
        first_packet_bucket_ = -1;
      }
    }  while (current_packet_bucket_ != bucket);
  }
  return bucket;
}

void QuicSendScheduler::SentPacket(QuicPacketSequenceNumber sequence_number,
                                   size_t bytes,
                                   bool retransmit) {
  int bucket = UpdatePacketHistory();
  packet_history_[bucket] += bytes;
  send_algorithm_->SentPacket(sequence_number, bytes, retransmit);
  if (!retransmit) {
    pending_packets_[sequence_number] =
        new PendingPacket(bytes, clock_->Now());
  }
  DLOG(INFO) << "Sent sequence number:" << sequence_number;
}

void QuicSendScheduler::OnIncomingAckFrame(const QuicAckFrame& ack_frame) {
  if (ack_frame.congestion_info.type != kNone) {
    send_algorithm_->OnIncomingCongestionInfo(ack_frame.congestion_info);
  }
  // We want to.
  // * Get all packets lower(including) than largest_received
  //   from pending_packets_.
  // * Remove all missing packets.
  // * Send each ACK in the list to send_algorithm_.
  QuicTime last_timestamp(QuicTime::FromMicroseconds(0));
  map<QuicPacketSequenceNumber, size_t> acked_packets;

  PendingPacketsMap::iterator it, it_upper;
  it = pending_packets_.begin();
  it_upper = pending_packets_.upper_bound(
      ack_frame.received_info.largest_received);

  while (it != it_upper) {
    QuicPacketSequenceNumber sequence_number = it->first;
    if (!ack_frame.received_info.IsAwaitingPacket(sequence_number)) {
      // Not missing, hence implicitly acked.
      scoped_ptr<PendingPacket> pending_packet_cleaner(it->second);
      acked_packets[sequence_number] = pending_packet_cleaner->BytesSent();
      last_timestamp = pending_packet_cleaner->SendTimestamp();
      pending_packets_.erase(it++);  // Must be incremented post to work.
    } else {
      ++it;
    }
  }
  // We calculate the RTT based on the highest ACKed sequence number, the lower
  // sequence numbers will include the ACK aggregation delay.
  QuicTime::Delta rtt = clock_->Now().Subtract(last_timestamp);

  map<QuicPacketSequenceNumber, size_t>::iterator it_acked_packets;
  for (it_acked_packets = acked_packets.begin();
      it_acked_packets != acked_packets.end();
      ++it_acked_packets) {
    send_algorithm_->OnIncomingAck(it_acked_packets->first,
                                   it_acked_packets->second,
                                   rtt);
    DLOG(INFO) << "ACKed sequence number:" << it_acked_packets->first;
  }
}

QuicTime::Delta QuicSendScheduler::TimeUntilSend(bool retransmit) {
  return send_algorithm_->TimeUntilSend(retransmit);
}

size_t QuicSendScheduler::AvailableCongestionWindow() {
  return send_algorithm_->AvailableCongestionWindow();
}

int QuicSendScheduler::BandwidthEstimate() {
  int bandwidth_estimate = send_algorithm_->BandwidthEstimate();
  if (bandwidth_estimate == kNoValidEstimate) {
    // If we don't have a valid estimate use the send rate.
    return SentBandwidth();
  }
  return bandwidth_estimate;
}

bool QuicSendScheduler::HasSentPacket() {
  return (current_packet_bucket_ != -1);
}

// TODO(pwestin) add a timer to make this accurate even if not called.
int QuicSendScheduler::SentBandwidth() {
  UpdatePacketHistory();

  if (first_packet_bucket_ != -1) {
    // We don't have a full set of data.
    int number_of_buckets = (current_packet_bucket_ - first_packet_bucket_) + 1;
    if (number_of_buckets < 0) {
      // We have a wrap in bucket index.
      number_of_buckets = kBitrateSmoothingBuckets + number_of_buckets;
    }
    int64 sum = 0;
    int bucket = first_packet_bucket_;
    for (int n = 0; n < number_of_buckets; bucket++, n++) {
      bucket = bucket % kBitrateSmoothingBuckets;
      sum += packet_history_[bucket];
    }
    current_estimated_bandwidth_ = (sum *
        (kNumMicrosPerSecond / kBitrateSmoothingPeriod)) / number_of_buckets;
  } else {
    int64 sum = 0;
    for (uint32 bucket = 0; bucket < kBitrateSmoothingBuckets; ++bucket) {
      sum += packet_history_[bucket];
    }
    current_estimated_bandwidth_ = (sum * (kNumMicrosPerSecond /
        kBitrateSmoothingPeriod)) / kBitrateSmoothingBuckets;
  }
  max_estimated_bandwidth_ = max(max_estimated_bandwidth_,
                                 current_estimated_bandwidth_);
  return current_estimated_bandwidth_;
}

int QuicSendScheduler::PeakSustainedBandwidth() {
  // To make sure that we get the latest estimate we call SentBandwidth.
  if (HasSentPacket()) {
    SentBandwidth();
  }
  return max_estimated_bandwidth_;
}

}  // namespace net
