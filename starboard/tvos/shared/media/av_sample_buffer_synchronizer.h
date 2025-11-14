// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_TVOS_SHARED_MEDIA_AV_SAMPLE_BUFFER_SYNCHRONIZER_H_
#define STARBOARD_TVOS_SHARED_MEDIA_AV_SAMPLE_BUFFER_SYNCHRONIZER_H_

#import <AVFoundation/AVFoundation.h>

#include <mutex>

#include "starboard/shared/starboard/player/filter/media_time_provider.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/tvos/shared/media/av_sample_buffer_audio_renderer.h"
#include "starboard/tvos/shared/media/av_sample_buffer_video_renderer.h"

namespace starboard {

class AVSBSynchronizer : public MediaTimeProvider, private JobQueue::JobOwner {
 public:
  AVSBSynchronizer();
  ~AVSBSynchronizer() override;

  void Play() override;
  void Pause() override;
  void SetPlaybackRate(double playback_rate) override;
  void Seek(int64_t seek_to_time) override;
  int64_t GetCurrentMediaTime(bool* is_playing,
                              bool* is_eos_played,
                              bool* is_underflow,
                              double* playback_rate) override;

  void SetRenderer(AVSBAudioRenderer* audio_renderer);
  void SetRenderer(AVSBVideoRenderer* video_renderer);
  void ResetRenderers();

 private:
  AVSBSynchronizer(const AVSBSynchronizer&) = delete;
  AVSBSynchronizer& operator=(const AVSBSynchronizer&) = delete;

  void CheckUnderflow();
  void UpdateIdleTimer();

  // Objective-C object.
  AVSampleBufferRenderSynchronizer* synchronizer_ = nullptr;

  // Pointers to renderers.
  AVSBAudioRenderer* audio_renderer_ = nullptr;
  AVSBVideoRenderer* video_renderer_ = nullptr;

  std::mutex mutex_;
  int64_t seek_to_time_ = 0;
  int64_t media_time_offset_ = 10 * 1000000;  // 10s
  double playback_rate_ = 1.0;
  bool seeking_ = false;
  bool paused_ = true;
  bool is_underflow_ = false;
  bool is_idle_timer_disabled_ = false;
};

}  // namespace starboard

#endif  // STARBOARD_TVOS_SHARED_MEDIA_AV_SAMPLE_BUFFER_SYNCHRONIZER_H_
