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

#include "starboard/common/log.h"
#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/media/media_support_internal.h"

#if ENABLE_IAMF_DECODE
#include "starboard/shared/starboard/media/iamf_util.h"
#endif  // ENABLE_IAMF_DECODE

namespace starboard::shared::starboard::media {

bool HasSupportedIamfProfile(const IamfMimeUtil* mime_util) {
  return mime_util->primary_profile() == kIamfProfileSimple ||
         mime_util->primary_profile() == kIamfProfileBase ||
         mime_util->additional_profile() == kIamfProfileSimple ||
         mime_util->additional_profile() == kIamfProfileBase;
}

bool MediaIsAudioSupported(SbMediaAudioCodec audio_codec,
                           const MimeType* mime_type,
                           int64_t bitrate) {
  if (audio_codec == kSbMediaAudioCodecAac) {
    return bitrate <= kSbMediaMaxAudioBitrateInBitsPerSecond;
  }

  if (audio_codec == kSbMediaAudioCodecOpus) {
    return bitrate <= kSbMediaMaxAudioBitrateInBitsPerSecond;
  }

#if ENABLE_IAMF_DECODE
  if (audio_codec == kSbMediaAudioCodecIamf) {
    if (!mime_type || !mime_type->is_valid()) {
      return false;
    }
    const std::vector<std::string>& codecs = mime_type->GetCodecs();
    for (auto& codec : codecs) {
      IamfMimeUtil mime_util(codec);
      // We support only IAMF Base or Simple profile streams with an Opus
      // substream.
      if (mime_util.is_valid() &&
          mime_util.substream_codec() == kIamfSubstreamCodecOpus &&
          HasSupportedIamfProfile(&mime_util)) {
        return bitrate <= kSbMediaMaxAudioBitrateInBitsPerSecond;
      }
    }
    return false;
  }
#endif  // ENABLE_IAMF_DECODE

  if (audio_codec == kSbMediaAudioCodecAc3 ||
      audio_codec == kSbMediaAudioCodecEac3) {
    return bitrate <= kSbMediaMaxAudioBitrateInBitsPerSecond;
  }

  if (audio_codec == kSbMediaAudioCodecVorbis) {
    return bitrate <= kSbMediaMaxAudioBitrateInBitsPerSecond;
  }
  if (audio_codec == kSbMediaAudioCodecMp3) {
    return bitrate <= kSbMediaMaxAudioBitrateInBitsPerSecond;
  }
  if (audio_codec == kSbMediaAudioCodecPcm) {
    return bitrate <= kSbMediaMaxAudioBitrateInBitsPerSecond;
  }
  if (audio_codec == kSbMediaAudioCodecFlac) {
    return bitrate <= kSbMediaMaxAudioBitrateInBitsPerSecond;
  }

  return false;
}

}  // namespace starboard::shared::starboard::media
