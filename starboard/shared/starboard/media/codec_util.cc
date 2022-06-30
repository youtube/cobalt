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

#include "starboard/shared/starboard/media/codec_util.h"

#include <algorithm>
#include <cctype>
#include <string>

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

VideoConfig::VideoConfig(SbMediaVideoCodec video_codec,
                         int width,
                         int height,
                         const uint8_t* data,
                         size_t size)
    : width_(width), height_(height) {
  if (video_codec == kSbMediaVideoCodecVp9) {
    video_codec_ = video_codec;
  } else if (video_codec == kSbMediaVideoCodecAv1) {
    video_codec_ = video_codec;
  } else if (video_codec == kSbMediaVideoCodecH264) {
    avc_parameter_sets_ =
        AvcParameterSets(AvcParameterSets::kAnnexB, data, size);
    if (avc_parameter_sets_->is_valid()) {
      video_codec_ = video_codec;
    }
  } else {
    SB_NOTREACHED();
  }
}

VideoConfig::VideoConfig(const SbMediaVideoSampleInfo& video_sample_info,
                         const uint8_t* data,
                         size_t size)
    : VideoConfig(video_sample_info.codec,
                  video_sample_info.frame_width,
                  video_sample_info.frame_height,
                  data,
                  size) {
  SB_DCHECK(video_sample_info.is_key_frame);
}

bool VideoConfig::operator==(const VideoConfig& that) const {
  if (video_codec_ == kSbMediaVideoCodecNone &&
      that.video_codec_ == kSbMediaVideoCodecNone) {
    return true;
  }
  return video_codec_ == that.video_codec_ &&
         avc_parameter_sets_ == that.avc_parameter_sets_ &&
         width_ == that.width_ && height_ == that.height_;
}

bool VideoConfig::operator!=(const VideoConfig& that) const {
  return !(*this == that);
}

SbMediaAudioCodec GetAudioCodecFromString(const char* codec) {
  if (strncmp(codec, "mp4a.40.", 8) == 0) {
    return kSbMediaAudioCodecAac;
  }
  if (kSbHasAc3Audio) {
    if (strcmp(codec, "ac-3") == 0) {
      return kSbMediaAudioCodecAc3;
    }
    if (strcmp(codec, "ec-3") == 0) {
      return kSbMediaAudioCodecEac3;
    }
  }
  if (strcmp(codec, "opus") == 0) {
    return kSbMediaAudioCodecOpus;
  }
  if (strcmp(codec, "vorbis") == 0) {
    return kSbMediaAudioCodecVorbis;
  }
#if SB_API_VERSION >= 14
  if (strcmp(codec, "mp3") == 0 || strcmp(codec, "mp4a.69") == 0 ||
      strcmp(codec, "mp4a.6B") == 0) {
    return kSbMediaAudioCodecMp3;
  }
  if (strcmp(codec, "flac") == 0) {
    return kSbMediaAudioCodecFlac;
  }
#endif  // SB_API_VERSION >= 14
  return kSbMediaAudioCodecNone;
}

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
