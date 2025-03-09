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

namespace starboard {
namespace android {
namespace shared {

// static
SbPlayerPrivate* ExoPlayer::CreateInstance(
    SbPlayerDeallocateSampleFunc sample_deallocate_func,
    SbPlayerDecoderStatusFunc decoder_status_func,
    SbPlayerStatusFunc player_status_func,
    SbPlayerErrorFunc player_error_func,
    void* context) {
  ExoPlayer* exo_player =
      new ExoPlayer(sample_deallocate_func, decoder_status_func,
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

void ExoPlayer::Seek(int64_t seek_to_timestamp, int ticket) const {
  SB_DCHECK(bridge_);
  SB_LOG(INFO) << "Called Seek() to " << seek_to_timestamp;
  bridge_->Seek(seek_to_timestamp, ticket);
}

void ExoPlayer::WriteSamples(SbMediaType sample_type,
                             const SbPlayerSampleInfo* sample_infos,
                             int number_of_sample_infos) {
  SB_LOG(INFO) << "Writing sample";
  bridge_->WriteSamples(sample_type, sample_infos, number_of_sample_infos);
  if (sample_type == kSbMediaTypeVideo &&
      (frame_height_ == 0 && frame_width_ == 0)) {
    frame_height_ = sample_infos->video_sample_info.stream_info.frame_height;
    frame_width_ = sample_infos->video_sample_info.stream_info.frame_width;
  }
  // sample_deallocate_func_(player_, reinterpret_cast<void*>(this),
  // sample_infos->buffer);
  SB_LOG(INFO) << "Sample deallocated";
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
    bridge_->Play();
  } else if (playback_rate_ == 1.0 && (playback_rate != playback_rate_)) {
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
  out_player_info->duration = SB_PLAYER_NO_DURATION;
  out_player_info->frame_width = frame_width_;
  out_player_info->frame_height = frame_height_;
  out_player_info->is_paused = (playback_rate_ == 0.0);
  out_player_info->volume = volume_;
  out_player_info->total_video_frames = 0;
  out_player_info->dropped_video_frames = 0;
  out_player_info->corrupted_video_frames = 0;
  out_player_info->playback_rate = playback_rate_;
}

ExoPlayer::ExoPlayer(SbPlayerDeallocateSampleFunc sample_deallocate_func,
                     SbPlayerDecoderStatusFunc decoder_status_func,
                     SbPlayerStatusFunc player_status_func,
                     SbPlayerErrorFunc player_error_func,
                     void* context)
    : sample_deallocate_func_(sample_deallocate_func),
      decoder_status_func_(decoder_status_func),
      player_status_func_(player_status_func),
      player_error_func_(player_error_func),
      context_(context),
      player_(reinterpret_cast<SbPlayerPrivate*>(this)) {
  bridge_.reset(new ExoPlayerBridge(
      decoder_status_func, player_status_func, player_error_func,
      reinterpret_cast<SbPlayerPrivate*>(this), context));
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
