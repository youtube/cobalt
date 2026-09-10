/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/scream/scream_v2.h"

#include <algorithm>
#include <vector>

#include "api/environment/environment.h"
#include "api/field_trials_view.h"
#include "api/transport/ecn_marking.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {

// Returns the size of packets that have been acked (including lost
// packets) and not marked as CE.
DataSize DataUnitsAckedAndNotMarked(const TransportPacketsFeedback& msg) {
  DataSize acked_not_marked = DataSize::Zero();
  for (const PacketResult& packet : msg.PacketsWithFeedback()) {
    if (packet.ecn != EcnMarking::kCe) {
      acked_not_marked += packet.sent_packet.size;
    }
  }
  return acked_not_marked;
}

bool HasCeMarking(const TransportPacketsFeedback& msg) {
  for (const auto& packet : msg.PacketsWithFeedback()) {
    if (packet.ecn == EcnMarking::kCe) {
      return true;
    }
  }
  return false;
}

}  // namespace

ScreamV2::Parameters::Parameters(const FieldTrialsView& trials)
    : min_ref_window("MinRefWindow", DataSize::Bytes(3000)),
      l4s_avg_g("L4sAvgG", 1.0 / 16.0),
      max_segment_size("MaxSegmentSize", DataSize::Bytes(1000)),
      bytes_in_flight_head_room("BytesInFlightHeadRoom", 2.0),
      post_congestion_delay_rtts("PostCongestionDelayRtts", 100),
      multiplicative_increase_factor("MultiplicativeIncreaseFactor", 0.02),
      virtual_rtt("VirtualRtt", TimeDelta::Millis(25)),
      // backoff_scale_factor_close_to_ref_window_i is set lower than in the rfc
      // (8.0). This means that increase/decrease around ref_window_i is slower
      // in this implementation.
      backoff_scale_factor_close_to_ref_window_i(
          "BackoffScaleFactorCloseToRefWindowI",
          2.0),
      number_of_rtts_between_ref_window_i_updates(
          "NumberOfRttsBetweenRefWindowIUpdates",
          10),
      number_of_rtts_between_reset_ref_window_i_on_congestion(
          "NumberOfRttsBetweenResetRefWindowIOnCongestion",
          100) {
  ParseFieldTrial({&min_ref_window, &l4s_avg_g, &max_segment_size,
                   &bytes_in_flight_head_room, &post_congestion_delay_rtts,
                   &multiplicative_increase_factor, &virtual_rtt,
                   &backoff_scale_factor_close_to_ref_window_i,
                   &number_of_rtts_between_ref_window_i_updates,
                   &number_of_rtts_between_reset_ref_window_i_on_congestion},
                  trials.Lookup("WebRTC-Bwe-ScreamV2"));
}

ScreamV2::ScreamV2(const Environment& env)
    : env_(env),
      params_(env_.field_trials()),
      ref_window_(params_.min_ref_window.Get()) {}

void ScreamV2::SetTargetBitrateConstraints(DataRate min, DataRate max) {
  RTC_DCHECK_GE(max, min);
  min_target_bitrate_ = min;
  max_target_bitrate_ = max;
}

DataRate ScreamV2::OnTransportPacketsFeedback(
    const TransportPacketsFeedback& msg) {
  UpdateL4SAlpha(msg);
  UpdateRefWindowAndTargetRate(msg);
  return target_rate_;
}

void ScreamV2::UpdateL4SAlpha(const TransportPacketsFeedback& msg) {
  // 4.2.1.3.
  const std::vector<PacketResult> received_packets = msg.ReceivedWithSendInfo();
  if (received_packets.empty()) {
    return;
  }
  data_units_delivered_this_rtt_ += received_packets.size();
  for (const PacketResult& packet : received_packets) {
    if (packet.ecn == EcnMarking::kCe) {
      ++data_units_marked_this_rtt_;
    }
  }

  // l4s_alpha can be updated more frequently than every smoothed rtt.
  constexpr TimeDelta kMaxTimeBetweenL4sUpdate = TimeDelta::Millis(10);
  if (msg.feedback_time - last_l4s_alpha_update_ <
      std::min(kMaxTimeBetweenL4sUpdate, msg.smoothed_rtt)) {
    return;
  }
  last_l4s_alpha_update_ = msg.feedback_time;

  double fraction_marked = static_cast<double>(data_units_marked_this_rtt_) /
                           data_units_delivered_this_rtt_;
  l4s_alpha_ = params_.l4s_avg_g.Get() * fraction_marked +
               (1.0 - params_.l4s_avg_g.Get()) * l4s_alpha_;

  data_units_delivered_this_rtt_ = 0;
  data_units_marked_this_rtt_ = 0;
}

void ScreamV2::UpdateRefWindowAndTargetRate(
    const TransportPacketsFeedback& msg) {
  const TimeDelta rtt = std::max(params_.virtual_rtt.Get(), msg.smoothed_rtt);

  max_data_in_flight_this_rtt_ =
      std::max(max_data_in_flight_this_rtt_, msg.data_in_flight);

  // ref_window_i may change due to reference window reduction. Thus
  // scale_close_to_ref_window_i is calculated here.
  double scale_close_to_ref_window_i =
      ref_window_scale_factor_close_to_ref_window_i();

  bool is_ce = false;
  if (msg.feedback_time - last_reaction_to_congestion_time_ >=
      std::min(msg.smoothed_rtt, params_.virtual_rtt.Get())) {
    is_ce = HasCeMarking(msg);
  }

  // Update `ref_window_i_` if enough time has passed since the last update and
  // a congestion event is detected.
  if (is_ce) {
    if (msg.feedback_time - last_ref_window_i_update_ >
        params_.number_of_rtts_between_ref_window_i_updates.Get() *
            msg.smoothed_rtt) {
      // Additional median filtering over more congestion epochs
      // may improve accuracy of ref_wnd_i_.
      last_ref_window_i_update_ = msg.feedback_time;
      ref_window_i_ = ref_window_;
    }
  }

  double backoff = 0.0;
  if (is_ce) {  // Backoff due to ECN-CE marking
    backoff = l4s_alpha_ / 2.0;
    //  Increase stability for very small ref_wnd
    backoff *= std::max(0.5, 1.0 - ref_window_mss_ratio());

    // Scale down backoff if close to the last known max reference window
    // This is complemented with a scale down of the reference window increase

    // TODO: bugs.webrtc.org/447037083 - Dont scale down backoff
    // if queue delay is large.
    backoff *= std::max(0.25, scale_close_to_ref_window_i);

    if (msg.feedback_time - last_reaction_to_congestion_time_ >
        params_.number_of_rtts_between_reset_ref_window_i_on_congestion.Get() *
            std::max(params_.virtual_rtt.Get(), msg.smoothed_rtt)) {
      // A long time(> 100 RTTs) since last congested because
      // link throughput exceeds max video bitrate. (or first congestion)
      // There is a certain risk that ref_wnd has increased way above
      // bytes in flight, so we reduce it here to get it better on
      // track and thus the congestion episode is shortened
      ref_window_i_ = std::min(ref_window_i_, max_data_in_flight_prev_rtt_);
      // Also, we back off a little extra if needed because alpha is quite
      // likely very low  This can in some cases be an over - reaction but as
      // this function should kick in relatively seldom it should not be a too
      // big concern
      backoff = std::max(backoff, 0.25);
      // In addition, bump up l4sAlpha to a more credible value
      // This may over react but it is better than
      // excessive queue delay
      l4s_alpha_ = 0.25;
    }
  }  // is_ce
  ref_window_ = (1.0 - backoff) * ref_window_;

  if (is_ce) {
    last_reaction_to_congestion_time_ = msg.feedback_time;
  }

  // Increase ref_window.
  // 4.2.2.2.  Reference Window Increase
  DataSize increase = DataUnitsAckedAndNotMarked(msg) * ref_window_mss_ratio();

  // Limit increase for small RTTs
  if (msg.smoothed_rtt < params_.virtual_rtt.Get()) {
    double rtt_ratio = msg.smoothed_rtt / params_.virtual_rtt.Get();
    increase = increase * (rtt_ratio * rtt_ratio);
  }
  // Limit reference window increase when close to the last inflection
  // point.
  increase = increase * std::max(0.25, scale_close_to_ref_window_i);
  // Limit reference window increase when the reference window close to
  // max segment size.
  increase = increase * std::max(0.5, 1.0 - ref_window_mss_ratio());

  // Use lower multiplicative scale factor if congestion was detected
  // recently.
  double post_congestion_scale =
      std::clamp((msg.feedback_time - last_reaction_to_congestion_time_) /
                     (params_.post_congestion_delay_rtts.Get() * rtt),
                 0.0, 1.0);
  double multiplicative_scale =
      1.0 + (ref_window_multiplicative_scale_factor() - 1.0) *
                post_congestion_scale * scale_close_to_ref_window_i;
  RTC_DCHECK_GE(multiplicative_scale, 1.0);
  increase = increase * multiplicative_scale;

  // Increase ref_window only if bytes in flight is large enough.
  // Quite a lot of slack is allowed here to avoid that bitrate locks to low
  // values. Increase is inhibited if max target bitrate is reached.
  DataSize max_allowed_ref_window = std::max(
      params_.max_segment_size.Get() +
          std::max(max_data_in_flight_this_rtt_, max_data_in_flight_prev_rtt_) *
              params_.bytes_in_flight_head_room.Get(),
      params_.min_ref_window.Get());

  if (ref_window_ < max_allowed_ref_window) {
    ref_window_ =
        std::clamp(ref_window_ + increase, params_.min_ref_window.Get(),
                   max_allowed_ref_window);
  }

  // TODO: bugs.webrtc.org/447037083 - Implement  bytes_in_flight limitation
  // when queue delay is increased according to section 4.4 and consider
  // packetization overhead.

  // Scale down target rate slightly when the reference window is very small
  // compared to MSS
  double scale_down = 1.0 - std::clamp(ref_window_mss_ratio() - 0.1, 0.0, 0.2);
  // Avoid division by zero.
  TimeDelta effective_rtt = std::max(msg.smoothed_rtt, TimeDelta::Millis(1));
  target_rate_ = std::clamp(scale_down * (ref_window_ / effective_rtt),
                            min_target_bitrate_, max_target_bitrate_);

  RTC_LOG(LS_VERBOSE) << "ScreamV2: "
                      << " increase=" << increase << " backoff=" << backoff
                      << " scale_close_to_ref_window_i= "
                      << scale_close_to_ref_window_i
                      << " multiplicative_scalel=" << multiplicative_scale
                      << " l4s_alpha=" << l4s_alpha_
                      << " smoothed_rtt=" << msg.smoothed_rtt
                      << " max_data_in_flight_this_rtt_="
                      << max_data_in_flight_this_rtt_
                      << " ref_window = " << ref_window_
                      << " ref_window_i_=" << ref_window_i_
                      << " target_rate_=" << target_rate_;

  if (msg.feedback_time - last_data_in_flight_update_ >= rtt) {
    last_data_in_flight_update_ = msg.feedback_time;
    max_data_in_flight_prev_rtt_ = max_data_in_flight_this_rtt_;
    max_data_in_flight_this_rtt_ = DataSize::Zero();
  }
}

}  // namespace webrtc
