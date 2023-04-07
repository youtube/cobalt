// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"
#include "starboard/media.h"
#include "starboard/shared/uwp/wasapi_audio.h"

using ::starboard::shared::starboard::media::MimeType;
using ::starboard::shared::uwp::WASAPIAudioDevice;

bool SbMediaIsAudioSupported(SbMediaAudioCodec audio_codec,
                             const MimeType* mime_type,
                             int64_t bitrate) {
  if (audio_codec != kSbMediaAudioCodecAac &&
      audio_codec != kSbMediaAudioCodecOpus &&
      audio_codec != kSbMediaAudioCodecAc3 &&
      audio_codec != kSbMediaAudioCodecEac3) {
    return false;
  }

  if (audio_codec == kSbMediaAudioCodecAc3 ||
      audio_codec == kSbMediaAudioCodecEac3) {
    if (!(WASAPIAudioDevice::GetPassthroughSupportOfDefaultAudioRenderer(
            audio_codec))) {
      return false;
    }
  }

  int64_t local_bitrate =
      WASAPIAudioDevice::GetCachedBitrateOfDefaultAudioRenderer();

  if (local_bitrate <= 0) {
    local_bitrate = kSbMediaMaxAudioBitrateInBitsPerSecond;
  }

  // Here we say : "The app cannot play any encoded audio whose encoded bitrate
  // exceeds the output bitrate"
  return bitrate <= local_bitrate;
}
