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

#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_CODEC_UTIL_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_CODEC_UTIL_H_

#include <vector>

#include "starboard/common/optional.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/media/avc_util.h"
#include "starboard/shared/starboard/media/media_util.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

// This class captures necessary information to describe a video config.  It can
// be used to detect config change of video stream during the playback.
// The class only supports h264 and vp9 for now, which is checked in its ctor.
class VideoConfig {
 public:
  // |data| must point to the encoded data of a key frame.
  VideoConfig(SbMediaVideoCodec video_codec,
              int width,
              int height,
              const uint8_t* data,
              size_t size);

  VideoConfig(const VideoStreamInfo& video_stream_info,
              const uint8_t* data,
              size_t size);

  bool is_valid() const { return video_codec_ != kSbMediaVideoCodecNone; }

  const AvcParameterSets& avc_parameter_sets() const {
    SB_DCHECK(is_valid());
    SB_DCHECK(video_codec_ == kSbMediaVideoCodecH264);
    SB_DCHECK(avc_parameter_sets_);
    return avc_parameter_sets_.value();
  }

  bool operator==(const VideoConfig& that) const;
  bool operator!=(const VideoConfig& that) const;

 private:
  SbMediaVideoCodec video_codec_ = kSbMediaVideoCodecNone;
  int width_ = -1;
  int height_ = -1;
  // Only valid when |video_codec_| is |kSbMediaVideoCodecH264|.
  optional<AvcParameterSets> avc_parameter_sets_;
};

// Attempts to determine an SbMediaAudioCodec from |codec|, returning
// kSbMediaAudioCodecNone if no match is found. |subtype| may be checked in
// cases of ambiguous codec strings.
SbMediaAudioCodec GetAudioCodecFromString(const char* codec,
                                          const char* subtype);

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_CODEC_UTIL_H_
