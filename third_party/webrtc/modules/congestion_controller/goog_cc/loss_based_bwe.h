/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_GOOG_CC_LOSS_BASED_BWE_H_
#define MODULES_CONGESTION_CONTROLLER_GOOG_CC_LOSS_BASED_BWE_H_

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <utility>

#include "api/field_trials_view.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/congestion_controller/goog_cc/loss_based_bwe_v2.h"

namespace webrtc {

// Estimates bandwidth available to WebRTC if there is packet loss.
// The estimate will depend on loss calculated from transport feedback if it
// exist, or (RTCP) receiver report otherwise.
class LossBasedBwe {
 public:
  explicit LossBasedBwe(const FieldTrialsView* field_trials);

  // Called when the network route change. Resets state.
  void OnRouteChanged();

  // Called when new transport feedback is received.
  void OnTransportPacketsFeedback(const TransportPacketsFeedback& report,
                                  DataRate delay_based,
                                  std::optional<DataRate> acknowledged_bitrate,
                                  bool is_probe_rate,
                                  bool in_alr);

  // Called when a new loss report (RTCP receiver report) is received.
  void OnPacketLossReport(int64_t packets_lost,
                          int64_t packets_received,
                          TimeDelta round_trip_time,
                          Timestamp at_time);

  // Returns true if estimate changed.
  bool OnPeriodicProcess(Timestamp at_time);

  void SetConfiguredMinMaxBitrate(DataRate min_rate, DataRate max_rate);
  // Sets the rate used as reference if there is no transport feedback. It is
  // also used as loss based estimate until enough transport feedback messages
  // has been received.
  void SetStartRate(DataRate fallback_rate);

  LossBasedState state() const { return current_state_; }
  DataRate GetEstimate();

  // Returns (number of packets lost << 8) / total number of packets. There has
  // to be at least 20 packets received or lost between each update.
  uint8_t fraction_loss() const { return last_fraction_loss_; }

 private:
  // Updates history of min bitrates.
  // After this method returns min_bitrate_history_.front().second contains the
  // min bitrate used during last kBweIncreaseIntervalMs.
  void UpdateMinHistory(Timestamp at_time);

  void UpdateFallbackEstimate(DataRate new_estimate);

  const FieldTrialsView* field_trials_;
  std::unique_ptr<LossBasedBweV2> loss_based_bwe_v2_;

  DataRate configured_min_rate_;
  DataRate configured_max_rate_;
  DataRate delay_based_bwe_ = DataRate::PlusInfinity();

  DataRate fallback_estimate_ = DataRate::Zero();
  LossBasedState current_state_ = LossBasedState::kDelayBasedEstimate;

  TimeDelta last_round_trip_time_;
  int lost_packets_since_last_loss_update_ = 0;
  int expected_packets_since_last_loss_update_ = 0;
  // State variables used before LossBasedBweV2 is ready to be used or if
  // LossBasedBweV2 is disabled.
  std::deque<std::pair<Timestamp, DataRate> > min_bitrate_history_;
  bool has_decreased_since_last_fraction_loss_ = false;
  Timestamp time_last_decrease_ = Timestamp::MinusInfinity();
  float low_loss_threshold_;
  float high_loss_threshold_;
  DataRate bitrate_threshold_;

  Timestamp first_report_time_ = Timestamp::MinusInfinity();
  Timestamp last_loss_feedback_ = Timestamp::MinusInfinity();

  Timestamp last_loss_packet_report_ = Timestamp::MinusInfinity();
  uint8_t last_fraction_loss_ = 0;
  uint8_t last_logged_fraction_loss_ = 0;
};

}  // namespace webrtc
#endif  // MODULES_CONGESTION_CONTROLLER_GOOG_CC_LOSS_BASED_BWE_H_
