// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/android/shared/fixed_frame_rate_estimator.h"

#include <cmath>
#include <cstdlib>

namespace starboard {
namespace {

constexpr int64_t kNanosPerSecond = 1'000'000'000LL;
constexpr int64_t kMinMatchingFrameDurationForHighConfidenceNs =
    5'000'000'000LL;
constexpr float kMinMediaFrameRateChangeHighConfidence = 0.1f;
constexpr float kMinMediaFrameRateChangeLowConfidence = 1.0f;
constexpr int kMinFramesWithoutSyncToClearEstimate =
    2 * FixedFrameRateEstimator::kConsecutiveMatchingFrameDurationsForSync;

}  // namespace

FixedFrameRateEstimator::Matcher::Matcher() {
  Reset();
}

void FixedFrameRateEstimator::Matcher::Reset() {
  first_frame_presentation_time_ns_ = 0;
  first_frame_duration_ns_ = 0;
  last_frame_presentation_time_ns_ = 0;
  frame_count_ = 0;
  matching_frame_count_ = 0;
  matching_frame_duration_sum_ns_ = 0;
  recent_frame_outlier_flags_.fill(false);
  recent_frame_outlier_count_ = 0;
}

bool FixedFrameRateEstimator::Matcher::IsSynced() const {
  return frame_count_ > kConsecutiveMatchingFrameDurationsForSync &&
         recent_frame_outlier_count_ == 0;
}

bool FixedFrameRateEstimator::Matcher::IsLastFrameOutlier() const {
  if (frame_count_ == 0) {
    return false;
  }
  return recent_frame_outlier_flags_[GetRecentFrameOutlierIndex(frame_count_ -
                                                                1)];
}

int64_t FixedFrameRateEstimator::Matcher::GetFrameDurationNs() const {
  return matching_frame_count_ == 0
             ? 0
             : (matching_frame_duration_sum_ns_ / matching_frame_count_);
}

void FixedFrameRateEstimator::Matcher::OnNextFrame(
    int64_t frame_presentation_time_ns) {
  if (frame_count_ == 0) {
    first_frame_presentation_time_ns_ = frame_presentation_time_ns;
  } else if (frame_count_ == 1) {
    int64_t initial_duration_ns = 0;
    if (frame_presentation_time_ns <= first_frame_presentation_time_ns_ ||
        __builtin_sub_overflow(frame_presentation_time_ns,
                               first_frame_presentation_time_ns_,
                               &initial_duration_ns) ||
        initial_duration_ns <= 0) {
      // Restart anchor if timestamps jump backward or overflow before lock.
      first_frame_presentation_time_ns_ = frame_presentation_time_ns;
      last_frame_presentation_time_ns_ = frame_presentation_time_ns;
      return;
    }
    first_frame_duration_ns_ = initial_duration_ns;
    matching_frame_duration_sum_ns_ = first_frame_duration_ns_;
    matching_frame_count_ = 1;
  } else {
    const int outlier_index = GetRecentFrameOutlierIndex(frame_count_);
    bool matches = false;
    int64_t last_frame_duration_ns = 0;
    if (frame_presentation_time_ns > last_frame_presentation_time_ns_ &&
        !__builtin_sub_overflow(frame_presentation_time_ns,
                                last_frame_presentation_time_ns_,
                                &last_frame_duration_ns)) {
      const int64_t diff_ns =
          last_frame_duration_ns >= first_frame_duration_ns_
              ? (last_frame_duration_ns - first_frame_duration_ns_)
              : (first_frame_duration_ns_ - last_frame_duration_ns);
      matches = diff_ns <= kMaxMatchingFrameDifferenceNs;
    }
    int64_t new_sum_ns = 0;
    if (matches &&
        !__builtin_add_overflow(matching_frame_duration_sum_ns_,
                                last_frame_duration_ns, &new_sum_ns)) {
      ++matching_frame_count_;
      matching_frame_duration_sum_ns_ = new_sum_ns;
      if (recent_frame_outlier_flags_[outlier_index]) {
        recent_frame_outlier_flags_[outlier_index] = false;
        --recent_frame_outlier_count_;
      }
    } else if (!matches && !recent_frame_outlier_flags_[outlier_index]) {
      recent_frame_outlier_flags_[outlier_index] = true;
      ++recent_frame_outlier_count_;
    }
  }

  ++frame_count_;
  last_frame_presentation_time_ns_ = frame_presentation_time_ns;
}

FixedFrameRateEstimator::FixedFrameRateEstimator() = default;

void FixedFrameRateEstimator::Reset() {
  matchers_[0].Reset();
  matchers_[1].Reset();
  current_matcher_index_ = 0;
  candidate_matcher_active_ = false;
  has_last_frame_presentation_time_ = false;
  last_frame_presentation_time_ns_ = kTimeUnset;
  frames_without_sync_count_ = 0;
  frame_rate_estimate_ = kNoFrameRate;
  frame_index_ = 0;
}

void FixedFrameRateEstimator::OnNextFrame(int64_t frame_presentation_time_ns) {
  if (has_last_frame_presentation_time_ &&
      frame_presentation_time_ns == last_frame_presentation_time_ns_) {
    return;
  }
  ++frame_index_;
  Matcher& current = matchers_[current_matcher_index_];
  Matcher& candidate = matchers_[current_matcher_index_ ^ 1];

  current.OnNextFrame(frame_presentation_time_ns);
  if (current.IsSynced()) {
    candidate_matcher_active_ = false;
  } else if (has_last_frame_presentation_time_) {
    if (!candidate_matcher_active_ || candidate.IsLastFrameOutlier()) {
      // Seed candidate matcher with the previous timestamp so it tracks the
      // most recent frame duration on format or cadence transitions.
      candidate.Reset();
      candidate.OnNextFrame(last_frame_presentation_time_ns_);
    }
    candidate_matcher_active_ = true;
    candidate.OnNextFrame(frame_presentation_time_ns);
  }
  if (candidate_matcher_active_ && candidate.IsSynced()) {
    current_matcher_index_ ^= 1;
    candidate_matcher_active_ = false;
  }
  has_last_frame_presentation_time_ = true;
  last_frame_presentation_time_ns_ = frame_presentation_time_ns;
  frames_without_sync_count_ = matchers_[current_matcher_index_].IsSynced()
                                   ? 0
                                   : (frames_without_sync_count_ + 1);
  UpdateReportedFrameRate();
}

bool FixedFrameRateEstimator::IsSynced() const {
  return matchers_[current_matcher_index_].IsSynced();
}

int64_t FixedFrameRateEstimator::GetMatchingFrameDurationSumNs() const {
  return IsSynced()
             ? matchers_[current_matcher_index_].GetMatchingFrameDurationSumNs()
             : 0;
}

int64_t FixedFrameRateEstimator::GetFrameDurationNs() const {
  return IsSynced() ? matchers_[current_matcher_index_].GetFrameDurationNs()
                    : kTimeUnset;
}

void FixedFrameRateEstimator::UpdateReportedFrameRate() {
  const Matcher& current = matchers_[current_matcher_index_];
  const bool synced = current.IsSynced();
  const int64_t duration_ns = synced ? current.GetFrameDurationNs() : 0;
  const float candidate_rate =
      (synced && duration_ns > 0)
          ? static_cast<float>(static_cast<double>(kNanosPerSecond) /
                               static_cast<double>(duration_ns))
          : kNoFrameRate;
  if (candidate_rate == frame_rate_estimate_) {
    return;
  }

  bool should_update = false;
  if (candidate_rate != kNoFrameRate && frame_rate_estimate_ != kNoFrameRate) {
    // Require 5s of matching frames before applying sub-1fps refinements so
    // Surface.setFrameRate is not re-invoked on tiny container rounding noise.
    const bool high_confidence =
        synced && current.GetMatchingFrameDurationSumNs() >=
                      kMinMatchingFrameDurationForHighConfidenceNs;
    const float min_change = high_confidence
                                 ? kMinMediaFrameRateChangeHighConfidence
                                 : kMinMediaFrameRateChangeLowConfidence;
    should_update =
        std::abs(candidate_rate - frame_rate_estimate_) >= min_change;
  } else if (candidate_rate != kNoFrameRate) {
    should_update = true;
  } else {
    should_update =
        frames_without_sync_count_ >= kMinFramesWithoutSyncToClearEstimate;
  }

  if (should_update) {
    frame_rate_estimate_ = candidate_rate;
  }
}

}  // namespace starboard
