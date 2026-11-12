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
#include <string_view>

#include "starboard/common/size.h"
#include "starboard/configuration.h"
#include "starboard/decode_target.h"
#include "starboard/media.h"

namespace starboard {

class ExperimentalFeatures;

bool IsSoftwareDecoderRequired(const ExperimentalFeatures& features,
                               const std::string& max_video_capabilities);

// The maximum number of times MediaCodecVideoDecoder retries creating the
// platform MediaCodec. With the 50ms linear backoff, the retries take at most
// 50 + 100 + ... + 500ms = 2.75s before an error is reported.
inline constexpr int kMaxVideoCodecCreationRetryCount = 10;

// Returns how many times MediaCodecVideoDecoder retries creating the platform
// MediaCodec after a failure, from the H5VCC setting
// `Media.VideoCodecCreationRetryCount` in |features|. Returns 0 (retries
// disabled) when the setting is unset, not an int, or not positive. The result
// is capped at |kMaxVideoCodecCreationRetryCount|.
int GetVideoCodecCreationRetryCount(const ExperimentalFeatures& features);

std::optional<Size> ParseMaxResolution(
    const std::string& max_video_capabilities,
    std::string_view param_name,
    const Size& frame_size);

bool Equal(const SbMediaMasteringMetadata& lhs,
           const SbMediaMasteringMetadata& rhs);

struct DecodeTargetGeometry {
  SbDecodeTargetInfoContentRegion content_region;
  Size coded_size;
};
DecodeTargetGeometry GetDecodeTargetGeometryFromMatrix(
    const std::array<float, 16>& matrix4x4,
    const Size& display_size);

// Returns a Size containing the minimum width and minimum height of |a| and
// |b|. If only one optional contains a value, returns that value. If neither
// contains a value, returns std::nullopt.
std::optional<Size> GetLowestResolution(const std::optional<Size>& a,
                                        const std::optional<Size>& b);

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_CODEC_VIDEO_DECODER_HELPERS_H_
