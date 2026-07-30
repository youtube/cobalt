// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/video_bitrate_suggester.h"

#include <algorithm>
#include <limits>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "media/base/media_switches.h"
#include "media/cast/constants.h"

namespace media::cast {

VideoBitrateSuggester::VideoBitrateSuggester(
    const FrameSenderConfig& config,
    GetVideoNetworkBandwidthCB get_bitrate_cb)
    : get_bandwidth_cb_(std::move(get_bitrate_cb)),
      min_bitrate_(config.min_bitrate),
      max_bitrate_(config.max_bitrate),
      max_frame_rate_(config.max_frame_rate),
      suggested_bitrate_(max_bitrate_) {
  CHECK_GE(max_bitrate_, min_bitrate_);
}

VideoBitrateSuggester::~VideoBitrateSuggester() = default;

uint32_t VideoBitrateSuggester::GetSuggestedBitrate() {
  // The bitrate retrieved from the callback is based on network usage, however
  // we also need to consider how well this device is handling encoding at
  // this bitrate overall.
  const uint32_t suggested_bitrate =
      std::min(get_bandwidth_cb_.Run(), suggested_bitrate_);

  // Honor the config boundaries.
  return std::clamp(suggested_bitrate, min_bitrate_, max_bitrate_);
}

void VideoBitrateSuggester::RecordShouldDropNextFrame(bool should_drop) {
  ++frames_requested_;
  if (should_drop) {
    ++frames_dropped_;
  }

  if (base::FeatureList::IsEnabled(
          media::kCastStreamingExponentialVideoBitrateAlgorithm)) {
    UpdateSuggestionUsingExponentialAlgorithm();
  } else {
    UpdateSuggestionUsingLinearAlgorithm();
  }
}

void VideoBitrateSuggester::UpdateSuggestionUsingExponentialAlgorithm() {
  // This is the V2 implementation of the exponential algorithm.
  int window_size =
      media::kCastStreamingExponentialVideoBitrateAlgorithmWindowSize.Get();
  const double multiplier =
      media::
          kCastStreamingExponentialVideoBitrateAlgorithmDynamicWindowMultiplier
              .Get();
  if (multiplier > 0.0) {
    window_size = std::max(1, static_cast<int>(max_frame_rate_ * multiplier));
  }

  if (frames_requested_ >= window_size) {
    const int drop_threshold =
        media::kCastStreamingExponentialVideoBitrateAlgorithmDropThreshold
            .Get();
    const double increase_factor =
        media::kCastStreamingExponentialVideoBitrateAlgorithmIncreaseFactor
            .Get();
    const double decrease_factor =
        media::kCastStreamingExponentialVideoBitrateAlgorithmDecreaseFactor
            .Get();

    suggested_bitrate_ =
        (frames_dropped_ > drop_threshold)
            ? std::max<uint32_t>(
                  min_bitrate_,
                  static_cast<uint32_t>(suggested_bitrate_ * decrease_factor))
            : std::min<uint32_t>(
                  max_bitrate_,
                  static_cast<uint32_t>(suggested_bitrate_ * increase_factor));

    // Reset the frame counts to start a new window.
    frames_requested_ = 0;
    frames_dropped_ = 0;
  }
}

void VideoBitrateSuggester::UpdateSuggestionUsingLinearAlgorithm() {
  // The window size here is fixed at 100 frames, which ends up being between
  // ~1.5 seconds at 60 FPS and ~3.3 seconds at 30 FPS. This is a relatively
  // good balance between responsiveness and stability.
  static constexpr int kWindowSize = 100;
  if (frames_requested_ == kWindowSize) {
    // If more than 2% of frames were dropped, decrease the bitrate.
    // 1% is common on even good WiFi, so 2% is a better threshold for
    // sustained performance issues.
    static constexpr int kDropThreshold = 2;

    // The adjust step allows for eight steps between the minimum and maximum
    // bitrate. These concrete delta values ensure that the encoder does not
    // have to make major adjustments for only minor changes in bitrate.
    static constexpr int kBitrateSteps = 8;
    const uint32_t adjustment = (max_bitrate_ - min_bitrate_) / kBitrateSteps;

    if (frames_dropped_ > kDropThreshold) {
      // Decrease bitrate: protect against unsigned underflow first.
      auto decreased_bitrate = (suggested_bitrate_ > adjustment)
                                   ? suggested_bitrate_ - adjustment
                                   : 0;

      suggested_bitrate_ = std::max(min_bitrate_, decreased_bitrate);
    } else {
      // Increase bitrate: clamp to the absolute maximum
      suggested_bitrate_ =
          std::min(max_bitrate_, suggested_bitrate_ + adjustment);
    }

    // Reset the frame counts to start a new window.
    frames_requested_ = 0;
    frames_dropped_ = 0;
  }
}

}  // namespace media::cast
