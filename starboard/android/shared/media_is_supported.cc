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

#include <string>

#include "starboard/shared/starboard/media/media_support_internal.h"

#include "starboard/android/shared/media_capabilities_cache.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/media/mime_type.h"
#include "starboard/string.h"

bool SbMediaIsSupported(SbMediaVideoCodec video_codec,
                        SbMediaAudioCodec audio_codec,
                        const char* key_system) {
  using starboard::android::shared::IsWidevineL1;
  using starboard::android::shared::MediaCapabilitiesCache;
  using starboard::shared::starboard::media::MimeType;

  // It is possible that the |key_system| comes with extra attributes, like
  // `com.widevine.alpha; encryptionscheme="cenc"`. We prepend "key_system/"
  // to it, so it can be parsed by MimeType.
  MimeType mime_type(std::string("key_system/") + key_system);
  if (!mime_type.is_valid() || !mime_type.ValidateStringParameter(
                                   "encryptionscheme", "cenc|cbcs|cbcs-1-9")) {
    return false;
  }

  // Use allow list to avoid accidentally introducing the support of a codec
  // brought in the future.
  if (audio_codec != kSbMediaAudioCodecNone &&
      audio_codec != kSbMediaAudioCodecAac &&
      audio_codec != kSbMediaAudioCodecOpus &&
      audio_codec != kSbMediaAudioCodecAc3 &&
      audio_codec != kSbMediaAudioCodecEac3) {
    return false;
  }
  const char* key_system_type = mime_type.subtype().c_str();
  if (!IsWidevineL1(key_system_type)) {
    return false;
  }

  if (!MediaCapabilitiesCache::GetInstance()->IsWidevineSupported()) {
    return false;
  }

  std::string encryption_scheme =
      mime_type.GetParamStringValue("encryptionscheme", "");
  if (encryption_scheme == "cbcs" || encryption_scheme == "cbcs-1-9") {
    return MediaCapabilitiesCache::GetInstance()->IsCbcsSchemeSupported();
  }

  return true;
}
