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
#include "starboard/shared/starboard/media/iamf_util.h"

namespace starboard {

VideoConfig::VideoConfig(SbMediaVideoCodec video_codec,
                         const Size& size,
                         const uint8_t* data,
                         size_t data_size)
    : size_(size) {
  if (video_codec == kSbMediaVideoCodecVp9) {
    video_codec_ = video_codec;
  } else if (video_codec == kSbMediaVideoCodecAv1) {
    video_codec_ = video_codec;
  } else if (video_codec == kSbMediaVideoCodecH264) {
    avc_parameter_sets_ =
        AvcParameterSets(AvcParameterSets::kAnnexB, data, data_size);
    if (avc_parameter_sets_->is_valid()) {
      video_codec_ = video_codec;
    }
  } else {
    SB_NOTREACHED();
  }
}

VideoConfig::VideoConfig(const VideoStreamInfo& video_stream_info,
                         const uint8_t* data,
                         size_t data_size)
    : VideoConfig(video_stream_info.codec,
                  video_stream_info.frame_size,
                  data,
                  data_size) {}

bool VideoConfig::operator==(const VideoConfig& that) const {
  if (video_codec_ == kSbMediaVideoCodecNone &&
      that.video_codec_ == kSbMediaVideoCodecNone) {
    return true;
  }
  return video_codec_ == that.video_codec_ &&
         avc_parameter_sets_ == that.avc_parameter_sets_ && size_ == that.size_;
}

bool VideoConfig::operator!=(const VideoConfig& that) const {
  return !(*this == that);
}

SbMediaAudioCodec GetAudioCodecFromString(const char* codec,
                                          const char* subtype) {
  const bool kCheckAc3Audio = true;
  if (strncmp(codec, "mp4a.40.", 8) == 0) {
    return kSbMediaAudioCodecAac;
  }
  if (kCheckAc3Audio) {
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
  if (strcmp(codec, "mp3") == 0 || strcmp(codec, "mp4a.69") == 0 ||
      strcmp(codec, "mp4a.6B") == 0) {
    return kSbMediaAudioCodecMp3;
  }
  if (strcmp(codec, "flac") == 0) {
    return kSbMediaAudioCodecFlac;
  }
  // For WAV, the "codecs" parameter of a MIME type refers to the WAVE format
  // ID, where 1 represents PCM: https://datatracker.ietf.org/doc/html/rfc2361
  const bool is_wav =
      strcmp(subtype, "wav") == 0 || strcmp(subtype, "wave") == 0;
  if (is_wav && strcmp(codec, "1") == 0) {
    return kSbMediaAudioCodecPcm;
  }
  if (strcmp(codec, "iamf") == 0 || IamfMimeUtil(codec).is_valid()) {
    return kSbMediaAudioCodecIamf;
  }
  return kSbMediaAudioCodecNone;
}

}  // namespace starboard
