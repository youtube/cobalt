/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc/send_side_bandwidth_estimation.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>

#include "api/field_trials_view.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_loss_based.h"
#include "modules/congestion_controller/goog_cc/loss_based_bwe.h"
#include "modules/congestion_controller/goog_cc/loss_based_bwe_v2.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {
namespace {

constexpr TimeDelta kStartPhase = TimeDelta::Millis(2000);
constexpr TimeDelta kBweConverganceTime = TimeDelta::Millis(20000);
constexpr DataRate kDefaultMaxBitrate = DataRate::BitsPerSec(1000000000);
constexpr TimeDelta kLowBitrateLogPeriod = TimeDelta::Millis(10000);
constexpr TimeDelta kRtcEventLogPeriod = TimeDelta::Millis(5000);

struct UmaRampUpMetric {
  const char* metric_name;
  int bitrate_kbps;
};

const UmaRampUpMetric kUmaRampupMetrics[] = {
    {"WebRTC.BWE.RampUpTimeTo500kbpsInMs", 500},
    {"WebRTC.BWE.RampUpTimeTo1000kbpsInMs", 1000},
    {"WebRTC.BWE.RampUpTimeTo2000kbpsInMs", 2000}};
const size_t kNumUmaRampupMetrics =
    sizeof(kUmaRampupMetrics) / sizeof(kUmaRampupMetrics[0]);

}  // namespace

RttBasedBackoff::RttBasedBackoff(const FieldTrialsView& key_value_config)
    : disabled_("Disabled"),
      configured_limit_("limit", TimeDelta::Seconds(3)),
      drop_fraction_("fraction", 0.8),
      drop_interval_("interval", TimeDelta::Seconds(1)),
      bandwidth_floor_("floor", DataRate::KilobitsPerSec(5)),
      rtt_limit_(TimeDelta::PlusInfinity()),
      // By initializing this to plus infinity, we make sure that we never
      // trigger rtt backoff unless packet feedback is enabled.
      last_propagation_rtt_update_(Timestamp::PlusInfinity()),
      last_propagation_rtt_(TimeDelta::Zero()),
      last_packet_sent_(Timestamp::MinusInfinity()) {
  ParseFieldTrial({&disabled_, &configured_limit_, &drop_fraction_,
                   &drop_interval_, &bandwidth_floor_},
                  key_value_config.Lookup("WebRTC-Bwe-MaxRttLimit"));
  if (!disabled_) {
    rtt_limit_ = configured_limit_.Get();
  }
}

void RttBasedBackoff::UpdatePropagationRtt(Timestamp at_time,
                                           TimeDelta propagation_rtt) {
  last_propagation_rtt_update_ = at_time;
  last_propagation_rtt_ = propagation_rtt;
}

bool RttBasedBackoff::IsRttAboveLimit() const {
  return CorrectedRtt() > rtt_limit_;
}

TimeDelta RttBasedBackoff::CorrectedRtt() const {
  // Avoid timeout when no packets are being sent.
  TimeDelta timeout_correction = std::max(
      last_packet_sent_ - last_propagation_rtt_update_, TimeDelta::Zero());
  return timeout_correction + last_propagation_rtt_;
}

RttBasedBackoff::~RttBasedBackoff() = default;

SendSideBandwidthEstimation::SendSideBandwidthEstimation(
    const FieldTrialsView* key_value_config,
    RtcEventLog* event_log)
    : rtt_backoff_(*key_value_config),
      loss_based_bwe_(key_value_config),
      last_logged_fraction_loss_(0),
      last_round_trip_time_(TimeDelta::Zero()),
      receiver_limit_(DataRate::PlusInfinity()),
      delay_based_limit_(DataRate::PlusInfinity()),
      loss_based_limit_(DataRate::PlusInfinity()),
      current_target_(kCongestionControllerMinBitrate),
      last_logged_target_(DataRate::Zero()),
      min_bitrate_configured_(kCongestionControllerMinBitrate),
      max_bitrate_configured_(kDefaultMaxBitrate),

      last_low_bitrate_log_(Timestamp::MinusInfinity()),
      time_last_decrease_due_to_rtt_(Timestamp::MinusInfinity()),
      first_loss_report_time_(Timestamp::MinusInfinity()),
      initially_lost_packets_(0),
      bitrate_at_2_seconds_(DataRate::Zero()),
      uma_update_state_(kNoUpdate),
      uma_rtt_state_(kNoUpdate),
      rampup_uma_stats_updated_(kNumUmaRampupMetrics, false),
      event_log_(event_log),
      last_rtc_event_log_(Timestamp::MinusInfinity()) {
  RTC_DCHECK(event_log);
  loss_based_bwe_.SetConfiguredMinMaxBitrate(min_bitrate_configured_,
                                             max_bitrate_configured_);
}

SendSideBandwidthEstimation::~SendSideBandwidthEstimation() {}

void SendSideBandwidthEstimation::OnRouteChange() {
  current_target_ = kCongestionControllerMinBitrate;
  min_bitrate_configured_ = kCongestionControllerMinBitrate;
  max_bitrate_configured_ = kDefaultMaxBitrate;
  last_low_bitrate_log_ = Timestamp::MinusInfinity();
  last_logged_fraction_loss_ = 0;
  last_round_trip_time_ = TimeDelta::Zero();
  receiver_limit_ = DataRate::PlusInfinity();
  delay_based_limit_ = DataRate::PlusInfinity();
  loss_based_limit_ = DataRate::PlusInfinity();
  time_last_decrease_due_to_rtt_ = Timestamp::MinusInfinity();
  first_loss_report_time_ = Timestamp::MinusInfinity();
  initially_lost_packets_ = 0;
  bitrate_at_2_seconds_ = DataRate::Zero();
  uma_update_state_ = kNoUpdate;
  uma_rtt_state_ = kNoUpdate;
  last_rtc_event_log_ = Timestamp::MinusInfinity();
  rtt_back_off_rate_ = std::nullopt;
  loss_based_bwe_.OnRouteChanged();
}

void SendSideBandwidthEstimation::SetBitrates(
    std::optional<DataRate> send_bitrate,
    DataRate min_bitrate,
    DataRate max_bitrate,
    Timestamp at_time) {
  SetMinMaxBitrate(min_bitrate, max_bitrate);
  if (send_bitrate) {
    delay_based_limit_ = DataRate::PlusInfinity();
    current_target_ = *send_bitrate;
    loss_based_bwe_.SetStartRate(*send_bitrate);
  }
}

void SendSideBandwidthEstimation::SetMinMaxBitrate(DataRate min_bitrate,
                                                   DataRate max_bitrate) {
  min_bitrate_configured_ =
      std::max(min_bitrate, kCongestionControllerMinBitrate);
  if (max_bitrate > DataRate::Zero() && max_bitrate.IsFinite()) {
    max_bitrate_configured_ = std::max(min_bitrate_configured_, max_bitrate);
  } else {
    max_bitrate_configured_ = kDefaultMaxBitrate;
  }
  loss_based_bwe_.SetConfiguredMinMaxBitrate(min_bitrate_configured_,
                                             max_bitrate_configured_);
}

int SendSideBandwidthEstimation::GetMinBitrate() const {
  return min_bitrate_configured_.bps<int>();
}

DataRate SendSideBandwidthEstimation::target_rate() const {
  return current_target_;
}

LossBasedState SendSideBandwidthEstimation::loss_based_state() const {
  return loss_based_bwe_.state();
}

bool SendSideBandwidthEstimation::IsRttAboveLimit() const {
  return rtt_backoff_.IsRttAboveLimit();
}

void SendSideBandwidthEstimation::UpdateReceiverEstimate(Timestamp at_time,
                                                         DataRate bandwidth) {
  // TODO(srte): Ensure caller passes PlusInfinity, not zero, to represent no
  // limitation.
  DataRate estimate = bandwidth.IsZero() ? DataRate::PlusInfinity() : bandwidth;
  if (estimate != receiver_limit_) {
    receiver_limit_ = estimate;

    if (IsInStartPhase(at_time) && loss_based_bwe_.fraction_loss() == 0 &&
        receiver_limit_ > current_target_ &&
        delay_based_limit_ > receiver_limit_) {
      // Reset the (fallback) loss based estimator and trust the remote estimate
      // is a good starting rate.
      loss_based_bwe_.SetStartRate(receiver_limit_);
      loss_based_limit_ = loss_based_bwe_.GetEstimate();
    }
    ApplyTargetLimits(at_time);
  }
}

void SendSideBandwidthEstimation::OnTransportPacketsFeedback(
    const TransportPacketsFeedback& report,
    DataRate delay_based_estimate,
    std::optional<DataRate> acknowledged_rate,
    bool is_probe_rate,
    bool in_alr) {
  delay_based_estimate = delay_based_estimate.IsZero()
                             ? DataRate::PlusInfinity()
                             : delay_based_estimate;
  acknowledged_rate_ = acknowledged_rate;

  loss_based_bwe_.OnTransportPacketsFeedback(
      report, delay_based_estimate, acknowledged_rate_, is_probe_rate, in_alr);

  DataRate loss_based_estimate = loss_based_bwe_.GetEstimate();
  if (loss_based_estimate != loss_based_limit_ ||
      delay_based_limit_ != delay_based_estimate) {
    delay_based_limit_ = delay_based_estimate;
    loss_based_limit_ = loss_based_estimate;
    ApplyTargetLimits(report.feedback_time);
  }
}

void SendSideBandwidthEstimation::UpdatePacketsLost(int64_t packets_lost,
                                                    int64_t packets_received,
                                                    Timestamp at_time) {
  if (first_loss_report_time_.IsInfinite()) {
    first_loss_report_time_ = at_time;
  }
  loss_based_bwe_.OnPacketLossReport(packets_lost, packets_received,
                                     last_round_trip_time_, at_time);
  UpdateUmaStatsPacketsLost(at_time, packets_lost);
  DataRate estimate = loss_based_bwe_.GetEstimate();
  if (estimate != loss_based_limit_) {
    loss_based_limit_ = loss_based_bwe_.GetEstimate();
    ApplyTargetLimits(at_time);
  }
}

void SendSideBandwidthEstimation::UpdateUmaStatsPacketsLost(Timestamp at_time,
                                                            int packets_lost) {
  DataRate bitrate_kbps =
      DataRate::KilobitsPerSec((current_target_.bps() + 500) / 1000);
  for (size_t i = 0; i < kNumUmaRampupMetrics; ++i) {
    if (!rampup_uma_stats_updated_[i] &&
        bitrate_kbps.kbps() >= kUmaRampupMetrics[i].bitrate_kbps) {
      RTC_HISTOGRAMS_COUNTS_100000(i, kUmaRampupMetrics[i].metric_name,
                                   (at_time - first_loss_report_time_).ms());
      rampup_uma_stats_updated_[i] = true;
    }
  }
  if (IsInStartPhase(at_time)) {
    initially_lost_packets_ += packets_lost;
  } else if (uma_update_state_ == kNoUpdate) {
    uma_update_state_ = kFirstDone;
    bitrate_at_2_seconds_ = bitrate_kbps;
    RTC_HISTOGRAM_COUNTS("WebRTC.BWE.InitiallyLostPackets",
                         initially_lost_packets_, 0, 100, 50);
    RTC_HISTOGRAM_COUNTS("WebRTC.BWE.InitialBandwidthEstimate",
                         bitrate_at_2_seconds_.kbps(), 0, 2000, 50);
  } else if (uma_update_state_ == kFirstDone &&
             at_time - first_loss_report_time_ >= kBweConverganceTime) {
    uma_update_state_ = kDone;
    int bitrate_diff_kbps = std::max(
        bitrate_at_2_seconds_.kbps<int>() - bitrate_kbps.kbps<int>(), 0);
    RTC_HISTOGRAM_COUNTS("WebRTC.BWE.InitialVsConvergedDiff", bitrate_diff_kbps,
                         0, 2000, 50);
  }
}

void SendSideBandwidthEstimation::UpdateRtt(TimeDelta rtt, Timestamp at_time) {
  // Update RTT if we were able to compute an RTT based on this RTCP.
  // FlexFEC doesn't send RTCP SR, which means we won't be able to compute RTT.
  if (rtt > TimeDelta::Zero())
    last_round_trip_time_ = rtt;

  if (!IsInStartPhase(at_time) && uma_rtt_state_ == kNoUpdate) {
    uma_rtt_state_ = kDone;
    RTC_HISTOGRAM_COUNTS("WebRTC.BWE.InitialRtt", rtt.ms<int>(), 0, 2000, 50);
  }
}

void SendSideBandwidthEstimation::OnPeriodicUpdate(Timestamp at_time) {
  if (rtt_backoff_.IsRttAboveLimit()) {
    if (at_time - time_last_decrease_due_to_rtt_ >=
            rtt_backoff_.drop_interval_ &&
        current_target_ > rtt_backoff_.bandwidth_floor_) {
      time_last_decrease_due_to_rtt_ = at_time;
      rtt_back_off_rate_ =
          std::max(current_target_ * rtt_backoff_.drop_fraction_,
                   rtt_backoff_.bandwidth_floor_.Get());
      ApplyTargetLimits(at_time);
    }
  } else if (rtt_back_off_rate_.has_value()) {
    rtt_back_off_rate_ = std::nullopt;
    ApplyTargetLimits(at_time);
  }
  if (loss_based_bwe_.OnPeriodicProcess(at_time)) {
    loss_based_limit_ = loss_based_bwe_.GetEstimate();
    ApplyTargetLimits(at_time);
  }
}

void SendSideBandwidthEstimation::UpdatePropagationRtt(
    Timestamp at_time,
    TimeDelta propagation_rtt) {
  rtt_backoff_.UpdatePropagationRtt(at_time, propagation_rtt);
}

void SendSideBandwidthEstimation::OnSentPacket(const SentPacket& sent_packet) {
  // Only feedback-triggering packets will be reported here.
  rtt_backoff_.last_packet_sent_ = sent_packet.send_time;
}

bool SendSideBandwidthEstimation::IsInStartPhase(Timestamp at_time) const {
  return first_loss_report_time_.IsInfinite() ||
         at_time - first_loss_report_time_ < kStartPhase;
}

void SendSideBandwidthEstimation::MaybeLogLowBitrateWarning(DataRate bitrate,
                                                            Timestamp at_time) {
  if (at_time - last_low_bitrate_log_ > kLowBitrateLogPeriod) {
    RTC_LOG(LS_WARNING) << "Estimated available bandwidth " << ToString(bitrate)
                        << " is below configured min bitrate "
                        << ToString(min_bitrate_configured_) << ".";
    last_low_bitrate_log_ = at_time;
  }
}

void SendSideBandwidthEstimation::MaybeLogLossBasedEvent(Timestamp at_time) {
  if (current_target_ != last_logged_target_ ||
      loss_based_bwe_.fraction_loss() != last_logged_fraction_loss_ ||
      at_time - last_rtc_event_log_ > kRtcEventLogPeriod) {
    event_log_->Log(std::make_unique<RtcEventBweUpdateLossBased>(
        current_target_.bps(), loss_based_bwe_.fraction_loss(),
        /*total_packets_ =*/0));
    last_logged_fraction_loss_ = loss_based_bwe_.fraction_loss();
    last_logged_target_ = current_target_;
    last_rtc_event_log_ = at_time;
  }
}

void SendSideBandwidthEstimation::ApplyTargetLimits(Timestamp at_time) {
  current_target_ =
      std::min({delay_based_limit_, receiver_limit_,
                rtt_back_off_rate_.value_or(DataRate::PlusInfinity()),
                loss_based_limit_, max_bitrate_configured_});

  if (current_target_ < min_bitrate_configured_) {
    MaybeLogLowBitrateWarning(current_target_, at_time);
    current_target_ = min_bitrate_configured_;
  }
  MaybeLogLossBasedEvent(at_time);
}

}  // namespace webrtc
