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

#include "starboard/media.h"

#include "starboard/character.h"
#include "starboard/log.h"
#include "starboard/shared/starboard/media/mime_type.h"
#include "starboard/string.h"

using starboard::shared::starboard::media::MimeType;

namespace {
bool HasMultiChannelOutput() {
  int count = SbMediaGetAudioOutputCount();
  for (int output_index = 0; output_index < count; ++output_index) {
    SbMediaAudioConfiguration configuration;
    if (!SbMediaGetAudioConfiguration(output_index, &configuration)) {
      continue;
    }

    if (configuration.number_of_channels >= 6) {
      return true;
    }
  }

  return false;
}
}  // namespace

SbMediaSupportType SbMediaCanPlayMimeAndKeySystem(const char* mime,
                                                  const char* key_system) {
  if (mime == NULL) {
    SB_DLOG(WARNING) << "mime cannot be NULL";
    return kSbMediaSupportTypeNotSupported;
  }
  if (key_system == NULL) {
    SB_DLOG(WARNING) << "key_system cannot be NULL";
    return kSbMediaSupportTypeNotSupported;
  }
  if (SbStringGetLength(key_system) != 0) {
    if (!SbMediaIsSupported(kSbMediaVideoCodecNone, kSbMediaAudioCodecNone,
                            key_system)) {
      return kSbMediaSupportTypeNotSupported;
    }
  }
  MimeType mime_type(mime);
  if (!mime_type.is_valid()) {
    SB_DLOG(WARNING) << mime << " is not a valid mime type";
    return kSbMediaSupportTypeNotSupported;
  }

  if (mime_type.type() == "audio" && mime_type.subtype() == "mp4") {
    // TODO: Base this on the "channels" param, not the codecs param.
    if (mime_type.GetParamStringValue("codecs", "") == "aac51") {
      if (!HasMultiChannelOutput()) {
        return kSbMediaSupportTypeNotSupported;
      }
    }
    return kSbMediaSupportTypeProbably;
  }
  if (mime_type.type() == "video" && mime_type.subtype() == "mp4") {
    return kSbMediaSupportTypeProbably;
  }
#if SB_HAS(MEDIA_WEBM_VP9_SUPPORT)
  if (mime_type.type() == "video" && mime_type.subtype() == "webm") {
    if (mime_type.GetCodecs().empty()) {
      return kSbMediaSupportTypeMaybe;
    }
    if (mime_type.GetParamStringValue(0) == "vp9") {
      return kSbMediaSupportTypeProbably;
    }
    return kSbMediaSupportTypeNotSupported;
  }
#endif  // SB_HAS(MEDIA_WEBM_VP9_SUPPORT)
  return kSbMediaSupportTypeNotSupported;
}
