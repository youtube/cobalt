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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <list>
#include <type_traits>
#include <vector>

#include "starboard/android/shared/video_frame_tracker.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace starboard {
namespace {

template <typename Container>
class MockVideoFrameTracker {
 public:
  explicit MockVideoFrameTracker(int max_pending_frames_size)
      : max_pending_frames_size_(max_pending_frames_size) {}

  void OnInputBuffer(int64_t timestamp) {
    if (frames_to_be_rendered_.empty()) {
      PushBack(frames_to_be_rendered_, timestamp);
      return;
    }

    if (frames_to_be_rendered_.size() >
        static_cast<size_t>(max_pending_frames_size_)) {
      PopFront(frames_to_be_rendered_);
    }

    // Sort by |timestamp|, because |timestamp| won't be monotonic if there are
    // B frames.
    for (auto it = frames_to_be_rendered_.end();
         it != frames_to_be_rendered_.begin();) {
      it--;
      if (*it < timestamp) {
        frames_to_be_rendered_.emplace(++it, timestamp);
        return;
      } else if (*it == timestamp) {
        return;
      }
    }

    EmplaceFront(frames_to_be_rendered_, timestamp);
  }

  void OnFrameRendered(int64_t frame_timestamp) {
    rendered_frames_on_decoder_thread_.push_back(frame_timestamp);
  }

  void Seek(int64_t seek_to_time) {
    UpdateDroppedFrames();
    frames_to_be_rendered_.clear();
    seek_to_time_ = seek_to_time;
  }

  int UpdateAndGetDroppedFrames() {
    UpdateDroppedFrames();
    return dropped_frames_;
  }

 private:
  void PushBack(Container& c, int64_t val) { c.push_back(val); }

  void PopFront(Container& c) {
    if constexpr (std::is_same_v<Container, std::vector<int64_t>>) {
      c.erase(c.begin());
    } else {
      c.pop_front();
    }
  }

  void EmplaceFront(Container& c, int64_t val) {
    if constexpr (std::is_same_v<Container, std::vector<int64_t>>) {
      c.insert(c.begin(), val);
    } else {
      c.emplace_front(val);
    }
  }

  void RemoveUnexpectedRenderedFrames(const Container& frames_to_render,
                                      std::vector<int64_t>* rendered_frames) {
    if (rendered_frames->empty()) {
      return;
    }
    if (frames_to_render.empty()) {
      rendered_frames->clear();
      return;
    }

    const int64_t min_valid_rendered_frame =
        std::max<int64_t>(0, frames_to_render.front() - kMaxAllowedSkew);
    const int64_t max_valid_rendered_frame =
        frames_to_render.back() + kMaxAllowedSkew;
    auto is_not_expected = [min_valid_rendered_frame,
                            max_valid_rendered_frame](int64_t timestamp) {
      return timestamp < min_valid_rendered_frame ||
             max_valid_rendered_frame < timestamp;
    };

    auto to_remove = std::remove_if(rendered_frames->begin(),
                                    rendered_frames->end(), is_not_expected);
    rendered_frames->erase(to_remove, rendered_frames->end());
  }

  void UpdateDroppedFrames() {
    rendered_frames_on_tracker_thread_.swap(rendered_frames_on_decoder_thread_);

    while (!frames_to_be_rendered_.empty() &&
           frames_to_be_rendered_.front() < seek_to_time_) {
      PopFront(frames_to_be_rendered_);
    }

    RemoveUnexpectedRenderedFrames(frames_to_be_rendered_,
                                   &rendered_frames_on_tracker_thread_);

    for (auto rendered_timestamp : rendered_frames_on_tracker_thread_) {
      auto to_render_timestamp = frames_to_be_rendered_.begin();
      while (to_render_timestamp != frames_to_be_rendered_.end() &&
             !(*to_render_timestamp - rendered_timestamp > kMaxAllowedSkew)) {
        if (std::abs(*to_render_timestamp - rendered_timestamp) <=
            kMaxAllowedSkew) {
          to_render_timestamp =
              frames_to_be_rendered_.erase(to_render_timestamp);
        } else if (rendered_timestamp - *to_render_timestamp >
                   kMaxAllowedSkew) {
          ++dropped_frames_;
          to_render_timestamp =
              frames_to_be_rendered_.erase(to_render_timestamp);
        } else {
          ++to_render_timestamp;
        }
      }
    }

    rendered_frames_on_tracker_thread_.clear();
  }

  Container frames_to_be_rendered_;
  const int max_pending_frames_size_;
  int dropped_frames_ = 0;
  int64_t seek_to_time_ = 0;

  std::vector<int64_t> rendered_frames_on_tracker_thread_;
  std::vector<int64_t> rendered_frames_on_decoder_thread_;

  static constexpr int64_t kMaxAllowedSkew = 5000;
};

template <typename Container>
void BM_VideoFrameTracker(::benchmark::State& state) {
  const int kMaxPendingFrames = 256;
  const int kFrameIntervalUs = 16666;  // 60fps
  const int kNumFrames = 1000;
  const int kRenderPeriod = 10;
  const int64_t kStartPts =
      1000000;  // Start from 1s to avoid negative PTS with skew

  for (auto _ : state) {
    MockVideoFrameTracker<Container> tracker(kMaxPendingFrames);

    int64_t next_render_timestamp = 0;

    for (int i = 0; i < kNumFrames; ++i) {
      // Strictly monotonic PTS (IPPP)
      int64_t pts = kStartPts + i * kFrameIntervalUs;

      tracker.OnInputBuffer(pts);

      // Periodically render frames
      if (i > 0 && i % kRenderPeriod == 0) {
        // Render frames up to current i
        for (int j = next_render_timestamp; j <= i; ++j) {
          // Simulate some skew (e.g. +/- 2ms)
          int64_t skew = (j % 3 - 1) * 1000;
          tracker.OnFrameRendered(kStartPts + j * kFrameIntervalUs + skew);
        }
        next_render_timestamp = i + 1;
        tracker.UpdateAndGetDroppedFrames();
      }
    }
  }
  state.SetItemsProcessed(kNumFrames * state.iterations());
}

void BM_VideoFrameTrackerBaseline(::benchmark::State& state) {
  const int kMaxPendingFrames = 256;
  const int kFrameIntervalUs = 16666;  // 60fps
  const int kNumFrames = 1000;
  const int kRenderPeriod = 10;
  const int64_t kStartPts = 1000000;

  for (auto _ : state) {
    VideoFrameTracker tracker(kMaxPendingFrames);

    int64_t next_render_timestamp = 0;

    for (int i = 0; i < kNumFrames; ++i) {
      // Strictly monotonic PTS (IPPP)
      int64_t pts = kStartPts + i * kFrameIntervalUs;

      tracker.OnInputBuffer(pts);

      // Periodically render frames
      if (i > 0 && i % kRenderPeriod == 0) {
        // Render frames up to current i
        for (int j = next_render_timestamp; j <= i; ++j) {
          // Simulate some skew (e.g. +/- 2ms)
          int64_t skew = (j % 3 - 1) * 1000;
          tracker.OnFrameRendered(kStartPts + j * kFrameIntervalUs + skew);
        }
        next_render_timestamp = i + 1;
        tracker.UpdateAndGetDroppedFrames();
      }
    }
  }
  state.SetItemsProcessed(kNumFrames * state.iterations());
}

BENCHMARK_TEMPLATE(BM_VideoFrameTracker, std::list<int64_t>);
BENCHMARK_TEMPLATE(BM_VideoFrameTracker, std::deque<int64_t>);
BENCHMARK_TEMPLATE(BM_VideoFrameTracker, std::vector<int64_t>);
BENCHMARK(BM_VideoFrameTrackerBaseline);

}  // namespace
}  // namespace starboard
