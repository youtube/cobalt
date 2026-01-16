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
#include <memory>
#include <mutex>
#include <string_view>

#include "starboard/shared/starboard/player/job_thread.h"

namespace starboard {

// Tracks the number of frames currently inside the decoder (input queue) and
// the output queue to enable flow control.
//
// By limiting the number of frames in flight, we can prevent the decoder from
// buffering an excessive amount of data, which reduces system memory usage.
//
// This class is thread-safe and can be used from multiple threads or
// SequencedTaskRunners.
class DecoderStateTracker {
 public:
  using FrameReleaseCB = std::function<void()>;

  struct State {
    int decoding_frames = 0;
    int decoded_frames = 0;

    int total_frames() const { return decoding_frames + decoded_frames; }
  };

  DecoderStateTracker(int initial_max_frames, FrameReleaseCB frame_released_cb);
  ~DecoderStateTracker();

  void OnFrameAdded(int64_t presentation_us);
  void OnEosAdded();
  void OnFrameDecoded(int64_t presentation_us);
  void OnReleased(int64_t presentation_us, int64_t release_us);

  bool CanAcceptMore() const;
  void Reset();

  State GetCurrentStateForTest() const;

 private:
  enum class FrameStatus {
    kDecoding,
    kDecoded,
  };

  State GetCurrentState_Locked() const;
  bool IsFull_Locked() const;
  void EngageKillSwitch_Locked(std::string_view reason, int64_t pts);

#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
  void StartPeriodicalLogging(int64_t log_interval_us);
  void LogStateAndReschedule(int64_t log_interval_us);
#endif

  const FrameReleaseCB frame_released_cb_;

  mutable std::mutex mutex_;
  std::map<int64_t, FrameStatus> frames_in_flight_;  // Guarded by |mutex_|.
  bool eos_added_ = false;                           // Guarded by |mutex_|.
  bool reached_max_ = false;                         // Guarded by |mutex_|.

  // Non-resettable members start.
  // These variables are preserved across calls to Reset().
  bool disabled_ = false;  // Guarded by |mutex_|.
  int max_frames_;         // Guarded by |mutex_|.
  // Non-resettable members end.

  // NOTE: |job_thread_| must be the last variable declared so that it is
  // destroyed first (following the reverse order of declaration).
  std::unique_ptr<shared::starboard::player::JobThread>
      job_thread_;  // Guarded by |mutex_|.
};

std::ostream& operator<<(std::ostream& os,
                         const DecoderStateTracker::State& status);

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_DECODER_STATE_TRACKER_H_
