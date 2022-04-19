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

#include "starboard/android/shared/application_android.h"
#include "starboard/common/log.h"
#include "starboard/media.h"

int SbMediaGetVideoBufferBudget(SbMediaVideoCodec codec,
                                int resolution_width,
                                int resolution_height,
                                int bits_per_pixel) {
  constexpr int kMaxVideoBufferBudget = 300 * 1024 * 1024;
  auto get_overlayed_video_buffer_budget = []() {
    int buffer_budget = starboard::android::shared::ApplicationAndroid::Get()
                            ->GetOverlayedIntValue("max_video_buffer_budget");
    if (buffer_budget == 0) {
      return kMaxVideoBufferBudget;
    }
    SB_LOG(INFO) << "RRO \"max_video_buffer_budget\" is set to "
                 << buffer_budget << " MB.";
    return buffer_budget * 1024 * 1024;
  };

  static const int overlayed_video_buffer_budget =
      get_overlayed_video_buffer_budget();

  int video_buffer_budget = 0;
  if ((resolution_width <= 1920 && resolution_height <= 1080) ||
      resolution_width == kSbMediaVideoResolutionDimensionInvalid ||
      resolution_height == kSbMediaVideoResolutionDimensionInvalid) {
    // Specifies the maximum amount of memory used by video buffers of media
    // source before triggering a garbage collection when the video resolution
    // is lower than 1080p (1920x1080).
    video_buffer_budget = 30 * 1024 * 1024;
  } else if (resolution_width <= 3840 && resolution_height <= 2160) {
    if (bits_per_pixel <= 8) {
      // Specifies the maximum amount of memory used by video buffers of media
      // source before triggering a garbage collection when the video resolution
      // is lower than 4k (3840x2160) and bit per pixel is lower than 8.
      video_buffer_budget = 100 * 1024 * 1024;
    } else {
      // Specifies the maximum amount of memory used by video buffers of media
      // source before triggering a garbage collection when video resolution is
      // lower than 4k (3840x2160) and bit per pixel is greater than 8.
      video_buffer_budget = 160 * 1024 * 1024;
    }
  } else {
    // Specifies the maximum amount of memory used by video buffers of media
    // source before triggering a garbage collection when the video resolution
    // is lower than 8k (7680x4320).
    video_buffer_budget = kMaxVideoBufferBudget;
  }

  return std::min(video_buffer_budget, overlayed_video_buffer_budget);
}
