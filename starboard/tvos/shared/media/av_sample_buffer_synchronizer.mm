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

#include "starboard/tvos/shared/media/av_sample_buffer_synchronizer.h"

#include "starboard/tvos/shared/application_darwin.h"

namespace starboard {

AVSBSynchronizer::AVSBSynchronizer(JobQueue* job_queue) : JobOwner(job_queue) {
  @autoreleasepool {
    synchronizer_ = [[AVSampleBufferRenderSynchronizer alloc] init];
  }  // @autoreleasepool
}

AVSBSynchronizer::~AVSBSynchronizer() {
  SB_DCHECK(BelongsToCurrentThread());
  if (is_idle_timer_disabled_) {
    ApplicationDarwin::DecrementIdleTimerLockCount();
  }
}

void AVSBSynchronizer::Play() {
  SB_DCHECK(BelongsToCurrentThread());
  @autoreleasepool {
    {
      std::lock_guard lock(mutex_);
      paused_ = false;
      // Set media time at the first time Play() is called after seeking.
      if (seeking_) {
        [synchronizer_
            setRate:playback_rate_
               time:CMTimeMake(seek_to_time_ + media_time_offset_, 1000000)];
        seeking_ = false;
        // Start underflow checking.
        Schedule(std::bind(&AVSBSynchronizer::CheckUnderflow, this));
        return;
      }
    }
    if (!is_underflow_) {
      synchronizer_.rate = playback_rate_;
    }
  }  // @autoreleasepool
  UpdateIdleTimer();
}

void AVSBSynchronizer::Pause() {
  SB_DCHECK(BelongsToCurrentThread());
  @autoreleasepool {
    synchronizer_.rate = 0.0;
  }  // @autoreleasepool
  {
    std::lock_guard lock(mutex_);
    paused_ = true;
  }
  UpdateIdleTimer();
}

void AVSBSynchronizer::SetPlaybackRate(double playback_rate) {
  SB_DCHECK(BelongsToCurrentThread());
  playback_rate_ = playback_rate;
  @autoreleasepool {
    if (seeking_ || paused_ || is_underflow_) {
      synchronizer_.rate = 0.0;
    } else {
      synchronizer_.rate = playback_rate_;
    }
  }  // @autoreleasepool
  UpdateIdleTimer();
}

void AVSBSynchronizer::Seek(int64_t seek_to_time) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(seek_to_time >= 0);
  // Pause() should be called in FilterBasedPlayerWorkerHandler::Seek() before
  // Seek().
  SB_DCHECK(synchronizer_.rate == 0);
  // Cancel underflow checking jobs.
  CancelPendingJobs();

  @autoreleasepool {
    CMTime media_time = CMTimeConvertScale(synchronizer_.currentTime, 1000000,
                                           kCMTimeRoundingMethod_QuickTime);
    {
      std::lock_guard lock(mutex_);
      seeking_ = true;
      if (seek_to_time < media_time.value + 1000000) {
        // It's very hard to exceed int64 value range and should be safe.
        media_time_offset_ = media_time.value - seek_to_time + 10 * 1000000;
      }
      seek_to_time_ = seek_to_time;
    }
    [synchronizer_ setRate:0 time:kCMTimeZero];
  }  // @autoreleasepool

  if (audio_renderer_) {
    audio_renderer_->Seek(seek_to_time, media_time_offset_);
  }
  if (video_renderer_) {
    // AVSBVideoRenderer::Seek() was called in FilterBasedPlayerWorkerHandler
    // before AVSBSynchronizer::Seek() is called. So, we don't need to call
    // AVSBVideoRenderer::Seek() here.
    video_renderer_->SetMediaTimeOffset(media_time_offset_);
  }
}

int64_t AVSBSynchronizer::GetCurrentMediaTime(bool* is_playing,
                                              bool* is_eos_played,
                                              bool* is_underflow,
                                              double* playback_rate) {
  SB_DCHECK(is_playing);
  SB_DCHECK(is_eos_played);
  SB_DCHECK(is_underflow);
  SB_DCHECK(playback_rate);

  @autoreleasepool {
    std::lock_guard lock(mutex_);
    *is_playing = !paused_ && !seeking_;
    *is_eos_played = false;  // It's not used.
    *is_underflow = *is_playing && is_underflow_;
    *playback_rate = synchronizer_.rate;
    if (seeking_) {
      return seek_to_time_;
    }
    int64_t media_time = CMTimeConvertScale(synchronizer_.currentTime, 1000000,
                                            kCMTimeRoundingMethod_QuickTime)
                             .value;
    return std::max(media_time - media_time_offset_, 0ll);
  }  // @autoreleasepool
}

void AVSBSynchronizer::SetRenderer(AVSBAudioRenderer* audio_renderer) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(audio_renderer);
  @autoreleasepool {
    audio_renderer_ = audio_renderer;
    [synchronizer_ addRenderer:audio_renderer_->GetAudioRenderer()];
  }  // @autoreleasepool
}

void AVSBSynchronizer::SetRenderer(AVSBVideoRenderer* video_renderer) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(video_renderer);
  @autoreleasepool {
    video_renderer_ = video_renderer;
    [synchronizer_ addRenderer:video_renderer->GetDisplayLayer()];
  }  // @autoreleasepool
}

void AVSBSynchronizer::ResetRenderers() {
  @autoreleasepool {
    if (audio_renderer_) {
      [synchronizer_ removeRenderer:audio_renderer_->GetAudioRenderer()
                             atTime:kCMTimeZero
                  completionHandler:nil];
      audio_renderer_ = nullptr;
    }
    if (video_renderer_) {
      [synchronizer_ removeRenderer:video_renderer_->GetDisplayLayer()
                             atTime:kCMTimeZero
                  completionHandler:nil];
      video_renderer_ = nullptr;
    }
  }  // @autoreleasepool
}

void AVSBSynchronizer::CheckUnderflow() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(!seeking_);

  const int64_t kCheckUnderflowIntervalUsec = 40000;  // 40ms
  Schedule(std::bind(&AVSBSynchronizer::CheckUnderflow, this),
           kCheckUnderflowIntervalUsec);

  bool is_underflow = false;
  if ((audio_renderer_ && audio_renderer_->IsUnderflow()) ||
      (video_renderer_ && video_renderer_->IsUnderflow())) {
    is_underflow = true;
  }

  if (is_underflow != is_underflow_) {
    if (is_underflow) {
      SB_LOG(WARNING) << "Pause the playback due to underflow.";
      synchronizer_.rate = 0.0;
    } else if (!paused_) {
      SB_LOG(WARNING) << "Resume the playback from underflow.";
      synchronizer_.rate = playback_rate_;
    }
    std::lock_guard lock(mutex_);
    is_underflow_ = is_underflow;
  }
}

void AVSBSynchronizer::UpdateIdleTimer() {
  SB_DCHECK(BelongsToCurrentThread());
  bool new_idle_timer_disabled = playback_rate_ > 0.0 && !paused_;
  if (new_idle_timer_disabled == is_idle_timer_disabled_) {
    return;
  }
  is_idle_timer_disabled_ = new_idle_timer_disabled;
  if (is_idle_timer_disabled_) {
    ApplicationDarwin::IncrementIdleTimerLockCount();
  } else {
    ApplicationDarwin::DecrementIdleTimerLockCount();
  }
}

}  // namespace starboard
