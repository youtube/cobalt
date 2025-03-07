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

#include "starboard/android/shared/exoplayer/exoplayer.h"
#include "starboard/player.h"

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
  return ExoPlayer::CreateInstance();
}

SbPlayerOutputMode SbPlayerGetPreferredOutputMode(
    const SbPlayerCreationParam* creation_param) {
  return kSbPlayerOutputModePunchOut;
}

void SbPlayerDestroy(SbPlayer player) {
  delete ExoPlayer::GetExoPlayerForSbPlayer(player);
}

void SbPlayerSeek(SbPlayer player, int64_t seek_to_timestamp, int ticket) {
  return;
}

void SbPlayerWriteSamples(SbPlayer player,
                          SbMediaType sample_type,
                          const SbPlayerSampleInfo* sample_infos,
                          int number_of_sample_infos) {
  return;
}

int SbPlayerGetMaximumNumberOfSamplesPerWrite(SbPlayer player,
                                              SbMediaType sample_type) {
  return 1;
}

void SbPlayerWriteEndOfStream(SbPlayer player, SbMediaType stream_type) {}

void SbPlayerSetBounds(SbPlayer player,
                       int z_index,
                       int x,
                       int y,
                       int width,
                       int height) {}

bool SbPlayerSetPlaybackRate(SbPlayer player, double playback_rate) {}

void SbPlayerSetVolume(SbPlayer player, double volume) {}

void SbPlayerGetInfo(SbPlayer player, SbPlayerInfo* out_player_info) {}

SbDecodeTarget SbPlayerGetCurrentFrame(SbPlayer player) {
  return kSbDecodeTargetInvalid;
}

bool SbPlayerGetAudioConfiguration(
    SbPlayer player,
    int index,
    SbMediaAudioConfiguration* out_audio_configuration) {
  return false;
}
