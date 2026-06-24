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

#include "starboard/shared/starboard/media/media_video_budget.h"

#include "build/build_config.h"
#include "starboard/shared/starboard/media/resolutions.h"

#if BUILDFLAG(IS_ANDROID)
#include "starboard/shared/starboard/features.h"
#endif

namespace starboard {

namespace {
constexpr int k1080pArea = Resolution::k1080p.width * Resolution::k1080p.height;
constexpr int k4KArea = Resolution::k4k.width * Resolution::k4k.height;
}  // namespace

int GetAreaBasedVideoBufferBudget(Size video_size, int bits_per_pixel) {
  int64_t resolution_area =
      (video_size.width <= 0 || video_size.height <= 0)
          ? 0
          : static_cast<int64_t>(video_size.width) * video_size.height;

  if (resolution_area == 0 || resolution_area <= k1080pArea) {
    return g_video_buffer_budget_1080p;
  } else if (resolution_area <= k4KArea) {
    if (bits_per_pixel <= 8) {
      return g_video_buffer_budget_4k_sdr;
    } else {
      return g_video_buffer_budget_4k_hdr;
    }
  } else {
    return g_video_buffer_budget_above_4k;
  }
}

int GetLegacyVideoBufferBudget(Size video_size, int bits_per_pixel) {
  starboard::Size resolution(video_size.width, video_size.height);
  if (resolution.FitsWithin(starboard::Resolution::k1080p) ||
      video_size.width <= 0 || video_size.height <= 0) {
    return g_video_buffer_budget_1080p;
  } else if (resolution.FitsWithin(starboard::Resolution::k4k)) {
    if (bits_per_pixel <= 8) {
      return g_video_buffer_budget_4k_sdr;
    } else {
      return g_video_buffer_budget_4k_hdr;
    }
  } else {
    return g_video_buffer_budget_above_4k;
  }
}

int GetDefaultVideoBufferBudget(Size video_size, int bits_per_pixel) {
#if BUILDFLAG(IS_ANDROID)
  if (starboard::features::FeatureList::IsEnabled(
          starboard::features::kAreaBasedVideoBufferBudget)) {
    return GetAreaBasedVideoBufferBudget(video_size, bits_per_pixel);
  } else {
    return GetLegacyVideoBufferBudget(video_size, bits_per_pixel);
  }
#else
  // Non-Android platforms use the new area-based logic directly.
  return GetAreaBasedVideoBufferBudget(video_size, bits_per_pixel);
#endif
}

}  // namespace starboard
