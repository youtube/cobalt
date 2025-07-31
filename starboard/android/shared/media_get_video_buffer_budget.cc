// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include <algorithm>

#include "starboard/android/shared/runtime_resource_overlay.h"
#include "starboard/common/log.h"
#include "starboard/media.h"

namespace {

// We set the maximum memory budget to 200MB, balancing the following factors:
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
// TODO: b/416039556 - Allow starboard::feature to override this value, once
// b/416039556 is completed.
constexpr int kMaxVideoBufferBudget = 200 * 1024 * 1024;

}  // namespace

int SbMediaGetVideoBufferBudget(SbMediaVideoCodec codec,
                                int resolution_width,
                                int resolution_height,
                                int bits_per_pixel) {
  auto get_overlaid_video_buffer_budget = []() {
    int buffer_budget =
        starboard::android::shared::RuntimeResourceOverlay::GetInstance()
            ->max_video_buffer_budget();
    if (buffer_budget == 0) {
      return kMaxVideoBufferBudget;
    }
    SB_LOG(INFO) << "RRO \"max_video_buffer_budget\" is set to "
                 << buffer_budget << " MB.";
    return buffer_budget * 1024 * 1024;
  };

  static const int overlaid_video_buffer_budget =
      get_overlaid_video_buffer_budget();

  int video_buffer_budget = 0;
  if ((resolution_width <= 1920 && resolution_height <= 1080) ||
      resolution_width == kSbMediaVideoResolutionDimensionInvalid ||
      resolution_height == kSbMediaVideoResolutionDimensionInvalid) {
    // Specifies the maximum amount of memory used by video buffers of media
    // source before triggering a garbage collection when the video resolution
    // is up to 1080p (1920x1080) or invalid.
    video_buffer_budget = 30 * 1024 * 1024;
  } else if (resolution_width <= 3840 && resolution_height <= 2160) {
    if (bits_per_pixel <= 8) {
      // Specifies the maximum amount of memory used by video buffers of media
      // source before triggering a garbage collection when the video resolution
      // is up to 4k (3840x2160) and bit per pixel is up to 8.
      video_buffer_budget = 100 * 1024 * 1024;
    } else {
      // Specifies the maximum amount of memory used by video buffers of media
      // source before triggering a garbage collection when video resolution is
      // up to 4k (3840x2160) and bit per pixel is greater than 8.
      video_buffer_budget = 160 * 1024 * 1024;
    }
  } else {
    // Specifies the maximum amount of memory used by video buffers of media
    // source before triggering a garbage collection when the video resolution
    // is above 4K (e.g., 8K).
    video_buffer_budget = kMaxVideoBufferBudget;
  }

  return std::min(video_buffer_budget, overlaid_video_buffer_budget);
}
