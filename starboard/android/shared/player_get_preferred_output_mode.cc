// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

// clang-format off
#include "starboard/player.h"
// clang-format on

#include <algorithm>

#include "starboard/configuration.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/player_components.h"

SbPlayerOutputMode SbPlayerGetPreferredOutputMode(
    const SbPlayerCreationParam* creation_param) {
  using ::starboard::PlayerComponents;

  if (!creation_param) {
    SB_LOG(ERROR) << "creation_param cannot be NULL";
    return kSbPlayerOutputModeInvalid;
  }

  const SbMediaAudioStreamInfo& audio_stream_info =
      creation_param->audio_stream_info;
  const SbMediaVideoStreamInfo& video_stream_info =
      creation_param->video_stream_info;

  if (audio_stream_info.codec != kSbMediaAudioCodecNone &&
      !audio_stream_info.mime) {
    SB_LOG(ERROR) << "creation_param->audio_stream_info.mime cannot be NULL";
    return kSbPlayerOutputModeInvalid;
  }

  if (video_stream_info.codec != kSbMediaVideoCodecNone &&
      !video_stream_info.mime) {
    SB_LOG(ERROR) << "creation_param->video_stream_info.mime cannot be NULL";
    return kSbPlayerOutputModeInvalid;
  }

  if (video_stream_info.codec != kSbMediaVideoCodecNone &&
      !video_stream_info.max_video_capabilities) {
    SB_LOG(ERROR) << "creation_param->video_stream_info.max_video_capabilities"
                  << " cannot be NULL";
    return kSbPlayerOutputModeInvalid;
  }

  auto codec = video_stream_info.codec;
  auto drm_system = creation_param->drm_system;
  auto max_video_capabilities = video_stream_info.max_video_capabilities;

  bool is_sdr = true;
  if (codec != kSbMediaVideoCodecNone) {
    const auto& color_metadata = video_stream_info.color_metadata;
    is_sdr = starboard::IsSDRVideo(
        color_metadata.bits_per_channel, color_metadata.primaries,
        color_metadata.transfer, color_metadata.matrix);
  }

  if (max_video_capabilities && strlen(max_video_capabilities) > 0) {
    // Sub players must use "decode-to-texture" on Android.
    // Since hdr videos are not supported under "decode-to-texture" mode, reject
    // it for sub players.
    if (PlayerComponents::Factory::OutputModeSupported(
            kSbPlayerOutputModeDecodeToTexture, codec, drm_system) &&
        is_sdr) {
      return kSbPlayerOutputModeDecodeToTexture;
    }
    SB_LOG_IF(INFO, !is_sdr)
        << "Returning kSbPlayerOutputModeInvalid as HDR videos are not "
           "supported under decode-to-texture.";
    return kSbPlayerOutputModeInvalid;
  }

  SbPlayerOutputMode output_modes_to_check[2] = {kSbPlayerOutputModeInvalid,
                                                 kSbPlayerOutputModeInvalid};
  int number_of_output_modes_to_check = 2;

  if (is_sdr) {
    if (creation_param->output_mode == kSbPlayerOutputModeDecodeToTexture) {
      output_modes_to_check[0] = kSbPlayerOutputModeDecodeToTexture;
      output_modes_to_check[1] = kSbPlayerOutputModePunchOut;
    } else {
      output_modes_to_check[0] = kSbPlayerOutputModePunchOut;
      output_modes_to_check[1] = kSbPlayerOutputModeDecodeToTexture;
    }
  } else {
    // HDR videos require "punch-out".
    output_modes_to_check[0] = kSbPlayerOutputModePunchOut;
    number_of_output_modes_to_check = 1;
  }

  for (int i = 0; i < number_of_output_modes_to_check; ++i) {
    if (PlayerComponents::Factory::OutputModeSupported(output_modes_to_check[i],
                                                       codec, drm_system)) {
      return output_modes_to_check[i];
    }
  }

  SB_LOG(WARNING) << "creation_param->video_stream_info.codec ("
                  << video_stream_info.codec << ") is not supported";
  return kSbPlayerOutputModeInvalid;
}
