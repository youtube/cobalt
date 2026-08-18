/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc/loss_based_bwe.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "api/field_trials_view.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/congestion_controller/goog_cc/loss_based_bwe_v2.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {

constexpr float kDefaultLowLossThreshold = 0.02f;
constexpr float kDefaultHighLossThreshold = 0.1f;
constexpr DataRate kDefaultBitrateThreshold = DataRate::Zero();
constexpr TimeDelta kBweIncreaseInterval = TimeDelta::Millis(1000);
constexpr TimeDelta kBweDecreaseInterval = TimeDelta::Millis(300);
constexpr TimeDelta kMaxRtcpFeedbackInterval = TimeDelta::Millis(5000);
constexpr int kLimitNumPackets = 20;

const char kBweLosExperiment[] = "WebRTC-BweLossExperiment";

bool BweLossExperimentIsEnabled(const FieldTrialsView& field_trials) {
  return field_trials.IsEnabled(kBweLosExperiment);
}

bool ReadBweLossExperimentParameters(const FieldTrialsView& field_trials,
                                     float* low_loss_threshold,
                                     float* high_loss_threshold,
                                     uint32_t* bitrate_threshold_kbps) {
  RTC_DCHECK(low_loss_threshold);
  RTC_DCHECK(high_loss_threshold);
  RTC_DCHECK(bitrate_threshold_kbps);
  std::string experiment_string = field_trials.Lookup(kBweLosExperiment);
  int parsed_values =
      sscanf(experiment_string.c_str(), "Enabled-%f,%f,%u", low_loss_threshold,
             high_loss_threshold, bitrate_threshold_kbps);
  if (parsed_values == 3) {
    RTC_CHECK_GT(*low_loss_threshold, 0.0f)
        << "Loss threshold must be greater than 0.";
    RTC_CHECK_LE(*low_loss_threshold, 1.0f)
        << "Loss threshold must be less than or equal to 1.";
    RTC_CHECK_GT(*high_loss_threshold, 0.0f)
        << "Loss threshold must be greater than 0.";
    RTC_CHECK_LE(*high_loss_threshold, 1.0f)
        << "Loss threshold must be less than or equal to 1.";
    RTC_CHECK_LE(*low_loss_threshold, *high_loss_threshold)
        << "The low loss threshold must be less than or equal to the high loss "
           "threshold.";
    RTC_CHECK_GE(*bitrate_threshold_kbps, 0)
        << "Bitrate threshold can't be negative.";
    RTC_CHECK_LT(*bitrate_threshold_kbps,
                 std::numeric_limits<int>::max() / 1000)
        << "Bitrate must be smaller enough to avoid overflows.";
    return true;
  }
  RTC_LOG(LS_WARNING) << "Failed to parse parameters for BweLossExperiment "
                         "experiment from field trial string. Using default.";
  *low_loss_threshold = kDefaultLowLossThreshold;
  *high_loss_threshold = kDefaultHighLossThreshold;
  *bitrate_threshold_kbps = kDefaultBitrateThreshold.kbps();
  return false;
}
}  // namespace

LossBasedBwe::LossBasedBwe(const FieldTrialsView* field_trials)
    : field_trials_(field_trials),
      loss_based_bwe_v2_(std::make_unique<LossBasedBweV2>(field_trials)),
      low_loss_threshold_(kDefaultLowLossThreshold),
      high_loss_threshold_(kDefaultHighLossThreshold),
      bitrate_threshold_(kDefaultBitrateThreshold) {
  if (BweLossExperimentIsEnabled(*field_trials)) {
    uint32_t bitrate_threshold_kbps;
    if (ReadBweLossExperimentParameters(*field_trials, &low_loss_threshold_,
                                        &high_loss_threshold_,
                                        &bitrate_threshold_kbps)) {
      RTC_LOG(LS_INFO) << "Enabled BweLossExperiment with parameters "
                       << low_loss_threshold_ << ", " << high_loss_threshold_
                       << ", " << bitrate_threshold_kbps;
      bitrate_threshold_ = DataRate::KilobitsPerSec(bitrate_threshold_kbps);
    }
  }
}

void LossBasedBwe::OnTransportPacketsFeedback(
    const TransportPacketsFeedback& report,
    DataRate delay_based,
    std::optional<DataRate> acknowledged_bitrate,
    bool is_probe_rate,
    bool in_alr) {
  if (is_probe_rate) {
    // delay_based bitrate overrides loss based BWE unless
    // loss_based_bandwidth_estimator_v2_ is used or until
    // loss_based_bandwidth_estimator_v2_ is ready.
    SetStartRate(delay_based);
  }
  delay_based_bwe_ = delay_based;
  if (!loss_based_bwe_v2_->IsEnabled()) {
    return;
  }
  if (acknowledged_bitrate.has_value()) {
    loss_based_bwe_v2_->SetAcknowledgedBitrate(*acknowledged_bitrate);
  }
  loss_based_bwe_v2_->UpdateBandwidthEstimate(report.packet_feedbacks,
                                              delay_based, in_alr);
}

void LossBasedBwe::OnRouteChanged() {
  current_state_ = LossBasedState::kDelayBasedEstimate;
  lost_packets_since_last_loss_update_ = 0;
  expected_packets_since_last_loss_update_ = 0;
  min_bitrate_history_.clear();
  delay_based_bwe_ = DataRate::PlusInfinity();
  fallback_estimate_ = DataRate::Zero();
  has_decreased_since_last_fraction_loss_ = false;
  last_loss_feedback_ = Timestamp::MinusInfinity();
  last_loss_packet_report_ = Timestamp::MinusInfinity();
  last_fraction_loss_ = 0;
  last_logged_fraction_loss_ = 0;
  last_round_trip_time_ = TimeDelta::Zero();
  time_last_decrease_ = Timestamp::MinusInfinity();
  first_report_time_ = Timestamp::MinusInfinity();
  loss_based_bwe_v2_ = std::make_unique<LossBasedBweV2>(field_trials_);
}

void LossBasedBwe::SetConfiguredMinMaxBitrate(DataRate min_rate,
                                              DataRate max_rate) {
  configured_min_rate_ = min_rate;
  configured_max_rate_ = max_rate;
  loss_based_bwe_v2_->SetMinMaxBitrate(min_rate, max_rate);
}

void LossBasedBwe::SetStartRate(DataRate fallback_rate) {
  // Clear last sent bitrate history so the new value can be used directly
  // and not capped.
  min_bitrate_history_.clear();
  fallback_estimate_ = fallback_rate;
}

void LossBasedBwe::OnPacketLossReport(int64_t packets_lost,
                                      int64_t packets_received,
                                      TimeDelta round_trip_time,
                                      Timestamp at_time) {
  last_loss_feedback_ = at_time;
  last_round_trip_time_ = round_trip_time;
  if (first_report_time_.IsInfinite()) {
    first_report_time_ = at_time;
  }
  int64_t number_of_packets = packets_lost + packets_received;
  // Check sequence number diff and weight loss report
  if (number_of_packets <= 0) {
    return;
  }
  int64_t expected =
      expected_packets_since_last_loss_update_ + number_of_packets;

  // Don't generate a loss rate until it can be based on enough packets.
  if (expected < kLimitNumPackets) {
    // Accumulate reports.
    expected_packets_since_last_loss_update_ = expected;
    lost_packets_since_last_loss_update_ += packets_lost;
    return;
  }

  has_decreased_since_last_fraction_loss_ = false;
  int64_t lost_q8 =
      std::max<int64_t>(lost_packets_since_last_loss_update_ + packets_lost, 0)
      << 8;
  last_fraction_loss_ = std::min<int>(lost_q8 / expected, 255);

  // Reset accumulators.
  lost_packets_since_last_loss_update_ = 0;
  expected_packets_since_last_loss_update_ = 0;
  last_loss_packet_report_ = at_time;
}

bool LossBasedBwe::OnPeriodicProcess(Timestamp at_time) {
  UpdateMinHistory(at_time);
  if (loss_based_bwe_v2_->IsReady()) {
    return false;
  }

  TimeDelta time_since_loss_packet_report = at_time - last_loss_packet_report_;
  if (time_since_loss_packet_report < 1.2 * kMaxRtcpFeedbackInterval) {
    // We only care about loss above a given bitrate threshold.
    float loss = last_fraction_loss_ / 256.0f;
    // We only make decisions based on loss when the bitrate is above a
    // threshold. This is a crude way of handling loss which is uncorrelated
    // to congestion.
    if (fallback_estimate_ < bitrate_threshold_ ||
        loss <= low_loss_threshold_) {
      // Loss < 2%: Increase rate by 8% of the min bitrate in the last
      // kBweIncreaseInterval.
      // Note that by remembering the bitrate over the last second one can
      // rampup up one second faster than if only allowed to start ramping
      // at 8% per second rate now. E.g.:
      //   If sending a constant 100kbps it can rampup immediately to 108kbps
      //   whenever a receiver report is received with lower packet loss.
      //   If instead one would do: current_bitrate_ *= 1.08^(delta time),
      //   it would take over one second since the lower packet loss to
      //   achieve 108kbps.
      // Add 1 kbps extra, just to make sure that we do not get stuck
      // (gives a little extra increase at low rates, negligible at higher
      // rates).
      UpdateFallbackEstimate(
          DataRate::BitsPerSec(
              min_bitrate_history_.front().second.bps() * 1.08 + 0.5) +
          DataRate::BitsPerSec(1000));
      return true;
    } else if (fallback_estimate_ > bitrate_threshold_) {
      if (loss <= high_loss_threshold_) {
        // Loss between 2% - 10%: Do nothing.
      } else {
        // Loss > 10%: Limit the rate decreases to once a kBweDecreaseInterval
        // + rtt.
        if (!has_decreased_since_last_fraction_loss_ &&
            (at_time - time_last_decrease_) >=
                (kBweDecreaseInterval + last_round_trip_time_)) {
          time_last_decrease_ = at_time;

          // Reduce rate:
          //   newRate = rate * (1 - 0.5*lossRate);
          //   where packetLoss = 256*lossRate;
          UpdateFallbackEstimate(DataRate::BitsPerSec(
              (fallback_estimate_.bps() *
               static_cast<double>(512 - last_fraction_loss_)) /
              512.0));
          has_decreased_since_last_fraction_loss_ = true;
          return true;
        }
      }
    }
  }
  return false;
}

void LossBasedBwe::UpdateMinHistory(Timestamp at_time) {
  // Remove old data points from history.
  // Since history precision is in ms, add one so it is able to increase
  // bitrate if it is off by as little as 0.5ms.
  while (!min_bitrate_history_.empty() &&
         at_time - min_bitrate_history_.front().first + TimeDelta::Millis(1) >
             kBweIncreaseInterval) {
    min_bitrate_history_.pop_front();
  }

  // Typical minimum sliding-window algorithm: Pop values higher than current
  // bitrate before pushing it.
  while (!min_bitrate_history_.empty() &&
         fallback_estimate_ <= min_bitrate_history_.back().second) {
    min_bitrate_history_.pop_back();
  }

  min_bitrate_history_.push_back(std::make_pair(at_time, fallback_estimate_));
}

DataRate LossBasedBwe::GetEstimate() {
  if (loss_based_bwe_v2_->IsReady()) {
    LossBasedBweV2::Result result = loss_based_bwe_v2_->GetLossBasedResult();
    current_state_ = result.state;
    return result.bandwidth_estimate;
  }
  return fallback_estimate_;
}

void LossBasedBwe::UpdateFallbackEstimate(DataRate new_estimate) {
  fallback_estimate_ = std::min({delay_based_bwe_, new_estimate});
  fallback_estimate_ = std::max(configured_min_rate_, new_estimate);
}

}  // namespace webrtc
