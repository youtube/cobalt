// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "net/quic/congestion_control/quic_send_scheduler.h"

#include <cmath>

#include "base/stl_util.h"
#include "base/time.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"
//#include "util/gtl/map-util.h"

namespace net {

// To prevent too aggressive pacing we allow the following packet burst size.
const int kMinPacketBurstSize = 2;
const int64 kNumMicrosPerSecond = base::Time::kMicrosecondsPerSecond;

QuicSendScheduler::QuicSendScheduler(
    QuicClock* clock,
    CongestionFeedbackType type)
    : clock_(clock),
      current_estimated_bandwidth_(-1),
      max_estimated_bandwidth_(-1),
      last_sent_packet_us_(0),
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
  uint64 now_us = clock_->NowInUsec();
  int timestamp_scaled = now_us / kBitrateSmoothingPeriod;

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
  last_sent_packet_us_ = clock_->NowInUsec();
  send_algorithm_->SentPacket(sequence_number, bytes, retransmit);
  if (!retransmit) {
    pending_packets_[sequence_number] = new PendingPacket(bytes,
                                                          last_sent_packet_us_);
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
  uint64 last_timestamp_us = 0;
  std::map<QuicPacketSequenceNumber, size_t> acked_packets;

  PendingPacketsMap::iterator it, it_upper;
  it = pending_packets_.begin();
  it_upper = pending_packets_.upper_bound(
      ack_frame.received_info.largest_received);

  while (it != it_upper) {
    QuicPacketSequenceNumber sequence_number = it->first;
    if (ack_frame.received_info.missing_packets.find(sequence_number) ==
        ack_frame.received_info.missing_packets.end()) {
      // Not missing, hence implicitly acked.
      scoped_ptr<PendingPacket> pending_packet_cleaner(it->second);
      acked_packets[sequence_number] = pending_packet_cleaner->BytesSent();
      last_timestamp_us = pending_packet_cleaner->SendTimestamp();
      pending_packets_.erase(it++);  // Must be incremented post to work.
    } else {
      ++it;
    }
  }
  // We calculate the RTT based on the highest ACKed sequence number, the lower
  // sequence numbers will include the ACK aggregation delay.
  uint64 rtt_us = clock_->NowInUsec() - last_timestamp_us;

  std::map<QuicPacketSequenceNumber, size_t>::iterator it_acked_packets;
  for (it_acked_packets = acked_packets.begin();
      it_acked_packets != acked_packets.end();
      ++it_acked_packets) {
    send_algorithm_->OnIncomingAck(it_acked_packets->first,
                                   it_acked_packets->second,
                                   rtt_us);
    DLOG(INFO) << "ACKed sequence number:" << it_acked_packets->first;
  }
}

int QuicSendScheduler::TimeUntilSend(bool retransmit) {
  return send_algorithm_->TimeUntilSend(retransmit);
}

size_t QuicSendScheduler::AvailableCongestionWindow() {
  size_t available_congestion_window =
      send_algorithm_->AvailableCongestionWindow();
  DLOG(INFO) << "Available congestion window:" << available_congestion_window;

  // Should we limit the window to pace the data?
  if (available_congestion_window > kMinPacketBurstSize * kMaxPacketSize) {
    // TODO(pwestin): implement pacing.
    // will depend on estimated bandwidth; higher bandwidth => larger burst
    // we need to consider our timing accuracy here too.
    // an accuracy of 1ms will allow us to send up to 19.2Mbit/s with 2 packets
    // per burst.
  }
  return available_congestion_window;
}

int QuicSendScheduler::BandwidthEstimate() {
  return send_algorithm_->BandwidthEstimate();
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
  max_estimated_bandwidth_ = std::max(max_estimated_bandwidth_,
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
