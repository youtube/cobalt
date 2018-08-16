// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include <functional>

#include "starboard/log.h"

namespace {

using starboard::shared::starboard::player::InputBuffer;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

SbTime GetMediaTime(SbTime media_time,
                    SbTimeMonotonic media_time_update_time,
                    double playback_rate) {
  SbTimeMonotonic elapsed = SbTimeGetMonotonicNow() - media_time_update_time;
  return media_time + static_cast<SbTime>(elapsed * playback_rate);
}

}  // namespace

int SbPlayerPrivate::number_of_players_ = 0;

SbPlayerPrivate::SbPlayerPrivate(
    SbMediaAudioCodec audio_codec,
    SbMediaVideoCodec video_codec,
    SbPlayerDeallocateSampleFunc sample_deallocate_func,
    SbPlayerDecoderStatusFunc decoder_status_func,
    SbPlayerStatusFunc player_status_func,
#if SB_HAS(PLAYER_ERROR_MESSAGE)
    SbPlayerErrorFunc player_error_func,
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)
    void* context,
    starboard::scoped_ptr<PlayerWorker::Handler> player_worker_handler)
    : sample_deallocate_func_(sample_deallocate_func),
      context_(context),
      media_time_updated_at_(SbTimeGetMonotonicNow()),
      worker_(new PlayerWorker(
          audio_codec,
          video_codec,
          player_worker_handler.Pass(),
          std::bind(&SbPlayerPrivate::UpdateMediaInfo, this, _1, _2, _3, _4),
          decoder_status_func,
          player_status_func,
#if SB_HAS(PLAYER_ERROR_MESSAGE)
          player_error_func,
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)
          this,
          context)) {
  ++number_of_players_;
}

void SbPlayerPrivate::Seek(SbTime seek_to_time, int ticket) {
  {
    starboard::ScopedLock lock(mutex_);
    SB_DCHECK(ticket_ != ticket);
    media_time_ = seek_to_time;
    media_time_updated_at_ = SbTimeGetMonotonicNow();
    ticket_ = ticket;
  }

  worker_->Seek(seek_to_time, ticket);
}

void SbPlayerPrivate::WriteSample(
    SbMediaType sample_type,
    const void* const* sample_buffers,
    const int* sample_buffer_sizes,
    int number_of_sample_buffers,
    SbTime sample_timestamp,
    const SbMediaVideoSampleInfo* video_sample_info,
    const SbDrmSampleInfo* sample_drm_info) {
  if (sample_type == kSbMediaTypeVideo) {
    ++total_video_frames_;
  }
  starboard::scoped_refptr<InputBuffer> input_buffer = new InputBuffer(
      sample_type, sample_deallocate_func_, this, context_, sample_buffers,
      sample_buffer_sizes, number_of_sample_buffers, sample_timestamp,
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

#if SB_API_VERSION < 10
void SbPlayerPrivate::GetInfo(SbPlayerInfo* out_player_info) {
#else   // SB_API_VERSION < 10
void SbPlayerPrivate::GetInfo(SbPlayerInfo2* out_player_info) {
#endif  // SB_API_VERSION < 10
  SB_DCHECK(out_player_info != NULL);

  starboard::ScopedLock lock(mutex_);
#if SB_API_VERSION < 10
  out_player_info->duration_pts = SB_PLAYER_NO_DURATION;
  if (is_paused_ || underflow_) {
    out_player_info->current_media_pts = SB_TIME_TO_SB_MEDIA_TIME(media_time_);
  } else {
    out_player_info->current_media_pts = SB_TIME_TO_SB_MEDIA_TIME(
        GetMediaTime(media_time_, media_time_updated_at_, playback_rate_));
  }
#else   // SB_API_VERSION < 10
  out_player_info->duration = SB_PLAYER_NO_DURATION;
  if (is_paused_ || underflow_) {
    out_player_info->current_media_timestamp = media_time_;
  } else {
    out_player_info->current_media_timestamp =
        GetMediaTime(media_time_, media_time_updated_at_, playback_rate_);
  }
#endif  // SB_API_VERSION < 10

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
  is_paused_ = pause;
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

void SbPlayerPrivate::UpdateMediaInfo(SbTime media_time,
                                      int dropped_video_frames,
                                      int ticket,
                                      bool underflow) {
  starboard::ScopedLock lock(mutex_);
  if (ticket_ != ticket) {
    return;
  }
  media_time_ = media_time;
  underflow_ = underflow;
  media_time_updated_at_ = SbTimeGetMonotonicNow();
  dropped_video_frames_ = dropped_video_frames;
}

SbDecodeTarget SbPlayerPrivate::GetCurrentDecodeTarget() {
  return worker_->GetCurrentDecodeTarget();
}
