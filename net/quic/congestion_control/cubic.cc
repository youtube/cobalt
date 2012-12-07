// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/cubic.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/time.h"

namespace net {

// Constants based on TCP defaults.
// The following constants are in 2^10 fractions of a second instead of ms to
// allow a 10 shift right to divide.
const int kCubeScale = 40;  // 1024*1024^3 (first 1024 is from 0.100^3)
                            // where 0.100 is 100 ms which is the scaling
                            // round trip time.
const int kCubeCongestionWindowScale = 410;
const uint64 kCubeFactor = (1ull << kCubeScale) / kCubeCongestionWindowScale;
const uint32 kBeta = 717;  // Back off factor after loss.
const uint32 kBetaLastMax = 871;  // Additional back off factor after loss for
                                  // the stored max value.

namespace {
// Find last bit in a 64-bit word.
int FindMostSignificantBit(uint64 x) {
  if (!x) {
    return 0;
  }
  int r = 0;
  if (x & 0xffffffff00000000ull) {
    x >>= 32;
    r += 32;
  }
  if (x & 0xffff0000u) {
    x >>= 16;
    r += 16;
  }
  if (x & 0xff00u) {
    x >>= 8;
    r += 8;
  }
  if (x & 0xf0u) {
    x >>= 4;
    r += 4;
  }
  if (x & 0xcu) {
    x >>= 2;
    r += 2;
  }
  if (x & 0x02u) {
    x >>= 1;
    r++;
  }
  if (x & 0x01u) {
    r++;
  }
  return r;
}

// 6 bits table [0..63]
const uint32 cube_root_table[] = {
    0,  54,  54,  54, 118, 118, 118, 118, 123, 129, 134, 138, 143, 147, 151,
  156, 157, 161, 164, 168, 170, 173, 176, 179, 181, 185, 187, 190, 192, 194,
  197, 199, 200, 202, 204, 206, 209, 211, 213, 215, 217, 219, 221, 222, 224,
  225, 227, 229, 231, 232, 234, 236, 237, 239, 240, 242, 244, 245, 246, 248,
  250, 251, 252, 254
};
}  // namespace

Cubic::Cubic(const QuicClock* clock)
    : clock_(clock) {
  Reset();
}

// Calculate the cube root using a table lookup followed by one Newton-Raphson
// iteration.
uint32 Cubic::CubeRoot(uint64 a) {
  uint32 msb = FindMostSignificantBit(a);
  DCHECK_LE(msb, 64u);

  if (msb < 7) {
    // MSB in our table.
    return ((cube_root_table[static_cast<uint32>(a)]) + 31) >> 6;
  }
  // MSB          7,  8,  9, 10, 11, 12, 13, 14, 15, 16, ...
  // cubic_shift  1,  1,  1,  2,  2,  2,  3,  3,  3,  4, ...
  uint32 cubic_shift = (msb - 4);
  cubic_shift = ((cubic_shift * 342) >> 10);  // Div by 3, biased high.

  // 4 to 6 bits accuracy depending on MSB.
  uint32 down_shifted_to_6bit = (a >> (cubic_shift * 3));
  uint64 root = ((cube_root_table[down_shifted_to_6bit] + 10) << cubic_shift)
      >> 6;

  // Make one Newton-Raphson iteration.
  // Since x has an error (inaccuracy due to the use of fix point) we get a
  // more accurate result by doing x * (x - 1) instead of x * x.
  root = 2 * root + (a / (root * (root - 1)));
  root = ((root * 341) >> 10);  // Div by 3, biased low.
  return static_cast<uint32>(root);
}

void Cubic::Reset() {
  epoch_ = QuicTime();  // Reset time.
  last_update_time_ = QuicTime();  // Reset time.
  last_congestion_window_ = 0;
  last_max_congestion_window_ = 0;
  acked_packets_count_ = 0;
  estimated_tcp_congestion_window_ = 0;
  origin_point_congestion_window_ = 0;
  time_to_origin_point_ = 0;
  last_target_congestion_window_ = 0;
}

QuicTcpCongestionWindow Cubic::CongestionWindowAfterPacketLoss(
    QuicTcpCongestionWindow current_congestion_window) {
  if (current_congestion_window < last_max_congestion_window_) {
    // We never reached the old max, so assume we are competing with another
    // flow. Use our extra back off factor to allow the other flow to go up.
    last_max_congestion_window_ =
        (kBetaLastMax * current_congestion_window) >> 10;
  } else {
    last_max_congestion_window_ = current_congestion_window;
  }
  epoch_ = QuicTime();  // Reset time.
  return (current_congestion_window * kBeta) >> 10;
}

QuicTcpCongestionWindow Cubic::CongestionWindowAfterAck(
    QuicTcpCongestionWindow current_congestion_window,
    QuicTime::Delta delay_min) {
  acked_packets_count_ += 1;  // Packets acked.
  QuicTime current_time = clock_->Now();

  // Cubic is "independent" of RTT, the update is limited by the time elapsed.
  if (last_congestion_window_ == current_congestion_window &&
      (current_time.Subtract(last_update_time_) <= MaxCubicTimeInterval())) {
    DCHECK(epoch_.IsInitialized());
    return std::max(last_target_congestion_window_,
                    estimated_tcp_congestion_window_);
  }
  last_congestion_window_ = current_congestion_window;
  last_update_time_ = current_time;

  if (!epoch_.IsInitialized()) {
    // First ACK after a loss event.
    DLOG(INFO) << "Start of epoch";
    epoch_ = current_time;  // Start of epoch.
    acked_packets_count_ = 1;  // Reset count.
    // Reset estimated_tcp_congestion_window_ to be in sync with cubic.
    estimated_tcp_congestion_window_ = current_congestion_window;
    if (last_max_congestion_window_ <= current_congestion_window) {
      time_to_origin_point_ = 0;
      origin_point_congestion_window_ = current_congestion_window;
    } else {
      time_to_origin_point_ = CubeRoot(kCubeFactor *
          (last_max_congestion_window_ - current_congestion_window));
      origin_point_congestion_window_ =
          last_max_congestion_window_;
    }
  }
  // Change the time unit from microseconds to 2^10 fractions per second. Take
  // the round trip time in account. This is done to allow us to use shift as a
  // divide operator.
  int64 elapsed_time =
      (current_time.Add(delay_min).Subtract(epoch_).ToMicroseconds() << 10) /
      base::Time::kMicrosecondsPerSecond;

  int64 offset = time_to_origin_point_ - elapsed_time;
  QuicTcpCongestionWindow delta_congestion_window = (kCubeCongestionWindowScale
      * offset * offset * offset) >> kCubeScale;

  QuicTcpCongestionWindow target_congestion_window =
      origin_point_congestion_window_ - delta_congestion_window;

  // We have a new cubic congestion window.
  last_target_congestion_window_ = target_congestion_window;

  // Update estimated TCP congestion_window.
  // Note: we do a normal Reno congestion avoidance calculation not the
  // calculation described in section 3.3 TCP-friendly region of the document.
  while (acked_packets_count_ >= estimated_tcp_congestion_window_) {
    acked_packets_count_ -= estimated_tcp_congestion_window_;
    estimated_tcp_congestion_window_++;
  }
  // Compute target congestion_window based on cubic target and estimated TCP
  // congestion_window, use highest (fastest).
  if (target_congestion_window < estimated_tcp_congestion_window_) {
    target_congestion_window = estimated_tcp_congestion_window_;
  }
  DLOG(INFO) << "Target congestion_window:" << target_congestion_window;
  return target_congestion_window;
}

}  // namespace net
