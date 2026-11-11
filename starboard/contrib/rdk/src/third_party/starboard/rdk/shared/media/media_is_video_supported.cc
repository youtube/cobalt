//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
//
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

#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"
#include "starboard/common/log.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/media/media_support_internal.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "third_party/starboard/rdk/shared/media/gst_media_utils.h"
#include "third_party/starboard/rdk/shared/platform/platform_interface.h"
#include "third_party/starboard/rdk/shared/log_override.h"

#include <cstring>

namespace starboard {

bool MediaIsVideoSupported(const MimeType* content_type,
                           const VideoSupportQuery& query) {
  const ParsedVideoCodec& codec_info = query.codec_info;

  if (query.decode_to_texture_required) {
    SB_LOG(WARNING) << "Decoding to texture required with "
                    << query.frame_size.width << "x" << query.frame_size.height;
    return false;
  }

  const auto video_resolution = platform::device().video_resolution().value_or(platform::Resolution{});

  if (query.frame_size.height > video_resolution.height ||
      query.frame_size.width > video_resolution.width) {
    return false;
  }

  if (codec_info.transfer_id != kSbMediaTransferIdUnspecified &&
      codec_info.transfer_id != kSbMediaTransferIdBt709 &&
      codec_info.transfer_id != kSbMediaTransferIdSmpteSt2084 &&
      codec_info.transfer_id != kSbMediaTransferIdAribStdB67) {
    return false;
  }

  if (!Is8BitSDRVideo(codec_info.bit_depth, codec_info.primary_id,
                      codec_info.transfer_id, codec_info.matrix_id)) {
    auto format = platform::device().hdr().value_or(platform::HDRFormat{});
    if (codec_info.transfer_id == kSbMediaTransferIdSmpteSt2084 &&
        !format.hdr10 && !format.hdr10Plus) {
      return false;
    }
    if (codec_info.transfer_id == kSbMediaTransferIdAribStdB67 && !format.hlg) {
      return false;
    }
    if (content_type) {
      for (auto &codec : content_type->GetCodecs()) {
        bool isDolbyVisionCodec =
          strncmp(codec.c_str(), "dvhe.", 5) == 0 ||
          strncmp(codec.c_str(), "dav1.", 5) == 0 ||
          strncmp(codec.c_str(), "dvh1.", 5) == 0;
        if (isDolbyVisionCodec && !format.dolbyVision) {
          return false;
        }
      }
    }
  }

  return query.bitrate <= kSbMediaMaxVideoBitrateInBitsPerSecond &&
         query.fps <= 60 && GstRegistryHasElementForMediaType(codec_info.codec);
}

}  // namespace starboard
