/*
 *  Copyright 2026 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_CONGESTION_CONTROLLER_SCREAM_LOSS_RATE_ESTIMATOR_H_
#define MODULES_CONGESTION_CONTROLLER_SCREAM_LOSS_RATE_ESTIMATOR_H_

#include <stdint.h>

#include "api/transport/network_types.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/congestion_controller/scream/scream_v2_parameters.h"

namespace webrtc {

// Estimates the exponentially weighted moving average (EWMA) loss event rate
// per round-trip time (RTT).
//
// The loss event rate is updated on RTT boundaries. If one or more packets are
// reported lost during an RTT interval, the EWMA filter treats the interval as
// a loss event.
//
// To handle packet reordering robustly, the estimator maintains a count of
// outstanding unrecovered lost packets. When a packet is reported lost for the
// first time, the outstanding count increments. If a subsequently received
// feedback report indicates that an out-of-order packet has been recovered,
// the count decrements. If the unrecovered count reaches zero, the connection
// is considered loss-free, and the loss event rate is reset to zero.
class LossRateEstimator {
 public:
  explicit LossRateEstimator(const ScreamV2Parameters& params);
  ~LossRateEstimator() = default;

  // Updates the loss event rate estimate. Returns true if a loss has been
  // detected in this feedback message, false otherwise.
  bool Update(const TransportPacketsFeedback& msg, TimeDelta rtt);

  double loss_event_rate() const { return loss_event_rate_; }

 private:
  const TimeDelta virtual_rtt_;
  const double loss_event_rate_avg_g_loss_;
  const double loss_event_rate_avg_g_no_loss_;

  // `loss_event_this_rtt_` tracks whether any packet loss was detected
  // specifically within the current RTT interval. Consumed and reset by the
  // EWMA filter on RTT boundaries. Needed separately from
  // `unrecovered_lost_packets_` because unrecovered counts persist across RTT
  // updates to track delayed reordered recoveries.
  bool loss_event_this_rtt_ = false;
  double loss_event_rate_ = 0.0;
  int unrecovered_lost_packets_ = 0;
  Timestamp last_loss_or_recovery_time_ = Timestamp::MinusInfinity();
  Timestamp last_rtt_update_time_ = Timestamp::MinusInfinity();
};

}  // namespace webrtc
#endif  // MODULES_CONGESTION_CONTROLLER_SCREAM_LOSS_RATE_ESTIMATOR_H_
