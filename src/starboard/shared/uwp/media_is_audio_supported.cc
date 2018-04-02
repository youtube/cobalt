// Copyright 2018 Google Inc. All Rights Reserved.
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
#include "starboard/media.h"
#include "starboard/shared/uwp/wasapi_audio.h"

using starboard::shared::uwp::WASAPIAudioDevice;

SB_EXPORT bool SbMediaIsAudioSupported(SbMediaAudioCodec audio_codec,
                                       int64_t bitrate) {
  // TODO: Add Opus.
  if (audio_codec != kSbMediaAudioCodecAac) {
    return false;
  }

  Microsoft::WRL::ComPtr<WASAPIAudioDevice> audioDevice =
      Microsoft::WRL::Make<WASAPIAudioDevice>();

  int64_t local_bitrate = audioDevice->GetBitrate();

  if (local_bitrate <= 0) {
    local_bitrate = SB_MEDIA_MAX_AUDIO_BITRATE_IN_BITS_PER_SECOND;
  }

  // Here we say : "The app cannot play any encoded audio whose encoded bitrate
  // exceeds the output bitrate"
  return bitrate <= local_bitrate;
}
