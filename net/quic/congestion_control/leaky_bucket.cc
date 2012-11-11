// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/leaky_bucket.h"

#include "base/time.h"

namespace net {

LeakyBucket::LeakyBucket(const QuicClock* clock, int bytes_per_second)
    : clock_(clock),
      bytes_(0),
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

QuicTime::Delta LeakyBucket::TimeRemaining() {
  Update();
  return QuicTime::Delta::FromMicroseconds(
      (bytes_ * base::Time::kMicrosecondsPerSecond) /
      draining_rate_bytes_per_s_);
}

size_t LeakyBucket::BytesPending() {
  Update();
  return bytes_;
}

void LeakyBucket::Update() {
  QuicTime::Delta elapsed_time = clock_->Now().Subtract(time_last_updated_);
  size_t bytes_cleared =
      (elapsed_time.ToMicroseconds() * draining_rate_bytes_per_s_) /
      base::Time::kMicrosecondsPerSecond;
  if (bytes_cleared >= bytes_) {
    bytes_ = 0;
  } else {
    bytes_ -= bytes_cleared;
  }
  time_last_updated_ = clock_->Now();
}

}  // namespace net
