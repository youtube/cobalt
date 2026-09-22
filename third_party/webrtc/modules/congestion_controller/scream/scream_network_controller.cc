/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/scream/scream_network_controller.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "api/transport/network_control.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/congestion_controller/scream/scream_v2.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
namespace webrtc {

static constexpr DataRate kDefaultStartRate = DataRate::KilobitsPerSec(300);

ScreamNetworkController::ScreamNetworkController(NetworkControllerConfig config)
    : env_(config.env),
      params_(env_.field_trials()),
      default_pacing_window_(config.default_pacing_time_window),
      allow_initial_bwe_before_media_(
          config.stream_based_config.enable_repeated_initial_probing),
      current_pacing_window_(config.default_pacing_time_window),
      scream_(std::in_place, env_),
      target_rate_constraints_(config.constraints),
      streams_config_(config.stream_based_config),
      last_padding_interval_started_(Timestamp::Zero()) {
  if (config.constraints.min_data_rate.has_value() ||
      config.constraints.max_data_rate.has_value()) {
    scream_->SetTargetBitrateConstraints(
        config.constraints.min_data_rate.value_or(DataRate::Zero()),
        config.constraints.max_data_rate.value_or(DataRate::PlusInfinity()));
  }
}

NetworkControlUpdate ScreamNetworkController::CreateFirstUpdate(Timestamp now) {
  RTC_DCHECK(network_available_);
  RTC_DCHECK(!first_update_created_);
  first_update_created_ = true;
  scream_->SetFirstTargetRate(
      target_rate_constraints_.starting_rate.value_or(kDefaultStartRate));
  NetworkControlUpdate update = CreateUpdate(now);

  if (allow_initial_bwe_before_media_) {
    // Creating a probe packet allows padding packets to be sent. So this is
    // only used for triggering padding.
    update.probe_cluster_configs.emplace_back(ProbeClusterConfig{
        .at_time = now,
        .target_data_rate = DataRate::KilobitsPerSec(50),
        .target_duration = TimeDelta::Millis(1),
        .min_probe_delta = TimeDelta::Millis(10),
        // Use two probe packets even though one should be enough. This is a
        // workaround needed because the pacer will not generate or send padding
        // packets until after two probing packets.
        .target_probe_count = 2,
    });
  }
  return update;
}

NetworkControlUpdate ScreamNetworkController::OnNetworkAvailability(
    NetworkAvailability msg) {
  network_available_ = msg.network_available;
  if (!first_update_created_ && network_available_ &&
      streams_config_.max_total_allocated_bitrate > DataRate::Zero()) {
    return CreateFirstUpdate(msg.at_time);
  }
  return NetworkControlUpdate();
}

NetworkControlUpdate ScreamNetworkController::OnNetworkRouteChange(
    NetworkRouteChange msg) {
  RTC_LOG(LS_INFO) << " OnNetworkRouteChange, resetting ScreamV2.";
  target_rate_constraints_ = msg.constraints;
  scream_.emplace(env_);
  first_update_created_ = false;
  // TODO: bugs.webrtc.org/447037083 - We should use the minimum rate from
  // constraints, REMB and remote network state estimates.
  scream_->SetTargetBitrateConstraints(
      target_rate_constraints_.min_data_rate.value_or(DataRate::Zero()),
      target_rate_constraints_.max_data_rate.value_or(
          DataRate::PlusInfinity()));

  if (network_available_ &&
      streams_config_.max_total_allocated_bitrate > DataRate::Zero()) {
    return CreateFirstUpdate(msg.at_time);
  }
  return NetworkControlUpdate();
}

NetworkControlUpdate ScreamNetworkController::OnProcessInterval(
    ProcessInterval msg) {
  // Scream currently has no need for periodic processing.
  return NetworkControlUpdate();
}

NetworkControlUpdate ScreamNetworkController::OnRemoteBitrateReport(
    RemoteBitrateReport msg) {
  // TODO: bugs.webrtc.org/447037083 - Implement;
  return NetworkControlUpdate();
}

NetworkControlUpdate ScreamNetworkController::OnRoundTripTimeUpdate(
    RoundTripTimeUpdate msg) {
  // Scream uses Smoothed RTT from TransportFeedback.
  return NetworkControlUpdate();
}

NetworkControlUpdate ScreamNetworkController::OnSentPacket(SentPacket msg) {
  if (msg.data_in_flight > scream_->max_data_in_flight()) {
    RTC_LOG(LS_VERBOSE) << " Send window full:" << msg.data_in_flight << " > "
                        << scream_->max_data_in_flight();
    return CreateUpdate(msg.send_time);
  }
  return NetworkControlUpdate();
}

NetworkControlUpdate ScreamNetworkController::OnReceivedPacket(
    ReceivedPacket msg) {
  // Scream does not have to know about received packets.
  return NetworkControlUpdate();
}

NetworkControlUpdate ScreamNetworkController::OnStreamsConfig(
    StreamsConfig msg) {
  streams_config_ = msg;
  if (!first_update_created_ && network_available_ &&
      streams_config_.max_total_allocated_bitrate > DataRate::Zero()) {
    return CreateFirstUpdate(msg.at_time);
  }
  return NetworkControlUpdate();
}

NetworkControlUpdate ScreamNetworkController::OnTargetRateConstraints(
    TargetRateConstraints msg) {
  target_rate_constraints_ = msg;

  // TODO: bugs.webrtc.org/447037083 - We should use the minimum rate from
  // constraints, REMB and remote network state estimates.
  scream_->SetTargetBitrateConstraints(
      target_rate_constraints_.min_data_rate.value_or(DataRate::Zero()),
      target_rate_constraints_.max_data_rate.value_or(
          DataRate::PlusInfinity()));
  // No need to change target rate immediately. Wait until next feedback.
  return NetworkControlUpdate();
}

NetworkControlUpdate ScreamNetworkController::OnTransportLossReport(
    TransportLossReport msg) {
  return NetworkControlUpdate();
}

NetworkControlUpdate ScreamNetworkController::OnNetworkStateEstimate(
    NetworkStateEstimate msg) {
  // TODO: bugs.webrtc.org/447037083 - Implement;
  RTC_LOG_F(LS_INFO) << "Not implemented";
  return NetworkControlUpdate();
}

NetworkControlUpdate ScreamNetworkController::OnTransportPacketsFeedback(
    TransportPacketsFeedback msg) {
  scream_->OnTransportPacketsFeedback(msg);
  return CreateUpdate(msg.feedback_time);
}

NetworkControlUpdate ScreamNetworkController::CreateUpdate(Timestamp now) {
  NetworkControlUpdate update;
  if (scream_->target_rate() != reported_target_rate_) {
    reported_target_rate_ = scream_->target_rate();
    TargetTransferRate target_rate_msg;
    target_rate_msg.at_time = now;
    target_rate_msg.target_rate = scream_->target_rate();
    target_rate_msg.network_estimate.at_time = now;
    target_rate_msg.network_estimate.round_trip_time = scream_->rtt();
    // TODO: bugs.webrtc.org/447037083 - bwe_period must currently be set but
    // it seems like it is not used for anything sensible. Try to remove it.
    target_rate_msg.network_estimate.bwe_period = TimeDelta::Millis(25);
    update.target_rate = target_rate_msg;
  }
  update.pacer_config = MaybeCreatePacerConfig();
  update.congestion_window = scream_->max_data_in_flight();
  return update;
}

std::optional<PacerConfig> ScreamNetworkController::MaybeCreatePacerConfig() {
  // Time window used for calculating pacing window if target rate is
  // constrained by CE markings.
  constexpr TimeDelta kReducedPacingWindow = TimeDelta::Millis(20);
  // Threshold used for guessing if target rate is constrained due to CE
  // marking.
  constexpr double kL4sAlphaThreshold = 0.01;

  DataRate max_needed_rate =
      streams_config_.max_total_allocated_bitrate.value_or(DataRate::Zero());

  DataRate padding_rate = DataRate::Zero();
  TimeDelta pacing_window = current_pacing_window_;
  DataRate target_rate = scream_->target_rate();

  Timestamp now = env_.clock().CurrentTime();
  if (target_rate < max_needed_rate &&
      target_rate < target_rate_constraints_.max_data_rate.value_or(
                        DataRate::PlusInfinity())) {
    // Periodically allow padding to be used to reach a target rate close to
    // `max_needed_rate`.
    if (params_.periodic_padding_interval->IsFinite() &&
        (now - last_padding_interval_started_ >
         params_.periodic_padding_interval.Get())) {
      last_padding_interval_started_ = now;
    }
    if (now - last_padding_interval_started_ <
        params_.periodic_padding_duration.Get()) {
      padding_rate = target_rate;
    }
  }

  if (current_pacing_window_ == default_pacing_window_ &&
      scream_->target_rate() < max_needed_rate &&
      scream_->l4s_alpha() > kL4sAlphaThreshold) {
    // Do stricter pacing if target rate is lower than what is needed and it
    // seems like L4S is enabled. Note that once stricter pacing is enabled,
    // it is not stopped.
    pacing_window = std::min(default_pacing_window_, kReducedPacingWindow);
  }
  DataRate pacing_rate = scream_->pacing_rate();
  if (padding_rate != reported_padding_rate_ ||
      pacing_rate != reported_pacing_rate_ ||
      current_pacing_window_ != pacing_window) {
    reported_padding_rate_ = padding_rate;
    reported_pacing_rate_ = pacing_rate;
    current_pacing_window_ = pacing_window;
    return PacerConfig::Create(now, pacing_rate, padding_rate,
                               current_pacing_window_);
  }
  return std::nullopt;
}

}  // namespace webrtc
