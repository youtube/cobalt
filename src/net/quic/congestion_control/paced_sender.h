// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper class that limits the congestion window to pace the packets.

#ifndef NET_QUIC_CONGESTION_CONTROL_PACED_SENDER_H_
#define NET_QUIC_CONGESTION_CONTROL_PACED_SENDER_H_

#include "base/basictypes.h"
#include "net/base/net_export.h"
#include "net/quic/congestion_control/leaky_bucket.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_time.h"

namespace net {

class NET_EXPORT_PRIVATE PacedSender {
 public:
  PacedSender(const QuicClock* clock, int bandwidth_estimate_bytes_per_s);

  // The estimated bandidth from the congestion algorithm changed.
  void UpdateBandwidthEstimate(int bytes_per_s);

  // A packet of size bytes was sent.
  void SentPacket(size_t bytes);

  // Return time until we can send based on the pacing.
  QuicTime::Delta TimeUntilSend(QuicTime::Delta time_until_send);

  // Return the amount of data in bytes we can send based on the pacing.
  // available_congestion_window is the congestion algorithms available
  // congestion window in bytes.
  size_t AvailableWindow(size_t available_congestion_window);

 private:
  // Helper object to track the rate data can leave the buffer for pacing.
  LeakyBucket leaky_bucket_;
  int pace_in_bytes_per_s_;
};

}  // namespace net

#endif  // NET_QUIC_CONGESTION_CONTROL_PACED_SENDER_H_
