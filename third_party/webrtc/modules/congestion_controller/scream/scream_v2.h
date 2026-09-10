/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_SCREAM_SCREAM_V2_H_
#define MODULES_CONGESTION_CONTROLLER_SCREAM_SCREAM_V2_H_

#include <algorithm>

#include "api/environment/environment.h"
#include "api/field_trials_view.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/experiments/field_trial_parser.h"

namespace webrtc {

// Implements ScreamV2 based on the draft RFC in
// https://datatracker.ietf.org/doc/draft-johansson-ccwg-rfc8298bis-screamv2/

// Note, this class is currently in development and not all features are yet
// implemented.
// TODO: bugs.webrtc.org/447037083 - revisit this comment when implementation is
// done.

class ScreamV2 {
 public:
  explicit ScreamV2(const Environment& env);
  ~ScreamV2() = default;

  void SetTargetBitrateConstraints(DataRate min, DataRate max);
  // Returns target send rate given feedback.
  DataRate OnTransportPacketsFeedback(const TransportPacketsFeedback& msg);

  // Target for the upper limit of the number of bytes that can be in
  // flight (transmitted but not yet acknowledged)
  DataSize ref_window() const { return ref_window_; }

  // Returns the average fraction of ECN-CE marked data units per RTT.
  double l4s_alpha() const { return l4s_alpha_; }

 private:
  struct Parameters {
    explicit Parameters(const FieldTrialsView& trials);

    // Minimum Reference Window
    FieldTrialParameter<DataSize> min_ref_window;

    // Exponentially Weighted Moving Average (EWMA) factor for for l4s_alpha.
    FieldTrialParameter<double> l4s_avg_g;

    // Maximum Segment Size (MSS)
    // Size of the largest data segment that a sender is able to transmit. I.e
    // largest possible IP packet.
    FieldTrialParameter<DataSize> max_segment_size;

    // Headroom for bytes in flight when increasing reference window.
    FieldTrialParameter<double> bytes_in_flight_head_room;

    // Determines how many RTTs after a congestion event the reference window
    // growth should be cautious.
    FieldTrialParameter<int> post_congestion_delay_rtts;

    // Determines how much (as a fraction of ref_window) that the ref_window can
    // increase per RTT.
    FieldTrialParameter<double> multiplicative_increase_factor;

    // This mimics Prague's RTT fairness such that flows with RTT below
    // virtual_rtt should get a roughly equal share over an L4S path.
    FieldTrialParameter<TimeDelta> virtual_rtt;

    // Increase and decrease of ref window is slower close to the last
    // inflection point. Both increase and decrease is scaled by
    // (backoff_scale_factor_close_to_ref_window_i* (ref_window_i -
    // ref_window)) / ref_window_i) ^2
    FieldTrialParameter<double> backoff_scale_factor_close_to_ref_window_i;

    FieldTrialParameter<int> number_of_rtts_between_ref_window_i_updates;

    // If CE is detected and this number of RTTs has passed since last
    // congestion, ref_window_i will be reset.
    FieldTrialParameter<int>
        number_of_rtts_between_reset_ref_window_i_on_congestion;
  };

  void UpdateL4SAlpha(const TransportPacketsFeedback& msg);
  void UpdateRefWindowAndTargetRate(const TransportPacketsFeedback& msg);

  // Ratio between `max_segment_size` and `ref_window_`.
  double ref_window_mss_ratio() const {
    return params_.max_segment_size.Get() / ref_window_;
  }

  // Scaling factor for reference window adjustment
  // when close to the last known inflection point.
  // (4.2.2.1)
  double ref_window_scale_factor_close_to_ref_window_i() const {
    const double scale_factor =
        params_.backoff_scale_factor_close_to_ref_window_i.Get();
    double scl =
        ref_window_ > ref_window_i_
            ? scale_factor * (ref_window_ - ref_window_i_) / ref_window_i_
            : scale_factor * (ref_window_i_ - ref_window_) / ref_window_i_;
    return std::clamp(scl * scl, 0.1, 1.0);
  }

  // Scale factor for reference window increase. (4.2.2.2)
  // Always > 1.0.
  double ref_window_multiplicative_scale_factor() const {
    return 1.0 + (params_.multiplicative_increase_factor.Get() * ref_window_) /
                     params_.max_segment_size.Get();
  }

  const Environment env_;
  const Parameters params_;

  DataRate max_target_bitrate_ = DataRate::PlusInfinity();
  DataRate min_target_bitrate_ = DataRate::Zero();
  DataRate target_rate_ = DataRate::Zero();

  // Upper limit on the number of bytes that can be in
  // flight (transmitted but not yet acknowledged)
  DataSize ref_window_;
  // Reference window inflection point. I.e, `ref_window_` when congestion was
  // noticed. Increase and decrease of `ref_window_` is scaled down around
  // `ref_window_i_`.
  DataSize ref_window_i_ = DataSize::Bytes(1);
  Timestamp last_ref_window_i_update_ = Timestamp::MinusInfinity();

  // `l4s_alpha_` tracks the average fraction of ECN-CE marked data units per
  // Round-Trip Time.
  double l4s_alpha_ = 0.0;
  Timestamp last_l4s_alpha_update_ = Timestamp::MinusInfinity();
  Timestamp last_ce_mark_detected_time_ = Timestamp::MinusInfinity();

  // Per-RTT stats
  int data_units_delivered_this_rtt_ = 0;
  int data_units_marked_this_rtt_ = 0;
  Timestamp last_data_in_flight_update_ = Timestamp::MinusInfinity();
  DataSize max_data_in_flight_this_rtt_ = DataSize::Zero();
  DataSize max_data_in_flight_prev_rtt_ = DataSize::Zero();

  // `last_reaction_to_congestion_time` is called
  // `last_congestion_detected_time` in 4.2.2. Reference Window Update.
  // Last received feedback that contained a congestion event that may have
  // caused a reaction.
  Timestamp last_reaction_to_congestion_time_ = Timestamp::MinusInfinity();
};

}  // namespace webrtc
#endif  // MODULES_CONGESTION_CONTROLLER_SCREAM_SCREAM_V2_H_
