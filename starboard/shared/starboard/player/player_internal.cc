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

using starboard::shared::starboard::player::PlayerWorker;

namespace {

SbMediaTime GetMediaTime(SbMediaTime media_pts,
                         SbTimeMonotonic media_pts_update_time) {
  SbTimeMonotonic elapsed = SbTimeGetMonotonicNow() - media_pts_update_time;
  return media_pts + elapsed * kSbMediaTimeSecond / kSbTimeSecond;
}
}

SbPlayerPrivate::SbPlayerPrivate(
    SbMediaVideoCodec video_codec,
    SbMediaAudioCodec audio_codec,
    SbMediaTime duration_pts,
    SbDrmSession drm,
    SbMediaAudioHeader* audio_header,
    SbPlayerDeallocateSampleFunc sample_deallocator_func,
    SbPlayerDecoderStatusFunc decoder_status_func,
    SbPlayerStatusFunc player_status_func,
    void* context)
    : ticket_(SB_PLAYER_INITIAL_TICKET),
      duration_pts_(duration_pts),
      media_pts_(0),
      media_pts_update_time_(SbTimeGetMonotonicNow()),
      frame_width_(0),
      frame_height_(0),
      is_paused_(true),
      volume_(1.0),
      worker_(this,
              video_codec,
              audio_codec,
              drm,
              *audio_header,
              sample_deallocator_func,
              decoder_status_func,
              player_status_func,
              this,
              context) {}

void SbPlayerPrivate::Seek(SbMediaTime seek_to_pts, int ticket) {
  {
    starboard::ScopedLock lock(mutex_);
    SB_DCHECK(ticket_ != ticket);
    media_pts_ = seek_to_pts;
    media_pts_update_time_ = SbTimeGetMonotonicNow();
    ticket_ = ticket;
  }

  PlayerWorker::SeekEventData data = {seek_to_pts, ticket};
  worker_.EnqueueEvent(data);
}

void SbPlayerPrivate::WriteSample(SbMediaType sample_type,
                                  void* sample_buffer,
                                  int sample_buffer_size,
                                  SbMediaTime sample_pts,
                                  SbMediaVideoSampleInfo* video_sample_info,
                                  SbDrmSampleInfo* sample_drm_info) {
  PlayerWorker::WriteSampleEventData data = {sample_type, sample_buffer,
                                             sample_buffer_size, sample_pts};
  if (video_sample_info) {
    data.video_sample_info = *video_sample_info;
  }
  if (sample_drm_info) {
    data.sample_drm_info = *sample_drm_info;
  }
  worker_.EnqueueEvent(data);
}

void SbPlayerPrivate::WriteEndOfStream(SbMediaType stream_type) {
  PlayerWorker::WriteEndOfStreamEventData data = {stream_type};
  worker_.EnqueueEvent(data);
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
}

void SbPlayerPrivate::SetPause(bool pause) {
  PlayerWorker::SetPauseEventData data = {pause};
  worker_.EnqueueEvent(data);
}

void SbPlayerPrivate::SetVolume(double volume) {
  SB_NOTIMPLEMENTED();
}

void SbPlayerPrivate::UpdateMediaTime(SbMediaTime media_time, int ticket) {
  starboard::ScopedLock lock(mutex_);
  if (ticket_ != ticket) {
    return;
  }
  media_pts_ = media_time;
  media_pts_update_time_ = SbTimeGetMonotonicNow();
}
