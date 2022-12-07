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

#include "cobalt/media/base/sbplayer_interface.h"

namespace cobalt {
namespace media {

SbPlayer DefaultSbPlayerInterface::Create(
    SbWindow window, const SbPlayerCreationParam* creation_param,
    SbPlayerDeallocateSampleFunc sample_deallocate_func,
    SbPlayerDecoderStatusFunc decoder_status_func,
    SbPlayerStatusFunc player_status_func, SbPlayerErrorFunc player_error_func,
    void* context, SbDecodeTargetGraphicsContextProvider* context_provider) {
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

void DefaultSbPlayerInterface::Seek(SbPlayer player, SbTime seek_to_timestamp,
                                    int ticket) {
  SbPlayerSeek2(player, seek_to_timestamp, ticket);
}

void DefaultSbPlayerInterface::WriteSample(
    SbPlayer player, SbMediaType sample_type,
    const SbPlayerSampleInfo* sample_infos, int number_of_sample_infos) {
  SbPlayerWriteSample2(player, sample_type, sample_infos,
                       number_of_sample_infos);
}

int DefaultSbPlayerInterface::GetMaximumNumberOfSamplesPerWrite(
    SbPlayer player, SbMediaType sample_type) {
  return SbPlayerGetMaximumNumberOfSamplesPerWrite(player, sample_type);
}

void DefaultSbPlayerInterface::WriteEndOfStream(SbPlayer player,
                                                SbMediaType stream_type) {
  SbPlayerWriteEndOfStream(player, stream_type);
}

void DefaultSbPlayerInterface::SetBounds(SbPlayer player, int z_index, int x,
                                         int y, int width, int height) {
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
                                       SbPlayerInfo2* out_player_info2) {
  SbPlayerGetInfo2(player, out_player_info2);
}

SbDecodeTarget DefaultSbPlayerInterface::GetCurrentFrame(SbPlayer player) {
  return SbPlayerGetCurrentFrame(player);
}

#if SB_HAS(PLAYER_WITH_URL)
SbPlayer DefaultSbPlayerInterface::CreateUrlPlayer(
    const char* url, SbWindow window, SbPlayerStatusFunc player_status_func,
    SbPlayerEncryptedMediaInitDataEncounteredCB
        encrypted_media_init_data_encountered_cb,
    SbPlayerErrorFunc player_error_func, void* context) {
  return SbUrlPlayerCreate(url, window, player_status_func,
                           encrypted_media_init_data_encountered_cb,
                           player_error_func, context);
}

void DefaultSbPlayerInterface::SetUrlPlayerDrmSystem(SbPlayer player,
                                                     SbDrmSystem drm_system) {
  SbUrlPlayerSetDrmSystem(player, drm_system);
}

bool DefaultSbPlayerInterface::GetUrlPlayerOutputModeSupported(
    SbPlayerOutputMode output_mode) {
  return SbUrlPlayerOutputModeSupported(output_mode);
}

void DefaultSbPlayerInterface::GetUrlPlayerExtraInfo(
    SbPlayer player, SbUrlPlayerExtraInfo* out_url_player_info) {
  SbUrlPlayerGetExtraInfo(player, out_url_player_info);
}
#endif  // SB_HAS(PLAYER_WITH_URL)

}  // namespace media
}  // namespace cobalt
