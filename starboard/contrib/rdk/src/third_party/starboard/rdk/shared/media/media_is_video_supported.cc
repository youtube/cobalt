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

using starboard::shared::starboard::media::IsSDRVideo;
using ::starboard::shared::starboard::media::MimeType;

bool MediaIsVideoSupported(SbMediaVideoCodec video_codec,
                           const MimeType* content_type,
                           int /*profile*/,
                           int /*level*/,
                           int bit_depth,
                           SbMediaPrimaryId primary_id,
                           SbMediaTransferId transfer_id,
                           SbMediaMatrixId matrix_id,
                           int frame_width,
                           int frame_height,
                           int64_t bitrate,
                           int fps,
                           bool decode_to_texture_required) {
  if (decode_to_texture_required) {
    SB_LOG(WARNING) << "Decoding to texture required with " << frame_width << "x"
                    << frame_height;
    return false;
  }

  const auto video_resolution = platform::device().video_resolution().value_or(platform::Resolution{});

  if (frame_height > video_resolution.height || frame_width > video_resolution.width) {
    return false;
  }

  if (transfer_id != kSbMediaTransferIdUnspecified &&
      transfer_id != kSbMediaTransferIdBt709 &&
      transfer_id != kSbMediaTransferIdSmpteSt2084 &&
      transfer_id != kSbMediaTransferIdAribStdB67) {
    return false;
  }

  if (!IsSDRVideo(bit_depth, primary_id, transfer_id, matrix_id)) {
    auto format = platform::device().hdr().value_or(platform::HDRFormat{});
    if (transfer_id == kSbMediaTransferIdSmpteSt2084 && !format.hdr10 && !format.hdr10Plus) {
      return false;
    }
    if (transfer_id == kSbMediaTransferIdAribStdB67 && !format.hlg) {
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

  return bitrate <= kSbMediaMaxVideoBitrateInBitsPerSecond && fps <= 60 &&
         GstRegistryHasElementForMediaType(video_codec);
}

}  // namespace starboard
