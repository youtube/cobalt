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
    SbPlayerErrorFunc player_error_func) {
  ExoPlayer* exo_player =
      new ExoPlayer(sample_deallocate_func, decoder_status_func,
                    player_status_func, player_error_func);
  return reinterpret_cast<SbPlayerPrivate*>(exo_player);
}

// static
ExoPlayer* ExoPlayer::GetExoPlayerForSbPlayer(SbPlayer player) {
  SB_DCHECK(player != kSbPlayerInvalid);
  return reinterpret_cast<ExoPlayer*>(player);
}

ExoPlayer::~ExoPlayer() {
  bridge_->Stop();
  bridge_.reset();
}

void ExoPlayer::Seek(int64_t seek_to_timestamp, int ticket) const {
  SB_DCHECK(bridge_);
}

void ExoPlayer::WriteSamples(SbMediaType sample_type,
                             const SbPlayerSampleInfo* sample_infos,
                             int number_of_sample_infos) const {
  bridge_->WriteSamples(sample_type, sample_infos, number_of_sample_infos);
}

void ExoPlayer::WriteEndOfStream(SbMediaType stream_type) const {
  SB_DCHECK(bridge_);
}

void ExoPlayer::SetBounds(int z_index,
                          int x,
                          int y,
                          int width,
                          int height) const {
  SB_DCHECK(bridge_);
}

bool ExoPlayer::SetPlaybackRate(double playback_rate) const {
  SB_DCHECK(bridge_);
  if (!bridge_->is_valid()) {
    return false;
  }

  // TODO: Support variable playback rate
  if (playback_rate != 0.0 && playback_rate != 1.0) {
    SB_LOG(ERROR) << "Unsupported playback rate " << playback_rate;
  }
  return false;
}

void ExoPlayer::SetVolume(double volume) const {
  SB_DCHECK(bridge_);
  if (!bridge_->is_valid()) {
    return;
  }

  bool volume_set = bridge_->SetVolume(volume);
  if (!volume_set) {
    SB_LOG(WARNING) << "Failed to set ExoPlayer volume";
  }
}

void ExoPlayer::GetInfo(SbPlayerInfo* out_player_info) const {
  SB_DCHECK(bridge_);
  if (!bridge_->is_valid()) {
    return;
  }
}

ExoPlayer::ExoPlayer(SbPlayerDeallocateSampleFunc sample_deallocate_func,
                     SbPlayerDecoderStatusFunc decoder_status_func,
                     SbPlayerStatusFunc player_status_func,
                     SbPlayerErrorFunc player_error_func)
    : sample_deallocate_func_(sample_deallocate_func),
      decoder_status_func_(decoder_status_func),
      player_status_func_(player_status_func),
      player_error_func_(player_error_func),
      player_(reinterpret_cast<SbPlayerPrivate*>(this)) {
  bridge_.reset(new ExoPlayerBridge);
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
