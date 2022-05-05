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

#include "starboard/shared/starboard/media/media_support_internal.h"

#include "starboard/common/log.h"
#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"
#include "starboard/media.h"

using ::starboard::shared::starboard::media::MimeType;

bool SbMediaIsAudioSupported(SbMediaAudioCodec audio_codec,
                             const MimeType* mime_type,
                             int64_t bitrate) {
  if (audio_codec == kSbMediaAudioCodecAac) {
    return bitrate <= kSbMediaMaxAudioBitrateInBitsPerSecond;
  }

  if (audio_codec == kSbMediaAudioCodecOpus) {
    return bitrate <= kSbMediaMaxAudioBitrateInBitsPerSecond;
  }

  if (kSbHasAc3Audio) {
    if (audio_codec == kSbMediaAudioCodecAc3 ||
        audio_codec == kSbMediaAudioCodecEac3) {
      return bitrate <= kSbMediaMaxAudioBitrateInBitsPerSecond;
    }
  }

  if (audio_codec == kSbMediaAudioCodecVorbis) {
    return bitrate <= kSbMediaMaxAudioBitrateInBitsPerSecond;
  }
#if SB_API_VERSION >= 14
  if (audio_codec == kSbMediaAudioCodecMp3) {
    return bitrate <= kSbMediaMaxAudioBitrateInBitsPerSecond;
  }
#endif  // SB_API_VERSION >= 14

  return false;
}
