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

#include <optional>
#include <string>

#include "starboard/common/log.h"
#include "starboard/common/media.h"
#include "starboard/common/size.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/media/mime_type.h"

namespace starboard {

class ParsedMimeInfo {
 public:
  struct AudioCodecInfo {
    SbMediaAudioCodec codec = kSbMediaAudioCodecNone;
    int channels;
    int bitrate;
  };

  struct VideoCodecInfo {
    // Information parsed from the codec string.
    ParsedVideoCodec codec_info;
    Size frame_size;
    int fps = 0;
    int bitrate = 0;
    bool decode_to_texture_required = false;
  };

  static std::optional<ParsedMimeInfo> Create(const std::string& mime_string);

  const MimeType& mime_type() const { return mime_type_; }

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
    return video_info_.codec_info.codec != kSbMediaVideoCodecNone;
  }
  // Extra information for video codec. Note that video_info() can only be
  // used when has_video_info() returns true.
  const VideoCodecInfo& video_info() const {
    SB_DCHECK(has_video_info());
    return video_info_;
  }

  // Returns a new ParsedMimeInfo with the bitrate updated.
  ParsedMimeInfo WithBitrate(int bitrate) const;

 private:
  ParsedMimeInfo(MimeType mime_type,
                 AudioCodecInfo audio_info,
                 VideoCodecInfo video_info);

  const MimeType mime_type_;
  const AudioCodecInfo audio_info_;
  const VideoCodecInfo video_info_;
};

std::ostream& operator<<(std::ostream& os, const ParsedMimeInfo& mime_info);

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_PARSED_MIME_INFO_H_
