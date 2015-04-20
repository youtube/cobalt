// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper class to track the rate data can leave the buffer for pacing.
// A leaky bucket drains the data at a constant rate regardless of fullness of
// the buffer.
// See http://en.wikipedia.org/wiki/Leaky_bucket for more details.

#ifndef NET_QUIC_CONGESTION_CONTROL_LEAKY_BUCKET_H_
#define NET_QUIC_CONGESTION_CONTROL_LEAKY_BUCKET_H_

#include "base/basictypes.h"
#include "net/base/net_export.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_time.h"

namespace net {

class NET_EXPORT_PRIVATE LeakyBucket {
 public:
  // clock is not owned by this class.
  LeakyBucket(const QuicClock* clock, int bytes_per_second);

  // Set the rate at which the bytes leave the buffer.
  void SetDrainingRate(int bytes_per_second);

  // Add data to the buffer.
  void Add(size_t bytes);

  // Time until the buffer is empty in us.
  QuicTime::Delta TimeRemaining();

  // Number of bytes in the buffer.
  size_t BytesPending();

 private:
  void Update();

  const QuicClock* clock_;
  size_t bytes_;
  QuicTime time_last_updated_;
  int draining_rate_bytes_per_s_;
};

}  // namespace net

#endif  // NET_QUIC_CONGESTION_CONTROL_LEAKY_BUCKET_H_
