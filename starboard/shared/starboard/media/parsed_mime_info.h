// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_PARSED_MIME_INFO_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_PARSED_MIME_INFO_H_

#include <string>

#include "starboard/common/log.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/media/mime_type.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

// TODO: add unit tests for ParsedMimeInfo
class ParsedMimeInfo {
 public:
  struct AudioCodecInfo {
    SbMediaAudioCodec codec = kSbMediaAudioCodecNone;
    int channels;
    int bitrate;
  };

  struct VideoCodecInfo {
    SbMediaVideoCodec codec = kSbMediaVideoCodecNone;
    int profile;
    int level;
    int bit_depth;
    SbMediaPrimaryId primary_id;
    SbMediaTransferId transfer_id;
    SbMediaMatrixId matrix_id;
    int frame_width;
    int frame_height;
    int fps;
    int bitrate;
    bool decode_to_texture_required;
  };

  ParsedMimeInfo() : mime_type_("") {}
  explicit ParsedMimeInfo(const std::string& mime_string);

  const MimeType& mime_type() const { return mime_type_; }

  bool is_valid() const { return is_valid_; }

  // A switch in the mime string to disable caches.
  bool disable_cache() const { return disable_cache_; }

  bool has_audio_info() const {
    return audio_info_.codec != kSbMediaAudioCodecNone;
  }
  // Extra information for audio codec. Note that audio_info() can only be
  // used when has_audio_info() returns true.
  const AudioCodecInfo& audio_info() const {
    SB_DCHECK(has_audio_info());
    return audio_info_;
  }

  bool has_video_info() const {
    return video_info_.codec != kSbMediaVideoCodecNone;
  }
  // Extra information for video codec. Note that video_info() can only be
  // used when has_video_info() returns true.
  const VideoCodecInfo& video_info() const {
    SB_DCHECK(has_video_info());
    return video_info_;
  }

  // Allow to overwrite the bitrate.
  void SetBitrate(int bitrate);

 private:
  void ParseMimeInfo();
  bool ParseAudioInfo(const std::string& codec);
  bool ParseVideoInfo(const std::string& codec);

  void ResetCodecInfos();

  MimeType mime_type_;
  bool is_valid_ = true;
  bool disable_cache_ = false;
  AudioCodecInfo audio_info_;
  VideoCodecInfo video_info_;
};

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_PARSED_MIME_INFO_H_
