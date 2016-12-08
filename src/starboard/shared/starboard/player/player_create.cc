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

#include "starboard/log.h"
#include "starboard/shared/starboard/player/player_internal.h"

SbPlayer SbPlayerCreate(SbWindow window,
                        SbMediaVideoCodec video_codec,
                        SbMediaAudioCodec audio_codec,
                        SbMediaTime duration_pts,
                        SbDrmSystem drm_system,
                        const SbMediaAudioHeader* audio_header,
                        SbPlayerDeallocateSampleFunc sample_deallocate_func,
                        SbPlayerDecoderStatusFunc decoder_status_func,
                        SbPlayerStatusFunc player_status_func,
                        void* context) {
  if (audio_codec != kSbMediaAudioCodecAac) {
    SB_LOG(ERROR) << "Unsupported audio codec " << audio_codec;
    return kSbPlayerInvalid;
  }

  if (!audio_header) {
    SB_LOG(ERROR) << "AAC requires a non-NULL SbMediaAudioHeader";
    return kSbPlayerInvalid;
  }

  if (video_codec != kSbMediaVideoCodecH264) {
    SB_LOG(ERROR) << "Unsupported video codec " << video_codec;
    return kSbPlayerInvalid;
  }

  return new SbPlayerPrivate(window, video_codec, audio_codec, duration_pts,
                             drm_system, audio_header, sample_deallocate_func,
                             decoder_status_func, player_status_func, context);
}
