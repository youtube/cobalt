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
#include <memory>
#include <utility>

#include "starboard/common/log.h"
#include "starboard/common/media.h"
#include "starboard/common/time.h"

#if SB_PLAYER_ENABLE_VIDEO_DUMPER
#include SB_PLAYER_DMP_WRITER_INCLUDE_PATH
#endif  // SB_PLAYER_ENABLE_VIDEO_DUMPER

namespace {

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

int64_t CalculateMediaTime(int64_t media_time,
                           int64_t media_time_update_time,
                           double playback_rate) {
  int64_t elapsed = starboard::CurrentMonotonicTime() - media_time_update_time;
  return media_time + static_cast<int64_t>(elapsed * playback_rate);
}

}  // namespace

int SbPlayerPrivate::number_of_players_ = 0;

SbPlayerPrivate::SbPlayerPrivate(
    SbMediaAudioCodec audio_codec,
    SbMediaVideoCodec video_codec,
    SbPlayerDeallocateSampleFunc sample_deallocate_func,
    SbPlayerDecoderStatusFunc decoder_status_func,
    SbPlayerStatusFunc player_status_func,
    SbPlayerErrorFunc player_error_func,
    void* context,
    std::unique_ptr<PlayerWorker::Handler> player_worker_handler)
    : sample_deallocate_func_(sample_deallocate_func),
      context_(context),
      media_time_updated_at_(starboard::CurrentMonotonicTime()) {
  worker_ = std::unique_ptr<PlayerWorker>(PlayerWorker::CreateInstance(
      audio_codec, video_codec, std::move(player_worker_handler),
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
    SbPlayerDeallocateSampleFunc sample_deallocate_func,
    SbPlayerDecoderStatusFunc decoder_status_func,
    SbPlayerStatusFunc player_status_func,
    SbPlayerErrorFunc player_error_func,
    void* context,
    std::unique_ptr<PlayerWorker::Handler> player_worker_handler) {
  SbPlayerPrivate* ret = new SbPlayerPrivate(
      audio_codec, video_codec, sample_deallocate_func, decoder_status_func,
      player_status_func, player_error_func, context,
      std::move(player_worker_handler));

  if (ret && ret->worker_) {
    return ret;
  }
  delete ret;
  return nullptr;
}

void SbPlayerPrivate::Seek(int64_t seek_to_time, int ticket) {
  {
    starboard::ScopedLock lock(mutex_);
    SB_DCHECK(ticket_ != ticket);
    media_time_ = seek_to_time;
    media_time_updated_at_ = starboard::CurrentMonotonicTime();
    is_progressing_ = false;
    ticket_ = ticket;
  }

  worker_->Seek(seek_to_time, ticket);
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

void SbPlayerPrivate::UpdateMediaInfo(int64_t media_time,
                                      int dropped_video_frames,
                                      int ticket,
                                      bool is_progressing) {
  starboard::ScopedLock lock(mutex_);
  if (ticket_ != ticket) {
    return;
  }
  media_time_ = media_time;
  is_progressing_ = is_progressing;
  media_time_updated_at_ = starboard::CurrentMonotonicTime();
  dropped_video_frames_ = dropped_video_frames;
}

SbDecodeTarget SbPlayerPrivate::GetCurrentDecodeTarget() {
  return worker_->GetCurrentDecodeTarget();
}

bool SbPlayerPrivate::GetAudioConfiguration(
    int index,
    SbMediaAudioConfiguration* out_audio_configuration) {
  SB_DCHECK(index >= 0);
  SB_DCHECK(out_audio_configuration);

  starboard::ScopedLock lock(audio_configurations_mutex_);
  if (audio_configurations_.empty()) {
#if !defined(COBALT_BUILD_TYPE_GOLD)
    int64_t start = starboard::CurrentMonotonicTime();
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)
    for (int i = 0; i < 32; ++i) {
      SbMediaAudioConfiguration audio_configuration;
      if (SbMediaGetAudioConfiguration(i, &audio_configuration)) {
        audio_configurations_.push_back(audio_configuration);
      } else {
        break;
      }
    }
    if (!audio_configurations_.empty() &&
        audio_configurations_[0].connector != kSbMediaAudioConnectorHdmi) {
      // Move the HDMI connector to the very beginning to be backwards
      // compatible.
      for (size_t i = 1; i < audio_configurations_.size(); ++i) {
        if (audio_configurations_[i].connector == kSbMediaAudioConnectorHdmi) {
          std::swap(audio_configurations_[i], audio_configurations_[0]);
          break;
        }
      }
    }
#if !defined(COBALT_BUILD_TYPE_GOLD)
    int64_t elapsed = starboard::CurrentMonotonicTime() - start;
    SB_LOG(INFO)
        << "GetAudioConfiguration(): Updating audio configurations takes "
        << elapsed << " microseconds.";
    for (auto&& audio_configuration : audio_configurations_) {
      SB_LOG(INFO) << "Found audio configuration "
                   << starboard::GetMediaAudioConnectorName(
                          audio_configuration.connector)
                   << ", channels " << audio_configuration.number_of_channels;
    }
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)
  }

  if (index < static_cast<int>(audio_configurations_.size())) {
    *out_audio_configuration = audio_configurations_[index];
    return true;
  }

  return false;
}
