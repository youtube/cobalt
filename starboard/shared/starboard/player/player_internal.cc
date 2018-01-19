// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "starboard/shared/starboard/player/player_internal.h"

#include "starboard/log.h"

using starboard::shared::starboard::player::InputBuffer;

namespace {

SbMediaTime GetMediaTime(SbMediaTime media_pts,
                         SbTimeMonotonic media_pts_update_time) {
  SbTimeMonotonic elapsed = SbTimeGetMonotonicNow() - media_pts_update_time;
  return media_pts + elapsed * kSbMediaTimeSecond / kSbTimeSecond;
}

}  // namespace

SbPlayerPrivate::SbPlayerPrivate(
    SbMediaAudioCodec audio_codec,
    SbMediaTime duration_pts,
    SbPlayerDeallocateSampleFunc sample_deallocate_func,
    SbPlayerDecoderStatusFunc decoder_status_func,
    SbPlayerStatusFunc player_status_func,
    void* context,
    starboard::scoped_ptr<PlayerWorker::Handler> player_worker_handler)
    : sample_deallocate_func_(sample_deallocate_func),
      context_(context),
      ticket_(SB_PLAYER_INITIAL_TICKET),
      duration_pts_(duration_pts),
      media_pts_(0),
      media_pts_update_time_(SbTimeGetMonotonicNow()),
      frame_width_(0),
      frame_height_(0),
      is_paused_(true),
      playback_rate_(1.0),
      volume_(1.0),
      total_video_frames_(0),
      dropped_video_frames_(0),
      worker_(new PlayerWorker(this,
                               audio_codec,
                               player_worker_handler.Pass(),
                               decoder_status_func,
                               player_status_func,
                               this,
                               context)) {}

void SbPlayerPrivate::Seek(SbMediaTime seek_to_pts, int ticket) {
  {
    starboard::ScopedLock lock(mutex_);
    SB_DCHECK(ticket_ != ticket);
    media_pts_ = seek_to_pts;
    media_pts_update_time_ = SbTimeGetMonotonicNow();
    ticket_ = ticket;
  }

  worker_->Seek(seek_to_pts, ticket);
}

void SbPlayerPrivate::WriteSample(
    SbMediaType sample_type,
    const void* const* sample_buffers,
    const int* sample_buffer_sizes,
    int number_of_sample_buffers,
    SbMediaTime sample_pts,
    const SbMediaVideoSampleInfo* video_sample_info,
    const SbDrmSampleInfo* sample_drm_info) {
  if (sample_type == kSbMediaTypeVideo) {
    ++total_video_frames_;
  }
  starboard::scoped_refptr<InputBuffer> input_buffer = new InputBuffer(
      sample_type, sample_deallocate_func_, this, context_, sample_buffers,
      sample_buffer_sizes, number_of_sample_buffers, sample_pts,
      video_sample_info, sample_drm_info);
  worker_->WriteSample(input_buffer);
}

void SbPlayerPrivate::WriteEndOfStream(SbMediaType stream_type) {
  worker_->WriteEndOfStream(stream_type);
}

void SbPlayerPrivate::SetBounds(int z_index,
                                int x,
                                int y,
                                int width,
                                int height) {
  PlayerWorker::Bounds bounds = {z_index, x, y, width, height};
  worker_->SetBounds(bounds);
  // TODO: Wait until a frame is rendered with the updated bounds.
}

void SbPlayerPrivate::GetInfo(SbPlayerInfo* out_player_info) {
  SB_DCHECK(out_player_info != NULL);

  starboard::ScopedLock lock(mutex_);
  out_player_info->duration_pts = duration_pts_;
  if (is_paused_) {
    out_player_info->current_media_pts = media_pts_;
  } else {
    out_player_info->current_media_pts =
        GetMediaTime(media_pts_, media_pts_update_time_);
  }
  out_player_info->frame_width = frame_width_;
  out_player_info->frame_height = frame_height_;
  out_player_info->is_paused = is_paused_;
  out_player_info->volume = volume_;
  out_player_info->total_video_frames = total_video_frames_;
  out_player_info->dropped_video_frames = dropped_video_frames_;
  out_player_info->corrupted_video_frames = 0;
  out_player_info->playback_rate = playback_rate_;
}

void SbPlayerPrivate::SetPause(bool pause) {
  worker_->SetPause(pause);
}

void SbPlayerPrivate::SetPlaybackRate(double playback_rate) {
  playback_rate_ = playback_rate;
  worker_->SetPlaybackRate(playback_rate);
}

void SbPlayerPrivate::SetVolume(double volume) {
  volume_ = volume;
  worker_->SetVolume(volume_);
}

void SbPlayerPrivate::UpdateMediaTime(SbMediaTime media_time, int ticket) {
  starboard::ScopedLock lock(mutex_);
  if (ticket_ != ticket) {
    return;
  }
  media_pts_ = media_time;
  media_pts_update_time_ = SbTimeGetMonotonicNow();
}

void SbPlayerPrivate::UpdateDroppedVideoFrames(int dropped_video_frames) {
  starboard::ScopedLock lock(mutex_);
  dropped_video_frames_ = dropped_video_frames;
}

SbDecodeTarget SbPlayerPrivate::GetCurrentDecodeTarget() {
  return worker_->GetCurrentDecodeTarget();
}
