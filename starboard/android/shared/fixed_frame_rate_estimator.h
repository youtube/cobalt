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

#ifndef STARBOARD_ANDROID_SHARED_FIXED_FRAME_RATE_ESTIMATOR_H_
#define STARBOARD_ANDROID_SHARED_FIXED_FRAME_RATE_ESTIMATOR_H_

#include <array>
#include <cstdint>
#include <limits>

namespace starboard {

// Detects and refines a fixed video frame rate from presentation timestamps
// using a dual-matcher outlier filter modeled after AndroidX Media3.
class FixedFrameRateEstimator {
 public:
  static constexpr int kConsecutiveMatchingFrameDurationsForSync = 15;
  static constexpr int64_t kMaxMatchingFrameDifferenceNs = 1'000'000;  // 1 ms
  static constexpr int64_t kTimeUnset = std::numeric_limits<int64_t>::min();
  static constexpr float kNoFrameRate = -1.0f;

  FixedFrameRateEstimator();

  void Reset();
  void OnNextFrame(int64_t frame_presentation_time_ns);

  bool IsSynced() const;
  int64_t GetMatchingFrameDurationSumNs() const;
  int64_t GetFrameDurationNs() const;
  float GetFrameRate() const { return frame_rate_estimate_; }
  int64_t frame_index() const { return frame_index_; }

 private:
  class Matcher {
   public:
    Matcher();
    void Reset();
    bool IsSynced() const;
    bool IsLastFrameOutlier() const;
    int64_t GetMatchingFrameDurationSumNs() const {
      return matching_frame_duration_sum_ns_;
    }
    int64_t GetFrameDurationNs() const;
    void OnNextFrame(int64_t frame_presentation_time_ns);

   private:
    static int GetRecentFrameOutlierIndex(int64_t frame_count) {
      return static_cast<int>(frame_count %
                              kConsecutiveMatchingFrameDurationsForSync);
    }

    int64_t first_frame_presentation_time_ns_ = 0;
    int64_t first_frame_duration_ns_ = 0;
    int64_t last_frame_presentation_time_ns_ = 0;
    int64_t frame_count_ = 0;
    int64_t matching_frame_count_ = 0;
    int64_t matching_frame_duration_sum_ns_ = 0;
    std::array<bool, kConsecutiveMatchingFrameDurationsForSync>
        recent_frame_outlier_flags_{};
    int recent_frame_outlier_count_ = 0;
  };

  void UpdateReportedFrameRate();

  std::array<Matcher, 2> matchers_;
  int current_matcher_index_ = 0;
  bool candidate_matcher_active_ = false;
  bool has_last_frame_presentation_time_ = false;
  int64_t last_frame_presentation_time_ns_ = kTimeUnset;
  int frames_without_sync_count_ = 0;
  float frame_rate_estimate_ = kNoFrameRate;
  int64_t frame_index_ = 0;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_FIXED_FRAME_RATE_ESTIMATOR_H_
