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
#include "starboard/shared/starboard/media/media_constants.h"

#if BUILDFLAG(IS_ANDROID)
#include "starboard/shared/starboard/features.h"
#endif

namespace starboard {
namespace media {

// The following budget variables are mutable (non-const) to allow runtime
// experimentation and overrides (e.g., via JS web player / H5VCC).
std::atomic<int> g_video_buffer_budget_1080p{30 * 1024 * 1024};
std::atomic<int> g_video_buffer_budget_4k_sdr{100 * 1024 * 1024};
std::atomic<int> g_video_buffer_budget_4k_hdr{160 * 1024 * 1024};

// We set the maximum memory budget to 200MB on Android (and 300MB on other
// platforms), balancing the following factors:
//
// 1. 8K Video Playback
// Playing 8K video requires significant memory for decoded frames. For
// instance, a specific device needs to hold up to 6 frames inside its decoder,
// which can take almost 300MB. See b/405467220#comment46 for details. Because
// of this large requirement for decoded frames, we can't allocate too much
// budget for encoded frames.
//
// 2. Chromium's Budget
// Chromium has a max memory budget of 150MB.
// https://github.com/youtube/cobalt/blob/a3c966f929aabea1d71813c31d404e1b319c2fcd/media/base/demuxer_memory_limit.h#L44
#if BUILDFLAG(IS_ANDROID)
std::atomic<int> g_video_buffer_budget_above_4k{200 * 1024 * 1024};
#else
std::atomic<int> g_video_buffer_budget_above_4k{300 * 1024 * 1024};
#endif

int GetAreaBasedVideoBufferBudget(Size resolution, int bits_per_pixel) {
  int64_t resolution_area =
      (resolution.width <= 0 || resolution.height <= 0)
          ? 0
          : static_cast<int64_t>(resolution.width) * resolution.height;

  if (resolution_area == 0 || resolution_area <= k1080pArea) {
    return g_video_buffer_budget_1080p.load();
  } else if (resolution_area <= k4KArea) {
    if (bits_per_pixel <= 8) {
      return g_video_buffer_budget_4k_sdr.load();
    } else {
      return g_video_buffer_budget_4k_hdr.load();
    }
  } else {
    return g_video_buffer_budget_above_4k.load();
  }
}

int GetLegacyVideoBufferBudget(Size resolution, int bits_per_pixel) {
  if ((resolution.width <= 1920 && resolution.height <= 1080) ||
      resolution.width <= 0 || resolution.height <= 0) {
    return g_video_buffer_budget_1080p.load();
  } else if (resolution.width <= 3840 && resolution.height <= 2160) {
    if (bits_per_pixel <= 8) {
      return g_video_buffer_budget_4k_sdr.load();
    } else {
      return g_video_buffer_budget_4k_hdr.load();
    }
  } else {
    return g_video_buffer_budget_above_4k.load();
  }
}

int GetDefaultVideoBufferBudget(Size resolution, int bits_per_pixel) {
#if BUILDFLAG(IS_ANDROID)
  if (starboard::features::FeatureList::IsEnabled(
          starboard::features::kAreaBasedVideoBufferBudget)) {
    return GetAreaBasedVideoBufferBudget(resolution, bits_per_pixel);
  } else {
    return GetLegacyVideoBufferBudget(resolution, bits_per_pixel);
  }
#else
  // Non-Android platforms use the new area-based logic directly.
  return GetAreaBasedVideoBufferBudget(resolution, bits_per_pixel);
#endif
}

}  // namespace media
}  // namespace starboard
