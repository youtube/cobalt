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
#include <utility>

#include "starboard/common/log.h"
#if SB_PLAYER_ENABLE_VIDEO_DUMPER
#include "starboard/shared/starboard/player/video_dmp_writer.h"
#endif  // SB_PLAYER_ENABLE_VIDEO_DUMPER

namespace {

using starboard::shared::starboard::player::InputBuffer;
using starboard::shared::starboard::player::InputBuffers;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

SbTime CalculateMediaTime(SbTime media_time,
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
    const SbMediaAudioSampleInfo* audio_sample_info,
    SbPlayerDeallocateSampleFunc sample_deallocate_func,
    SbPlayerDecoderStatusFunc decoder_status_func,
    SbPlayerStatusFunc player_status_func,
    SbPlayerErrorFunc player_error_func,
    void* context,
    starboard::scoped_ptr<PlayerWorker::Handler> player_worker_handler)
    : sample_deallocate_func_(sample_deallocate_func),
      context_(context),
      media_time_updated_at_(SbTimeGetMonotonicNow()) {
  worker_ = starboard::make_scoped_ptr(PlayerWorker::CreateInstance(
      audio_codec, video_codec, player_worker_handler.Pass(),
      std::bind(&SbPlayerPrivate::UpdateMediaInfo, this, _1, _2, _3, _4),
      decoder_status_func, player_status_func, player_error_func, this,
      context));

  ++number_of_players_;
  SB_DLOG(INFO) << "Creating SbPlayerPrivate. There are " << number_of_players_
                << " players.";
}

// static
SbPlayerPrivate* SbPlayerPrivate::CreateInstance(
    SbMediaAudioCodec audio_codec,
    SbMediaVideoCodec video_codec,
    const SbMediaAudioSampleInfo* audio_sample_info,
    SbPlayerDeallocateSampleFunc sample_deallocate_func,
    SbPlayerDecoderStatusFunc decoder_status_func,
    SbPlayerStatusFunc player_status_func,
    SbPlayerErrorFunc player_error_func,
    void* context,
    starboard::scoped_ptr<PlayerWorker::Handler> player_worker_handler) {
  SbPlayerPrivate* ret = new SbPlayerPrivate(
      audio_codec, video_codec, audio_sample_info, sample_deallocate_func,
      decoder_status_func, player_status_func, player_error_func, context,
      player_worker_handler.Pass());

  if (ret && ret->worker_) {
    return ret;
  }
  delete ret;
  return nullptr;
}

void SbPlayerPrivate::Seek(SbTime seek_to_time, int ticket) {
  {
    starboard::ScopedLock lock(mutex_);
    SB_DCHECK(ticket_ != ticket);
    media_time_ = seek_to_time;
    media_time_updated_at_ = SbTimeGetMonotonicNow();
    is_progressing_ = false;
    ticket_ = ticket;
  }

  worker_->Seek(seek_to_time, ticket);
}

void SbPlayerPrivate::WriteSamples(const SbPlayerSampleInfo* sample_infos,
                                   int number_of_sample_infos) {
  SB_DCHECK(sample_infos);
  SB_DCHECK(number_of_sample_infos > 0);

  if (sample_infos[0].type == kSbMediaTypeVideo) {
    const auto& last_sample_info = sample_infos[number_of_sample_infos - 1];
    total_video_frames_ += number_of_sample_infos;
    frame_width_ = last_sample_info.video_sample_info.frame_width;
    frame_height_ = last_sample_info.video_sample_info.frame_height;
  }

  InputBuffers input_buffers;
  input_buffers.reserve(number_of_sample_infos);
  for (int i = 0; i < number_of_sample_infos; i++) {
    input_buffers.push_back(new InputBuffer(sample_deallocate_func_, this,
                                            context_, sample_infos[i]));
#if SB_PLAYER_ENABLE_VIDEO_DUMPER
    using ::starboard::shared::starboard::player::video_dmp::VideoDmpWriter;
    VideoDmpWriter::OnPlayerWriteSample(this, input_buffers.back());
#endif  // SB_PLAYER_ENABLE_VIDEO_DUMPER
  }
  worker_->WriteSamples(std::move(input_buffers));
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

void SbPlayerPrivate::GetInfo(SbPlayerInfo2* out_player_info) {
  SB_DCHECK(out_player_info != NULL);

  starboard::ScopedLock lock(mutex_);
  out_player_info->duration = SB_PLAYER_NO_DURATION;
  if (is_paused_ || !is_progressing_) {
    out_player_info->current_media_timestamp = media_time_;
  } else {
    out_player_info->current_media_timestamp =
        CalculateMediaTime(media_time_, media_time_updated_at_, playback_rate_);
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
                                      bool is_progressing) {
  starboard::ScopedLock lock(mutex_);
  if (ticket_ != ticket) {
    return;
  }
  media_time_ = media_time;
  is_progressing_ = is_progressing;
  media_time_updated_at_ = SbTimeGetMonotonicNow();
  dropped_video_frames_ = dropped_video_frames;
}

SbDecodeTarget SbPlayerPrivate::GetCurrentDecodeTarget() {
  return worker_->GetCurrentDecodeTarget();
}
