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

#include "starboard/shared/starboard/media/media_support_internal.h"

#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"
#if SB_API_VERSION >= 11
#include "starboard/gles.h"
#endif  // SB_API_VERSION >= 11
#include "starboard/media.h"
#include "starboard/shared/libaom/aom_library_loader.h"
#include "starboard/shared/libde265/de265_library_loader.h"
#include "starboard/shared/libvpx/vpx_library_loader.h"
#include "starboard/shared/starboard/media/media_util.h"

using starboard::shared::aom::is_aom_supported;
using starboard::shared::de265::is_de265_supported;
using starboard::shared::starboard::media::IsSDRVideo;
using starboard::shared::vpx::is_vpx_supported;

bool SbMediaIsVideoSupported(SbMediaVideoCodec video_codec,
#if SB_HAS(MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT)
                             int profile,
                             int level,
                             int bit_depth,
                             SbMediaPrimaryId primary_id,
                             SbMediaTransferId transfer_id,
                             SbMediaMatrixId matrix_id,
#endif  // SB_HAS(MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT)
                             int frame_width,
                             int frame_height,
                             int64_t bitrate,
                             int fps
#if SB_API_VERSION >= 10
                             ,
                             bool decode_to_texture_required
#endif  // SB_API_VERSION >= 10
                             ) {
#if SB_API_VERSION < 11
  const auto kSbMediaVideoCodecAv1 = kSbMediaVideoCodecVp10;
#endif  // SB_API_VERSION < 11

#if SB_HAS(MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT)
  SB_UNREFERENCED_PARAMETER(profile);
  SB_UNREFERENCED_PARAMETER(level);

  if (!IsSDRVideo(bit_depth, primary_id, transfer_id, matrix_id)) {
    if (bit_depth != 10 && bit_depth != 12) {
      return false;
    }
    if (video_codec != kSbMediaVideoCodecAv1 &&
        video_codec != kSbMediaVideoCodecH265) {
      return false;
    }
  }
#endif  // SB_HAS(MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT)

#if SB_API_VERSION >= 10
  if (decode_to_texture_required) {
    bool has_gles_support = false;

#if SB_API_VERSION >= 11
    has_gles_support = SbGetGlesInterface();
#elif SB_HAS(GLES2)
    has_gles_support = true;
#endif

    if (!has_gles_support) {
      return false;
    }
    // Assume that all GLES2 Linux platforms can play decode-to-texture video
    // just as well as normal video.
  }
#endif  // SB_API_VERSION >= 10

  return ((video_codec == kSbMediaVideoCodecAv1 && is_aom_supported()) ||
          video_codec == kSbMediaVideoCodecH264 ||
          (video_codec == kSbMediaVideoCodecH265 && is_de265_supported()) ||
          (video_codec == kSbMediaVideoCodecVp9 && is_vpx_supported())) &&
         frame_width <= 1920 && frame_height <= 1080 &&
         bitrate <= kSbMediaMaxVideoBitrateInBitsPerSecond && fps <= 60;
}
