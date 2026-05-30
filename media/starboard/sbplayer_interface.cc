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

#include "base/logging.h"
#include "starboard/extension/player_configuration.h"
#include "starboard/system.h"

namespace media {

bool SbPlayerInterface::SetDecodeToTexturePreferred(bool preferred) {
  const StarboardExtensionPlayerConfigurationApi* extension_api =
      static_cast<const StarboardExtensionPlayerConfigurationApi*>(
          SbSystemGetExtension(kStarboardExtensionPlayerConfigurationName));
  if (!extension_api) {
    return false;
  }

  DCHECK_EQ(extension_api->name,
            // Avoid comparing raw string pointers for equal.
            std::string(kStarboardExtensionPlayerConfigurationName));
  DCHECK_EQ(extension_api->version, 1u);

  // SetDecodeToTexturePreferred api could be NULL.
  if (extension_api->SetDecodeToTexturePreferred) {
    extension_api->SetDecodeToTexturePreferred(preferred);
    return true;
  } else {
    LOG(INFO) << "DecodeToTextureModePreferred is not supported.";
    return false;
  }
}

SbPlayer DefaultSbPlayerInterface::Create(
    SbWindow window,
    const SbPlayerCreationParam* creation_param,
    SbPlayerDeallocateSampleFunc sample_deallocate_func,
    SbPlayerDecoderStatusFunc decoder_status_func,
    SbPlayerStatusFunc player_status_func,
    SbPlayerErrorFunc player_error_func,
    void* context,
    SbDecodeTargetGraphicsContextProvider* context_provider) {
  media_metrics_provider_.StartTrackingAction(MediaAction::SBPLAYER_CREATE);
  auto player = SbPlayerCreate(window, creation_param, sample_deallocate_func,
                               decoder_status_func, player_status_func,
                               player_error_func, context, context_provider);
  media_metrics_provider_.EndTrackingAction(MediaAction::SBPLAYER_CREATE);
  return player;
}

SbPlayerOutputMode DefaultSbPlayerInterface::GetPreferredOutputMode(
    const SbPlayerCreationParam* creation_param) {
  media_metrics_provider_.StartTrackingAction(
      MediaAction::SBPLAYER_GET_PREFERRED_OUTPUT_MODE);
  auto output_mode = SbPlayerGetPreferredOutputMode(creation_param);
  media_metrics_provider_.EndTrackingAction(
      MediaAction::SBPLAYER_GET_PREFERRED_OUTPUT_MODE);

  switch (output_mode) {
    case kSbPlayerOutputModeDecodeToTexture:
      LOG(ERROR) << "Miguelaaaoooooo " << "decode-to-texture";
      break;
    case kSbPlayerOutputModePunchOut:
      LOG(ERROR) << "Miguelaaaoooooo " << "punch-out";
      break;
    case kSbPlayerOutputModeInvalid:
      LOG(ERROR) << "Miguelaaaoooooo " << "invalid";
      break;
  }
  /////////////////////////////////////////////////  Miguelao
  output_mode = kSbPlayerOutputModeDecodeToTexture;


  return output_mode;
}

void DefaultSbPlayerInterface::Destroy(SbPlayer player) {
  media_metrics_provider_.StartTrackingAction(MediaAction::SBPLAYER_DESTROY);
  SbPlayerDestroy(player);
  media_metrics_provider_.EndTrackingAction(MediaAction::SBPLAYER_DESTROY);
}

void DefaultSbPlayerInterface::Seek(SbPlayer player,
                                    base::TimeDelta seek_to_timestamp,
                                    int ticket) {
  media_metrics_provider_.StartTrackingAction(MediaAction::SBPLAYER_SEEK);
  SbPlayerSeek(player, seek_to_timestamp.InMicroseconds(), ticket);
  media_metrics_provider_.EndTrackingAction(MediaAction::SBPLAYER_SEEK);
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
  auto media_action = (stream_type == kSbMediaTypeAudio)
                          ? MediaAction::SBPLAYER_WRITE_END_OF_STREAM_AUDIO
                          : MediaAction::SBPLAYER_WRITE_END_OF_STREAM_VIDEO;
  media_metrics_provider_.StartTrackingAction(media_action);
  SbPlayerWriteEndOfStream(player, stream_type);
  media_metrics_provider_.EndTrackingAction(media_action);
}

void DefaultSbPlayerInterface::SetBounds(SbPlayer player,
                                         int z_index,
                                         int x,
                                         int y,
                                         int width,
                                         int height) {
  media_metrics_provider_.StartTrackingAction(MediaAction::SBPLAYER_SET_BOUNDS);
  SbPlayerSetBounds(player, z_index, x, y, width, height);
  media_metrics_provider_.EndTrackingAction(MediaAction::SBPLAYER_SET_BOUNDS);
}

bool DefaultSbPlayerInterface::SetPlaybackRate(SbPlayer player,
                                               double playback_rate) {
  media_metrics_provider_.StartTrackingAction(
      MediaAction::SBPLAYER_SET_PLAYBACK_RATE);
  auto set_playback_rate = SbPlayerSetPlaybackRate(player, playback_rate);
  media_metrics_provider_.EndTrackingAction(
      MediaAction::SBPLAYER_SET_PLAYBACK_RATE);
  return set_playback_rate;
}

void DefaultSbPlayerInterface::SetVolume(SbPlayer player, double volume) {
  media_metrics_provider_.StartTrackingAction(MediaAction::SBPLAYER_SET_VOLUME);
  SbPlayerSetVolume(player, volume);
  media_metrics_provider_.EndTrackingAction(MediaAction::SBPLAYER_SET_VOLUME);
}

void DefaultSbPlayerInterface::GetInfo(SbPlayer player,
                                       SbPlayerInfo* out_player_info) {
  media_metrics_provider_.StartTrackingAction(MediaAction::SBPLAYER_GET_INFO);
  SbPlayerGetInfo(player, out_player_info);
  media_metrics_provider_.EndTrackingAction(MediaAction::SBPLAYER_GET_INFO);
}

SbDecodeTarget DefaultSbPlayerInterface::GetCurrentFrame(SbPlayer player) {
  media_metrics_provider_.StartTrackingAction(
      MediaAction::SBPLAYER_GET_CURRENT_FRAME);
  auto current_frame = SbPlayerGetCurrentFrame(player);
  media_metrics_provider_.EndTrackingAction(
      MediaAction::SBPLAYER_GET_CURRENT_FRAME);
  return current_frame;
}

#if SB_HAS(PLAYER_WITH_URL)
SbPlayer DefaultSbPlayerInterface::CreateUrlPlayer(
    const char* url,
    SbWindow window,
    SbPlayerStatusFunc player_status_func,
    SbPlayerEncryptedMediaInitDataEncounteredCB
        encrypted_media_init_data_encountered_cb,
    SbPlayerErrorFunc player_error_func,
    void* context) {
  media_metrics_provider_.StartTrackingAction(
      MediaAction::SBPLAYER_CREATE_URL_PLAYER);
  auto player = SbUrlPlayerCreate(url, window, player_status_func,
                                  encrypted_media_init_data_encountered_cb,
                                  player_error_func, context);
  media_metrics_provider_.EndTrackingAction(
      MediaAction::SBPLAYER_CREATE_URL_PLAYER);
  return player;
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
    SbPlayer player,
    SbUrlPlayerExtraInfo* out_url_player_info) {
  SbUrlPlayerGetExtraInfo(player, out_url_player_info);
}
#endif  // SB_HAS(PLAYER_WITH_URL)

bool DefaultSbPlayerInterface::GetAudioConfiguration(
    SbPlayer player,
    int index,
    SbMediaAudioConfiguration* out_audio_configuration) {
  media_metrics_provider_.StartTrackingAction(
      MediaAction::SBPLAYER_GET_AUDIO_CONFIG);
  auto audio_configuration =
      SbPlayerGetAudioConfiguration(player, index, out_audio_configuration);
  media_metrics_provider_.EndTrackingAction(
      MediaAction::SBPLAYER_GET_AUDIO_CONFIG);
  return audio_configuration;
}

}  // namespace media
