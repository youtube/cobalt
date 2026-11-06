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

#ifndef STARBOARD_ANDROID_SHARED_AAUDIO_PRESENTED_FRAME_TRACKER_H_
#define STARBOARD_ANDROID_SHARED_AAUDIO_PRESENTED_FRAME_TRACKER_H_

#include <cstdint>
#include <deque>

namespace starboard {

// Tracks the frames copied into an AAudio stream until AAudio reports them
// presented, so that a pull sink can report frames as consumed when they are
// actually output instead of when they are copied. Positions are stream frame
// positions, i.e. the number of frames passed to the stream's data callback
// before, which match AAudioStream_getTimestamp() positions.
//
// Not thread-safe.
class AaudioPresentedFrameTracker {
 public:
  explicit AaudioPresentedFrameTracker(int sample_rate);

  // Forgets all frames in flight, e.g. on flush or when the stream is
  // replaced.
  void Reset();

  // Records |frames| frames copied at stream position |stream_position|.
  // Positions must not decrease, except after Reset().
  void OnFramesCopied(int64_t stream_position, int frames);

  // Returns the number of frames in flight that are presented by |now_us|,
  // given that the frame at |timestamp_position| was presented at
  // |timestamp_us| (from AAudioStream_getTimestamp()), and removes them from
  // the frames in flight. The timestamp may be stale (AAudio may update it only
  // every few bursts), so the presented position is extrapolated to |now_us|,
  // but never beyond |written_position|, the position of the next frame to be
  // written.
  int OnTimestamp(int64_t timestamp_position,
                  int64_t timestamp_us,
                  int64_t now_us,
                  int64_t written_position);

  // Returns the number of frames copied but not presented yet.
  int frames_in_flight() const { return frames_in_flight_; }

 private:
  struct Segment {
    int64_t start;  // Stream position of the first frame.
    int frames;
  };

  const int sample_rate_;
  std::deque<Segment> segments_;
  int frames_in_flight_ = 0;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_AAUDIO_PRESENTED_FRAME_TRACKER_H_
