// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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
#include "starboard/shared/starboard/media/parsed_mime_info.h"

bool SbMediaCanChangeType(const char* current_mime, const char* new_mime) {
  if (!current_mime || !new_mime || current_mime[0] == '\0' ||
      new_mime[0] == '\0') {
    return false;
  }

  auto current_info = starboard::ParsedMimeInfo::Create(current_mime);
  auto new_info = starboard::ParsedMimeInfo::Create(new_mime);

  if (!current_info || !new_info) {
    return false;
  }

  // Reject cross-family video codec switches (e.g. VP9 -> AV1).
  if (current_info->has_video_info() && new_info->has_video_info()) {
    if (current_info->video_info().codec != new_info->video_info().codec) {
      return false;
    }
  }

  // Reject cross-family audio codec switches (e.g. AAC -> Opus).
  if (current_info->has_audio_info() && new_info->has_audio_info()) {
    if (current_info->audio_info().codec != new_info->audio_info().codec) {
      return false;
    }
  }

  return true;
}
