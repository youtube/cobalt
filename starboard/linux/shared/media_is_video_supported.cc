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

// clang-format off
#include "starboard/shared/starboard/media/media_support_internal.h"
// clang-format on

#include "starboard/common/log.h"
#include "starboard/common/size.h"
#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"
#include "starboard/gles.h"
#include "starboard/media.h"
#include "starboard/shared/libde265/de265_library_loader.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/media/resolutions.h"
#include "starboard/system.h"

namespace starboard {

bool MediaIsVideoSupported(const MimeType* mime_type,
                           const VideoSupportQueryParams& params) {
  const ParsedVideoCodec& codec_info = params.codec_info;
  const SbMediaVideoCodec video_codec = codec_info.codec;

  if (!Is8BitSDRVideo(codec_info.bit_depth, codec_info.primary_id,
                      codec_info.transfer_id, codec_info.matrix_id)) {
    if (codec_info.bit_depth != 10 && codec_info.bit_depth != 12) {
      return false;
    }
    if (video_codec != kSbMediaVideoCodecAv1 &&
        video_codec != kSbMediaVideoCodecH265 &&
        video_codec != kSbMediaVideoCodecVp9) {
      return false;
    }
  }

  if (params.decode_to_texture_required) {
    bool has_gles_support = SbGetGlesInterface();

    if (!has_gles_support) {
      return false;
    }
    // Assume that all GLES2 Linux platforms can play decode-to-texture video
    // just as well as normal video.
  }

  static const bool can_support_8k = SbSystemGetNumberOfProcessors() >= 16;

  const Size max_size = can_support_8k ? Resolution::k8k : Resolution::k1080p;

  return (video_codec == kSbMediaVideoCodecAv1 ||
          video_codec == kSbMediaVideoCodecH264 ||
          (video_codec == kSbMediaVideoCodecH265 && is_de265_supported()) ||
          (video_codec == kSbMediaVideoCodecVp8) ||
          (video_codec == kSbMediaVideoCodecVp9)) &&
         params.frame_size.FitsWithin(max_size) &&
         params.bitrate <= kSbMediaMaxVideoBitrateInBitsPerSecond &&
         params.fps <= 60;
}

}  // namespace starboard
