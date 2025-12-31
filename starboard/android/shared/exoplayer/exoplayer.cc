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

#include "starboard/android/shared/exoplayer/exoplayer.h"

#include "starboard/common/log.h"
#include "starboard/common/time.h"

namespace starboard {
namespace android {
namespace shared {
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

// static
SbPlayerPrivate* ExoPlayer::CreateInstance(
    const SbPlayerCreationParam* creation_param,
    SbPlayerDeallocateSampleFunc sample_deallocate_func,
    SbPlayerDecoderStatusFunc decoder_status_func,
    SbPlayerStatusFunc player_status_func,
    SbPlayerErrorFunc player_error_func,
    void* context) {
  ExoPlayer* exo_player =
      new ExoPlayer(creation_param, sample_deallocate_func, decoder_status_func,
                    player_status_func, player_error_func, context);
  return reinterpret_cast<SbPlayerPrivate*>(exo_player);
}

// static
ExoPlayer* ExoPlayer::GetExoPlayerForSbPlayer(SbPlayer player) {
  SB_DCHECK(player != kSbPlayerInvalid);
  return reinterpret_cast<ExoPlayer*>(player);
}

ExoPlayer::~ExoPlayer() {
  SB_LOG(INFO) << "Called ExoPlayer Destructor";
  bridge_.reset();
}

void ExoPlayer::Seek(int64_t seek_to_timestamp, int ticket) {
  SB_DCHECK(bridge_);
  SB_LOG(INFO) << "Called Seek() to " << seek_to_timestamp;
  {
    starboard::ScopedLock lock(mutex_);
    SB_DCHECK(ticket_ != ticket);
    media_time_ = seek_to_timestamp;
    media_time_updated_at_ = starboard::CurrentMonotonicTime();
    is_progressing_ = false;
  }
  bridge_->Seek(seek_to_timestamp, ticket);
}

void ExoPlayer::WriteSamples(SbMediaType sample_type,
                             const SbPlayerSampleInfo* sample_infos,
                             int number_of_sample_infos) {
  // SB_LOG(INFO) << "Writing sample with timestamp " << sample_infos->timestamp
  // ;
  bridge_->WriteSamples(sample_type, sample_infos, number_of_sample_infos);
  if (sample_type == kSbMediaTypeVideo &&
      (frame_height_ == 0 && frame_width_ == 0)) {
    frame_height_ = sample_infos->video_sample_info.stream_info.frame_height;
    frame_width_ = sample_infos->video_sample_info.stream_info.frame_width;
  }
}

void ExoPlayer::WriteEndOfStream(SbMediaType stream_type) const {
  SB_DCHECK(bridge_);
  SB_LOG(INFO) << "Called WriteEndOfStream()";
}

void ExoPlayer::SetBounds(int z_index,
                          int x,
                          int y,
                          int width,
                          int height) const {
  SB_DCHECK(bridge_);
  SB_LOG(INFO) << "Called SetBounds()";
}

bool ExoPlayer::SetPlaybackRate(double playback_rate) {
  SB_DCHECK(bridge_);
  SB_LOG(INFO) << "Called SetPlaybackRate() to " << playback_rate;
  if (!bridge_->is_valid()) {
    return false;
  }

  // TODO: Support variable playback rate
  if (playback_rate != 0.0 && playback_rate != 1.0) {
    SB_LOG(ERROR) << "Unsupported playback rate " << playback_rate;
  }

  if (playback_rate_ == 0.0 && (playback_rate != playback_rate_)) {
    is_paused_ = false;
    bridge_->Play();
  } else if (playback_rate_ == 1.0 && (playback_rate != playback_rate_)) {
    is_paused_ = true;
    bridge_->Pause();
  }
  playback_rate_ = playback_rate;
  return false;
}

void ExoPlayer::SetVolume(double volume) {
  SB_DCHECK(bridge_);
  SB_LOG(INFO) << "Called SetVolume() to " << volume;
  if (!bridge_->is_valid()) {
    return;
  }

  bool volume_set = bridge_->SetVolume(volume);
  if (!volume_set) {
    SB_LOG(WARNING) << "Failed to set ExoPlayer volume";
  }
  volume_ = volume;
}

void ExoPlayer::GetInfo(SbPlayerInfo* out_player_info) const {
  SB_DCHECK(bridge_);
  SB_LOG(INFO) << "Called GetInfo()";
  if (!bridge_->is_valid()) {
    return;
  }
  SB_DCHECK(out_player_info != NULL);

  starboard::ScopedLock lock(mutex_);
  if (is_paused_ || !is_progressing_) {
    out_player_info->current_media_timestamp = media_time_;
  } else {
    out_player_info->current_media_timestamp =
        CalculateMediaTime(media_time_, media_time_updated_at_, playback_rate_);
  }
  out_player_info->duration = SB_PLAYER_NO_DURATION;
  out_player_info->frame_width = frame_width_;
  out_player_info->frame_height = frame_height_;
  out_player_info->is_paused = is_paused_;
  out_player_info->volume = volume_;
  out_player_info->total_video_frames = 0;
  out_player_info->dropped_video_frames = dropped_video_frames_;
  out_player_info->corrupted_video_frames = 0;
  out_player_info->playback_rate = playback_rate_;
}

ExoPlayer::ExoPlayer(const SbPlayerCreationParam* creation_param,
                     SbPlayerDeallocateSampleFunc sample_deallocate_func,
                     SbPlayerDecoderStatusFunc decoder_status_func,
                     SbPlayerStatusFunc player_status_func,
                     SbPlayerErrorFunc player_error_func,
                     void* context)
    : sample_deallocate_func_(sample_deallocate_func),
      decoder_status_func_(decoder_status_func),
      player_status_func_(player_status_func),
      player_error_func_(player_error_func),
      context_(context),
      player_(reinterpret_cast<SbPlayerPrivate*>(this)),
      media_time_updated_at_(starboard::CurrentMonotonicTime()) {
  bridge_.reset(new ExoPlayerBridge(
      creation_param, sample_deallocate_func_, decoder_status_func,
      player_status_func, player_error_func,
      std::bind(&ExoPlayer::UpdateMediaInfo, this, _1, _2, _3, _4),
      reinterpret_cast<SbPlayerPrivate*>(this), context));
}

void ExoPlayer::UpdateMediaInfo(int64_t media_time,
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

}  // namespace shared
}  // namespace android
}  // namespace starboard
