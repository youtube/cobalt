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

#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_DECODER_STATE_TRACKER_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_DECODER_STATE_TRACKER_H_

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/job_thread.h"

namespace starboard {

class DecoderStateTracker {
 public:
  struct State {
    int decoding_frames = 0;
    int decoded_frames = 0;

    int total_frames() const { return decoding_frames + decoded_frames; }
  };

  explicit DecoderStateTracker(int initial_max_frames);
  ~DecoderStateTracker();

  void TrackNewFrame(int64_t presentation_us);
  void MarkFrameDecoded(int64_t presentation_us);
  void MarkFrameReleased(int64_t presentation_us, int64_t release_us);
  void MarkEosReached();

  bool CanAcceptMore();
  void Reset();

  State GetCurrentStateForTest() const;

 private:
  enum class FrameStatus { kDecoding, kDecoded, kReleased };
  struct FrameInfo {
    FrameStatus status;
    int64_t release_time_us = 0;  // Valid only when status == kReleased
  };

  State GetCurrentState_Locked() const;
  bool IsFull_Locked() const;
  void EngageKillSwitch_Locked(std::string_view reason, int64_t pts);
  void PruneReleasedFrames_Locked();

#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
  void StartPeriodicalLogging(int64_t log_interval_us);
  void LogStateAndReschedule(int64_t log_interval_us);
#endif

  int max_frames_;
  std::atomic_bool disabled_ = false;

  mutable std::mutex mutex_;
  std::vector<std::pair<int64_t, FrameInfo>> frames_in_flight_;
  int pending_released_frames_ = 0;  // Guarded by |mutex_|.
  bool eos_added_ = false;           // Guarded by |mutex_|.
  bool reached_max_ = false;         // Guarded by |mutex_|.

#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
  std::unique_ptr<shared::starboard::player::JobThread> logging_thread_;
#endif
};

std::ostream& operator<<(std::ostream& os,
                         const DecoderStateTracker::State& state);

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_DECODER_STATE_TRACKER_H_
