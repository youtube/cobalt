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

#ifndef STARBOARD_ANDROID_SHARED_MEDIA_CODEC_VIDEO_DECODER_HELPERS_H_
#define STARBOARD_ANDROID_SHARED_MEDIA_CODEC_VIDEO_DECODER_HELPERS_H_

#include <array>
#include <optional>
#include <string>

#include "starboard/common/size.h"
#include "starboard/configuration.h"
#include "starboard/decode_target.h"
#include "starboard/media.h"

namespace starboard {

bool IsSoftwareDecoderRequired(const std::string& max_video_capabilities);

std::optional<Size> ParseMaxResolution(
    const std::string& max_video_capabilities,
    const Size& frame_size);

bool Equal(const SbMediaMasteringMetadata& lhs,
           const SbMediaMasteringMetadata& rhs);

bool IsIdentity(const SbMediaColorMetadata& color_metadata);

struct DecodeTargetGeometry {
  SbDecodeTargetInfoContentRegion content_region;
  Size coded_size;
};
DecodeTargetGeometry GetDecodeTargetGeometryFromMatrix(
    const std::array<float, 16>& matrix4x4,
    const Size& display_size);

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_CODEC_VIDEO_DECODER_HELPERS_H_
