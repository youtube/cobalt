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

// clang-format off
#include "starboard/media.h"
// clang-format on

#include "starboard/common/log.h"
#include "starboard/common/media.h"
#include "starboard/shared/starboard/media/parsed_mime_info.h"

namespace {

bool IsAacOrOpus(SbMediaAudioCodec codec) {
  return codec == kSbMediaAudioCodecAac || codec == kSbMediaAudioCodecOpus;
}

}  // namespace

bool SbMediaCanChangeType(const char* current_mime, const char* new_mime) {
  if (!current_mime) {
    SB_LOG(ERROR) << "current_mime cannot be NULL.";
    return false;
  }
  if (!new_mime) {
    SB_LOG(ERROR) << "new_mime cannot be NULL.";
    return false;
  }

  auto current_mime_info = starboard::ParsedMimeInfo::Create(current_mime);
  if (!current_mime_info) {
    SB_LOG(ERROR) << "Failed to parse current_mime: " << current_mime;
    return false;
  }

  auto new_mime_info = starboard::ParsedMimeInfo::Create(new_mime);
  if (!new_mime_info) {
    SB_LOG(ERROR) << "Failed to parse new_mime: " << new_mime;
    return false;
  }

  // Reject stream media type mismatches (e.g. Video -> Audio or vice versa).
  if (current_mime_info->has_video_info() != new_mime_info->has_video_info() ||
      current_mime_info->has_audio_info() != new_mime_info->has_audio_info()) {
    SB_DLOG(INFO) << "MIME stream type mismatch between " << current_mime
                  << " and " << new_mime;
    return false;
  }

  // Reject cross-family video codec switches (e.g. VP9 -> AV1).
  if (current_mime_info->has_video_info() &&
      current_mime_info->video_info().codec !=
          new_mime_info->video_info().codec) {
    SB_DLOG(INFO) << "Cannot change video codec family from "
                  << starboard::GetMediaVideoCodecName(
                         current_mime_info->video_info().codec)
                  << " to "
                  << starboard::GetMediaVideoCodecName(
                         new_mime_info->video_info().codec);
    return false;
  }

  // Reject cross-family audio codec switches (e.g. AAC -> AC-3), except
  // between AAC and Opus.
  if (current_mime_info->has_audio_info()) {
    const SbMediaAudioCodec current_codec =
        current_mime_info->audio_info().codec;
    const SbMediaAudioCodec new_codec = new_mime_info->audio_info().codec;
    if (current_codec != new_codec &&
        !(IsAacOrOpus(current_codec) && IsAacOrOpus(new_codec))) {
      SB_DLOG(INFO) << "Cannot change audio codec family from "
                    << starboard::GetMediaAudioCodecName(current_codec)
                    << " to " << starboard::GetMediaAudioCodecName(new_codec);
      return false;
    }
  }

  return true;
}
