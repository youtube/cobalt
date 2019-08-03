// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/hybrid_slow_start.h"

namespace net {

// Note(pwestin): the magic clamping numbers come from the original code in
// tcp_cubic.c.
// Number of delay samples for detecting the increase of delay.
const int kHybridStartMinSamples = 8;
const int kHybridStartDelayFactorExp = 4;  // 2^4 = 16
const int kHybridStartDelayMinThresholdUs = 2000;
const int kHybridStartDelayMaxThresholdUs = 16000;

HybridSlowStart::HybridSlowStart(const QuicClock* clock)
    : clock_(clock),
      started_(false),
      found_ack_train_(false),
      found_delay_(false),
      end_sequence_number_(0),
      sample_count_(0) {
}

void HybridSlowStart::Restart() {
  found_ack_train_ = false;
  found_delay_  = false;
}

void HybridSlowStart::Reset(QuicPacketSequenceNumber end_sequence_number) {
  DLOG(INFO) << "Reset hybrid slow start @" << end_sequence_number;
  round_start_ = last_time_ = clock_->Now();
  end_sequence_number_ = end_sequence_number;
  current_rtt_ = QuicTime::Delta();  // Reset to 0.
  sample_count_ = 0;
  started_ = true;
}

bool HybridSlowStart::EndOfRound(QuicPacketSequenceNumber ack) {
  // TODO(pwestin): do we need to handle wraparound?
  return end_sequence_number_ <= ack;
}

void HybridSlowStart::Update(QuicTime::Delta rtt, QuicTime::Delta delay_min) {
  // The original code doesn't invoke this until we hit 16 packet per burst.
  // Since the code handles lower than 16 grecefully and I removed that
  // limit.
  if (found_ack_train_ || found_delay_) {
    return;
  }
  QuicTime current_time = clock_->Now();

  // First detection parameter - ack-train detection.
  // Since slow start burst out packets we can indirectly estimate the inter-
  // arrival time by looking at the arrival time of the ACKs if the ACKs are
  // spread out more then half the minimum RTT packets are beeing spread out
  // more than the capacity.
  // This first trigger will not come into play until we hit roughly 4.8 Mbit/s.
  // TODO(pwestin): we need to make sure our pacing don't trigger this detector.
  if (current_time.Subtract(last_time_).ToMicroseconds() <=
      kHybridStartDelayMinThresholdUs) {
    last_time_ = current_time;
    if (current_time.Subtract(round_start_).ToMicroseconds() >=
        (delay_min.ToMicroseconds() >> 1)) {
      found_ack_train_ = true;
    }
  }
  // Second detection parameter - delay increase detection.
  // Compare the minimum delay (current_rtt_) of the current
  // burst of packets relative to the minimum delay during the session.
  // Note: we only look at the first few(8) packets in each burst, since we
  // only want to compare the lowest RTT of the burst relative to previous
  // bursts.
  sample_count_++;
  if (sample_count_ <= kHybridStartMinSamples) {
    if (current_rtt_.IsZero() || current_rtt_ > rtt) {
      current_rtt_ = rtt;
    }
  }
  // We only need to check this once.
  if (sample_count_ == kHybridStartMinSamples) {
    int accepted_variance_us = delay_min.ToMicroseconds() >>
        kHybridStartDelayFactorExp;
    accepted_variance_us = std::min(accepted_variance_us,
                                    kHybridStartDelayMaxThresholdUs);
    QuicTime::Delta accepted_variance = QuicTime::Delta::FromMicroseconds(
        std::max(accepted_variance_us, kHybridStartDelayMinThresholdUs));

    if (current_rtt_ > delay_min.Add(accepted_variance)) {
      found_delay_ = true;
    }
  }
}

bool HybridSlowStart::Exit() {
  // If either one of the two conditions are met we exit from slow start
  // immediately.
  if (found_ack_train_ || found_delay_) {
    return true;
  }
  return false;
}

}  // namespace net
