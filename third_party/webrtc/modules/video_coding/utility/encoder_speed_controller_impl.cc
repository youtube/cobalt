/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/encoder_speed_controller_impl.h"

#include <cstddef>
#include <memory>
#include <optional>

#include "api/units/time_delta.h"
#include "api/video_codecs/encoder_speed_controller.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {
// We want to increase the speed quickly in case we're overusing,
// but be slower to decrease speed and thus try using more resources.

// Constants governing how we adapt towards slower speed/higher quality.
constexpr double kSlowFilterAlpha = 0.1;
constexpr int kMinSamplesForDecreasedSpeed = 6;
constexpr double kReducedSpeedUtilizationFactorThreshold = 0.50;

// Constants governing how we adapt towards fasted speed/lower quality.
// Allows the utilization up to 95% for the fast reacting smaller window, but
// only up to 75% utilization for the slower and longer window.
constexpr double kFastFilterAlpha = 0.3;
constexpr int kMinSamplesForIncreasedSpeedFastFilter = 4;
constexpr int kMinSamplesForIncreasedSpeedSlowFilter = 10;
constexpr double kIncreasedSpeedUtilizationFactorThresholdSlowFilter = 0.75;
constexpr double kIncreasedSpeedUtilizationFactorThresholdFastFilter = 0.95;

// Exp filter constant for calculating the "current" QP.
constexpr double kQpFilterAlpha = 0.2;

// Keyframes usually take 4-5 times longer to encoder, but they are
// rare (relatively speaking) so divide the encode time by this
// factor in order to not over-react.
constexpr double kKeyframeEncodeTimeCompensator = 3.5;

}  // namespace

EncoderSpeedControllerImpl::EncoderSpeedControllerImpl(
    const Config& config,
    TimeDelta start_frame_interval)
    : config_(config),
      frame_interval_(start_frame_interval),
      current_speed_index_(config.start_speed_index),
      num_samples_(0),
      slow_filtered_encode_time_ms_(0),
      fast_filtered_encode_time_ms_(0),
      filtered_qp_(0) {}

std::unique_ptr<webrtc::EncoderSpeedController>
EncoderSpeedControllerImpl::Create(
    const webrtc::EncoderSpeedController::Config& config,
    TimeDelta start_frame_interval) {
  if (config.speed_levels.empty()) {
    RTC_LOG(LS_WARNING) << "EncoderSpeedController: No speed levels provided.";
    return nullptr;
  }

  if (config.start_speed_index < 0 ||
      static_cast<size_t>(config.start_speed_index) >=
          config.speed_levels.size()) {
    RTC_LOG(LS_WARNING) << "EncoderSpeedController: Invalid start_speed_index: "
                        << config.start_speed_index;
    return nullptr;
  }

  std::optional<int> last_seen_qp_limit = std::nullopt;

  for (size_t i = 0; i < config.speed_levels.size(); ++i) {
    const auto& speed_level = config.speed_levels[i];
    if (speed_level.min_qp.has_value()) {
      if (last_seen_qp_limit.has_value() &&
          *speed_level.min_qp > *last_seen_qp_limit) {
        RTC_LOG(LS_WARNING) << "EncoderSpeedController: Speed level " << i
                            << " has min_qp value of " << *speed_level.min_qp
                            << " which is higher than the previous limit of "
                            << *last_seen_qp_limit;
        return nullptr;
      }
      last_seen_qp_limit = speed_level.min_qp;
    }
  }

  if (start_frame_interval.IsInfinite() || start_frame_interval.us() <= 0) {
    RTC_LOG(LS_WARNING)
        << "EncoderSpeedController: Invalid start frame interval: "
        << start_frame_interval;
    return nullptr;
  }

  return std::unique_ptr<EncoderSpeedControllerImpl>(
      new EncoderSpeedControllerImpl(config, start_frame_interval));
}

void EncoderSpeedControllerImpl::ResetStats() {
  num_samples_ = 0;
  slow_filtered_encode_time_ms_ = 0;
  fast_filtered_encode_time_ms_ = 0;
  filtered_qp_ = 0;
}

void EncoderSpeedControllerImpl::IncreaseSpeed() {
  if (static_cast<size_t>(current_speed_index_) <
      config_.speed_levels.size() - 1) {
    RTC_LOG(LS_VERBOSE) << "EncoderSpeedController: Increasing speed from "
                        << current_speed_index_ << " to "
                        << current_speed_index_ + 1;
    ++current_speed_index_;
    ResetStats();
  }
}

void EncoderSpeedControllerImpl::DecreaseSpeed() {
  if (current_speed_index_ > 0) {
    RTC_LOG(LS_VERBOSE) << "EncoderSpeedController: Decreasing speed from "
                        << current_speed_index_ << " to "
                        << current_speed_index_ - 1;
    --current_speed_index_;
    ResetStats();
  }
}

void EncoderSpeedControllerImpl::SetFrameInterval(TimeDelta frame_interval) {
  frame_interval_ = frame_interval;
}

EncoderSpeedController::EncodeSettings
EncoderSpeedControllerImpl::GetEncodeSettings(
    EncoderSpeedController::FrameEncodingInfo frame_info) {
  RTC_CHECK(frame_interval_.IsFinite());
  EncodeSettings settings;
  const Config::SpeedLevel& current_level =
      config_.speed_levels[current_speed_index_];

  settings.speed =
      current_level.speeds[static_cast<int>(frame_info.reference_type)];

  return settings;
}

void EncoderSpeedControllerImpl::OnEncodedFrame(
    EncoderSpeedController::EncodeResults results) {
  double encode_tims_ms = results.encode_time.us() / 1000.0;
  if (results.frame_info.reference_type == ReferenceClass::kKey) {
    encode_tims_ms /= kKeyframeEncodeTimeCompensator;
  }

  if (num_samples_ == 0) {
    if (results.frame_info.is_repeat_frame) {
      RTC_LOG(LS_WARNING) << "EncoderSpeedController: Try to start "
                             "measurements with a repeat frame.";
      return;
    }
    slow_filtered_encode_time_ms_ = encode_tims_ms;
    fast_filtered_encode_time_ms_ = encode_tims_ms;
    filtered_qp_ = results.qp;
    ++num_samples_;
  } else if (!results.frame_info.is_repeat_frame) {
    // Add encode time measurement to filtered members. Don't count repeat
    // frames as they have artificially low complexity due to zero movement.
    ++num_samples_;
    slow_filtered_encode_time_ms_ =
        (kSlowFilterAlpha * encode_tims_ms) +
        ((1 - kSlowFilterAlpha) * slow_filtered_encode_time_ms_);
    fast_filtered_encode_time_ms_ =
        (kFastFilterAlpha * encode_tims_ms) +
        ((1 - kFastFilterAlpha) * fast_filtered_encode_time_ms_);
    filtered_qp_ =
        (kQpFilterAlpha * results.qp) + ((1 - kQpFilterAlpha) * filtered_qp_);
  }

  if (ShouldIncreaseSpeed()) {
    // Using too much resources or QP is good enough, try to increase the speed.
    IncreaseSpeed();
  } else if (ShouldDecreaseSpeed()) {
    // Headroom exists to reduce speed.
    DecreaseSpeed();
  }
}

bool EncoderSpeedControllerImpl::ShouldIncreaseSpeed() const {
  if (current_speed_index_ >=
      static_cast<int>(config_.speed_levels.size()) - 1) {
    // Already at max speed.
    return false;
  }
  if (num_samples_ < kMinSamplesForIncreasedSpeedFastFilter) {
    // Too few samples for fast filter.
    return false;
  }
  if (fast_filtered_encode_time_ms_ >
      (kIncreasedSpeedUtilizationFactorThresholdFastFilter *
       frame_interval_.ms())) {
    // Fast filter has detected overuse.
    return true;
  }

  if (num_samples_ < kMinSamplesForIncreasedSpeedSlowFilter) {
    // Too few samples for slow filter and filtered QP.
    return false;
  }
  if (slow_filtered_encode_time_ms_ >
      (kIncreasedSpeedUtilizationFactorThresholdSlowFilter *
       frame_interval_.ms())) {
    // Slow filter has detected overuse.
    return true;
  }

  const Config::SpeedLevel& current_level =
      config_.speed_levels[current_speed_index_];
  if (current_level.min_qp.has_value() &&
      filtered_qp_ < *current_level.min_qp) {
    // Quality is already high, increase speed.
    return true;
  }

  return false;
}

bool EncoderSpeedControllerImpl::ShouldDecreaseSpeed() const {
  if (current_speed_index_ <= 0) {
    // Already at slowest speed.
    return false;
  }

  if (num_samples_ < kMinSamplesForDecreasedSpeed) {
    // Not enough samples collected.
    return false;
  }

  if (slow_filtered_encode_time_ms_ >
      (kReducedSpeedUtilizationFactorThreshold * frame_interval_.ms())) {
    // Not enough headroom exists to reduce speed.
    return false;
  }

  // Headroom exists, check conditions of the next slower speed.
  const Config::SpeedLevel& next_speed_level =
      config_.speed_levels[current_speed_index_ - 1];
  if (!next_speed_level.min_qp.has_value() ||
      filtered_qp_ >= *next_speed_level.min_qp) {
    // No QP limit, or current QP high enough - allow slower speed.
    return true;
  }

  return false;
}
}  // namespace webrtc
