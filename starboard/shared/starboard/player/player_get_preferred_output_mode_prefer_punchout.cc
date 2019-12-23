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

#include "starboard/configuration.h"

#if SB_HAS(PLAYER_GET_PREFERRED_OUTPUT_MODE)

SbPlayerOutputMode SbPlayerGetPreferredOutputMode(
    const SbMediaAudioSampleInfo* audio_sample_info,
    const SbMediaVideoSampleInfo* video_sample_info,
    SbDrmSystem drm_system,
    const char* max_video_capabilities) {
  SB_UNREFERENCED_PARAMETER(max_video_capabilities);

  if (!audio_sample_info) {
    SB_LOG(ERROR) << "audio_sample_info cannot be NULL";
    return kSbPlayerOutputModeInvalid;
  }

  if (!video_sample_info) {
    SB_LOG(ERROR) << "video_sample_info cannot be NULL";
    return kSbPlayerOutputModeInvalid;
  }

  if (!max_video_capabilities) {
    SB_LOG(ERROR) << "max_video_capabilities cannot be NULL";
    return kSbPlayerOutputModeInvalid;
  }

  if (SbPlayerOutputModeSupported(kSbPlayerOutputModePunchOut,
                                  video_sample_info->codec, drm_system)) {
    return kSbPlayerOutputModePunchOut;
  }
  if (SbPlayerOutputModeSupported(kSbPlayerOutputModeDecodeToTexture,
                                  video_sample_info->codec, drm_system)) {
    return kSbPlayerOutputModeDecodeToTexture;
  }

  SB_NOTREACHED();
  return kSbPlayerOutputModeInvalid;
}

#endif  // SB_HAS(PLAYER_GET_PREFERRED_OUTPUT_MODE)
