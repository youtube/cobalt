// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/leaky_bucket.h"

#include "base/time.h"

namespace net {

LeakyBucket::LeakyBucket(const QuicClock* clock, int bytes_per_second)
    : clock_(clock),
      bytes_(0),
      time_last_updated_us_(0),
      draining_rate_bytes_per_s_(bytes_per_second) {
}

void LeakyBucket::SetDrainingRate(int bytes_per_second) {
  Update();
  draining_rate_bytes_per_s_ = bytes_per_second;
}

void LeakyBucket::Add(size_t bytes) {
  Update();
  bytes_ += bytes;
}

uint64 LeakyBucket::TimeRemaining() {
  Update();
  uint64 time_remaining_us = (bytes_ * base::Time::kMicrosecondsPerSecond) /
      draining_rate_bytes_per_s_;
  return time_remaining_us;
}

size_t LeakyBucket::BytesPending() {
  Update();
  return bytes_;
}

void LeakyBucket::Update() {
  uint64 elapsed_time_us = clock_->NowInUsec() - time_last_updated_us_;
  size_t bytes_cleared = (elapsed_time_us * draining_rate_bytes_per_s_) /
      base::Time::kMicrosecondsPerSecond;
  if (bytes_cleared >= bytes_) {
    bytes_ = 0;
  } else {
    bytes_ -= bytes_cleared;
  }
  time_last_updated_us_ = clock_->NowInUsec();
}

}  // namespace net
