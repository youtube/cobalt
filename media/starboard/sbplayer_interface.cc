// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "media/starboard/sbplayer_interface.h"

#include <string>

namespace media {

SbPlayer DefaultSbPlayerInterface::Create(
    SbWindow window,
    const SbPlayerCreationParam* creation_param,
    SbPlayerDeallocateSampleFunc sample_deallocate_func,
    SbPlayerDecoderStatusFunc decoder_status_func,
    SbPlayerStatusFunc player_status_func,
    SbPlayerErrorFunc player_error_func,
    void* context,
    SbDecodeTargetGraphicsContextProvider* context_provider) {
  return SbPlayerCreate(window, creation_param, sample_deallocate_func,
                        decoder_status_func, player_status_func,
                        player_error_func, context, context_provider);
}

SbPlayerOutputMode DefaultSbPlayerInterface::GetPreferredOutputMode(
    const SbPlayerCreationParam* creation_param) {
  return SbPlayerGetPreferredOutputMode(creation_param);
}

void DefaultSbPlayerInterface::Destroy(SbPlayer player) {
  SbPlayerDestroy(player);
}

void DefaultSbPlayerInterface::Seek(SbPlayer player,
                                    base::TimeDelta seek_to_timestamp,
                                    int ticket) {
  SbPlayerSeek(player, seek_to_timestamp.InMicroseconds(), ticket);
}

void DefaultSbPlayerInterface::WriteSamples(
    SbPlayer player,
    SbMediaType sample_type,
    const SbPlayerSampleInfo* sample_infos,
    int number_of_sample_infos) {
  SbPlayerWriteSamples(player, sample_type, sample_infos,
                       number_of_sample_infos);
}

int DefaultSbPlayerInterface::GetMaximumNumberOfSamplesPerWrite(
    SbPlayer player,
    SbMediaType sample_type) {
  return SbPlayerGetMaximumNumberOfSamplesPerWrite(player, sample_type);
}

void DefaultSbPlayerInterface::WriteEndOfStream(SbPlayer player,
                                                SbMediaType stream_type) {
  SbPlayerWriteEndOfStream(player, stream_type);
}

void DefaultSbPlayerInterface::SetBounds(SbPlayer player,
                                         int z_index,
                                         int x,
                                         int y,
                                         int width,
                                         int height) {
  SbPlayerSetBounds(player, z_index, x, y, width, height);
}

bool DefaultSbPlayerInterface::SetPlaybackRate(SbPlayer player,
                                               double playback_rate) {
  return SbPlayerSetPlaybackRate(player, playback_rate);
}

void DefaultSbPlayerInterface::SetVolume(SbPlayer player, double volume) {
  SbPlayerSetVolume(player, volume);
}

void DefaultSbPlayerInterface::GetInfo(SbPlayer player,
                                       SbPlayerInfo* out_player_info) {
  SbPlayerGetInfo(player, out_player_info);
}

SbDecodeTarget DefaultSbPlayerInterface::GetCurrentFrame(SbPlayer player) {
  return SbPlayerGetCurrentFrame(player);
}

bool DefaultSbPlayerInterface::GetAudioConfiguration(
    SbPlayer player,
    int index,
    SbMediaAudioConfiguration* out_audio_configuration) {
  return SbPlayerGetAudioConfiguration(player, index, out_audio_configuration);
}

}  // namespace media
