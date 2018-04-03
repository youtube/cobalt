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

SbPlayer SbPlayerCreate(SbWindow /*window*/,
                        SbMediaVideoCodec /*video_codec*/,
                        SbMediaAudioCodec /*audio_codec*/,
#if SB_API_VERSION < SB_DEPRECATE_SB_MEDIA_TIME_API_VERSION
                        SbMediaTime /*duration_pts*/,
#endif  // SB_API_VERSION < SB_DEPRECATE_SB_MEDIA_TIME_API_VERSION
                        SbDrmSystem /*drm_system*/,
                        const SbMediaAudioHeader* /*audio_header*/,
                        SbPlayerDeallocateSampleFunc /*sample_deallocate_func*/,
                        SbPlayerDecoderStatusFunc /*decoder_status_func*/,
                        SbPlayerStatusFunc /*player_status_func*/,
                        SbPlayerErrorFunc /*player_error_func*/,
                        void* /*context*/,
                        SbPlayerOutputMode /*output_mode*/,
                        SbDecodeTargetGraphicsContextProvider* /*provider*/) {
  return kSbPlayerInvalid;
}
