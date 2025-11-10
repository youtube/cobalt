// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_DECODER_STATE_TRACKER_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_DECODER_STATE_TRACKER_H_

#include <functional>
#include <iosfwd>
#include <map>
#include <mutex>
#include <string_view>

#include "starboard/shared/starboard/player/job_queue.h"

namespace starboard {

class DecoderStateTracker
    : private shared::starboard::player::JobQueue::JobOwner {
 public:
  using StateChangedCB = std::function<void()>;

  struct State {
    int decoding_frames = 0;
    int decoded_frames = 0;

    int total_frames() const { return decoding_frames + decoded_frames; }
  };

  DecoderStateTracker(int max_frames,
                      StateChangedCB state_changed_cb,
                      shared::starboard::player::JobQueue* job_queue);
  ~DecoderStateTracker() = default;

  void SetFrameAdded(int64_t presentation_time_us);
  void SetFrameDecoded(int64_t presentation_time_us);
  void SetFrameReleasedAt(int64_t presentation_time_us, int64_t release_us);
  void Reset();

  State GetCurrentStateForTest() const;
  bool CanAcceptMore();

 private:
  enum class FrameStatus {
    kDecoding,
    kDecoded,
  };

  State GetCurrentState_Locked() const;
  bool IsFull_Locked() const;
  void EngageKillSwitch_Locked(std::string_view reason, int64_t pts);
  void LogStateAndReschedule(int64_t log_interval_us);

  const StateChangedCB state_changed_cb_;

  mutable std::mutex mutex_;
  std::map<int64_t, FrameStatus> frames_in_flight_;  // GUARDED_BY(mutex_)
  bool disabled_ = false;                            // GUARDED_BY(mutex_)
  int max_frames_;                                   // GUARDED_BY(mutex_)
  bool reached_max_ = false;                         // GUARDED_BY(mutex_)
};

std::ostream& operator<<(std::ostream& os,
                         const DecoderStateTracker::State& status);

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_DECODER_STATE_TRACKER_H_
