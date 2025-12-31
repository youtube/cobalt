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

#include <string>
#include <utility>

#include "starboard/player.h"

#include "starboard/android/shared/exoplayer/exoplayer.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/common/log.h"

using starboard::android::shared::ExoPlayer;

SbPlayer SbPlayerCreate(SbWindow window,
                        const SbPlayerCreationParam* creation_param,
                        SbPlayerDeallocateSampleFunc sample_deallocate_func,
                        SbPlayerDecoderStatusFunc decoder_status_func,
                        SbPlayerStatusFunc player_status_func,
                        SbPlayerErrorFunc player_error_func,
                        void* context,
                        SbDecodeTargetGraphicsContextProvider* provider) {
  if ((creation_param->audio_stream_info.codec != kSbMediaAudioCodecOpus) ||
      (creation_param->video_stream_info.codec != kSbMediaVideoCodecVp9)) {
    return kSbPlayerInvalid;
  }
  return ExoPlayer::CreateInstance(creation_param, sample_deallocate_func,
                                   decoder_status_func, player_status_func,
                                   player_error_func, context);
}

SbPlayerOutputMode SbPlayerGetPreferredOutputMode(
    const SbPlayerCreationParam* creation_param) {
  return kSbPlayerOutputModePunchOut;
}

void SbPlayerDestroy(SbPlayer player) {
  auto exoplayer = ExoPlayer::GetExoPlayerForSbPlayer(player);
  if (exoplayer) {
    delete ExoPlayer::GetExoPlayerForSbPlayer(player);
  }
}

void SbPlayerSeek(SbPlayer player, int64_t seek_to_timestamp, int ticket) {
  auto exoplayer = ExoPlayer::GetExoPlayerForSbPlayer(player);
  if (exoplayer) {
    exoplayer->Seek(seek_to_timestamp, ticket);
  }
}

void SbPlayerWriteSamples(SbPlayer player,
                          SbMediaType sample_type,
                          const SbPlayerSampleInfo* sample_infos,
                          int number_of_sample_infos) {
  SB_DCHECK(number_of_sample_infos == 1);
  auto exoplayer = ExoPlayer::GetExoPlayerForSbPlayer(player);
  if (exoplayer) {
    exoplayer->WriteSamples(sample_type, sample_infos, number_of_sample_infos);
  }
}

int SbPlayerGetMaximumNumberOfSamplesPerWrite(SbPlayer player,
                                              SbMediaType sample_type) {
  return 1;
}

void SbPlayerWriteEndOfStream(SbPlayer player, SbMediaType stream_type) {
  auto exoplayer = ExoPlayer::GetExoPlayerForSbPlayer(player);
  if (exoplayer) {
    exoplayer->WriteEndOfStream(stream_type);
  }
}

void SbPlayerSetBounds(SbPlayer player,
                       int z_index,
                       int x,
                       int y,
                       int width,
                       int height) {
  auto exoplayer = ExoPlayer::GetExoPlayerForSbPlayer(player);
  SB_DCHECK(exoplayer);
  // exoplayer->SetBounds(z_index, x, y, width, height);
  starboard::android::shared::JniEnvExt::Get()->CallStarboardVoidMethodOrAbort(
      "setVideoSurfaceBounds", "(IIII)V", x, y, width, height);
}

bool SbPlayerSetPlaybackRate(SbPlayer player, double playback_rate) {
  auto exoplayer = ExoPlayer::GetExoPlayerForSbPlayer(player);
  SB_DCHECK(exoplayer);
  return exoplayer->SetPlaybackRate(playback_rate);
}

void SbPlayerSetVolume(SbPlayer player, double volume) {
  auto exoplayer = ExoPlayer::GetExoPlayerForSbPlayer(player);
  SB_DCHECK(exoplayer);
  exoplayer->SetVolume(volume);
}

void SbPlayerGetInfo(SbPlayer player, SbPlayerInfo* out_player_info) {
  auto exoplayer = ExoPlayer::GetExoPlayerForSbPlayer(player);
  SB_DCHECK(exoplayer);
  exoplayer->GetInfo(out_player_info);
}

SbDecodeTarget SbPlayerGetCurrentFrame(SbPlayer player) {
  return kSbDecodeTargetInvalid;
}

bool SbPlayerGetAudioConfiguration(
    SbPlayer player,
    int index,
    SbMediaAudioConfiguration* out_audio_configuration) {
  return false;
}
