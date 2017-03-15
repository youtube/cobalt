// Copyright 2016 Google Inc. All Rights Reserved.
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

#if !SB_HAS(PLAYER)
#error "SbPlayerCreate requires SB_HAS(PLAYER)."
#endif

#if SB_API_VERSION <= 3
SbPlayer SbPlayerCreate(SbWindow /*window*/,
                        SbMediaVideoCodec /*video_codec*/,
                        SbMediaAudioCodec /*audio_codec*/,
                        SbMediaTime /*duration_pts*/,
                        SbDrmSystem /*drm_system*/,
                        const SbMediaAudioHeader* /*audio_header*/,
                        SbPlayerDeallocateSampleFunc /*sample_deallocate_func*/,
                        SbPlayerDecoderStatusFunc /*decoder_status_func*/,
                        SbPlayerStatusFunc /*player_status_func*/,
                        void* /*context*/
#if SB_API_VERSION == 3
                        ,
                        SbDecodeTargetProvider* /*provider*/
#endif  // SB_API_VERSION == 3

#else  // SB_API_VERSION <= 3
SbPlayer SbPlayerCreate(SbWindow /*window*/,
                        SbMediaVideoCodec /*video_codec*/,
                        SbMediaAudioCodec /*audio_codec*/,
                        SbMediaTime /*duration_pts*/,
                        SbDrmSystem /*drm_system*/,
                        const SbMediaAudioHeader* /*audio_header*/,
#if SB_API_VERSION >= SB_PLAYER_CREATE_WITH_VIDEO_HEADER_VERSION
                        const SbMediaVideoHeader* /*video_header*/,
#endif  // SB_API_VERSION >= SB_PLAYER_CREATE_WITH_VIDEO_HEADER_VERSION
                        SbPlayerDeallocateSampleFunc /*sample_deallocate_func*/,
                        SbPlayerDecoderStatusFunc /*decoder_status_func*/,
                        SbPlayerStatusFunc /*player_status_func*/,
#if SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
                        SbPlayerOutputMode /*output_mode*/,
#endif  // SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
                        SbDecodeTargetProvider* /*provider*/,
                        void* /*context*/
#endif  // SB_API_VERSION <= 3
                        ) {
  return kSbPlayerInvalid;
}
