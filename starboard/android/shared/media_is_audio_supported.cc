// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
#include "starboard/shared/starboard/media/media_support_internal.h"
// clang-format on

#include "starboard/android/shared/media_capabilities_cache.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/audio_sink.h"
#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"
#include "starboard/media.h"

namespace starboard::shared::starboard::media {

bool MediaIsAudioSupported(SbMediaAudioCodec audio_codec,
                           const MimeType* mime_type,
                           int64_t bitrate) {
  if (bitrate >= kSbMediaMaxAudioBitrateInBitsPerSecond) {
    return false;
  }

  bool is_passthrough = false;
  const char* mime =
      SupportedAudioCodecToMimeType(audio_codec, &is_passthrough);
  if (!mime) {
    return false;
  }

  bool enable_audio_passthrough = true;
  if (mime_type) {
    if (!mime_type->is_valid()) {
      return false;
    }

    // Enables audio passthrough if the codec supports it.
    if (!mime_type->ValidateBoolParameter("audiopassthrough")) {
      return false;
    }
    enable_audio_passthrough =
        mime_type->GetParamBoolValue("audiopassthrough", true);
  }

  // Android uses a libopus based opus decoder for clear content, or a platform
  // opus decoder for encrypted content, if available.
  if (audio_codec == kSbMediaAudioCodecOpus) {
    return true;
  }

  bool media_codec_supported =
      MediaCapabilitiesCache::GetInstance()->HasAudioDecoderFor(mime, bitrate);

  if (!media_codec_supported) {
    return false;
  }

  if (!is_passthrough) {
    return true;
  }

  if (!enable_audio_passthrough) {
    SB_LOG(INFO) << "Passthrough codec is rejected because passthrough is "
                    "disabled through mime param.";
    return false;
  }

  return MediaCapabilitiesCache::GetInstance()->IsPassthroughSupported(
      audio_codec);
}

}  // namespace starboard::shared::starboard::media
