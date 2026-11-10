/*
 *  Copyright 2026 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/congestion_controller/scream/loss_rate_estimator.h"

#include <algorithm>

#include "api/transport/network_types.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/congestion_controller/scream/scream_v2_parameters.h"

namespace webrtc {

LossRateEstimator::LossRateEstimator(const ScreamV2Parameters& params)
    : virtual_rtt_(params.virtual_rtt.Get()),
      loss_event_rate_avg_g_loss_(params.loss_event_rate_avg_g_loss.Get()),
      loss_event_rate_avg_g_no_loss_(
          params.loss_event_rate_avg_g_no_loss.Get()) {}

bool LossRateEstimator::Update(const TransportPacketsFeedback& msg,
                               TimeDelta rtt) {
  if (msg.PacketsWithFeedback().empty()) {
    return false;
  }

  const TimeDelta max_rtt = std::max(virtual_rtt_, rtt);
  if (msg.feedback_time - last_loss_or_recovery_time_ > max_rtt) {
    // Reset the unrecovered lost packet count if no new losses or recoveries
    // were reported for a full RTT. This prevents old, permanently lost
    // packets (which will never be recovered) from keeping this counter
    // positive indefinitely.
    unrecovered_lost_packets_ = 0;
  }

  bool has_lost_packets = false;
  for (const PacketResult& packet : msg.PacketsWithFeedback()) {
    bool is_lost =
        !packet.IsReceived() && packet.reported_lost_for_the_first_time;
    bool is_recovered = packet.reported_recovered_for_the_first_time;
    if (!is_lost && !is_recovered) {
      continue;
    }

    last_loss_or_recovery_time_ = msg.feedback_time;

    if (is_recovered) {
      unrecovered_lost_packets_--;
      unrecovered_lost_packets_ = std::max(0, unrecovered_lost_packets_);
      if (unrecovered_lost_packets_ == 0) {
        loss_event_rate_ = 0.0;
        loss_event_this_rtt_ = false;
      }
    } else {
      has_lost_packets = true;
      unrecovered_lost_packets_++;
      loss_event_this_rtt_ = true;
    }
  }

  if (msg.feedback_time - last_rtt_update_time_ >= max_rtt) {
    last_rtt_update_time_ = msg.feedback_time;

    double g = loss_event_this_rtt_ ? loss_event_rate_avg_g_loss_
                                    : loss_event_rate_avg_g_no_loss_;
    loss_event_rate_ =
        g * (loss_event_this_rtt_ ? 1.0 : 0.0) + (1.0 - g) * loss_event_rate_;
    loss_event_this_rtt_ = false;
  }
  return has_lost_packets;
}

}  // namespace webrtc
