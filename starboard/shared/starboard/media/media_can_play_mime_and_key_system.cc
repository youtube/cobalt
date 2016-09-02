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
#include "starboard/shared/starboard/media/mime_parser.h"
#include "starboard/string.h"

using starboard::shared::starboard::media::MimeParser;

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
  MimeParser parser(mime);
  if (!parser.is_valid()) {
    SB_DLOG(WARNING) << mime << " is not a valid mime type";
    return kSbMediaSupportTypeNotSupported;
  }
  if (parser.mime() == "application/octet-stream") {
    return kSbMediaSupportTypeProbably;
  }
  if (parser.mime() == "audio/mp4") {
    return kSbMediaSupportTypeProbably;
  }
  if (parser.mime() == "video/mp4") {
    return kSbMediaSupportTypeProbably;
  }
#if SB_HAS(MEDIA_WEBM_VP9_SUPPORT)
  if (parser.mime() == "video/webm") {
    if (!parser.HasParam("codecs")) {
      return kSbMediaSupportTypeMaybe;
    }
    std::string codecs = parser.GetParam("codecs");
    if (codecs == "vp9") {
      return kSbMediaSupportTypeProbably;
    }
    return kSbMediaSupportTypeNotSupported;
  }
#endif  // SB_HAS(MEDIA_WEBM_VP9_SUPPORT)
  return kSbMediaSupportTypeNotSupported;
}
