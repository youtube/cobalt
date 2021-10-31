// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/media/mime_type.h"

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

  starboard::shared::starboard::media::MimeType mime_type(mime);
  if (!mime_type.is_valid()) {
    SB_DLOG(WARNING) << mime << " is not a valid mime type";
    return kSbMediaSupportTypeNotSupported;
  }

  return CanPlayMimeAndKeySystem(mime_type, key_system);
}
