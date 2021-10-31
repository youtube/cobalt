// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/player.h"

#include <algorithm>

#include "starboard/configuration.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"

#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

SbPlayerOutputMode SbPlayerGetPreferredOutputMode(
    const SbPlayerCreationParam* creation_param) {
  using starboard::shared::starboard::player::filter::VideoDecoder;

  if (!creation_param) {
    SB_LOG(ERROR) << "creation_param cannot be NULL";
    return kSbPlayerOutputModeInvalid;
  }

  if (creation_param->audio_sample_info.codec != kSbMediaAudioCodecNone &&
      !creation_param->audio_sample_info.mime) {
    SB_LOG(ERROR) << "creation_param->audio_sample_info.mime cannot be NULL";
    return kSbPlayerOutputModeInvalid;
  }

  if (creation_param->video_sample_info.codec != kSbMediaVideoCodecNone &&
      !creation_param->video_sample_info.mime) {
    SB_LOG(ERROR) << "creation_param->video_sample_info.mime cannot be NULL";
    return kSbPlayerOutputModeInvalid;
  }

  if (creation_param->video_sample_info.codec != kSbMediaVideoCodecNone &&
      !creation_param->video_sample_info.max_video_capabilities) {
    SB_LOG(ERROR) << "creation_param->video_sample_info.max_video_capabilities"
                  << " cannot be NULL";
    return kSbPlayerOutputModeInvalid;
  }

  auto codec = creation_param->video_sample_info.codec;
  auto drm_system = creation_param->drm_system;

  SbPlayerOutputMode output_modes_to_check[] = {
      kSbPlayerOutputModePunchOut, kSbPlayerOutputModeDecodeToTexture,
  };

  // Check |kSbPlayerOutputModeDecodeToTexture| first if the caller prefers it.
  if (creation_param->output_mode == kSbPlayerOutputModeDecodeToTexture) {
    std::swap(output_modes_to_check[0], output_modes_to_check[1]);
  }

  if (VideoDecoder::OutputModeSupported(output_modes_to_check[0], codec,
                                        drm_system)) {
    return output_modes_to_check[0];
  }

  if (VideoDecoder::OutputModeSupported(output_modes_to_check[1], codec,
                                        drm_system)) {
    return output_modes_to_check[1];
  }

  SB_LOG(WARNING) << "creation_param->video_sample_info.codec ("
                  << creation_param->video_sample_info.codec
                  << ") is not supported";
  return kSbPlayerOutputModeInvalid;
}

#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
