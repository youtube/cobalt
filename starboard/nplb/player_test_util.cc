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

#include "starboard/nplb/player_test_util.h"

#include "starboard/nplb/player_creation_param_helpers.h"

namespace starboard {
namespace nplb {

SbPlayer CallSbPlayerCreate(
    SbWindow window,
    SbMediaVideoCodec video_codec,
    SbMediaAudioCodec audio_codec,
    SbDrmSystem drm_system,
    const SbMediaAudioSampleInfo* audio_sample_info,
    const char* max_video_capabilities,
    SbPlayerDeallocateSampleFunc sample_deallocate_func,
    SbPlayerDecoderStatusFunc decoder_status_func,
    SbPlayerStatusFunc player_status_func,
    SbPlayerErrorFunc player_error_func,
    void* context,
    SbPlayerOutputMode output_mode,
    SbDecodeTargetGraphicsContextProvider* context_provider) {
#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

  if (audio_sample_info) {
    SB_CHECK(audio_sample_info->codec == audio_codec);
  } else {
    SB_CHECK(audio_codec == kSbMediaAudioCodecNone);
  }

  SbPlayerCreationParam creation_param =
      nplb::CreatePlayerCreationParam(audio_codec, video_codec);
  if (audio_sample_info) {
    creation_param.audio_sample_info = *audio_sample_info;
  }
  creation_param.drm_system = drm_system;
  creation_param.output_mode = output_mode;
  creation_param.video_sample_info.max_video_capabilities =
      max_video_capabilities;

  return SbPlayerCreate(window, &creation_param, sample_deallocate_func,
                        decoder_status_func, player_status_func,
                        player_error_func, context, context_provider);

#else  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

  return SbPlayerCreate(
      window, video_codec, audio_codec, kSbDrmSystemInvalid, audio_sample_info,
#if SB_API_VERSION >= 11
      max_video_capabilities,
#endif  // SB_API_VERSION >= 11
      sample_deallocate_func, decoder_status_func, player_status_func,
      player_error_func, context, output_mode, context_provider);

#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
}

bool IsOutputModeSupported(SbPlayerOutputMode output_mode,
                           SbMediaVideoCodec codec) {
#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
  SbPlayerCreationParam creation_param =
      CreatePlayerCreationParam(kSbMediaAudioCodecNone, codec);
  creation_param.output_mode = output_mode;
  return SbPlayerGetPreferredOutputMode(&creation_param) == output_mode;
#else   // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
  return SbPlayerOutputModeSupported(output_mode, codec, kSbDrmSystemInvalid);
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
}

}  // namespace nplb
}  // namespace starboard
